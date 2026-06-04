#ifndef AUDIO_ENGINE_H_
#define AUDIO_ENGINE_H_

#include "LPC17xx.h"
#include "lpc_types.h"

#include "lpc17xx_dac.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpdma.h"

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================
// CONFIG
// =====================================================

#define AUDIO_SAMPLE_RATE     22050
#define AUDIO_BUFFER_SIZE     256
#define AUDIO_NUM_BUFFERS     2
#define AUDIO_MAX_VOICES      8

// =====================================================
// TYPES
// =====================================================

// One active sound instance
typedef struct {
    const int16_t *sample;
    uint32_t length;
    float pos;        // fractional position
    float step;       // pitch multiplier
    uint8_t active;
} AudioVoice;

// Song event
typedef struct {
    uint32_t time;   // in samples
    uint8_t type;    // 0 = guitar, 1 = drum
    float pitch;
} AudioEvent;

// DMA buffer index state
typedef enum {
    AUDIO_BUF_A = 0,
    AUDIO_BUF_B = 1
} AudioBufferIndex;

// =====================================================
// PUBLIC API (WHAT YOU REQUESTED)
// =====================================================

/**
 * Initialize:
 * - PINSEL for DAC (P0.26)
 * - DAC init
 * - GPDMA init
 * - Timer + DMA config (22.05 kHz)
 * - prepares buffers
 * - DOES NOT start playback (safe state)
 */
void Audio_Init(void);

/**
 * Fill the buffer that is NOT currently being played.
 * This is called from CPU loop.
 *
 * Returns:
 *   0 = success
 *  -1 = buffer not ready / still playing
 */
int Audio_FillBuffer(void);

/**
 * Starts playback AFTER first buffer is filled.
 * Enables DMA + DAC streaming.
 */
void Audio_Start(void);

/**
 * Stops playback safely.
 */
void Audio_Stop(void);

// =====================================================
// DMA INTERRUPT (SWAPS BUFFERS)
// =====================================================

void DMA_IRQHandler(void);

// =====================================================
// AUDIO CONTROL
// =====================================================

void Audio_StartVoice(const int16_t *sample,
                      uint32_t length,
                      float pitch);

// advance song clock
void Audio_Advance(uint32_t samples);

// =====================================================
// EXTERNAL STATE (used internally but exposed for simplicity)
// =====================================================

extern volatile uint8_t Audio_CurrentBuffer;
extern volatile uint8_t Audio_BufferReady[AUDIO_NUM_BUFFERS];
extern volatile uint32_t Audio_Time;

extern int16_t Audio_Buffers[AUDIO_NUM_BUFFERS][AUDIO_BUFFER_SIZE];

extern AudioVoice Audio_Voices[AUDIO_MAX_VOICES];

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_H_ */
