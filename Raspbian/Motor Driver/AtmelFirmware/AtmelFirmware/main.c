/*
Firmware for "Raspberry Pi Servo board v3" Version 0.2  2013-02-10

Copyright 2012 Mikael Johansson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#define F_CPU 4000000UL        // CPU Freq for delay.h below

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <ctype.h>
#include <stdlib.h>
#include <avr/eeprom.h>

#define USART_BPS_9600    0
#define USART_BPS_19200   1
#define USART_BPS_38400   2
#define USART_BPS_57600   3  // Not usable
#define USART_BPS_115200  4  // Not usable
#define USART_BPS_230400  5  // Not usable
#define USART_BPS_500000  6	 // RPi to slow for this.

#define TRUE   1
#define FALSE  0

// Should be enough for longest possible cmd or parameter, including one space:
#define INPUT_BUFFER_SIZE  8

void setup_usart(unsigned short speed);
unsigned short promille_to_timer(signed short promille);
void Process_handler(void);
void RX_handler(void);
void TX_handler(void);
void MSG_handler(void);

enum {
	IDLE,
	REC_CMD,
	REC_DATA
} state;

// Used to buffer input from serial port one cmd/parameter at a time.
unsigned char buf[INPUT_BUFFER_SIZE];
unsigned char bufpos = 0;

// Used to save the cmd as ascii, while parsing parameters.
unsigned char cmd[INPUT_BUFFER_SIZE];
// Used to save the parameters converted to a number while parsing.
signed short  param[8];


// Used for sending both NACK and ACK.
const unsigned char const *acknack = (unsigned char *)"NACK\r\n";
#define ACK_STR  (acknack+1)
#define NACK_STR (acknack)

#define VERSION_STR "02\r\n"

union {
	unsigned int ucAll;
	unsigned int ucAny;
	struct {
		unsigned int exec_CMD        : 1;
		unsigned int send_ACK        : 1;
		unsigned int send_NACK       : 1;
		unsigned int send_VER        : 1;
		unsigned int run_test        : 1;
		unsigned int change_bps      : 1;
		unsigned int ignore_ws       : 1;
		unsigned int TX              : 1;
		unsigned int changeoutput    : 1;
		unsigned int outputenabled   : 1;

	};
} uFlags;


// This array contains the target servo position.
unsigned short servo_target[8] = {6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000};

//This array is read by Interrupt and contains current servo position.
volatile unsigned short servo[8] = {6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000};

//This array contains the how much the servo should move every ms.
unsigned char servo_step[8] = {0};

//The index of wanted uart speed, ACK is sent before speed is changed.
unsigned char bps_index = 0;


// Clear pulse to all servos
ISR (TIMER1_COMPA_vect)
{
	PORTD &= ~0xFC;  // Clear all but two lowest on port D..
	PORTC &= ~0x03;  // ..and lowest two on port C.
}



// Get pulse length for current servo and set up timer1 for it and set correct pin high.
ISR (TIMER2_COMP_vect)
{
	static volatile unsigned char current_servo = 0;  // Just used here, volatile not needed?
	volatile unsigned char tmpD;
	volatile unsigned char tmpC;

	if(uFlags.changeoutput == TRUE)
	{
		uFlags.changeoutput = FALSE;
		if(uFlags.outputenabled)
		PORTC &= ~0x08; //Enable 741.
		else
		PORTC |= 0x08;  //Disable 741.
	}
	
	// TCNT2 = 0x00; Not needed?  CTC should fix this.
	TIFR |= (1 << OCF1A);
	current_servo = (current_servo + 1) & 0x07;
	tmpD = ((1 << current_servo) & 0xFC);
	tmpC = ((1 << current_servo) & 0x03) | (PORTC & 0x38);  // Dont touch output pins and enable/disable 741
	OCR1A = servo[current_servo] - 17;  // -17 is for interrupt time, choosen oscilloscope measurment.
	
	TCNT1 = 0x0000;
	PORTD = tmpD;
	PORTC = tmpC;
}



int main(void)
{
	uFlags.ucAll = 0;    // Clear all flags

	// 0xFFFF can never be a real value, assume eeprom is not initialized.
	if(eeprom_read_word((void *)0) != 0xFFFF)
	{
		unsigned char i;
		for(i=0;i<8;i++)
		{
			servo[i] = eeprom_read_word((void *)(i<<1));
		}
	}
	

	//PORTS

	DDRC =  0x03;          // PortC Pin 0 and 1 is servo out.
	DDRC |= 0x08;          // PortC Pin 3 is 741 disable.
	DDRC |= 0x30;          // PortC Pin 4 and 5 output for pin 2 and 3 of extra connector.
	
	DDRB =  0x3C;          // PortB Pin 2..5 output for pin 4..7 of extra connector.
	PORTB = 0x00;          //
	
	DDRD = 0xFE;           // Only serial RX pin input on portD.
	//DDRD = 0xFF;
	//DDRD = 0x01;

	PORTC = 0x00;          // Enable servo out.
	
	//TIMERS

	// 8-bit timer - interval between the eight servo pulses.
	TCCR2 |= _BV(WGM21);  // CTC
	TCCR2 |= _BV(CS22) |_BV(CS21);   //  clk/256

	// 16-bit timer - length of pulse
	TCCR1B = _BV(CS10);  // clk/1.

	TIMSK |= (1 << OCIE1A) | (1 << OCIE2);   //Enable interrupt for CompareA-match for timer 1 and 2.

	OCR2  = 39;   // Makes pulse starting interrupt run ~8 times per 20ms  (Timer 0)

	// 8-bit timer - for main-loop timing.
	
	TCCR0 |=  _BV(CS02);  // start counting att 15625 Hz
	TCNT0 = 0x00;
	
	// UART

	UCSRA |= (1<<TXC);  // Clear TX complete.


	//상수 값이 잘못 설정되어 있음
	//USART_BSP_38400이 9600으로 동작하는 모드임
	setup_usart(USART_BPS_38400);
	
	sei();
	
	uFlags.exec_CMD = 0;

	while(1)
	{
		RX_handler();
		MSG_handler();
		TX_handler();
		Process_handler();
	}

}



void setup_usart(unsigned short speed)
{
	//Default to disable fast mode
	UCSRA &= ~_BV(U2X);
	//disable receiver and transmitter
	UCSRB &= ~(1<<RXEN) & ~(1<<TXEN);

	switch(speed)
	{
		case USART_BPS_9600:
		UBRRL = 25;
		break;
		case USART_BPS_19200:
		UBRRL = 12;
		break;
		case USART_BPS_38400:
		//Enable fast mode.
		UCSRA |= _BV(U2X);
		UBRRL = 12;
		break;
		/*  case USART_BPS_57600:
		UBRRL = ;
		break;  */
		/*  case USART_BPS_115200:
		UBRRL = ;
		break;  */
		/*  case USART_BPS_230400:
		UBRRL = ;
		break;  */
		case USART_BPS_500000:  //Untested! main-loop to slow?
		//Enable fast mode.
		UCSRA |= _BV(U2X);
		UBRRL = 0;
		break;
		default:
		uFlags.send_NACK = 1;
		break;
	}

	//enable receiver and transmitter
	UCSRB |= (1<<RXEN) |(1<<TXEN);
}



