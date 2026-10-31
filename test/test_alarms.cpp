#include "minitest.h"
#include "alarms.h"
#include <string.h>

static AlarmInput ok(uint32_t t) { AlarmInput in; in.nowMs = t; return in; }
static const uint32_t MIN = 60000;

struct Run {
  Settings s; AlarmState st; AlarmEvent ev[ALARM_COUNT]; uint8_t n = 0;
  Run() { alarmsInit(st); }
  void go(const AlarmInput& in) { n = evaluateAlarms(in, s, st, ev, ALARM_COUNT); }
  const AlarmEvent* find(AlarmId id) const {
    for (uint8_t i = 0; i < n; i++) if (ev[i].id == id) return &ev[i];
    return nullptr;
  }
};

TEST(temp_high_needs_delay) {
  Run r; AlarmInput in = ok(0); in.airTemp = 30.5f;
  r.go(in); CHECK(r.find(A_TEMP_HIGH) == nullptr);
  in.nowMs = 299999; r.go(in); CHECK(r.find(A_TEMP_HIGH) == nullptr);
  in.nowMs = 300000; r.go(in);
  const AlarmEvent* e = r.find(A_TEMP_HIGH);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(e->active); CHECK(e->sendSms); }
}

TEST(temp_spike_no_alarm) {
  Run r; AlarmInput in = ok(0); in.airTemp = 30.5f;
  for (uint32_t t = 0; t <= 200000; t += 1000) { in.nowMs = t; r.go(in); CHECK(r.n == 0); }
  in.nowMs = 200001; in.airTemp = 29; r.go(in); CHECK(r.n == 0);
  in.airTemp = 30.5f;
  for (uint32_t t = 200002; t <= 400000; t += 1000) { in.nowMs = t; r.go(in); CHECK(r.n == 0); }
  CHECK(!r.st.active[A_TEMP_HIGH]);
}

TEST(source_hot_immediate) {
  Run r; AlarmInput in = ok(0); in.heaterTemp = 30.1f; r.go(in);
  const AlarmEvent* e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(e->active); CHECK(e->sendSms); }
}

TEST(no_temp_alarm_when_dht_unusable) {
  Run r; AlarmInput in = ok(0); in.dhtUsable = false; in.airTemp = 0;
  for (uint32_t t = 0; t <= 400000; t += 10000) { in.nowMs = t; r.go(in); CHECK(r.find(A_TEMP_LOW) == nullptr); }
  CHECK(!r.st.active[A_TEMP_LOW]);
}

TEST(sensor_fail_alarm) {
  Run r; AlarmInput in = ok(0); in.dhtFaulty = true; r.go(in);
  const AlarmEvent* e = r.find(A_DHT_FAIL);
  CHECK(e != nullptr);
  if (e) { CHECK(e->active); CHECK(e->transition); }
}

TEST(restore_sends_sms) {
  Run r; uint32_t t0 = 1000; AlarmInput in = ok(t0); in.heaterTemp = 31; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) && r.find(A_SOURCE_HOT)->sendSms);
  in = ok(t0 + 61 * MIN); r.go(in);
  const AlarmEvent* e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(!e->active); CHECK(e->sendSms); }
}

TEST(rate_limit_defers_restore) {
  Run r; uint32_t t0 = 1000; AlarmInput in = ok(t0); in.heaterTemp = 31; r.go(in);
  in = ok(t0 + 10 * MIN); r.go(in);
  const AlarmEvent* e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(!e->active); CHECK(!e->sendSms); }
  in = ok(t0 + 59 * MIN); r.go(in); CHECK(r.find(A_SOURCE_HOT) == nullptr);
  in = ok(t0 + 60 * MIN); r.go(in);
  e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(!e->transition); CHECK(!e->active); CHECK(e->sendSms); }
  CHECK(r.st.lastSent[A_SOURCE_HOT] == Notify::Restored);
}

