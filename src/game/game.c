#include "game/game.h"
#include "display/display.h"

#define NOTE_TRAVEL_TIME_MS 1500

static const GameNote_t *song_notes = 0;
static uint32_t song_note_count = 0;

static uint32_t start_time_ms = 0;
static uint8_t running = 0;

static int32_t note_to_x(
    uint32_t note_time,
    uint32_t now
)
{
    int32_t dt =
        (int32_t)note_time -
        (int32_t)now;

    return 7 -
           ((dt * 8) /
            NOTE_TRAVEL_TIME_MS);
}

void game_init()
{
    running = 0;
}

void game_set_song(
    const GameNote_t *notes,
    uint32_t count
)
{
    song_notes = notes;
    song_note_count = count;
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

void game_update(uint32_t current_time_ms)
{
    if (!running)
    {
        return;
    }

    display_clear();

    for (uint32_t i = 0;
         i < song_note_count;
         i++)
    {
        const GameNote_t *note =
            &song_notes[i];

        int32_t x =
            note_to_x(
                note->time_ms,
                current_time_ms - start_time_ms
            );

        if (x < 0 || x > 7)
        {
            continue;
        }

        uint8_t y0 =
            (3 - note->lane) * 2;

        uint8_t y1 =
            y0 + 1;

uint8_t r = 0;
uint8_t g = 0;
uint8_t b = 0;

switch (note->lane)
{
    case 0: // green
        g = 5;
        break;

    case 1: // red
        r = 5;
        break;

    case 2: // yellow
        r = 5;
        g = 5;
        break;

    case 3: // blue
        b = 5;
        break;
}

int32_t x0 = x;
int32_t x1 = x;

/*
 * Serpentine compensation:
 * odd rows are mirrored
 */
if (y0 & 1)
{
    x0 = 7 - x0;
}

if (y1 & 1)
{
    x1 = 7 - x1;
}

display_setLED(
    x0,
    y0,
    r,
    g,
    b
);

display_setLED(
    x1,
    y1,
    r,
    g,
    b
);
    }
}
