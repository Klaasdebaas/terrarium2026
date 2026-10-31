#include <Arduino.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "logformat.h"

namespace {
const char* const LOG_DIR = "/log";
const char* const LAST = "/log/last.txt";
bool g_sdOk = false;

// Open, een regel (+ optionele header als het bestand nieuw is), close.
bool appendLine(const char* path, const char* header, const char* line) {
  bool isNew = header && !SD.exists(path);
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  bool ok = true;
  if (isNew) ok = f.print(header) == strlen(header) && f.print("\n") == 1;
  size_t n = strlen(line);
  ok = ok && f.print(line) == n && f.print("\n") == 1;
  f.close();
  return ok;
}
}  // namespace

bool loggerBegin() {
  g_sdOk = SD.exists(LOG_DIR) || SD.mkdir(LOG_DIR);
  return g_sdOk;
}

bool loggerSdOk() { return g_sdOk; }

void loggerMeasurement(const SystemState& s) {
  if (!s.timeValid) return;
  char name[24], line[160];
  formatDayFileName(name, sizeof name, s.time);
  formatMeasurementLine(line, sizeof line, s);
  if (!appendLine(name, measurementHeader(), line)) { g_sdOk = false; return; }
  // last.txt: overschrijven (tijdstip van de laatste meting, voor herstart zonder RTC)
  SD.remove(LAST);
  File f = SD.open(LAST, FILE_WRITE);
  if (!f) { g_sdOk = false; return; }
  char ts[24];
  snprintf(ts, sizeof ts, "%04d-%02u-%02u %02u:%02u:%02u", (int)s.time.year, (unsigned)s.time.month,
           (unsigned)s.time.day, (unsigned)s.time.hour, (unsigned)s.time.minute, (unsigned)s.time.second);
  bool ok = f.print(ts) == strlen(ts);
  f.close();
  if (!ok) g_sdOk = false;
}

void loggerEvent(const LocalTime& t, const char* kind, const char* text) {
  char line[160], name[32];
  formatEventLine(line, sizeof line, t, kind, text);
  formatEventFileName(name, sizeof name, t);   // per maand: /log/events-YYYY-MM.csv
  if (!appendLine(name, eventHeader(), line)) g_sdOk = false;
}

bool loggerLastTimestamp(char* buf, size_t len) {
  if (len == 0) return false;
  File f = SD.open(LAST, FILE_READ);
  if (!f) return false;
  size_t n = f.read((uint8_t*)buf, len - 1);
  f.close();
  buf[n] = '\0';
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
  return n > 0;
}

void loggerDailyMaintenance(const LocalTime& t, uint16_t retentionDays) {
  // Namen eerst verzamelen, daarna verwijderen (niet wijzigen tijdens iteratie).
  // Dagbestanden (YYYY-MM-DD.csv) en maandelijkse eventbestanden (events-YYYY-MM.csv).
  char expired[8][24];
  uint8_t count = 0;
  File dir = SD.open(LOG_DIR);
  if (!dir) { g_sdOk = false; return; }
  for (;;) {
    File e = dir.openNextFile();
    if (!e) break;
    const char* full = e.name();
    const char* slash = strrchr(full, '/');
    const char* base = slash ? slash + 1 : full;
    bool old = isExpired(base, t, retentionDays) || isExpiredMonthly(base, t, retentionDays);
    if (!e.isDirectory() && count < 8 && strlen(base) < sizeof expired[0] && old)
      strcpy(expired[count++], base);
    e.close();
  }
  dir.close();
  for (uint8_t i = 0; i < count; i++) {
    char path[32];
    snprintf(path, sizeof path, "%s/%s", LOG_DIR, expired[i]);
    SD.remove(path);
  }
}
