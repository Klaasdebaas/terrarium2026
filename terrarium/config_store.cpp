#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include "config_store.h"
#include "pins.h"

namespace {
constexpr uint8_t EEPROM_ADDR = 0x57;
constexpr size_t EEPROM_PAGE = 32;
constexpr size_t EEPROM_SIZE = 4096;  // AT24C32
constexpr size_t HEADER = 10;         // "TCFG" + uint16 len + uint32 crc
constexpr size_t JSON_MAX = EEPROM_SIZE - HEADER;
const char* const SD_FILE = "/config.json";
const char* const SD_TMP = "/config.tmp";

char g_json[JSON_MAX + 1];  // gedeelde buffer (alleen vanuit de hoofdlus)

bool eepromRead(uint16_t addr, uint8_t* out, size_t n) {
  while (n > 0) {
    size_t chunk = n > EEPROM_PAGE ? EEPROM_PAGE : n;
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((uint8_t)(addr >> 8)); Wire.write((uint8_t)(addr & 0xFF));
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)EEPROM_ADDR, (int)chunk) != (int)chunk) return false;
    for (size_t i = 0; i < chunk; i++) out[i] = (uint8_t)Wire.read();
    addr += chunk; out += chunk; n -= chunk;
  }
  return true;
}

// Schrijft n bytes vanaf addr; elke write blijft binnen één 32-byte pagina. Slaat ongewijzigde delen over.
bool eepromWrite(uint16_t addr, const uint8_t* data, size_t n) {
  while (n > 0) {
    size_t chunk = EEPROM_PAGE - (addr % EEPROM_PAGE);
    if (chunk > n) chunk = n;
    uint8_t cur[EEPROM_PAGE];
    bool same = eepromRead(addr, cur, chunk) && memcmp(cur, data, chunk) == 0;
    if (!same) {
      Wire.beginTransmission(EEPROM_ADDR);
      Wire.write((uint8_t)(addr >> 8)); Wire.write((uint8_t)(addr & 0xFF));
      Wire.write(data, chunk);
      if (Wire.endTransmission() != 0) return false;
      delay(5);  // schrijfcyclus AT24C32
    }
    addr += chunk; data += chunk; n -= chunk;
  }
  return true;
}

// Schrijft header+json naar de EEPROM (alleen gewijzigde pagina's).
bool eepromStore(const char* json, size_t len) {
  if (len == 0 || len > JSON_MAX) return false;
  uint8_t hdr[HEADER] = {'T', 'C', 'F', 'G', (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
  uint32_t crc = crc32((const uint8_t*)json, len);
  for (int i = 0; i < 4; i++) hdr[6 + i] = (uint8_t)(crc >> (8 * i));
  return eepromWrite(0, hdr, HEADER) && eepromWrite(HEADER, (const uint8_t*)json, len);
}

// Leest geldige JSON uit de EEPROM in g_json; geeft lengte of 0.
size_t eepromLoadJson() {
  uint8_t hdr[HEADER];
  if (!eepromRead(0, hdr, HEADER)) return 0;
  if (memcmp(hdr, "TCFG", 4) != 0) return 0;
  size_t len = hdr[4] | ((size_t)hdr[5] << 8);
  uint32_t crc = hdr[6] | ((uint32_t)hdr[7] << 8) | ((uint32_t)hdr[8] << 16) | ((uint32_t)hdr[9] << 24);
  if (len == 0 || len > JSON_MAX) return 0;
  if (!eepromRead(HEADER, (uint8_t*)g_json, len)) return 0;
  if (crc32((const uint8_t*)g_json, len) != crc) return 0;
  g_json[len] = 0;
  return len;
}

bool backupToEeprom(const Config& c) {
  size_t n = serializeConfig(c, g_json, sizeof g_json, true);
  return n > 0 && eepromStore(g_json, n);
}
}  // namespace

ConfigSource configLoad(Config& c, ConfigResult* result) {
  setDefaults(c);
  if (result) { result->ok = true; result->rejected = 0; result->error[0] = 0; }
  File f = SD.open(SD_FILE, FILE_READ);
  if (f) {
    size_t n = f.size() <= JSON_MAX ? f.read((uint8_t*)g_json, JSON_MAX) : 0;
    f.close();
    g_json[n] = 0;
    if (n > 0) {
      Config tmp;
      setDefaults(tmp);
      ConfigResult r = parseConfig(g_json, tmp, false);
      if (r.ok) {
        c = tmp;
        if (result) *result = r;
        backupToEeprom(c);
        return ConfigSource::Sd;
      }
    }
  }
  if (eepromLoadJson() > 0) {
    Config tmp;
    setDefaults(tmp);
    ConfigResult r = parseConfig(g_json, tmp, false);
    if (r.ok) {
      c = tmp;
      if (result) *result = r;
      return ConfigSource::Eeprom;
    }
  }
  return ConfigSource::Defaults;
}

bool configSave(const Config& c) {
  size_t n = serializeConfig(c, g_json, sizeof g_json, true);
  if (n == 0) return false;
  bool sdOk = false;
  SD.remove(SD_TMP);  // FILE_WRITE appends; oud restant van een onderbroken opslag weg
  File f = SD.open(SD_TMP, FILE_WRITE);
  if (f) {
    size_t w = f.write((const uint8_t*)g_json, n);
    f.close();
    if (w == n) {
      SD.remove(SD_FILE);
      sdOk = SD.rename(SD_TMP, SD_FILE);
    }
    if (!sdOk) SD.remove(SD_TMP);
  }
  bool eeOk = eepromStore(g_json, n);
  return sdOk || eeOk;
}
