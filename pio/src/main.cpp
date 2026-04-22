#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
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
#define TOF_SDA  21
#define TOF_SCL  22

#define PISSING_RANGE_MM   60
#define SERVO_REST_DEG      0
#define SERVO_MAX_DEG     180
#define SERVO_HOMING_MS  3000


/////////////////////
//     GLOBALS     //
/////////////////////

static VL53L0X    sensor;
static Servo      servo;
static ClockTimer servoTimer;
static bool       servoAtMax = false;

// Per-node UUIDs built from POOLANTIR_NODE_ID (valid hex: a1-a6 / b1-b6)
//   service: 4fafc201-1fb5-459e-8fcc-c5c9c33191a<N>
//   char:    beb5483e-36e1-4688-b7f5-e073f246f7b<N>
static String gServiceUuid;
static String gCharUuid;

static BLECharacteristic* gBleChar      = nullptr;
static bool               gBleConnected = false;
static std::mutex         gRxMutex;
static std::deque<String> gRxQueue;

static void writeServo(int degrees);
static void handleParsedArray(const int* values, int count);


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

static void initToF() {
  Wire.begin(TOF_SDA, TOF_SCL);
  Wire.setClock(100000);
  sensor.setTimeout(500);
  bool ok = false;
  for (int i = 0; i < 5; i++) {
    if (sensor.init()) { ok = true; break; }
    delay(500);
  }
  if (!ok) {
    Serial.println("[ERROR] ToF sensor init failed");
    digitalWrite(LED_R, HIGH);
    while (1) {}
  }
  sensor.startContinuous();
}

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
  servoAtMax = false;
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
//    RUNTIME FUNCTIONS   //
////////////////////////////

static void writeServo(int degrees) {
  if (!servo.attached()) return;
  servo.write(constrain(degrees, 0, 180));
}

static bool isToiletInUse() {
  uint16_t mm = sensor.readRangeContinuousMillimeters();
  return !sensor.timeoutOccurred() && mm > 0 && mm <= PISSING_RANGE_MM;
}

static int receiveArrayPayload(const String& raw, int* out, int maxOut) {
  Serial.printf("[BLE RX] %s\n", raw.c_str());

  String s = raw;
  s.trim();
  if (s.length() == 0) return 0;

  int spaceIdx = s.indexOf(' ');
  if (spaceIdx < 0) {
    Serial.println("[BLE RX] parse error: missing space between count and body");
    return 0;
  }

  int count = s.substring(0, spaceIdx).toInt();
  if (count <= 0) {
    Serial.println("[BLE RX] parse error: count must be > 0");
    return 0;
  }

  int openBrace  = s.indexOf('{', spaceIdx);
  int closeBrace = s.indexOf('}', openBrace + 1);
  if (openBrace < 0 || closeBrace < 0) {
    Serial.println("[BLE RX] parse error: missing { }");
    return 0;
  }

  String inner = s.substring(openBrace + 1, closeBrace);
  inner.replace("[", "");
  inner.replace("]", "");

  int parsed = 0;
  while (inner.length() > 0 && parsed < maxOut) {
    int comma = inner.indexOf(',');
    String token = (comma >= 0) ? inner.substring(0, comma) : inner;
    inner = (comma >= 0) ? inner.substring(comma + 1) : String();
    token.trim();
    if (token.length() == 0) continue;
    out[parsed++] = token.toInt();
  }

  if (parsed != count) {
    Serial.printf("[BLE RX] warning: declared %d values but parsed %d\n", count, parsed);
  }
  return parsed;
}

static void sendBleMessage(const String& msg) {
  if (!gBleChar || !gBleConnected) return;
  gBleChar->setValue(msg);
  gBleChar->notify();
  Serial.printf("[BLE TX] %s\n", msg.c_str());
}

static void handleParsedArray(const int* values, int count) {
  for (int i = 0; i < count; i++) {
    Serial.printf("[APP] value[%d] = %d\n", i, values[i]);
  }
  sendBleMessage(String("node ") + POOLANTIR_NODE_ID + " ack " + count);
}


/////////////////
//    SETUP    //
/////////////////

void setup() {
  Serial.begin(115200);
  initLeds();
  initToF();
  initServo();

  digitalWrite(LED_R, HIGH);
  writeServo(SERVO_REST_DEG);
  delay(2000);
  digitalWrite(LED_R, LOW);

  initBleConnection();
}


////////////////
//    LOOP    //
////////////////

void loop() {
  for (;;) {
    String payload;
    {
      std::lock_guard<std::mutex> lock(gRxMutex);
      if (gRxQueue.empty()) break;
      payload = gRxQueue.front();
      gRxQueue.pop_front();
    }
    int values[8];
    int n = receiveArrayPayload(payload, values, 8);
    if (n > 0) handleParsedArray(values, n);
  }

  delay(5);
}
