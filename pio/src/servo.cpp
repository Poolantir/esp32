// AI-ASSISTED
// Matt Krueger
// April 2026

#include "servo.h"
#include "config.h"
#include <ESP32Servo.h>
#include <ESP32PWM.h>

static Servo sServo;
static int sCurDeg = SERVO_REST_DEG;

void servoInit() {
  ESP32PWM::allocateTimer(0);
  sServo.setPeriodHertz(50);
  if (sServo.attach(PIN_SERVO, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH) == 0) {
    Serial.println("[WARN] Servo attach failed");
  }
  if (sServo.attached()) {
    sServo.write(constrain(SERVO_REST_DEG, 0, 180));
  }
  sCurDeg = SERVO_REST_DEG;
}

void servoWriteImmediate(int deg) {
  if (!sServo.attached()) return;
  int clamped = constrain(deg, 0, 180);
  sServo.write(clamped);
  sCurDeg = clamped;
}

int servoCurrentDeg() { return sCurDeg; }
