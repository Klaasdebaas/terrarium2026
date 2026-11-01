#ifndef TERRARIUM_RELAYS_H
#define TERRARIUM_RELAYS_H
#include <stdint.h>
#include "types.h"

void relaysBegin(bool activeLow);
// Stuurt alle pinnen; geeft aantal wijzigingen terug, changed[i] per relais.
uint8_t relaysApply(const bool on[RELAY_COUNT], bool changed[RELAY_COUNT]);

#endif  // TERRARIUM_RELAYS_H
