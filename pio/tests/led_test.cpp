// Matt Krueger
// April 2026

#include <Arduino.h>

#define LED_R 27
#define LED_G 26
#define LED_B 25

void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  Serial.println("LED test started");
}

void loop() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  Serial.println("RED");
  delay(2000);

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
  Serial.println("GREEN");
  delay(2000);

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
  Serial.println("BLUE");
  delay(2000);
}
