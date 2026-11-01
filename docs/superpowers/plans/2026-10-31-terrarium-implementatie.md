# Terrarium-besturing ESP32-C6 — Implementatieplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Een Arduino-sketch voor de ESP32-C6 die een terrarium regelt (verwarming, bevochtiging, ventilatie, verlichting), logt op SD, storingen per SMS meldt en een lokale webinterface biedt.

**Architecture:** Eén modulaire sketch met een non-blocking coöperatieve `loop()`. Alle beslislogica (regeling, alarmen, config-validatie, formattering, SMS-state-machine, JSON-opbouw) zit in *pure* C++-bestanden zonder `Arduino.h`, die op de pc met g++ getest worden. Dunne hardware-bestanden koppelen die logica aan sensoren, relais, SD, RTC, scherm, modem en WiFi.

**Tech Stack:** Arduino IDE 2.x, Espressif ESP32 Arduino core 3.x (board *ESP32C6 Dev Module*, *USB CDC On Boot: Enabled*), libraries U8g2, DHT sensor library (Adafruit) + Adafruit Unified Sensor, OneWire, DallasTemperature, RTClib, ArduinoJson v7. Pc-tests: g++ (C++17) + make.

**Spec:** `docs/superpowers/specs/2026-10-31-terrarium-design.md`

## Global Constraints

- Alle sketchbestanden staan plat in `terrarium/` (Arduino compileert alleen de sketchmap zelf, geen submappen).
- Pure bestanden (`types.h`, `state.h`, `control.*`, `alarms.*`, `sensor_logic.*`, `config_json.*`, `logformat.*`, `sms_modem.*`, `display_format.*`, `web_json.*`) includen **nooit** `Arduino.h` of hardware-libraries; alleen `<stdint.h>`, `<stddef.h>`, `<string.h>`, `<stdio.h>`, `<math.h>` en `ArduinoJson.h`.
- Tijd in pure code: `uint32_t nowMs` (van `millis()`); verschillen altijd als `(uint32_t)(nowMs - sinceMs)` zodat de overloop na 49,7 dagen goed gaat.
- Geen `delay()` > 10 ms buiten `setup()` (uitzondering: EEPROM-pagina-writes bij config opslaan, max ~350 ms, alleen na een gebruikersactie).
- Geen `String`-concatenatie in de hoofdlus; vaste `char`-buffers met `snprintf`.
- Geheimen (WiFi-wachtwoord, webwachtwoord, SIM-pin, telefoonnummer) nooit in broncode of repo; `sd-template/config.json` bevat lege waarden en `+316XXXXXXXX`.
- SMS-teksten: alleen ASCII (GSM-7-veilig), ≤ 160 tekens, formaat `TERRARIUM <ALARM|HERSTELD|INFO>: <omschrijving> (<dd-mm hh:mm>)`.
- Pinnen uitsluitend uit `pins.h` (waarden: spec §3.2).
- Standaardwaarden uitsluitend uit `setDefaults()` (waarden: spec §5.1).

## Review Focus

1. **Opstart zonder geldige meting** — in de eerste seconden (of als een sensor nooit antwoordt) moet de verwarming en bevochtiging UIT blijven, zonder valse "sensorstoring"-SMS vóór 3 mislukte metingen. → Task 3 (`FaultCounter`) en Task 2 (`decide` met `dhtOk=false`).
2. **`millis()`-overloop na 49,7 dagen** — max-aan-tijd bevochtiging, alarmvertragingen, SMS-limiet en handbediening moeten doorwerken over de wrap heen. → tests met `nowMs` rond `0xFFFFFF00` in Task 2 en Task 4.
3. **Zomer-/wintertijd en middernacht** — `isDay` moet werken als `day_start > night_start` (schema over middernacht) en een sprong in `minuteOfDay` mag geen relais laten hangen. → Task 2.
4. **Kapotte of halve config** — onleesbare JSON, verkeerde types, waarden buiten bereik of tegenstrijdige grenzen (`off_above ≤ on_below`) mogen het systeem niet onbruikbaar maken; vorige geldige waarde blijft. → Task 5.
5. **Pad-manipulatie bij log-download** — `/logs/../config.json` mag nooit geheimen lekken. → Task 6 (`isSafeLogName`).

---

## Bestandsoverzicht

| Bestand | Puur? | Verantwoordelijkheid | Task |
|---|---|---|---|
| `terrarium/terrarium.ino` | nee | `setup()`/`loop()`, watchdog, samenvoegen | 1, 13 |
| `terrarium/pins.h` | ja | pinnummers | 1 |
| `terrarium/types.h` | ja | `RelayId`, `RelayMode`, `LocalTime`, `AlarmId` | 1 |
| `terrarium/state.h` | ja | `SystemState` | 1 |
| `terrarium/control.h/.cpp` | ja | regellogica + handbediening | 2 |
| `terrarium/sensor_logic.h/.cpp` | ja | validatie, `FaultCounter` | 3 |
| `terrarium/alarms.h/.cpp` | ja | alarmlogica, SMS-tekst | 4 |
| `terrarium/config.h`, `config_json.cpp` | ja | `Config`, defaults, JSON, CRC32 | 5 |
| `terrarium/config_store.h/.cpp` | nee | SD + AT24C32 laden/opslaan | 5 |
| `terrarium/logformat.h/.cpp` | ja | CSV-regels, bestandsnamen, ouderdom | 6 |
| `terrarium/logger.h/.cpp` | nee | SD-logboek | 6 |
| `terrarium/clock.h/.cpp` | nee | DS3231, NTP, tijdzone | 7 |
| `terrarium/sensors.h/.cpp` | nee | DHT22, DS18B20 | 8 |
| `terrarium/relays.h/.cpp` | nee | relaispinnen | 8 |
| `terrarium/sms_modem.h/.cpp` | ja | SIM7600-state-machine via `ModemIO` | 9 |
| `terrarium/sms_port.h/.cpp` | nee | `ModemIO` op `Serial1` | 9 |
| `terrarium/display_format.h/.cpp` | ja | 6 schermregels | 10 |
| `terrarium/display.h/.cpp` | nee | U8g2 ST7565R | 10 |
| `terrarium/web_json.h/.cpp` | ja | status-JSON, verzoeken parsen | 11 |
| `terrarium/net_wifi.h/.cpp` | nee | WiFi, AP, mDNS | 12 |
| `terrarium/web.h/.cpp`, `webpage.h` | nee | WebServer, HTML | 12 |
| `test/Makefile`, `test/minitest.h`, `test/test_*.cpp` | — | pc-tests | 1–11 |
| `sd-template/config.json` | — | voorbeeldconfig | 5 |
| `docs/bringup.md` | — | hardware-testchecklist | 14 |

