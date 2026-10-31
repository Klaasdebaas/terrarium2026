#include "minitest.h"
#include "display_format.h"
#include <string.h>

static SystemState base() {
  SystemState s; memset(&s, 0, sizeof(s));
  s.time = {2026, 10, 31, 14, 23, 5}; s.timeValid = true; s.isDay = true;
  s.airTemp = 28.6f; s.rh = 82.1f; s.heaterTemp = 27.1f; s.dhtOk = true; s.dsOk = true;
  s.relayOn[R_HEAT] = true; s.relayOn[R_LIGHT] = true;
  s.wifiOk = true; s.gsmOk = true; s.gsmSignal = 18;
  return s;
}

TEST(lines_normal) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  formatDisplayLines(s, false, l);
  CHECK_STR(l[0], "31-10-26 14:23   DAG");
  CHECK_STR(l[1], "Lucht 28.6C RV 82%");
  CHECK_STR(l[2], "Bron  27.1C");
  CHECK_STR(l[3], "VW:AAN  BV:UIT");
  CHECK_STR(l[4], "VE:UIT  LI:AAN");
  CHECK_STR(l[5], "WiFi:OK GSM:18");
}

TEST(lines_night_and_no_time) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.isDay = false; formatDisplayLines(s, false, l);
  CHECK_STR(l[0], "31-10-26 14:23  NACHT");
  s.timeValid = false; formatDisplayLines(s, false, l);
  CHECK_STR(l[0], "--:--  GEEN TIJD");
}

TEST(lines_manual) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.relayMode[R_VENT] = RelayMode::ManualOn; s.relayOn[R_VENT] = true;
  formatDisplayLines(s, false, l);
  CHECK(strncmp(l[4], "VE*AAN", 6) == 0);
}

TEST(lines_invalid) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.dhtOk = false; s.dsOk = false; formatDisplayLines(s, false, l);
  CHECK_STR(l[1], "Lucht --.-C RV --%");
  CHECK_STR(l[2], "Bron  --.-C");
}

TEST(lines_wifi_gsm_variants) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.wifiOk = false; s.gsmOk = false; formatDisplayLines(s, false, l);
  CHECK_STR(l[5], "WiFi:-- GSM:--");
  s.apMode = true; s.gsmOk = true; s.gsmSignal = -1; formatDisplayLines(s, false, l);
  CHECK_STR(l[5], "AP GSM:--");
}

TEST(lines_alarm) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.airTemp = 30.4f; s.alarmActive[A_TEMP_HIGH] = true;
  formatDisplayLines(s, true, l);
  CHECK_STR(l[5], "! TEMP HOOG 30.4C");
  formatDisplayLines(s, false, l);
  CHECK_STR(l[5], "WiFi:OK GSM:18");
  s.alarmActive[A_TEMP_HIGH] = false; s.alarmActive[A_RH_LOW] = true; s.rh = 40.4f;
  formatDisplayLines(s, true, l);
  CHECK_STR(l[5], "! RV LAAG 40%");
  s.alarmActive[A_RH_LOW] = false; s.alarmActive[A_DS_FAIL] = true;
  formatDisplayLines(s, true, l);
  CHECK_STR(l[5], "! DS18B20 STORING");
  // niets actief: showAlarm negeren
  s.alarmActive[A_DS_FAIL] = false; formatDisplayLines(s, true, l);
  CHECK_STR(l[5], "WiFi:OK GSM:18");
}

TEST(lines_fit) {
  SystemState s = base(); char l[DISPLAY_LINES][DISPLAY_COLS + 1];
  s.airTemp = -12.34f; s.rh = 100; s.heaterTemp = 105.5f; s.gsmSignal = 31;
  for (int i = 0; i < RELAY_COUNT; i++) { s.relayMode[i] = RelayMode::ManualOff; s.relayOn[i] = true; }
  for (int sa = 0; sa < 2; sa++) for (int a = -1; a < ALARM_COUNT; a++) {
    for (int i = 0; i < ALARM_COUNT; i++) s.alarmActive[i] = (i == a);
    for (int v = 0; v < 2; v++) {
      s.timeValid = v; s.isDay = v; s.dhtOk = v;
      formatDisplayLines(s, sa, l);
      for (int i = 0; i < DISPLAY_LINES; i++) CHECK(strlen(l[i]) <= DISPLAY_COLS);
    }
  }
}
