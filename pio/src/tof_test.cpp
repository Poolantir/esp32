#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

// ----------------------------- //
//         PIN ASSIGNMENTS       //
// ----------------------------- //
// ESP32 I2C: SDA=21, SCL=22 (connect 21→ToF SDA, 22→ToF SCL)
#define TOF_SDA 21
#define TOF_SCL 22
#define LED_a 4     // GPIO 4
#define LED_b 16    // GPIO 16
#define LED_c 17    // GPIO 17

VL53L0X tof;

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
  delay(200);  // Let UART stabilize (helps avoid garbled first lines)
  Serial.println("\n\n=== ToF test @ 115200 ===");
  initLeds();
  Wire.begin(TOF_SDA, TOF_SCL);

  tof.setTimeout(500);

  if (!tof.init()) {
    Serial.println("[ERROR] ToF sensor not found. Wire 21→SDA, 22→SCL");
    while (1) {}
  }

  tof.startContinuous();
}

void loop() {
  uint16_t mm = tof.readRangeContinuousMillimeters();
  if (tof.timeoutOccurred()) {
    Serial.println("(timeout)");
  }
  else {
    Serial.print("distance: ");
    Serial.print(mm);
    Serial.println(" mm");

    if (mm < 50) {
      digitalWrite(LED_a, HIGH);
    } else {
      digitalWrite(LED_a, LOW);
    }
    if (mm < 100) {
      digitalWrite(LED_b, HIGH);
    } else {
      digitalWrite(LED_b, LOW);
    }
    if (mm < 150) {
      digitalWrite(LED_c, HIGH);
    } else {
      digitalWrite(LED_c, LOW);
    }
  }

  delay(500);  // one reading per second so serial doesn't flood
}