---

### Task 1: Projectskelet, gedeelde types en testharnas

**Files:**
- Create: `terrarium/terrarium.ino`, `terrarium/pins.h`, `terrarium/types.h`, `terrarium/state.h`
- Create: `test/Makefile`, `test/minitest.h`, `test/test_smoke.cpp`, `test/vendor/.gitignore` (bevat `ArduinoJson.h`)
- Create: `.gitignore` (`test/build/`)

**Interfaces:**
- Produces (`types.h`):
  ```cpp
  enum RelayId : uint8_t { R_HEAT = 0, R_HUMID = 1, R_VENT = 2, R_LIGHT = 3, RELAY_COUNT = 4 };
  enum class RelayMode : uint8_t { Auto, ManualOn, ManualOff };
  struct LocalTime { int16_t year; uint8_t month, day, hour, minute, second; };
  enum AlarmId : uint8_t { A_TEMP_LOW, A_TEMP_HIGH, A_SOURCE_HOT, A_RH_LOW, A_RH_HIGH,
                           A_DHT_FAIL, A_DS_FAIL, A_SD_FAIL, A_RTC_FAIL, A_WIFI_LOST, ALARM_COUNT };
  ```
- Produces (`state.h`):
  ```cpp
  struct SystemState {
    LocalTime time; bool timeValid; bool isDay;
    float airTemp, rh, heaterTemp; bool dhtOk, dsOk;        // laatste geldige waarde; *Ok = bruikbaar
    bool relayOn[RELAY_COUNT]; RelayMode relayMode[RELAY_COUNT]; uint32_t manualRemainingS[RELAY_COUNT];
    bool alarmActive[ALARM_COUNT];
    bool wifiOk; bool apMode; int8_t wifiRssi; char ip[16];
    bool gsmOk; int8_t gsmSignal;                            // 0..31, -1 onbekend
    bool sdOk, rtcOk; uint32_t uptimeS;
  };
  ```
- Produces (`pins.h`): `constexpr uint8_t PIN_SDA=23, PIN_SCL=22, PIN_SCK=21, PIN_MOSI=19, PIN_MISO=20, PIN_SD_CS=18, PIN_LCD_CS=6, PIN_LCD_DC=5, PIN_LCD_RST=4, PIN_DHT=3, PIN_DS18B20=2, PIN_MODEM_RX=7, PIN_MODEM_TX=15; constexpr uint8_t PIN_RELAY[RELAY_COUNT] = {0, 1, 10, 11};`
- Produces (`test/minitest.h`): macros `TEST(name)`, `CHECK(cond)`, `CHECK_EQ(a,b)`, `CHECK_NEAR(a,b,eps)`, `CHECK_STR(a,b)`; `main()` in `minitest.h` onder `#ifdef MINITEST_MAIN` draait alle geregistreerde tests, print `N passed, M failed` en geeft exitcode ≠ 0 bij falen.
- Produces (`test/Makefile`): target `test` compileert elk `test_*.cpp` met `-std=c++17 -Wall -Wextra -Werror -I../terrarium -Ivendor` plus de pure `.cpp`'s waarnaar het verwijst, en draait ze; target `vendor/ArduinoJson.h` downloadt `https://github.com/bblanchon/ArduinoJson/releases/download/v7.2.1/ArduinoJson-v7.2.1.h`.

- [ ] **Step 1:** Schrijf `test/test_smoke.cpp` met `TEST(smoke_types) { CHECK_EQ(RELAY_COUNT, 4); CHECK_EQ(ALARM_COUNT, 10); }`.
- [ ] **Step 2:** Run `make -C test` → FAIL (headers ontbreken).
- [ ] **Step 3:** Maak `types.h`, `state.h`, `pins.h`, `minitest.h`, `Makefile`, en een `terrarium.ino` met lege `setup()`/`loop()` die alleen `Serial.begin(115200)` doet.
- [ ] **Step 4:** Run `make -C test` → `1 passed, 0 failed`.
- [ ] **Step 5 (optioneel, alleen als het netwerk het toelaat):** installeer `arduino-cli` vanaf de GitHub-release, voeg index `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` toe, installeer core `esp32:esp32` en de libraries uit Tech Stack, en run `arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc terrarium` → succes. Lukt dit niet, noteer dan in de commit dat de ESP32-compilatie door de gebruiker in de Arduino IDE gedaan wordt; dit geldt dan voor elke "compileer"-stap in volgende tasks.
- [ ] **Step 6:** Commit `feat: projectskelet, gedeelde types en testharnas`.

---

### Task 2: Regellogica (`control`)

**Files:**
- Create: `terrarium/control.h`, `terrarium/control.cpp`, `terrarium/config.h` (alleen de struct `Settings` — de rest volgt in Task 5)
- Test: `test/test_control.cpp`

**Interfaces:**
- Consumes: `RelayId`, `RelayMode` (Task 1).
- Produces (`config.h`, struct met standaardwaarden als member-initializers, waarden uit spec §5.1):
  ```cpp
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
  ```
- Produces (`control.h`):
  ```cpp
  bool isDay(uint16_t minuteOfDay, uint16_t dayStartMin, uint16_t nightStartMin); // werkt ook als dayStart > nightStart
  bool inVentWindow(uint16_t minuteOfDay, uint16_t intervalMin, uint16_t durationMin); // (minuteOfDay % interval) < duration
  struct ControlInput { uint32_t nowMs; uint16_t minuteOfDay; bool timeValid;
                        float airTemp, rh, heaterTemp; bool dhtOk, dsOk; RelayMode mode[RELAY_COUNT]; };
  struct ControlMemory { bool autoOn[RELAY_COUNT]; uint32_t humOnSinceMs, humPauseSinceMs; bool humPausing; bool sourceTripped; };
  struct ControlOutput { bool on[RELAY_COUNT]; bool isDay; };
  void controlInit(ControlMemory& m);  // alles uit/false
  ControlOutput decide(const ControlInput& in, const Settings& s, ControlMemory& m);
  struct Overrides { RelayMode mode[RELAY_COUNT]; uint32_t sinceMs[RELAY_COUNT]; };
  void overridesInit(Overrides& o);  // alles Auto
  void setOverride(Overrides& o, RelayId r, RelayMode mode, uint32_t nowMs);
  void expireOverrides(Overrides& o, uint32_t nowMs, uint16_t timeoutMin);
  uint32_t overrideRemainingS(const Overrides& o, RelayId r, uint32_t nowMs, uint16_t timeoutMin); // 0 bij Auto
  ```
