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

#define sqrt_50 4, 29, 39, 47, 54, 60, 65, 70, 75, 79, 83, 87, 91, 94, 98, 101, 104, 107, 110, 113, 116, 119, 122, 124, 127, 130, 132, 134, 137, 139, 141, 144, 146, 148, 150, 152, 155, 157, 159, 161, 163, 165, 167, 169, 170, 172, 174, 176, 178, 180, 181, 183, 185, 187, 188, 190, 192, 194, 195, 197, 198, 200, 202, 203, 205, 206, 208, 209, 211, 212, 214, 215, 217, 218, 220, 221, 223, 224, 226, 227, 229, 230, 231, 233, 234, 235, 237, 238, 239, 241, 242, 243, 245, 246, 247, 249, 250, 251, 252, 254, 255

// perceived intensity is basically linearly increasing, steps are 
// visible and slightly larger at the bottom
#define squared 4, 4, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 21, 24, 27, 30, 33, 37, 40, 44, 48, 53, 57, 62, 67, 72, 77, 83, 88, 94, 100, 107, 113, 120, 127, 134, 141, 149, 157, 165, 173, 181, 190, 198, 207, 216, 226, 235, 245, 255

#define sin_squared 4, 4, 4, 4, 4, 4, 4, 5, 5, 6, 6, 7, 9, 10, 13, 15, 18, 21, 25, 30, 35, 41, 47, 54, 61, 69, 77, 86, 95, 105, 115, 125, 135, 145, 156, 166, 176, 186, 195, 204, 213, 221, 228, 235, 240, 245, 249, 252, 254, 255, 255, 254, 252, 249, 245, 240, 235, 228, 221, 213, 204, 195, 186, 176, 166, 156, 145, 135, 125, 115, 105, 95, 86, 77, 69, 61, 54, 47, 41, 35, 30, 25, 21, 18, 15, 13, 10, 9, 7, 6, 6, 5, 5, 4, 4, 4, 4, 4, 4, 4

#define sin_squared_half_period 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 8, 9, 10, 10, 11, 13, 14, 15, 16, 18, 20, 21, 23, 25, 28, 30, 32, 35, 38, 41, 44, 47, 50, 54, 57, 61, 65, 69, 73, 77, 81, 86, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150, 156, 161, 166, 171, 176, 181, 186, 190, 195, 200, 204, 209, 213, 217, 221, 224, 228, 231, 234, 237, 240, 243, 245, 247, 249, 250, 252, 253, 254, 254, 255, 255

// delay in ms between each ramp step
#define RAMP_DELAY 30
// store in program memory. It would use too much SRAM.
uint8_t const ramp_LUT[] PROGMEM = { sin_squared_half_period };

// store in uninitialized memory so it will not be overwritten and
// can still be read at startup after short (<500ms) power off
volatile uint8_t noinit_decay __attribute__ ((section (".noinit")));
volatile uint8_t noinit_lvl __attribute__ ((section (".noinit")));

uint8_t EEMEM MODE_P;

/* Rise-Fall Ramping brightness selection /\/\/\/\
 * cycle through PWM values from ramp_LUT (look up table). Traverse LUT 
 * forwards, then backwards. Current PWM value is saved in noinit_lvl so
 *  it is available at next startup (after a short press).
*/
void ramp()
{
	uint8_t i = 0;
	while (1){
		for (i = 0; i < sizeof(ramp_LUT); i++){
			PWM_LVL = pgm_read_byte(&(ramp_LUT[i]));
			noinit_lvl = PWM_LVL; // remember after short power off
			_delay_ms(60); //gives a period of x seconds
		}
		for (i = sizeof(ramp_LUT) - 1; i > 0; i--){
			PWM_LVL = pgm_read_byte(&(ramp_LUT[i]));
			noinit_lvl = PWM_LVL; // remember after short power off
			_delay_ms(60); //gives a period of x seconds
		}
		
	}
}

/* Rising Ramping brightness selection //////
 * Cycle through PWM values from ramp_LUT (look up table). Current PWM 
 * value is saved in noinit_lvl so it is available at next startup 
 * (after a short press)
*/
void ramp2()
{
	uint8_t i = 0;
	while (1){
		for (i = 0; i < sizeof(ramp_LUT); i++){
			PWM_LVL = pgm_read_byte(&(ramp_LUT[i]));
			noinit_lvl = PWM_LVL; // remember after short power off
			_delay_ms(60); //gives a period of x seconds
		}
		
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
