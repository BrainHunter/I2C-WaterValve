/*
Filename:       	main.c
Project:        	I2C Water Valve
Copyright:      	Nicolas Kaufmann  mailto: Mr.Spock.NK@gmx.de
Author:         	Nicolas Kaufmann
Creation:       	19.03.2016 
Version:        	19.03.2016 
Description:    	Firmware für die I2C Vetilsteuerung

Dieses Programm ist freie Software. Sie können es unter den Bedingungen der 
GNU General Public License, wie von der Free Software Foundation veröffentlicht, 
weitergeben und/oder modifizieren, entweder gemäß Version 2 der Lizenz oder 
(nach Ihrer Option) jeder späteren Version. 

Die Veröffentlichung dieses Programms erfolgt in der Hoffnung, 
daß es Ihnen von Nutzen sein wird, aber OHNE IRGENDEINE GARANTIE, 
sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT 
FÜR EINEN BESTIMMTEN ZWECK. Details finden Sie in der GNU General Public License. 

Sie sollten eine Kopie der GNU General Public License zusammen mit diesem 
Programm  erhalten haben. Der Text der GNU General Public License ist auch 
im Internet unter http://www.gnu.org/licenses/gpl.txt veröffentlicht.
Eine inoffizielle deutsche Übersetzung findet sich unter 
http://www.gnu.de/gpl-ger.html. Diese Übersetzung soll nur zu einem besseren 
Verständnis der GPL verhelfen; rechtsverbindlich ist alleine die englischsprachige
Version.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdint.h>
//#include "usiTwiSlave.h"
#include "i2c.h"

//---------  Pinnig: ---------//
///// Usi (I2C) Pins: 
// SDA = PB5
// SCL = PB7
///// Valves:
// V1 = PB6
// V2 = PB0
// V3 = PB1
// V4 = PB2
// V5 = PB3
// V6 = PB4
///// Buttons:
// B1 = PD0
// B2 = PD1
// B3 = PD2
// B4 = PD3
// B5 = PD4
// B6 = PD5


//----- Register Layout ------//
// Register | RW?	| Usage			| Comment
// 0		| R		| Valve Status	| Bit 0 = Valve 1 Status ... Bit 5 = Valve 6 Status
// 1		| RW	| Valve 1 Time	| Watering time = Value * 10s 	--> Max 2540 Sec = 42,3mins	// 255 will not count down.
// 2		| RW	| Valve 2 Time
// 3		| RW	| Valve 3 Time
// 4		| RW	| Valve 4 Time
// 5		| RW	| Valve 5 Time
// 6		| RW	| Valve 6 Time

//------ Configuration -------//
#define I2C_ADDRESS 0x20	//0x40

#define NUMOPENVALVES	2

#define BUTTONSHORTTIME 100		// [ms] time for the short button trigger
#define BUTTONLONGTIME  2000	// 2sec
#define BUTTONADDTIME	12		// 120 sec = 2mins

// no changes below this line should be needed (but you ever know...)

// Functions
void init(void);
uint32_t timediff(uint32_t t1,uint32_t t2);
void switchValve(uint8_t valve, uint8_t status );
uint8_t countBitsSet(unsigned int v);

// globals
volatile uint32_t globalSystime = 0;
#define NUMVALVES	6

int main(void){
	
	// Pin Configuration for Valves:
	DDRB |= 0x1F; 	// PB0 - PB4
	PORTB &=~ 0x1F;
	DDRD |= 0x40;	// PD6
	PORTD &=~ 0x40;
	
	// Pin Configurations for Switches
	DDRD &=~ 0x3F;	// PD0 - PD5
	PORTD |= 0x3F;	// Pullups on
	
	// Pin Configurations for Debug LED
	//DDRB |= 0x40; 	// PB6
	//PORTB &=~ 0x40;
	//PORTB |= 0x40;
	
	// Init the Systimer & I2C...
	init();
	
	#define rxbuffer USI_Buffer
 	
	static uint8_t valveStatus = 0;
	uint8_t* bufferValveStatus = (uint8_t*)&rxbuffer[0];
	uint8_t* valveTime = (uint8_t*)&rxbuffer[1];
	static uint32_t testtime = 0; 
	static uint32_t systime = 0;
	static uint32_t i2ctime = 0;
	
	// Buttons:
	static uint8_t buttonPressed[6] = {0};
	static uint32_t buttonTime[6] = {0};
	
	
	//main loop
	for(;;){
	
		// read the systime atomic. we don't want the interrupt to corrupt the data.
		ATOMIC_BLOCK(ATOMIC_FORCEON)
		{
			systime = globalSystime;
		}	
		
		// Handle Buttons:
		for(int i = 0; i < 6; i++)	// 6 Buttons.
		{
			uint8_t buttonState = PIND & ( 1<<i );
			if(!buttonState) // button pressed - active low
			{
				
				if(!buttonPressed[i])
				{
					buttonTime[i] = systime;
					buttonPressed[i]++;
				}
				else
				{
					if(timediff(systime, buttonTime[i]) > BUTTONSHORTTIME && buttonPressed[i] == 1)
					{
						buttonPressed[i]++;
					}
					else if(timediff(systime, buttonTime[i]) > BUTTONLONGTIME && buttonPressed[i] == 2)
					{
						buttonPressed[i]++;
						// shut off valve
						valveTime[i] = 0;
						//SwitchOff Valve:
						switchValve(i, 0);
						// mark in valveStatus
						valveStatus &=~ (1 << i); 
					}
				}
			}
			else // button released
			{
				if(buttonPressed[i] == 2) // short press
				{
					// add another xx sec to the  valve
					valveTime[i] += BUTTONADDTIME;
					// are we allowed to switch this valve on? --> Then switch it on now. 
					if(countBitsSet(valveStatus) < NUMOPENVALVES)
					{
						//SwitchOn Valve:
						switchValve(i, 1);
						// mark in valveStatus
						valveStatus |= (1 << i); 
					}
				}
				buttonPressed[i] = 0;
			}
		}
		
		// Handle Valves
		if(timediff(testtime, systime) > 10000)
		{
			testtime = systime;
			// PORTB ^= 0xF0;			// toggle high nibble
			
			for(uint8_t i=0; i < NUMVALVES; i++)
			{
				if(valveTime[i] > 0)	
				{	
					// are we allowed to switch this valve on?
					if(countBitsSet(valveStatus) < NUMOPENVALVES)
					{
						//SwitchOn Valve:
						switchValve(i, 1);
						// mark in valveStatus
						valveStatus |= (1 << i); 
					}
					// is the valve switched on?
					if(valveStatus & (1 << i))
					{
						if(valveTime[i] < 255)	// don't decrement if the time is 255
						{
							// Decrement the Valve Time 
							valveTime[i]--;	
						}
					}
				}
				else
				{	
					//SwitchOff Valve:
					switchValve(i, 0);
					// mark in valveStatus
					valveStatus &=~ (1 << i); 
				}
			}// END for(uint8_t i=0; i < NUMVALVES; i++)
		}// END if(timediff(testtime, systime) > 10000)
		
		// i2c watchdog: 
		// SDA = PB5
		// SCL = PB7	
		// reset I2C if SDA is Low for a long time
		// or if i2c comm status changes.
		extern volatile uint8_t COMM_STATUS;
		uint8_t old_comm_status = 0; // = NONE
		#define NONE				0
		
		if(PIND & ( 1<<PB5))	// SDA pin is high --> bus is NOT locked
		{						// --> 
			i2ctime = systime;	// reset counter! --> no I2C reset!
		}
		if(COMM_STATUS != NONE) // only if comm
		{
			if(COMM_STATUS != old_comm_status) // comm status changed -->
			{
				i2ctime = systime;	// reset counter! 
				old_comm_status = COMM_STATUS;	//mark the old COMM_Status
			}
		}
		else
		{	// reset old_comm_status when COMM_Status is NONE
			old_comm_status = COMM_STATUS;
		}
		// Reset I2C if error persits for 100msec
		if(timediff(i2ctime, systime) > 100) // 100 msec timeout! 
		{
			COMM_STATUS = NONE; 	// reset to none
			USI_init(I2C_ADDRESS);	
			i2ctime = systime;		// don't do this again in the next loop
		}
				
		//set the valve status in the usi buffer - the buffer can be overwritten by the bus master, but we want this to be "read only" since we don't care about this register. 
		*bufferValveStatus = valveStatus;
		
	}// END main Loop
	
	return 0;
}

void init(){
	// Timer0 init: - 1ms systime
	TCCR0B |= (1 << CS00) | (1 << CS01);				//Prescaler 64 - see datasheet page 86
	TCCR0A |= (1 << WGM01);								// CTC mode
	OCR0A = (uint8_t)(((F_CPU / 64.0) * 1e-3) - 0.5);  	// 1ms  
	TIMSK |= (1 << OCIE0A);								//Output compare interrupt enable

	// init i2c slave:
	//usiTwiSlaveInit(I2C_ADDRESS);
	USI_init(I2C_ADDRESS);
	
	sei();												// enable Interrupts
}


// Timer0 Compare Interrupt	
ISR( TIMER0_COMPA_vect ){
	globalSystime++;
}

// get time difference
uint32_t timediff(uint32_t t1, uint32_t t2)
{
    int32_t d = (int32_t)t1 - (int32_t)t2;
	if(d < 0) d = -d;
    return (uint32_t) d;
}

void switchValve(uint8_t valve, uint8_t status )
{
// V1 = PD6
// V2 = PB0
// V3 = PB1
// V4 = PB2
// V5 = PB3
// V6 = PB4
	if(valve == 0)
	{
		if(status)
		{
			PORTD |= (1 << PD6);
		}
		else
		{
			PORTD &=~(1 << PD6);
		}
	}
	else
	{
		if(status)
		{
			PORTB |= (1 << (valve-1));
		}
		else
		{
			PORTB &=~(1 << (valve-1));
		}
	}
}

//Counting bits set, Brian Kernighan's way //https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan
uint8_t countBitsSet(unsigned int v){
	// count the number of bits set in v
	uint8_t c; // c accumulates the total bits set in v
	for (c = 0; v; c++)
	{
	  v &= v - 1; // clear the least significant bit set
	}
return c;
}
