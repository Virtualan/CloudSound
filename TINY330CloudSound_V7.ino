/*
  CLOUD SOUND - AN ATTINY85 BASED MIDI PROJECT.
  The primary motivation for this program was to generate music based upon the levels of sunlight
  as clouds pass by obscuring the sun.  "Cloud Sounds" all from an 8-Pin Chip :)
  There are 4 main 'modes' that the system engages. Transition, Brighter, Darker and Static.

  The LDR (ADC1) detects changes in light levels the eyes can't really perceive so be aware
  that this should only be used with natural sunlight changes. Even an AC powered tungsten
  filament light bulb or LED lamp could be interpreted as a radically changing light level.
  The program will simply generate nonsense music and rhythms. The light Tolerance variable
  can be used to compensate this.
  ADC2 is used to read a potentiometer value to set the tempo/speed of the midi sequence.
  ADC3 is used to read a second potentiometer value to set an 'offset' value used
  primarily for the filter settings of the Boss DR-Synth DS-330 and other program controls.
  The main sequence is 32 notes long. This can increased or reduced within the
  limitations of dynamic memory. With regards to sending the actual midi,
  PB0 is used via the software serial library to generate and send out the
  31250 Midi Baud rate messages.  PB1 is used to indicate with an LED the running status.
  These midi messages were developed specifically around the Boss DR-Synth 330 module.
  But I'm sure you could tweak them to suit whatever midi module you decide to connect it up to.

*/

#include <SoftwareSerial.h>
SoftwareSerial midiSerial(1, 0); // soft serial RX & TX

int beat = 0, index = 0;
char transitDir = -1;// snm1 = 0, snm2 = 0;
byte beatStep = 1, offset = 0, globFilt = 64, globRes = 64;
byte upFlag = 0 , staticFlag = 0, drumVolume = 101;
byte key = 0, ch = 0, scale = 1, staticNote = 64, lightNote = 0, lightTolerance = 8, progRange = 56;
int lightMeter = 0, olightLevel = 64, lightLevel = 64, lightChange = 0;  // these can go negitive
byte sysexArray[11] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};  //DS-330 GM mode
byte Ds330Kits[9] = {32, 0, 32, 8, 16, 24, 25, 40, 25}; // program numbers for the ds330 kits
unsigned long timeStamp = 0, timeStamp2 = 250, timeStamp3 = 0, tuneSpeed = 100, staticSpeed = 100;
const byte channels = 16, mas = 32;  // array size

// the basic midi message
struct midiEvent {
  byte channel = 9;
  char note = 50;
  byte velocity = 100;
  byte fingers = 5;  //solo root note
  byte len = 4;
  byte playing = 0;
};

//build an array of the above midi messages
struct midiEvent ma[mas];

void setup() {
  pinMode(2, INPUT); //the tuneSpeed speed pot
  pinMode(3, INPUT); //the note/lightLevel offset pot
  pinMode(1, OUTPUT);//output for the beat/activity LED
  pinMode(4, INPUT); //the LDR input for the lightLevel
  midiSerial.begin(31250);
  delay(500);
  // send out sysex for the DS330 DR Synth to switch to General Midi Multi Timberal Mode
  for (byte n = 0; n < 11; n++) {
    midiSerial.write(sysexArray[n]);
  }
  delay(500);
  // Randomly pick a drumkit
  ProgChange(9, Ds330Kits[random(10)]);
  //
  delay(500);

  // fill the arrays
  for (byte x = 0; x < mas ; x++) {
    randomSeed(analogRead(1)*analogRead(2) - analogRead(3));
    ma[x].note = random(36, 110);
    ma[x].channel = random(channels);
    ma[x].velocity = random(40, 120);
    ma[x].fingers = random(1, 0xFF);
    ma[x].len = random(2, 32);  // no of beats note will be sustained.
    ma[x].playing = 0;
  }
  MidiReset();
}

