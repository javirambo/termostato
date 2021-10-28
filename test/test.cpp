//#include <Arduino.h>
#include <avr/io.h>       //					//		Attiny13a, Fuses =0xFF7A, CLK=9.6 MHz, Flash=476b, SRAM=42b, EEPROM=0b	//
#include <avr/pgmspace.h> //					//											 �����: Dolphin, 25.02.2015 17:00	//
#include <util/delay.h>   //					////////////////////////////////////////////////////////////////////////////////////////////////

register volatile uint8_t LED_Byte asm("r4"), LED_Mask_Off asm("r6"), LED_Mask_On asm("r8"), //
    HUE_SectOffs asm("r7"), HUE_Sect asm("r9"), HUE_Byte asm("r11");                         //
register volatile int8_t HUE_Delta asm("r5"), Lum asm("r10");                                //

uint8_t LED_RGB[] = {0xf0, 0x80, 0x30};

void inline LED_Out(uint8_t *LED_Data) //	     ������� �������� ������ WS2811			//
{                                      //	� ���������� - ��������� �� ��������� GRB	//
    for (uint8_t Byte_Pos = 0; Byte_Pos < 3; Byte_Pos++)
    {                                           //////////////////    ( 48 ���� ���� ) (30 ���)     /////////////////////
        LED_Byte = *LED_Data++;                 //														//
        asm volatile("	ldi   R16,8		\n\t"   //				             Connection notes:					//
                     "NxtBit:			\n\t"   //														//
                     "	out   0x18,R8	\n\t"   //  PB <- 1  		//				(PB0) 									//
                     "	nop				\n\t"   //			------------------------- ( ws2811(0.0))  .. (ws2811(0.13))	//
                     "	nop				\n\t"   //		      |           _____________							//
                     "	sbrs  R4,7		\n\t"   //		      -----------|		       |								//
                     "	out   0x18,R6	\n\t"   //  PB <- 0		//		                 _______| Attiny 13A  |								//
                     "	lsl   R4		\n\t"   //		   |	      |			    |								//
                     "	nop				\n\t"   //		  |          |			   |								//
                     "	nop				\n\t"   //		 |          |			  |								//
                     "	out   0x18,R6	\n\t"   //  PB <- 0		//	       |          ----------------------								//
                     "	dec   R16		\n\t"   //	      |													//
                     "	brne  NxtBit	\n\t"); //	     |	  (PB1) 											//
    }                                           //	    -----------------	( ws2811(1.0))  .. (ws2811(1.13)) 				//
}

int main()
{
    DDRB = LED_Mask_On = 0x10;
    LED_Mask_Off = 0;
    asm("cli\n\t");
    while (1)
    {
        LED_Out(LED_RGB);
        _delay_ms(30);
    }
}
