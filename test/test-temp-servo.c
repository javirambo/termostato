
#include <Arduino.h>
#include <avr/io.h>
#include <inttypes.h>
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
#define BUZZER_ON PORTB |= BUZZER;
#define BUZZER_OFF PORTB &= ~BUZZER;
//
// primera posicion - 0°, ultima posicion - 180°
uint16_t servo_positions[] = {600, 800, 1089, 1252, 1415, 1578, 1741, 1904, 2067, 2230, 2393, 2556, 2730};
//
uint16_t temperatureSelected; // valor de posicion de servo
uint16_t temperatureActual;   //   "
uint16_t temperaturaBuscada;  // valor de ADC
uint8_t termostato;           // flag que indica si la pava tiene que encenderse o apagarse
//uint8_t termostatoOld;        //
uint8_t estado;              // estado del sistema
uint16_t servo_position;     // registro para posicionar el servo
uint8_t r = 0, g = 0, b = 0; // para el color del led

#define mover_servo(pos)                       \
    {                                          \
        servo_position = servo_positions[pos]; \
        mover_servo_f();                       \
    }

// mueve el servo a una posicion determinada.
void mover_servo_f()
{
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

    uint16_t t = ADC;
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

int main()
{
    temperatureSelected = POS_MATE;
    temperatureActual = POS_FRIO;
    temperaturaBuscada = TEMP_80;
    termostato = 0;
    estado = E_START;

    // seteo E/S: 0/0/Lm35/Rele/Agua/Led/Servo/Buzzer
    DDRB = 0b00010111;

    mover_servo(POS_APAGADO);

    while (1)
    {
        if (termostato)
        {
            RELE_ON;
            //LED_COLOR(20, 50, 200);
        }
        else
        {
            RELE_OFF;
            //LED_COLOR(20, 200, 50);
        }
        mover_servo(temperatureActual);
        leer_temperatura();
    }
}