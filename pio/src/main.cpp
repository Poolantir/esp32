#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <cctype>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEUUID.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <deque>
#include <mutex>
#include "clock.h"

// check for node id, if nothing exists use "0"
// note that our nodes will be 1-6
#ifndef POOLANTIR_NODE_ID
#define POOLANTIR_NODE_ID "0"
#endif

///////////////////////////
//                       //
//    PIN ASSIGNMENTS    //
//                       //
///////////////////////////

#define PIN_SERVO  14

#define LED_R  25
#define LED_G  26
#define LED_B  27

#define TOF_SDA  21
#define TOF_SCL  22




///////////////////////////
//                       //
//       CONSTANTS       //
//                       //
///////////////////////////

#define PISSING_RANGE_MM  60
#define SERVO_REST_DEG       0
#define SERVO_MAX_DEG      180
#define SERVO_HOMING_MS   3000




/////////////////////
//                //
//    GLOBALS     //
//                //
/////////////////////
static VL53L0X sensor;
static Servo   servo;

static ClockTimer servoTimer;
static bool servoAtMax = false;

static String gBleNodeId;
static BLEServer* gBleServer = nullptr;
static BLECharacteristic* gPoolantirDataChar = nullptr;

static bool isBLEConnected = false;

static std::mutex gBleRxMutex;
static std::deque<String> gBleRxQueue;

// Minimal GATT so a central (e.g. Mac) can complete a connection; extend for real protocol.
static constexpr const char* kBleServiceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static constexpr const char* kBleCharUuid = "beb5483e-36e1-4688-b7f5-e073f246f7d4";

class PoolantirBleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* /*server*/) override {
    isBLEConnected = true;
    Serial.println("[BLE] Central connected");
  }

  void onDisconnect(BLEServer* server) override {
    isBLEConnected = false;
    Serial.println("[BLE] Central disconnected; advertising again");
    server->getAdvertising()->start();
  }
};

static PoolantirBleServerCallbacks gBleServerCallbacks;

static void writeServo(int degrees);
static void receiveBLE(const String& payload);
static void transmitBLE(const String& msg);
static void drainBleReceiveQueue();
static void applyBleServoPayload(const String& payload);
static bool stringAllAsciiDigits(const String& s);

class PoolantirBleWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    String v = ch->getValue();
    std::lock_guard<std::mutex> lock(gBleRxMutex);
    gBleRxQueue.push_back(v);
  }
};

static PoolantirBleWriteCallbacks gBleWriteCallbacks;


////////////////////////
//                    //
//    INITIALIZERS    //
//                    //
////////////////////////

/**
 * @brief obtain the persisted node id 
 */
static void persistNodeIdFromBuild() {
  Preferences prefs;
  if (!prefs.begin("poolantir", false)) {
    Serial.println("[BLE] Preferences (NVS) begin failed");
    gBleNodeId = String(POOLANTIR_NODE_ID);
    return;
  }
  prefs.putString("nid", String(POOLANTIR_NODE_ID));
  gBleNodeId = prefs.getString("nid", String(POOLANTIR_NODE_ID));
  prefs.end();
}

/**
 * @brief connect ToF
 */
static void initToF() {
  Wire.begin(TOF_SDA, TOF_SCL);
  Wire.setClock(100000);

  sensor.setTimeout(500);
  bool initialized = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    if (sensor.init()) {
      initialized = true;
      break;
    }
    delay(500);
  }
  if (!initialized) {
    Serial.println("[ERROR] Failed to initialize ToF sensor!");
    digitalWrite(LED_R, HIGH);
    while (1) {}
  }

  sensor.startContinuous();
}

/**
 * @brief connect servo
 */
static void initServo() {
  // Reserve PWM timers early so BLE/WiFi stack init does not steal them from ESP32Servo.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  int ch = servo.attach(PIN_SERVO, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  if (ch == 0) {
    Serial.println("[WARN] Servo attach failed; check PIN_SERVO / PWM availability.");
  }
  writeServo(SERVO_REST_DEG);
  servoAtMax = false;
}

/**
 * @brief connect LEDs
 */
static void initLeds() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

/**
 * @brief expose BLE server
 */
static void initBLE() {
  persistNodeIdFromBuild();
  String deviceName = String("poolantir-node-") + gBleNodeId;

  BLEDevice::init(deviceName);
  gBleServer = BLEDevice::createServer();
  gBleServer->setCallbacks(&gBleServerCallbacks);

  BLEService* svc = gBleServer->createService(kBleServiceUuid);
  BLECharacteristic* ch =
      svc->createCharacteristic(
        kBleCharUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
  ch->addDescriptor(new BLE2902());
  ch->setCallbacks(&gBleWriteCallbacks);
  ch->setValue("poolantir");
  gPoolantirDataChar = ch;
  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLEUUID(kBleServiceUuid));
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  adv->start();

  Serial.printf("[BLE] Advertising as \"%s\" (stored id \"%s\")\n", deviceName.c_str(),
                gBleNodeId.c_str());
}

