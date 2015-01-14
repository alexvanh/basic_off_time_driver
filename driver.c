/*
 * "Off Time Basic Driver" for ATtiny controlled flashlights
 *  Copyright (C) 2014 Alex van Heuvelen (alexvanh)
 *
 * Basic firmware demonstrating a method for using off-time to
 * switch modes on attiny13 drivers such as the nanjg drivers (a feature
 * not supported by the original firmware).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
 
 
/* This firmware uses off-time memory for mode switching without
 * hardware modifications on nanjg drivers. This is accomplished by
 * storing a flag in an area of memory that does not get initialized.
 * There is enough energy stored in the decoupling capacitor to
 * power the SRAM and keep data during power off for about 500ms.
 *
 * When the firmware starts a byte flag is checked. If the flashlight
 * was off for less than ~500ms all bits will still be 0. If the
 * flashlight was off longer than that some of the bits in SRAM will
 * have decayed to 1. After the flag is checked it is reset to 0.
 * Being off for less than ~500ms means the user half-pressed the
 * switch (using it as a momentary button) and intended to switch modes.
 *
 * This can be used to store any value in memory. However it is not
 * guaranteed that all of the bits will decay to 1. Checking that no
 * bits in the flag have decayed acts as a checksum and seems to be
 * enough to be reasonably certain other SRAM data is still valid.
 *
 * In order for this to work brown out detection must be enabled by
 * setting the correct fuse bits. I'm not sure why this is, maybe
 * reduced current consumption due to the reset being held once the
 * capacitor voltage drops below the threshold?
 */

#define F_CPU 4800000
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h> 

// disable mode memory.
#define NO_MODE_MEMORY


// PWM configuration
#define PWM_PIN PB1
#define PWM_LVL OCR0B
#define PWM_TCR 0x21
#define PWM_SCL 0x01

// brightness steps too large at lower end
#define SINUSOID 4, 4, 5, 6, 8, 10, 13, 16, 20, 24, 28, 33, 39, 44, 50, 57, 63, 70, 77, 85, 92, 100, 108, 116, 124, 131, 139, 147, 155, 163, 171, 178, 185, 192, 199, 206, 212, 218, 223, 228, 233, 237, 241, 244, 247, 250, 252, 253, 254, 255, 255, 254, 253, 252, 250, 247, 244, 241, 237, 233, 228, 223, 218, 212, 206, 199, 192, 185, 178, 171, 163, 155, 147, 139, 131, 124, 116, 108, 100, 92, 85, 77, 70, 63, 57, 50, 44, 39, 33, 28, 24, 20, 16, 13, 10, 8, 6, 5, 4, 4

// natural log of a sinusoid, spends too long at lowest levels
#define LN_SINUSOID 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 8, 8, 9, 10, 11, 12, 14, 16, 18, 21, 24, 27, 32, 37, 43, 50, 58, 67, 77, 88, 101, 114, 128, 143, 158, 174, 189, 203, 216, 228, 239, 246, 252, 255, 255, 252, 246, 239, 228, 216, 203, 189, 174, 158, 143, 128, 114, 101, 88, 77, 67, 58, 50, 43, 37, 32, 27, 24, 21, 18, 16, 14, 12, 11, 10, 9, 8, 8, 7, 7, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5
// store in program memory. It would use too much SRAM.
uint8_t const ramp_LUT[] PROGMEM = { LN_SINUSOID };

// store in uninitialized memory so it will not be overwritten and
// can still be read at startup after short (<500ms) power off
volatile uint8_t noinit_decay __attribute__ ((section (".noinit")));
volatile uint8_t noinit_lvl __attribute__ ((section (".noinit")));

uint8_t EEMEM MODE_P;

/* Ramping brightness selection
 * cycle through PWM values from ramp_LUT (look up table). current PWM 
 * value is saved in noinit_lvl so it is available at next startup 
 * (after a short press)
*/
void ramp()
{
	uint8_t i = 0;
	uint8_t j = 0;
	while (1){
		for (i = 0; i < sizeof(ramp_LUT); i++){
			PWM_LVL = pgm_read_byte(&(ramp_LUT[i]));
			noinit_lvl = PWM_LVL; // remember after short power off
			_delay_ms(60); //gives a period of x seconds
		}
		j++;
		
		//_delay_ms(1000);
	}
}



int main(void)
{
	// PWM setup 
    // set PWM pin to output
    DDRB |= _BV(PWM_PIN);
    // PORTB = 0x00; // initialised to 0 anyway

    // Initialise PWM on output pin and set level to zero
    TCCR0A = PWM_TCR;
    TCCR0B = PWM_SCL;

    PWM_LVL = 0;

	
	uint8_t mode =  eeprom_read_byte(&MODE_P);
	
	#ifdef 	NO_MODE_MEMORY
	if (noinit_decay) // not short press, forget mode
	{
		mode = 0;
	}
	#endif
	
	if (!noinit_decay) // no decay, it was a short press
	{
		++mode;
	}

    // set noinit data for next boot
    noinit_decay = 0;

    // mode needs to loop back around
    // (or the mode is invalid)
    if (mode > 5) // there are 6 modes
    {
        mode = 0;
    }
    
    eeprom_busy_wait(); //make sure eeprom is ready
	eeprom_write_byte(&MODE_P, mode); // save mode

    switch(mode){
        case 0:
        PWM_LVL = 0xFF;
        break;
        case 1:
        PWM_LVL = 0x40;
        break;
        case 2:
        PWM_LVL = 0x10;
        break;
        case 3:
        PWM_LVL = 0x04;
        break;
        case 4:
        ramp(); // ramping brightness selection
        break;
        case 5:
        PWM_LVL = noinit_lvl; // use value selected by ramping function
        break;
    }
    
    while(1);
    return 0;
}
