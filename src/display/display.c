#include "display/display.h"


#include "lpc17xx_gpdma.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_ssp.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define WS2812_SPI_CLOCK_HZ         2410000UL
#define WS2812_DMA_CHANNEL          GPDMA_CH_3

/*
 * SPI symbol encoding:
 *
 * 0 -> 100
 * 1 -> 110
 *
 * Encoded MSB-first into 3 bytes
 */
#define WS2812_SYMBOL_0             0b100
#define WS2812_SYMBOL_1             0b110

/* -------------------------------------------------------------------------- */
/* Private Variables                                                          */
/* -------------------------------------------------------------------------- */

/*
 * Framebuffer is already SPI-expanded.
 *
 * 64 LEDs × 9 bytes = 576
 * Reset tail = 32 bytes
 * Total = 608 bytes
 */


/*
 * Runtime brightness
 * Default = 30%
 */
static uint8_t wsBrightness = WS2812_BRIGHTNESS_PERCENT;

/*
 * DMA self-loop descriptor.
 *
 * LPC17xx LLI format:
 * srcAddr
 * dstAddr
 * nextLLI
 * control
 */
typedef struct {
    uint32_t srcAddr;
    uint32_t dstAddr;
    uint32_t nextLLI;
    uint32_t control;
} WS2812_DMA_LLI_T;

static WS2812_DMA_LLI_T wsDMAlli;

/* -------------------------------------------------------------------------- */
/* Private Function Prototypes                                                */
/* -------------------------------------------------------------------------- */

static void display_initSSP0(void);
static void display_initDMA(void);

