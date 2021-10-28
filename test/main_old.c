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

#include "At13WS2812B.h"
#include <util/delay.h>

#define F_CPU 9600000
#define BUZZER _BV(PINB0)
#define SERVO _BV(PINB1)
#define BOTON _BV(PINB2)
#define LED _BV(PINB2)
#define RELE _BV(PINB4)
#define AGUA PINB3
#define LM35 PINB5

// posiciones del servo para cada visualizacion:
enum
{
  POS_APAGADO,   //                 // POS_SELECCIONE, // setup "seleccione temperatura"
  POS_TE_BLANCO, // setup: 60°
  POS_TE_VERDE,  // setup: 70°
  POS_MATE,      // setup: 80°
  POS_TE_NEGRO,  // setup: 95°
  POS_HERVIR,    // setup: 100°
  // termometro:
  POS_FRIO,     // < 50°
  POS_50,       // = 50°
  POS_60,       // = 60°
  POS_70,       // = 70°
  POS_80,       // = 80°
  POS_90,       // = 90°
  POS_100,      // >= 100°
  POS_PROHIBIDA //
};

static uint16_t servo_positions[] = {
    600, // primera posicion - 0°
    763,
    926,
    1089,
    1252,
    1415,
    1578,
    1741,
    1904,
    2067,
    2230,
    2393,
    2556,
    2730 // ultima posicion - 180°
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

// estados:
enum
{
  E_START, // luego de un reset o cada vez que quede en reposo (sin agua)
  //E_INIT_SETUP, // muestra que va a seleccionar la temperatura (paso intermedio)
  E_SETUP, // esperando seleccion de la temperatura
  E_ON,    // se enciende la pava (con agua)
  E_OFF    // se apaga la pava (con agua)
};

//
#define LED_ROJO ws2812b_set_color(LED, 255, 0, 0);
#define LED_AZUL ws2812b_set_color(LED, 0, 0, 255);
#define LED_VERDE ws2812b_set_color(LED, 0, 255, 0);
#define LED_AMARILLO ws2812b_set_color(LED, 255, 255, 0);
#define LED_MAGENTA ws2812b_set_color(LED, 255, 0, 255);
#define LED_BLANCO ws2812b_set_color(LED, 99, 99, 99);
#define LED_OFF ws2812b_set_color(LED, 0, 0, 0);
//
#define HAY_AGUA (leer_adc_agua() == -1)
#define BOTON_PULSADO (leer_adc_agua() == 1)
#define SIN_AGUA_O_BOTON_PULSADO (leer_adc_agua() >= 0)
//
#define RELE_ON PORTB |= RELE;
#define RELE_OFF PORTB &= ~RELE;
#define BUZZER_ON PORTB |= BUZZER;
#define BUZZER_OFF PORTB &= ~BUZZER;
//

// temperaturas a seleccionar:
uint8_t temperatureSelected = POS_MATE;
uint16_t temperatureActual = 0; // es un indice
uint8_t estado = E_START;
uint32_t timerMillisecs = 0;
volatile uint16_t servo_position;

void beep()
{
  PORTB |= BUZZER;  //     ___
  _delay_ms(100);   //  __| T |__
  PORTB &= ~BUZZER; //
}

/*
void shift(uint8_t *c, uint8_t *d)
{
  *c += *d;
  if (*c > 100)
    *d = -1;
  if (*c < 2)
    *d = 1;
}

void heart_beat()
{
  static uint8_t r = 0, dr = 1;
  static uint8_t g = 33, dg = 1;
  static uint8_t b = 66, db = 1;
  ws2812b_set_color(LED, r, g, b);
  shift(&r, &dr);
  shift(&g, &dg);
  shift(&b, &db);
  _delay_ms(50);
}*/

void heart_beat()
{
  static uint32_t r = 0, g = 85, b = 170;
  ws2812b_set_color(LED, r++, g++, b++);
  _delay_ms(50);
}

// mueve el servo a una posicion determinada.
void mover_servo(uint8_t position)
{
  servo_position = servo_positions[position];
  for (uint8_t i = 0; i < 30; i++)
  {
    PORTB |= SERVO;
    delayMicroseconds(servo_position);
    PORTB &= ~SERVO;
    _delay_ms(20);
  }
}

// determina la temperatura con un indice para la posicion del servo.
void leer_temperatura()
{
  // (no seteo ADMUX; queda seleccionado el PB5/ADC0 y right adjust)
  ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set prescaler to clock/128 & enable ADC
  ADCSRA |= (1 << ADSC);                               // Start the conversion
  while (ADCSRA & (1 << ADSC))                         // Wait for it to finish
    ;
  // busco una ventana donde cuadre la temperatura : solo 5 temperaturas posibles.
  if (ADC < TEMP_50)
  {
    // temperatura < 50°C
    temperatureActual = POS_FRIO;
  }
  else if (ADC < TEMP_60)
  {
    // temperatura 50°C
    temperatureActual = POS_50;
  }
  else if (ADC < TEMP_70)
  {
    // temperatura 60°C
    temperatureActual = POS_60;
  }
  else if (ADC < TEMP_80)
  {
    // temperatura 70°C
    temperatureActual = POS_70;
  }
  else if (ADC < TEMP_90)
  {
    // temperatura 80°C
    temperatureActual = POS_80;
  }
  else if (ADC < TEMP_100)
  {
    // temperatura 95°C
    temperatureActual = POS_90;
  }
  else
  {
    // temperatura máxima (~100°C)
    temperatureActual = POS_100;
  }
}

// detecta nada/agua o tierra (es para leer el sensor de agua y un pulsador a masa en la misma entrada)
// nada=0 / agua=-1 / boton=1
int8_t leer_adc_agua()
{
  ADMUX |= (1 << ADLAR);                               // left adjust result
  ADMUX |= (1 << MUX0);                                // Set the ADC input
  ADMUX |= (1 << MUX1);                                //
  ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // Set the prescaler to clock/128 & enable ADC
  ADCSRA |= (1 << ADSC);                               // Start the conversion
  while (ADCSRA & (1 << ADSC))                         // Wait for it to finish
    ;
  // (estos valores funcionan bien con 55K)
  if (ADCH < 20)
    return 1; // boton (Gnd)
  else if (ADCH < 200)
    return -1; // agua (aprox 1,5V)
  else
    return 0; // nada (Vcc)
}

void setup()
{
  // seteo E/S: 0/0/Lm35/Rele/Agua/Led/Servo/Buzzer
  DDRB = 0b00010111;
  beep();
  mover_servo(POS_APAGADO);
}

void loop()
{
  if (estado == E_START)
  {
    RELE_OFF;
    //heart_beat();
    if (BOTON_PULSADO)
    {
      // primera vez que se pulsa el boton...
      estado = E_SETUP;
      LED_AMARILLO;
      beep();
      //// mover_servo(POS_SELECCIONE);
      mover_servo(temperatureSelected);
      //  delay(666);
      //timer_start();
      // timerMillisecs = millis();
    }
  }
  /*else if (estado == E_INIT_SETUP)
  {
    uint32_t t = millis() - timerMillisecs;
    if (t < 500)
    {
      LED_AMARILLO;
      // pasaron 30 segundos y no se toco el boton...chau!
      //   estado = E_START;
      //   mover_servo(POS_APAGADO);
    }
    else
    {
      LED_OFF;
      if (t > 1000)
        timerMillisecs = millis();
    }

    //  else
    if (BOTON_PULSADO)
    {
      LED_MAGENTA;
      estado = E_SETUP;
      beep();
      mover_servo(temperatureSelected);
      // delay(666);
    }
  }*/
  else if (estado == E_SETUP)
  {
    if (BOTON_PULSADO)
    {
      // siguiente posicion (es circular)
      if (++temperatureSelected > POS_HERVIR)
        temperatureSelected = POS_TE_BLANCO;
      beep();
      mover_servo(temperatureSelected);
      // delay(666);
    }
    else if (HAY_AGUA)
    {
      estado = E_ON;
      beep();
      RELE_ON;
      //LED_MAGENTA;
      // delay(200);
      // beep_corto();
    }
  }
  else // estado E_ON y E_OFF
  {
    // como temperatureActual va de 6 a 12, mapeo a un color... 6*20=120 ; 12*20=240
    ws2812b_set_color(LED, 20, estado == E_ON ? 255 : 0, temperatureActual >> 4);
    mover_servo(temperatureActual);

    if (estado == E_ON && temperatureActual == temperatureSelected) // ya calentó ?
    {
      beep();
      estado = E_OFF;
      RELE_OFF;
    }
    else if (estado == E_OFF && temperatureActual != temperatureSelected) // ya se enfrió ?
    {
      beep();
      estado = E_ON;
      RELE_ON;
    }
    else if (SIN_AGUA_O_BOTON_PULSADO)
    {
      estado = E_START;
      RELE_OFF;
      beep();
      mover_servo(POS_APAGADO);
    }
    //_delay_ms(500);
  }
  leer_temperatura();
}
