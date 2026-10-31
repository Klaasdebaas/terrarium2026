// Pure configuratielogica: JSON <-> Config, validatie en CRC32. Geen hardware.
#include "config.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

void setDefaults(Config& c) { c = Config(); }

bool parseHHMM(const char* s, uint16_t& minutes) {
  if (!s || strlen(s) != 5 || s[2] != ':') return false;
  for (int i : {0, 1, 3, 4}) if (s[i] < '0' || s[i] > '9') return false;
  int h = (s[0] - '0') * 10 + (s[1] - '0'), m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  minutes = (uint16_t)(h * 60 + m);
  return true;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

namespace {

struct Parser {
  ConfigResult res;
  bool keepOldSecret;

  void reject(const char* group, const char* key, const char* why) {
    if (res.rejected < 255) res.rejected++;
    if (res.error[0] == 0) snprintf(res.error, sizeof res.error, "%s.%s: %s", group, key, why);
  }
  void rejectPair(const char* what) {
    if (res.rejected < 255) res.rejected++;
    if (res.error[0] == 0) snprintf(res.error, sizeof res.error, "%s", what);
  }

  // Geeft het object van een groep (of null-object als afwezig). Verkeerd type: afgekeurd.
  JsonObjectConst group(JsonObjectConst root, const char* g) {
    JsonVariantConst v = root[g];
    if (v.isNull()) return JsonObjectConst();
    if (!v.is<JsonObjectConst>()) { reject(g, "(groep)", "geen object"); return JsonObjectConst(); }
    return v.as<JsonObjectConst>();
  }

  void num(JsonVariantConst v, const char* g, const char* k, float& out, float lo, float hi) {
    if (v.isNull()) return;
    if (!v.is<float>() || v.is<bool>()) { reject(g, k, "geen getal"); return; }
    float f = v.as<float>();
    if (!(f >= lo && f <= hi)) { reject(g, k, "buiten bereik"); return; }
    out = f;
  }
  void minutes(JsonVariantConst v, const char* g, const char* k, uint16_t& out, int lo = 1, int hi = 1440) {
    if (v.isNull()) return;
    if (!v.is<int32_t>() || v.is<bool>()) { reject(g, k, "geen geheel getal"); return; }
    int32_t n = v.as<int32_t>();
    if (n < lo || n > hi) { reject(g, k, "buiten bereik"); return; }
    out = (uint16_t)n;
  }
  void flag(JsonVariantConst v, const char* g, const char* k, bool& out) {
    if (v.isNull()) return;
    if (!v.is<bool>()) { reject(g, k, "geen boolean"); return; }
    out = v.as<bool>();
  }
  // secret: lege waarde behoudt de oude (indien gevraagd).
  void str(JsonVariantConst v, const char* g, const char* k, char* out, size_t cap, bool secret = false) {
    if (v.isNull()) return;
    if (!v.is<const char*>()) { reject(g, k, "geen tekst"); return; }
    const char* s = v.as<const char*>();
    if (strlen(s) >= cap) { reject(g, k, "te lang"); return; }
    if (secret && keepOldSecret && s[0] == 0) return;
    strcpy(out, s);
  }
  void hhmm(JsonVariantConst v, const char* g, const char* k, uint16_t& out) {
    if (v.isNull()) return;
    if (!v.is<const char*>()) { reject(g, k, "geen tekst"); return; }
    uint16_t m;
    if (!parseHHMM(v.as<const char*>(), m)) { reject(g, k, "geen HH:MM"); return; }
    out = m;
  }
  void phone(JsonVariantConst v, const char* g, const char* k, char* out, size_t cap) {
    if (v.isNull()) return;
    if (!v.is<const char*>()) { reject(g, k, "geen tekst"); return; }
    const char* s = v.as<const char*>();
    if (s[0] != 0) {  // leeg = geen SMS-ontvanger
      if (s[0] != '+') { reject(g, k, "moet met + beginnen"); return; }
      size_t digits = 0;
      for (const char* p = s + 1; *p; p++) {
        if (*p < '0' || *p > '9') { reject(g, k, "alleen cijfers na +"); return; }
        digits++;
      }
      if (digits < 8 || digits > 15) { reject(g, k, "8-15 cijfers"); return; }
    }
    if (strlen(s) >= cap) { reject(g, k, "te lang"); return; }
    strcpy(out, s);
  }
};

}  // namespace

ConfigResult parseConfig(const char* json, Config& c, bool emptySecretKeepsOld) {
  Parser p;
  p.res.ok = true; p.res.rejected = 0; p.res.error[0] = 0;
  p.keepOldSecret = emptySecretKeepsOld;

  JsonDocument doc;
  DeserializationError err = json ? deserializeJson(doc, json) : DeserializationError::EmptyInput;
  if (err || !doc.is<JsonObject>()) {
    p.res.ok = false;
    snprintf(p.res.error, sizeof p.res.error, "JSON onleesbaar: %s", err ? err.c_str() : "geen object");
    return p.res;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  Config w = c;  // werkkopie; c blijft ongewijzigd tot het eind
  Settings& s = w.settings;
  const Settings& o = c.settings;

  JsonObjectConst g = p.group(root, "wifi");
  p.str(g["ssid"], "wifi", "ssid", w.cred.wifiSsid, sizeof w.cred.wifiSsid);
  p.str(g["password"], "wifi", "password", w.cred.wifiPass, sizeof w.cred.wifiPass, true);
  p.str(g["hostname"], "wifi", "hostname", w.cred.hostname, sizeof w.cred.hostname);

  g = p.group(root, "web");
  p.str(g["user"], "web", "user", w.cred.webUser, sizeof w.cred.webUser);
  p.str(g["password"], "web", "password", w.cred.webPass, sizeof w.cred.webPass, true);

  g = p.group(root, "sms");
  p.phone(g["phone"], "sms", "phone", w.cred.smsPhone, sizeof w.cred.smsPhone);
  p.str(g["sim_pin"], "sms", "sim_pin", w.cred.simPin, sizeof w.cred.simPin, true);
  p.minutes(g["limit_min"], "sms", "limit_min", s.smsLimitMin);

  g = p.group(root, "schedule");
  p.hhmm(g["day_start"], "schedule", "day_start", s.dayStartMin);
  p.hhmm(g["night_start"], "schedule", "night_start", s.nightStartMin);

  g = p.group(root, "heating");
  p.num(g["day_c"], "heating", "day_c", s.heatDayC, 0, 45);
  p.num(g["night_c"], "heating", "night_c", s.heatNightC, 0, 45);
  p.num(g["hyst_c"], "heating", "hyst_c", s.heatHystC, 0.1f, 5);
  p.num(g["source_max_c"], "heating", "source_max_c", s.sourceMaxC, 0, 45);

  g = p.group(root, "humidity");
  p.num(g["on_below"], "humidity", "on_below", s.humOnBelow, 0, 100);
  p.num(g["off_above"], "humidity", "off_above", s.humOffAbove, 0, 100);
  p.minutes(g["max_on_min"], "humidity", "max_on_min", s.humMaxOnMin);
  p.minutes(g["pause_min"], "humidity", "pause_min", s.humPauseMin);

  g = p.group(root, "vent");
  p.num(g["on_above"], "vent", "on_above", s.ventOnAbove, 0, 100);
  p.num(g["off_below"], "vent", "off_below", s.ventOffBelow, 0, 100);
  p.minutes(g["interval_min"], "vent", "interval_min", s.ventIntervalMin);
  p.minutes(g["duration_min"], "vent", "duration_min", s.ventDurationMin);

  g = p.group(root, "alarms");
  p.num(g["temp_low_c"], "alarms", "temp_low_c", s.tempLowC, 0, 45);
  p.num(g["temp_high_c"], "alarms", "temp_high_c", s.tempHighC, 0, 45);
  p.minutes(g["temp_delay_min"], "alarms", "temp_delay_min", s.tempDelayMin);
  p.num(g["rh_low"], "alarms", "rh_low", s.rhLow, 0, 100);
  p.num(g["rh_high"], "alarms", "rh_high", s.rhHigh, 0, 100);
  p.minutes(g["rh_delay_min"], "alarms", "rh_delay_min", s.rhDelayMin);
  p.minutes(g["wifi_lost_min"], "alarms", "wifi_lost_min", s.wifiLostMin);

  p.minutes(root["manual_timeout_min"], "manual_timeout_min", "", s.manualTimeoutMin);
  p.flag(root["relay_active_low"], "relay_active_low", "", s.relayActiveLow);

  g = p.group(root, "log");
  p.minutes(g["interval_min"], "log", "interval_min", s.logIntervalMin);
  p.minutes(g["retention_days"], "log", "retention_days", s.retentionDays, 1, 3650);

  g = p.group(root, "ntp");
  p.str(g["server"], "ntp", "server", w.ntpServer, sizeof w.ntpServer);
  p.str(g["tz"], "ntp", "tz", w.tz, sizeof w.tz);

  // Paarregels op de resulterende waarden; bij schending beide velden terug naar de oude waarde.
  if (!(s.humOffAbove > s.humOnBelow)) {
    p.rejectPair("humidity: off_above moet groter zijn dan on_below");
    s.humOnBelow = o.humOnBelow; s.humOffAbove = o.humOffAbove;
  }
  if (!(s.ventOffBelow < s.ventOnAbove)) {
    p.rejectPair("vent: off_below moet kleiner zijn dan on_above");
    s.ventOnAbove = o.ventOnAbove; s.ventOffBelow = o.ventOffBelow;
  }
  if (!(s.tempLowC < s.tempHighC)) {
    p.rejectPair("alarms: temp_low_c moet kleiner zijn dan temp_high_c");
    s.tempLowC = o.tempLowC; s.tempHighC = o.tempHighC;
  }
  if (!(s.rhLow < s.rhHigh)) {
    p.rejectPair("alarms: rh_low moet kleiner zijn dan rh_high");
    s.rhLow = o.rhLow; s.rhHigh = o.rhHigh;
  }
  if (!(s.ventDurationMin < s.ventIntervalMin)) {
    p.rejectPair("vent: duration_min moet kleiner zijn dan interval_min");
    s.ventDurationMin = o.ventDurationMin; s.ventIntervalMin = o.ventIntervalMin;
  }
  if (s.dayStartMin == s.nightStartMin) {
    p.rejectPair("schedule: day_start en night_start mogen niet gelijk zijn");
    s.dayStartMin = o.dayStartMin; s.nightStartMin = o.nightStartMin;
  }

  c = w;
  return p.res;
}

static void hhmmStr(uint16_t m, char* out) { snprintf(out, 6, "%02u:%02u", (unsigned)((m / 60) % 24), (unsigned)(m % 60)); }

size_t serializeConfig(const Config& c, char* buf, size_t len, bool includeSecrets) {
  if (!buf || len == 0) return 0;
  buf[0] = 0;
  const Settings& s = c.settings;
  JsonDocument doc;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = c.cred.wifiSsid;
  if (includeSecrets) wifi["password"] = c.cred.wifiPass;
  wifi["hostname"] = c.cred.hostname;
  JsonObject web = doc["web"].to<JsonObject>();
  web["user"] = c.cred.webUser;
  if (includeSecrets) web["password"] = c.cred.webPass;
  JsonObject sms = doc["sms"].to<JsonObject>();
  sms["phone"] = c.cred.smsPhone;
  if (includeSecrets) sms["sim_pin"] = c.cred.simPin;
  sms["limit_min"] = s.smsLimitMin;
  char t[6];
  JsonObject sch = doc["schedule"].to<JsonObject>();
  hhmmStr(s.dayStartMin, t); sch["day_start"] = t;      // ArduinoJson kopieert char[]-waarden
  hhmmStr(s.nightStartMin, t); sch["night_start"] = t;
  JsonObject h = doc["heating"].to<JsonObject>();
  h["day_c"] = s.heatDayC; h["night_c"] = s.heatNightC; h["hyst_c"] = s.heatHystC; h["source_max_c"] = s.sourceMaxC;
  JsonObject hu = doc["humidity"].to<JsonObject>();
  hu["on_below"] = s.humOnBelow; hu["off_above"] = s.humOffAbove;
  hu["max_on_min"] = s.humMaxOnMin; hu["pause_min"] = s.humPauseMin;
  JsonObject v = doc["vent"].to<JsonObject>();
  v["on_above"] = s.ventOnAbove; v["off_below"] = s.ventOffBelow;
  v["interval_min"] = s.ventIntervalMin; v["duration_min"] = s.ventDurationMin;
  JsonObject a = doc["alarms"].to<JsonObject>();
  a["temp_low_c"] = s.tempLowC; a["temp_high_c"] = s.tempHighC; a["temp_delay_min"] = s.tempDelayMin;
  a["rh_low"] = s.rhLow; a["rh_high"] = s.rhHigh; a["rh_delay_min"] = s.rhDelayMin;
  a["wifi_lost_min"] = s.wifiLostMin;
  doc["manual_timeout_min"] = s.manualTimeoutMin;
  doc["relay_active_low"] = s.relayActiveLow;
  JsonObject lg = doc["log"].to<JsonObject>();
  lg["interval_min"] = s.logIntervalMin; lg["retention_days"] = s.retentionDays;
  JsonObject ntp = doc["ntp"].to<JsonObject>();
  ntp["server"] = c.ntpServer; ntp["tz"] = c.tz;

  size_t need = measureJson(doc);
  if (need + 1 > len) return 0;
  return serializeJson(doc, buf, len);
}
