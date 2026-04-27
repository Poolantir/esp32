#include <Arduino.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <deque>
#include <mutex>
#include "clock.h"

#ifndef POOLANTIR_NODE_ID
#define POOLANTIR_NODE_ID "0"
#endif

#define PIN_SERVO  14
#define LED_R  25
#define LED_G  26
#define LED_B  27

#define SERVO_REST_DEG      0
#define SERVO_MAX_DEG     180

#define SIM_PEE_HOLD_MS   2000
#define SIM_POO_HOLD_MS   4000
#define SIM_REST_MS        3000

#define LED_FLASH_MS        500


/////////////////////
//     GLOBALS     //
/////////////////////

static Servo      servo;

static String gServiceUuid;
static String gCharUuid;

static BLECharacteristic* gBleChar      = nullptr;
static bool               gBleConnected = false;
static std::mutex         gRxMutex;
static std::deque<String> gRxQueue;

// Simulation queue and state machine
static std::deque<int>    gSimQueue;
static std::mutex         gSimMutex;

enum SimState {
  SIM_IDLE,
  SIM_HOLDING,
  SIM_RETURNING,
  SIM_RESTING,
};
static SimState   gSimState      = SIM_IDLE;
static ClockTimer gSimTimer;
static int        gSimCurrentType = 0;  // 1=pee, 2=poo

static void writeServo(int degrees);
static void handleCommand(const String& raw);
static void sendBleMessage(const String& msg);
static String tsPrefix();


/////////////////////////
//    BLE CALLBACKS    //
/////////////////////////

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    gBleConnected = true;
    Serial.println("[BLE] connected");
  }
  void onDisconnect(BLEServer* s) override {
    gBleConnected = false;
    Serial.println("[BLE] disconnected, re-advertising");
    s->getAdvertising()->start();
  }
};

class WriteCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    std::lock_guard<std::mutex> lock(gRxMutex);
    gRxQueue.push_back(ch->getValue());
  }
};

static ServerCB sServerCB;
static WriteCB  sWriteCB;


////////////////////////
//    INITIALIZERS    //
////////////////////////

static void initServo() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  if (servo.attach(PIN_SERVO, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH) == 0) {
    Serial.println("[WARN] Servo attach failed");
  }
  writeServo(SERVO_REST_DEG);
}

static void initLeds() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

static void initBleConnection() {
  gServiceUuid = String("4fafc201-1fb5-459e-8fcc-c5c9c33191a") + POOLANTIR_NODE_ID;
  gCharUuid    = String("beb5483e-36e1-4688-b7f5-e073f246f7b") + POOLANTIR_NODE_ID;

  String name = String("poolantir-node-") + POOLANTIR_NODE_ID;
  BLEDevice::init(name);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&sServerCB);

  BLEService* svc = server->createService(gServiceUuid.c_str());
  gBleChar = svc->createCharacteristic(
    gCharUuid.c_str(),
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  gBleChar->addDescriptor(new BLE2902());
  gBleChar->setCallbacks(&sWriteCB);
  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(gServiceUuid.c_str());
  adv->setScanResponse(true);
  adv->start();

  Serial.printf("[BLE] advertising as \"%s\" svc=%s char=%s\n",
                name.c_str(), gServiceUuid.c_str(), gCharUuid.c_str());
}


////////////////////////////
//    UTILITY FUNCTIONS   //
////////////////////////////

static String tsPrefix() {
  unsigned long ms = millis();
  unsigned long s  = ms / 1000;
  unsigned long m  = s  / 60;
  unsigned long h  = m  / 60;
  char buf[32];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu:%02lu.%03lu]",
           h, m % 60, s % 60, ms % 1000);
  return String(buf);
}

static void writeServo(int degrees) {
  if (!servo.attached()) return;
  servo.write(constrain(degrees, 0, 180));
}

static void sendBleMessage(const String& msg) {
  if (!gBleChar || !gBleConnected) return;
  gBleChar->setValue(msg);
  gBleChar->notify();
  Serial.printf("[BLE TX] %s\n", msg.c_str());
}

static void logAndNotify(const String& msg) {
  String full = tsPrefix() + " " + msg;
  Serial.println(full);
  sendBleMessage(full);
}


////////////////////////////
//    COMMAND DISPATCH    //
////////////////////////////

static String extractArg(const String& raw, int afterPrefix) {
  String arg = raw.substring(afterPrefix);
  arg.trim();
  return arg;
}

static void cmdEcho(const String& arg) {
  logAndNotify("ECHO " + arg);
}

static void cmdServo(const String& arg) {
  String a = arg;
  a.toUpperCase();

  int deg;
  if (a == "REST") {
    deg = SERVO_REST_DEG;
  } else if (a == "MAX") {
    deg = SERVO_MAX_DEG;
  } else {
    deg = a.toInt();
    if (deg < 0 || deg > 180) {
      logAndNotify("ERR SERVO degree must be 0-180");
      return;
    }
  }

  writeServo(deg);
  logAndNotify(String("SERVO moved to ") + deg + " degrees");
}

static void cmdLed(const String& arg) {
  String a = arg;
  a.toUpperCase();

  int pin;
  String color;
  if (a == "R") {
    pin = LED_R; color = "RED";
  } else if (a == "G") {
    pin = LED_G; color = "GREEN";
  } else if (a == "B") {
    pin = LED_B; color = "BLUE";
  } else {
    logAndNotify("ERR LED color must be R, G, or B");
    return;
  }

  logAndNotify("LED flashing " + color);
  digitalWrite(pin, HIGH);
  delay(LED_FLASH_MS);
  digitalWrite(pin, LOW);
  logAndNotify("LED " + color + " off");
}

