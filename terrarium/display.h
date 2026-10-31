#pragma once
#include <stdint.h>
#include "state.h"

// SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI) moet vooraf in setup() zijn aangeroepen.
void displayBegin();
// Herschildert elke 1000 ms; bij actieve alarmen wisselt regel 6 elke 2000 ms.
void displayUpdate(const SystemState& s, uint32_t nowMs);
