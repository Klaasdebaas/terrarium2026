#include "minitest.h"
#include "logformat.h"
#include <string.h>

static SystemState baseState() {
  SystemState s; memset(&s, 0, sizeof s);
  s.time = {2026, 10, 31, 14, 25, 0}; s.timeValid = true;
  s.airTemp = 28.6f; s.rh = 82.1f; s.heaterTemp = 27.1f; s.dhtOk = true; s.dsOk = true;
  s.relayOn[R_HEAT] = true; s.relayOn[R_LIGHT] = true;
  return s;
}

TEST(day_file_name) {
  char b[32]; LocalTime t = {2026, 10, 31, 1, 2, 3};
  formatDayFileName(b, sizeof b, t);
  CHECK_STR(b, "/log/2026-10-31.csv");
}

TEST(measurement_header) {
  CHECK_STR(measurementHeader(), "tijd;lucht_c;rv_pct;bron_c;r1_verw;r2_bevocht;r3_vent;r4_licht;alarmen");
}

TEST(event_header) {
  CHECK_STR(eventHeader(), "tijd;soort;omschrijving");
}

TEST(measurement_line) {
  char b[160]; SystemState s = baseState();
  formatMeasurementLine(b, sizeof b, s);
  CHECK_STR(b, "2026-10-31 14:25:00;28.6;82.1;27.1;1;0;0;1;");
}

TEST(measurement_invalid_and_alarms) {
  char b[160]; SystemState s = baseState();
  s.dhtOk = false; s.alarmActive[A_TEMP_HIGH] = true; s.alarmActive[A_SD_FAIL] = true;
  formatMeasurementLine(b, sizeof b, s);
  CHECK_STR(b, "2026-10-31 14:25:00;;;27.1;1;0;0;1;temp_high,sd_fail");
  s.dsOk = false;
  formatMeasurementLine(b, sizeof b, s);
  CHECK_STR(b, "2026-10-31 14:25:00;;;;1;0;0;1;temp_high,sd_fail");
}

TEST(event_line) {
  char b[160]; LocalTime t = {2026, 10, 31, 14, 23, 5};
  formatEventLine(b, sizeof b, t, "ALARM", "temp_high 30.4C");
  CHECK_STR(b, "2026-10-31 14:23:05;ALARM;temp_high 30.4C");
}

TEST(safe_names) {
  CHECK(isSafeLogName("2026-10-31.csv"));
  CHECK(isSafeLogName("events.csv"));
  CHECK(!isSafeLogName("../config.json"));
  CHECK(!isSafeLogName("x/y.csv"));
  CHECK(!isSafeLogName("config.json"));
  CHECK(!isSafeLogName("a.csv.bak"));
  CHECK(!isSafeLogName(""));
  CHECK(!isSafeLogName(".csv"));
  CHECK(!isSafeLogName("..csv"));
  CHECK(!isSafeLogName("a..b.csv"));
  CHECK(!isSafeLogName(nullptr));
  CHECK(isSafeLogName("abcdefghijklmnopqrstuvwxyz01.csv"));  // 32 tekens
  CHECK(!isSafeLogName("abcdefghijklmnopqrstuvwxyz012.csv")); // 33 tekens
}

TEST(expired) {
  LocalTime today = {2026, 10, 31, 0, 0, 0};
  CHECK(isExpired("2025-10-30.csv", today, 365));
  CHECK(!isExpired("2025-10-31.csv", today, 365));
  CHECK(!isExpired("events.csv", today, 365));
  CHECK(!isExpired("last.txt", today, 365));
  CHECK(!isExpired("2025-10-30.csv.bak", today, 365));
  CHECK(!isExpired("2025-13-30.csv", today, 365));
  CHECK(isExpired("2026-10-30.csv", today, 0));
  CHECK(!isExpired("2026-10-31.csv", today, 0));
}

TEST(days_from_civil) {
  CHECK_EQ(daysFromCivil(1970, 1, 1), 0);
  CHECK_EQ(daysFromCivil(2000, 3, 1), 11017);
  CHECK_EQ(daysFromCivil(2026, 10, 31) - daysFromCivil(2025, 10, 31), 365);
  CHECK_EQ(daysFromCivil(2024, 3, 1) - daysFromCivil(2024, 2, 28), 2);
}

TEST(event_file_name_monthly) {
  char b[32]; LocalTime t = {2026, 10, 31, 1, 2, 3};
  formatEventFileName(b, sizeof b, t);
  CHECK_STR(b, "/log/events-2026-10.csv");
  CHECK(isSafeLogName(b + 5));
  LocalTime jan = {2027, 1, 1, 0, 0, 0};
  formatEventFileName(b, sizeof b, jan);
  CHECK_STR(b, "/log/events-2027-01.csv");
}

TEST(expired_monthly) {
  LocalTime today = {2026, 10, 31, 0, 0, 0};
  // leeftijd telt vanaf de 1e van de volgende maand
  CHECK(isExpiredMonthly("events-2025-09.csv", today, 365));    // 2025-10-01: 395 dagen
  CHECK(!isExpiredMonthly("events-2025-10.csv", today, 365));   // 2025-11-01: 364 dagen
  CHECK(!isExpiredMonthly("events-2026-10.csv", today, 0));     // lopende maand nooit
  CHECK(isExpiredMonthly("events-2026-09.csv", today, 0));
  CHECK(!isExpiredMonthly("events-2026-09.csv", today, 30));    // 2026-10-01: precies 30
  CHECK(isExpiredMonthly("events-2026-09.csv", today, 29));
  LocalTime jan = {2026, 1, 2, 0, 0, 0};
  CHECK(isExpiredMonthly("events-2025-12.csv", jan, 0));        // jaarovergang: 2026-01-01
  CHECK(!isExpiredMonthly("events-2025-12.csv", jan, 1));
  // geen maandbestand
  CHECK(!isExpiredMonthly("events.csv", today, 0));
  CHECK(!isExpiredMonthly("events-0000-00.csv", today, 0));
  CHECK(!isExpiredMonthly("events-2025-13.csv", today, 0));
  CHECK(!isExpiredMonthly("events-2025-1.csv", today, 0));
  CHECK(!isExpiredMonthly("events-2025-01.csv.bak", today, 0));
  CHECK(!isExpiredMonthly("2025-01-01.csv", today, 0));
  CHECK(!isExpiredMonthly(nullptr, today, 0));
  // dagbestanden-check raakt maandbestanden niet
  CHECK(!isExpired("events-2020-01.csv", today, 0));
}
