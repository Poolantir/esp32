// Matt Krueger
// April 2026

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

#define LED_R 27
#define LED_G 26
#define LED_B 25

#define TOF_SDA 21
#define TOF_SCL 22

#define PISSING_RANGE_MM 60

VL53L0X sensor;

void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  delay(2000);
  Wire.begin(TOF_SDA, TOF_SCL);
  Wire.setClock(100000);

  // FLASH SEQUENCE
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  Serial.println("RED");
  delay(1000);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
  Serial.println("GREEN");
  delay(1000);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
  Serial.println("BLUE");
  delay(1000);

  // TURN OFF
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);

  sensor.setTimeout(500);
  bool initialized = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    Serial.printf("ToF init attempt %d/5...\n", attempt);
    if (sensor.init()) {
      initialized = true;
      break;
    }
    Serial.println("  failed, retrying...");
    delay(500);
  }
  if (!initialized) {
    Serial.println("failure initializing the ToF sensor after 5 attempts");
    digitalWrite(LED_R, HIGH);
    while (1) {}
  }
  sensor.startContinuous();
  Serial.println("ToF test started");
}

void loop() {
  uint16_t mm = sensor.readRangeContinuousMillimeters();
  bool timeout = sensor.timeoutOccurred();
  bool inRange = !timeout && mm > 0 && mm <= PISSING_RANGE_MM;

  Serial.print("Distance: ");
  Serial.print(mm);
  Serial.print(" mm");
  if (timeout) Serial.print(" [timeout]");
  if (inRange) Serial.print(" [IN RANGE]");
  Serial.println();

  if (inRange) {
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  } else {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  }
  digitalWrite(LED_B, LOW);
}