- Regels in `decide` (volgorde): (1) automatische stand per relais volgens spec §4.4 met hysterese in `m.autoOn`; (2) `in.mode` toepassen (ManualOn/ManualOff overschrijft); (3) veiligheid: R1 uit als `!dhtOk || !dsOk || sourceTripped`; R2 uit als `!dhtOk`. `sourceTripped` wordt true bij `heaterTemp > sourceMaxC` (alleen als `dsOk`) en false bij `heaterTemp < sourceMaxC − 1`. Bij `!timeValid`: `isDay=false`, nachtstreefwaarde, R4 uit, geen ventilatievenster. Bij `!dhtOk`: R3 alleen ventilatievenster. Max-aan-tijd R2: na `humMaxOnMin` aan → `humPausing=true` voor `humPauseMin`, tijdens pauze uit.

- [ ] **Step 1: Schrijf de falende tests** in `test/test_control.cpp`, met een helper `ControlInput base()` (dag 600 min, `timeValid`, 25 °C, 80 %, bron 25 °C, beide ok, alles Auto):
  ```cpp
  TEST(isday_normal)      { CHECK(isDay(480,480,1200)); CHECK(!isDay(1200,480,1200)); CHECK(!isDay(479,480,1200)); }
  TEST(isday_wraps)       { CHECK(isDay(1380,1320,360)); CHECK(isDay(100,1320,360)); CHECK(!isDay(600,1320,360)); }
  TEST(vent_window)       { CHECK(inVentWindow(0,120,5)); CHECK(inVentWindow(124,120,5)); CHECK(!inVentWindow(125,120,5)); }
  TEST(heat_day_hyst)     // 28.4 → aan; 29.0 → blijft aan; 29.6 → uit; 29.0 → blijft uit
  TEST(heat_night_target) // minuteOfDay 1300, 22.6 → uit; 21.4 → aan
  TEST(heat_source_trip)  // bron 30.1 → R1 uit ook bij 20 °C lucht; bron 29.5 → nog uit; bron 28.9 → weer aan
  TEST(heat_off_on_sensor_fault) // dhtOk=false → R1 uit; dsOk=false → R1 uit
  TEST(hum_hyst)          // 69 → aan; 72 → aan; 75.5 → uit
  TEST(hum_max_on_pause)  // rh 60 constant: aan t=0..599999 ms, uit t=600000..899999, weer aan t=900000
  TEST(hum_max_on_wraps)  // zelfde als hierboven met start nowMs = 0xFFFFFF00
  TEST(vent_rh_hyst)      // 96 → aan; 92 → aan; 89 → uit (buiten venster, minuteOfDay 600+10)
  TEST(vent_schedule)     // rh 80, minuteOfDay 720 → aan; 725 → uit
  TEST(vent_dht_fault_schedule_only) // dhtOk=false, rh=99 (oude waarde), minuteOfDay 610 → uit; 720 → aan
  TEST(light_schedule)    // 480 → aan, 1199 → aan, 1200 → uit
  TEST(no_time_safe)      // timeValid=false: R4 uit, isDay false, streef 22, geen venster
  TEST(manual_overrides_auto) // mode[R_LIGHT]=ManualOff overdag → R4 uit; mode[R_VENT]=ManualOn → R3 aan
  TEST(safety_beats_manual)   // mode[R_HEAT]=ManualOn, bron 31 → R1 uit; mode[R_HUMID]=ManualOn, dhtOk=false → R2 uit
  TEST(override_expires)  // setOverride(R_VENT,ManualOn,1000); expire op 1000+30*60000-1 → nog ManualOn; op +30*60000 → Auto
  TEST(override_remaining)// direct na set met timeout 30 → 1800; Auto → 0
  TEST(dst_jump_no_stuck) // minuteOfDay 119 → 180 (zomertijd): R3 niet aan door venster, geen crash; R4 nog aan
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL (control.h ontbreekt).
- [ ] **Step 3:** Implementeer `config.h` (`Settings`) en `control.h/.cpp` volgens de interface en regels hierboven.
- [ ] **Step 4:** Run `make -C test` → alle tests PASS.
- [ ] **Step 5:** Commit `feat: regellogica met hysterese, schema's, handbediening en veiligheid`.

---

### Task 3: Sensorvalidatie (`sensor_logic`)

**Files:**
- Create: `terrarium/sensor_logic.h`, `terrarium/sensor_logic.cpp`
- Test: `test/test_sensor_logic.cpp`

**Interfaces:**
- Produces:
  ```cpp
  bool validDht(float tempC, float rh);   // !isnan, −40..80 °C, 0..100 %
  bool validDs(float tempC);              // !isnan, −55..125, en niet −127.0 en niet exact 85.0
  class FaultCounter {
  public:
    void record(bool valid);              // valid reset de teller, invalid telt op
    bool usable() const;                  // minstens één geldige meting ooit én niet faulty()
    bool faulty() const;                  // ≥ 3 opeenvolgende ongeldige metingen
  };
  ```

