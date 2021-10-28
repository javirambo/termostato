#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>

#define LED _BV(PINB2)

uint8_t r = 0, g = 0, b = 0, sector = 0, onda = 0, dirOnda = 1;

/**
    AtTiny13 Datasheet:
        https://ww1.microchip.com/downloads/en/devicedoc/doc2535.pdf

    WS2812B Datasheet:
        https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf

    https://sudonull.com/post/100926-Using-color-spaces-in-ATTiny13a-for-WS2811
 */
void ws2812b_change_color()
{
    const uint8_t portb = PORTB; // PORTB is volatile, so preload value
    const uint8_t lo = portb & ~LED;
    const uint8_t hi = portb | LED;
    __asm__ volatile(
        // Each 12 cycles go high on cycle 0 and go low on cycle:
        //   - 8 if a one bit is transmitted
        //   - 4 if a zero bit is transmitted
        //
        // In the spare time we have left, we read out the next bit from the current color
        // byte. We keep a mask bit in r17, do a bitwise and operation, and branch if (not)
        // zero. After 8 shifts the carry flag will be set and we will move on to the next
        // color bit.
        //
        // I have figured that it should be possible to decode the bits from the RGB values
        // on the fly. However, I think that we do not have enough cycles to read from the
        // different color in a non-unrolled loop, but to be completely honest I did not
        // take a lot of effort to find a good argument on why this should be impossible.
        //
        ".green:                        \n\t"
        "mov r16,%[green]               \n\t"
        "ldi r17,0x80                   \n\t"
        "and r16,r17                    \n\t"
        "brne .green_transmit_one_0     \n\t"
        "rjmp .green_transmit_zero_0    \n\t"

        // Green
        ".green_transmit_one_1:         \n\t"
        "nop                            \n\t" // cycle -1
        ".green_transmit_one_0:         \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "nop                            \n\t" // cycle 1
        "lsr r17                        \n\t" // cycle 2
        "brcs .green_transmit_one_done  \n\t" // cycle 3
        "rjmp .+0                       \n\t" // cycle 4
        "mov r16,%[green]               \n\t" // cycle 6
        "and r16,r17                    \n\t" // cycle 7
        "out 0x18,%[lo]                 \n\t" // cycle 8
        "brne .green_transmit_one_1     \n\t" // cycle 9
        "rjmp .green_transmit_zero_0    \n\t" // cycle 10

        ".green_transmit_one_done:      \n\t"
        "ldi r17,0x80                   \n\t" // cycle 5
        "mov r16,%[red]                 \n\t" // cycle 6
        "and r16,r17                    \n\t" // cycle 7
        "out 0x18,%[lo]                 \n\t" // cycle 8
        "brne .red_transmit_one_1       \n\t" // cycle 9
        "rjmp .red_transmit_zero_0      \n\t" // cycle 10

        ".green_transmit_zero_0:        \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "lsr r17                        \n\t" // cycle 1
        "brcs .green_transmit_zero_done \n\t" // cycle 2
        "nop                            \n\t" // cycle 3
        "out 0x18,%[lo]                 \n\t" // cycle 4
        "rjmp .+0                       \n\t" // cycle 5
        "mov r16,%[green]               \n\t" // cycle 7
        "and r16,r17                    \n\t" // cycle 8
        "brne .green_transmit_one_1     \n\t" // cycle 9
        "rjmp .green_transmit_zero_0    \n\t" // cycle 10

        ".green_transmit_zero_done:     \n\t"
        "out 0x18,%[lo]                 \n\t" // cycle 4
        "nop                            \n\t" // cycle 5
        "ldi r17,0x80                   \n\t" // cycle 6
        "mov r16,%[red]                 \n\t" // cycle 7
        "and r16,r17                    \n\t" // cycle 8
        "brne .red_transmit_one_1       \n\t" // cycle 9
        "rjmp .red_transmit_zero_0      \n\t" // cycle 10

        // Red
        ".red_transmit_one_1:           \n\t"
        "nop                            \n\t" // cycle -1
        ".red_transmit_one_0:           \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "nop                            \n\t" // cycle 1
        "lsr r17                        \n\t" // cycle 2
        "brcs .red_transmit_one_done    \n\t" // cycle 3
        "rjmp .+0                       \n\t" // cycle 4
        "mov r16,%[red]                 \n\t" // cycle 6
        "and r16,r17                    \n\t" // cycle 7
        "out 0x18,%[lo]                 \n\t" // cycle 8
        "brne .red_transmit_one_1       \n\t" // cycle 9
        "rjmp .red_transmit_zero_0      \n\t" // cycle 10

        ".red_transmit_one_done:        \n\t"
        "ldi r17,0x80                   \n\t" // cycle 5
        "mov r16,%[blue]                \n\t" // cycle 6
        "and r16,r17                    \n\t" // cycle 7
        "out 0x18,%[lo]                 \n\t" // cycle 8
        "brne .blue_transmit_one_1      \n\t" // cycle 9
        "rjmp .blue_transmit_zero_0     \n\t" // cycle 10

        ".red_transmit_zero_0:          \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "lsr r17                        \n\t" // cycle 1
        "brcs .red_transmit_zero_done   \n\t" // cycle 2
        "nop                            \n\t" // cycle 3
        "out 0x18,%[lo]                 \n\t" // cycle 4
        "rjmp .+0                       \n\t" // cycle 5
        "mov r16,%[red]                 \n\t" // cycle 7
        "and r16,r17                    \n\t" // cycle 8
        "brne .red_transmit_one_1       \n\t" // cycle 9
        "rjmp .red_transmit_zero_0      \n\t" // cycle 10

        ".red_transmit_zero_done:       \n\t"
        "out 0x18,%[lo]                 \n\t" // cycle 4
        "nop                            \n\t" // cycle 5
        "ldi r17,0x80                   \n\t" // cycle 6
        "mov r16,%[blue]                \n\t" // cycle 7
        "and r16,r17                    \n\t" // cycle 8
        "brne .blue_transmit_one_1      \n\t" // cycle 9
        "rjmp .blue_transmit_zero_0     \n\t" // cycle 10

        // Blue
        ".blue_transmit_one_1:          \n\t"
        "nop                            \n\t" // cycle -1
        ".blue_transmit_one_0:          \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "rjmp .+0                       \n\t" // cycle 1
        "lsr r17                        \n\t" // cycle 3
        "brcs .blue_transmit_one_done   \n\t" // cycle 4
        "nop                            \n\t" // cycle 5
        "mov r16,%[blue]                \n\t" // cycle 6
        "and r16,r17                    \n\t" // cycle 7
        "out 0x18,%[lo]                 \n\t" // cycle 8
        "brne .blue_transmit_one_1      \n\t" // cycle 9
        "rjmp .blue_transmit_zero_0     \n\t" // cycle 10

        ".blue_transmit_one_done:       \n\t"
        "rjmp .blue_transmit_zero_done  \n\t" // cycle 6

        ".blue_transmit_zero_0:         \n\t"
        "out 0x18,%[hi]                 \n\t" // cycle 0
        "lsr r17                        \n\t" // cycle 1
        "brcs .blue_transmit_zero_done  \n\t" // cycle 2
        "nop                            \n\t" // cycle 3
        "out 0x18,%[lo]                 \n\t" // cycle 4
        "rjmp .+0                       \n\t" // cycle 5
        "mov r16,%[blue]                \n\t" // cycle 7
        "and r16,r17                    \n\t" // cycle 8
        "brne .blue_transmit_one_1      \n\t" // cycle 9
        "rjmp .blue_transmit_zero_0     \n\t" // cycle 10

        ".blue_transmit_zero_done:      \n\t"
        "out 0x18,%[lo]                 \n\t" // cycle 4 / cycle 8
        ".end:                          \n\t"

        : // No outputs
        : [lo] "r"(lo), [hi] "r"(hi), [green] "r"(g), [red] "r"(r), [blue] "r"(b)
        : "r16", "r17");

    PORTB &= ~LED; // RESET
}