void loop() {
  // main beat timer circuit


  lightLevel = analogRead(1);  // read the LDR value
  lightNote = lightLevel % 127; //map(lightLevel, 0, 1023, 0, 127) - 50; // offset for general room daylight
  lightChange = (lightLevel - olightLevel) ; // measure difference since last reading

  if (staticFlag == 0) {

    DoTransit();
  }

  progRange = ((beat / mas) % 119); // increase instrument range through time

  // timeStamp = millis();

  // }

  if (staticFlag) {
    DoStatic();
  }


  //read the analog values
  tuneSpeed = map(analogRead(2), 0, 1023, 10, 500) ;//- lightMeter;
  offset = (map(analogRead(3), 0, 1023, 0, 127));   // offset for cut off freq



  //check the changes caught by the LDR // over 8 beats?

  if (lightChange < 0) {   //moving to darker
    lightMeter --;
    globFilt --;
    upFlag = 0;
    staticSpeed ++;
    //lightChange = 0;
  }
  if (lightChange > 0) {   //moving to brighter
    lightMeter ++;
    globFilt ++;
    upFlag = 1;
    staticSpeed --;
    //lightChange = 0;
  }

  if ((lightChange < 0 - lightTolerance) && staticFlag) { //got a lot darker?
    GotAlotDarker();
    lightChange = 0;
    //drumVolume = 100;
    staticFlag =  0;
    beatStep = 2 + random(17);
  }

  if ((lightChange > lightTolerance) && staticFlag) {  //got a lot brighter?
    GotAlotBrighter();
    lightChange = 0;
    //drumVolume = 100;
    staticFlag = 0;
    beatStep = 2 + random(17);
  }

  // for use as note and tempo moving motivator
  if (abs(lightMeter) > 12) { // if greater that +/- 12 reset to zero
    lightMeter = 0;
    beatStep = 1;
  }
  DoChanges(random(channels));

  if (abs(lightChange) < lightTolerance ) {//stay the same?


    if (millis() > timeStamp3 + 15000) {  // a 15 seconds timer
      upFlag = byte((beat / 8) % 3);  // then alternate chord masks
      timeStamp3 = millis();

      if (staticFlag == 0) {
        ProgChange(random(channels), random(progRange));  // change the static ambiance channel sound every timer3 trigger
      }

      beatStep = 1; // reset the beat speed.

    }
    //flash running light non static mode
    if (staticFlag == 0) {
      digitalWrite(1, !(beat % 4)); // fast blink (on the 4th beat)
    }
  }



  // reset lightLevel
  if (beat % 8 == 0) {
    olightLevel = lightLevel;
  }

  // cater for any stuck notes (there shouldn't be any)
  //CC(beat % 16, 120, 0);

  //increment loop counter
  index ++;
}
//end of loop

void DoStatic() {
  // alternative static test circuit

  if (millis() > timeStamp2 + staticSpeed ) {
    DoDrums();
    beat ++;

    //staticFlag = 1;
    byte ca = (byte(beat) % channels);//random(channels);
    byte nn = staticNote += lightChange;
    if (nn < 24) {
      nn = 36;
    }
    if (nn > 70) {
      nn = random(24, 70);
    }
    globFilt -= lightMeter;
    nn = ScaleFilter(scale, nn, key);
    //snm1 = nn;
    //if (ca != 9) {
    //CC(ca, 7, 100);
    //ProgChange(ca, random(88, 96));  //pad sounds

    //}
    // set drum channel volume
    CC(9, 7, drumVolume - lightMeter);
    NoteOn(ca, nn, random(drumVolume + lightMeter));
    NoteOff(ca, nn);
    //snm2 = snm1;
    DoChanges(random(channels));
    if (beat % 8 == 0 ) {
      ma[byte(beat) % mas].len = random(4, 16);
      if (ma[byte(beat) % mas].playing == 0) {
        ma[byte(beat) % mas].note = nn;
      }

      DoTransit();
    }
    timeStamp2 = millis();
    //flash running light
    if (staticFlag) {
      digitalWrite(1, !(index % 64)); //hearbeat pulse
    }
  }
}
//}

