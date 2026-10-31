#include "relays.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "pins.h"

static bool lowActive = true;
static bool applied[RELAY_COUNT];

static uint8_t level(bool on) { return (on == lowActive) ? LOW : HIGH; }

void relaysBegin(bool activeLow) {
  lowActive = activeLow;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    applied[i] = false;
    // Core 3.x negeert digitalWrite() voor pinMode(); zet het uitgangsniveau eerst via de
    // IDF-driver zodat pinMode(OUTPUT) een actief-laag relais niet even inschakelt.
    gpio_set_level((gpio_num_t)PIN_RELAY[i], level(false));
    pinMode(PIN_RELAY[i], OUTPUT);
    digitalWrite(PIN_RELAY[i], level(false));
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
