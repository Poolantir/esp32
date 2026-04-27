#pragma once

#include <Arduino.h>

void enterSimMode();
void simModeTick();
void simNewUser(const String& id, float durationS);
void simPause();
void simPlay();
