#include "sensor_logic.h"
#include <math.h>

bool validDht(float tempC, float rh) {
  if (isnan(tempC) || isnan(rh)) return false;
  return tempC >= -40.0f && tempC <= 80.0f && rh >= 0.0f && rh <= 100.0f;
}

bool validDs(float tempC) {
  if (isnan(tempC)) return false;
  if (tempC < -55.0f || tempC > 125.0f) return false;
  return tempC != -127.0f && tempC != 85.0f;
}

void FaultCounter::record(bool valid) {
  if (valid) {
    invalidStreak_ = 0;
    everValid_ = true;
  } else if (invalidStreak_ < 255) {
    invalidStreak_++;
  }
}

bool FaultCounter::faulty() const { return invalidStreak_ >= 3; }
bool FaultCounter::usable() const { return everValid_ && !faulty(); }
