/****************************************************/
/* The USI as Slave                                 */
/* Author: Axel Gartner                             */
/****************************************************/

/* 
Source from
https://www.mikrocontroller.net/topic/38917     

modified to use USI_Buffer as "register"
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "i2c.h"
#include <util/delay.h>

#define __attiny2313__

#define USI_DATA   			USIDR
#define USI_STATUS  		USISR
#define USI_CONTROL 		USICR
uint8_t USI_ADDRESS;

#define NONE				0
#define ACK_PR_RX			1
#define BYTE_RX				2

#define ACK_PR_TX			3
#define PR_ACK_TX			4
#define BYTE_TX				5

// Device dependant defines
#if defined(__at90tiny26__) | defined(__attiny26__)
    #define DDR_USI             DDRB
    #define PORT_USI            PORTB
    #define PIN_USI             PINB
    #define PORT_USI_SDA        PORTB0
    #define PORT_USI_SCL        PORTB2
#endif

#if defined(__at90Tiny2313__) | defined(__attiny2313__) 
    #define DDR_USI             DDRB
    #define PORT_USI            PORTB
    #define PIN_USI             PINB
    #define PORT_USI_SDA        PORTB5
    #define PORT_USI_SCL        PORTB7
#endif

volatile uint8_t COMM_STATUS = NONE;

volatile uint8_t USI_Buffer[USI_BUFFER_SIZE] ={0};
volatile uint8_t USI_BufferPointer = 0;

void USI_init(uint8_t id) {
	USI_ADDRESS = id;

	// 2-wire mode; Hold SCL on start and overflow; ext. clock
	USI_CONTROL |= (1<<USIWM1) | (1<<USICS1);
	USI_STATUS = 0xf0;  // write 1 to clear flags, clear counter
	DDR_USI  &= ~(1<<PORT_USI_SDA);
	PORT_USI &= ~(1<<PORT_USI_SDA);
	DDR_USI  |=  (1<<PORT_USI_SCL);
	PORT_USI |=  (1<<PORT_USI_SCL);
	// startcondition interrupt enable
	USI_CONTROL |= (1<<USISIE);
}


SIGNAL(SIG_USI_START) {
	uint8_t tmpUSI_STATUS;
	tmpUSI_STATUS = USI_STATUS;
	COMM_STATUS = NONE;
	// Wait for SCL to go low to ensure the "Start Condition" has completed.
	// otherwise the counter will count the transition
	while ( (PIN_USI & (1<<PORT_USI_SCL)) );
	USI_STATUS = 0xf0; // write 1 to clear flags; clear counter
	// enable USI interrupt on overflow; SCL goes low on overflow
	USI_CONTROL |= (1<<USIOIE) | (1<<USIWM0);
}

SIGNAL(SIG_USI_OVERFLOW) {
  uint8_t BUF_USI_DATA = USI_DATA;
  static uint8_t firstbyte = 0;
	switch(COMM_STATUS) {
	case NONE:																					// Check Address
		if (((BUF_USI_DATA & 0xfe) >> 1) != USI_ADDRESS) {	// if not receiving my address
			// disable USI interrupt on overflow; disable SCL low on overflow
			USI_CONTROL &= ~((1<<USIOIE) | (1<<USIWM0));
		}
		else { // else address is mine
			DDR_USI  |=  (1<<PORT_USI_SDA);
			USI_STATUS = 0x0e;	// reload counter for ACK, (SCL) high and back low
			if (BUF_USI_DATA & 0x01) COMM_STATUS = ACK_PR_TX; else COMM_STATUS = ACK_PR_RX;
			firstbyte = 0;	//reset the firstbyte flag
		}
		break;				
	case ACK_PR_RX:																				// Receiving
		DDR_USI  &= ~(1<<PORT_USI_SDA);
		_delay_us(5);
		COMM_STATUS = BYTE_RX;
		break;
	case BYTE_RX:
		/* Save received byte here! ... = USI_DATA*/
		if(!firstbyte)
		{
			USI_BufferPointer = USI_DATA;									// first rx byte is the register address
			firstbyte = 1;
		}
		else
		{
			USI_Buffer[USI_BufferPointer++] = USI_DATA;						// save the received data in the buffer an increment the buffer pointer	
		}
		/* end Save received byte here! ... = USI_DATA*/
		DDR_USI  |=  (1<<PORT_USI_SDA);
		USI_STATUS = 0x0e;	// reload counter for ACK, (SCL) high and back low
		COMM_STATUS = ACK_PR_RX;
		break;
	case ACK_PR_TX:																				// Transmitting
		/* Put first byte to transmit in buffer here! USI_DATA = ... */
		USI_DATA = USI_Buffer[USI_BufferPointer++];
		/* end Put first byte to transmit in buffer here! USI_DATA = ... */
		PORT_USI |=  (1<<PORT_USI_SDA); // transparent for shifting data out
		
		COMM_STATUS = BYTE_TX;
		break;
	case PR_ACK_TX:
		if(BUF_USI_DATA & 0x01) {
			COMM_STATUS = NONE; // no ACK from master --> no more bytes to send
		}
		else {
			/* Put next byte to transmit in buffer here! USI_DATA = ... */
			USI_DATA = USI_Buffer[USI_BufferPointer++];
			/* end Put first byte to transmit in buffer here! USI_DATA = ... */
			PORT_USI |=  (1<<PORT_USI_SDA); // transparent for shifting data out
			DDR_USI  |=  (1<<PORT_USI_SDA);
			COMM_STATUS = BYTE_TX;
		}
		break;
	case BYTE_TX:							// byte transmitted...
		DDR_USI  &= ~(1<<PORT_USI_SDA);
		PORT_USI &= ~(1<<PORT_USI_SDA);
		_delay_us(5);
		USI_STATUS = 0x0e;	// reload counter for ACK, (SCL) high and back low
		COMM_STATUS = PR_ACK_TX;
		break;
	}
	USI_STATUS |= (1<<USIOIF); // clear overflowinterruptflag, this also releases SCL
	
	if(USI_BufferPointer == USI_BUFFER_SIZE) USI_BufferPointer = 0;	// prevent buffer overflow and wrap around.
}
