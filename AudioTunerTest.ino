/* Audio Tuner Test App
 * for Colin Duffy's Library Guitar and Bass Tuner
 * Copyright (c) 2015, David Elvig
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <AudioTuner.h>
AudioTuner                tuner;

AudioInputI2S            Mic;            //xy=702,162
AudioSynthWaveformSine   sineFundamental; //xy=721,250
AudioSynthWaveformSine   sine2ndHarmonic; //xy=721,285
AudioSynthWaveformSine   sine3rdHarmonic; //xy=721,320
AudioMixer4              pitchMixer;      //xy=979,172
AudioMixer4              leftMixer;      //xy=1200,150
AudioMixer4              rightMixer;     //xy=1200,369
AudioSynthWaveformSine   sineFromFFTSynth; //xy=984,405
AudioOutputI2S           Jack;           //xy=1332,275
AudioConnection          patchCord1(Mic, 0, pitchMixer, 0);
AudioConnection          patchCord2(sineFundamental, 0, pitchMixer, 1);
AudioConnection          patchCord3(sine2ndHarmonic, 0, pitchMixer, 2);
AudioConnection          patchCord4(sine3rdHarmonic, 0, pitchMixer, 3);
AudioConnection          patchCord6(pitchMixer, 0, leftMixer, 0);
AudioConnection          patchCord7(leftMixer, 0, Jack, 0);
AudioConnection          patchCord8(sineFromFFTSynth, 0, rightMixer, 0);
AudioConnection          patchCord9(rightMixer, 0, Jack, 1);
AudioControlSGTL5000     TeensyCodec;    //xy=724,113

AudioConnection          patchCord5a(pitchMixer, tuner);

//*********************************************************************
// Test of AudioTuners response to sine wave input or microphone
// Left earphone play the input, right earphone plays the calculated output.
//
// PIN1-PIN3 allow mixing various ratios of fundamental frequency and harmonics one and two
// PITCH_PIN adjusts fundamental frequency in a range of roughly 0 - 4100 Hz
// LOOP_DELAY - I adjust this to get zero fails to AudioTuner.available().  70 ms is about the minimum in my testing

#define PIN1      15
#define PIN2      16
#define PIN3      17
#define PITCH_PIN 20
#define LOOP_DELAY 70
unsigned long oldTime = 0;

void setup() { 
    Serial.begin(9600);
    delay(500);
    Serial.println("Started...");
  
    AudioMemory(30);
 
    TeensyCodec.enable();
    TeensyCodec.volume(.3);
    TeensyCodec.inputSelect(AUDIO_INPUT_MIC);
    TeensyCodec.micGain(40);
    
    sineFundamental.amplitude(.5);
    sine2ndHarmonic.amplitude(.5);
    sine3rdHarmonic.amplitude(.5);
  
    tuner.initialize(.15);
    oldTime = millis();
}

bool synthOn = false;

void setMixers(float pot1, float pot2, float pot3) {
    float potTotal = pot1 + pot2 + pot3;
    
    // cover for flaky pots that cannot reach 0
    if (pot1 > .02) {pot1 = pot1/potTotal;} else {pot1 = 0;}
    if (pot2 > .02) {pot2 = pot2/potTotal;} else {pot2 = 0;}
    if (pot3 > .02) {pot3 = pot3/potTotal;} else {pot3 = 0;}
    
    if (pot1 + pot2 + pot3  > 0) {
        synthOn = true;
        pitchMixer.gain(0, .5);
        pitchMixer.gain(1, pot1);
        pitchMixer.gain(2, pot2);
        pitchMixer.gain(3, pot3);
    } else {
        synthOn = false;
        pitchMixer.gain(0, .5);
        pitchMixer.gain(1, 0);
        pitchMixer.gain(2, 0);
        pitchMixer.gain(3, 0);
    }   
    rightMixer.gain(0, 1);
    rightMixer.gain(1, 0);
    rightMixer.gain(2, 0);
    rightMixer.gain(3, 0);
}

int lastInFreq = 0;
float lastPot1 = 0, lastPot2 = 0, lastPot3 = 0;
unsigned long loopCount = 0;

void loop() {
    int inFreq = 0, outFreq = 0;
    Serial.print("Loops: "); Serial.print(loopCount++);
      
    float pot1 = (float)analogRead(PIN1)/1024;
    float pot2 = (float)analogRead(PIN2)/1024;
    float pot3 = (float)analogRead(PIN3)/1024;
    setMixers(pot1, pot2, pot3);

    if (synthOn) {
         inFreq = analogRead(PITCH_PIN) * 4;
         if (abs(inFreq-lastInFreq) < 10) { inFreq = lastInFreq; } else { lastInFreq = inFreq;}  // to avoid effect of jittery potentiometer
    
         sineFundamental.frequency(inFreq);
         sine2ndHarmonic.frequency(inFreq * 2);  
         sine3rdHarmonic.frequency(inFreq * 3);
         
    } else {
         inFreq = 0;
         sineFundamental.frequency(0);
         sine2ndHarmonic.frequency(0);  
         sine3rdHarmonic.frequency(0);
    }

    if (tuner.available()) {
         outFreq = tuner.read();
         float prob = tuner.probability();
         sineFromFFTSynth.frequency(outFreq);
         sineFromFFTSynth.amplitude(1);
         
         float ratio = 0;
         if (inFreq>0) {
              ratio = (abs((float)outFreq-(float)inFreq))  / (float)inFreq;
         }
         Serial.print("\tinFreq: ");  Serial.print(inFreq);
         Serial.print("\toutFreq: "); Serial.print(outFreq);
         Serial.print("\tProb: ");    Serial.print(prob);  
         Serial.print("\tAudMem: ");  Serial.print(AudioMemoryUsage());  
         Serial.print("\tErr: ");     Serial.print(ratio * 100);
         Serial.print("\tpot1: ");    Serial.print(pot1);
         Serial.print("\tpot2: ");    Serial.print(pot2);
         Serial.print("\tpot3: ");    Serial.print(pot3);
         Serial.print("\tms: ");      Serial.println(millis() - oldTime);
         
         oldTime = millis();
         
         delay(LOOP_DELAY);
    } else {
         outFreq = 0;
         sineFromFFTSynth.frequency(outFreq);
         sineFromFFTSynth.amplitude(0);
         Serial.println("\tTuner not available()");
         delay(10);
    }
}
