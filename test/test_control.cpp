#include "minitest.h"
#include "control.h"

static ControlInput base() {
  ControlInput in;
  in.nowMs = 0; in.minuteOfDay = 610; in.timeValid = true;
  in.airTemp = 25; in.rh = 80; in.heaterTemp = 25; in.dhtOk = true; in.dsOk = true;
  for (int i = 0; i < RELAY_COUNT; i++) in.mode[i] = RelayMode::Auto;
  return in;
}

TEST(isday_normal) { CHECK(isDay(480,480,1200)); CHECK(!isDay(1200,480,1200)); CHECK(!isDay(479,480,1200)); }
TEST(isday_wraps) { CHECK(isDay(1380,1320,360)); CHECK(isDay(100,1320,360)); CHECK(!isDay(600,1320,360)); }
TEST(vent_window) { CHECK(inVentWindow(0,120,5)); CHECK(inVentWindow(124,120,5)); CHECK(!inVentWindow(125,120,5)); }

TEST(heat_day_hyst) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.airTemp = 28.4f; CHECK(decide(in, s, m).on[R_HEAT]);
  in.airTemp = 29.0f; CHECK(decide(in, s, m).on[R_HEAT]);
  in.airTemp = 29.6f; CHECK(!decide(in, s, m).on[R_HEAT]);
  in.airTemp = 29.0f; CHECK(!decide(in, s, m).on[R_HEAT]);
}
TEST(heat_night_target) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.minuteOfDay = 1300;
  in.airTemp = 22.6f; CHECK(!decide(in, s, m).on[R_HEAT]);
  in.airTemp = 21.4f; CHECK(decide(in, s, m).on[R_HEAT]);
}
TEST(heat_source_trip) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.airTemp = 20; in.heaterTemp = 30.1f; CHECK(!decide(in, s, m).on[R_HEAT]);
  in.heaterTemp = 29.5f; CHECK(!decide(in, s, m).on[R_HEAT]);
  in.heaterTemp = 28.9f; CHECK(decide(in, s, m).on[R_HEAT]);
}
TEST(heat_off_on_sensor_fault) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.airTemp = 20; CHECK(decide(in, s, m).on[R_HEAT]);
  in.dhtOk = false; CHECK(!decide(in, s, m).on[R_HEAT]);
  in.dhtOk = true; in.dsOk = false; CHECK(!decide(in, s, m).on[R_HEAT]);
}
TEST(hum_hyst) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.rh = 69; CHECK(decide(in, s, m).on[R_HUMID]);
  in.rh = 72; CHECK(decide(in, s, m).on[R_HUMID]);
  in.rh = 75.5f; CHECK(!decide(in, s, m).on[R_HUMID]);
}

static void humCycle(uint32_t t0) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.rh = 60;
  in.nowMs = t0;                  CHECK(decide(in, s, m).on[R_HUMID]);
  in.nowMs = t0 + 599999u;        CHECK(decide(in, s, m).on[R_HUMID]);
  in.nowMs = t0 + 600000u;        CHECK(!decide(in, s, m).on[R_HUMID]);
  in.nowMs = t0 + 899999u;        CHECK(!decide(in, s, m).on[R_HUMID]);
  in.nowMs = t0 + 900000u;        CHECK(decide(in, s, m).on[R_HUMID]);
}
TEST(hum_max_on_pause) { humCycle(0); }
TEST(hum_max_on_wraps) { humCycle(0xFFFFFF00u); }

TEST(vent_rh_hyst) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.minuteOfDay = 610;
  in.rh = 96; CHECK(decide(in, s, m).on[R_VENT]);
  in.rh = 92; CHECK(decide(in, s, m).on[R_VENT]);
  in.rh = 89; CHECK(!decide(in, s, m).on[R_VENT]);
}
TEST(vent_schedule) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.rh = 80;
  in.minuteOfDay = 720; CHECK(decide(in, s, m).on[R_VENT]);
  in.minuteOfDay = 725; CHECK(!decide(in, s, m).on[R_VENT]);
}
TEST(vent_dht_fault_schedule_only) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.dhtOk = false; in.rh = 99;
  in.minuteOfDay = 610; CHECK(!decide(in, s, m).on[R_VENT]);
  in.minuteOfDay = 720; CHECK(decide(in, s, m).on[R_VENT]);
}
TEST(light_schedule) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.minuteOfDay = 480;  CHECK(decide(in, s, m).on[R_LIGHT]);
  in.minuteOfDay = 1199; CHECK(decide(in, s, m).on[R_LIGHT]);
  in.minuteOfDay = 1200; CHECK(!decide(in, s, m).on[R_LIGHT]);
}
TEST(no_time_safe) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.timeValid = false; in.minuteOfDay = 720; in.rh = 80; in.airTemp = 22.6f;
  ControlOutput o = decide(in, s, m);
  CHECK(!o.on[R_LIGHT]); CHECK(!o.isDay); CHECK(!o.on[R_VENT]);
  CHECK(!o.on[R_HEAT]);            // 22.6 > 22 - 0.5 -> nachtstreefwaarde, niet verwarmen
  in.airTemp = 21.4f; CHECK(decide(in, s, m).on[R_HEAT]);
}
TEST(manual_overrides_auto) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.mode[R_LIGHT] = RelayMode::ManualOff; in.mode[R_VENT] = RelayMode::ManualOn;
  ControlOutput o = decide(in, s, m);
  CHECK(!o.on[R_LIGHT]); CHECK(o.on[R_VENT]);
}
TEST(safety_beats_manual) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.mode[R_HEAT] = RelayMode::ManualOn; in.heaterTemp = 31;
  CHECK(!decide(in, s, m).on[R_HEAT]);
  ControlMemory m2; controlInit(m2); ControlInput in2 = base();
  in2.mode[R_HUMID] = RelayMode::ManualOn; in2.dhtOk = false;
  CHECK(!decide(in2, s, m2).on[R_HUMID]);
}
TEST(override_expires) {
  Overrides o; overridesInit(o);
  setOverride(o, R_VENT, RelayMode::ManualOn, 1000);
  expireOverrides(o, 1000u + 30u * 60000u - 1u, 30); CHECK(o.mode[R_VENT] == RelayMode::ManualOn);
  expireOverrides(o, 1000u + 30u * 60000u, 30);      CHECK(o.mode[R_VENT] == RelayMode::Auto);
}
TEST(override_remaining) {
  Overrides o; overridesInit(o);
  CHECK_EQ(overrideRemainingS(o, R_VENT, 5000, 30), 0u);
  setOverride(o, R_VENT, RelayMode::ManualOn, 5000);
  CHECK_EQ(overrideRemainingS(o, R_VENT, 5000, 30), 1800u);
}
TEST(dst_jump_no_stuck) {
  Settings s; ControlMemory m; controlInit(m); ControlInput in = base();
  in.minuteOfDay = 119; decide(in, s, m);
  in.minuteOfDay = 180; ControlOutput o = decide(in, s, m);
  CHECK(!o.on[R_VENT]); CHECK(!o.on[R_LIGHT] == !isDay(180, s.dayStartMin, s.nightStartMin));
}
