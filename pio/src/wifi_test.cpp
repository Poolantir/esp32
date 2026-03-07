#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  delay(1000);
  Serial.println("WiFi test");
}
