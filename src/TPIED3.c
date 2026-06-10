/*
 * Copyright 2022 NXP
 * NXP confidential.
 * This software is owned or controlled by NXP and may only be used strictly
 * in accordance with the applicable license terms.  By expressly accepting
 * such terms or by downloading, installing, activating and/or otherwise using
 * the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to
 * be bound by the applicable license terms, then you may not retain, install,
 * activate or otherwise use the software.
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>

#include <stdio.h>

#include "display/display.h"
#include "game/game.h"
#include "song/song.h"
#include "audio/audio.h"
#include "song/song_audio.h"
#include "mstimer/mstimer.h"

volatile uint32_t game_time_ms =0;


int main(void)
{
    SystemInit();

    display_init();
    display_start();

    DAC_PLAYER_Init(8000);

    game_init();

    game_set_song(
        generated_song,
        sizeof(generated_song) /
        sizeof(generated_song[0])
    );
    setup_timer(&game_time_ms);
    game_start(game_time_ms);
    DAC_PLAYER_Play(song_audio, song_audio_length);





    while(1)
    {
        volatile uint32_t count = 1000000;
        while (count--)
        {
            __asm volatile ("nop");
        }
        game_update(game_time_ms);

    }
}
