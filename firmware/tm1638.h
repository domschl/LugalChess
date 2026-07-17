#ifndef TM1638_H
#define TM1638_H

#include "pico/stdlib.h"

// Default GPIO Pin Mapping for QYF-TM1638 Module
#define TM_STB_PIN  16
#define TM_CLK_PIN  17
#define TM_DIO_PIN  18

// Initialize GPIO pins and TM1638 chip
void tm1638_init(void);

// Write a command/data byte
void tm1638_write_byte(uint8_t byte);

// Read a byte from DIO (during key scanning)
uint8_t tm1638_read_byte(void);

// Display an 8-character string on the 7-segment displays
void tm1638_display_string(const char *str);

// Set states of the 8 individual LEDs (bitmask)
void tm1638_set_leds(uint8_t mask);

// Scan the keyboard and return pressed key index (0 to 15), or -1 if none
int tm1638_get_key(void);

#endif // TM1638_H
