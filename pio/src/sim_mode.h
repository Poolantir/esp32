// AI-ASSISTED
// Matt Krueger
// April 2026

#pragma once

#include <Arduino.h>

void enterSimMode();
void simModeTick();
void simNewUser(const String& id, float durationS);
void simPause();
void simPlay();
void sendSimAck(const String& id, bool ok, const String& error = "");
