#include "relays.h"
#include <Arduino.h>
#include "pins.h"

static bool lowActive = true;
static bool applied[RELAY_COUNT];

static uint8_t level(bool on) { return (on == lowActive) ? LOW : HIGH; }

void relaysBegin(bool activeLow) {
  lowActive = activeLow;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    applied[i] = false;
    digitalWrite(PIN_RELAY[i], level(false));
    pinMode(PIN_RELAY[i], OUTPUT);
  }
}

uint8_t relaysApply(const bool on[RELAY_COUNT], bool changed[RELAY_COUNT]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    changed[i] = (on[i] != applied[i]);
    if (changed[i]) n++;
    applied[i] = on[i];
    digitalWrite(PIN_RELAY[i], level(on[i]));
  }
  return n;
}