static void WS2812_EncodeColor(
    uint16_t ledIndex,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

static void WS2812_EncodeByte(
    uint8_t value,
    uint8_t* dst
);

static inline uint8_t WS2812_ApplyBrightness(uint8_t value);

/* -------------------------------------------------------------------------- */
/* Private Functions                                                          */
/* -------------------------------------------------------------------------- */

static inline uint8_t WS2812_ApplyBrightness(uint8_t value)
{
    return (uint8_t)(
        ((uint16_t)value * wsBrightness) / 100U
    );
}

/*
 * Serpentine XY mapping
 *
 * y=0
 * 0  1  2  3  4  5  6  7
 * 15 14 13 12 11 10 9  8
 * 16 17 ...
 */
uint16_t WS2812_XYToIndex(uint8_t x, uint8_t y)
{
    if (x >= WS2812_MATRIX_WIDTH ||
        y >= WS2812_MATRIX_HEIGHT)
    {
        return 0;
    }

    if ((y & 1U) == 0U)
    {
        return (y * WS2812_MATRIX_WIDTH) + x;
    }

    return (y * WS2812_MATRIX_WIDTH)
        + ((WS2812_MATRIX_WIDTH - 1U) - x);
}

/*
 * Encode one byte into 3 bytes using:
 *
 * 0 -> 100
 * 1 -> 110
 *
 * Example:
 *
 * 10110010
 *
 * becomes 24 SPI bits
 */
static void WS2812_EncodeByte(
    uint8_t value,
    uint8_t* dst
)
{
    uint32_t encoded = 0;

    for (int bit = 7; bit >= 0; bit--)
    {
        encoded <<= 3;

        if (value & (1U << bit))
        {
            encoded |= WS2812_SYMBOL_1;
        }
        else
        {
            encoded |= WS2812_SYMBOL_0;
        }
    }

    /*
     * Split 24-bit stream into 3 bytes
     */
    dst[0] = (encoded >> 16) & 0xFF;
    dst[1] = (encoded >> 8)  & 0xFF;
    dst[2] = encoded & 0xFF;
}

/*
 * Encode one LED directly into framebuffer.
 *
 * WS2812 uses:
 * GRB order
 *
 * Each LED occupies 9 bytes:
 *
 * G = 3 bytes
 * R = 3 bytes
 * B = 3 bytes
 */
static void WS2812_EncodeColor(
    uint16_t ledIndex,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    if (ledIndex >= WS2812_LED_COUNT)
    {
        return;
    }

    uint8_t* ledPtr =
        &wsFramebuffer[
            ledIndex * WS2812_BYTES_PER_LED
        ];

    /*
     * Apply global brightness
     */
    r = WS2812_ApplyBrightness(r);
    g = WS2812_ApplyBrightness(g);
    b = WS2812_ApplyBrightness(b);

    /*
     * GRB order
     */
    WS2812_EncodeByte(g, &ledPtr[0]);
    WS2812_EncodeByte(r, &ledPtr[3]);
    WS2812_EncodeByte(b, &ledPtr[6]);
}

static void WS2812_FillEncodedZero(void)
{

    uint8_t encoded[3];

    WS2812_EncodeByte(0x00, encoded);

    for (uint32_t i = 0; i < WS2812_LED_COUNT; i++)
    {
        uint8_t *ledPtr = &wsFramebuffer[i * WS2812_BYTES_PER_LED];

        // Fill GRB with encoded zero
        memcpy(&ledPtr[0], encoded, 3);
        memcpy(&ledPtr[3], encoded, 3);
        memcpy(&ledPtr[6], encoded, 3);
    }

    memset(&wsFramebuffer[WS2812_LED_COUNT * WS2812_BYTES_PER_LED],
           0,
           WS2812_FRAMEBUFFER_SIZE - (WS2812_LED_COUNT * WS2812_BYTES_PER_LED));
}
/* -------------------------------------------------------------------------- */
/* SSP0 Configuration                                                         */
/* -------------------------------------------------------------------------- */

static void display_initSSP0(void)
{
    PINSEL_CFG_T pinCfg;

    /*
     * P0.18 -> SSP0 MOSI
     */
    pinCfg.port       = 0;
    pinCfg.pin        = 18;
    pinCfg.func       = PINSEL_FUNC_10;
    pinCfg.mode       = PINSEL_PULLUP;
    pinCfg.openDrain  = DISABLE;

    PINSEL_ConfigPin(&pinCfg);

    SSP_CFG_Type sspCfg;
    SSP_ConfigStructInit(&sspCfg);

    sspCfg.ClockRate  = WS2812_SPI_CLOCK_HZ;
    sspCfg.Databit    = SSP_DATABIT_8;
    sspCfg.CPHA       = SSP_CPHA_FIRST;
    sspCfg.CPOL       = SSP_CPOL_LO;
    sspCfg.Mode       = SSP_MASTER_MODE;
    sspCfg.FrameFormat = SSP_FRAME_SPI;

    SSP_Init(LPC_SSP0, &sspCfg);

    /*
     * Enable SSP0 DMA TX
     */
    SSP_DMACmd(
        LPC_SSP0,
        SSP_DMA_TX,
        ENABLE
    );

    SSP_Cmd(LPC_SSP0, ENABLE);
}


static void display_initDMA(void)
{
    GPDMA_Channel_CFG_T dmaCfg;

    GPDMA_Init();

    /*
     * Self-looping LLI
     *
     * Source = framebuffer
     * Dest   = SSP0->DR
     * Next   = itself
     */
    wsDMAlli.srcAddr =
        (uint32_t)wsFramebuffer;

    wsDMAlli.dstAddr =
        (uint32_t)&LPC_SSP0->DR;

    wsDMAlli.nextLLI =
        (uint32_t)&wsDMAlli;

    wsDMAlli.control =
        GPDMA_DMACCxControl_TransferSize(
            WS2812_FRAMEBUFFER_SIZE
        )
        |
        GPDMA_DMACCxControl_SBSize(
            GPDMA_BSIZE_4
        )
        |
        GPDMA_DMACCxControl_DBSize(
            GPDMA_BSIZE_4
        )
        |
        GPDMA_DMACCxControl_SWidth(
            GPDMA_BYTE
        )
        |
        GPDMA_DMACCxControl_DWidth(
            GPDMA_BYTE
        )
        |
        GPDMA_DMACCxControl_SI;

    memset(&dmaCfg, 0, sizeof(dmaCfg));

    dmaCfg.channelNum   = WS2812_DMA_CHANNEL;
    dmaCfg.transferSize = WS2812_FRAMEBUFFER_SIZE;

    dmaCfg.type         = GPDMA_M2P;

    dmaCfg.srcMemAddr   =
        (uint32_t)wsFramebuffer;

    dmaCfg.dstMemAddr   = 0;

    dmaCfg.srcConn      = GPDMA_SSP0_Tx;
    dmaCfg.dstConn      = GPDMA_SSP0_Tx;

    dmaCfg.src.width    = GPDMA_BYTE;
    dmaCfg.src.burst    = GPDMA_BSIZE_4;
    dmaCfg.src.increment = ENABLE;

    dmaCfg.dst.width    = GPDMA_BYTE;
    dmaCfg.dst.burst    = GPDMA_BSIZE_4;
    dmaCfg.dst.increment = DISABLE;

    dmaCfg.intTC        = DISABLE;
    dmaCfg.intErr       = DISABLE;

    dmaCfg.linkedList =
        (uint32_t)&wsDMAlli;

    GPDMA_SetupChannel(&dmaCfg);
}


void display_init(void)
{

    memset(
        wsFramebuffer,
        0,
        sizeof(wsFramebuffer)
    );
    WS2812_FillEncodedZero();

    display_initSSP0();
    display_initDMA();

    display_clear();
}

void display_start(void)
{
    GPDMA_ChannelStart(
        WS2812_DMA_CHANNEL
    );
}

void display_stop(void)
{
    GPDMA_ChannelGracefulStop(
        WS2812_DMA_CHANNEL
    );
}

void display_setLED(
    uint8_t x,
    uint8_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    if (x >= WS2812_MATRIX_WIDTH ||
        y >= WS2812_MATRIX_HEIGHT)
    {
        return;
    }

    uint16_t index =
        WS2812_XYToIndex(x, y);

    WS2812_EncodeColor(
        index,
        r,
        g,
        b
    );
}

void WS2812_SetIndex(
    uint16_t index,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    if (index >= WS2812_LED_COUNT)
    {
        return;
    }

    WS2812_EncodeColor(
        index,
        r,
        g,
        b
    );
}

void WS2812_Fill(
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    for (uint16_t i = 0;
         i < WS2812_LED_COUNT;
         i++)
    {
        WS2812_EncodeColor(
            i,
            r,
            g,
            b
        );
    }
}

void display_clear(void)
{
    WS2812_FillEncodedZero();
}


uint8_t* WS2812_GetFramebuffer(void)
{
    return wsFramebuffer;
}

uint32_t WS2812_GetFramebufferSize(void)
{
    return WS2812_FRAMEBUFFER_SIZE;
}


void WS2812_SetBrightness(
    uint8_t brightness
)
{
    if (brightness > 100U)
    {
        brightness = 100U;
    }

    wsBrightness = brightness;
}

uint8_t WS2812_GetBrightness(void)
{
    return wsBrightness;
}
