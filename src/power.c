//#define USE_EEPROM
#define USE_UART

#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_exti.h>
#include <ch32v00x_tim.h>
#include <ch32v00x_misc.h>
#endif
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "ina226.h"
#include "micros.h"
#include "buttons.h"
#ifdef USE_EEPROM
#include "eeprom.h"
#endif
#ifdef USE_UART
#include "uart.h"
#endif
#include "strutils.h"

#define DEF_SHUNT       150 // R150

#define MIN_AVG         AVG4
#define MAX_AVG         AVG1024
#define DEF_AVG         AVG64

typedef struct __attribute__((__packed__)) {
    uint8_t avg;
    bool flip;
} params_t;

static volatile uint32_t inaTime = 0;
static volatile bool inaReady = false;

static void initAlert(void) {
#ifdef USE_SPL
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);

    {
        GPIO_InitTypeDef GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Pin = 1 << 4;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIOC, &GPIO_InitStructure);
    }

    {
        EXTI_InitTypeDef EXTI_InitStructure = { 0 };

        GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, 4);
        EXTI_InitStructure.EXTI_Line = 1 << 4;
        EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStructure.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStructure);
    }

    {
        NVIC_InitTypeDef NVIC_InitStructure = { 0 };

        NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
#else
    RCC->APB2PCENR |= (RCC_AFIOEN | RCC_IOPCEN);

    UPDATE_REG32(&GPIOC->CFGLR, ~(0x0F << (4 * 4)), (0x08 << (4 * 4)));
    GPIOC->BSHR = 1 << 4;

    UPDATE_REG32(&AFIO->EXTICR, ~(0x03 << (4 * 2)), 2 << (4 * 2));

    EXTI->RTENR &= ~(1 << 4);
    EXTI->FTENR |= 1 << 4;
    EXTI->INTENR |= (1 << 4);

    NVIC_SetPriority(EXTI7_0_IRQn, (0 << 7) | (0 << 6));
    NVIC_EnableIRQ(EXTI7_0_IRQn);
#endif
}

