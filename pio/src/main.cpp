#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <ESP32Servo.h>
#include "clock.h"

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

////////////////////////
//                    //
//    GLOBAL OBJS     //
//                    //
////////////////////////
static VL53L0X sensor;
static Servo   servo;

static ClockTimer servoTimer;
static bool servoAtMax = false;

////////////////////////
//                    //
//    INITIALIZERS    //
//                    //
////////////////////////

static void initToF() {
  Wire.begin(TOF_SDA, TOF_SCL);
  Wire.setClock(100000);

  // try to initialize ToF sensor (5 attempts)
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

  // start the continuous read of the ToF sensor
  sensor.startContinuous();
}

static void initServo() {
  servo.attach(PIN_SERVO);
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

////////////////////////////////
//                            //
//     RUN-TIME FUNCTIONS     //
//                            //
////////////////////////////////
static void writeServo(int degrees) {
  servo.write(degrees);
}

static void transmitBLE(int msg) {
  // nothing here yet...
}

static bool isToiletInUse() {
  uint16_t mm = sensor.readRangeContinuousMillimeters();
  bool timeout = sensor.timeoutOccurred();
  bool inRange = !timeout && mm > 0 && mm <= PISSING_RANGE_MM;
}



////////////////////
//                //
//     SETUP      //
//                //
////////////////////
void setup() {
  Serial.begin(115200);   

  initLeds();
  initToF();
  initServo();

  // starting sequence
  digitalWrite(LED_B, HIGH);
  writeServo(SERVO_REST_DEG);
  delay(2000);
  digitalWrite(LED_B, LOW);
}

///////////////////////////
//                       //
//    SIMULATION LOOP    //
//                       //
///////////////////////////
void loop() {
  uint16_t mm = sensor.readRangeContinuousMillimeters();
  bool timeout = sensor.timeoutOccurred();
  bool inRange = !timeout && mm > 0 && mm <= PISSING_RANGE_MM;

  Serial.printf("Distance: %u mm%s%s\n",
                mm,
                timeout  ? " [timeout]"  : "",
                inRange  ? " [IN RANGE]" : "");


  if (isToiletInUse()) {
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  } else {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  }
  digitalWrite(LED_B, LOW);

  // --- Servo toggle every 1 s ---
  if (servoTimer.expired(1000)) {
    servoAtMax = !servoAtMax;
    writeServo(servoAtMax ? SERVO_MAX_DEG : SERVO_REST_DEG);
    Serial.printf("[Servo] -> %d\n", servoAtMax ? SERVO_MAX_DEG : SERVO_REST_DEG);
    servoTimer.reset();
  }
}
