/*
Copyright (c) 2014-2015 NicoHood
See the readme for credit to other people.

PinChangeInterrupt_TickTock
Demonstrates how to use the library

Connect a button/cable to pin 7 and ground.
The Led state will change if the pin state does.

PinChangeInterrupts are different than normal Interrupts.
See readme for more information.
Dont use Serial or delay inside interrupts!
This library is not compatible with SoftSerial.

The following pins are usable for PinChangeInterrupt:
Arduino Uno/Nano/Mini: All pins are usable
Arduino Mega: 10, 11, 12, 13, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64),
              A11 (65), A12 (66), A13 (67), A14 (68), A15 (69)
Arduino Leonardo/Micro: 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI)
HoodLoader2: All (broken out 1-7) pins are usable
Attiny 24/44/84: All pins are usable
Attiny 25/45/85: All pins are usable
Attiny 13: All pins are usable
ATmega644P/ATmega1284P: All pins are usable
*/

#include "PinChangeInterrupt.h"
int counter = 0;
int previous = 0;

#define pinBlink 7
#define INCREASE(pin) void increase_##pin(void) { ++countersNow[pin]; }
#define ATTACH(pin) attachPCINT(digitalPinToPCINT(pin), increase_##pin, CHANGE)
int countersNow[12] = {};
int countersPrevious[12] = {};

// choose a valid PinChangeInterrupt pin of your Arduino board
INCREASE(1)
INCREASE(2)
INCREASE(3)
INCREASE(4)
INCREASE(5)
INCREASE(6)
INCREASE(7)
INCREASE(8)


void setup() {
  Serial.begin(115200);
  // set pin to input with a pullup, led to output
  pinMode(pinBlink, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  // attach the new PinChangeInterrupt and enable event function below
  attachPCINT(digitalPinToPCINT(pinBlink), increase_7, CHANGE);
  ATTACH(7);
}

void blinkLed(void) {
  // switch Led state
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  ++counter;
}

void loop() {
  for ( int a = 0 ; a < 8 ; ++a )
  {
    countersPrevious[a] = countersNow[a];
    Serial.println(countersPrevious[a]);
  }
  delay(1000);
  for ( int a = 0 ; a < 8 ; ++a )
  {
    Serial.println(countersNow[a] - countersPrevious[a]);
  }
}