static void blockingWaitForConnection() {
  Serial.println("[BLE] Waiting for connection...");
  while (!isBLEConnected) {
    delay(20);
  }
  Serial.println("[BLE] Connected; continuing...");
}



////////////////////////////////
//                            //
//     RUN-TIME FUNCTIONS     //
//                            //
////////////////////////////////

/**
 * @brief rotate servo to degree passed
 */
static void writeServo(int degrees) {
  if (!servo.attached()) {
    Serial.println("[Servo] write skipped (not attached)");
    return;
  }
  if (degrees < 0) {
    degrees = 0;
  }
  if (degrees > 180) {
    degrees = 180;
  }
  servo.write(degrees);
}

/**
 * @brief BLE text -> servo: send decimal 0-180, or "toggle" / "t" to flip end positions.
 */
static bool stringAllAsciiDigits(const String& s) {
  if (s.length() == 0) {
    return false;
  }
  for (unsigned i = 0; i < s.length(); i++) {
    if (!isdigit((unsigned char)s[i])) {
      return false;
    }
  }
  return true;
}

static void applyBleServoPayload(const String& raw) {
  String p = raw;
  p.trim();
  if (p.length() == 0) {
    return;
  }

  if (p.equalsIgnoreCase("toggle") || p.equalsIgnoreCase("t")) {
    if (servoAtMax) {
      writeServo(SERVO_REST_DEG);
      servoAtMax = false;
    } else {
      writeServo(SERVO_MAX_DEG);
      servoAtMax = true;
    }
    Serial.printf("[Servo] toggle -> %d deg\n", servoAtMax ? SERVO_MAX_DEG : SERVO_REST_DEG);
    return;
  }

  if (stringAllAsciiDigits(p)) {
    int deg = p.toInt();
    if (deg < 0) {
      deg = 0;
    }
    if (deg > 180) {
      deg = 180;
    }
    writeServo(deg);
    servoAtMax = (deg >= 90);
    Serial.printf("[Servo] BLE angle -> %d\n", deg);
    return;
  }

  Serial.println(
      "[Servo] no move (send digits 0-180, e.g. \"90\", or \"toggle\"). "
      "Plain text like \"hello\" is ignored for servo.");
}

/**
 * @brief log payload written by the central to the Poolantir characteristic (raw bytes).
 */
static void receiveBLE(const String& payload) {
  Serial.print("[BLE RX] ");
  for (size_t i = 0; i < payload.length(); i++) {
    Serial.write((uint8_t)payload[i]);
  }
  Serial.println();

  applyBleServoPayload(payload);

  String ack = String("node ") + gBleNodeId + " received";
  transmitBLE(ack);
}

/**
 * @brief push a message to the connected device
 */
static void transmitBLE(const String& msg) {
  if (gPoolantirDataChar == nullptr || !isBLEConnected) {
    return;
  }
  gPoolantirDataChar->setValue(msg);
  gPoolantirDataChar->notify();
  Serial.printf("[BLE TX] %s\n", msg.c_str());
}

/**
 * @brief Process queued central writes on the main loop (log + notify ack).
 */
static void drainBleReceiveQueue() {
  for (;;) {
    String payload;
    {
      std::lock_guard<std::mutex> lock(gBleRxMutex);
      if (gBleRxQueue.empty()) {
        break;
      }
      payload = gBleRxQueue.front();
      gBleRxQueue.pop_front();
    }
    receiveBLE(payload);
  }
}

/**
 * @brief determine whether user is in front of the toilet
 */
static bool isToiletInUse() {
  uint16_t mm = sensor.readRangeContinuousMillimeters();
  bool timeout = sensor.timeoutOccurred();
  return!timeout && mm > 0 && mm <= PISSING_RANGE_MM;
}



////////////////////
//                //
//     SETUP      //
//                //
////////////////////

/**
 * @brief initializes peripherals & starts BLE comms
 */
void setup() {
  Serial.begin(115200);   

  initLeds();
  initToF();
  initServo();

  // home servo
  digitalWrite(LED_R, HIGH);
  writeServo(SERVO_REST_DEG);
  delay(2000);
  digitalWrite(LED_R, LOW);

  // start BLE
  digitalWrite(LED_B, HIGH);
  initBLE();
  digitalWrite(LED_B, LOW);

  // connect to mac/pi
  if (!isBLEConnected) {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_R, LOW);
    blockingWaitForConnection();
    digitalWrite(LED_B, HIGH);
  }
}



///////////////////////////
//                       //
//    SIMULATION LOOP    //
//                       //
///////////////////////////

/**
 * @brief main loop
 *
 * BLE: while connected, drain writes queued from GATT onWrite — each payload is logged
 * and an ack is notified (`receiveBLE` → `transmitBLE`). Reconnect if the central drops.
 */
void loop() {
  if (!isBLEConnected) {
    blockingWaitForConnection();
  }

  drainBleReceiveQueue();

  delay(5);
}