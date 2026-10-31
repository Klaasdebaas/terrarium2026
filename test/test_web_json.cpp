#include "minitest.h"
#include "web_json.h"
#include <ArduinoJson.h>
#include <string.h>

static SystemState base() {
  SystemState s; memset(&s, 0, sizeof(s));
  s.time = {2026, 10, 31, 14, 23, 5}; s.timeValid = true; s.isDay = true;
  s.airTemp = 28.64f; s.rh = 82.1f; s.heaterTemp = 27.1f; s.dhtOk = true; s.dsOk = true;
  s.relayOn[R_HEAT] = true; s.relayOn[R_VENT] = true; s.relayOn[R_LIGHT] = true;
  s.relayMode[R_VENT] = RelayMode::ManualOn; s.manualRemainingS[R_VENT] = 1500;
  s.alarmActive[A_TEMP_HIGH] = true;
  s.wifiOk = true; s.wifiRssi = -61; strcpy(s.ip, "192.168.1.42");
  s.gsmOk = true; s.gsmSignal = 18; s.sdOk = true; s.rtcOk = true; s.uptimeS = 86400;
  return s;
}

TEST(status_json_fields) {
  SystemState s = base(); static char buf[1024];
  size_t n = buildStatusJson(s, buf, sizeof(buf));
  CHECK(n > 0 && n == strlen(buf));
  JsonDocument d; CHECK(deserializeJson(d, buf) == DeserializationError::Ok);
  CHECK_STR(d["time"] | "", "2026-10-31T14:23:05");
  CHECK(d["is_day"].as<bool>());
  CHECK_NEAR(d["air_temp"].as<double>(), 28.6, 1e-4);
  CHECK_NEAR(d["rh"].as<double>(), 82.1, 1e-4);
  CHECK_NEAR(d["heater_temp"].as<double>(), 27.1, 1e-4);
  CHECK(d["dht_ok"].as<bool>() && d["ds_ok"].as<bool>());
  CHECK_EQ(d["relays"].size(), 4u);
  CHECK_STR(d["relays"][0]["name"] | "", "verwarming");
  CHECK_STR(d["relays"][1]["name"] | "", "bevochtiging");
  CHECK_STR(d["relays"][2]["name"] | "", "ventilatie");
  CHECK_STR(d["relays"][3]["name"] | "", "verlichting");
  CHECK(d["relays"][0]["on"].as<bool>());
  CHECK_STR(d["relays"][0]["mode"] | "", "auto");
  CHECK_EQ(d["relays"][0]["manual_remaining_s"].as<int>(), 0);
  CHECK_STR(d["relays"][2]["mode"] | "", "manual");
  CHECK_EQ(d["relays"][2]["manual_remaining_s"].as<int>(), 1500);
  CHECK_EQ(d["alarms"].size(), 1u);
  CHECK_STR(d["alarms"][0] | "", "temp_high");
  CHECK(d["wifi"]["ok"].as<bool>());
  CHECK_EQ(d["wifi"]["rssi"].as<int>(), -61);
  CHECK_STR(d["wifi"]["ip"] | "", "192.168.1.42");
  CHECK(d["gsm"]["ok"].as<bool>());
  CHECK_EQ(d["gsm"]["signal"].as<int>(), 18);
  CHECK(d["sd_ok"].as<bool>() && d["rtc_ok"].as<bool>());
  CHECK(d["uptime_s"].as<uint32_t>() == 86400u);
}

TEST(status_json_null_when_invalid) {
  SystemState s = base(); static char buf[1024];
  s.dhtOk = false; s.dsOk = false; s.timeValid = false;
  CHECK(buildStatusJson(s, buf, sizeof(buf)) > 0);
  JsonDocument d; CHECK(deserializeJson(d, buf) == DeserializationError::Ok);
  CHECK(d["air_temp"].isNull()); CHECK(d["rh"].isNull()); CHECK(d["heater_temp"].isNull());
  CHECK(d["time"].isNull());
  CHECK(!d["dht_ok"].as<bool>());
}

TEST(status_json_buffer_small) {
  SystemState s = base(); char buf[40]; memset(buf, 'x', sizeof(buf));
  CHECK(buildStatusJson(s, buf, 32) == 0);
  CHECK(buf[0] == 0);
  CHECK(buf[32] == 'x');  // niet voorbij len geschreven
  CHECK(buildStatusJson(s, nullptr, 0) == 0);
}

