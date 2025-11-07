#define USE_QUEUE

#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_misc.h>
#include <ch32v00x_tim.h>
#include <ch32v00x_adc.h>
#endif
#include "utils.h"
#include "buttons.h"
#ifdef USE_QUEUE
#include "queue.h"
#endif

#define AVG_COUNT       2
#define ADC_PERIOD      10 // 10 ms.
#define ADC_TOLERANCE   20

#ifdef USE_QUEUE
#define QUEUE_DEPTH     4 // Must be power of 2!
#define ITEM_SIZE       1

static uint8_t _queue[QUEUE_SIZE(QUEUE_DEPTH, ITEM_SIZE)];
#else
static volatile uint8_t _btns_state = BTN_NONE;
#endif

void BUTTONS_Init(uint8_t adc_channel) {
#ifdef USE_SPL
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div2); // 24 MHz

    {
        GPIO_InitTypeDef GPIO_InitStructure = { 0 };

        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
        if (adc_channel <= 1) { // PA2, PA1
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
            GPIO_InitStructure.GPIO_Pin = adc_channel == 0 ? GPIO_Pin_2 : GPIO_Pin_1;
            GPIO_Init(GPIOA, &GPIO_InitStructure);
        } else if (adc_channel == 2) { // PC4
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
            GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
            GPIO_Init(GPIOC, &GPIO_InitStructure);
        } else { // adc_channel > 2
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
            if (adc_channel == 3)
                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
            else if (adc_channel == 4)
                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
            else if (adc_channel == 5)
                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
            else if (adc_channel == 6)
                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
            else // adc_channel == 7
                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
            GPIO_Init(GPIOD, &GPIO_InitStructure);
        }
    }

    {
        ADC_InitTypeDef ADC_InitStructure = { 0 };

        ADC_DeInit(ADC1);
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
#if AVG_COUNT > 1
        ADC_InitStructure.ADC_ScanConvMode = ENABLE;
#else
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
#endif
        ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
        ADC_Init(ADC1, &ADC_InitStructure);
    }

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) {}
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) {}

    ADC_InjectedSequencerLengthConfig(ADC1, AVG_COUNT);
#if AVG_COUNT > 1
    for (uint8_t i = 1; i <= AVG_COUNT; ++i)
        ADC_InjectedChannelConfig(ADC1, adc_channel, i, ADC_SampleTime_241Cycles);
#else
    ADC_InjectedChannelConfig(ADC1, adc_channel, 1, ADC_SampleTime_241Cycles);
#endif
    ADC_ExternalTrigInjectedConvConfig(ADC1, ADC_ExternalTrigInjecConv_T2_CC4);
    ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);

    ADC_ITConfig(ADC1, ADC_IT_JEOC, ENABLE);

    {
        NVIC_InitTypeDef NVIC_InitStructure = { 0 };

        NVIC_InitStructure.NVIC_IRQChannel = ADC_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }

    {
        TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

        TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
        TIM_TimeBaseInitStructure.TIM_Prescaler = FCPU / 1000000 - 1; // 1 us.
        TIM_TimeBaseInitStructure.TIM_Period = 1000 * ADC_PERIOD / 2 - 1; // 5 ms.
        TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    }

    {
        TIM_OCInitTypeDef TIM_OCInitStructure;

        TIM_OCStructInit(&TIM_OCInitStructure);
        TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Toggle;
        TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
        TIM_OCInitStructure.TIM_Pulse = 1000 * ADC_PERIOD / 4 - 1; // 2.5 ms.
        TIM_OC4Init(TIM2, &TIM_OCInitStructure);

    }

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
#else
    RCC->APB1PCENR |= RCC_TIM2EN;
    RCC->APB2PCENR |= RCC_ADC1EN;

    UPDATE_REG32(&RCC->CFGR0, 0xFFFF07FF, 0); // CFGR0_ADCPRE_Reset_Mask, 24 MHz

    if (adc_channel <= 1) { // PA2, PA1
        RCC->APB2PCENR |= RCC_IOPAEN;
        GPIOA->CFGLR &= ~(uint32_t)(0x0F << (4 * (adc_channel == 0 ? 2 : 1)));
    } else if (adc_channel == 2) { // PC4
        RCC->APB2PCENR |= RCC_IOPCEN;
        GPIOC->CFGLR &= ~(uint32_t)(0x0F << (4 * 4));
    } else { // adc_channel > 2
        RCC->APB2PCENR |= RCC_IOPDEN;
        if (adc_channel == 3)
            GPIOD->CFGLR &= ~(uint32_t)(0x0F << (4 * 2));
        else if (adc_channel == 4)
            GPIOD->CFGLR &= ~(uint32_t)(0x0F << (4 * 3));
        else if (adc_channel == 5)
            GPIOD->CFGLR &= ~(uint32_t)(0x0F << (4 * 5));
        else if (adc_channel == 6)
            GPIOD->CFGLR &= ~(uint32_t)(0x0F << (4 * 6));
        else // adc_channel == 7
            GPIOD->CFGLR &= ~(uint32_t)(0x0F << (4 * 4));
    }

    RCC->APB2PRSTR |= RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

