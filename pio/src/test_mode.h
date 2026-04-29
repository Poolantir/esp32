// AI-ASSISTED
// Matt Krueger
// April 2026

#pragma once

#include <Arduino.h>

void enterTestMode();
void testModeTick();
void testLedCmd(const String& color);
void testServoCmd(const String& action);
void testQueueCmd();
