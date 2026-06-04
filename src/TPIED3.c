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

#include "audio_engine.h"
#include "song.h"

#define BPM             123U
#define TICKS_BEAT      (AUDIO_RATE_HZ * 60U / BPM)
#define TICKS_HALF      (TICKS_BEAT / 2)
#define TICKS_16TH      (TICKS_BEAT / 4)

#define NOTE_REST   0.0000f
#define NOTE_B1     0.7071f   //  58.27 Hz
#define NOTE_C2     0.7491f   //  61.74 Hz
#define NOTE_D2     0.8909f   //  73.42 Hz
#define NOTE_Ds2    0.9439f   //  77.78 Hz
#define NOTE_E2     1.0000f   //  82.41 Hz
#define NOTE_F2     1.0595f   //  87.31 Hz
#define NOTE_G2     1.1892f   //  98.00 Hz

static const SeqNote guitar_notes[] = {
    //  sample              length                  pitch       duration
    { guitar_e_sample, guitar_e_sample_len,  NOTE_E2,  TICKS_16TH * 2 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_E2,  TICKS_16TH * 1 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_G2,  TICKS_16TH * 2 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_E2,  TICKS_16TH * 2 },
    { NULL,            0,                    NOTE_REST, TICKS_16TH * 1 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_D2,  TICKS_16TH * 2 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_C2,  TICKS_16TH * 2 },
    { guitar_e_sample, guitar_e_sample_len,  NOTE_B1,  TICKS_16TH * 4 },
};

static const SeqNote drum_notes[] = {
    { drum_sample, drum_sample_len,  1.0f,      TICKS_BEAT     },  // kick beat 1
    { NULL,        0,                NOTE_REST, TICKS_BEAT     },  // rest beat 2
    { drum_sample, drum_sample_len,  1.0f,      TICKS_BEAT     },  // kick beat 3
    { NULL,        0,                NOTE_REST, TICKS_BEAT     },  // rest beat 4
};

SeqTrack g_Tracks[2];
uint32_t g_TrackCount = 2;


int main(void)
{
    SystemInit();

    WS2812_Init();
    WS2812_Start();

    WS2812_Clear();


    WS2812_SetLED(0, 0, 255, 0, 0);
    WS2812_SetLED(1, 0, 0, 255, 0);
    WS2812_SetLED(2, 0, 0, 0, 255);

    Audio_Init();

    Seq_Init(&g_Tracks[0], guitar_notes,
             sizeof(guitar_notes) / sizeof(guitar_notes[0]));

    Seq_Init(&g_Tracks[1], drum_notes,
             sizeof(drum_notes) / sizeof(drum_notes[0]));

    Audio_Start();

    /*
     * serpentine row test
     */
    for(int x = 0; x < 8; x++)
    {
        WS2812_SetLED(
            x,
            1,
            255,
            255,
            0
        );
    }
    WS2812_Clear();

    while(1)
    {
    	Audio_FillBuffer();
    }
}
