#pragma once

#include <stdint.h>
#include <stdbool.h>

#define BTN_COUNT   3

#if (BTN_COUNT < 1) || (BTN_COUNT > 4)
#error "Illegal button count!"
#endif

//#define USE_PRESS_STATES

#define DEBOUNCE_TIME   50
#define LONGPRESS_TIME  500

enum { BTN_NONE
#ifdef USE_PRESS_STATES
    , BTN_PRESS1
#endif
    , BTN_CLICK1
#ifdef USE_PRESS_STATES
    , BTN_LONGPRESS1
#endif
    , BTN_LONGCLICK1
#if BTN_COUNT > 1
#ifdef USE_PRESS_STATES
    , BTN_PRESS2
#endif
    , BTN_CLICK2
#ifdef USE_PRESS_STATES
    , BTN_LONGPRESS2
#endif
    , BTN_LONGCLICK2
#endif
#if BTN_COUNT > 2
#ifdef USE_PRESS_STATES
    , BTN_PRESS3
#endif
    , BTN_CLICK3
#ifdef USE_PRESS_STATES
    , BTN_LONGPRESS3
#endif
    , BTN_LONGCLICK3
#endif
#if BTN_COUNT > 3
#ifdef USE_PRESS_STATES
    , BTN_PRESS4
#endif
    , BTN_CLICK4
#ifdef USE_PRESS_STATES
    , BTN_LONGPRESS4
#endif
    , BTN_LONGCLICK4
#endif
};

void BUTTONS_Init(uint8_t adc_channel);
void BUTTONS_Done(void);
void BUTTONS_Clear(void);
bool BUTTONS_Read(uint8_t *state);
