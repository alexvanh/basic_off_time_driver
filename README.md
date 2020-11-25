Update: After approximately 6 years of use of this firmware, the off-time mode switching no longer works correctly. An off-time of even greater than 1 minute is interpreted as a momentary button press (off-time < 500ms). The flashlight can malfunction and behave like a "next mode memory" flashlight and can also unintentionally enter the hidden strobe mode. The nature of the decay of the bits in SRAM after the ATTiny is powered down has seemingly changed over time. As a result, I would consider this method of determining the off-time of the flashlight to be unreliable for long term use. 

# basic_off_time_driver
Flashlight driver firmware with ramping using off-time to 
switch modes on attiny13 nanjg drivers.
#Modes
The flashlight driver has 6 main modes: 
high, medium, low, moonlight, smooth ramp, ramp selection. (plus a hidden strobe mode).

To change modes the user must turn the flashlight off then on in less 
than about 500ms (off-time mode switching), such as by quickly pressing
the switch halfway.
The driver has optional mode memory, enabled if MODE_MEMORY is defined
when compiled. The current mode the driver is in is memorized if in that mode for more than 1 second. The light will come on in the same mode the next time the light turns on. 

#Ramping
When the user goes in to ramping mode the light will smoothly increase 
and decrease brightness. A short press will select the current brightness
and the light will stay at that level in ramp selection mode.

#Strobe
To access the strobe the user must very quickly press the switch at least
3 times in a row, with very short on times in between. 

#Off-time mode switching implementation
Previously off-time mode switching was not possible without hardware
modifications (such as adding a capacitor to a spare pin of the 
attiny). The method presented in this firmware can be used on a stock 
nanjg driver.

Off-time mode switching is implemented by storing a flag in an area of 
memory that does not get initialized. There is enough energy stored in 
the decoupling capacitor to power the SRAM and keep data during power 
off for about 500ms.

When the firmware starts a byte flag (decay) is checked. If the flashlight
was off for less than ~500ms all bits will still be 0. If the
flashlight was off longer than that some of the bits in SRAM will
have decayed to 1. After the flag is checked it is reset to 0.
Being off for less than ~500ms means the user half-pressed the
switch (using it as a momentary button) and intended to switch modes.

This can be used to store any value in memory. Checking that no
bits in the flag have decayed acts as a checksum on the flag and seems 
to be enough to be reasonably certain other SRAM data is still valid.

In order for this to work brown-out detection must be enabled by
setting the correct fuse bits. I'm not sure why this is, maybe
reduced current consumption due to the reset being held once the
capacitor voltage drops below the threshold?

The smooth ramping mode uses the ram retention method described above.
Without ram retention, the ramp selection would rely on the eeprom. The
large number of writes required by the smooth ramp could cause the 
eeprom to fail if left on for an extended amount of time.

#Fuse bits
Example of working fuse bit configuration avrdude arguments:
-U lfuse:w:0x79:m -U hfuse:w:0xed:m 
