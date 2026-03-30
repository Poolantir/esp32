/**
 * @file main.cpp
 * @brief Poolantir ESP32 sensing module — simulation & normal modes
 *
 * Simulation mode: receives usage requests (1=pee, 2=poo) over Bluetooth from
 * the Raspberry Pi, queues them FIFO, and drives a servo to move a figurine
 * into ToF sensor range for the appropriate duration. Occupancy transitions
 * are reported back to the Pi over Bluetooth.
 *
 * Normal mode: passively monitors the ToF sensor for real occupancy and
 * reports state changes back to the Pi over Bluetooth.
 *
 * Mode is toggled via Bluetooth commands from the Pi.
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
#include <queue>
#include "clock.h"
#include <vector>

/* ---------------------------------------------------------------------------
 * Pin assignments (see /peripherals directory)
 * --------------------------------------------------------------------------*/
#define PIN_LED_IN_USE    4
#define PIN_LED_WIFI      16
#define PIN_LED_BLUETOOTH 17
#define PIN_SERVO         14
#define TOF_SDA           21
#define TOF_SCL           22

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------*/
#define PISSING_RANGE_MM           60
#define SERVO_ANGLE_NOT_IN_USE     0
#define SERVO_ANGLE_IN_USE         90
#define SERVO_SETTLE_MS            2000   // time for servo to physically reach target
#define SETTLE_TIMEOUT_MS          5000   // fallback if ToF never confirms position
#define SAMPLE_PERIOD_MS           200
#define PEE_DURATION_MS            2000   // type 1:  20s assumed real / 10 = 2s simulated
#define POO_DURATION_MS            30000  // type 2: 300s assumed real / 10 = 30s simulated

/* ---------------------------------------------------------------------------
 * Globals
 * --------------------------------------------------------------------------*/
static VL53L0X         tof;
static Servo           servo;
static BluetoothSerial SerialBT;

static bool simulationMode = true;

// ToF sampling
static uint32_t lastSampleMs = 0;
static bool     inRange      = false;
static bool     prevInRange  = false;

// FIFO queue of usage requests: 1 = pee, 2 = poo
static std::queue<uint8_t> usageQueue;

// Simulation state machine
enum SimState {
  SIM_IDLE,
  SIM_MOVING_IN,
  SIM_IN_USE,
  SIM_MOVING_OUT,
};
static SimState  simState        = SIM_IDLE;
static uint8_t   currentUsageType = 0;
static ClockTimer simTimer;

// Normal mode edge detection
static ClockTimer normalUsageTimer;
static bool       normalUsageActive = false;

// Bluetooth receive buffer
static String btBuffer = "";

/* ---------------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------------*/
static void initLeds() {
  pinMode(PIN_LED_IN_USE, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_LED_BLUETOOTH, OUTPUT);

  // off to start
  digitalWrite(PIN_LED_IN_USE, LOW);
  digitalWrite(PIN_LED_WIFI, LOW);
  digitalWrite(PIN_LED_BLUETOOTH, LOW);
}

static void initToF() {
  Wire.begin(TOF_SDA, TOF_SCL);
  if (!tof.init()) {
    Serial.println("[ERROR] Failure initializing the ToF sensor");
    while (1) {}
  }
  tof.setTimeout(500);
  tof.startContinuous();
}

static void initServo() {
  servo.attach(PIN_SERVO);
  servo.write(SERVO_ANGLE_NOT_IN_USE);
}

static void initBluetooth() {
  SerialBT.begin("Poolantir_Node_1");
  Serial.println("[BT] Bluetooth started as 'Poolantir_Node_1'");
}

/* ---------------------------------------------------------------------------
 * Runtime helpers
 * --------------------------------------------------------------------------*/

/** Poll ToF at SAMPLE_PERIOD_MS intervals; updates inRange + prevInRange. */
static void updateRangeSample(uint32_t now) {
  // blocking until next sample is taken
  if (now - lastSampleMs < SAMPLE_PERIOD_MS) return;
  
  lastSampleMs = now;
  prevInRange = inRange;

  uint16_t mm = tof.readRangeContinuousMillimeters();
  if (tof.timeoutOccurred()) {
    Serial.println("[ToF] timeout");
    inRange = false;
    return;
  }

  // update the flag
  inRange = (mm > 0 && mm <= PISSING_RANGE_MM);
}

/** Read and dispatch Bluetooth commands line-by-line. */
static void processBtInput() {
  // codes reference:
  // -1: normal mode 
  //  0: simulation mode
  //  1: Pee
  //  2: Poo
  //  3: Queue of restroom uses

  while (SerialBT.available()) {
    int incoming = SerialBT.read();

    if (incoming == -1) {                  
      Serial.println("Switching to normal mode");

      // add queue to local schedule
    } else {
      Serial.println("SINGLE VALUE");
      // add single value to the local schedule
    }
  }
}

/** Drive status LEDs. */
static void updateLeds(bool occupied) {
  digitalWrite(PIN_LED_IN_USE, occupied ? HIGH : LOW);
  digitalWrite(PIN_LED_BLUETOOTH, occupied ? HIGH : LOW);
  digitalWrite(PIN_LED_WIFI, LOW);
}

