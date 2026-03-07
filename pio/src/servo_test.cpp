#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>

#define PIN_SERVO 14  // GPIO 14
#define LED_a 4 // GPIO 4
#define LED_b 16 // GPIO 16
#define LED_c 17 // GPIO 17

Servo servo;

void initServo() {
  servo.attach(PIN_SERVO);
}

void initLeds() {
  pinMode(LED_a, OUTPUT);
  pinMode(LED_b, OUTPUT);
  pinMode(LED_c, OUTPUT);
  digitalWrite(LED_a, LOW);
  digitalWrite(LED_b, LOW);
  digitalWrite(LED_c, LOW);
}

void setup() {
  Serial.begin(115200);
  initLeds();
  initServo();
}

void loop() {
  // Move one direction for 4 seconds (0 = one way on continuous-rotation servo)
  Serial.println("Moving forward");
  servo.write(0);
  digitalWrite(LED_a, HIGH);
  delay(1000);
  digitalWrite(LED_a, LOW);

  // Pause 2 seconds (90 = stop on continuous-rotation servo)
  Serial.println("Pause");
  servo.write(90);
  digitalWrite(LED_b, HIGH);
  delay(4000);
  digitalWrite(LED_b, LOW);

  // Move other direction for 4 seconds
  Serial.println("Moving backward");
  servo.write(180);
  digitalWrite(LED_c, HIGH);
  delay(1000);
  digitalWrite(LED_c, LOW);

  // Pause 2 seconds before next cycle
  Serial.println("Pause");
  servo.write(90);
  delay(2000);
}
