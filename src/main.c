#include <ch32v00x.h>
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"

extern void power(void);

int main(void) {
    TWI_Init(400000);

    delay_ms(50);

    if (! ssd1306_begin()) {
        while (1) {}
    }

    power();
}
