#pragma once
#include <stdint.h>

// Instellingen met standaardwaarden uit spec 5.1. De rest van config.h volgt in Task 5.
struct Settings {
  uint16_t dayStartMin = 480, nightStartMin = 1200;
  float heatDayC = 29, heatNightC = 22, heatHystC = 0.5f, sourceMaxC = 30;
  float humOnBelow = 70, humOffAbove = 75; uint16_t humMaxOnMin = 10, humPauseMin = 5;
  float ventOnAbove = 95, ventOffBelow = 90; uint16_t ventIntervalMin = 120, ventDurationMin = 5;
  float tempLowC = 20, tempHighC = 30; uint16_t tempDelayMin = 5;
  float rhLow = 70, rhHigh = 95; uint16_t rhDelayMin = 30, wifiLostMin = 30;
  uint16_t smsLimitMin = 60, manualTimeoutMin = 30; bool relayActiveLow = true;
  uint16_t logIntervalMin = 5, retentionDays = 365;
};