//With current crystal and timer settings 4000 timer steps is 1ms pulse, 8000 timer steps is 2ms pulse.
unsigned short promille_to_timer(signed short promille)
{
	if(promille<-2500)
	promille = -2500;
	else if(promille>1900)
	promille = 1900;

	// Less code than promille*2+6000 ?
	return (promille+promille+6000);
}



void Process_handler(void)
{
	unsigned char i;
	unsigned char tickflag = 0;

	if(TCNT0 > 78)  // Only correct in theory, needs adjustment.
	{
		TCNT0 = 0x00;
		tickflag = 1;
	}

	//Update servo movement every 200us.
	if(uFlags.run_test && tickflag)
	{
		unsigned char i;
		static unsigned char dir = 1;
		static unsigned short tmp=6000;

		for(i=0;i<8;i++)
		servo[i] = tmp;

		if(tmp >= 8000)
		dir = 0;
		if(tmp <= 4000)
		dir = 1;

		if(dir == 1)
		tmp = tmp+10;
		else
		tmp = tmp-10;

	}


	// Promille / ms
	if(tickflag)
	{
		tickflag = 0;
		for(i=0;i<8;i++)
		{
			if(servo[i] != servo_target[i])  // Not yet at target?
			{
				//No
				if(servo_step[i] != 0)   // Supposed to update?
				{
					//Yes
					if(abs((servo_target[i] - (signed long)servo[i])) <= (signed long)servo_step[i])  // One step is bigger than remaining distance to target?
					servo[i] = servo_target[i];  // Yes - just set servo to target, done!

					if(servo[i] != servo_target[i]) // Are we there yet?
					{
						//No - Move servo in correct direction
						if((servo_target[i] - (signed long)servo[i]) > 0)
						servo[i] += (servo_step[i]);
						else
						servo[i] -= (servo_step[i]);
					}
				}
			}

		}
	}

}



