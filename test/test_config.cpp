#include "minitest.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

static Config defs() { Config c; setDefaults(c); return c; }

TEST(defaults) {
  Config c = defs();
  CHECK_EQ(c.settings.heatDayC, 29);
  CHECK_EQ(c.settings.dayStartMin, 480);
  CHECK_STR(c.cred.hostname, "terrarium");
  CHECK_STR(c.cred.webUser, "admin");
  CHECK_STR(c.ntpServer, "pool.ntp.org");
  CHECK_STR(c.tz, "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(c.cred.wifiSsid, "");
}
TEST(parse_full) {
  Config c = defs();
  ConfigResult r = parseConfig(
      "{\"heating\":{\"day_c\":28,\"night_c\":21.5,\"hyst_c\":1,\"source_max_c\":31},"
      "\"schedule\":{\"day_start\":\"07:30\",\"night_start\":\"21:00\"},"
      "\"wifi\":{\"ssid\":\"net\",\"password\":\"pw\",\"hostname\":\"terra\"},"
      "\"sms\":{\"phone\":\"+31612345678\",\"sim_pin\":\"1234\",\"limit_min\":30},"
      "\"relay_active_low\":false,\"log\":{\"interval_min\":10,\"retention_days\":30},"
      "\"ntp\":{\"server\":\"ntp.example.org\",\"tz\":\"UTC0\"}}", c, false);
  CHECK(r.ok); CHECK_EQ(r.rejected, 0);
  CHECK_EQ(c.settings.heatDayC, 28); CHECK_NEAR(c.settings.heatNightC, 21.5, 1e-6);
  CHECK_EQ(c.settings.dayStartMin, 450); CHECK_EQ(c.settings.nightStartMin, 1260);
  CHECK_STR(c.cred.wifiSsid, "net"); CHECK_STR(c.cred.simPin, "1234");
  CHECK_STR(c.cred.smsPhone, "+31612345678"); CHECK_EQ(c.settings.smsLimitMin, 30);
  CHECK(!c.settings.relayActiveLow); CHECK_EQ(c.settings.retentionDays, 30);
  CHECK_STR(c.ntpServer, "ntp.example.org"); CHECK_STR(c.tz, "UTC0");
}
TEST(parse_partial) {
  Config c = defs();
  ConfigResult r = parseConfig("{\"heating\":{\"night_c\":21}}", c, false);
  CHECK(r.ok); CHECK_EQ(r.rejected, 0);
  CHECK_EQ(c.settings.heatNightC, 21); CHECK_EQ(c.settings.heatDayC, 29);
}
TEST(parse_garbage) {
  Config c = defs();
  ConfigResult r = parseConfig("{niet json", c, false);
  CHECK(!r.ok); CHECK(r.error[0] != 0);
  CHECK_EQ(c.settings.heatDayC, 29);
  CHECK(!parseConfig("[1,2]", c, false).ok);
}
TEST(parse_wrong_type) {
  Config c = defs();
  ConfigResult r = parseConfig("{\"heating\":{\"day_c\":\"warm\"}}", c, false);
  CHECK(r.ok); CHECK_EQ(r.rejected, 1); CHECK_EQ(c.settings.heatDayC, 29);
  CHECK(r.error[0] != 0);
  r = parseConfig("{\"heating\":5}", c, false);
  CHECK_EQ(r.rejected, 1);
  r = parseConfig("{\"humidity\":{\"max_on_min\":10.5}}", c, false);  // geheel getal vereist
  CHECK_EQ(r.rejected, 1); CHECK_EQ(c.settings.humMaxOnMin, 10);
  r = parseConfig("{\"relay_active_low\":1}", c, false);
  CHECK_EQ(r.rejected, 1);
}
TEST(parse_out_of_range) {
  Config c = defs();
  CHECK_EQ(parseConfig("{\"heating\":{\"day_c\":80}}", c, false).rejected, 1);
  CHECK_EQ(c.settings.heatDayC, 29);
  CHECK_EQ(parseConfig("{\"heating\":{\"hyst_c\":0.05}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"humidity\":{\"on_below\":101}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"humidity\":{\"max_on_min\":0}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"humidity\":{\"pause_min\":1441}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"log\":{\"retention_days\":3651}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"log\":{\"retention_days\":3650}}", c, false).rejected, 0);
  CHECK_EQ(c.settings.retentionDays, 3650);
}
TEST(parse_pair_conflict) {
  Config c = defs();
  ConfigResult r = parseConfig("{\"humidity\":{\"on_below\":80,\"off_above\":75}}", c, false);
  CHECK(r.ok); CHECK(r.rejected >= 1);
  CHECK_EQ(c.settings.humOnBelow, 70); CHECK_EQ(c.settings.humOffAbove, 75);
  r = parseConfig("{\"vent\":{\"on_above\":85,\"off_below\":90}}", c, false);
  CHECK(r.rejected >= 1); CHECK_EQ(c.settings.ventOnAbove, 95); CHECK_EQ(c.settings.ventOffBelow, 90);
  r = parseConfig("{\"alarms\":{\"temp_low_c\":31}}", c, false);
  CHECK(r.rejected >= 1); CHECK_EQ(c.settings.tempLowC, 20); CHECK_EQ(c.settings.tempHighC, 30);
  r = parseConfig("{\"alarms\":{\"rh_low\":96}}", c, false);
  CHECK(r.rejected >= 1); CHECK_EQ(c.settings.rhLow, 70);
  r = parseConfig("{\"vent\":{\"duration_min\":120}}", c, false);
  CHECK(r.rejected >= 1); CHECK_EQ(c.settings.ventDurationMin, 5); CHECK_EQ(c.settings.ventIntervalMin, 120);
  r = parseConfig("{\"schedule\":{\"day_start\":\"20:00\"}}", c, false);
  CHECK(r.rejected >= 1); CHECK_EQ(c.settings.dayStartMin, 480);
  // geldige wijziging van een paar gaat wel door
  r = parseConfig("{\"humidity\":{\"on_below\":60,\"off_above\":65}}", c, false);
  CHECK_EQ(r.rejected, 0); CHECK_EQ(c.settings.humOnBelow, 60); CHECK_EQ(c.settings.humOffAbove, 65);
}
TEST(parse_hhmm) {
  uint16_t m = 0;
  CHECK(parseHHMM("08:00", m)); CHECK_EQ(m, 480);
  CHECK(parseHHMM("23:59", m)); CHECK_EQ(m, 1439);
  CHECK(parseHHMM("00:00", m)); CHECK_EQ(m, 0);
  CHECK(!parseHHMM("24:00", m)); CHECK(!parseHHMM("8:0", m));
  CHECK(!parseHHMM("12:60", m)); CHECK(!parseHHMM("12-30", m)); CHECK(!parseHHMM(nullptr, m));
  CHECK(!parseHHMM("12:300", m));
}
TEST(empty_secret_keeps_old) {
  Config c = defs();
  strcpy(c.cred.webPass, "geheim"); strcpy(c.cred.wifiPass, "wifi"); strcpy(c.cred.simPin, "1111");
  ConfigResult r = parseConfig(
      "{\"web\":{\"password\":\"\"},\"wifi\":{\"password\":\"\"},\"sms\":{\"sim_pin\":\"\"}}", c, true);
  CHECK_EQ(r.rejected, 0);
  CHECK_STR(c.cred.webPass, "geheim"); CHECK_STR(c.cred.wifiPass, "wifi"); CHECK_STR(c.cred.simPin, "1111");
  parseConfig("{\"web\":{\"password\":\"\"}}", c, false);
  CHECK_STR(c.cred.webPass, "");
  parseConfig("{\"web\":{\"password\":\"nieuw\"},\"wifi\":{\"ssid\":\"\"}}", c, true);
  CHECK_STR(c.cred.webPass, "nieuw"); CHECK_STR(c.cred.wifiSsid, "");
}
TEST(string_too_long) {
  Config c = defs();
  char json[200];
  snprintf(json, sizeof json, "{\"wifi\":{\"ssid\":\"%s\"}}", "0123456789012345678901234567890123");  // 34 > 32
  CHECK_EQ(parseConfig(json, c, false).rejected, 1);
  CHECK_STR(c.cred.wifiSsid, "");
  snprintf(json, sizeof json, "{\"wifi\":{\"ssid\":\"%s\"}}", "01234567890123456789012345678901");  // 32
  CHECK_EQ(parseConfig(json, c, false).rejected, 0);
  CHECK_STR(c.cred.wifiSsid, "01234567890123456789012345678901");
}
TEST(phone_validation) {
  Config c = defs();
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"+31600000000\"}}", c, false).rejected, 0);
  CHECK_STR(c.cred.smsPhone, "+31600000000");
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"0612345678\"}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"+316XXXXXXXX\"}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"+1234567\"}}", c, false).rejected, 1);
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"+1234567890123456\"}}", c, false).rejected, 1);
  CHECK_STR(c.cred.smsPhone, "+31600000000");
  CHECK_EQ(parseConfig("{\"sms\":{\"phone\":\"\"}}", c, false).rejected, 0);  // leeg = geen SMS
  CHECK_STR(c.cred.smsPhone, "");
}
TEST(serialize_no_secrets) {
  Config c = defs();
  strcpy(c.cred.webPass, "geheim"); strcpy(c.cred.wifiPass, "wifigeheim"); strcpy(c.cred.simPin, "4321");
  char buf[2048];
  size_t n = serializeConfig(c, buf, sizeof buf, false);
  CHECK(n > 0); CHECK_EQ(n, strlen(buf));
  CHECK(strstr(buf, "geheim") == nullptr); CHECK(strstr(buf, "4321") == nullptr);
  CHECK(strstr(buf, "day_c") != nullptr); CHECK(strstr(buf, "\"password\"") == nullptr);
  CHECK(strstr(buf, "sim_pin") == nullptr);
  char tiny[10];
  CHECK_EQ(serializeConfig(c, tiny, sizeof tiny, true), (size_t)0);
}
TEST(roundtrip) {
  Config a = defs();
  a.settings.heatDayC = 27.5f; a.settings.humOnBelow = 55; a.settings.humOffAbove = 80;
  a.settings.dayStartMin = 390; a.settings.relayActiveLow = false; a.settings.retentionDays = 90;
  strcpy(a.cred.wifiSsid, "ssid"); strcpy(a.cred.wifiPass, "pw\"q\\"); strcpy(a.cred.smsPhone, "+31612345678");
  strcpy(a.cred.simPin, "0000"); strcpy(a.cred.webPass, "w");
  char buf[2048];
  CHECK(serializeConfig(a, buf, sizeof buf, true) > 0);
  Config b = defs();
  ConfigResult r = parseConfig(buf, b, false);
  CHECK(r.ok); CHECK_EQ(r.rejected, 0);
  CHECK(memcmp(&a.settings, &b.settings, sizeof a.settings) == 0 || (
    a.settings.heatDayC == b.settings.heatDayC && a.settings.humOnBelow == b.settings.humOnBelow &&
    a.settings.humOffAbove == b.settings.humOffAbove && a.settings.dayStartMin == b.settings.dayStartMin &&
    a.settings.nightStartMin == b.settings.nightStartMin && a.settings.relayActiveLow == b.settings.relayActiveLow &&
    a.settings.retentionDays == b.settings.retentionDays && a.settings.heatNightC == b.settings.heatNightC &&
    a.settings.heatHystC == b.settings.heatHystC && a.settings.tempLowC == b.settings.tempLowC));
  CHECK(memcmp(&a.cred, &b.cred, sizeof a.cred) == 0 || (
    strcmp(a.cred.wifiPass, b.cred.wifiPass) == 0 && strcmp(a.cred.wifiSsid, b.cred.wifiSsid) == 0 &&
    strcmp(a.cred.smsPhone, b.cred.smsPhone) == 0 && strcmp(a.cred.simPin, b.cred.simPin) == 0 &&
    strcmp(a.cred.webPass, b.cred.webPass) == 0 && strcmp(a.cred.hostname, b.cred.hostname) == 0 &&
    strcmp(a.cred.webUser, b.cred.webUser) == 0));
  CHECK_STR(b.cred.wifiPass, "pw\"q\\");
  CHECK_NEAR(b.settings.heatDayC, 27.5, 1e-6);
  CHECK_STR(b.ntpServer, a.ntpServer); CHECK_STR(b.tz, a.tz);
}
TEST(crc32_vector) {
  CHECK_EQ(crc32((const uint8_t*)"123456789", 9), 0xCBF43926u);
}
TEST(sd_template_parses) {
  FILE* f = fopen("../sd-template/config.json", "rb");
  CHECK(f != nullptr);
  if (!f) return;
  static char buf[4096];
  size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
  Config c = defs();
  ConfigResult r = parseConfig(buf, c, false);
  CHECK(r.ok);
  CHECK_EQ(r.rejected, 1);  // enkel de plaatshouder-telefoon "+316XXXXXXXX"
  CHECK_STR(c.cred.smsPhone, "");
  CHECK_STR(c.cred.hostname, "terrarium"); CHECK_EQ(c.settings.heatDayC, 29);
}
