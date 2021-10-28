
#include "At13WS2812B.h"
#include <util/delay.h>

#define F_CPU 9600000
#define LED _BV(PINB2)

uint8_t r = 0, g = 33, b = 66;

void shift(uint8_t *c, uint8_t *d)
{
    *c += *d;
    if (*c > 100)
        *d = -1;
    if (*c < 2)
        *d = 1;
}

void heart_beat_cool()
{
    static uint8_t r = 0, dr = 1;
    static uint8_t g = 33, dg = 1;
    static uint8_t b = 66, db = 1;
    ws2812b_set_color(LED, r, g, b);
    shift(&r, &dr);
    shift(&g, &dg);
    shift(&b, &db);
    _delay_ms(50);
}

void setup() {}

void loop()
{
    // ws2812b_set_color(LED, r++, g++, b++);
    heart_beat_cool();
    _delay_ms(50);
}
