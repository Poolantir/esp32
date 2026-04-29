// AI-ASSISTED
// Matt Krueger
// April 2026

#include "led.h"
#include "config.h"

void ledInit() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  ledAllOff();
}

void ledAllOff() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

void ledSetRed() {
  ledAllOff();
  digitalWrite(LED_R, HIGH);
}

void ledSetGreen() {
  ledAllOff();
  digitalWrite(LED_G, HIGH);
}

void ledSetBlue() {
  ledAllOff();
  digitalWrite(LED_B, HIGH);
}

void ledFlashTest(const String& color) {
  String c = color;
  c.toUpperCase();

  int pin;
  const char* name;
  if      (c == "R") { pin = LED_R; name = "RED";   }
  else if (c == "G") { pin = LED_G; name = "GREEN"; }
  else if (c == "B") { pin = LED_B; name = "BLUE";  }
  else {
    Serial.printf("[LED] invalid flash color: \"%s\"\n", color.c_str());
    return;
  }

  ledAllOff();
  Serial.printf("[LED] flashing %s for %d ms\n", name, LED_FLASH_MS);
  digitalWrite(pin, HIGH);
  delay(LED_FLASH_MS);
  digitalWrite(pin, LOW);
}
