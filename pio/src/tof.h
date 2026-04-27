#pragma once

#include <Arduino.h>

void tofInit();
void tofStartContinuous();
void tofStopContinuous();
bool tofIsInRange();
void tofSetInRangeMm(int mm);
int  tofGetInRangeMm();
bool tofIsContinuous();
