#pragma once

#include <Arduino.h>

void   bleInit();
void   bleSendMessage(const String& msg);
bool   bleIsConnected();
void   bleEnsureAdvertising();
bool   bleHasRxMessage();
String blePopRxMessage();
