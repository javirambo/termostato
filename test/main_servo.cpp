#include <avr/io.h>
#include <util/delay.h>

#define F_CPU 9600000
#define SERVO _BV(PINB1)

enum
{
    POS_APAGADO,
    POS_SELECCIONE,
    POS_FRIO,
    POS_TE_BLANCO,
    POS_TE_VERDE,
    POS_MATE,
    POS_TE_NEGRO,
    POS_HERVIR,
    POS_PROHIBIDA
};

// mueve el servo a una posicion determinada.
volatile uint16_t servo_position;
void mover_servo(uint8_t position)
{
    static uint16_t servoPositionArray[] = {600, 763, 800, 926, 1089, 1252, 1415, 1578};
    DDRB |= SERVO;
    servo_position = servoPositionArray[position];
    for (uint8_t i = 0; i < 30; i++)
    {
        PORTB |= SERVO;
        delayMicroseconds(servo_position);
        PORTB &= ~SERVO;
        delay(20);
    }
}

void setup() {}

void loop()
{
    mover_servo(POS_APAGADO);
    _delay_ms(1000);
    mover_servo(POS_FRIO);
    _delay_ms(1000);
    mover_servo(POS_MATE);
    _delay_ms(1000);
    mover_servo(POS_HERVIR);
    _delay_ms(1000);
}