void RX_handler(void)
{
	static unsigned char n = 0;
	unsigned char rx;
	unsigned char i;

	// Have we received a new byte?
	if ( (UCSRA & (1<<RXC)) )
	{
		// Get byte from HW serial rx register..
		rx = UDR;
		
		// A command always start with "s" and can not contain an "s".
		if(rx == 's')
		{
			for(i=0;i<8;i++)
			{
				param[i] = 0;
			}
			n = 0;
			bufpos = 0;
			state = REC_CMD;
			uFlags.ignore_ws = 0;
		}

		if(rx == ' ' && uFlags.ignore_ws)
		{
			// Do nothing.
		}
		else if(state != IDLE)
		{
			uFlags.ignore_ws = 0;

			if(bufpos < INPUT_BUFFER_SIZE)
			{
				if((rx >= 'a' && rx <= 'z') || (rx >= '0' && rx <= '9') || rx == '-' || rx == '+')
				{
					buf[bufpos++] = rx;	
				}
				

				if(rx == ' ' || rx == '\r' || rx == '\n' || rx == '#')
				{
			
					buf[bufpos] = '\0';

					if(state == REC_CMD)
					{
						
						// save cmd for execution
						for(i=0;i<bufpos;i++)
						{
							cmd[i] = buf[i];
						}
						state = REC_DATA;
					}
					else // if(state == REC_DATA)
					{
						if(n<8)
						{
							// parse parameter and save for execution
							param[n++] = atoi((char *)buf);
						}
					}

					uFlags.ignore_ws = 1;
					bufpos = 0;
				}

				if(rx == '\r' || rx == '\n' || rx == '#')
				{
					// Run command.
					uFlags.exec_CMD = 1;
					state = IDLE;

				}

			}
			else
			{
				// Message part is to long
				bufpos = 0;
				state = IDLE;
				uFlags.send_NACK = 1;
			}
		}

	}
}



void TX_handler(void)
{
	static unsigned char const *cur;

	if(uFlags.send_VER)
	{
		uFlags.send_VER = 0;
		uFlags.TX = 1;

		cur = (unsigned char *)VERSION_STR;
	}

	if(uFlags.send_ACK)
	{
		uFlags.send_ACK = 0;
		uFlags.TX = 1;

		cur = ACK_STR;
	}

	if(uFlags.send_NACK)
	{
		uFlags.send_NACK = 0;
		uFlags.TX = 1;

		cur = NACK_STR;
	}

	if(uFlags.TX)
	{
		if(*cur != '\0') //Any char to send?
		{
			// yes!
			if( UCSRA & (1<<UDRE) ) // Anything being sent now?
			{
				// No! we can send!
				UCSRA |= (1<<TXC);  // CLEAR TXC before trying to send something. (clear by writing 1)
				UDR = *cur;          // Send!
				cur++;
			}
		}
		else
		{
			// no
			uFlags.TX = 0;
		}
	}
	else // We have nothing to send.
	{
		// Do we want to change bps?
		if(uFlags.change_bps)
		{
			if( UCSRA & (1<<TXC) ) // Transmission complete?
			{
				// Yes
				// We are now allowed to change BPS
				uFlags.change_bps = 0;
				setup_usart(bps_index);
				UCSRA |= (1<<TXC);  // Not needed? Probably not.
			}
		}
	}

}



