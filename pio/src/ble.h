// AI-ASSISTED
// Matt Krueger
// April 2026

#pragma once

#include <Arduino.h>
#include <functional>

using BleConnectionCB = std::function<void(bool connected)>;

void   bleInit();
void   bleSetConnectionCallback(BleConnectionCB cb);
void   bleSendMessage(const String& msg);
bool   bleIsConnected();
void   bleEnsureAdvertising();
bool   bleHasRxMessage();
String blePopRxMessage();