- [ ] **Step 1: Tests:**
  ```cpp
  TEST(dht_valid)   { CHECK(validDht(25,80)); CHECK(!validDht(NAN,80)); CHECK(!validDht(25,101)); CHECK(!validDht(81,50)); }
  TEST(ds_valid)    { CHECK(validDs(27.5)); CHECK(!validDs(-127)); CHECK(!validDs(85.0f)); CHECK(!validDs(NAN)); }
  TEST(fault_initial_not_usable_not_faulty) { FaultCounter f; CHECK(!f.usable()); CHECK(!f.faulty()); }
  TEST(fault_after_three) { FaultCounter f; f.record(true); f.record(false); f.record(false); CHECK(f.usable()); CHECK(!f.faulty());
                            f.record(false); CHECK(f.faulty()); CHECK(!f.usable()); f.record(true); CHECK(f.usable()); }
  TEST(fault_never_valid) { FaultCounter f; f.record(false); f.record(false); f.record(false); CHECK(f.faulty()); CHECK(!f.usable()); }
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Commit `feat: sensorvalidatie en storingsteller`.

---

### Task 4: Alarmlogica (`alarms`)

**Files:**
- Create: `terrarium/alarms.h`, `terrarium/alarms.cpp`
- Test: `test/test_alarms.cpp`

**Interfaces:**
- Consumes: `AlarmId`, `LocalTime` (Task 1), `Settings` (Task 2).
- Produces:
  ```cpp
  struct AlarmInput { uint32_t nowMs; float airTemp, rh, heaterTemp;
                      bool dhtUsable, dsUsable, dhtFaulty, dsFaulty, sdOk, rtcOk, wifiOk; };
  enum class Notify : uint8_t { None, Alarm, Restored };
  struct AlarmState { bool active[ALARM_COUNT]; bool condPrev[ALARM_COUNT]; uint32_t condSinceMs[ALARM_COUNT];
                      Notify pending[ALARM_COUNT]; Notify lastSent[ALARM_COUNT]; bool everSent[ALARM_COUNT]; uint32_t lastSmsMs[ALARM_COUNT]; };
  struct AlarmEvent { AlarmId id; bool transition; bool active; bool sendSms; };
  void alarmsInit(AlarmState& st);
  uint8_t evaluateAlarms(const AlarmInput& in, const Settings& s, AlarmState& st, AlarmEvent* out, uint8_t maxOut);
  const char* alarmKey(AlarmId id);   // "temp_low","temp_high","source_hot","rh_low","rh_high","dht_fail","ds_fail","sd_fail","rtc_fail","wifi_lost"
  size_t formatAlarmSms(char* buf, size_t len, AlarmId id, bool active, const AlarmInput& in, const LocalTime& t);
  size_t formatInfoSms(char* buf, size_t len, const char* text, const LocalTime& t);
  ```
- Voorwaarden en vertraging per alarm: temp_low `dhtUsable && airTemp < tempLowC` na `tempDelayMin`; temp_high `> tempHighC` na `tempDelayMin`; source_hot `dsUsable && heaterTemp > sourceMaxC` direct; rh_low/rh_high `dhtUsable` en `rh < rhLow` / `rh > rhHigh` na `rhDelayMin`; dht_fail `dhtFaulty`, ds_fail `dsFaulty`, sd_fail `!sdOk`, rtc_fail `!rtcOk` direct; wifi_lost `!wifiOk` na `wifiLostMin`. Herstel: direct zodra de voorwaarde onwaar is.
- SMS-limiet: bij elke transitie wordt `pending[id]` gezet op Alarm/Restored. Een pending melding wordt verstuurd (`sendSms=true`) als `!everSent[id]` of `nowMs − lastSmsMs[id] ≥ smsLimitMin`. Een Restored-melding voor een alarm waarvan nog nooit een Alarm-SMS ging, of een pending die gelijk is aan `lastSent[id]`, vervalt zonder SMS.
- SMS-teksten (exact, `%.1f`): temp_low `Temperatuur te laag 19.4C`, temp_high `Temperatuur te hoog 30.4C`, source_hot `Warmtebron te heet 31.2C, verwarming uit`, rh_low `Luchtvochtigheid te laag 65%`, rh_high `Luchtvochtigheid te hoog 97%` (RV als `%.0f`), dht_fail `Sensorstoring DHT22`, ds_fail `Sensorstoring DS18B20`, sd_fail `SD-kaart fout`, rtc_fail `Klok (RTC) fout`, wifi_lost `WiFi verbinding weg`. Hersteld-varianten: `Temperatuur normaal 28.9C`, `Warmtebron normaal 28.0C`, `Luchtvochtigheid normaal 80%`, `Sensor DHT22 werkt weer`, `Sensor DS18B20 werkt weer`, `SD-kaart werkt weer`, `Klok (RTC) werkt weer`, `WiFi verbinding hersteld`.

- [ ] **Step 1: Tests** (helper `AlarmInput ok(uint32_t t)` = alles normaal):
  ```cpp
  TEST(temp_high_needs_delay) // 30.5 van t=0: op t=299999 geen event; op t=300000 transition+active+sendSms voor A_TEMP_HIGH
  TEST(temp_spike_no_alarm)   // 30.5 op t=0..200000, 29 op t=200001, 30.5 op t=200002..400000 → geen alarm (teller herstart)
  TEST(source_hot_immediate)  // heaterTemp 30.1 → direct A_SOURCE_HOT active+sms
  TEST(no_temp_alarm_when_dht_unusable) // dhtUsable=false, airTemp=0 → geen A_TEMP_LOW
  TEST(sensor_fail_alarm)     // dhtFaulty=true → A_DHT_FAIL direct
  TEST(restore_sends_sms)     // alarm op t0 (sms), herstel op t0+61 min → transition active=false, sendSms=true
  TEST(rate_limit_defers_restore) // alarm t0 (sms), herstel t0+10 min → transition, geen sms; op t0+60 min → sendSms Restored
  TEST(rate_limit_flap_drops) // alarm t0 (sms), herstel +10 min, opnieuw alarm +20 min → op +60 min géén sms (pending==lastSent)
  TEST(rate_limit_wraps)      // zelfde als restore_sends_sms met t0 = 0xFFFFFF00
  TEST(wifi_lost_delay)       // wifiOk=false: op 29:59 geen alarm, op 30:00 wel
  TEST(sms_format_alarm)      // CHECK_STR(buf, "TERRARIUM ALARM: Temperatuur te hoog 30.4C (31-10 14:23)")
  TEST(sms_format_restored)   // "TERRARIUM HERSTELD: Temperatuur normaal 28.9C (31-10 14:41)"
  TEST(sms_format_info)       // formatInfoSms("Herstart. Laatste log 31-10 03:15") → "TERRARIUM INFO: Herstart. Laatste log 31-10 03:15 (31-10 07:02)"
  TEST(sms_max_160)           // alle 20 varianten: strlen ≤ 160 en alle tekens 0x20..0x7E
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Commit `feat: alarmlogica met vertraging, SMS-limiet en berichtteksten`.

---

### Task 5: Configuratie (JSON, validatie, opslag SD + AT24C32)

**Files:**
- Modify: `terrarium/config.h` (uitbreiden)
- Create: `terrarium/config_json.cpp`, `terrarium/config_store.h`, `terrarium/config_store.cpp`, `sd-template/config.json`
- Test: `test/test_config.cpp`

**Interfaces:**
- Consumes: `Settings` (Task 2).
- Produces (`config.h`):
  ```cpp
  struct Credentials { char wifiSsid[33], wifiPass[65], hostname[33], webUser[33], webPass[65], smsPhone[20], simPin[9]; };
  struct Config { Settings settings; Credentials cred; char ntpServer[64]; char tz[64]; };
  struct ConfigResult { bool ok; uint8_t rejected; char error[96]; }; // ok=false alleen bij onleesbare JSON
  void setDefaults(Config& c);   // Settings-defaults, hostname "terrarium", webUser "admin", ntp "pool.ntp.org", tz "CET-1CEST,M3.5.0,M10.5.0/3", rest leeg
  ConfigResult parseConfig(const char* json, Config& c, bool emptySecretKeepsOld);
  size_t serializeConfig(const Config& c, char* buf, size_t len, bool includeSecrets); // JSON-structuur exact als spec §5.1; zonder secrets: wachtwoorden en simPin weggelaten
  bool parseHHMM(const char* s, uint16_t& minutes);
  uint32_t crc32(const uint8_t* data, size_t len);  // IEEE 802.3, init 0xFFFFFFFF, final xor
  ```