TEST(relay_cmd) {
  RelayId r = R_HEAT; RelayMode m = RelayMode::Auto;
  CHECK(parseRelayCommand("{\"relay\":3,\"mode\":\"on\"}", r, m));
  CHECK_EQ(r, R_VENT); CHECK(m == RelayMode::ManualOn);
  CHECK(parseRelayCommand("{\"relay\":1,\"mode\":\"off\"}", r, m));
  CHECK_EQ(r, R_HEAT); CHECK(m == RelayMode::ManualOff);
  CHECK(parseRelayCommand("{\"relay\":4,\"mode\":\"auto\"}", r, m));
  CHECK_EQ(r, R_LIGHT); CHECK(m == RelayMode::Auto);
  CHECK(!parseRelayCommand("{\"relay\":0,\"mode\":\"on\"}", r, m));
  CHECK(!parseRelayCommand("{\"relay\":5,\"mode\":\"on\"}", r, m));
  CHECK(!parseRelayCommand("{\"relay\":1,\"mode\":\"aan\"}", r, m));
  CHECK(!parseRelayCommand("{\"relay\":\"1\",\"mode\":\"on\"}", r, m));
  CHECK(!parseRelayCommand("{\"mode\":\"on\"}", r, m));
  CHECK(!parseRelayCommand("garbage", r, m));
  CHECK(!parseRelayCommand(nullptr, r, m));
}

TEST(iso_local) {
  LocalTime t;
  CHECK(parseIsoLocal("2026-10-31T14:23:00", t));
  CHECK_EQ(t.year, 2026); CHECK_EQ(t.month, 10); CHECK_EQ(t.day, 31);
  CHECK_EQ(t.hour, 14); CHECK_EQ(t.minute, 23); CHECK_EQ(t.second, 0);
  CHECK(!parseIsoLocal("2026-02-30T10:00:00", t));
  CHECK(!parseIsoLocal("2026-10-31 14:23", t));
  CHECK(!parseIsoLocal("2026-10-31 14:23:00", t));
  CHECK(!parseIsoLocal("2026-10-31T14:23", t));
  CHECK(!parseIsoLocal("2026-10-31T14:23:00Z", t));
  CHECK(parseIsoLocal("2028-02-29T00:00:00", t));
  CHECK(!parseIsoLocal("2026-02-29T00:00:00", t));
  CHECK(!parseIsoLocal("2100-02-29T00:00:00", t));
  CHECK(parseIsoLocal("2000-02-29T23:59:59", t));
  CHECK(!parseIsoLocal("2026-13-01T00:00:00", t));
  CHECK(!parseIsoLocal("2026-00-01T00:00:00", t));
  CHECK(!parseIsoLocal("2026-04-31T00:00:00", t));
  CHECK(!parseIsoLocal("2026-10-31T24:00:00", t));
  CHECK(!parseIsoLocal("2026-10-31T10:60:00", t));
  CHECK(!parseIsoLocal("2026-10-31T10:00:60", t));
  CHECK(!parseIsoLocal("2026-10-3aT10:00:00", t));
  CHECK(!parseIsoLocal(nullptr, t));
}

TEST(setup_request) {
  char ssid[33], pass[65], web[33];
  CHECK(parseSetupRequest("{\"ssid\":\"Thuis\",\"password\":\"geheim123\",\"web_password\":\"abcdef\"}",
                          ssid, sizeof(ssid), pass, sizeof(pass), web, sizeof(web)));
  CHECK_STR(ssid, "Thuis"); CHECK_STR(pass, "geheim123"); CHECK_STR(web, "abcdef");
  CHECK(!parseSetupRequest("{\"ssid\":\"Thuis\",\"password\":\"x\",\"web_password\":\"abc\"}",
                           ssid, sizeof(ssid), pass, sizeof(pass), web, sizeof(web)));
  CHECK(!parseSetupRequest("{\"ssid\":\"\",\"password\":\"x\",\"web_password\":\"abcdef\"}",
                           ssid, sizeof(ssid), pass, sizeof(pass), web, sizeof(web)));
  CHECK(!parseSetupRequest("{\"password\":\"x\",\"web_password\":\"abcdef\"}",
                           ssid, sizeof(ssid), pass, sizeof(pass), web, sizeof(web)));
  // te lang voor buffer
  char small[4];
  CHECK(!parseSetupRequest("{\"ssid\":\"Thuis\",\"password\":\"x\",\"web_password\":\"abcdef\"}",
                           small, sizeof(small), pass, sizeof(pass), web, sizeof(web)));
  // open netwerk: leeg wachtwoord toegestaan
  CHECK(parseSetupRequest("{\"ssid\":\"Open\",\"web_password\":\"abcdef\"}",
                          ssid, sizeof(ssid), pass, sizeof(pass), web, sizeof(web)));
  CHECK_STR(pass, "");
}
