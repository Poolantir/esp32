// AI-ASSISTED
// Matt Krueger
// April 2026

#include "config.h"
#include "servo.h"
#include "led.h"
#include "tof.h"
#include "ble.h"
#include "test_mode.h"
#include "sim_mode.h"
#include "clock.h"
#include <ArduinoJson.h>

static Mode gMode = MODE_TEST;

static const char* modeName(Mode m) {
  return (m == MODE_TEST) ? "TEST" : "SIM";
}

////////////////////////////
//       LOGGING          //
////////////////////////////

static void logIncoming(const String& raw) {
  Serial.println();
  Serial.println("Incoming:");
  Serial.print('"');
  Serial.print(raw);
  Serial.println('"');
}

static void logParsed(const String& summary) {
  Serial.print("Parsed: ");
  Serial.println(summary);
  Serial.printf("Status: node=%s connected=%c mode=%s\n",
                POOLANTIR_NODE_ID, bleIsConnected() ? 'T' : 'F', modeName(gMode));
  Serial.println();
}

////////////////////////////
//      TX HELPERS        //
////////////////////////////

static void sendModeAck(const String& mode, bool ok, const String& error = "") {
  JsonDocument doc;
  doc["command"] = "MODE";
  doc["type"]    = "ACK";
  JsonObject action = doc["action"].to<JsonObject>();
  action["mode"] = mode;
  action["ok"]   = ok;
  if (error.length()) action["error"] = error;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

static void sendEchoAck(const String& message) {
  JsonDocument doc;
  doc["command"] = "ECHO";
  doc["type"]    = "MESSAGE";
  doc["action"]  = message;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

static void sendFlashAck(const String& type, int value) {
  JsonDocument doc;
  doc["command"] = "FLASH";
  doc["type"]    = type;
  doc["action"]  = value;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

static void sendGetResponse(const String& type, int value) {
  JsonDocument doc;
  doc["command"] = "GET";
  doc["id"]      = "";
  doc["type"]    = type;
  doc["action"]  = value;
  String msg;
  serializeJson(doc, msg);
  bleSendMessage(msg);
}

////////////////////////////
//    MODE MANAGEMENT     //
////////////////////////////

static void setMode(const String& action) {
  String v = action;
  v.toUpperCase();

  Mode next;
  if      (v == "TEST") next = MODE_TEST;
  else if (v == "SIM")  next = MODE_SIM;
  else {
    Serial.printf("[MODE] invalid: \"%s\"\n", action.c_str());
    sendModeAck(v, false, "invalid_mode");
    return;
  }

  gMode = next;
  if (next == MODE_TEST) enterTestMode();
  else                   enterSimMode();

  sendModeAck(v, true);
  Serial.printf("[MODE] now %s\n", modeName(gMode));
}

////////////////////////////
//   COMMAND DISPATCH     //
////////////////////////////

static void handleCommand(const String& raw) {
  logIncoming(raw);

  if (raw.length() == 0) {
    logParsed("<empty>");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, raw);
  if (err) {
    logParsed(String("<invalid json: ") + err.c_str() + ">");
    return;
  }

  String command = doc["command"] | "";
  String type    = doc["type"]    | "";
  command.trim(); command.toUpperCase();
  type.trim();    type.toUpperCase();

  if (!command.length()) {
    logParsed("<missing command>");
    return;
  }

  // ---- MODE: always accepted ----
  if (command == "MODE") {
    String action = doc["action"] | "";
    if (!action.length()) { logParsed("MODE <missing action>"); return; }
    logParsed(String("MODE SET ") + action);
    setMode(action);
    return;
  }

  if (!type.length()) {
    logParsed("<missing type>");
    return;
  }

  // ---- ECHO: always accepted ----
  if (command == "ECHO") {
    String message = doc["action"] | "";
    logParsed(String("ECHO MESSAGE ") + message);
    if (type != "MESSAGE") { logParsed("ECHO invalid type (expected MESSAGE)"); return; }
    sendEchoAck(message);
    return;
  }

  // ---- GET: always accepted (query current parameter values) ----
  if (command == "GET") {
    logParsed(String("GET ") + type);

    if (type == "IN_RANGE") {
      sendGetResponse("IN_RANGE", tofGetInRangeMm());
    } else {
      logParsed("GET unknown type (expected IN_RANGE)");
    }
    return;
  }

  // ---- FLASH: always accepted (runtime-tunable parameters) ----
  if (command == "FLASH") {
    if (doc["action"].isNull()) { logParsed("FLASH <missing action>"); return; }
    int value = doc["action"].as<int>();
    logParsed(String("FLASH ") + type + " " + String(value));

    if (type == "IN_RANGE") {
      tofSetInRangeMm(value);
      sendFlashAck("IN_RANGE", tofGetInRangeMm());
    } else {
      logParsed("FLASH unknown type (expected IN_RANGE)");
    }
    return;
  }

  // ---- TEST: only in MODE_TEST ----
  if (command == "TEST") {
    String action = doc["action"] | "";
    action.toUpperCase();
    String summary = String("TEST ") + type + " " + action;

    if (gMode != MODE_TEST) {
      logParsed(summary + " (ignored, mode=" + modeName(gMode) + ")");
      return;
    }
    logParsed(summary);

    if (type == "LED") {
      testLedCmd(action);
    } else if (type == "SERVO") {
      testServoCmd(action);
    } else if (type == "QUEUE") {
      if (action == "RUN") testQueueCmd();
      else logParsed("TEST QUEUE invalid action (expected RUN)");
    } else {
      logParsed(summary + " (unknown type)");
    }
    return;
  }

  // ---- SIM ----
  if (command == "SIM") {
    String id = doc["id"] | "";
    String summary = String("SIM ") + type + " id=" + id;
    logParsed(summary);

    if (type == "NEW") {
      if (gMode != MODE_SIM) {
        sendSimAck(id, false, "wrong_mode");
        return;
      }
      float duration = 0;
      if (!doc["action"].isNull()) {
        if (doc["action"].is<JsonObject>())
          duration = doc["action"]["duration_s"].as<float>();
        else
          duration = doc["action"].as<float>();
      }
      simNewUser(id, duration);
    } else if (type == "CONTROL") {
      if (gMode != MODE_SIM) {
        logParsed(summary + " (ignored, mode=" + modeName(gMode) + ")");
        return;
      }
      String action = doc["action"] | "";
      action.toUpperCase();
      if (action == "PAUSE")     simPause();
      else if (action == "PLAY") simPlay();
      else logParsed("SIM CONTROL invalid action (expected PAUSE or PLAY)");
    } else {
      logParsed(summary + " (unknown type)");
    }
    return;
  }

  logParsed(String("<unknown command: \"") + command + "\">");
}

////////////////////////////
//  BLE CONNECTION HOOK   //
////////////////////////////

static void onBleConnection(bool connected) {
  if (connected) {
    if (gMode == MODE_TEST) ledSetBlue();
    else                    ledSetGreen();
  } else {
    ledAllOff();
  }
}

/////////////////
//    SETUP    //
/////////////////

void setup() {
  Serial.begin(115200);
  ledInit();
  tofInit();
  servoInit();
  delay(500);

  bleSetConnectionCallback(onBleConnection);
  enterTestMode();
  bleInit();

  Serial.printf("[ESP32] ready, Node %s mode=%s\n",
                POOLANTIR_NODE_ID, modeName(gMode));
}

////////////////
//    LOOP    //
////////////////

void loop() {
  while (bleHasRxMessage()) {
    handleCommand(blePopRxMessage());
  }

  if (gMode == MODE_TEST) testModeTick();
  else                     simModeTick();

  bleEnsureAdvertising();

  delay(5);
}
