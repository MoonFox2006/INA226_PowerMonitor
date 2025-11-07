#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_tim.h>
#include <ch32v00x_misc.h>
#endif
#include "micros.h"

static volatile uint32_t _us = 0;

void MICROS_Init(void) {
#ifdef USE_SPL
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    {
        TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

        TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
        TIM_TimeBaseInitStructure.TIM_Prescaler = FCPU / 1000000 - 1; // 1 us.
        TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    }
    {
        NVIC_InitTypeDef NVIC_InitStructure = { 0 };

        NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }

    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
#else
    RCC->APB2PCENR |= RCC_TIM1EN;

    NVIC_SetPriority(TIM1_UP_IRQn, (0 << 7) | (0 << 6));
    NVIC_EnableIRQ(TIM1_UP_IRQn);

    TIM1->CTLR1 = TIM_ARPE;
    TIM1->ATRLR = 0xFFFF;
    TIM1->PSC = FCPU / 1000000 - 1; // 1 us.
    TIM1->RPTCR = 0;
    TIM1->SWEVGR = TIM_UG;

    TIM1->DMAINTENR |= TIM_UIE;
    TIM1->CTLR1 |= TIM_CEN;
#endif
}

inline __attribute__((always_inline)) uint32_t MICROS_Get(void) {
#ifdef USE_SPL
    return _us | TIM_GetCounter(TIM1);
#else
    return _us | TIM1->CNT;
#endif
}

void __attribute__((interrupt("WCH-Interrupt-fast"))) TIM1_UP_IRQHandler(void) {
    _us += 0x10000;
#ifdef USE_SPL
    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
#else
    TIM1->INTFR = ~TIM_UIF;
#endif
}