- Validatieregels (per veld; ongeldig → veld genegeerd, `rejected++`, eerste reden in `error`): temperaturen 0..45, `heatHystC` 0.1..5, RV-waarden 0..100, `humOffAbove > humOnBelow`, `ventOffBelow < ventOnAbove`, `tempLowC < tempHighC`, `rhLow < rhHigh`, minuten 1..1440, `ventDurationMin < ventIntervalMin`, `retentionDays` 1..3650, tijden via `parseHHMM` (00:00–23:59) en `day_start ≠ night_start`, telefoon begint met `+` en bevat 8–15 cijfers, verkeerd JSON-type = ongeldig. Paarregels worden gecontroleerd op de *resulterende* waarden; bij schending wordt het hele paar teruggezet.
- Produces (`config_store.h`, hardware):
  ```cpp
  enum class ConfigSource : uint8_t { Sd, Eeprom, Defaults };
  ConfigSource configLoad(Config& c);           // SD /config.json → anders EEPROM (magic "TCFG", uint16 len, uint32 crc, json) → anders defaults
  bool configSave(const Config& c);             // SD (schrijf /config.tmp, verwijder /config.json, hernoem) + EEPROM als inhoud gewijzigd
  ```
  AT24C32 op I2C-adres `0x57`, page-writes van 32 bytes met 5 ms wachttijd, max 4096 bytes.

- [ ] **Step 1: Tests:**
  ```cpp
  TEST(defaults)          // settings.heatDayC==29, dayStartMin==480, cred.hostname=="terrarium", tz klopt
  TEST(parse_full)        // sd-template-achtige JSON met heating.day_c 28 → ok, heatDayC 28
  TEST(parse_partial)     // {"heating":{"night_c":21}} → alleen nightC gewijzigd, dayC blijft 29
  TEST(parse_garbage)     // "{niet json" → ok=false, config ongewijzigd
  TEST(parse_wrong_type)  // {"heating":{"day_c":"warm"}} → rejected==1, dayC 29
  TEST(parse_out_of_range)// {"heating":{"day_c":80}} → rejected==1
  TEST(parse_pair_conflict)// {"humidity":{"on_below":80,"off_above":75}} → beide terug naar 70/75, rejected≥1
  TEST(parse_hhmm)        // "08:00"→480, "23:59"→1439, "24:00" fout, "8:0" fout
  TEST(empty_secret_keeps_old) // webPass "geheim", parse {"web":{"password":""}} met emptySecretKeepsOld=true → nog "geheim"
  TEST(phone_validation)  // "+31600000000" ok; "0612345678" rejected
  TEST(serialize_no_secrets) // includeSecrets=false → strstr(buf,"geheim")==nullptr, wel "day_c"
  TEST(roundtrip)         // serialize(true) → parse in verse defaults → gelijke Settings en Credentials
  TEST(crc32_vector)      // crc32("123456789") == 0xCBF43926
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer `config.h`/`config_json.cpp` met ArduinoJson v7 (`JsonDocument`, `deserializeJson`, `is<float>()`-checks).
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Implementeer `config_store.cpp` (`SD`, `Wire`) en schrijf `sd-template/config.json` = `serializeConfig(defaults, includeSecrets=true)` met `"phone": "+316XXXXXXXX"`, pretty-printed. Compileer (zie Task 1 Step 5).
- [ ] **Step 6:** Commit `feat: configuratie met validatie, SD-opslag en EEPROM-backup`.

---

### Task 6: Logboek (formattering + SD)

**Files:**
- Create: `terrarium/logformat.h`, `terrarium/logformat.cpp`, `terrarium/logger.h`, `terrarium/logger.cpp`
- Test: `test/test_logformat.cpp`

**Interfaces:**
- Consumes: `SystemState`, `LocalTime` (Task 1), `alarmKey` (Task 4).
- Produces (`logformat.h`, puur):
  ```cpp
  void formatDayFileName(char* buf, size_t len, const LocalTime& t);            // "/log/2026-10-31.csv"
  const char* measurementHeader();  // "tijd;lucht_c;rv_pct;bron_c;r1_verw;r2_bevocht;r3_vent;r4_licht;alarmen"
  size_t formatMeasurementLine(char* buf, size_t len, const SystemState& s);    // ongeldige meting = leeg veld; alarmen = keys gescheiden door ','
  size_t formatEventLine(char* buf, size_t len, const LocalTime& t, const char* kind, const char* text);
  bool isSafeLogName(const char* name);   // alleen [A-Za-z0-9_-] + ".csv", geen '/', geen "..", max 32 tekens
  int32_t daysFromCivil(int y, unsigned m, unsigned d);
  bool isExpired(const char* fileName, const LocalTime& today, uint16_t retentionDays); // "2025-10-30.csv"; niet-datumnamen → false
  ```
- Produces (`logger.h`, hardware):
  ```cpp
  bool loggerBegin();                                           // mkdir /log, sdOk
  void loggerMeasurement(const SystemState& s);                 // append dagbestand (header als nieuw) + overschrijf /log/last.txt met tijd
  void loggerEvent(const LocalTime& t, const char* kind, const char* text);  // kind: ALARM, HERSTELD, SMS, RELAIS, HAND, CONFIG, START, WIFI, NTP, HEAP
  void loggerDailyMaintenance(const LocalTime& t, uint16_t retentionDays);   // verwijder verlopen dagbestanden; op 1 jan: events.csv → events-<vorig jaar>.csv
  bool loggerLastTimestamp(char* buf, size_t len);              // inhoud van /log/last.txt
  bool loggerSdOk();
  ```
  Elke schrijfactie: open, één regel, close (spec §9). Bij fout `sdOk=false`; `loggerBegin()` wordt door de hoofdlus elke 60 s opnieuw geprobeerd zolang `!sdOk`.

- [ ] **Step 1: Tests:**
  ```cpp
  TEST(day_file_name)    // {2026,10,31,...} → "/log/2026-10-31.csv"
  TEST(measurement_line) // → "2026-10-31 14:25:00;28.6;82.1;27.1;1;0;0;1;"
  TEST(measurement_invalid_and_alarms) // dhtOk=false, alarm temp_high+sd_fail → "...;;;27.1;...;temp_high,sd_fail"
  TEST(event_line)       // "2026-10-31 14:23:05;ALARM;temp_high 30.4C"
  TEST(safe_names)       // "2026-10-31.csv" ok, "events.csv" ok; "../config.json", "x/y.csv", "config.json", "a.csv.bak", "" → false
  TEST(expired)          // today 2026-10-31, 365: "2025-10-30.csv" true, "2025-10-31.csv" false, "events.csv" false
  TEST(days_from_civil)  // daysFromCivil(1970,1,1)==0, (2000,3,1)==11017
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer `logformat.*`.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Implementeer `logger.*` met `SD`/`File`. Compileer.
- [ ] **Step 6:** Commit `feat: CSV-logboek, events en opruimen`.

