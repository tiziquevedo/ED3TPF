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

#define WS2812_BYTES_PER_LED        9


#define WS2812_RESET_BYTES          100

#define WS2812_FRAMEBUFFER_SIZE \
    ((WS2812_LED_COUNT * WS2812_BYTES_PER_LED) + WS2812_RESET_BYTES)


#define WS2812_BRIGHTNESS_PERCENT   100U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812_Color_T;


void display_init(void);


void display_start(void);


void display_stop(void);


void display_clear(void);

void display_test(void);

void WS2812_Fill(uint8_t r, uint8_t g, uint8_t b);

void display_setLED(
    uint8_t x,
    uint8_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

void WS2812_SetIndex(
    uint16_t index,
    uint8_t r,
    uint8_t g,
    uint8_t b
);


uint8_t* WS2812_GetFramebuffer(void);

uint32_t WS2812_GetFramebufferSize(void);

uint16_t WS2812_XYToIndex(uint8_t x, uint8_t y);


void WS2812_SetBrightness(uint8_t brightness);

uint8_t WS2812_GetBrightness(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_MATRIX_H_ */
