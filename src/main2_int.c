/*
    Hago un PWM con interrupciones de timer.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "At13Adc.h"

#define F_CPU 9600000
#define BUZZER _BV(PINB0)
#define SERVO _BV(PINB1)
#define LED _BV(PINB2)
#define LM35 PINB3
#define RELE _BV(PINB4)
#define AGUA PINB5

#define LED_AZUL LED_COLOR(0, 0, 255)
#define LED_AMARILLO LED_COLOR(255, 255, 0)
#define LED_ROJO LED_COLOR(255, 0, 0)
#define LED_MAGENTA LED_COLOR(255, 0, 255)

uint16_t temperatura_buscada;
uint8_t posicion_seleccionada;
uint8_t r = 0, g = 0, b = 0; // para el color del led
// primera posicion - 0°, ultima posicion - 180°
uint16_t servo_microseconds[] = {60, 77, 95, 113, 131, 149, 166, 184, 202, 220, 238, 255, 273};
volatile uint16_t Tick;   // 100KHz pulse
volatile uint16_t sPulse; // Servo pulse variable
volatile uint8_t UnSeg;
volatile uint8_t DecSeg;

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

#define LED_COLOR(_r, _g, _b)   \
    {                           \
        r = _r;                 \
        g = _g;                 \
        b = _b;                 \
        ws2812b_change_color(); \
    }

// posiciones del servo para cada visualizacion:
enum
{
    // 13 posiciones:
    POS_APAGADO,   // setup de temperatura:
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
    TEMP_FRIO = 92, // menos de 50°C
    TEMP_50 = 102,  //
    TEMP_60 = 123,  //
    TEMP_70 = 143,  //
    TEMP_80 = 164,  //
    TEMP_90 = 184,  //
    TEMP_100 = 196  // mas de 96°C
};

uint16_t temperaturas_seleccionadas[] = {TEMP_60, TEMP_70, TEMP_80, TEMP_90, TEMP_100};

#define RELE_ON PORTB |= RELE;
#define RELE_OFF PORTB &= ~RELE;
#define BUZZER_ON PORTB |= BUZZER;
#define BUZZER_OFF PORTB &= ~BUZZER;

#define mover_servo(index) sPulse = servo_microseconds[index];

///// *** Esta INT de timer mueve el servo motor ***
///// Interrupcion de timer con frecuencia de 100Khz
///// Se incrementa Tick cada 0.01mS (60 ticks son .6ms, 273 ticks son 2.73ms)
ISR(TIM0_COMPA_vect)
{
    if (Tick >= 2000)
    { // One servo frame (20ms) completed
        Tick = 0;
        if (DecSeg++ >= 50)
        {
            UnSeg++; // un segundo completo
        }
    }
    Tick++;
    if (Tick <= sPulse) // Generate servo pulse
    {
        PORTB |= SERVO;
    }
    else
    {
        PORTB &= ~SERVO;
    }
}

void pwm_init()
{
    sei();                   //  Enable global interrupts
    TCCR0A |= (1 << WGM01);  // Configure timer 1 for CTC mode
    TIMSK0 |= (1 << OCIE0A); // Enable CTC interrupt
    OCR0A = 95;              // Set CTC compare value
    TCCR0B |= (1 << CS00);   // No prescaler
    Tick = 0;
    sPulse = 100;
}

// lee la entrada del sensor de agua y del pulsador (que es el mismo)
int8_t leer_adc_agua()
{
    adc_setup_10(PINB5);
    uint16_t t = adc_read_10();
    if (t < 650)
        return 0; // nada (Vcc/2)
    else if (t > 900)
        return 1; // boton (Vcc)
    else
        return -1; // agua (aprox 3/4 Vcc)
}

void beep()
{
    BUZZER_ON;
    delay(11);
    BUZZER_OFF;
    delay(11);
}

// determina la temperatura y posiciona el servo.
// solo actua con la maxima temperatura detectada.
// no sale de la rutina hasta llegar a la temperatura buscada o si se saca el sensor del agua.
void controlar_temperatura()
{
    uint16_t temperatura_actual, temperatura_max = 0;
    LED_ROJO;
    RELE_ON;
    beep();

    // mientras leo temperatura y muevo el motor, verifico si llego a la temp buscada
    // (si se saca el sensor del agua tambien termina)
    while (temperatura_max < temperatura_buscada && leer_adc_agua() != 0)
    {
        // leo temperatura:
        adc_setup_10(PINB3);
        temperatura_actual = adc_read_10();

        // guardo siempre la max.
        if (temperatura_max < temperatura_actual)
            temperatura_max = temperatura_actual;

        // busco una ventana donde cuadre la temperatura:
        int8_t index;
        if (temperatura_max <= TEMP_FRIO)
            index = POS_FRIO;
        else if (temperatura_max <= TEMP_50)
            index = POS_50;
        else if (temperatura_max <= TEMP_60)
            index = POS_60;
        else if (temperatura_max <= TEMP_70)
            index = POS_70;
        else if (temperatura_max <= TEMP_80)
            index = POS_80;
        else if (temperatura_max <= TEMP_90)
            index = POS_90;
        else
            index = POS_100;

        // muestro la temp con el servo:
        mover_servo(index);
    }

    // ** FIN **
    LED_AMARILLO;
    RELE_OFF;
    beep();
    beep();
    LED_AZUL;
}

int main()
{
    // seteo E/S:
    // lm35|rele|agua|led|servo|buzzer
    //  0    1    0    1    1     1
    DDRB = 0b010111;

    LED_AZUL;
    pwm_init();
    beep();

    temperatura_buscada = TEMP_80; // temperatura por defecto=MATE
    posicion_seleccionada = 2;     // 2=MATE

    while (1)
    {
        // SENSOR EN EL AGUA! START!!
        if (leer_adc_agua() == -1)
        {
            controlar_temperatura();
            // listo, ya calentó....espero sacar el sensor del agua....
            while (leer_adc_agua() == -1)
                ;
            mover_servo(POS_APAGADO);
        }

        // boton pulsado? selecciono proxima temperatura:
        else if (leer_adc_agua() == 1)
        {
            LED_MAGENTA;
            if (++posicion_seleccionada > POS_HERVIR)
                posicion_seleccionada = POS_TE_BLANCO;
            mover_servo(posicion_seleccionada);
            temperatura_buscada = temperaturas_seleccionadas[posicion_seleccionada - 1];
            beep();
            _delay_loop_2(65000);
        }

        // idle
        else
        {
            _delay_loop_2(10000);
            if (UnSeg == 5)
            {
                 beep();
            }
        }
    }
}