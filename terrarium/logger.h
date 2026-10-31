#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"
#include "state.h"

bool loggerBegin();                                           // mkdir /log, zet sdOk (SD.begin() door aanroeper)
void loggerMeasurement(const SystemState& s);                 // append dagbestand + /log/last.txt
void loggerEvent(const LocalTime& t, const char* kind, const char* text);  // events.csv
void loggerDailyMaintenance(const LocalTime& t, uint16_t retentionDays);
bool loggerLastTimestamp(char* buf, size_t len);
bool loggerSdOk();
