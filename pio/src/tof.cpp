// AI-ASSISTED
// Matt Krueger
// April 2026

#include "tof.h"
#include "config.h"
#include <Wire.h>
#include <VL53L0X.h>

static VL53L0X sSensor;
static int sInRangeMm   = DEFAULT_IN_RANGE_MM;
static bool sContinuous = false;

void tofInit() {
  Wire.begin(TOF_SDA, TOF_SCL);
  Wire.setClock(100000);
  sSensor.setTimeout(500);

  bool ok = false;
  for (int i = 0; i < 5; i++) {
    if (sSensor.init()) { ok = true; break; }
    delay(500);
  }
  if (!ok) {
    Serial.println("[ERROR] ToF sensor init failed");
    digitalWrite(LED_R, HIGH);
    while (1) {}
  }
  sContinuous = false;
}

void tofStartContinuous() {
  if (!sContinuous) {
    sSensor.startContinuous();
    sContinuous = true;
    Serial.println("[ToF] continuous mode started");
  }
}

void tofStopContinuous() {
  if (sContinuous) {
    sSensor.stopContinuous();
    sContinuous = false;
    Serial.println("[ToF] continuous mode stopped");
  }
}

bool tofIsInRange() {
  if (!sContinuous) return false;
  uint16_t mm = sSensor.readRangeContinuousMillimeters();
  return !sSensor.timeoutOccurred() && mm > 0 && mm <= (uint16_t)sInRangeMm;
}

void tofSetInRangeMm(int mm) {
  sInRangeMm = constrain(mm, 20, 2000);
  Serial.printf("[ToF] in-range threshold set to %d mm\n", sInRangeMm);
}

int  tofGetInRangeMm()  { return sInRangeMm; }
bool tofIsContinuous()  { return sContinuous; }
