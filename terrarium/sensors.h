#ifndef TERRARIUM_SENSORS_H
#define TERRARIUM_SENSORS_H
#include <stdint.h>

struct SensorSnapshot {
  float airTemp, rh, heaterTemp;
  bool dhtUsable, dsUsable, dhtFaulty, dsFaulty;
};

void sensorsBegin();
void sensorsUpdate(uint32_t nowMs);   // elke 5000 ms
SensorSnapshot sensorsGet();

#endif  // TERRARIUM_SENSORS_H
