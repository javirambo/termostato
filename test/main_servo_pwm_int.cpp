/*
  ESTE FUNCIONA PERFECTO MOVIENDO UN SERVO POR PWM
  USA INTERRUPCIONES DE TIMER
  CADA 20ms TERMINA EL CICLO COMPLETO
  sPulse ES EL ANCHO DE PULSO (EN LA TABLA ESTÁ EL MIN(60) Y MAX(273) POSIBLES PARA LOS SERVOS AZULES)
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint16_t Tick;   // 100KHz pulse
volatile uint16_t sPulse; // Servo pulse variable

uint16_t m[] = {60, 77, 95, 113, 131, 149, 166, 184, 202, 220, 238, 255, 273};

///// Interrupcion de timer con frecuencia de 100Khz
///// Se incrementa Tick cada 0.01mS (60 ticks son .6ms, 273 ticks son 2.73ms)
ISR(TIM0_COMPA_vect)
{
  if (Tick >= 2000) // One servo frame (20ms) completed
    Tick = 0;
  Tick++;
  if (Tick <= sPulse)    // Generate servo pulse
    PORTB |= (1 << PB0); // Servo pulse high
  else
    PORTB &= ~(1 << PB0); // Servo pulse low
}

void pwm_init()
{
  sei();                           //  Enable global interrupts
  DDRB |= (1 << PB0) | (1 << PB1); // PB0 and PB1 as outputs
  TCCR0A |= (1 << WGM01);          // Configure timer 1 for CTC mode
  TIMSK0 |= (1 << OCIE0A);         // Enable CTC interrupt
  OCR0A = 95;                      // Set CTC compare value
  TCCR0B |= (1 << CS00);           // No prescaler
  Tick = 0;
}

int main(void)
{
  pwm_init();

  while (1)
  {
    for (int i = 0; i < 13; i++) // Servo teste cycling
    {
      sPulse = m[i]; // esta variable mueve el tcycle
      _delay_ms(500);
    }
  }
}