---

### Task 7: Klok (DS3231, NTP, tijdzone)

**Files:**
- Create: `terrarium/clock.h`, `terrarium/clock.cpp`

**Interfaces:**
- Consumes: `LocalTime` (Task 1), `Config::ntpServer`, `Config::tz` (Task 5).
- Produces:
  ```cpp
  bool clockBegin(const char* tz, const char* ntpServer);  // RTClib RTC_DS3231; RTC bevat UTC; zet systeemtijd uit RTC; setenv("TZ"), tzset()
  void clockUpdate(uint32_t nowMs, bool wifiOk);           // bij eerste WiFi en daarna elke 24 u: configTzTime; bij geslaagde sync (sntp_get_sync_status) UTC → RTC, event via clockTakeSyncEvent
  bool clockNow(LocalTime& out);                           // lokale tijd via time()+localtime_r; false als jaar < 2024
  bool clockRtcOk();                                       // RTC bereikbaar en (na begin) geldige tijd
  bool clockSetLocal(const LocalTime& t);                  // mktime(lokaal) → UTC → RTC + settimeofday
  bool clockTakeSyncEvent();                               // true één keer na elke geslaagde NTP-sync
  ```
  Tussen syncs loopt de systeemklok op de ESP32 zelf; elke 10 min wordt de systeemtijd opnieuw uit de RTC gelezen zolang er geen NTP is (drift ESP32 vermijden).

- [ ] **Step 1:** Implementeer `clock.*`.
- [ ] **Step 2:** Tijdelijke testcode in `setup()` onder `#ifdef TEST_CLOCK`: print `clockNow` elke seconde. Compileer. (Hardware-check volgt in `docs/bringup.md`, Task 14.)
- [ ] **Step 3:** Commit `feat: klok met DS3231, NTP en tijdzone`.

---

### Task 8: Sensoren en relais (hardware)

**Files:**
- Create: `terrarium/sensors.h`, `terrarium/sensors.cpp`, `terrarium/relays.h`, `terrarium/relays.cpp`

**Interfaces:**
- Consumes: `validDht`, `validDs`, `FaultCounter` (Task 3); `pins.h` (Task 1).
- Produces (`sensors.h`):
  ```cpp
  struct SensorSnapshot { float airTemp, rh, heaterTemp; bool dhtUsable, dsUsable, dhtFaulty, dsFaulty; };
  void sensorsBegin();                       // DHT22 begin; DallasTemperature begin, setWaitForConversion(false), resolutie 12 bit
  void sensorsUpdate(uint32_t nowMs);        // elke 5000 ms: DHT lezen, DS uitlezen (conversie vorige cyclus) en nieuwe requestTemperatures()
  SensorSnapshot sensorsGet();               // laatste geldige waarden + FaultCounter-status
  ```
- Produces (`relays.h`):
  ```cpp
  void relaysBegin(bool activeLow);          // per pin: eerst digitalWrite(UIT-niveau), dan pinMode(OUTPUT)
  uint8_t relaysApply(const bool on[RELAY_COUNT], bool changed[RELAY_COUNT]); // stuurt pinnen, geeft aantal wijzigingen terug
  ```

- [ ] **Step 1:** Implementeer beide modules.
- [ ] **Step 2:** Tijdelijke testcode onder `#ifdef TEST_IO`: elke 5 s sensorwaarden printen en relais R1→R4 om de beurt 1 s aan. Compileer.
- [ ] **Step 3:** Commit `feat: sensor- en relaisaansturing`.

---

### Task 9: SMS-modem (SIM7600)

**Files:**
- Create: `terrarium/sms_modem.h`, `terrarium/sms_modem.cpp` (puur), `terrarium/sms_port.h`, `terrarium/sms_port.cpp` (hardware)
- Test: `test/test_sms_modem.cpp`

**Interfaces:**
- Produces (`sms_modem.h`):
  ```cpp
  class ModemIO { public: virtual ~ModemIO() {} virtual int available() = 0; virtual int read() = 0; virtual void write(const char* s) = 0; virtual void writeByte(uint8_t b) = 0; };
  enum class SmsEventType : uint8_t { Sent, Failed, Reset };
  struct SmsEvent { SmsEventType type; char text[48]; };   // text = eerste 47 tekens van het bericht of reden
  class SmsModem {
  public:
    explicit SmsModem(ModemIO& io);
    void begin(const char* simPin, const char* phone);
    void update(uint32_t nowMs);
    bool enqueue(const char* text);        // max 10; bij vol vervalt het oudste
    bool ready() const;                    // geregistreerd op het netwerk
    int8_t signal() const;                 // laatste AT+CSQ (0..31), -1 onbekend/99
    bool popEvent(SmsEvent& ev);
  };
  ```
- State machine en time-outs exact volgens spec §8.1: `AT` (10× per 1 s) → `ATE0` → `AT+CPIN?` (bij `SIM PIN`: `AT+CPIN=<pin>`) → `AT+CMGF=1` → `AT+CSCS="GSM"` → `AT+CREG?` elke 2 s tot `,1` of `,5` → Ready (`AT+CSQ` elke 60 s) → versturen: `AT+CMGS="<nr>"\r`, wacht op `>` (10 s), tekst + byte `0x1A`, wacht op `+CMGS:` gevolgd door `OK` (60 s). Mislukt: opnieuw na 1, 5, 15 min, daarna `Failed`-event en volgende bericht. 5 min geen enkel antwoord → `Reset`-event, terug naar begin. Regels lezen tot `\n` in een buffer van 128 bytes, `\r` negeren.
- Produces (`sms_port.h`): `class SerialModemIO : public ModemIO` rond `HardwareSerial& Serial1`, met `smsPortBegin()` = `Serial1.begin(115200, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX)`.

