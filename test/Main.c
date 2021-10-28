/*
  Termostato para TE/MATE usando servo y otras cosas estrambóticas.

  Usa un micro Attiny13.
  
  PB5: sensor LM35
  PB3: sensor de agua y BOTON
  PB4: salida al rele para encender la pava eléctrica
  PB0: Buzzer o parlante
  PB1: SERVO
  PB2: LEDS RGB (tipo WS2812B)


  Funcionamiento:
  ==============

  - se pulsa el boton -> si está apagado -> setea temperatura (led rojo)
  - se pulsa el boton -> si esta en set -> cambia de 50 -> 60 -> 70 -> 90 -> 100 -> (y vuelve al principio)
  - 30 segundos sin boton -> apagado
  - detecta agua -> led AZUL, mostrar temperatura
  - llega a la temperatura deseada -> led ROJO/BLANCO TITILA y BUZZER -> si no se saca el sensor del agua sigue 
    controlando la temperatura del agua.
  - cuando se saca el sensor del agua -> apagado

  Cuadro de Temperaturas:
  ======================
  60 - te blanco
  70 - te verde
  80 - mate / te azul / te oolong
  95 - te negro / pu-erh
  100- hervir


  ISET57 2021
  Javier
*/

#include <Arduino.h>
#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>

#define F_CPU 9600000
#define BUZZER _BV(PINB0)
#define SERVO _BV(PINB1)
#define LED _BV(PINB2)
#define LM35 PINB3
#define RELE _BV(PINB4)
#define AGUA PINB5

