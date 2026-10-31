#pragma once
#include <stdint.h>

bool validDht(float tempC, float rh);
bool validDs(float tempC);

class FaultCounter {
public:
  void record(bool valid);
  bool usable() const;
  bool faulty() const;
private:
  uint8_t invalidStreak_ = 0;
  bool everValid_ = false;
};