void MSG_handler(void)
{
	if(uFlags.exec_CMD)
	{
		uFlags.exec_CMD = 0;
		uFlags.run_test = 0;        //Always stop servo test when receiving a valid or invalid cmd.


		if(cmd[1] == 't')            // "st" - Servo Test
		{
			uFlags.run_test = 1;
			uFlags.send_ACK = 1;
		}
			
		else if(cmd[1] == 'a' && cmd[2] == 'v')        // "sav" - Servo All Velocity, has 8 parameters, speed
		{
			unsigned char i;
			for(i=0;i<8;i++)
			servo_step[i] = (unsigned char)param[i];
			
			uFlags.send_ACK = 1;
	
		}
		else if(cmd[1] == 'a')                        // "sa" - Servo All, has 8 parameters, position
		{
			unsigned char i;
			
			//if(param[0] == 100 && param[1] == -100)
			//{
				//PORTD = 0xff;
			//}

			for(i=0;i<8;i++)
			{
				if(servo_step[i] == 0)
				servo_target[i] = servo[i] = promille_to_timer(param[i]);
				else
				servo_target[i] = promille_to_timer(param[i]);
			}

			uFlags.send_ACK = 1;
		}
		else if(((unsigned char)(cmd[1]-'0')) < 8)    // "s0", "s1" ... "s7" - Servo N, has 1 or 2 parameter, position and speed.
		{
			unsigned char tmp;
			unsigned char whatservo = (cmd[1]-'0');

			servo_target[whatservo] = promille_to_timer(param[0]);  // Position in parameter 0
			tmp = (unsigned char)param[1];                          // Speed in parameter 1 (0 == as fast as possible)

			if(tmp == 0)
			{
				servo[whatservo] = servo_target[whatservo];
			}
			else
			{
				//
				servo_step[whatservo] = tmp;
			}

			uFlags.send_ACK = 1;
		}
		else if(cmd[1] == 'i' && cmd[2] == 'a')      // "sia" - Servo Init All, has 8 parameters, positions.
		{
			unsigned char i;

			for(i=0;i<8;i++)
			{
				eeprom_write_word((void *)(i<<1), promille_to_timer(param[i]));
			}
			uFlags.send_ACK = 1;
		}
		else if(cmd[1] == 'b' && cmd[2] == 'r')      // "sbr" - Servo (set) Bit Rate, has one parameter 0-6.
		{
			bps_index = (unsigned char)param[0];
			uFlags.change_bps = 1;
			uFlags.send_ACK = 1;
		}
		else if(cmd[1] == 'n')    // "sn" - Servo versioN.
		{
			uFlags.send_VER = 1;
		}
		else if(cmd[1] == 'd')    // "sd" - Servo Disable
		{
			uFlags.outputenabled = FALSE;
			uFlags.changeoutput = TRUE;
			uFlags.send_ACK = TRUE;
			
			//PORTD = 0x00;
		}
		else if(cmd[1] == 'e')    // "se" - Servo Enable
		{
			uFlags.outputenabled = TRUE;
			uFlags.changeoutput = TRUE;
			uFlags.send_ACK = TRUE;
		}
		else if(cmd[1] == 'o')    // "so" - (Servo) Output
		{
			unsigned char i;
			unsigned char tmp;
			
			for(i=0;i<=1;i++)
			{
				tmp = param[i];
				if(tmp)
				{
					PORTC |= (1 << (i+4));
				}
				else
				{
					PORTC &= ~(1 << (i+4));
				}

			}
			for(i=2;i<=5;i++)
			{
				tmp = param[i];
				if(tmp)
				{
					PORTB |= (1 << i);  // Port pins happens to match i
				}
				else
				{
					PORTB &= ~(1 << i); // Port pins happens to match i
				}
				
			}
			
			uFlags.send_ACK = TRUE;
		}
		else
		{
			uFlags.send_NACK = 1;
		}
	}
}


