#pragma once
#include "config.h"

// Hardware: SD (/config.json) en AT24C32-EEPROM (I2C 0x57) als backup.
// Vereist dat SD.begin() en Wire.begin() al zijn aangeroepen.
enum class ConfigSource : uint8_t { Sd, Eeprom, Defaults };

// SD /config.json -> anders EEPROM (magic "TCFG", uint16 len, uint32 crc, json) -> anders defaults.
// c wordt altijd volledig gevuld. Bij een geslaagde SD-/EEPROM-load wordt de EEPROM bijgewerkt (alleen bij wijziging).
// result (optioneel): parse-resultaat van de gebruikte bron (rejected/error); bij Defaults ok, 0 geweigerd.
ConfigSource configLoad(Config& c, ConfigResult* result = nullptr);

// SD: schrijf /config.tmp, verwijder /config.json, hernoem; daarna EEPROM als inhoud gewijzigd.
// true als minstens SD of EEPROM is bijgewerkt/actueel.
bool configSave(const Config& c);
