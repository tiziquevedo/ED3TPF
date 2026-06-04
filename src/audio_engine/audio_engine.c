#include "audio_engine.h"
#include <string.h>
#include "lpc17xx_timer.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_clkpwr.h"

// =====================================================
// STATE
// =====================================================

volatile uint8_t  Audio_CurrentBuffer          = 0;
volatile uint8_t  Audio_BufferReady[AUDIO_NUM_BUFFERS] = {0, 0};
volatile uint32_t Audio_Time                   = 0;

int16_t  Audio_Buffers[AUDIO_NUM_BUFFERS][AUDIO_BUFFER_SIZE];

static uint16_t Audio_DACBuffers[AUDIO_NUM_BUFFERS][AUDIO_BUFFER_SIZE]
    __attribute__((aligned(4)));

AudioVoice Audio_Voices[AUDIO_MAX_VOICES];

static const uint8_t DMA_CH = GPDMA_CH_4;


static GPDMA_LLI_T Audio_LLI[2] __attribute__((aligned(4)));


#define DMA_CTRL_WORD                              \
    ( ((AUDIO_BUFFER_SIZE) & 0xFFFUL)             \
    | (0UL  << 12)   /* SBSize = 1              */\
    | (0UL  << 15)   /* DBSize = 1              */\
    | (1UL  << 18)   /* SWidth = halfword       */\
    | (2UL  << 21)   /* DWidth = word           */\
    | (1UL  << 26)   /* SI     = increment      */\
    | (0UL  << 27)   /* DI     = no increment   */\
    | (1UL  << 31) ) /* I      = TC interrupt   */

// =====================================================
// AUDIO RATE
// =====================================================

#define AUDIO_RATE_HZ  22050U

// =====================================================
// PCLK HELPER
// =====================================================

static uint32_t getPclkTimer1(void)
{
    return CLKPWR_GetPCLK(CLKPWR_PCLKSEL_TIMER1);
}

// =====================================================
// SAMPLE FORMAT CONVERSION — PCM int16 -> DAC uint16
// =====================================================

static inline uint16_t audio_to_dac(int16_t s)
{
    // 1. Bias to unsigned: 0..65535
    // 2. Take top 10 bits: >> 6  -> 0..1023
    // 3. Shift into DAC bits [15:6]: << 6
    // Net: the >> 6 and << 6 cancel, so we just bias and mask.
    uint16_t val = (uint16_t)(((int32_t)s + 32768) & 0xFFC0U);
    return val;  // bits [15:6] carry the 10-bit DAC value
}

static void Audio_ConvertToDac(uint8_t buf)
{
    for (uint32_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        Audio_DACBuffers[buf][i] = audio_to_dac(Audio_Buffers[buf][i]);
    }
}

// =====================================================
// MIXER
// =====================================================

static void Audio_Mix(int16_t *out, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {

        int32_t mix = 0;

        for (int v = 0; v < AUDIO_MAX_VOICES; v++) {

            if (!Audio_Voices[v].active)
                continue;

            uint32_t idx = (uint32_t)Audio_Voices[v].pos;

            // Advance position first, then check bounds.
            // This prevents a one-sample overread when step > 1.0.
            Audio_Voices[v].pos += Audio_Voices[v].step;

            if (idx >= Audio_Voices[v].length) {
                Audio_Voices[v].active = 0;
                continue;
            }

            mix += Audio_Voices[v].sample[idx];
        }

        // Hard clip
        if (mix >  32767) mix =  32767;
        if (mix < -32768) mix = -32768;

        out[i] = (int16_t)mix;
    }
}

// =====================================================
// VOICES
// =====================================================

void Audio_StartVoice(const int16_t *sample,
                      uint32_t       length,
                      float          pitch)
{
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!Audio_Voices[i].active) {
            Audio_Voices[i].sample = sample;
            Audio_Voices[i].length = length;
            Audio_Voices[i].pos    = 0.0f;
            Audio_Voices[i].step   = pitch;
            Audio_Voices[i].active = 1;
            return;
        }
    }
}

// =====================================================
// LLI INIT
//
// Two LLIs form a closed loop
// =====================================================

static void Audio_LLI_Init(void)
{
    Audio_LLI[0].srcAddr = (uint32_t)Audio_DACBuffers[0];
    Audio_LLI[0].dstAddr = (uint32_t)&LPC_DAC->DACR;
    Audio_LLI[0].nextLLI = (uint32_t)&Audio_LLI[1];
    Audio_LLI[0].control = DMA_CTRL_WORD;

    Audio_LLI[1].srcAddr = (uint32_t)Audio_DACBuffers[1];
    Audio_LLI[1].dstAddr = (uint32_t)&LPC_DAC->DACR;
    Audio_LLI[1].nextLLI = (uint32_t)&Audio_LLI[0];
    Audio_LLI[1].control = DMA_CTRL_WORD;
}

// =====================================================
// DMA INTERRUPT
// =====================================================

void DMA_IRQHandler(void)
{
    if (GPDMA_IntGetStatus(GPDMA_INTTC, DMA_CH)) {

        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, DMA_CH);

        // CurrentBuffer was just played out by the DMA.
        // Flip to the other one (now being played) and
        // mark the old one as free to refill.
        Audio_CurrentBuffer ^= 1;
        Audio_BufferReady[Audio_CurrentBuffer ^ 1] = 0;
        Audio_Time += AUDIO_BUFFER_SIZE;
    }
}

// =====================================================
// TIMER1 CONFIG (22.05 kHz DMA trigger)
// =====================================================

