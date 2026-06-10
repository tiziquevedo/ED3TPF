#ifndef AUDIO_AUDIO_H_
#define AUDIO_AUDIO_H_

#include <stdint.h>


#define DAC_PLAYER_DMA_CHANNEL   (2)
#define DAC_PLAYER_LLI_CHUNK     (4095U)
#define DAC_PLAYER_MAX_LLI       (256U)


void DAC_PLAYER_Init(uint32_t sampleRateHz);

void DAC_PLAYER_Play(const uint8_t* samples, uint32_t length);


void DAC_PLAYER_Stop(void);


int  DAC_PLAYER_Done(void);


void DAC_PLAYER_DMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_AUDIO_H_ */
