// AI-ASSISTED
// Matt Krueger
// April 2026

#include "ble.h"
#include "config.h"
#include "clock.h"
#include <BLEDevice.h>
#include <BLE2902.h>
#include <mutex>
#include <deque>

static String sServiceUuid;
static String sCharUuid;
static BLECharacteristic* sChar = nullptr;
static bool sConnected = false;
static std::mutex sRxMutex;
static std::deque<String> sRxQueue;
static String sRxAssembleBuffer;
static ClockTimer sAdvRetryTimer;
static bool sAdvRetryArmed = false;

static BleConnectionCB sConnectionCB;
static void appendRxChunk(const String& chunk);
static void restartAdvertising();

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    sConnected = true;
    sAdvRetryArmed = false;
    Serial.println("[BLE] connected");
    if (sConnectionCB) sConnectionCB(true);
  }
  void onDisconnect(BLEServer* s) override {
    sConnected = false;
    Serial.println("[BLE] disconnected, restarting advertising");
    if (s) s->getAdvertising()->start();
    BLEDevice::startAdvertising();
    sAdvRetryArmed = true;
    sAdvRetryTimer.start();
    if (sConnectionCB) sConnectionCB(false);
  }
};

class WriteCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    std::lock_guard<std::mutex> lock(sRxMutex);
    String raw = String(ch->getValue().c_str());
    Serial.printf("[BLE RX] %s\n", raw.c_str());
    appendRxChunk(raw);
  }
};

static ServerCB sServerCB;
static WriteCB  sWriteCB;

// Reassemble fragmented JSON objects from BLE MTU-chunked writes.
static void appendRxChunk(const String& chunk) {
  if (!chunk.length()) return;

  sRxAssembleBuffer += chunk;

  int start = -1;
  int depth = 0;
  bool inString = false;
  bool escape = false;

  for (int i = 0; i < (int)sRxAssembleBuffer.length(); i++) {
    char c = sRxAssembleBuffer[i];

    if (start < 0) {
      if (c == '{') { start = i; depth = 1; inString = false; escape = false; }
      continue;
    }

    if (inString) {
      if (escape)       escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"')  inString = false;
      continue;
    }

    if      (c == '"') inString = true;
    else if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) {
        sRxQueue.push_back(sRxAssembleBuffer.substring(start, i + 1));
        start = -1;
      }
    }
  }

  if (start < 0)       sRxAssembleBuffer = "";
  else if (start > 0)  sRxAssembleBuffer = sRxAssembleBuffer.substring(start);
}

static void restartAdvertising() {
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  if (adv) adv->start();
  BLEDevice::startAdvertising();
  Serial.println("[BLE] advertising restart requested");
}

void bleInit() {
  sServiceUuid = String("4fafc201-1fb5-459e-8fcc-c5c9c33191a") + POOLANTIR_NODE_ID;
  sCharUuid    = String("beb5483e-36e1-4688-b7f5-e073f246f7b") + POOLANTIR_NODE_ID;

  String name = String("poolantir-node-") + POOLANTIR_NODE_ID;
  BLEDevice::init(name);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&sServerCB);

  BLEService* svc = server->createService(sServiceUuid.c_str());
  sChar = svc->createCharacteristic(
    sCharUuid.c_str(),
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  sChar->addDescriptor(new BLE2902());
  sChar->setCallbacks(&sWriteCB);
  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(sServiceUuid.c_str());
  adv->setScanResponse(true);
  adv->start();

  Serial.printf("[BLE] advertising as \"%s\" svc=%s char=%s\n",
                name.c_str(), sServiceUuid.c_str(), sCharUuid.c_str());
}

void bleSendMessage(const String& msg) {
  if (!sChar || !sConnected) {
    Serial.printf("[BLE TX skipped, not connected] %s\n", msg.c_str());
    return;
  }
  sChar->setValue(msg);
  sChar->notify();
  Serial.printf("[BLE TX] %s\n", msg.c_str());
}

void bleSetConnectionCallback(BleConnectionCB cb) { sConnectionCB = cb; }

bool bleIsConnected() { return sConnected; }

void bleEnsureAdvertising() {
  if (sConnected) return;

  if (!sAdvRetryArmed) {
    sAdvRetryArmed = true;
    sAdvRetryTimer.start();
    restartAdvertising();
    return;
  }

  if (sAdvRetryTimer.expired(2000)) {
    sAdvRetryTimer.start();
    restartAdvertising();
  }
}

bool bleHasRxMessage() {
  std::lock_guard<std::mutex> lock(sRxMutex);
  return !sRxQueue.empty();
}

String blePopRxMessage() {
  std::lock_guard<std::mutex> lock(sRxMutex);
  if (sRxQueue.empty()) return "";
  String msg = sRxQueue.front();
  sRxQueue.pop_front();
  return msg;
}