- [ ] **Step 1: Tests** met `FakeModemIO` in het testbestand (houdt verzonden tekst bij; script van `{verwacht commando-prefix, antwoord}`; antwoordt pas bij de volgende `update`):
  ```cpp
  TEST(init_to_ready)        // script AT→OK, ATE0→OK, AT+CPIN?→"+CPIN: READY\r\nOK", CMGF→OK, CSCS→OK, CREG→"+CREG: 0,1\r\nOK" → ready()
  TEST(init_with_pin)        // CPIN? → "+CPIN: SIM PIN" → stuurt "AT+CPIN=1234\r"
  TEST(signal_parsed)        // na ready: "+CSQ: 18,99" → signal()==18; "+CSQ: 99,99" → -1
  TEST(send_sms_ok)          // enqueue("test") → stuurt AT+CMGS="+31600000000"\r, na ">" de tekst + 0x1A, na "+CMGS: 5\r\nOK" → Sent-event
  TEST(send_retry_then_fail) // geen ">" binnen 10 s → retry na 60 s, 300 s, 900 s; daarna Failed-event, wachtrij leeg
  TEST(queue_overflow)       // 11× enqueue zonder ready → oudste vervalt, 10 over
  TEST(silent_modem_reset)   // na ready 5 min geen antwoord op AT+CSQ → Reset-event en opnieuw "AT\r"
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer `sms_modem.*`.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Implementeer `sms_port.*`. Compileer.
- [ ] **Step 6:** Commit `feat: SIM7600 SMS-state-machine`.

---

### Task 10: Display

**Files:**
- Create: `terrarium/display_format.h`, `terrarium/display_format.cpp` (puur), `terrarium/display.h`, `terrarium/display.cpp`
- Test: `test/test_display_format.cpp`

**Interfaces:**
- Consumes: `SystemState` (Task 1).
- Produces:
  ```cpp
  constexpr uint8_t DISPLAY_LINES = 6, DISPLAY_COLS = 21;
  void formatDisplayLines(const SystemState& s, bool showAlarm, char lines[DISPLAY_LINES][DISPLAY_COLS + 1]);
  void displayBegin();                                  // U8G2_ST7565_ERC12864_ALT_F_4W_HW_SPI(U8G2_R0, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST), daarna u8g2.setContrast(LCD_CONTRAST) met `#define LCD_CONTRAST 0` bovenaan display.cpp
  void displayUpdate(const SystemState& s, uint32_t nowMs); // elke 1000 ms; showAlarm wisselt elke 2000 ms als er een alarm actief is
  ```
  Font `u8g2_font_6x10_tf`, regels op y = 10, 20, … 60. Alternatieve constructors (bij leeg of verschoven beeld) als commentaar in `display.cpp`: `U8G2_ST7565_ERC12864_F_4W_HW_SPI`, `U8G2_ST7565_NHD_C12864_F_4W_HW_SPI`.
- Regelinhoud exact zoals spec §7. Relaislabels `VW`, `BV`, `VE`, `LI`; scheidingsteken `:` in Auto, `*` bij handbediening; `--.-` bij ongeldige temperatuur, `--` bij ongeldige RV; regel 6 `WiFi:OK`/`WiFi:--`/`AP` en `GSM:<signaal>`/`GSM:--`; bij `showAlarm` regel 6 = `! ` + eerste actieve alarm in hoofdletters (bv. `! TEMP HOOG 30.4C`), afgekapt op 21 tekens. Zonder geldige tijd regel 1 `--:--  GEEN TIJD`.

- [ ] **Step 1: Tests:**
  ```cpp
  TEST(lines_normal)   // regel1 "31-10-26 14:23   DAG", regel2 "Lucht 28.6C RV 82%", regel3 "Bron  27.1C", regel4 "VW:AAN  BV:UIT", regel5 "VE:UIT  LI:AAN", regel6 "WiFi:OK GSM:18"
  TEST(lines_manual)   // relayMode[R_VENT]=ManualOn → regel5 begint met "VE*AAN"
  TEST(lines_invalid)  // dhtOk=false → regel2 "Lucht --.-C RV --%"
  TEST(lines_alarm)    // alarm temp_high, showAlarm=true → regel6 "! TEMP HOOG 30.4C"
  TEST(lines_fit)      // in alle varianten strlen(lines[i]) ≤ 21
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer `display_format.*`.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Implementeer `display.*` (SPI wordt in `setup()` gestart met `SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI)`; SD en U8g2 delen deze bus). Compileer.
- [ ] **Step 6:** Commit `feat: schermweergave ST7565R`.

---

### Task 11: Web-JSON (status, verzoeken)

**Files:**
- Create: `terrarium/web_json.h`, `terrarium/web_json.cpp`
- Test: `test/test_web_json.cpp`

**Interfaces:**
- Consumes: `SystemState` (Task 1), `alarmKey` (Task 4).
- Produces:
  ```cpp
  size_t buildStatusJson(const SystemState& s, char* buf, size_t len);
  bool parseRelayCommand(const char* json, RelayId& r, RelayMode& m);   // {"relay":1..4,"mode":"on"|"off"|"auto"}
  bool parseIsoLocal(const char* s, LocalTime& t);                     // "2026-10-31T14:23:00", geldige datum/tijd
  bool parseSetupRequest(const char* json, char* ssid, size_t ssidLen, char* pass, size_t passLen, char* webPass, size_t webPassLen); // web_password ≥ 6 tekens verplicht
  ```
- Status-JSON: structuur als spec §6.2, **met één afwijking**: per relais `"manual_remaining_s"` (seconden) in plaats van `"manual_until"`, omdat de ESP32 dan geen datumrekenkunde hoeft te doen. Ongeldige meting → `null`. Relaisnamen `verwarming`, `bevochtiging`, `ventilatie`, `verlichting`; mode `auto`/`manual`.

- [ ] **Step 1: Tests:**
  ```cpp
  TEST(status_json_fields) // parse output met ArduinoJson: air_temp 28.6, relays[2]["mode"]=="manual", manual_remaining_s==1500, alarms[0]=="temp_high", gsm.signal==18
  TEST(status_json_null_when_invalid) // dhtOk=false → air_temp en rh zijn null
  TEST(status_json_buffer_small)      // len 32 → return 0, geen overloop
  TEST(relay_cmd)          // {"relay":3,"mode":"on"} → R_VENT, ManualOn; relay 0 of 5 → false; mode "aan" → false
  TEST(iso_local)          // "2026-10-31T14:23:00" ok; "2026-02-30T10:00:00" false; "2026-10-31 14:23" false
  TEST(setup_request)      // korte web_password "abc" → false
  ```
- [ ] **Step 2:** Run `make -C test` → FAIL.
- [ ] **Step 3:** Implementeer.
- [ ] **Step 4:** Run `make -C test` → PASS.
- [ ] **Step 5:** Commit `feat: JSON voor status-API en verzoeken`.

---

### Task 12: Netwerk en webserver

**Files:**
- Create: `terrarium/net_wifi.h`, `terrarium/net_wifi.cpp`, `terrarium/web.h`, `terrarium/web.cpp`, `terrarium/webpage.h`

