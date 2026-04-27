#include "sim_mode.h"
#include "config.h"
#include "servo.h"
#include "led.h"
#include "tof.h"
#include "ble.h"
#include "clock.h"
#include <ArduinoJson.h>

enum SimState { SIM_IDLE, SIM_MOVING_MAX, SIM_HOLDING };

static SimState sState     = SIM_IDLE;
static String   sUserId;
static uint32_t sDurationMs = 0;
static ClockTimer sHoldTimer;

static bool     sPaused          = false;
static SimState sSavedState      = SIM_IDLE;
static uint32_t sRemainingHoldMs = 0;
static bool     sServoWasMoving  = false;

static void sendSimComplete(const String& id, bool success) {
  JsonDocument doc;
  doc["command"] = "SIM";
  doc["id"]      = id;
  doc["type"]    = "COMPLETE";
  doc["action"]  = success;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

void enterSimMode() {
  servoWriteImmediate(SERVO_REST_DEG);
  ledSetGreen();
  tofStopContinuous();
  sState      = SIM_IDLE;
  sUserId     = "";
  sDurationMs = 0;
  sPaused     = false;
  Serial.println("[SIM] entered SIM mode");
}

void simNewUser(const String& id, float durationS) {
  if (sState != SIM_IDLE) {
    Serial.printf("[SIM] busy with user %s, ignoring new user %s\n",
                  sUserId.c_str(), id.c_str());
    return;
  }
  sUserId     = id;
  sDurationMs = (uint32_t)(durationS * 1000.0f);
  servoMoveAnimated(SERVO_MAX_DEG);
  sState = SIM_MOVING_MAX;
  Serial.printf("[SIM] new user id=%s duration=%.1fs\n", id.c_str(), durationS);
}

void simPause() {
  if (sPaused || sState == SIM_IDLE) return;
  sPaused     = true;
  sSavedState = sState;

  if (sState == SIM_HOLDING) {
    uint32_t elapsed = sHoldTimer.elapsedMs();
    sRemainingHoldMs = (elapsed >= sDurationMs) ? 0 : (sDurationMs - elapsed);
  }

  sServoWasMoving = servoIsMoving();
  if (sServoWasMoving) servoPause();

  Serial.println("[SIM] paused");
}

void simPlay() {
  if (!sPaused) return;
  sPaused = false;

  if (sSavedState == SIM_HOLDING) {
    sDurationMs = sRemainingHoldMs;
    sHoldTimer.start();
  }

  if (sServoWasMoving) servoResume();

  Serial.println("[SIM] resumed");
}

void simModeTick() {
  if (sPaused) return;

  switch (sState) {

    case SIM_IDLE:
      break;

    // Wait for animated servo move to finish, then turn LED RED.
    case SIM_MOVING_MAX:
      if (!servoIsMoving()) {
        ledSetRed();
        sHoldTimer.start();
        sState = SIM_HOLDING;
        Serial.printf("[SIM] servo at MAX, holding for %lu ms\n",
                      (unsigned long)sDurationMs);
      }
      break;

    // Hold at MAX for the requested duration, then snap to REST immediately.
    case SIM_HOLDING:
      if (sHoldTimer.expired(sDurationMs)) {
        servoWriteImmediate(SERVO_REST_DEG);
        ledSetGreen();
        sendSimComplete(sUserId, true);
        Serial.printf("[SIM] cycle complete for user %s\n", sUserId.c_str());
        sState      = SIM_IDLE;
        sUserId     = "";
        sDurationMs = 0;
      }
      break;
  }
}
