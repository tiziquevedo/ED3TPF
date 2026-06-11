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

volatile uint32_t game_time_ms = 0;
volatile uint32_t button_times[4] = {0, 0, 0, 0};

#define BTN0_PIN  17
#define BTN1_PIN  15
#define BTN2_PIN  16
#define BTN3_PIN  23

static void buttons_init(void)
{
    LPC_GPIO0->FIODIR &= ~((1u << BTN0_PIN) |
                           (1u << BTN1_PIN) |
                           (1u << BTN2_PIN) |
                           (1u << BTN3_PIN));

    LPC_PINCON->PINMODE0 &= ~(3u << 30);
    LPC_PINCON->PINMODE1 &= ~(3u << 0);
    LPC_PINCON->PINMODE1 &= ~(3u << 2);
    LPC_PINCON->PINMODE1 &= ~(3u << 14);

    LPC_GPIOINT->IO0IntEnF |= (1u << BTN0_PIN) |
                               (1u << BTN1_PIN) |
                               (1u << BTN2_PIN) |
                               (1u << BTN3_PIN);

    LPC_GPIOINT->IO0IntEnR &= ~((1u << BTN0_PIN) |
                                (1u << BTN1_PIN) |
                                (1u << BTN2_PIN) |
                                (1u << BTN3_PIN));

    NVIC_SetPriority(EINT3_IRQn, 1);
    NVIC_EnableIRQ(EINT3_IRQn);
}

void EINT3_IRQHandler(void)
{
    uint32_t fallen = LPC_GPIOINT->IO0IntStatF;

    if (fallen & (1u << BTN0_PIN))
    {
        button_times[0] = game_time_ms;
        LPC_GPIOINT->IO0IntClr = (1u << BTN0_PIN);
    }
    if (fallen & (1u << BTN1_PIN))
    {
        button_times[1] = game_time_ms;
        LPC_GPIOINT->IO0IntClr = (1u << BTN1_PIN);
    }
    if (fallen & (1u << BTN2_PIN))
    {
        button_times[2] = game_time_ms;
        LPC_GPIOINT->IO0IntClr = (1u << BTN2_PIN);
    }
    if (fallen & (1u << BTN3_PIN))
    {
        button_times[3] = game_time_ms;
        LPC_GPIOINT->IO0IntClr = (1u << BTN3_PIN);
    }
}

int main(void)
{
    SystemInit();

    display_init();
    display_start();

    DAC_PLAYER_Init(8000);
    buttons_init();

    game_init();

    game_set_song(
        generated_song,
        sizeof(generated_song) / sizeof(generated_song[0])
    );

    setup_timer(&game_time_ms);
    game_start(game_time_ms);
    DAC_PLAYER_Play(song_audio, song_audio_length);

    while (1)
    {
        volatile uint32_t count = 100000;
        while (count--) __asm volatile ("nop");

        uint32_t pressed[4];
        __disable_irq();
        pressed[0] = button_times[0]; button_times[0] = 0;
        pressed[1] = button_times[1]; button_times[1] = 0;
        pressed[2] = button_times[2]; button_times[2] = 0;
        pressed[3] = button_times[3]; button_times[3] = 0;
        __enable_irq();

        game_update(game_time_ms, pressed, 4);
    }
}
