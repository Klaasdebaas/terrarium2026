#pragma once
// Pure tekstopmaak voor het 128x64 scherm (6 regels van 21 tekens). Geen hardware.
#include <stdint.h>
#include "state.h"

constexpr uint8_t DISPLAY_LINES = 6, DISPLAY_COLS = 21;

// showAlarm: regel 6 toont het eerste actieve alarm (alleen als er een alarm actief is).
void formatDisplayLines(const SystemState& s, bool showAlarm, char lines[DISPLAY_LINES][DISPLAY_COLS + 1]);
