// Pure tekstopmaak voor het scherm. Geen hardware.
#include "display_format.h"
#include <stdio.h>
#include <string.h>

static const char* const RELAY_LABEL[RELAY_COUNT] = {"VW", "BV", "VE", "LI"};

// Kopieert src naar een regel en kapt af op DISPLAY_COLS tekens.
static void putLine(char* dst, const char* src) {
  size_t n = strlen(src);
  if (n > DISPLAY_COLS) n = DISPLAY_COLS;
  memcpy(dst, src, n); dst[n] = 0;
}

static void alarmText(const SystemState& s, AlarmId id, char* out) {
  char t[24], full[32];
  switch (id) {
    case A_TEMP_LOW:   snprintf(t, sizeof(t), "TEMP LAAG %.1fC", (double)s.airTemp); break;
    case A_TEMP_HIGH:  snprintf(t, sizeof(t), "TEMP HOOG %.1fC", (double)s.airTemp); break;
    case A_SOURCE_HOT: snprintf(t, sizeof(t), "BRON HEET %.1fC", (double)s.heaterTemp); break;
    case A_RH_LOW:     snprintf(t, sizeof(t), "RV LAAG %.0f%%", (double)s.rh); break;
    case A_RH_HIGH:    snprintf(t, sizeof(t), "RV HOOG %.0f%%", (double)s.rh); break;
    case A_DHT_FAIL:   snprintf(t, sizeof(t), "DHT22 STORING"); break;
    case A_DS_FAIL:    snprintf(t, sizeof(t), "DS18B20 STORING"); break;
    case A_SD_FAIL:    snprintf(t, sizeof(t), "SD FOUT"); break;
    case A_RTC_FAIL:   snprintf(t, sizeof(t), "KLOK FOUT"); break;
    case A_WIFI_LOST:  snprintf(t, sizeof(t), "WIFI WEG"); break;
    default:           t[0] = 0; break;
  }
  snprintf(full, sizeof(full), "! %s", t);
  putLine(out, full);
}

void formatDisplayLines(const SystemState& s, bool showAlarm, char lines[DISPLAY_LINES][DISPLAY_COLS + 1]) {
  char b[64];
  if (s.timeValid)
    snprintf(b, sizeof(b), "%02u-%02u-%02u %02u:%02u%s", (unsigned)s.time.day, (unsigned)s.time.month,
             (unsigned)(s.time.year % 100), (unsigned)s.time.hour, (unsigned)s.time.minute,
             s.isDay ? "   DAG" : "  NACHT");  // NACHT: 2 spaties zodat het in 21 tekens past
  else
    snprintf(b, sizeof(b), "--:--  GEEN TIJD");

  putLine(lines[0], b);

  char rh[8], air[12], src[12];
  if (s.dhtOk) {
    snprintf(air, sizeof(air), "%.1f", (double)s.airTemp);
    snprintf(rh, sizeof(rh), "%.0f", (double)s.rh);
  } else {
    snprintf(air, sizeof(air), "--.-"); snprintf(rh, sizeof(rh), "--");
  }
  if (s.dsOk) snprintf(src, sizeof(src), "%.1f", (double)s.heaterTemp);
  else snprintf(src, sizeof(src), "--.-");
  snprintf(b, sizeof(b), "Lucht %sC RV %s%%", air, rh); putLine(lines[1], b);
  snprintf(b, sizeof(b), "Bron  %sC", src); putLine(lines[2], b);

  for (int row = 0; row < 2; row++) {
    char part[2][9];
    for (int k = 0; k < 2; k++) {
      int r = row * 2 + k;
      snprintf(part[k], sizeof(part[k]), "%s%c%s", RELAY_LABEL[r],
               s.relayMode[r] == RelayMode::Auto ? ':' : '*', s.relayOn[r] ? "AAN" : "UIT");
    }
    snprintf(b, sizeof(b), "%s  %s", part[0], part[1]); putLine(lines[3 + row], b);
  }

  int first = -1;
  if (showAlarm)
    for (int i = 0; i < ALARM_COUNT; i++) if (s.alarmActive[i]) { first = i; break; }
  if (first >= 0) {
    alarmText(s, (AlarmId)first, lines[5]);
  } else {
    char gsm[8];
    if (s.gsmOk && s.gsmSignal >= 0) snprintf(gsm, sizeof(gsm), "%d", (int)s.gsmSignal);
    else snprintf(gsm, sizeof(gsm), "--");
    snprintf(b, sizeof(b), "%s GSM:%s", s.apMode ? "AP" : (s.wifiOk ? "WiFi:OK" : "WiFi:--"), gsm);
    putLine(lines[5], b);
  }
}