TEST(rate_limit_flap_drops) {
  Run r; uint32_t t0 = 1000; AlarmInput in = ok(t0); in.heaterTemp = 31; r.go(in);
  in = ok(t0 + 10 * MIN); r.go(in);
  in = ok(t0 + 20 * MIN); in.heaterTemp = 31; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) && r.find(A_SOURCE_HOT)->active);
  in = ok(t0 + 60 * MIN); in.heaterTemp = 31; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) == nullptr);
  in = ok(t0 + 120 * MIN); in.heaterTemp = 31; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) == nullptr);
}

TEST(rate_limit_wraps) {
  Run r; uint32_t t0 = 0xFFFFFF00u; AlarmInput in = ok(t0); in.heaterTemp = 31; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) && r.find(A_SOURCE_HOT)->sendSms);
  in = ok(t0 + 61 * MIN); r.go(in);
  const AlarmEvent* e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(!e->active); CHECK(e->sendSms); }
}

TEST(wifi_lost_delay) {
  Run r; AlarmInput in = ok(0); in.wifiOk = false; r.go(in);
  in.nowMs = 30 * MIN - 1; r.go(in); CHECK(r.find(A_WIFI_LOST) == nullptr);
  in.nowMs = 30 * MIN; r.go(in);
  CHECK(r.find(A_WIFI_LOST) && r.find(A_WIFI_LOST)->active);
}

static LocalTime lt(uint8_t d, uint8_t mo, uint8_t h, uint8_t mi) {
  LocalTime t; t.year = 2026; t.month = mo; t.day = d; t.hour = h; t.minute = mi; t.second = 0; return t;
}

TEST(sms_format_alarm) {
  char buf[200]; AlarmInput in = ok(0); in.airTemp = 30.4f;
  size_t n = formatAlarmSms(buf, sizeof buf, A_TEMP_HIGH, true, in, lt(31, 10, 14, 23));
  CHECK_STR(buf, "TERRARIUM ALARM: Temperatuur te hoog 30.4C (31-10 14:23)");
  CHECK_EQ(n, strlen(buf));
}

TEST(sms_format_restored) {
  char buf[200]; AlarmInput in = ok(0); in.airTemp = 28.9f;
  formatAlarmSms(buf, sizeof buf, A_TEMP_HIGH, false, in, lt(31, 10, 14, 41));
  CHECK_STR(buf, "TERRARIUM HERSTELD: Temperatuur normaal 28.9C (31-10 14:41)");
}

TEST(sms_format_info) {
  char buf[200];
  formatInfoSms(buf, sizeof buf, "Herstart. Laatste log 31-10 03:15", lt(31, 10, 7, 2));
  CHECK_STR(buf, "TERRARIUM INFO: Herstart. Laatste log 31-10 03:15 (31-10 07:02)");
}

TEST(sms_max_160) {
  char buf[400]; AlarmInput in = ok(0); in.airTemp = 19.4f; in.rh = 65; in.heaterTemp = 31.2f;
  LocalTime t = lt(1, 2, 3, 4);
  for (int a = 0; a < ALARM_COUNT; a++) {
    for (int act = 0; act < 2; act++) {
      formatAlarmSms(buf, sizeof buf, (AlarmId)a, act != 0, in, t);
      CHECK(strlen(buf) <= 160);
      for (const char* p = buf; *p; p++) CHECK(*p >= 0x20 && *p <= 0x7E);
    }
  }
  formatAlarmSms(buf, sizeof buf, A_SOURCE_HOT, true, in, t);
  CHECK_STR(buf, "TERRARIUM ALARM: Warmtebron te heet 31.2C, verwarming uit (01-02 03:04)");
  formatAlarmSms(buf, sizeof buf, A_RH_LOW, true, in, t);
  CHECK_STR(buf, "TERRARIUM ALARM: Luchtvochtigheid te laag 65% (01-02 03:04)");
  formatAlarmSms(buf, sizeof buf, A_WIFI_LOST, false, in, t);
  CHECK_STR(buf, "TERRARIUM HERSTELD: WiFi verbinding hersteld (01-02 03:04)");
}

