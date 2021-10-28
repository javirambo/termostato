#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define F_CPU 9600000
#define BUZZER _BV(PINB0)
#define SERVO _BV(PINB1)
#define LED _BV(PINB2)
#define LM35 PINB3
#define RELE _BV(PINB4)
#define AGUA PINB5

#define LED_COLOR(_r, _g, _b)   \
    {                           \
        r = _r;                 \
        g = _g;                 \
        b = _b;                 \
        ws2812b_change_color(); \
    }

#define LED_AZUL LED_COLOR(0, 0, 255)
#define LED_ROJO LED_COLOR(255, 0, 0)

uint8_t r = 0, g = 0, b = 0; // para el color del led

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

// posiciones del servo para cada visualizacion:
enum
{
    POS_APAGADO,   // 13 posiciones:
    POS_TE_BLANCO, // setup: 60°
    POS_TE_VERDE,  // setup: 70°
    POS_MATE,      // setup: 80°
    POS_TE_NEGRO,  // setup: 95°
    POS_HERVIR,    // setup: 100°
    // termometro:
    POS_FRIO, // < 50°
    POS_50,   // = 50°
    POS_60,   // = 60°
    POS_70,   // = 70°
    POS_80,   // = 80°
    POS_90,   // = 90°
    POS_100   // >= 100°
};

// son valores directos del conversor ADC (ajustar con el LM35)
enum
{
    TEMP_FRIO = 30, // menos de 50°C
    TEMP_50 = 60,   // entre 50 y 55
    TEMP_60 = 100,  // entre 60 y 65
    TEMP_70 = 130,  // entre 70 y 75
    TEMP_80 = 170,  // entre 80 y 85
    TEMP_90 = 200,  // entre 90 y 95
    TEMP_100 = 240  // mas de 96
};

#define RELE_ON PORTB |= RELE;
#define RELE_OFF PORTB &= ~RELE;
#define BUZZER_ON PORTB |= BUZZER;
#define BUZZER_OFF PORTB &= ~BUZZER;

// primera posicion - 0°, ultima posicion - 180°
uint16_t servo_microseconds[] = {590, 768, 946, 1124, 1302, 1480, 1658, 1836, 2014, 2192, 2370, 2548, 2730UL};
uint16_t servo_position; // registro para posicionar el servo

void mover_servo(uint8_t index)
{
    static uint8_t index_ant = 255;
    if (index_ant == index)
        return;
    index_ant = index;

    servo_position = servo_microseconds[index];
    for (uint8_t i = 0; i < 30; i++)
    {
        PORTB |= SERVO;
        delayMicroseconds(servo_position);
        PORTB &= ~SERVO;
        delayMicroseconds(21000 - servo_position);
    }
}

int8_t leer_adc_agua()
{
    //ADMUX = (1 << ADLAR) | (1 << MUX0) | (1 << MUX1);                   // left adjust result, Set the ADC input to PB3
    ADMUX = (1 << ADLAR);                                              // left adjust result, Set the ADC input to PB5
    ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set the prescaler to clock/128 & enable ADC
    ADCSRA |= (1 << ADSC);                                             // Start the conversion
    while (ADCSRA & (1 << ADSC))                                       // Wait for it to finish
        ;                                                              // ...
    uint8_t t = ADCH;                                                  // read adc 8 bits

    if (t < 160)
        return 0; // nada (Vcc/2)
    else if (t > 240)
        return 1; // boton (Vcc)
    else
        return -1; // agua (aprox 3/4 Vcc)
}

// determina la temperatura y posiciona el servo.
void mostrar_temperatura()
{
    //static uint16_t servo_position_ant = 0;
    //ADMUX = (1 << MUX1);                                               // right adjust result, Set the ADC input to PB4
    ADMUX = (1 << ADLAR) | (1 << MUX0) | (1 << MUX1);                  // left adjust result, Set the ADC input to PB3
    ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set prescaler to clock/128 & enable ADC
    ADCSRA |= (1 << ADSC);                                             // Start the conversion
    while (ADCSRA & (1 << ADSC))                                       // Wait for it to finish
        ;                                                              // ...
    uint8_t t = ADCH;                                                  // read adc 10 bits

    // busco una ventana donde cuadre la temperatura:
    if (t < TEMP_50)
        t = POS_FRIO;
    else if (t < TEMP_60)
        t = POS_50;
    else if (t < TEMP_70)
        t = POS_60;
    else if (t < TEMP_80)
        t = POS_70;
    else if (t < TEMP_90)
        t = POS_80;
    else if (t < TEMP_100)
        t = POS_90;
    else
        t = POS_100;

    mover_servo(t);

    //b = t;
    //ws2812b_change_color();

    // el control del termostato funciona siempre:
    /* if (t < temperaturaBuscada - OFFSET_TEMPERATURA)
    {
        termostato = 0;
    }
    else if (t > temperaturaBuscada)
    {
        termostato = 1;
    }*/
}

int main()
{
    // seteo E/S:
    // lm35|agua|rele|led|servo|buzzer
    //  0    0    1    1    1     1
    DDRB = 0b010111;

    // DIDR0 = 0xff; // desabilita las ent digitales para los conv ADC

    BUZZER_ON;
    RELE_ON;
    LED_AZUL;
    mover_servo(12);
    delay(500);

    BUZZER_OFF;
    RELE_OFF;
    LED_ROJO;
    mover_servo(0);
    delay(500);

    int i = 12, a;
    while (1)
    {
        mover_servo(i);
        a = leer_adc_agua();
        if (a == 1)
            i--;
        //  else if (a == -1)
        //    i = 0;

        if (i < 0)
            i = 12;
    }

    /*
uint8_t a_ant = 2;

while (1)
{
    g = r = b = 0;

    uint8_t a = leer_adc_agua();
    if (a != a_ant)
    {
        a_ant = a;
        if (a == 0)
        {
            b = 255; //nada
            RELE_OFF;
        }
        else if (a == 1)
        {
            g = 255; //boton
        }
        else
        {
            r = 155; //agua
            RELE_ON;
        }
        ws2812b_change_color();
        delay(50);
    }
    else
        mostrar_temperatura();
}*/
}