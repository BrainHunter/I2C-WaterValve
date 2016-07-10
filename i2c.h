#ifndef _I2C_H__
#define __I2C_H__

#include <stdint.h>

// variables
#define USI_BUFFER_SIZE	7
extern volatile uint8_t USI_Buffer[USI_BUFFER_SIZE];

// functions
extern void USI_init(uint8_t id);

#endif