#if AVG_COUNT > 1
    ADC1->CTLR1 = ADC_CALVOLSELECT_0 | ADC_SCAN | ADC_JEOCIE;
#else
    ADC1->CTLR1 = ADC_CALVOLSELECT_0 | ADC_JEOCIE;
#endif
    ADC1->CTLR2 = ADC_JEXTTRIG | (ADC_JEXTSEL_0 | ADC_JEXTSEL_1);
    ADC1->RSQR1 = 0;

    ADC1->CTLR2 |= ADC_ADON;

    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL) {}
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) {}

    ADC1->SAMPTR2 = 0x07 << (3 * adc_channel);
#if AVG_COUNT == 1
    ADC1->ISQR = (0 << 20) | (adc_channel << 15);
#elif AVG_COUNT == 2
    ADC1->ISQR = (1 << 20) | (adc_channel << 15) | (adc_channel << 10);
#elif AVG_COUNT == 3
    ADC1->ISQR = (2 << 20) | (adc_channel << 15) | (adc_channel << 10) | (adc_channel << 5);
#elif AVG_COUNT == 4
    ADC1->ISQR = (3 << 20) | (adc_channel << 15) | (adc_channel << 10) | (adc_channel << 5) | adc_channel;
#endif

    NVIC_SetPriority(ADC_IRQn, (0 << 7) | (1 << 6));
    NVIC_EnableIRQ(ADC_IRQn);

    TIM2->CTLR1 = TIM_ARPE;
    TIM2->ATRLR = 1000 * ADC_PERIOD / 2 - 1; // 5 ms.
    TIM2->PSC = FCPU / 1000000 - 1; // 1 us.
    TIM2->SWEVGR = TIM_UG;

    UPDATE_REG16(&TIM2->CCER, ~(TIM_CC4E | TIM_CC4P), TIM_CC4E);
    UPDATE_REG16(&TIM2->CHCTLR2, ~(TIM_OC4M | TIM_CC4S | TIM_OC4PE), TIM_OC4M_1 | TIM_OC4M_0);
    TIM2->CH4CVR = 1000 * ADC_PERIOD / 4 - 1; // 2.5 ms.

    TIM2->BDTR |= TIM_MOE;
    TIM2->CTLR1 |= TIM_CEN;
#endif

#ifdef USE_QUEUE
    QUEUE_init(_queue, QUEUE_DEPTH, ITEM_SIZE);
#else
    BUTTONS_Clear();
#endif
}

