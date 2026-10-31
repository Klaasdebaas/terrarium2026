#pragma once
#include <stdint.h>
#include "types.h"

bool clockBegin(const char* tz, const char* ntpServer);  // Wire moet al gestart zijn
void clockUpdate(uint32_t nowMs, bool wifiOk);
bool clockNow(LocalTime& out);        // false als jaar < 2024
bool clockRtcOk();                    // RTC bereikbaar en geldige tijd
bool clockSetLocal(const LocalTime& t);
bool clockTakeSyncEvent();            // true eenmaal na elke geslaagde NTP-sync