TEST(alarm_keys) {
  CHECK_STR(alarmKey(A_TEMP_LOW), "temp_low"); CHECK_STR(alarmKey(A_WIFI_LOST), "wifi_lost");
  CHECK_STR(alarmKey(A_SOURCE_HOT), "source_hot"); CHECK_STR(alarmKey(A_RTC_FAIL), "rtc_fail");
}

TEST(temp_alarm_held_while_dht_unusable) {
  Run r; AlarmInput in = ok(0); in.airTemp = 31.2f;
  r.go(in); in.nowMs = 5 * MIN; r.go(in);
  CHECK(r.st.active[A_TEMP_HIGH]);
  // sensor onbruikbaar: geen (valse) HERSTELD, ook niet als de oude waarde normaal lijkt
  in.dhtUsable = false; in.airTemp = 25;
  for (uint32_t t = 6 * MIN; t <= 200 * MIN; t += MIN) {
    in.nowMs = t; r.go(in);
    CHECK(r.find(A_TEMP_HIGH) == nullptr);
  }
  CHECK(r.st.active[A_TEMP_HIGH]);
  // weer bruikbaar maar nog te warm: blijft actief zonder event
  in.dhtUsable = true; in.airTemp = 31.2f; in.nowMs = 201 * MIN; r.go(in);
  CHECK(r.find(A_TEMP_HIGH) == nullptr);
  CHECK(r.st.active[A_TEMP_HIGH]);
  // bruikbaar en normaal: hersteld
  in.airTemp = 25; in.nowMs = 202 * MIN; r.go(in);
  const AlarmEvent* e = r.find(A_TEMP_HIGH);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(!e->active); }
}

TEST(rh_alarm_held_while_dht_unusable) {
  Run r; AlarmInput in = ok(0); in.rh = 50;
  r.go(in); in.nowMs = 30 * MIN; r.go(in);
  CHECK(r.st.active[A_RH_LOW]);
  in.dhtUsable = false; in.rh = 80; in.nowMs = 31 * MIN; r.go(in);
  CHECK(r.find(A_RH_LOW) == nullptr);
  CHECK(r.st.active[A_RH_LOW]);
}

TEST(source_hot_held_while_ds_unusable) {
  Run r; AlarmInput in = ok(0); in.heaterTemp = 31; r.go(in);
  CHECK(r.st.active[A_SOURCE_HOT]);
  in.dsUsable = false; in.heaterTemp = 20; in.nowMs = MIN; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) == nullptr);
  CHECK(r.st.active[A_SOURCE_HOT]);
  in.dsUsable = true; in.nowMs = 2 * MIN; r.go(in);
  CHECK(r.find(A_SOURCE_HOT) != nullptr);
  CHECK(!r.st.active[A_SOURCE_HOT]);
}

TEST(source_hot_hysteresis_1c) {
  Run r; AlarmInput in = ok(0); in.heaterTemp = 30.5f; r.go(in);
  CHECK(r.st.active[A_SOURCE_HOT]);
  in.heaterTemp = 29.5f; in.nowMs = MIN; r.go(in);   // onder max, maar niet 1 C eronder
  CHECK(r.find(A_SOURCE_HOT) == nullptr);
  CHECK(r.st.active[A_SOURCE_HOT]);
  in.heaterTemp = 29.0f; in.nowMs = 2 * MIN; r.go(in);  // grens: nog niet < max-1
  CHECK(r.st.active[A_SOURCE_HOT]);
  in.heaterTemp = 28.9f; in.nowMs = 3 * MIN; r.go(in);
  const AlarmEvent* e = r.find(A_SOURCE_HOT);
  CHECK(e != nullptr);
  if (e) { CHECK(e->transition); CHECK(!e->active); }
  // daarna opnieuw boven max: direct weer actief
  in.heaterTemp = 30.1f; in.nowMs = 4 * MIN; r.go(in);
  CHECK(r.st.active[A_SOURCE_HOT]);
}