// posiciones del servo para cada visualizacion:
enum
{
  POS_APAGADO,   // posiciones:
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

// son valores directos del conversor ADC
enum
{
  TEMP_FRIO = 400,
  TEMP_50 = 500,
  TEMP_60 = 600,
  TEMP_70 = 700,
  TEMP_80 = 800,
  TEMP_90 = 900,
  TEMP_100 = 1000
};
// histeresis
#define OFFSET_TEMPERATURA 50

// estados:
enum
{
  E_START,     // luego de un reset o cada vez que quede en reposo (sin agua)
  E_SETUP,     // esperando seleccion de la temperatura
  E_TERMOSTATO // pava ON/OFF
};

//
#define LED_COLOR(_r, _g, _b) \
  {                           \
    r = _r;                   \
    g = _g;                   \
    b = _b;                   \
    ws2812b_change_color();   \
  }
//
#define LED_ROJO LED_COLOR(255, 0, 0)
#define LED_AZUL LED_COLOR(0, 0, 255)
#define LED_VERDE LED_COLOR(0, 255, 0)
#define LED_AMARILLO LED_COLOR(255, 255, 0)
#define LED_MAGENTA LED_COLOR(255, 0, 255)
#define LED_BLANCO LED_COLOR(99, 99, 99)
#define LED_OFF LED_COLOR()
//
#define HAY_AGUA (leer_adc_agua() == -1)
#define BOTON_PULSADO (leer_adc_agua() == 1)
//
#define RELE_ON PORTB |= RELE;
#define RELE_OFF PORTB &= ~RELE;
#define BUZZER_ON    \
  {                  \
    DDRB |= BUZZER;  \
    PORTB |= BUZZER; \
  }
#define BUZZER_OFF    \
  {                   \
    PORTB &= ~BUZZER; \
    DDRB &= ~BUZZER;  \
  }

// primera posicion - 0°, ultima posicion - 180°
uint16_t servo_positions[] = {600, 800, 1089, 1252, 1415, 1578, 1741, 1904, 2067, 2230, 2393, 2556, 2730};
//
uint16_t temperatureSelected; // valor de posicion de servo
uint16_t temperatureActual;   //   "
uint16_t temperaturaBuscada;  // valor de ADC
uint8_t termostato;           // flag que indica si la pava tiene que encenderse o apagarse
uint8_t termostatoOld;        //
uint8_t estado;               // estado del sistema
uint16_t servo_position;      // registro para posicionar el servo
uint8_t posicionado = -1;
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

/*void beep()
{
  PORTB |= BUZZER;  //     ___
  _delay_ms(100);   //  __| T |__
  PORTB &= ~BUZZER; //
}*/

/*void heart_beat()
{
  // r = g = 0;
  b++;
  if (b > 200)
    b = 0;
  ws2812b_change_color();
  _delay_ms(123);
}*/

/*#define mover_servo(pos)                   \
  {                                        \
    servo_position = servo_positions[pos]; \
    mover_servo_f();                       \
  }*/

// mueve el servo a una posicion determinada.
void mover_servo()
{
  for (uint8_t i = 0; i < 30; i++)
  {
    PORTB |= SERVO;
    delayMicroseconds(servo_position);
    PORTB &= ~SERVO;
    _delay_ms(18);
  }
}

// determina la temperatura con un indice para la posicion del servo.
void leer_temperatura()
{
  ADMUX = (1 << MUX0) | (1 << MUX1);                                 // left adjust result, Set the ADC input to PB3
  ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set the prescaler to clock/128 & enable ADC
  ADCSRA |= (1 << ADSC);                                             // Start the conversion
  while (ADCSRA & (1 << ADSC))                                       // Wait for it to finish
    ;                                                                //...
  uint16_t t = ADC;                                                  // read adc 10 bits

  // busco una ventana donde cuadre la temperatura:
  if (t < TEMP_50)
    temperatureActual = POS_FRIO;
  else if (t < TEMP_60)
    temperatureActual = POS_50;
  else if (t < TEMP_70)
    temperatureActual = POS_60;
  else if (t < TEMP_80)
    temperatureActual = POS_70;
  else if (t < TEMP_90)
    temperatureActual = POS_80;
  else if (t < TEMP_100)
    temperatureActual = POS_90;
  else
    temperatureActual = POS_100;

  // el control del termostato funciona siempre:
  if (t < temperaturaBuscada - OFFSET_TEMPERATURA)
  {
    termostato = 0;
  }
  else if (t > temperaturaBuscada)
  {
    termostato = 1;
  }
}

// detecta nada/agua/boton (es para leer el sensor de agua y un pulsador a Vcc en la misma entrada)
// nada=0 / agua=-1 / boton=1
int8_t leer_adc_agua()
{
  ADMUX = (1 << ADLAR);                                               // left adjust result, Set the ADC input to PB5
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set the prescaler to clock/128 & enable ADC
  ADCSRA |= (1 << ADSC);                                              // Start the conversion
  while (ADCSRA & (1 << ADSC))                                        // Wait for it to finish
    ;                                                                 // ...
  uint8_t t = ADCH;                                                   // read adc 8 bits

  if (t < 150)
    return 0; // nada (Vcc/2)
  else if (t > 240)
    return 1; // boton (Vcc)
  else
    return -1; // agua (aprox 3/4 Vcc)
}

int main()
{
  estado = E_START;
  temperatureSelected = POS_MATE;
  temperatureActual = POS_FRIO;
  temperaturaBuscada = TEMP_80;
  //termostato = 0;

  // seteo E/S: [0-0-Rele-Lm35-Agua-Led-Servo-Buzzer]
  //            [E-E-  S - E  - E  - S -  S  -  S   ]
  DDRB = 0b00010111;
  BUZZER_ON;

  while (1)
  {
    leer_temperatura();

    //--------------------------------------------------
    //-- ESTADO INICIAL: SOLO SE VE EL LED DE HEART BEAT.
    //--------------------------------------------------
    if (estado == E_START)
    {
      servo_position = servo_positions[POS_APAGADO];
      mover_servo();
      RELE_OFF;
      LED_AZUL;
      //heart_beat();
      if (BOTON_PULSADO)
      {
        // primera vez que se pulsa el boton...
        estado = E_SETUP;
        LED_COLOR(255, 255, 0);
        BUZZER_ON;
      }
    }
    //--------------------------------------------------
    //-- mueve el servo a la temperatura seleccionada,
    //-- si detecta agua, se enciende la pava.
    //--------------------------------------------------
    else if (estado == E_SETUP)
    {
      servo_position = servo_positions[temperatureSelected];
      mover_servo();
      if (BOTON_PULSADO)
      {
        // siguiente posicion (es circular)
        if (++temperatureSelected > POS_HERVIR)
          temperatureSelected = POS_TE_BLANCO;
        BUZZER_ON;
      }
      else if (HAY_AGUA)
      {
        estado = E_TERMOSTATO;
        termostatoOld = termostato;
        BUZZER_ON;
        RELE_ON;
      }
    }
    //--------------------------------------------------
    //--------------------------------------------------
    else // estado=E_TERMOSTATO
    {
      if (termostato)
      {
        RELE_ON;
        LED_COLOR(20, 50, 200);
        if (termostato != termostatoOld)
        {
          termostatoOld = termostato;
          BUZZER_ON;
        }
      }
      else
      {
        RELE_OFF;
        LED_COLOR(20, 200, 50);
        if (termostato != termostatoOld)
        {
          termostatoOld = termostato;
          BUZZER_ON;
        }
      }

      // como temperatureActual va de 6 a 12, mapeo a un color... 6*20=120 ; 12*20=240
      //ws2812b_change_color(LED, 20, 50 , temperatureActual >> 4);

      servo_position = servo_positions[temperatureSelected];
      mover_servo();

      if (!HAY_AGUA)
      {
        estado = E_START;
        BUZZER_ON;
      }
    }
    //--------------------------------------------------
    // apago el beep luego de 100 ms (siempre) por las dudas que haya sonado.
    _delay_ms(100);
    BUZZER_OFF;
  }
}