**Interfaces:**
- Consumes: `Config`, `parseConfig`, `serializeConfig` (Task 5); `configSave` (Task 5); `buildStatusJson`, `parseRelayCommand`, `parseIsoLocal`, `parseSetupRequest` (Task 11); `isSafeLogName` (Task 6); `clockSetLocal` (Task 7); `setOverride` (Task 2).
- Produces (`net_wifi.h`):
  ```cpp
  void networkBegin(const Config& c);      // lege ssid → AP "Terrarium-Setup" (open, 192.168.4.1); anders STA + hostname + mDNS "<hostname>.local"
  void networkUpdate(uint32_t nowMs);      // STA: bij geen verbinding elke 60 s WiFi.reconnect(); events WIFI weg/terug
  bool networkConnected(); bool networkApMode(); int8_t networkRssi(); void networkIp(char* buf, size_t len);
  bool networkTakeEvent(bool& connected);  // true bij verandering van verbinding
  ```
- Produces (`web.h`):
  ```cpp
  struct WebContext { Config* config; Overrides* overrides; const SystemState* state; void (*onSmsTest)(); void (*onConfigChanged)(); };
  void webBegin(WebContext& ctx);          // WebServer op poort 80; routes spec §6.1/§6.2; in AP-modus alleen GET / (setupformulier) en POST /api/setup
  void webUpdate();                        // server.handleClient()
  ```
- Auth: `server.authenticate(webUser, webPass)`; als `webPass` leeg is → wijzigende routes antwoorden `403` met tekst `Stel eerst een webwachtwoord in (config.json of setup)`. `POST /api/config` gebruikt `parseConfig(..., emptySecretKeepsOld=true)`, daarna `configSave` en `onConfigChanged`; antwoord `{"ok":true,"rejected":N,"error":"..."}` (400 als `ok=false`). `POST /api/setup`: velden in config, `configSave`, antwoord 200, daarna `ESP.restart()` na 1 s. Log-download: `/logs/<naam>` alleen als `isSafeLogName`, anders 404; `server.streamFile` met `text/csv`.
- `webpage.h`: drie `PROGMEM`-strings (`PAGE_DASHBOARD`, `PAGE_SETTINGS`, `PAGE_SETUP`) met inline CSS/JS, Nederlandstalig. Dashboard haalt elke 5 s `/api/status` op en heeft per relais knoppen AAN/UIT/AUTO (`POST /api/relay`); instellingenpagina vult zich via `GET /api/config` en verstuurt via `POST /api/config`, en heeft knoppen voor "Klok zetten" (met browsertijd) en "Test-SMS"; logpagina wordt server-side als HTML-lijst gegenereerd.

- [ ] **Step 1:** Implementeer `network.*`.
- [ ] **Step 2:** Implementeer `web.*` en `webpage.h`.
- [ ] **Step 3:** Compileer.
- [ ] **Step 4:** Commit `feat: WiFi, setupmodus en webinterface`.

---

### Task 13: Integratie in `terrarium.ino`

**Files:**
- Modify: `terrarium/terrarium.ino`

**Interfaces:**
- Consumes: alles hierboven.

`setup()` in deze volgorde: `relaysBegin(true)` (vóór alles, met de standaardwaarde; na het laden van de config opnieuw als `relayActiveLow` false is) → `Serial.begin` → `Wire.begin(PIN_SDA, PIN_SCL)` → `SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI)` → `SD.begin(PIN_SD_CS)` → `configLoad` → `clockBegin` → `loggerBegin` → `sensorsBegin` → `displayBegin` → `smsPortBegin` + `modem.begin` → `networkBegin` → `webBegin` → `esp_task_wdt` met 30 s → `controlInit`, `overridesInit`, `alarmsInit` → event `START` + `formatInfoSms("Herstart. Laatste log <last.txt>")` (zonder last.txt: `"Herstart"`) in de SMS-wachtrij. Bij `ConfigSource::Eeprom`/`Defaults`: event `CONFIG` met de bron.

`loop()` in de volgorde uit spec §4.3:
1. `clockUpdate` → `state.time`, `timeValid`, `rtcOk`; NTP-event loggen.
2. `sensorsUpdate` → `sensorsGet` → state (`state.dhtOk = dhtUsable`, `state.dsOk = dsUsable`; `ControlInput.dhtOk/dsOk` krijgen dezelfde *usable*-waarden, `AlarmInput` krijgt usable én faulty).
3. `expireOverrides` (+ event `HAND` bij verlopen) → `decide` → `relaysApply` → bij wijzigingen event `RELAIS` (`R3 ventilatie AAN (auto|hand|veiligheid)`).
4. `evaluateAlarms` → per transitie event `ALARM`/`HERSTELD`; bij `sendSms` → `formatAlarmSms` → `modem.enqueue`.
5. `modem.update`; `popEvent` → event `SMS`.
6. Elke `logIntervalMin` op hele minuten (`minute % interval == 0`, één keer per minuut) → `loggerMeasurement`; om 03:00 → `loggerDailyMaintenance` + event `HEAP` met `ESP.getFreeHeap()`; zolang `!sdOk` elke 60 s `SD.begin` + `loggerBegin` opnieuw.
7. `displayUpdate`.
8. `networkUpdate` (+ events), `webUpdate`.
9. `esp_task_wdt_reset()`.

Bij `onConfigChanged`: event `CONFIG`; `relaysBegin` alleen opnieuw als `relayActiveLow` wijzigt.

- [ ] **Step 1:** Implementeer `setup()` en `loop()` zoals hierboven; verwijder de tijdelijke `#ifdef TEST_*`-code niet (die blijft beschikbaar voor bring-up).
- [ ] **Step 2:** Run `make -C test` → alle pc-tests PASS.
- [ ] **Step 3:** Compileer → succes, en noteer flash- en RAM-gebruik in de commit-boodschap.
- [ ] **Step 4:** Commit `feat: integratie hoofdlus`.

---

### Task 14: Bring-up- en acceptatiechecklist

**Files:**
- Create: `docs/bringup.md`
- Modify: `README.md` (installatie: Arduino IDE-instellingen, libraries, SD-kaart voorbereiden, link naar bringup)

- [ ] **Step 1:** Schrijf `docs/bringup.md` met per onderdeel een afvinkbare stap (aansluiten, `#define TEST_CLOCK` / `TEST_IO` gebruiken, verwacht resultaat): RTC + EEPROM, DHT22, DS18B20, relais (active-low check), scherm (constructor-alternatieven bij leeg beeld), SD, SIM7600 (stroom, test-SMS), WiFi-setupmodus, webinterface. Daarna de integratietests en duurtest uit spec §11 (stappen 3–4) en de acceptatiecriteria uit spec §12, plus een tabel met alle FR-ID's uit spec §2.1 en een kolom "getest op / door".
- [ ] **Step 2:** Werk `README.md` bij: board-instellingen (*ESP32C6 Dev Module*, *USB CDC On Boot: Enabled*), libraries met versies, `sd-template/config.json` kopiëren naar de kaart en invullen, `make -C test` voor pc-tests.
- [ ] **Step 3:** Commit `docs: bring-up- en acceptatiechecklist`.
