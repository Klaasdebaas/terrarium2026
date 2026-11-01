#ifndef TERRARIUM_LOGGER_H
#define TERRARIUM_LOGGER_H
#include <stdint.h>
#include <stddef.h>
#include "types.h"
#include "state.h"

bool loggerBegin();                                           // mkdir /log, zet sdOk (SD.begin() door aanroeper)
void loggerMeasurement(const SystemState& s);                 // append dagbestand + /log/last.txt
void loggerEvent(const LocalTime& t, const char* kind, const char* text);  // /log/events-YYYY-MM.csv
void loggerDailyMaintenance(const LocalTime& t, uint16_t retentionDays);
bool loggerLastTimestamp(char* buf, size_t len);
bool loggerSdOk();

#endif  // TERRARIUM_LOGGER_H
