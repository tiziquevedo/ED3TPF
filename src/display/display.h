#ifndef WS2812_MATRIX_H_
#define WS2812_MATRIX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define WS2812_MATRIX_WIDTH         8
#define WS2812_MATRIX_HEIGHT        8
#define WS2812_LED_COUNT            (WS2812_MATRIX_WIDTH * WS2812_MATRIX_HEIGHT)

/*
 * SPI encoding:
 *
 * 0 -> 100
 * 1 -> 110
 *
 * 24 bits/LED * 3 SPI bits = 72 bits = 9 bytes per LED
 */
#define WS2812_BYTES_PER_LED        9

/*
 * WS2812 reset time >50us
 *
 * At 2.4MHz:
 * byte time = 8 / 2.4MHz = 3.33us
 *
 * 32 bytes = ~106us reset
 */
#define WS2812_RESET_BYTES          32

#define WS2812_FRAMEBUFFER_SIZE \
    ((WS2812_LED_COUNT * WS2812_BYTES_PER_LED) + WS2812_RESET_BYTES)

/*
 * Fixed brightness scaling
 * 30%
 */
#define WS2812_BRIGHTNESS_PERCENT   30U

/* -------------------------------------------------------------------------- */
/* Types                                                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812_Color_T;

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize SSP0, DMA and framebuffer.
 *
 * Configures:
 * - SSP0 @ 2.4MHz
 * - P0.18 as MOSI
 * - DMA self-looping transfer
 *
 * Does not start transmission automatically.
 */
void WS2812_Init(void);

/**
 * @brief Start continuous DMA streaming.
 */
void WS2812_Start(void);

/**
 * @brief Stop DMA streaming.
 */
void WS2812_Stop(void);

/**
 * @brief Turn off all LEDs.
 */
void WS2812_Clear(void);

/**
 * @brief Fill all LEDs with same color.
 *
 * @param r Red   (0-255)
 * @param g Green (0-255)
 * @param b Blue  (0-255)
 */
void WS2812_Fill(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set LED by matrix coordinates.
 *
 * Automatically applies:
 * - serpentine mapping
 * - GRB order
 * - 30% brightness scaling
 *
 * Origin:
 *
 * y=0
 * 0  1  2  3  4  5  6  7
 * 15 14 13 12 11 10 9  8
 * 16 17 18 ...
 *
 * @param x X coordinate [0-7]
 * @param y Y coordinate [0-7]
 * @param r Red   (0-255)
 * @param g Green (0-255)
 * @param b Blue  (0-255)
 */
void WS2812_SetLED(
    uint8_t x,
    uint8_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

/**
 * @brief Set LED by linear index.
 *
 * Index range:
 * [0 - 63]
 *
 * Physical ordering follows
 * serpentine matrix layout.
 *
 * @param index LED index
 * @param r Red
 * @param g Green
 * @param b Blue
 */
void WS2812_SetIndex(
    uint16_t index,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

/**
 * @brief Get raw framebuffer pointer.
 *
 * Useful for debugging or
 * advanced effects.
 *
 * NOTE:
 * Framebuffer is already SPI-encoded.
 *
 * @return framebuffer pointer
 */
uint8_t* WS2812_GetFramebuffer(void);

/**
 * @brief Get framebuffer size in bytes.
 *
 * @return size in bytes
 */
uint32_t WS2812_GetFramebufferSize(void);

/**
 * @brief Convert x,y coordinate to serpentine index.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @return linear LED index
 */
uint16_t WS2812_XYToIndex(uint8_t x, uint8_t y);

/**
 * @brief Set brightness percentage.
 *
 * Optional runtime brightness.
 * Default = 30%
 *
 * @param brightness 0-100
 */
void WS2812_SetBrightness(uint8_t brightness);

/**
 * @brief Get current brightness.
 *
 * @return brightness percent
 */
uint8_t WS2812_GetBrightness(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_MATRIX_H_ */