static void Audio_Timer1_Init(void)
{
    TIM_TIMERCFG_T cfg;

    cfg.prescaleOpt   = TIM_TICK;
    cfg.prescaleValue = 1;

    TIM_InitTimer(LPC_TIM1, &cfg);

    uint32_t pclk = getPclkTimer1();
    uint32_t match = (pclk / AUDIO_RATE_HZ) - 1U;

    TIM_MATCHCFG_T mcfg;
    mcfg.channel    = TIM_MATCH_0;
    mcfg.intEn      = DISABLE;
    mcfg.stopEn     = DISABLE;
    mcfg.resetEn    = ENABLE;
    mcfg.extOpt     = TIM_NOTHING;
    mcfg.matchValue = match;

    TIM_ConfigMatch(LPC_TIM1, &mcfg);

    TIM_ResetCounter(LPC_TIM1);
    TIM_Enable(LPC_TIM1);
}

// =====================================================
// DMA CONFIG
//
// The initial transfer uses cfg.srcMemAddr = DACBuffer[0].
// cfg.linkedList points to LLI[1], so after the first
// transfer completes the hardware loads LLI[1] (buffer 1),
// and LLI[1].NextLLI points back to LLI[0] — closed loop.
// =====================================================

static void Audio_DMA_Init(void)
{
    GPDMA_Init();

    GPDMA_Channel_CFG_T cfg;

    cfg.channelNum    = DMA_CH;
    cfg.transferSize  = AUDIO_BUFFER_SIZE;
    cfg.srcMemAddr    = (uint32_t)Audio_DACBuffers[0];
    cfg.dstMemAddr    = (uint32_t)&LPC_DAC->DACR;
    cfg.type          = GPDMA_M2P;
    cfg.srcConn       = 0;
    cfg.dstConn       = GPDMA_MAT1_0;
    cfg.src.width     = GPDMA_HALFWORD;  // 16-bit source
    cfg.dst.width     = GPDMA_HALFWORD;      // 32-bit DAC register
    cfg.src.burst     = GPDMA_BSIZE_1;
    cfg.dst.burst     = GPDMA_BSIZE_1;
    cfg.src.increment = ENABLE;
    cfg.dst.increment = DISABLE;
    cfg.intTC         = ENABLE;
    cfg.intErr        = ENABLE;

    cfg.linkedList    = (uint32_t)&Audio_LLI[1];

    GPDMA_SetupChannel(&cfg);
    GPDMA_ChannelStart(DMA_CH);
}

// =====================================================
// INIT AUDIO SYSTEM
// Call once at startup before Audio_Start().
// =====================================================

void Audio_Init(void)
{
    PINSEL_CFG_T pin;

    pin.port      = 0;
    pin.pin       = 26;
    pin.func      = 2;          // AOUT function
    pin.mode      = PINSEL_TRISTATE;
    pin.openDrain = DISABLE;

    PINSEL_ConfigPin(&pin);

    DAC_Init();
    DAC_SetBias(DAC_700uA);

    DAC_CONVERTER_CFG_T dacCfg;
    dacCfg.doubleBuffer = ENABLE;
    dacCfg.dmaCounter   = DISABLE;  // Timer1 drives the rate, not DAC counter
    dacCfg.dmaRequest   = ENABLE;

    DAC_ConfigDAConverterControl(&dacCfg);
    DAC_SetDMATimeOut(1);

    memset(Audio_Buffers,    0, sizeof(Audio_Buffers));
    memset(Audio_DACBuffers, 0, sizeof(Audio_DACBuffers));
    memset(Audio_Voices,     0, sizeof(Audio_Voices));

    Audio_CurrentBuffer = 0;
    Audio_Time          = 0;

    // Enable DMA interrupt in NVIC
    NVIC_EnableIRQ(DMA_IRQn);
}

// =====================================================
// BUFFER FILL
//
// Fills whichever buffer is not  being played.
// Returns 0 on success, -1 if the buffer was already full
// =====================================================
extern void Seq_Tick(SeqTrack *t);
extern SeqTrack g_Tracks[];
extern uint32_t g_TrackCount;

int Audio_FillBuffer(void)
{
    uint8_t buf = Audio_CurrentBuffer ^ 1;

    if (Audio_BufferReady[buf])
        return -1;

    // Run sequencer before mixing so new voices are
    // already active when Audio_Mix processes this buffer
    for (uint32_t i = 0; i < g_TrackCount; i++)
        Seq_Tick(&g_Tracks[i]);

    Audio_Mix(Audio_Buffers[buf], AUDIO_BUFFER_SIZE);
    Audio_ConvertToDac(buf);
    Audio_BufferReady[buf] = 1;

    return 0;
}

// =====================================================
// START PLAYBACK
//
// Pre-fills both buffers so DMA never plays garbage on
// the first LLI wrap, then starts the timer and DMA.
// =====================================================

void Audio_Start(void)
{
    // Bootstrap: fill both buffers unconditionally
    Audio_Mix(Audio_Buffers[0], AUDIO_BUFFER_SIZE);
    Audio_ConvertToDac(0);
    Audio_BufferReady[0] = 1;

    Audio_Mix(Audio_Buffers[1], AUDIO_BUFFER_SIZE);
    Audio_ConvertToDac(1);
    Audio_BufferReady[1] = 1;

    Audio_LLI_Init();
    Audio_Timer1_Init();
    Audio_DMA_Init();
}

// =====================================================
// STOP PLAYBACK
// =====================================================

void Audio_Stop(void)
{
    TIM_Disable(LPC_TIM1);
    GPDMA_ChannelStop(DMA_CH);
}
