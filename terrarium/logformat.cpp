#include "logformat.h"
#include "alarms.h"
#include <stdio.h>
#include <string.h>

void formatDayFileName(char* buf, size_t len, const LocalTime& t) {
  snprintf(buf, len, "/log/%04d-%02u-%02u.csv", (int)t.year, (unsigned)t.month, (unsigned)t.day);
}

const char* measurementHeader() {
  return "tijd;lucht_c;rv_pct;bron_c;r1_verw;r2_bevocht;r3_vent;r4_licht;alarmen";
}

static size_t clampLen(int n, size_t len) {
  if (n < 0) return 0;
  return (size_t)n >= len ? (len ? len - 1 : 0) : (size_t)n;
}

size_t formatMeasurementLine(char* buf, size_t len, const SystemState& s) {
  if (len == 0) return 0;
  char air[12] = "", rh[12] = "", src[12] = "";
  if (s.dhtOk) { snprintf(air, sizeof air, "%.1f", (double)s.airTemp); snprintf(rh, sizeof rh, "%.1f", (double)s.rh); }
  if (s.dsOk) snprintf(src, sizeof src, "%.1f", (double)s.heaterTemp);
  int n = snprintf(buf, len, "%04d-%02u-%02u %02u:%02u:%02u;%s;%s;%s;%d;%d;%d;%d;",
                   (int)s.time.year, (unsigned)s.time.month, (unsigned)s.time.day,
                   (unsigned)s.time.hour, (unsigned)s.time.minute, (unsigned)s.time.second,
                   air, rh, src, s.relayOn[R_HEAT] ? 1 : 0, s.relayOn[R_HUMID] ? 1 : 0,
                   s.relayOn[R_VENT] ? 1 : 0, s.relayOn[R_LIGHT] ? 1 : 0);
  size_t pos = clampLen(n, len);
  bool first = true;
  for (int i = 0; i < ALARM_COUNT; i++) {
    if (!s.alarmActive[i]) continue;
    n = snprintf(buf + pos, len - pos, "%s%s", first ? "" : ",", alarmKey((AlarmId)i));
    pos += clampLen(n, len - pos);
    first = false;
  }
  return pos;
}

size_t formatEventLine(char* buf, size_t len, const LocalTime& t, const char* kind, const char* text) {
  if (len == 0) return 0;
  int n = snprintf(buf, len, "%04d-%02u-%02u %02u:%02u:%02u;%s;%s",
                   (int)t.year, (unsigned)t.month, (unsigned)t.day,
                   (unsigned)t.hour, (unsigned)t.minute, (unsigned)t.second,
                   kind ? kind : "", text ? text : "");
  return clampLen(n, len);
}

bool isSafeLogName(const char* name) {
  if (!name) return false;
  size_t n = strlen(name);
  if (n <= 4 || n > 32) return false;
  if (strcmp(name + n - 4, ".csv") != 0) return false;
  for (size_t i = 0; i < n - 4; i++) {
    char c = name[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

bool isExpired(const char* fileName, const LocalTime& today, uint16_t retentionDays) {
  if (!fileName || strlen(fileName) != 14) return false;
  const char* p = fileName;
  for (int i = 0; i < 10; i++) {
    if (i == 4 || i == 7) { if (p[i] != '-') return false; }
    else if (p[i] < '0' || p[i] > '9') return false;
  }
  if (strcmp(p + 10, ".csv") != 0) return false;
  int y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
  unsigned m = (unsigned)((p[5] - '0') * 10 + (p[6] - '0'));
  unsigned d = (unsigned)((p[8] - '0') * 10 + (p[9] - '0'));
  if (m < 1 || m > 12 || d < 1 || d > 31) return false;
  int32_t age = daysFromCivil(today.year, today.month, today.day) - daysFromCivil(y, m, d);
  return age > (int32_t)retentionDays;
}
