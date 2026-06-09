/*
 * Copyright 2022 NXP
 * NXP confidential.
 * This software is owned or controlled by NXP and may only be used strictly
 * in accordance with the applicable license terms.  By expressly accepting
 * such terms or by downloading, installing, activating and/or otherwise using
 * the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to
 * be bound by the applicable license terms, then you may not retain, install,
 * activate or otherwise use the software.
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>

#include <stdio.h>

#include "display/display.h"


int main(void)
{
    SystemInit();

    display_init();
    display_start();

    display_clear();


    display_SetLED(0, 0, 255, 0, 0);
    display_SetLED(1, 0, 0, 255, 0);
    display_SetLED(2, 0, 0, 0, 255);

    while(1)
    {

    }
}
