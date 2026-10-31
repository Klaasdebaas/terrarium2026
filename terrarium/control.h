#pragma once
#include <stdint.h>
#include "types.h"
#include "config.h"

bool isDay(uint16_t minuteOfDay, uint16_t dayStartMin, uint16_t nightStartMin);
bool inVentWindow(uint16_t minuteOfDay, uint16_t intervalMin, uint16_t durationMin);

struct ControlInput {
  uint32_t nowMs; uint16_t minuteOfDay; bool timeValid;
  float airTemp, rh, heaterTemp; bool dhtOk, dsOk; RelayMode mode[RELAY_COUNT];
};
struct ControlMemory {
  bool autoOn[RELAY_COUNT]; uint32_t humOnSinceMs, humPauseSinceMs; bool humPausing; bool sourceTripped;
};
struct ControlOutput { bool on[RELAY_COUNT]; bool isDay; };

void controlInit(ControlMemory& m);
ControlOutput decide(const ControlInput& in, const Settings& s, ControlMemory& m);

struct Overrides { RelayMode mode[RELAY_COUNT]; uint32_t sinceMs[RELAY_COUNT]; };
void overridesInit(Overrides& o);
void setOverride(Overrides& o, RelayId r, RelayMode mode, uint32_t nowMs);
void expireOverrides(Overrides& o, uint32_t nowMs, uint16_t timeoutMin);
uint32_t overrideRemainingS(const Overrides& o, RelayId r, uint32_t nowMs, uint16_t timeoutMin);
