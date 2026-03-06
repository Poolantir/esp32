/**
 * @file poolantir.cpp (main code)
 * @brief Poolantir ESP32 sensing module - Iteration 1
 *
 * Detects user in "pissing range" via ToF sensor, drives status LEDs (in-use + WiFi/Bluetooth),
 * and controls a servo to simulate pissing (move in, pause, move out).
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

/* ---------------------------------------------------------------------------/
 * Pin assignments (from /peripherals directory)
 * --------------------------------------------------------------------------*/
#define PIN_LED_IN_USE    4   /** In-use indicator LED */
#define PIN_LED_WIFI      16  /** WiFi transmit indicator LED */
#define PIN_LED_BLUETOOTH 17  /** Bluetooth transmit indicator LED */
#define PIN_SERVO         13  /** Servo signal (SG90 brown wire) */
#define TOF_SDA           22  /** ToF I2C data */
#define TOF_SCL           21  /** ToF I2C clock */

/** Distance threshold (mm): obstruction within this range = "user in range" */
#define PISSING_RANGE_MM  50

/** Servo angle when "in range" (simulating piss position). */
#define SERVO_ANGLE_IN_RANGE   90
/** Servo angle when "out of range" (rest position). */
#define SERVO_ANGLE_OUT_RANGE  0

/** Duration (ms) to remain in "pissing" position during simulation. */
#define SIMULATE_PISS_DURATION_MS  3000
/** Min delay (ms) before next simulate-piss cycle. */
#define WAIT_NEXT_USER_MIN_MS     3000
/** Max delay (ms) before next simulate-piss cycle. */
#define WAIT_NEXT_USER_MAX_MS     10000

/** true = WiFi mode, false = Bluetooth mode */
static bool transmit_mode_wifi = true;

/** ToF sensor object */
static VL53L0X tof;
/** true if ToF init succeeded; when false, isPissing() always returns false */
static bool tof_ok = false;

/** Servo PWM: 50 Hz, 16-bit. SG90 uses 1 ms (0°) to 2 ms (180°) pulse. */
static const uint32_t SERVO_FREQ_HZ   = 50;
static const uint32_t SERVO_RES_BITS  = 16;
static const uint32_t SERVO_MAX_DUTY  = (1U << SERVO_RES_BITS) - 1;

/** @brief Initialize GPIO for LEDs and set them off. */
static void initLeds(void);

/** @brief Initialize I2C and ToF sensor. */
static void initToF(void);

/** @brief Initialize servo PWM (LEDC) and set to rest position. */
static void initServo(void);

/**
 * @brief Set servo angle via LEDC PWM (0--180 degrees).
 * @param angle_deg Desired angle in degrees
 */
static void servoWrite(int angle_deg);

/**
 * @brief Update status LEDs based on user-in-range and transmit mode.
 *
 * In-use LED is on only when an object is within range (no dependency on servo).
 * Transmit LED (WiFi or Bluetooth) is on only when user is in range; when user
 * exits range, both in-use and transmit LEDs are turned off.
 * @param user_in_range true if ToF reports obstruction within PISSING_RANGE_MM
 */
void ToggleLED(bool user_in_range);

/**
 * @brief Check if there is an obstruction within pissing range.
 * @return true if distance <= PISSING_RANGE_MM, false otherwise
 */
bool isPissing(void);

/**
 * @brief Move servo to "in range" position (final state for simulation).
 */
static void moveInRange(void);

/**
 * @brief Hold "pissing" state for the configured duration (e.g. 3 s).
 */
static void simulatePissWait(void);

/**
 * @brief Move servo back to original/rest position.
 */
static void moveOutOfRange(void);

/**
 * @brief Run one full simulate-piss cycle: move in, wait, move out.
 */
void SimulatePiss(void);

/**
 * @brief Wait a random time between WAIT_NEXT_USER_MIN_MS and WAIT_NEXT_USER_MAX_MS
 *        before allowing the next simulate-piss cycle.
 */