/* ---------------------------------------------------------------------------
 * Simulation mode
 *
 * State machine:
 *   IDLE  -->  MOVING_IN  -->  IN_USE  -->  MOVING_OUT  -->  IDLE --> ... 
 *
 * 1. Dequeue next usage type from FIFO
 * 2. Move servo to bring figurine into ToF range (~50 mm)
 * 3. Wait for ToF to confirm "in-use", light the LED, start usage timer
 * 4. After PEE/POO duration, move servo back to rest
 * 5. Wait for ToF to confirm "not-in-use", report over BT to Pi
 * --------------------------------------------------------------------------*/
static void runSimulation(uint32_t now) {
  updateRangeSample(now);
  processBtInput();

  switch (simState) {

    case SIM_IDLE:
      if (usageQueue.empty()) return;
      currentUsageType = usageQueue.front();
      usageQueue.pop();
      Serial.printf("[Sim] Dequeued usage type %d\n", currentUsageType);
      servo.write(SERVO_ANGLE_IN_USE);
      simTimer.start();
      simState = SIM_MOVING_IN;
      break;

    case SIM_MOVING_IN:
      // Wait for servo to settle, then confirm figurine is in ToF range
      if (simTimer.expired(SERVO_SETTLE_MS) && inRange) {
        Serial.println("[Sim] Figurine in range — usage started");
        digitalWrite(PIN_LED_IN_USE, HIGH);                                           // TODO - decouple the sim and led in-use... led turn on only if in range. 
        simTimer.start();
        simState = SIM_IN_USE;
      } else if (simTimer.expired(SETTLE_TIMEOUT_MS)) {                               // TODO - not a fan of this
        // Fallback: proceed even if ToF doesn't confirm (sensor issue)
        Serial.println("[Sim] ToF confirm timeout — proceeding anyway");
        digitalWrite(PIN_LED_IN_USE, HIGH);
        simTimer.start();
        simState = SIM_IN_USE;
      }
      break;

    case SIM_IN_USE: {
      uint32_t duration = (currentUsageType == 1) ? PEE_DURATION_MS : POO_DURATION_MS;
      if (simTimer.expired(duration)) {
        Serial.printf("[Sim] Usage complete (%u ms), returning figurine\n", duration);
        servo.write(SERVO_ANGLE_NOT_IN_USE);
        simTimer.start();
        simState = SIM_MOVING_OUT;
      }
      break;
    }

    case SIM_MOVING_OUT:
      if (simTimer.expired(SERVO_SETTLE_MS) && !inRange) {
        Serial.println("[Sim] Figurine cleared — reporting to Pi");
        digitalWrite(PIN_LED_IN_USE, LOW);

        uint32_t duration = (currentUsageType == 1) ? PEE_DURATION_MS : POO_DURATION_MS;
        SerialBT.printf("USAGE:%d:%u:%lu\n", currentUsageType, duration, millis());

        digitalWrite(PIN_LED_BLUETOOTH, HIGH);                      // TODO - decouple this
        delay(50);
        digitalWrite(PIN_LED_BLUETOOTH, LOW);

        simState = SIM_IDLE;
      } else if (simTimer.expired(SETTLE_TIMEOUT_MS)) {                                         
        Serial.println("[Sim] ToF clear timeout — reporting anyway");
        digitalWrite(PIN_LED_IN_USE, LOW);

        uint32_t duration = (currentUsageType == 1) ? PEE_DURATION_MS : POO_DURATION_MS;
        SerialBT.printf("USAGE:%d:%u:%lu\n", currentUsageType, duration, millis());

        digitalWrite(PIN_LED_BLUETOOTH, HIGH);
        delay(50);
        digitalWrite(PIN_LED_BLUETOOTH, LOW);

        simState = SIM_IDLE;
      }
      break;
  }
}

static void endSimulation() {
  simState = SIM_IDLE;
  simTimer.stop();
  usageQueue.empty();
  prevInRange = false;    // this might cause errors
  inRange = false;
}

/* ---------------------------------------------------------------------------
 * Normal mode
 *
 * Passively monitors the ToF sensor for real occupancy and reports
 * rising / falling edges back to the Raspberry Pi over Bluetooth.
 * --------------------------------------------------------------------------*/
static void runNormalMode(uint32_t now) {
  updateRangeSample(now);
  updateLeds(inRange);

  // Rising edge: someone sat down
  if (inRange && !prevInRange) {
    normalUsageTimer.start();
    normalUsageActive = true;
    SerialBT.print("USER DETECTED\n");
  }

  // Falling edge: person left
  if (!inRange && prevInRange && normalUsageActive) {
    uint32_t elapsed = normalUsageTimer.elapsedMs();
    normalUsageTimer.stop();
    normalUsageActive = false;
    SerialBT.printf("USER FINISHED (elapsed time: %u)\n", elapsed);
  }
}

static void endNormalMode() {
  normalUsageTimer.stop();
  normalUsageActive = false;
  prevInRange = false;    // this might cause errors
  inRange = false;
}

/* ---------------------------------------------------------------------------
 * Setup & Loop
 * --------------------------------------------------------------------------*/
void setup() {
  Serial.begin(115200);
  initLeds();
  initToF();
  initServo();
  // initBluetooth();     
  Serial.println("[Poolantir] Setup complete");
}

void loop() {
  uint32_t now = millis();

  processBtInput();

  if (simulationMode) {
    Serial.println("[Poolantir] STARTING SIMULATION")
    runSimulation(now);
  } else {
    Serial.println("[Poolantir] STARTING NORMAL MODE")
    runNormalMode(now);
  }
}
