#ifndef TERRARIUM_LOGFORMAT_H
#define TERRARIUM_LOGFORMAT_H
#include <stdint.h>
#include <stddef.h>
#include "types.h"
#include "state.h"

void formatDayFileName(char* buf, size_t len, const LocalTime& t);   // "/log/2026-10-31.csv"
void formatEventFileName(char* buf, size_t len, const LocalTime& t); // "/log/events-2026-10.csv"
const char* measurementHeader();
const char* eventHeader();
size_t formatMeasurementLine(char* buf, size_t len, const SystemState& s);  // zonder newline
size_t formatEventLine(char* buf, size_t len, const LocalTime& t, const char* kind, const char* text);
bool isSafeLogName(const char* name);   // alleen naam, geen pad
int32_t daysFromCivil(int y, unsigned m, unsigned d);
bool isExpired(const char* fileName, const LocalTime& today, uint16_t retentionDays);   // "YYYY-MM-DD.csv"
// "events-YYYY-MM.csv": leeftijd gerekend vanaf de 1e van de volgende maand.
bool isExpiredMonthly(const char* name, const LocalTime& today, uint16_t retentionDays);

#endif  // TERRARIUM_LOGFORMAT_H
