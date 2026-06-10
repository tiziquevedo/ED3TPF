#include "audio/audio.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_clkpwr.h"
#include "LPC17xx.h"

#include <string.h>


static GPDMA_LLI_T s_lli[DAC_PLAYER_MAX_LLI];
static uint32_t s_lli_count;
static volatile int s_done;
static inline uint32_t dacr_upper_byte_addr(void)
{
    return (uint32_t)((uint8_t*)(&LPC_DAC->DACR) + 1);
}

static uint32_t make_control(uint32_t chunk, int last)
{
    uint32_t ctrl = GPDMA_DMACCxControl_TransferSize(chunk)
                  | GPDMA_DMACCxControl_SBSize(GPDMA_BSIZE_1)
                  | GPDMA_DMACCxControl_DBSize(GPDMA_BSIZE_1)
                  | GPDMA_DMACCxControl_SWidth(GPDMA_BYTE)
                  | GPDMA_DMACCxControl_DWidth(GPDMA_BYTE)
                  | GPDMA_DMACCxControl_SI;
    if (last) {
        ctrl |= GPDMA_DMACCxControl_I;
    }
    return ctrl;
}


void DAC_PLAYER_Init(uint32_t sampleRateHz)
{
    DAC_Init();

    DAC_CONVERTER_CFG_T dacCtrl = {
        .doubleBuffer = DISABLE,
        .dmaCounter   = ENABLE,
        .dmaRequest   = ENABLE
    };
    DAC_ConfigDAConverterControl(&dacCtrl);
    uint32_t pclk    = CLKPWR_GetPCLK(CLKPWR_PCLKSEL_DAC);
    uint16_t timeout = (uint16_t)(pclk / sampleRateHz);
    DAC_SetDMATimeOut(timeout);
    //GPDMA_Init();
}

void DAC_PLAYER_Play(const uint8_t* samples, uint32_t length)
{
    if (!samples || length == 0) return;

    s_done = 0;
    const uint32_t dst = dacr_upper_byte_addr();
    uint32_t remaining = length;
    uint32_t offset    = 0;
    s_lli_count        = 0;

    while (remaining > 0)
    {
        uint32_t chunk = (remaining > DAC_PLAYER_LLI_CHUNK)
                       ? DAC_PLAYER_LLI_CHUNK
                       : remaining;

        int last = (remaining <= DAC_PLAYER_LLI_CHUNK);

        GPDMA_LLI_T* node = &s_lli[s_lli_count];

        node->srcAddr = (uint32_t)(samples + offset);
        node->dstAddr = dst;
        node->nextLLI = last ? 0 : (uint32_t)(&s_lli[s_lli_count + 1]);
        node->control = make_control(chunk, last);

        offset    += chunk;
        remaining -= chunk;
        s_lli_count++;
    }
    GPDMA_Channel_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.channelNum  = DAC_PLAYER_DMA_CHANNEL;
    cfg.type        = GPDMA_M2P;
    cfg.srcMemAddr  = s_lli[0].srcAddr;
    cfg.dstMemAddr  = 0;
    cfg.dstConn     = GPDMA_DAC;
    cfg.srcConn     = GPDMA_DAC;
    cfg.transferSize = (s_lli_count == 1)
                     ? (uint32_t)(length < DAC_PLAYER_LLI_CHUNK ? length : DAC_PLAYER_LLI_CHUNK)
                     : DAC_PLAYER_LLI_CHUNK;

    cfg.src.width     = GPDMA_BYTE;
    cfg.src.burst     = GPDMA_BSIZE_1;
    cfg.src.increment = ENABLE;

    cfg.dst.width     = GPDMA_BYTE;
    cfg.dst.burst     = GPDMA_BSIZE_1;
    cfg.dst.increment = DISABLE;

    cfg.intTC  = ENABLE;
    cfg.intErr = ENABLE;

    cfg.linkedList = (s_lli_count > 1) ? (uint32_t)(&s_lli[1]) : 0;

    NVIC_EnableIRQ(DMA_IRQn);

    GPDMA_SetupChannel(&cfg);
    GPDMA_ChannelStart(DAC_PLAYER_DMA_CHANNEL);
}

void DAC_PLAYER_Stop(void)
{
    GPDMA_ChannelStop(DAC_PLAYER_DMA_CHANNEL);
    DAC_UpdateValue(0);
    s_done = 1;
}

int DAC_PLAYER_Done(void)
{
    return s_done;
}

void DMA_IRQHandler(void)
{
    if (GPDMA_IntGetStatus(GPDMA_INTTC, DAC_PLAYER_DMA_CHANNEL) == SET)
    {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, DAC_PLAYER_DMA_CHANNEL);
        DAC_UpdateValue(0);
        s_done = 1;
    }

    if (GPDMA_IntGetStatus(GPDMA_INTERR, DAC_PLAYER_DMA_CHANNEL) == SET)
    {
        GPDMA_ClearIntPending(GPDMA_CLR_INTERR, DAC_PLAYER_DMA_CHANNEL);
        GPDMA_ChannelStop(DAC_PLAYER_DMA_CHANNEL);
        DAC_UpdateValue(0);
        s_done = 1;
    }
}