void DoTransit() {
  if (staticSpeed < tuneSpeed ) {
    staticSpeed = tuneSpeed;
  }
  if (staticSpeed > tuneSpeed * 4) {
    staticSpeed = tuneSpeed * 4;
  }
  if (millis() >= timeStamp + tuneSpeed ) {
    if (!staticFlag) {
      DoDrums();
      beat += beatStep;
    }
    //scan the main midi message sequence/array
    for (byte x = 0; x < mas; x++) {
      ma[x].fingers |= 0b00000001;

      if (x == byte(beat % mas) && ma[x].playing == 0) {

        if (upFlag == 0) {
          ma[x].fingers &= 0b10001001;  //minor on dulling light
        }

        if (upFlag == 1) {
          ma[x].fingers &= 0b10010001;  //major on brighter light
        }

        // scale correction
        ma[x].note = ScaleFilter(scale, ma[x].note, key);

        for (byte f = 0; f < 8; f++) {
          if (ma[x].fingers >> f & 1) {
            byte n = ScaleFilter(scale, ma[x].note + f, key);
            NoteOn(ma[x].channel, n , ma[x].velocity);
          }
        }
        ma[x].playing = 1;  // the note is now playing
      }

      // Note off handling
      if (ma[x].playing == 1 && beat % ma[x].len == 0) {   //turn note off at each .len value
        for (byte f = 0; f < 8; f++) {
          if (ma[x].fingers >> f & 1) {
            byte n = ScaleFilter(scale, ma[x].note + f, key);
            NoteOff(ma[x].channel, n);
          }
        }
        ma[x].playing = 0;  // the note is now off
      }

      // adjustments to array of notes that are not playing to prevent stuck notes
      if (ma[x].playing == 0) {

        // adjust note into useable midi range
        if (ma[x].note < 20 || ma[x].note > 110) {
          ma[x].note = 60;
          ma[x].velocity = 100;
        }

        // random program changes on all but drums and the static ambiance channel
        if (random(20) == 5) {
          ch = random(channels);
          if (not(ch == 9 || ch == 12)) {
            ProgChange(ch, random(progRange));
          }
        }
        // fade out velocity on light inactivity but dont fade out too quickly
        if (ma[x].velocity > 25 && beat % 4 == 0) { //
          ma[x].velocity = drumVolume;  // reduce velocity
          // random channel changes
          if (random(10) > 8) {
            ma[x].channel = random(channels);
          }
          // set up array for static mode
          if (ma[x].velocity < 25) { // when quiet mute the channel
            //CC(ma[x].channel, 7, 0); //mute channel
            //ma[x].channel = 12;  //force note to ambient channel
            ma[x].note = lightNote;
            //ma[x].velocity = random(6, 30);  // not too loud
            ma[x].len = random(12, 24);  // nice and long for pad sounds
            // Who knows what sound will be used for the ambient sounds during the static stages

          }
        }
        if (random(1800) < 3  ) { //&& !x
          ma[x].note = random(24, 110);
        }
        // change chord shape for mask
        ma[x].fingers ++;

        //melodize the array if next note is not within an octaves range
        if ( x > 0 && staticFlag == 0) {
          if (ma[x].note - ma[x - 1].note < -6) {
            ma[x].note ++;
          }
          if (ma[x].note - ma[x - 1].note > 6) {
            ma[x].note --;
          }
        }

        if (random(80) == 5 && !x ) {
          ma[x].note += (random(13) - 6);
        }
        if (drumVolume <= 30) {  // Static mode trigger
          if (staticFlag == 0) {
            DoScrub(0);  // kill any playing notes
            staticSpeed = tuneSpeed; // set static speed to match playing speed
          }

          staticFlag = 1;  // !!!!!!!!!!we just entered static mode!!!!!!!!!!!!
          ma[x].note = lightNote;  // force the note to reflect the light level
          // it's dark or dazzling
          if (ma[x].note < 20 || ma[x].note > 110) {
            ma[x].note = (byte(index) % 48) + 36 ;
          }
        }
      }
    }
    timeStamp = millis();
  }

} //end of DoTransit


void DoScrub(byte e) {
  // turn off notes that are playing in the array to prevent stuck notes
  for (byte x = 0; x < mas; x++) {
    if (ma[x].playing == 1) {
      for (byte f = 0; f < 8; f++) {
        if (ma[x].fingers >> f & 1  ) {
          byte n = ScaleFilter(scale, ma[x].note + f, key);
          NoteOff(ma[x].channel, n);
        }
      }
      ma[x].playing = 0;
    }
    if (e) {
      ma[x].note = random(24, 101);
      ma[x].channel = random(channels);
      ma[x].velocity = random(80, 110);
    }
  }
  scale = random(10);
  key = random(12);
} // End of DoScrub

void GotAlotBrighter() {

  DoScrub(1);  // clear all playing notes and randomize
  byte x = byte(beat % mas);
  ma[x].len = random(3, 16);
  if (ma[x].velocity < 100) {
    ma[x].velocity = random(60, 110);
  }
  for (byte n = 0; n < 16; n++) {
    CC(n, 7, 110); //reset the muted channel
  }
  ma[x].note = lightNote;
  if (ma[x].note < 36 || ma[x].note > 100) {
    ma[x].note = random(36, 100);
  }
  ma[x].channel = random(channels);

}


void GotAlotDarker() {

  DoScrub(1);   // clear all playing notes and randomize
  byte x = byte(beat % mas);
  ma[x].len = random(4, 16);
  if (ma[x].velocity < 100) {
    ma[x].velocity = random(60, 110);
  }
  for (byte n = 0; n < 16; n++) {
    CC(n, 7, 110); //reset the muted channel
  }
  ma[x].note = lightNote;
  if (ma[x].note < 36 || ma[x].note > 100) {
    ma[x].note = random(36, 100);
  }
  ma[x].channel = random(channels);

}

