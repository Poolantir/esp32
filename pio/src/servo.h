#pragma once

#include <Arduino.h>

void servoInit();
void servoWriteImmediate(int deg);
void servoMoveAnimated(int targetDeg);
void servoTick();
bool servoIsMoving();
int  servoCurrentDeg();
void servoSetRampMs(uint32_t ms);
uint32_t servoGetRampMs();
void servoPause();
void servoResume();
