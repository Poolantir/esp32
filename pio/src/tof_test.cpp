#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>


// ----------------------------- //
//         PIN ASSIGNMENTS       //
// ----------------------------- //

// ESP32 I2C: SDA=21, SCL=22 (connect 21→ToF SDA, 22→ToF SCL)
#define TOF_SDA 21
#define TOF_SCL 22

// testing purposes
#define LED_a 4     // GPIO 4
#define LED_b 16    // GPIO 16
#define LED_c 17    // GPIO 17

///////////////////
//    TOF OBJ   //
///////////////////
VL53L0X sensor;

void initLeds() {
  pinMode(LED_a, OUTPUT);
  pinMode(LED_b, OUTPUT);
  pinMode(LED_c, OUTPUT);
  digitalWrite(LED_a, HIGH);
  digitalWrite(LED_b, HIGH);
  digitalWrite(LED_c, HIGH);
  Serial.println("LEDs initialized");

  delay(5000);
  digitalWrite(LED_a, LOW);
  digitalWrite(LED_b, LOW);
  digitalWrite(LED_c, LOW);
}
void setup() {
  Serial.begin(115200);
  Wire.begin();
  initLeds();

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("failure initializing the ToF sensuh");
    while (1) {}
  }

  // start the continous mode (there is continuous and single options)
  sensor.startContinuous();
}

void loop() {
  Serial.print("Sensor Reading: ");
  Serial.print(sensor.readRangeContinuousMillimeters());
  if (sensor.timeoutOccurred())
  {
    Serial.print("[timeout occurred]"); 
  }

  Serial.println();
}
