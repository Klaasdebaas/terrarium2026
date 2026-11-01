#ifndef TERRARIUM_CONFIG_H
#define TERRARIUM_CONFIG_H
#include <stdint.h>

// Instellingen met standaardwaarden uit spec 5.1. De rest van config.h volgt in Task 5.
struct Settings {
  uint16_t dayStartMin = 480, nightStartMin = 1200;
  float heatDayC = 29, heatNightC = 22, heatHystC = 0.5f, sourceMaxC = 30;
  float humOnBelow = 70, humOffAbove = 75; uint16_t humMaxOnMin = 10, humPauseMin = 5;
  float ventOnAbove = 95, ventOffBelow = 90; uint16_t ventIntervalMin = 120, ventDurationMin = 5;
  float tempLowC = 20, tempHighC = 30; uint16_t tempDelayMin = 5;
  float rhLow = 70, rhHigh = 95; uint16_t rhDelayMin = 30, wifiLostMin = 30;
  uint16_t smsLimitMin = 60, manualTimeoutMin = 30; bool relayActiveLow = true;
  uint16_t logIntervalMin = 5, retentionDays = 365;
};

struct Credentials {
  char wifiSsid[33] = "", wifiPass[65] = "", hostname[33] = "terrarium";
  char webUser[33] = "admin", webPass[65] = "", smsPhone[20] = "", simPin[9] = "";
};
struct Config {
  Settings settings;
  Credentials cred;
  char ntpServer[64] = "pool.ntp.org";
  char tz[64] = "CET-1CEST,M3.5.0,M10.5.0/3";
};
struct ConfigResult { bool ok; uint8_t rejected; char error[96]; };  // ok=false alleen bij onleesbare JSON

#include <stddef.h>
void setDefaults(Config& c);
// Ongeldige velden worden genegeerd (rejected++, eerste reden in error). Lege geheimen
// (wifi.password, web.password, sms.sim_pin) behouden de oude waarde als emptySecretKeepsOld.
ConfigResult parseConfig(const char* json, Config& c, bool emptySecretKeepsOld);
// Geeft de lengte (zonder NUL); bij te kleine buffer: 0 en buf leeg.
size_t serializeConfig(const Config& c, char* buf, size_t len, bool includeSecrets);
bool parseHHMM(const char* s, uint16_t& minutes);
uint32_t crc32(const uint8_t* data, size_t len);

#endif  // TERRARIUM_CONFIG_H