void waitForNextUser(void);

/* ---------------------------------------------------------------------------/
 * Setup & loop
 * --------------------------------------------------------------------------*/

void setup() {
  Serial.begin(115200);
  initLeds();
  initToF();
  initServo();
  randomSeed(esp_random());
}

void loop() {
  bool user_in_range = isPissing();

  ToggleLED(user_in_range);

  if (user_in_range) {
    SimulatePiss();
    waitForNextUser();
  }
}

/* ---------------------------------------------------------------------------/
 * Initialization
 * --------------------------------------------------------------------------*/

static void initLeds(void) {
  pinMode(PIN_LED_IN_USE, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_LED_BLUETOOTH, OUTPUT);
  digitalWrite(PIN_LED_IN_USE, LOW);
  digitalWrite(PIN_LED_WIFI, LOW);
  digitalWrite(PIN_LED_BLUETOOTH, LOW);
}

static void initToF(void) {
  Wire.begin(TOF_SDA, TOF_SCL);
  if (!tof.init()) {
    Serial.println("[ERROR] ToF sensor not found; continuing without ToF (isPissing() = false)");
    tof_ok = false;
    return;
  }
  tof_ok = true;
  tof.setTimeout(500);
  tof.startContinuous();
}

static void servoWrite(int angle_deg) {
  angle_deg = (angle_deg < 0) ? 0 : (angle_deg > 180) ? 180 : angle_deg;
  /* SG90: 1 ms = 0°, 2 ms = 180°; period 20 ms */
  uint32_t duty = (uint32_t)((1.0f + (float)angle_deg / 180.0f) / 20.0f * (float)SERVO_MAX_DUTY);
  ledcWrite(PIN_SERVO, duty);
}

static void initServo(void) {
  ledcAttach(PIN_SERVO, SERVO_FREQ_HZ, SERVO_RES_BITS);
  servoWrite(SERVO_ANGLE_OUT_RANGE);
}

/* ---------------------------------------------------------------------------/
 * LED control
 * --------------------------------------------------------------------------*/

void ToggleLED(bool user_in_range) {
  digitalWrite(PIN_LED_IN_USE, user_in_range ? HIGH : LOW);

  if (user_in_range) {
    if (transmit_mode_wifi) {
      digitalWrite(PIN_LED_WIFI, HIGH);
      digitalWrite(PIN_LED_BLUETOOTH, LOW);
    } else {
      digitalWrite(PIN_LED_WIFI, LOW);
      digitalWrite(PIN_LED_BLUETOOTH, HIGH);
    }
  } else {
    digitalWrite(PIN_LED_WIFI, LOW);
    digitalWrite(PIN_LED_BLUETOOTH, LOW);
  }
}

/* ---------------------------------------------------------------------------/
 * ToF / range check
 * --------------------------------------------------------------------------*/

bool isPissing(void) {
  if (!tof_ok) {
    return false;
  }
  uint16_t mm = tof.readRangeContinuousMillimeters();
  if (tof.timeoutOccurred()) {
    return false;
  }
  return (mm > 0 && mm <= PISSING_RANGE_MM);
}

/* ---------------------------------------------------------------------------/
 * Simulate piss: servo in, wait, servo out
 * --------------------------------------------------------------------------*/

static void moveInRange(void) {
  servoWrite(SERVO_ANGLE_IN_RANGE);
}

static void simulatePissWait(void) {
  delay(SIMULATE_PISS_DURATION_MS);
}

static void moveOutOfRange(void) {
  servoWrite(SERVO_ANGLE_OUT_RANGE);
}

void SimulatePiss(void) {
  moveInRange();
  simulatePissWait();
  moveOutOfRange();
}

void waitForNextUser(void) {
  unsigned long ms = (unsigned long)random(WAIT_NEXT_USER_MIN_MS, WAIT_NEXT_USER_MAX_MS + 1);
  delay(ms);
}