static void normalizeU32(char *str, uint32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (value < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%u.%03u %s%s", (uint16_t)(value / 1000), (uint16_t)(value % 1000), prefix, suffix);
    s = u16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, value % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

static void normalizeI32(char *str, int32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (labs(value) < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%d.%03u %s%s", (int16_t)(value / 1000), (uint16_t)(labs(value) % 1000), prefix, suffix);
    s = i16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, labs(value) % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

void power(void) {
    const char PROGRESS[4] = { '-', '\\', '|', '/' };

    uint64_t totalMicroAmps = 0;
    uint64_t totalTime = 0;
    params_t params;
    uint8_t progress = 0;

#ifdef USE_EEPROM
    EEPROM_Init(&params, sizeof(params));
    if ((params.avg < MIN_AVG) || (params.avg > MAX_AVG) || (params.flip > true)) { // Wrong or empty EEPROM
        params.avg = DEF_AVG;
        params.flip = true;
    }
#else
    params.avg = DEF_AVG;
    params.flip = true;
#endif
    if (! params.flip)
        ssd1306_flip(params.flip);

    MICROS_Init();

    initAlert();

#ifdef USE_UART
    uartInit();
#endif

    if ((! ina226_begin(DEF_SHUNT)) || (! ina226_measure(true, params.avg, US8244, US8244))) {
        screen_clear();
        screen_printstr_x2("INA226", 0, 16, 1);
        screen_printstr_x2("not ready!", 0, 32, 1);
        ssd1306_flush(true);
#ifdef USE_UART
        uartPrint("INA226 not ready!\r\n");
#endif
        while (1) {}
    } else {
        screen_clear();
        screen_printstr_x2("INA226...", 0, 24, 1);
        ssd1306_flush(true);
    }

    BUTTONS_Init(0); // PA2 (A0)

    while (1) {
        {
            uint8_t btn;

            if (BUTTONS_Read(&btn)) {
                uint8_t a = params.avg;
                bool f = params.flip;

                do {
                    switch (btn) {
                        case BTN_CLICK1: // DEC
                            if (params.flip) {
                                if (a > MIN_AVG)
                                    --a;
                                else
                                    a = MAX_AVG;
                            } else {
                                if (a < MAX_AVG)
                                    ++a;
                                else
                                    a = MIN_AVG;
                            }
                            break;
                        case BTN_LONGCLICK1:
                            a = params.flip ? MIN_AVG : MAX_AVG;
                            break;
                        case BTN_CLICK2: // OK (clear)
                            totalMicroAmps = 0;
                            totalTime = 0;
                            break;
                        case BTN_LONGCLICK2: // OK long (flip)
                            f = ! f;
                            break;
                        case BTN_CLICK3: // INC
                            if (params.flip) {
                                if (a < MAX_AVG)
                                    ++a;
                                else
                                    a = MIN_AVG;
                            } else {
                                if (a > MIN_AVG)
                                    --a;
                                else
                                    a = MAX_AVG;
                            }
                            break;
                        case BTN_LONGCLICK3:
                            a = params.flip ? MAX_AVG : MIN_AVG;
                            break;
                    }
                } while (BUTTONS_Read(&btn));
                if ((params.avg != a) || (params.flip != f)) {
                    if (params.flip != f) {
                        params.flip = f;
                        ssd1306_flip(params.flip);
                    }
                    if (params.avg != a) {
                        params.avg = a;
                        ina226_measure(true, params.avg, US8244, US8244);
                        inaReady = false;
                    }
#ifdef USE_EEPROM
                    EEPROM_Flush();
#endif
                }
            }
        }

//        if (ina226_ready()) {
        if (inaReady && ina226_ready()) {
#ifdef USE_UART
            char ustr[10];
#endif
            char str[16];
            char *s;
            int32_t microAmps;
            uint32_t microWatts;
            uint16_t milliVolts;

            inaReady = false;
            if (inaTime) {
                milliVolts = ina226_getMilliVolts();
                microAmps = ina226_getMicroAmps(DEF_SHUNT);
                microWatts = ina226_getMicroWatts(DEF_SHUNT);
                totalMicroAmps += labs(microAmps);
                totalTime += inaTime;

#ifdef USE_UART
                itoa(microAmps, ustr, 10);
                strcat(ustr, "\r\n");
                uartPrint(ustr);
#endif

                screen_clear();
                s = u16str(str, milliVolts / 1000, 1);
                *s++ = '.';
                s = u16str(s, milliVolts % 1000, 100);
                *s++ = ' ';
                *s++ = 'V';
                *s = '\0';
                screen_printstr_x2(str, 0, 0, 1);
                normalizeI32(str, microAmps, "A");
                screen_printstr_x2(str, 0, FONT_HEIGHT * 2, 1);
                normalizeU32(str, microWatts, "W");
                screen_printstr_x2(str, 0, FONT_HEIGHT * 4, 1);
                normalizeU32(str, (uint32_t)((totalMicroAmps * 1000000ULL / totalTime) / 3600), "A/h"); // uA/s * 1/3600 = uA/h
                screen_printstr_x2(str, 0, FONT_HEIGHT * 6, 1);
                s = str;
                *s++ = 'R';
                s = u16str(s, DEF_SHUNT, 100);
                *s++ = ' ';
                *s++ = PROGRESS[progress];
                *s = '\0';
                screen_printstr(str, SCREEN_WIDTH - font_strwidth(str, false), 0, 1);
                if (++progress >= ARRAY_SIZE(PROGRESS))
                    progress = 0;
                normalizeU32(str, inaTime, "s");
                screen_printstr(str, SCREEN_WIDTH - font_strwidth(str, false), FONT_HEIGHT, 1);
                ssd1306_flush(true);
            }
        }
    }
}

void __attribute__((interrupt("WCH-Interrupt-fast"))) EXTI7_0_IRQHandler(void) {
    static uint32_t lastTime = 0;

    uint32_t us = MICROS_Get();

    if (lastTime)
        inaTime = us - lastTime;
    else
        inaTime = 0;
    inaReady = true;
    lastTime = us;
#ifdef USE_SPL
    EXTI_ClearITPendingBit(1 << 4);
#else
    EXTI->INTFR = 1 << 4;
#endif
}