void DoChanges(byte ch) {

  if ((random(100) == 5) ) {
    ProgChange(9, Ds330Kits[random(10)]);
  }

  if (beat % 64 == 0) {
    ma[random(mas)].len = random(1, 32);
  }

  if (beat % 128 == 0) {
    DoScrub(0);   // kill all playing notes change key and scale (dont randomize)
  }

  // controller changes for Boss DS330
  // global filter
  // globFilt += random(9) - 4;
  globRes += random(9) - 4;

  if (globFilt < 24 || globFilt > 100) {
    globFilt = 64;
  }

  if (globRes < 30 || globRes > 90) {
    globRes = 60;
  }

  //ch = byte(beat) % channels;
  if (ch != 12) {
    DoFilter(ch , globRes, globFilt - offset );
  }
  else {
    DoFilter(ch , 64, globFilt - offset);  // it must be 12
  }



  // random pans
  //ch = random(channels);
  if (ch != 9 ) {   // not drums
    CC(ch, 10, random(0, 127)); //pan
  }
  //reverb send
  //ch = random(channels);
  if (random(5) == 0) {
    CC(ch, 91, random(0, 127));
  }
  //chorus send
  //ch = random(channels);
  if (random(5) == 0) {
    CC(ch, 93, random(0, 127));
  }
  //hold
  //  if (ch != 9 && random(9) == 0) {
  //    CC(ch, 64, random(0, 127));
  //  }

  // modulation
  //ch = random(channels);
  if (ch != 9 && random(10) == 0) {
    CC(ch, 1, random(0, 127));
  }

  // random decay time
  //ch = random(channels);
  if (ch != 9 && random(12) == 0) {
    CC(ch, 0x63, 0x01);
    CC(ch, 0x62, 0x64);
    CC(ch, 6, random(0x3e, 0x72));
  }

  // random release time
  //ch = random(channels);
  if (ch != 9 && random(12) == 0) {
    CC(ch, 0x63, 0x01);
    CC(ch, 0x62, 0x66);
    CC(ch, 6, random(0x4e, 0x72));
  }

  // random attack time
  //ch = random(channels);
  if (ch != 9 && random(24) == 0) {
    CC(ch, 0x63, 0x01);
    CC(ch, 0x62, 0x63);
    CC(ch, 6, random(0x0e, 0x32)); // random(0x0e, 0x72)
  }

  if (random(channels) == 0) {  // drum tunings
    CC(9, 0x63, 0x18);
    CC(9, 0x62, random(24, 68));
    CC(9, 6, random(0x2e, 0x5f));
  }
}

void DoFilter(byte ch, byte res, byte coff) {
  CC(ch, 0x63, 0x01);
  CC(ch, 0x62, 0x21);
  CC(ch, 6, res); //resonance can go to 0x72
  CC(ch, 0x63, 0x01);
  CC(ch, 0x62, 0x20);
  CC(ch, 6, coff);//cut off frequency
}

//drums
void DoDrums() {
  if (staticFlag == 0 && beat % 8 == 0) {
    CC(9, 7, drumVolume--); // send the drum channel volume.
  }
  if (staticFlag == 1 && beat % 8 == 0 && drumVolume < 50) {
    CC(9, 7, drumVolume++); // send the drum channel volume.
  }

  if (beat % 4 == 0 && !staticFlag) {
    byte bd = 35 + random(2);
    NoteOn(9, bd, 110);
    NoteOff(9, bd);
  }
  if (beat % 8 == 0 && !staticFlag) {
    byte sn = 38 + (random(2) * 2);
    NoteOn(9, sn, 100);
    NoteOff(9, sn);
  }
  if (upFlag > 0 && !staticFlag) {
    if ( beat % (6 * upFlag) == 0) {
      byte sn = 38 + (random(2) * 2);
      NoteOn(9, sn, 100);
      NoteOff(9, sn);
    }
  }
  if (beat % 2 == 0 && !staticFlag) {
    byte hh = 42 + (random(3) * 2);
    NoteOn(9, hh, random(70, 95));
    NoteOff(9, hh);
  }

  if (beat % 128 >= 112 && !staticFlag) {
    byte hh = 36 + random(24);
    NoteOn(9, hh, random(80, 90));
    NoteOff(9, hh);
  }
  else if ((beat % 3 == 0 || beat % 7 == 0) && !staticFlag) {
    byte hh = 36 + (ma[byte(beat) % mas].note % 12);
    NoteOn(9, hh, random(40, 80));
    NoteOff(9, hh);
  }
}
void MidiReset() {
  for (byte c = 0; c < channels; c++) {
    CC(c, 123, 0);
    CC(c, 120, 0);
    CC(c, 121, 0);
    CC(c, 91, 0);
    CC(c, 93, 0);
    CC(c, 7, 84);
    CC(c, 10, 64);
    if (c != 9) {
      ProgChange(c, random(progRange));
    }
  }
}

