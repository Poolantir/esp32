// AI-ASSISTED
// Matt Krueger
// April 2026

#include "sim_mode.h"
#include "config.h"
#include "servo.h"
#include "led.h"
#include "tof.h"
#include "ble.h"
#include "clock.h"
#include <ArduinoJson.h>

enum SimState { SIM_IDLE, SIM_HOLDING };

static SimState sState     = SIM_IDLE;
static String   sUserId;
static uint32_t sDurationMs = 0;
static ClockTimer sHoldTimer;

static bool     sPaused          = false;
static uint32_t sRemainingHoldMs = 0;

void sendSimAck(const String& id, bool ok, const String& error) {
  JsonDocument doc;
  doc["command"] = "SIM";
  doc["id"]      = id;
  doc["type"]    = "ACK";
  JsonObject action = doc["action"].to<JsonObject>();
  action["ok"] = ok;
  if (error.length()) action["error"] = error;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

static void sendSimComplete(const String& id, bool success) {
  JsonDocument doc;
  doc["command"] = "SIM";
  doc["id"]      = id;
  doc["type"]    = "COMPLETE";
  JsonObject action = doc["action"].to<JsonObject>();
  action["success"] = success;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

void enterSimMode() {
  servoWriteImmediate(SERVO_REST_DEG);
  if (bleIsConnected()) ledSetGreen();
  else                  ledAllOff();
  tofStopContinuous();
  sState      = SIM_IDLE;
  sUserId     = "";
  sDurationMs = 0;
  sPaused     = false;
  Serial.println("[SIM] entered SIM mode");
}

void simNewUser(const String& id, float durationS) {
  if (sState != SIM_IDLE) {
    Serial.printf("[SIM] busy with user %s, rejecting %s\n",
                  sUserId.c_str(), id.c_str());
    sendSimAck(id, false, "busy");
    return;
  }
  if (durationS <= 0) {
    Serial.printf("[SIM] invalid duration %.1f for user %s\n", durationS, id.c_str());
    sendSimAck(id, false, "invalid_duration");
    return;
  }

  sendSimAck(id, true);

  sUserId     = id;
  sDurationMs = (uint32_t)(durationS * 1000.0f);
  servoWriteImmediate(SERVO_MAX_DEG);
  ledSetRed();
  sHoldTimer.start();
  sState = SIM_HOLDING;
  Serial.printf("[SIM] new user id=%s duration=%.1fs\n", id.c_str(), durationS);
}

void simPause() {
  if (sPaused || sState == SIM_IDLE) return;
  sPaused = true;

  if (sState == SIM_HOLDING) {
    uint32_t elapsed = sHoldTimer.elapsedMs();
    sRemainingHoldMs = (elapsed >= sDurationMs) ? 0 : (sDurationMs - elapsed);
  }

  Serial.println("[SIM] paused");
}

void simPlay() {
  if (!sPaused) return;
  sPaused = false;

  if (sState == SIM_HOLDING) {
    sDurationMs = sRemainingHoldMs;
    sHoldTimer.start();
  }

  Serial.println("[SIM] resumed");
}

void simModeTick() {
  if (sPaused) return;

  switch (sState) {

    case SIM_IDLE:
      break;

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
