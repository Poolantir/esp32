#include "servo.h"
#include "config.h"
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <cmath>

static Servo sServo;
static int sCurDeg      = SERVO_REST_DEG;
static int sStartDeg    = SERVO_REST_DEG;
static int sTargetDeg   = SERVO_REST_DEG;
static uint32_t sMoveStartMs = 0;
static bool sMoving     = false;
static uint32_t sRampMs = DEFAULT_SERVO_RAMP_MS;
static bool sPaused     = false;
static uint32_t sPausedElapsedMs = 0;

void servoInit() {
  ESP32PWM::allocateTimer(0);
  sServo.setPeriodHertz(50);
  if (sServo.attach(PIN_SERVO, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH) == 0) {
    Serial.println("[WARN] Servo attach failed");
  }
  if (sServo.attached()) {
    sServo.write(constrain(SERVO_REST_DEG, 0, 180));
  }
  sCurDeg    = SERVO_REST_DEG;
  sStartDeg  = SERVO_REST_DEG;
  sTargetDeg = SERVO_REST_DEG;
  sMoving    = false;
}

void servoWriteImmediate(int deg) {
  if (!sServo.attached()) return;
  int clamped = constrain(deg, 0, 180);
  sServo.write(clamped);
  sCurDeg    = clamped;
  sStartDeg  = clamped;
  sTargetDeg = clamped;
  sMoving    = false;
  sPaused    = false;
}

void servoMoveAnimated(int targetDeg) {
  if (!sServo.attached()) return;
  int clamped = constrain(targetDeg, 0, 180);
  if (clamped == sTargetDeg && !sMoving && !sPaused) return;
  sStartDeg    = sCurDeg;
  sTargetDeg   = clamped;
  sMoveStartMs = millis();
  sMoving      = true;
  sPaused      = false;
}

void servoTick() {
  if (!sServo.attached() || !sMoving) return;

  uint32_t elapsed = millis() - sMoveStartMs;
  float t = (elapsed >= sRampMs) ? 1.0f : (float)elapsed / (float)sRampMs;

  float nextF = (float)sStartDeg + ((float)(sTargetDeg - sStartDeg) * t);
  int nextDeg = constrain((int)lroundf(nextF), 0, 180);

  if (nextDeg != sCurDeg) {
    sServo.write(nextDeg);
    sCurDeg = nextDeg;
  }

  if (t >= 1.0f) {
    sMoving = false;
  }
}

bool     servoIsMoving()  { return sMoving; }
int      servoCurrentDeg(){ return sCurDeg; }
void     servoSetRampMs(uint32_t ms) {
  sRampMs = constrain(ms, (uint32_t)200, (uint32_t)10000);
  Serial.printf("[SERVO] ramp set to %lu ms\n", (unsigned long)sRampMs);
}
uint32_t servoGetRampMs() { return sRampMs; }

void servoPause() {
  if (!sMoving || sPaused) return;
  sPausedElapsedMs = millis() - sMoveStartMs;
  sMoving = false;
  sPaused = true;
}

void servoResume() {
  if (!sPaused) return;
  sMoveStartMs = millis() - sPausedElapsedMs;
  sMoving = true;
  sPaused = false;
}
