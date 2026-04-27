#include <Arduino.h>
#include <ESP32Servo.h>
#include "clock.h"
#include <Wire.h>

#define PIN_SERVO 14  // GPIO 14 (orange wire)

// testing purposes
#define LED_a 4       // GPIO 4
#define LED_b 16      // GPIO 16 
#define LED_c 17      // GPIO 17 

Servo servo;

//////////////////////////
//    INITIALIZATION    //
//////////////////////////
void initServo() {
  servo.attach(PIN_SERVO);
}

void initLeds() {
  pinMode(LED_a, OUTPUT);
  pinMode(LED_b, OUTPUT);
  pinMode(LED_c, OUTPUT);
  digitalWrite(LED_a, LOW);
  digitalWrite(LED_b, LOW);
  digitalWrite(LED_c, LOW);
}

void setup() {
  Serial.begin(115200);
  initLeds();
  initServo();
}

//////////////////////////
//        LOOP          //
//////////////////////////
void loop() {
  static bool startedTimedTask = false;
  static bool timedTaskDone = false;
  static ClockTimer timedTimer;

  // start the timer
  if (!startedTimedTask) {
    timedTimer.start();
    startedTimedTask = true;
    Serial.println("[Clock] Servo timed task: running for 10s");
  }

  // first task
  if (!timedTaskDone) {
    // Example timed work: sweep between 0 and 90 degrees for ~10 seconds.
    servo.write(0);
    delay(500);
    servo.write(90);
    delay(500);
    servo.write(0);
    delay(500);
    servo.write(180);
    delay(500);


    // check if the timer has expired
    if (timedTimer.expired(10000)) {
      timedTaskDone = true;
      servo.write(0);
      Serial.println("[Clock] Servo timed task complete; continuing");
      timedTimer.stop();
    }
  }

  // reset and new test
  Serial.println("[Clock] Servo timed task: new test");
  timedTimer.reset();
  delay(1000);
  timedTimer.start();

  // loop to move the servo 1 degree every 44ms
  // 4s --> 4000ms 
  while (!timedTaskDone) {
    servo.write(1);
    delay(44); // delay 44ms 
    if (timedTimer.expired(4000)) {
      servo.write(0);
      timedTimer.reset();
      return;
    }
  }
}