void BUTTONS_Done(void) {
#ifdef USE_SPL
    TIM_Cmd(TIM2, DISABLE);

    NVIC_DisableIRQ(ADC_IRQn);

    ADC_ITConfig(ADC1, ADC_IT_JEOC, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
#else
    TIM2->CTLR1 &= ~TIM_CEN;

    NVIC_DisableIRQ(ADC_IRQn);

    ADC1->CTLR1 &= ~ADC_JEOCIE;
    ADC1->CTLR2 &= ~ADC_ADON;
#endif
}

inline __attribute__((always_inline)) void BUTTONS_Clear(void) {
#ifdef USE_QUEUE
    QUEUE_clear(_queue);
#else
    _btns_state = BTN_NONE;
#endif
}

#ifdef USE_QUEUE
inline __attribute__((always_inline)) bool BUTTONS_Read(uint8_t *state) {
    return QUEUE_get(_queue, state);
#else
bool BUTTONS_Read(uint8_t *state) {
    bool result = _btns_state != BTN_NONE;

    if (state)
        *state = _btns_state;
    _btns_state = BTN_NONE;
    return result;
#endif
}

void __attribute__((interrupt("WCH-Interrupt-fast"))) ADC1_IRQHandler() {
#ifdef USE_SPL
    if (ADC_GetITStatus(ADC1, ADC_IT_JEOC)) {
#else
    if (ADC1->STATR & ADC_JEOC) {
#endif
        static uint8_t counter = 0;
        static uint8_t last_btn = 0;

        uint16_t adc;
#ifdef USE_QUEUE
        uint8_t state = BTN_NONE;
#endif

#ifdef USE_SPL
        adc = ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_1);
#if AVG_COUNT > 1
        adc += ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_2);
#if AVG_COUNT > 2
        adc += ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_3);
#endif
#if AVG_COUNT > 3
        adc += ADC_GetInjectedConversionValue(ADC1, ADC_InjectedChannel_4);
#endif
        adc /= AVG_COUNT;
#endif
#else // #ifdef USE_SPL
        adc = ADC1->IDATAR1;
#if AVG_COUNT > 1
        adc += ADC1->IDATAR2;
#if AVG_COUNT > 2
        adc += ADC1->IDATAR3;
#endif
#if AVG_COUNT > 3
        adc += ADC1->IDATAR4;
#endif
        adc /= AVG_COUNT;
#endif
#endif

        if (adc > 1023 / 2 - ADC_TOLERANCE) // 1 / 2
            adc = 1;
#if BTN_COUNT > 1
        else if (adc > 1023 / 3 - ADC_TOLERANCE) // 1 / 3
            adc = 2;
#endif
#if BTN_COUNT > 2
        else if (adc > 1023 / 4 - ADC_TOLERANCE) // 1 / 4
            adc = 3;
#endif
#if BTN_COUNT > 3
        else if (adc > 1023 / 5 - ADC_TOLERANCE) // 1 / 5
            adc = 4;
#endif
        else
            adc = 0;

        if (adc != last_btn) { // Another button pressed/released
            if (last_btn) { // Any button released
                if (counter >= LONGPRESS_TIME / ADC_PERIOD - 1)
#ifdef USE_PRESS_STATES
#ifdef USE_QUEUE
                    state = BTN_LONGCLICK1 + ((last_btn - 1) << 2);
#else
                    _btns_state = BTN_LONGCLICK1 + ((last_btn - 1) << 2);
#endif
#else // #ifdef USE_PRESS_STATES
#ifdef USE_QUEUE
                    state = BTN_LONGCLICK1 + ((last_btn - 1) << 1);
#else
                    _btns_state = BTN_LONGCLICK1 + ((last_btn - 1) << 1);
#endif
#endif
                else if (counter >= DEBOUNCE_TIME / ADC_PERIOD - 1)
#ifdef USE_PRESS_STATES
#ifdef USE_QUEUE
                    state = BTN_CLICK1 + ((last_btn - 1) << 2);
#else
                    _btns_state = BTN_CLICK1 + ((last_btn - 1) << 2);
                else
                    _btns_state = BTN_NONE;
#endif
#else // #ifdef USE_PRESS_STATES
#ifdef USE_QUEUE
                    state = BTN_CLICK1 + ((last_btn - 1) << 1);
#else
                    _btns_state = BTN_CLICK1 + ((last_btn - 1) << 1);
                else
                    _btns_state = BTN_NONE;
#endif
#endif
            }
            last_btn = adc;
            counter = 0;
        } else {
            if (adc) { // Any button pressed
                if (counter < LONGPRESS_TIME / ADC_PERIOD - 1) {
                    ++counter;
#ifdef USE_PRESS_STATES
                    if (counter == LONGPRESS_TIME / ADC_PERIOD - 1)
#ifdef USE_QUEUE
                        state = BTN_LONGPRESS1 + ((adc - 1) << 2);
#else
                        _btns_state = BTN_LONGPRESS1 + ((adc - 1) << 2);
#endif
                    else if (counter == DEBOUNCE_TIME / ADC_PERIOD - 1)
#ifdef USE_QUEUE
                        state = BTN_PRESS1 + ((adc - 1) << 2);
#else
                        _btns_state = BTN_PRESS1 + ((adc - 1) << 2);
#endif
#endif
                }
            }
        }

#ifdef USE_QUEUE
        if (state != BTN_NONE) {
            QUEUE_put(_queue, &state);
        }
#endif

#ifdef USE_SPL
        ADC_ClearITPendingBit(ADC1, ADC_IT_JEOC);
#else
        ADC1->STATR = ~(uint32_t)ADC_JEOC;
#endif
    }
}