static void cmdSim(const String& arg) {
  // Parse: "count {v1,v2,...}" or just "{v1,v2,...}"
  String s = arg;
  s.trim();

  int openBrace  = s.indexOf('{');
  int closeBrace = s.indexOf('}', openBrace + 1);
  if (openBrace < 0 || closeBrace < 0) {
    logAndNotify("ERR SIM format: SIM count {1,2,...}");
    return;
  }

  String inner = s.substring(openBrace + 1, closeBrace);
  inner.replace("[", "");
  inner.replace("]", "");

  std::deque<int> elems;
  while (inner.length() > 0) {
    int comma = inner.indexOf(',');
    String token = (comma >= 0) ? inner.substring(0, comma) : inner;
    inner = (comma >= 0) ? inner.substring(comma + 1) : String();
    token.trim();
    if (token.length() == 0) continue;
    int val = token.toInt();
    if (val != 1 && val != 2) {
      logAndNotify(String("ERR SIM invalid element ") + val + " (must be 1=pee or 2=poo)");
      return;
    }
    elems.push_back(val);
  }

  if (elems.empty()) {
    logAndNotify("ERR SIM empty element list");
    return;
  }

  {
    std::lock_guard<std::mutex> lock(gSimMutex);
    for (int v : elems) gSimQueue.push_back(v);
  }

  logAndNotify(String("SIM enqueued ") + (int)elems.size() + " elements, queue size now " + (int)gSimQueue.size());
}

static void handleCommand(const String& raw) {
  Serial.printf("[BLE RX] %s\n", raw.c_str());

  String s = raw;
  s.trim();
  if (s.length() == 0) return;

  String upper = s;
  upper.toUpperCase();

  if (upper.startsWith("ECHO ")) {
    cmdEcho(extractArg(s, 5));
  } else if (upper.startsWith("SERVO ")) {
    cmdServo(extractArg(s, 6));
  } else if (upper.startsWith("LED ")) {
    cmdLed(extractArg(s, 4));
  } else if (upper.startsWith("SIM ")) {
    cmdSim(extractArg(s, 4));
  } else {
    logAndNotify(String("ERR unknown command: ") + s);
  }
}


//////////////////////////////////
//    SIMULATION STATE MACHINE  //
//////////////////////////////////

static void simTick() {
  switch (gSimState) {

    case SIM_IDLE: {
      int elem = 0;
      {
        std::lock_guard<std::mutex> lock(gSimMutex);
        if (gSimQueue.empty()) return;
        elem = gSimQueue.front();
        gSimQueue.pop_front();
      }

      gSimCurrentType = elem;
      const char* label = (elem == 1) ? "pee" : "poo";
      uint32_t holdMs   = (elem == 1) ? SIM_PEE_HOLD_MS : SIM_POO_HOLD_MS;

      writeServo(SERVO_MAX_DEG);
      gSimState = SIM_HOLDING;
      gSimTimer.start();

      int remaining;
      {
        std::lock_guard<std::mutex> lock(gSimMutex);
        remaining = (int)gSimQueue.size();
      }
      logAndNotify(String("consuming ") + label + ", moving servo into position (" + holdMs/1000 + "s hold, " + remaining + " elems remaining)");
      break;
    }

    case SIM_HOLDING: {
      uint32_t holdMs = (gSimCurrentType == 1) ? SIM_PEE_HOLD_MS : SIM_POO_HOLD_MS;
      if (!gSimTimer.expired(holdMs)) return;

      const char* label = (gSimCurrentType == 1) ? "pee" : "poo";
      writeServo(SERVO_REST_DEG);
      gSimState = SIM_RETURNING;
      gSimTimer.start();

      logAndNotify(String(label) + " complete, returning servo");
      break;
    }

    case SIM_RETURNING: {
      if (!gSimTimer.expired(500)) return;

      bool moreElems;
      int remaining;
      {
        std::lock_guard<std::mutex> lock(gSimMutex);
        moreElems = !gSimQueue.empty();
        remaining = (int)gSimQueue.size();
      }

      if (moreElems) {
        gSimState = SIM_RESTING;
        gSimTimer.start();
        logAndNotify(String("waiting 3s to consume next elem (elems remaining: ") + remaining + ")");
      } else {
        gSimState = SIM_IDLE;
        logAndNotify("simulation queue empty, returning to idle");
      }
      break;
    }

    case SIM_RESTING: {
      if (!gSimTimer.expired(SIM_REST_MS)) return;
      gSimState = SIM_IDLE;
      break;
    }
  }
}


/////////////////
//    SETUP    //
/////////////////

void setup() {
  Serial.begin(115200);
  initLeds();
  initServo();

  digitalWrite(LED_R, HIGH);
  writeServo(SERVO_REST_DEG);
  delay(2000);
  digitalWrite(LED_R, LOW);

  initBleConnection();
  Serial.println("[APP] dummy terminal ready — commands: ECHO, SERVO, LED, SIM");
}


////////////////
//    LOOP    //
////////////////

void loop() {
  // Drain BLE RX queue
  for (;;) {
    String payload;
    {
      std::lock_guard<std::mutex> lock(gRxMutex);
      if (gRxQueue.empty()) break;
      payload = gRxQueue.front();
      gRxQueue.pop_front();
    }
    handleCommand(payload);
  }

  // Advance simulation state machine (non-blocking)
  simTick();

  delay(5);
}
