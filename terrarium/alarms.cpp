#include "alarms.h"
#include <stdio.h>
#include <string.h>

static const char* const KEYS[ALARM_COUNT] = {
  "temp_low", "temp_high", "source_hot", "rh_low", "rh_high",
  "dht_fail", "ds_fail", "sd_fail", "rtc_fail", "wifi_lost" };

void alarmsInit(AlarmState& st) {
  for (int i = 0; i < ALARM_COUNT; i++) {
    st.active[i] = false; st.condPrev[i] = false; st.condSinceMs[i] = 0;
    st.pending[i] = Notify::None; st.lastSent[i] = Notify::None;
    st.everSent[i] = false; st.lastSmsMs[i] = 0;
  }
}

const char* alarmKey(AlarmId id) { return id < ALARM_COUNT ? KEYS[id] : ""; }

static void conditions(const AlarmInput& in, const Settings& s, bool cond[], uint32_t delayMs[]) {
  for (int i = 0; i < ALARM_COUNT; i++) delayMs[i] = 0;
  uint32_t tempD = (uint32_t)s.tempDelayMin * 60000u, rhD = (uint32_t)s.rhDelayMin * 60000u;
  cond[A_TEMP_LOW] = in.dhtUsable && in.airTemp < s.tempLowC;   delayMs[A_TEMP_LOW] = tempD;
  cond[A_TEMP_HIGH] = in.dhtUsable && in.airTemp > s.tempHighC; delayMs[A_TEMP_HIGH] = tempD;
  cond[A_SOURCE_HOT] = in.dsUsable && in.heaterTemp > s.sourceMaxC;
  cond[A_RH_LOW] = in.dhtUsable && in.rh < s.rhLow;             delayMs[A_RH_LOW] = rhD;
  cond[A_RH_HIGH] = in.dhtUsable && in.rh > s.rhHigh;           delayMs[A_RH_HIGH] = rhD;
  cond[A_DHT_FAIL] = in.dhtFaulty;
  cond[A_DS_FAIL] = in.dsFaulty;
  cond[A_SD_FAIL] = !in.sdOk;
  cond[A_RTC_FAIL] = !in.rtcOk;
  cond[A_WIFI_LOST] = !in.wifiOk; delayMs[A_WIFI_LOST] = (uint32_t)s.wifiLostMin * 60000u;
}

uint8_t evaluateAlarms(const AlarmInput& in, const Settings& s, AlarmState& st, AlarmEvent* out, uint8_t maxOut) {
  bool cond[ALARM_COUNT]; uint32_t delayMs[ALARM_COUNT];
  conditions(in, s, cond, delayMs);
  const uint32_t limitMs = (uint32_t)s.smsLimitMin * 60000u;
  uint8_t n = 0;
  for (int i = 0; i < ALARM_COUNT; i++) {
    bool transition = false;
    if (cond[i]) {
      if (!st.condPrev[i]) { st.condPrev[i] = true; st.condSinceMs[i] = in.nowMs; }
      if (!st.active[i] && (uint32_t)(in.nowMs - st.condSinceMs[i]) >= delayMs[i]) {
        st.active[i] = true; transition = true; st.pending[i] = Notify::Alarm;
      }
    } else {
      st.condPrev[i] = false;
      if (st.active[i]) { st.active[i] = false; transition = true; st.pending[i] = Notify::Restored; }
    }

    bool send = false;
    if (st.pending[i] != Notify::None) {
      // Vervalt: Hersteld zonder eerder verstuurde Alarm-SMS, of gelijk aan laatst verstuurde.
      bool drop = (st.pending[i] == st.lastSent[i]) ||
                  (st.pending[i] == Notify::Restored && st.lastSent[i] != Notify::Alarm);
      if (drop) {
        st.pending[i] = Notify::None;
      } else if (!st.everSent[i] || (uint32_t)(in.nowMs - st.lastSmsMs[i]) >= limitMs) {
        send = true;
        st.lastSent[i] = st.pending[i]; st.pending[i] = Notify::None;
        st.everSent[i] = true; st.lastSmsMs[i] = in.nowMs;
      }
    }
    if ((transition || send) && n < maxOut) {
      out[n].id = (AlarmId)i; out[n].transition = transition;
      out[n].active = st.active[i]; out[n].sendSms = send; n++;
    }
  }
  return n;
}

static size_t finish(char* buf, size_t len, int r) {
  if (len == 0) return 0;
  if (r < 0) { buf[0] = 0; return 0; }
  return (size_t)r >= len ? len - 1 : (size_t)r;
}

size_t formatAlarmSms(char* buf, size_t len, AlarmId id, bool active, const AlarmInput& in, const LocalTime& t) {
  char d[96];
  switch (id) {
    case A_TEMP_LOW: case A_TEMP_HIGH:
      if (active) snprintf(d, sizeof d, "Temperatuur te %s %.1fC", id == A_TEMP_LOW ? "laag" : "hoog", (double)in.airTemp);
      else snprintf(d, sizeof d, "Temperatuur normaal %.1fC", (double)in.airTemp);
      break;
    case A_SOURCE_HOT:
      if (active) snprintf(d, sizeof d, "Warmtebron te heet %.1fC, verwarming uit", (double)in.heaterTemp);
      else snprintf(d, sizeof d, "Warmtebron normaal %.1fC", (double)in.heaterTemp);
      break;
    case A_RH_LOW: case A_RH_HIGH:
      if (active) snprintf(d, sizeof d, "Luchtvochtigheid te %s %.0f%%", id == A_RH_LOW ? "laag" : "hoog", (double)in.rh);
      else snprintf(d, sizeof d, "Luchtvochtigheid normaal %.0f%%", (double)in.rh);
      break;
    case A_DHT_FAIL: snprintf(d, sizeof d, "%s", active ? "Sensorstoring DHT22" : "Sensor DHT22 werkt weer"); break;
    case A_DS_FAIL: snprintf(d, sizeof d, "%s", active ? "Sensorstoring DS18B20" : "Sensor DS18B20 werkt weer"); break;
    case A_SD_FAIL: snprintf(d, sizeof d, "%s", active ? "SD-kaart fout" : "SD-kaart werkt weer"); break;
    case A_RTC_FAIL: snprintf(d, sizeof d, "%s", active ? "Klok (RTC) fout" : "Klok (RTC) werkt weer"); break;
    case A_WIFI_LOST: snprintf(d, sizeof d, "%s", active ? "WiFi verbinding weg" : "WiFi verbinding hersteld"); break;
    default: d[0] = 0; break;
  }
  return finish(buf, len, snprintf(buf, len, "TERRARIUM %s: %s (%02u-%02u %02u:%02u)",
                active ? "ALARM" : "HERSTELD", d, t.day, t.month, t.hour, t.minute));
}

size_t formatInfoSms(char* buf, size_t len, const char* text, const LocalTime& t) {
  return finish(buf, len, snprintf(buf, len, "TERRARIUM INFO: %s (%02u-%02u %02u:%02u)",
                text, t.day, t.month, t.hour, t.minute));
}
