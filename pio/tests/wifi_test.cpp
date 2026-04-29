// Matt Krueger
// April 2026

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  delay(1000);
  Serial.println("WiFi test");
}