# CloudSound
CLOUD SOUND - AN ATTINY85 BASED MIDI PROJECT. 
The primary motivation for this program was to generate music based upon the levels of sunlight as clouds pass by obscuring the sun.  "Cloud Sounds" all from an 8-Pin Chip :)
There are 4 main 'modes' that the system engages. Transition, Brighter, Darker and Static.
The LDR (ADC1) detects changes in light levels the eyes can't really perceive so be aware that this should only be used with natural sunlight changes. Even an AC powered tungsten filament light bulb or LED lamp could be interpreted as a radically changing light level. The program will simply generate nonsense music and rhythms. The light Tolerance variable can be used to compensate this.
ADC2 is used to read a potentiometer value to set the tempo/speed of the midi sequence.  ADC3 is used to read a second potentiometer value to set an 'offset' value used primarily for the filter    settings of the Boss DR-Synth DS-330 and other program controls.
The main sequence is 32 notes long. This can increased or reduced within the limitations of dynamic memory. With regards to sending the actual midi, PB0 is used via the software serial library to generate and send out the 31250 Midi Baud rate messages.  PB1 is used to indicate with an LED the running status.  These midi messages were developed specifically around the Boss DR-Synth 330 module. But I'm sure you could tweak them to suit whatever midi module you decide to connect it up to.

you can hear/see it in action here... https://youtu.be/BzlO7wsJvEU
