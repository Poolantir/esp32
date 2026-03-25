#pragma once

#include <Arduino.h>

// millis()-based timer utility.
// - Uses unsigned subtraction so it is rollover-safe.
// - Typical usage:
//     ClockTimer t;
//     t.start();
//     while (!t.expired(10'000)) { ... } // run for 10s, then continue
class ClockTimer {
public:
  ClockTimer() = default;

  void start() {
    _running = true;
    _startMs = millis();
  }

  void reset() {
    start();
  }

  void stop() {
    _running = false;
  }

  bool isRunning() const {
    return _running;
  }

  uint32_t elapsedMs() const {
    if (!_running) {
      return 0;
    }
    return static_cast<uint32_t>(millis() - _startMs);
  }

  bool expired(uint32_t durationMs) const {
    return elapsedMs() >= durationMs;
  }

private:
  uint32_t _startMs = 0;
  bool _running = false;
};

