
#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include "mstimer/mstimer.h"
#include "lpc17xx_timer.h"
#include <stdio.h>

uint32_t *timeReg;
_Bool  timerSetup = 0;


void setup_timer(uint32_t *timeMs){
	if(timerSetup){return;}
	timeReg = timeMs;

	static TIM_TIMERCFG_T tmrconf= {0};
	tmrconf.prescaleOpt= TIM_US;
	tmrconf.prescaleValue = 1000;

	TIM_InitTimer(LPC_TIM0, &tmrconf);

	TIM_MATCHCFG_T mr0Conf= {0};
	mr0Conf.channel =0;
	mr0Conf.intEn= ENABLE;
	mr0Conf.stopEn= DISABLE;
	mr0Conf.resetEn= ENABLE;
	mr0Conf.extOpt= 0;
	mr0Conf.matchValue= 0;

	TIM_ConfigMatch(LPC_TIM0, &mr0Conf);

	NVIC_EnableIRQ(TIMER0_IRQn);
	TIM_Enable(LPC_TIM0);

}


void TIMER0_IRQHandler(){
	(*timeReg)++;
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
}