void ProgChange(byte chan, byte prog) {
  midiSerial.write(chan + 0xc0);
  midiSerial.write(prog);
}

void CC(byte chan, byte cont, byte val) {
  midiSerial.write(chan + 0xb0);
  midiSerial.write(cont);
  midiSerial.write(val);
}

void NoteOn(byte chan, byte note, byte vel) {
  midiSerial.write(chan + 0x90);
  midiSerial.write(note);
  midiSerial.write(vel);
}

void NoteOff(byte chan, byte note) {
  midiSerial.write(chan + 0x80);
  midiSerial.write(note);
  midiSerial.write(byte(0));
}

byte ScaleFilter(byte scale, char n, byte key) {

  switch (scale) {
    case 0: //Harmonic Minor
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 4
          || (n + (key % 12)) % 12 == 6
          || (n + (key % 12)) % 12 == 9
         )
      {
        return n - 1; break;
      }
      else if ((n + (key % 12)) % 12 == 10
              )
      {
        return n + 1; break;
      }
      else {
        return n;
        break;
      }


    case 1: //Minor Melodic
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 4
          || (n + (key % 12)) % 12 == 6
          || (n + (key % 12)) % 12 == 8
          || (n + (key % 12)) % 12 == 10)

      {
        return n - 1; break;
      }

      else {
        return n;
        break;
      }

    case 2: //Diminished
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 4
          || (n + (key % 12)) % 12 == 7
          || (n + (key % 12)) % 12 == 10)
      {
        return n - 1; break;
      }

      else {
        return n;
        break;
      }

    case 3: //Blues
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 4
          || (n + (key % 12)) % 12 == 8)

      {
        return n - 1; break;
      }
      else if ((n + (key % 12)) % 12 == 2
               || (n + (key % 12)) % 12 == 9)

      {
        return n + 1; break;
      }

      else {
        return n;
        break;
      }

    case 4: //Major Pentatonic

      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 3
          || (n + (key % 12)) % 12 == 5
          || (n + (key % 12)) % 12 == 8
          || (n + (key % 12)) % 12 == 10)
      {
        return n - 1; break;
      }
      else if ((n + (key % 12)) % 12 == 6
               || (n + (key % 12)) % 12 == 11)

      {
        return n - 2; break;
      }

      else {
        return n;
        break;
      }

    case 5: //Minor Pentatonic

      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 8)

      {
        return n - 1; break;
      }

      else {
        return n;
        break;
      }

    case 6: //Gypsy

      if ((n + (key % 12)) % 12 == 2
          || (n + (key % 12)) % 12 == 6
          || (n + (key % 12)) % 12 == 10)

      {
        return n - 1; break;
      }
      else if ((n + (key % 12)) % 12 == 3
               || (n + (key % 12)) % 12 == 11)

      {
        return n - 2; break;
      }

      else {
        return n;
        break;
      }

    case 7: //Augmented
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 3
          || (n + (key % 12)) % 12 == 5
          || (n + (key % 12)) % 12 == 7
          || (n + (key % 12)) % 12 == 10)

      {
        return n - 1; break;
      }

      else {
        return n;
        break;
      }

    case 8: //Alans - black keys
      if ((n + (key % 12)) % 12 == 0
          || (n + (key % 12)) % 12 == 2
          || (n + (key % 12)) % 12 == 5
          || (n + (key % 12)) % 12 == 7
          || (n + (key % 12)) % 12 == 9)
      {
        return n + 1; break;
      }
      else if ((n + (key % 12)) % 12 == 4
               || (n + (key % 12)) % 12 == 11)
      {
        return n - 1; break;
      }

      else {
        return n;
        break;
      }


    case 9: // whole tone
      if ((n + (key % 12)) % 12 == 1
          || (n + (key % 12)) % 12 == 3
          || (n + (key % 12)) % 12 == 5
          || (n + (key % 12)) % 12 == 7
          || (n + (key % 12)) % 12 == 9
          || (n + (key % 12)) % 12 == 11)
      {
        return n - 1; break;
      }
      else {
        return n;
        break;
      }
  }
}

