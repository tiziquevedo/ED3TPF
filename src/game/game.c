#include "game/game.h"
#include "display/display.h"
#include <stdio.h>

#define NOTE_TRAVEL_TIME_MS 1500
#define HIT_WINDOW_MS 150
#define NOTE_MISS_MARGIN_MS 200

static const GameNote_t *song_notes = 0;
static uint32_t song_note_count = 0;

static uint32_t start_time_ms = 0;
static uint8_t running = 0;

static uint8_t note_hit[256];
static uint8_t note_expired[256];
static uint8_t lane_flash[4];

static uint32_t score_hits = 0;
static uint32_t score_total = 0;

static uint8_t end_scheduled = 0;
static uint32_t end_time_ms = 0;

uint32_t game_get_hits(void) { return score_hits; }
uint32_t game_get_total(void) { return score_total; }

static int32_t note_to_x(uint32_t note_time, uint32_t now)
{
    int32_t dt = (int32_t)note_time - (int32_t)now;
    return 7 - ((dt * 8) / NOTE_TRAVEL_TIME_MS);
}

void game_init(void)
{
    running = 0;
    score_hits = 0;
    score_total = 0;
    end_scheduled = 0;
    end_time_ms = 0;

    for (int i = 0; i < 4; i++) lane_flash[i] = 0;
    for (int i = 0; i < 256; i++) note_hit[i] = 0;
    for (int i = 0; i < 256; i++) note_expired[i] = 0;
}

void game_set_song(const GameNote_t *notes, uint32_t count)
{
    song_notes = notes;
    song_note_count = count;

    score_hits = 0;
    score_total = 0;
    end_scheduled = 0;
    end_time_ms = 0;

    uint32_t cap = count < 256 ? count : 256;

    for (uint32_t i = 0; i < cap; i++)
    {
        note_hit[i] = 0;
        note_expired[i] = 0;
    }
}

void game_start(uint32_t start)
{
    start_time_ms = start;
    running = 1;
}

void game_stop(void)
{
    running = 0;
}

static void schedule_end(uint32_t now)
{
    if (!end_scheduled)
    {
        end_scheduled = 1;
        end_time_ms = now + 1000;
    }
}

void game_update(uint32_t current_time_ms,
                 const uint32_t *button_times,
                 uint32_t button_count)
{
    if (!running) return;

    if (end_scheduled && current_time_ms >= end_time_ms)
    {
        game_stop();
        printf("DONE %lu / %lu\n",
               (unsigned long)score_hits,
               (unsigned long)song_note_count);
        return;
    }

    uint32_t game_now = current_time_ms - start_time_ms;
    uint32_t cap = song_note_count < 256 ? song_note_count : 256;
    uint32_t lanes = button_count < 4 ? button_count : 4;

    for (uint32_t lane = 0; lane < lanes; lane++)
    {
        if (button_times[lane] == 0) continue;

        uint32_t press_game_time = button_times[lane] - start_time_ms;

        int32_t best_delta = HIT_WINDOW_MS + 1;
        uint32_t best_idx = 0;
        uint8_t found = 0;

        for (uint32_t i = 0; i < cap; i++)
        {
            if (note_hit[i]) continue;
            if (song_notes[i].lane != (uint8_t)lane) continue;

            int32_t delta = (int32_t)press_game_time - (int32_t)song_notes[i].time_ms;
            int32_t abs_delta = delta < 0 ? -delta : delta;

            if (abs_delta <= HIT_WINDOW_MS && abs_delta < best_delta)
            {
                best_delta = abs_delta;
                best_idx = i;
                found = 1;
            }
        }

        if (found)
        {
            note_hit[best_idx] = 1;
            note_expired[best_idx] = 1;
            lane_flash[lane] = 1;
            score_hits++;
            score_total++;

            if (!end_scheduled && score_total == song_note_count)
                schedule_end(current_time_ms);
        }
    }

    for (uint32_t i = 0; i < cap; i++)
    {
        if (note_expired[i]) continue;

        uint32_t note_time = song_notes[i].time_ms;

        if ((int32_t)game_now > (int32_t)(note_time + NOTE_TRAVEL_TIME_MS + NOTE_MISS_MARGIN_MS))
        {
            note_expired[i] = 1;
            score_total++;

            if (!end_scheduled && score_total == song_note_count)
                schedule_end(current_time_ms);
        }
    }

    display_clear();

    for (uint8_t lane = 0; lane < 4; lane++)
    {
        if (!lane_flash[lane]) continue;

        uint8_t y0 = (3 - lane) * 2;
        uint8_t y1 = y0 + 1;

        uint8_t r = 0, g = 0, b = 0;

        switch (lane)
        {
            case 0: g = 2; break;
            case 1: r = 2; break;
            case 2: r = 2; g = 2; break;
            case 3: b = 2; break;
        }

        for (int x = 0; x <= 7; x++)
        {
            int x0 = x;
            int x1 = x;

            if (y0 & 1) x0 = 7 - x0;
            if (y1 & 1) x1 = 7 - x1;

            display_setLED(x0, y0, r, g, b);
            display_setLED(x1, y1, r, g, b);
        }

        lane_flash[lane] = 0;
    }

    for (uint32_t i = 0; i < cap; i++)
    {
        if (note_hit[i]) continue;

        const GameNote_t *note = &song_notes[i];

        int32_t x = note_to_x(note->time_ms, game_now);
        if (x < 0 || x > 7) continue;

        uint8_t y0 = (3 - note->lane) * 2;
        uint8_t y1 = y0 + 1;

        uint8_t r = 0, g = 0, b = 0;

        switch (note->lane)
        {
            case 0: g = 16; break;
            case 1: r = 16; break;
            case 2: r = 16; g = 16; break;
            case 3: b = 16; break;
        }

        int x0 = x;
        int x1 = x;

        if (y0 & 1) x0 = 7 - x0;
        if (y1 & 1) x1 = 7 - x1;

        display_setLED(x0, y0, r, g, b);
        display_setLED(x1, y1, r, g, b);
    }
}
