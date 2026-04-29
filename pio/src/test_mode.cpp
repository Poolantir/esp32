// AI-ASSISTED
// Matt Krueger
// April 2026

#include "test_mode.h"
#include "config.h"
#include "servo.h"
#include "led.h"
#include "tof.h"
#include "ble.h"
#include "clock.h"
#include <deque>

enum QueueState { Q_IDLE, Q_HOLDING, Q_GAP };
enum LedTrack   { TL_NONE, TL_BLUE, TL_GREEN, TL_RED };

static QueueState sQueueState = Q_IDLE;
static std::deque<int> sQueue;
static ClockTimer sQueueTimer;
static uint32_t sQueueHoldMs = 0;
static LedTrack sPrevLed = TL_NONE;

static void queueTick() {
  switch (sQueueState) {

    case Q_IDLE:
      if (sQueue.empty()) return;
      {
        int val = sQueue.front();
        sQueue.pop_front();
        sQueueHoldMs = (uint32_t)val * 1000UL;
        servoWriteImmediate(SERVO_MAX_DEG);
        sQueueTimer.start();
        sQueueState = Q_HOLDING;
        Serial.printf("[TEST QUEUE] -> MAX, hold %lu ms (%d left)\n",
                      (unsigned long)sQueueHoldMs, (int)sQueue.size());
      }
      break;

    case Q_HOLDING:
      if (sQueueTimer.expired(sQueueHoldMs)) {
        servoWriteImmediate(SERVO_REST_DEG);
        sQueueTimer.start();
        sQueueState = Q_GAP;
        Serial.println("[TEST QUEUE] hold done, -> REST");
      }
      break;

    case Q_GAP:
      if (!sQueueTimer.expired(SIM_GAP_MS)) return;
      sQueueState = Q_IDLE;
      if (sQueue.empty()) Serial.println("[TEST QUEUE] complete");
      break;
  }
}

void enterTestMode() {
  servoWriteImmediate(SERVO_REST_DEG);
  if (bleIsConnected()) { ledSetBlue(); sPrevLed = TL_BLUE; }
  else                  { ledAllOff();  sPrevLed = TL_NONE; }
  tofStartContinuous();
  sQueueState = Q_IDLE;
  sQueue.clear();
  Serial.println("[TEST] entered TEST mode");
}

void testModeTick() {
  queueTick();

  if (!bleIsConnected()) {
    if (sPrevLed != TL_NONE) { ledAllOff(); sPrevLed = TL_NONE; }
    return;
  }

  bool inRange     = tofIsInRange();
  bool queueActive = (sQueueState != Q_IDLE || !sQueue.empty());

  LedTrack desired;
  if (inRange)            desired = TL_RED;
  else if (queueActive)   desired = TL_GREEN;
  else                    desired = TL_BLUE;

  if (desired != sPrevLed) {
    switch (desired) {
      case TL_BLUE:  ledSetBlue();  break;
      case TL_GREEN: ledSetGreen(); break;
      case TL_RED:   ledSetRed();   break;
      default: break;
    }
    sPrevLed = desired;
  }
}

void testLedCmd(const String& color) {
  ledFlashTest(color);
  sPrevLed = TL_NONE;
}

void testServoCmd(const String& action) {
  String a = action;
  a.toUpperCase();

  int deg;
  if      (a == "MAX")  deg = SERVO_MAX_DEG;
  else if (a == "REST") deg = SERVO_REST_DEG;
  else {
    Serial.printf("[TEST SERVO] invalid: \"%s\"\n", action.c_str());
    return;
  }

  servoWriteImmediate(deg);
  Serial.printf("[TEST SERVO] -> %s (%d deg)\n", a.c_str(), deg);
}

void testQueueCmd() {
  if (sQueueState != Q_IDLE || !sQueue.empty()) {
    Serial.println("[TEST QUEUE] already running");
    return;
  }
  sQueue.push_back(1);
  sQueue.push_back(2);
  sQueue.push_back(1);
  sQueue.push_back(1);
  Serial.printf("[TEST QUEUE] starting, %d items\n", (int)sQueue.size());
}
