/*
 * game.h
 *
 *  Created on: 9 jun. 2026
 *      Author: Admin
 */

#ifndef GAME_GAME_H_
#define GAME_GAME_H_

#include <stdint.h>

typedef struct
{
    uint32_t time_ms;
    uint8_t lane;
    uint16_t length_ms;
} GameNote_t;

void game_init(void);

void game_set_song(
    const GameNote_t *notes,
    uint32_t count
);

void game_start(uint32_t start);
void game_stop(void);

void game_update(
    uint32_t current_time_ms
);


#endif /* GAME_GAME_H_ */
