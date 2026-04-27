#pragma once

#include <Arduino.h>

void servoInit();
void servoWriteImmediate(int deg);
int  servoCurrentDeg();