void shift(uint8_t *c, uint8_t *d)
{
    *c += *d;
    if (*c == 255)
    {
        *d = -1;
    }
    if (*c == 0)
    {
        *d = 1;
        sector++;
        if (sector > 5)
            sector = 0;
    }
}

//RAM:   [=         ]  10.9% (used 7 bytes from 64 bytes)
//Flash: [==        ]  16.2% (used 166 bytes from 1024 bytes)
void heart_beat2()
{
    /*
RAM:   [=         ]  10.9% (used 7 bytes from 64 bytes)
Flash: [====      ]  38.3% (used 392 bytes from 1024 bytes)
RAM:   [=         ]  10.9% (used 7 bytes from 64 bytes)
Flash: [====      ]  37.3% (used 382 bytes from 1024 bytes)
RAM:   [=         ]  12.5% (used 8 bytes from 64 bytes)
Flash: [====      ]  43.8% (used 448 bytes from 1024 bytes)
RAM:   [=         ]  10.9% (used 7 bytes from 64 bytes)
Flash: [====      ]  41.4% (used 424 bytes from 1024 bytes)
    */
    ws2812b_change_color();

    onda += dirOnda;
    if (onda == 200)
        dirOnda = -1;
    else if (onda == 40)
    {
        dirOnda = 1;
        if (++sector == 6)
            sector = 0;
    }

    switch (sector)
    {
    case 0:
        r = onda;
        break;
    case 1:
        g = onda;
        break;
    case 2:
        b = onda;
        break;
    case 3:
        r = g = onda;
        break;
    case 4:
        g = b = onda;
        break;
    case 5:
        b = r = onda;
        break;
    }
    _delay_ms(5);
}

void setup()
{
    DDRB |= LED;
}

void loop()
{
    heart_beat2();
}