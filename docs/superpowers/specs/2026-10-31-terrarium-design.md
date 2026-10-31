# Terrarium-besturing ESP32-C6 — Specificatie & Ontwerp

| | |
|---|---|
| **Datum** | 2026-10-31 |
| **Status** | Ter review |
| **Platform** | ESP32-C6-DevKitC-1, Arduino IDE, Espressif ESP32 Arduino core 3.x |

---

## 1. Doel en scope

Een zelfstandige besturing die het klimaat in een terrarium automatisch regelt en bewaakt:

- temperatuur en luchtvochtigheid meten en regelen (verwarming, bevochtiging, ventilatie);
- verlichting schakelen volgens een dag/nacht-schema;
- configuratie en logboek opslaan op een micro-SD-kaart;
- storingen melden per SMS;
- meetwaarden tonen, instellingen wijzigen en relais schakelen via een eigen webpagina (WiFi, lokaal netwerk).

**Buiten scope:** clouddiensten, MQTT/Home Assistant, SMS-commando's (alleen uitgaande meldingen), toegang van buiten het thuisnetwerk, HTTPS.

---

## 2. Requirements

### 2.1 Functionele requirements

#### Meten
| ID | Requirement |
|---|---|
| FR-M1 | Het systeem meet luchttemperatuur en relatieve vochtigheid (RV) met een DHT22, elke 5 s. |
| FR-M2 | Het systeem meet de temperatuur nabij de warmtebron met een DS18B20, elke 5 s. |
| FR-M3 | Een meting buiten het fysisch mogelijke bereik (DHT22: −40…80 °C, 0…100 % RV; DS18B20: −55…125 °C, of de foutwaarden NaN / −127 °C / 85 °C bij opstart) is ongeldig en wordt genegeerd. |
| FR-M4 | Na 3 opeenvolgende ongeldige metingen van een sensor wordt die sensor als *gestoord* gemarkeerd; één geldige meting heft dit op. |

#### Dag/nacht en verlichting
| ID | Requirement |
|---|---|
| FR-D1 | Dag loopt van 08:00 tot 20:00, nacht van 20:00 tot 08:00 (instelbaar). |
| FR-D2 | Relais R4 (verlichting) is aan tijdens dag en uit tijdens nacht. |

#### Verwarming (R1)
| ID | Requirement |
|---|---|
| FR-V1 | Streeftemperatuur (DHT22) is 29 °C overdag en 22 °C 's nachts (instelbaar). |
| FR-V2 | Verwarming gaat aan onder *streef − 0,5 °C* en uit boven *streef + 0,5 °C* (hysterese instelbaar). |
| FR-V3 | Verwarming is altijd uit als de DS18B20 boven 30 °C meet (oververhittingsbeveiliging, instelbaar); ze mag pas weer aan als de DS18B20 onder *grens − 1 °C* is. |
| FR-V4 | Verwarming is altijd uit als de DHT22 of de DS18B20 gestoord is. |

#### Bevochtiging (R2)
| ID | Requirement |
|---|---|
| FR-B1 | Bevochtiging gaat aan onder 70 % RV en uit boven 75 % RV (instelbaar). |
| FR-B2 | Bevochtiging staat maximaal 10 min aaneengesloten aan, daarna minimaal 5 min pauze (instelbaar). |
| FR-B3 | Bevochtiging is altijd uit als de DHT22 gestoord is. |

#### Ventilatie (R3)
| ID | Requirement |
|---|---|
| FR-VE1 | Ventilatie gaat aan boven 95 % RV en uit onder 90 % RV (instelbaar). |
| FR-VE2 | Ventilatie draait daarnaast elke 2 uur gedurende 5 min, dag en nacht, op vaste tijden vanaf 00:00 (00:00, 02:00, … — instelbaar). |
| FR-VE3 | Verwarming en ventilatie mogen tegelijk aan zijn. |

#### Handbediening
| ID | Requirement |
|---|---|
| FR-H1 | Elk relais kan via de webpagina handmatig op AAN, UIT of AUTO gezet worden. |
| FR-H2 | Een handmatige stand vervalt automatisch na 30 min (instelbaar); daarna weer AUTO. |
| FR-H3 | Veiligheidsregels (FR-V3, FR-V4, FR-B3) gaan altijd vóór een handmatige stand. |

#### Alarmen en SMS
| ID | Requirement |
|---|---|
| FR-A1 | Alarm *temperatuur te laag*: DHT22 < 20 °C gedurende ≥ 5 min aaneengesloten. |
| FR-A2 | Alarm *temperatuur te hoog*: DHT22 > 30 °C gedurende ≥ 5 min aaneengesloten. |
| FR-A3 | Alarm *oververhitting warmtebron*: DS18B20 > 30 °C (direct, geen vertraging). |
| FR-A4 | Alarm *vochtigheid te laag*: RV < 70 % gedurende ≥ 30 min. |
| FR-A5 | Alarm *vochtigheid te hoog*: RV > 95 % gedurende ≥ 30 min. |
| FR-A6 | Alarm *sensorstoring*: per sensor (DHT22, DS18B20) zodra FR-M4 optreedt. |
| FR-A7 | Alarm *SD-kaart*: kaart ontbreekt, niet leesbaar, of schrijven mislukt. |
| FR-A8 | Alarm *RTC*: DS3231 niet bereikbaar of tijd ongeldig (vóór 2024). |
| FR-A9 | Alarm *WiFi weg*: langer dan 30 min geen verbinding. |
| FR-A10 | Melding *herstart*: bij elke opstart, met (indien bekend) het tijdstip van de laatste logregel vóór de uitval. |
| FR-A11 | Bij herstel van een alarm wordt een korte *hersteld*-SMS verstuurd. |
| FR-A12 | Per alarmsoort maximaal 1 SMS per 60 min (instelbaar). Hersteld-meldingen vallen onder dezelfde limiet. |
| FR-A13 | SMS gaat naar één telefoonnummer uit de configuratie (internationaal formaat, `+31…`). |
| FR-A14 | Alle alarmgrenzen en vertragingen zijn instelbaar. |

#### Logboek
| ID | Requirement |
|---|---|
| FR-L1 | Elke 5 min wordt een meetregel weggeschreven naar `/log/JJJJ-MM-DD.csv`. |
| FR-L2 | Gebeurtenissen (relais schakelt, alarm aan/hersteld, SMS verstuurd/mislukt, handbediening, config gewijzigd, herstart, WiFi weg/terug, NTP-sync) gaan naar een maandbestand `/log/events-JJJJ-MM.csv`. |
| FR-L3 | Meetbestanden ouder dan 365 dagen worden automatisch verwijderd (dagelijks om 03:00). Eventbestanden `events-JJJJ-MM.csv` worden met dezelfde bewaartermijn verwijderd, gerekend vanaf de 1e van de maand na de laatste regel. |
| FR-L4 | Logbestanden zijn via de webpagina te downloaden. |

#### Display
| ID | Requirement |
|---|---|
| FR-S1 | Het scherm toont op één pagina: datum/tijd, dag/nacht, temperatuur + RV (DHT22), temperatuur warmtebron (DS18B20), stand van R1–R4, WiFi-status en GSM-signaal. |
| FR-S2 | Bij een actief alarm knippert de onderste regel tussen status en alarmtekst. |
| FR-S3 | Het scherm ververst elke seconde. |

#### Web
| ID | Requirement |
|---|---|
| FR-W1 | De ESP32 serveert een webpagina op `http://terrarium.local` (mDNS) en op zijn IP-adres. |
| FR-W2 | Kijken (status, logbestanden) kan zonder inloggen. |
| FR-W3 | Instellingen wijzigen, relais schakelen, tijd zetten en test-SMS versturen vereist inloggen (HTTP Basic Auth, gebruiker/wachtwoord uit de configuratie). |
| FR-W4 | Er is een JSON-API (`/api/status`) voor andere software. |

#### Netwerk, tijd en opstart
| ID | Requirement |
|---|---|
| FR-N1 | Bij WiFi-verlies blijft de regeling werken; elke 60 s een herverbindpoging. |
| FR-N2 | Zonder geldige WiFi-gegevens start een access point `Terrarium-Setup` met configuratiepagina op `http://192.168.4.1`. |
| FR-N3 | De DS3231 is de klok; bij WiFi wordt dagelijks (en bij opstart) via NTP gesynchroniseerd. Tijdzone Europe/Amsterdam met automatische zomer-/wintertijd. |
| FR-N4 | Bij opstart worden alle relais direct UIT gezet; de regeling neemt binnen 10 s over. |
| FR-N5 | Zonder SD-kaart draait het systeem op de configuratie uit de AT24C32-backup, en anders op ingebouwde standaardwaarden. |
| FR-N6 | Een hardware-watchdog herstart de ESP32 als de hoofdlus langer dan 30 s niet rondkomt. |

### 2.2 Niet-functionele requirements

| ID | Requirement |
|---|---|
| NFR-1 | Ontwikkeld in de Arduino IDE met de Espressif ESP32 core 3.x; extra libraries alleen via de Library Manager (zie §5.3). |
| NFR-2 | De hoofdlus is non-blocking: geen `delay()` > 10 ms buiten `setup()`; een rondgang duurt normaal < 50 ms. |
| NFR-3 | Geheimen (WiFi-wachtwoord, webwachtwoord, SIM-pincode, telefoonnummer) staan alleen in `config.json` op de SD-kaart en nooit in de broncode of de repository. |
| NFR-4 | De regellogica (`control`) en alarmlogica (`alarms`) zijn hardware-onafhankelijk en op een pc te testen. |
| NFR-5 | SMS-teksten gebruiken alleen GSM-7 tekens (geen accenten) en zijn ≤ 160 tekens. |
| NFR-6 | Het systeem draait onbeheerd 24/7; geheugengebruik mag niet groeien in de tijd (geen heap-fragmentatie door herhaalde `String`-concatenatie in de hoofdlus). |

---

## 3. Hardware

### 3.1 Componenten

| Component | Type | Opmerking |
|---|---|---|
| Processorboard | ESP32-C6-DevKitC-1 | Programmeren/serial via de **native USB-poort** (GPIO12/13) |
| Klokmodule | DS3231 + AT24C32 | I2C; EEPROM wordt gebruikt voor config-backup |
| Temperatuursensor | DS18B20 | 1-Wire, nabij (niet óp) de warmtebron |
| Temp/RV-sensor | DHT22 / AM2302 | Vervangt de oorspronkelijk genoemde DHT11 |
| Relaismodule | 4-kanaals met optocoupler | Meestal active-LOW (instelbaar) |
| SMS-module | SIM7600 (4G/LTE) | Eigen voeding; 2G-modules (SIM800) zijn ongeschikt i.v.m. uitfasering 2G in NL |
| Scherm | GMG12864-06D, 128×64, controller **ST7565R**, SPI | 3,3 V |
| SD-kaartmodule | micro-SD breakout **zonder** regelaar/niveau-omzetter, 3,3 V | Bv. SparkFun BOB-00544 of Adafruit 254. Niet: Catalex-module met AMS1117/74LVC125 |
| SD-kaart | 8–32 GB, FAT32, bij voorkeur "High Endurance" | |

### 3.2 Pinindeling

Vermeden pinnen: GPIO8/9 (strapping, GPIO8 = RGB-LED), GPIO12/13 (USB), GPIO16/17 (UART0 naar USB-brug), GPIO24–30 (flash). GPIO4/5/15 (strapping) alleen voor uitgangen naar randapparatuur waarvan de bootstand niet uitmaakt.

| Functie | GPIO | Opmerking |
|---|---|---|
| I2C SDA | 23 | DS3231, AT24C32 |
| I2C SCL | 22 | |
| SPI SCK | 21 | Gedeeld: SD + scherm |
| SPI MOSI | 19 | |
| SPI MISO | 20 | Alleen SD |
| SD CS | 18 | |
| Scherm CS | 6 | |
| Scherm A0 (DC) | 5 | strapping, alleen uitgang |
| Scherm RST | 4 | strapping, alleen uitgang |
| DHT22 data | 3 | 10 kΩ pull-up naar 3,3 V (als niet op module) |
| DS18B20 data | 2 | 4,7 kΩ pull-up naar 3,3 V |
| R1 Verwarming | 0 | |
| R2 Bevochtiging | 1 | |
| R3 Ventilatie | 10 | |
| R4 Verlichting | 11 | |
| SIM7600 TXD → ESP RX | 7 | `Serial1`, 115200 baud |
| ESP TX → SIM7600 RXD | 15 | strapping, UART-TX |

Alle pinnummers staan centraal in `pins.h`.

### 3.3 Voeding en elektrische eisen

1. Hoofdvoeding 5 V / ≥ 3 A voor ESP32, scherm, sensoren en relaisspoelen.
2. SIM7600 op een eigen step-down-voeding die ≥ 2 A piek levert, met ≥ 1000 µF elco dicht bij de module. Gemeenschappelijke GND met de ESP32. UART op 3,3 V-niveau (controleer de breakout; anders niveau-omzetter).
3. Relaispinnen worden in `setup()` als eerste op de UIT-stand gezet (rekening houdend met `relay_active_low`).
4. 230 V-schakeling (verwarming, vernevelaar, lamp) in een gesloten behuizing met zekering; aansluiten door een bevoegd persoon indien geen ervaring.

---

## 4. Software-architectuur

### 4.1 Aanpak

Eén modulaire Arduino-sketch met een **non-blocking coöperatieve hoofdlus** (geen FreeRTOS-taken). Elk onderdeel heeft een `begin()` en een `update(now)` die snel terugkeert en zelf bijhoudt (met `millis()`) wanneer het weer iets moet doen. Reden: de ESP32-C6 heeft één rekenkern, en zo is er geen mutex-beheer nodig voor de gedeelde SPI- en I2C-bus.

### 4.2 Mapstructuur

```
terrarium/                 Arduino-sketchmap (naam = .ino)
  terrarium.ino            setup() + loop()
  pins.h                   alle pinnummers
  state.h                  SystemState (gedeelde toestand)
  config.h / .cpp          Config-struct, laden/opslaan SD + EEPROM, standaardwaarden
  clock.h / .cpp           DS3231, NTP, tijdzone, dag/nacht
  sensors.h / .cpp         DHT22, DS18B20, validatie, storing
  control.h / .cpp         pure regellogica (geen hardware)
  relays.h / .cpp          uitgangen, handbediening, logging schakelmomenten
  alarms.h / .cpp          pure alarmlogica (geen hardware)
  sms.h / .cpp             SIM7600 AT-state-machine + wachtrij
  logger.h / .cpp          CSV-logboek, events, opruimen
  display.h / .cpp         U8g2-schermopbouw
  network.h / .cpp         WiFi, AP-setupmodus, mDNS
  web.h / .cpp             WebServer, pagina's, API
  webpage.h                HTML/JS van de webpagina als PROGMEM-string
test/                      pc-tests voor control en alarms (g++)
docs/                      documentatie
sd-template/config.json    voorbeeldconfiguratie (zonder geheimen)
```

### 4.3 Onderdelen en interfaces

| Onderdeel | Verantwoordelijkheid | Leest | Schrijft |
|---|---|---|---|
| `config` | Instellingen laden/valideren/opslaan; volgorde: SD → EEPROM → standaard | SD, EEPROM | `Config` |
| `clock` | Huidige lokale tijd; dag/nacht; NTP dagelijks + bij opstart; RTC-fout detecteren | DS3231, NTP | `state.time`, `state.isDay`, `state.rtcOk` |
| `sensors` | Metingen elke 5 s, validatie, storingsteller | DHT22, DS18B20 | `state.airTemp`, `state.rh`, `state.heaterTemp`, `state.dhtOk`, `state.dsOk` |
| `control` | `ControlOutput decide(const ControlInput&, ControlMemory&)` — gewenste stand R1–R4 | state, config | gewenste relaisstanden |
| `relays` | Handbediening toepassen (met vervaltijd), veiligheidsregels altijd laatst, pinnen aansturen, wijzigingen loggen | gewenste standen, overrides | pinnen, `state.relay[]` |
| `alarms` | Per alarmsoort: actief/inactief met vertraging, SMS-limiet, hersteld-melding | state, config | `state.alarms`, SMS-wachtrij, events |
| `sms` | Initialisatie modem, signaalsterkte (elke 60 s `AT+CSQ`), registratie (`AT+CREG?`), versturen uit wachtrij | SMS-wachtrij | `state.gsmSignal`, `state.gsmOk`, events |
| `logger` | Meetregel elke 5 min, events direct, opruimen om 03:00 | state, events | SD |
| `display` | Pagina tekenen elke 1 s | state | scherm |
| `network` | Verbinden/herverbinden, AP-modus, mDNS, WiFi-weg-timer | config | `state.wifiOk`, `state.ip` |
| `web` | HTTP-verzoeken afhandelen | state, config | config, overrides, RTC |

**Volgorde in `loop()`:**
`clock → sensors → control → relays → alarms → sms → logger → display → network → web → watchdog-reset`

`control` en `alarms` krijgen alle invoer (inclusief de tijd) als parameter en raken geen hardware aan; daardoor zijn ze op een pc te testen (NFR-4).

### 4.4 Regellogica (samenvatting)

Per rondgang, met `streef = isDay ? dag_streef : nacht_streef`:

| Relais | Aan | Uit | Altijd uit (veiligheid) |
|---|---|---|---|
| R1 Verwarming | `airTemp < streef − hyst` | `airTemp > streef + hyst` | DS18B20 > grens (tot < grens − 1 °C), DHT22 of DS18B20 gestoord |
| R2 Bevochtiging | `rh < 70` | `rh > 75` of 10 min aan (dan 5 min pauze) | DHT22 gestoord |
| R3 Ventilatie | `rh > 95` of in vast ventilatievenster (elke 2 u, 5 min) | `rh < 90` en buiten venster | — (bij DHT22-storing alleen het vaste schema) |
| R4 Verlichting | dag | nacht | — |

Tussen de aan- en uitgrens blijft een relais in zijn vorige stand (hysterese). Daarna: handbediening toepassen, daarna de veiligheidsregels (FR-H3).

---

## 5. Configuratie

### 5.1 Bestand `/config.json` op de SD-kaart

```json
{
  "wifi":   { "ssid": "", "password": "", "hostname": "terrarium" },
  "web":    { "user": "admin", "password": "" },
  "sms":    { "phone": "+316XXXXXXXX", "sim_pin": "", "limit_min": 60 },
  "schedule": { "day_start": "08:00", "night_start": "20:00" },
  "heating":  { "day_c": 29.0, "night_c": 22.0, "hyst_c": 0.5, "source_max_c": 30.0 },
  "humidity": { "on_below": 70, "off_above": 75, "max_on_min": 10, "pause_min": 5 },
  "vent":     { "on_above": 95, "off_below": 90, "interval_min": 120, "duration_min": 5 },
  "alarms": {
    "temp_low_c": 20.0, "temp_high_c": 30.0, "temp_delay_min": 5,
    "rh_low": 70, "rh_high": 95, "rh_delay_min": 30,
    "wifi_lost_min": 30
  },
  "manual_timeout_min": 30,
  "relay_active_low": true,
  "log": { "interval_min": 5, "retention_days": 365 },
  "ntp": { "server": "pool.ntp.org", "tz": "CET-1CEST,M3.5.0,M10.5.0/3" }
}
```

- `sd-template/config.json` in de repository bevat dit bestand met lege geheimen; de gebruiker vult het in en zet het op de kaart.
- Ontbrekende velden krijgen de standaardwaarde; ongeldige waarden (bv. `off_above ≤ on_below`) worden geweigerd met een event en melding in de webinterface, waarna de vorige geldige waarde blijft gelden.

### 5.2 Backup in AT24C32

- Na elke succesvolle laad- of opslagactie wordt de compacte JSON (zonder spaties, < 2 KB) met een lengte + CRC32 naar de EEPROM geschreven (alleen als de inhoud veranderd is, i.v.m. slijtage).
- Bij opstart: SD leesbaar en geldig → SD gebruiken; anders EEPROM met geldige CRC; anders ingebouwde standaardwaarden.

### 5.3 Libraries

Uit de ESP32 core: `WiFi`, `WebServer`, `ESPmDNS`, `SD`, `SPI`, `Wire`, `esp_task_wdt`, `time.h` (NTP).
Via Library Manager: **U8g2**, **DHT sensor library** (Adafruit, + Adafruit Unified Sensor), **OneWire**, **DallasTemperature**, **RTClib** (Adafruit), **ArduinoJson** (v7).

---

## 6. Webinterface en API

### 6.1 Pagina's

| Pad | Auth | Inhoud |
|---|---|---|
| `GET /` | nee | Dashboard: metingen, relaisstanden met knoppen AAN/UIT/AUTO, alarmen, WiFi/GSM. Ververst elke 5 s via `/api/status`. |
| `GET /settings` | ja | Formulier voor alle instellingen uit §5.1 (wachtwoordvelden worden nooit teruggestuurd, leeg = ongewijzigd). |
| `GET /logs` | nee | Lijst van logbestanden met downloadlinks. |
| `GET /logs/<bestand>` | nee | Download CSV (alleen bestanden in `/log/`, geen `..` in pad). |

### 6.2 API

| Methode + pad | Auth | Beschrijving |
|---|---|---|
| `GET /api/status` | nee | Actuele toestand (zie voorbeeld). |
| `GET /api/config` | ja | Configuratie zonder geheimen. |
| `POST /api/config` | ja | JSON met (een deel van) de configuratie; valideren, toepassen, opslaan op SD + EEPROM. Antwoord 200 of 400 met foutmelding. |
| `POST /api/relay` | ja | `{"relay":1..4,"mode":"on"\|"off"\|"auto"}` |
| `POST /api/time` | ja | `{"datetime":"2026-10-31T14:23:00"}` — RTC handmatig zetten (lokale tijd). |
| `POST /api/sms/test` | ja | Stuurt een test-SMS. |

Voorbeeld `/api/status`:
```json
{
  "time": "2026-10-31T14:23:05", "is_day": true,
  "air_temp": 28.6, "rh": 82.1, "heater_temp": 27.1,
  "dht_ok": true, "ds_ok": true,
  "relays": [
    {"name":"verwarming","on":true,"mode":"auto"},
    {"name":"bevochtiging","on":false,"mode":"auto"},
    {"name":"ventilatie","on":false,"mode":"manual","manual_until":"2026-10-31T14:50:00"},
    {"name":"verlichting","on":true,"mode":"auto"}
  ],
  "alarms": ["temp_high"],
  "wifi": {"ok":true,"rssi":-61,"ip":"192.168.1.42"},
  "gsm":  {"ok":true,"signal":18},
  "sd_ok": true, "rtc_ok": true, "uptime_s": 86400
}
```

### 6.3 Beveiliging

HTTP Basic Auth voor alle wijzigende acties. Er is geen HTTPS: de interface is bedoeld voor het eigen (vertrouwde) thuisnetwerk en mag niet via port-forwarding op internet gezet worden. In AP-setupmodus is alleen de WiFi-configuratiepagina beschikbaar.

---

## 7. Display-indeling (128×64, font ±6×10 px, 6 regels)

```
31-10-26 14:23   DAG
Lucht 28.6C RV 82%
Bron  27.1C
VW:AAN  BV:UIT
VE:UIT  LI:AAN
WiFi:OK GSM:18
```
Bij een actief alarm wisselt regel 6 elke 2 s met de alarmtekst (bv. `! TEMP HOOG 30.4C`). Een handmatig geschakeld relais toont een `*` (bv. `VE*AAN`).

---

## 8. SMS

### 8.1 Modem-afhandeling (SIM7600)

State machine, elke stap met time-out:
1. `AT` (tot 10× met 1 s ertussen) → `ATE0`
2. `AT+CPIN?` → indien `SIM PIN`: `AT+CPIN=<pin>`
3. `AT+CMGF=1` (tekstmodus), `AT+CSCS="GSM"`
4. Wachten op registratie: `AT+CREG?` / `AT+CEREG?` = 1 of 5
5. Gereed: elke 60 s `AT+CSQ` voor signaalsterkte
6. Versturen: `AT+CMGS="<nr>"` → wacht op `>` → tekst + Ctrl-Z → wacht op `+CMGS` (time-out 60 s)

Een mislukte verzending wordt maximaal 3× opnieuw geprobeerd (na 1, 5 en 15 min), daarna gelogd als mislukt. Bij 5 minuten geen antwoord van het modem: opnieuw initialiseren vanaf stap 1. De wachtrij bevat maximaal 10 berichten; bij overloop vervalt het oudste.

### 8.2 Berichtformaat

`TERRARIUM <ALARM|HERSTELD|INFO>: <omschrijving> <waarde> (<dd-mm hh:mm>)`

Voorbeelden:
- `TERRARIUM ALARM: Temperatuur te hoog 30.4C (31-10 14:23)`
- `TERRARIUM ALARM: Warmtebron te heet 31.2C, verwarming uit (31-10 14:23)`
- `TERRARIUM HERSTELD: Temperatuur normaal 28.9C (31-10 14:41)`
- `TERRARIUM INFO: Herstart. Laatste log 31-10 03:15 (31-10 07:02)`

---

## 9. Logboek

**Meetbestand** `/log/JJJJ-MM-DD.csv` (scheidingsteken `;`, decimaalpunt):
```
tijd;lucht_c;rv_pct;bron_c;r1_verw;r2_bevocht;r3_vent;r4_licht;alarmen
2026-10-31 14:25:00;28.6;82.1;27.1;1;0;0;1;
```
Een ongeldige meting wordt als leeg veld weggeschreven.

**Events** `/log/events-JJJJ-MM.csv` (een bestand per maand, zodat een download klein blijft):
```
tijd;soort;omschrijving
2026-10-31 14:23:05;ALARM;temp_high 30.4C
2026-10-31 14:23:20;SMS;verstuurd temp_high
2026-10-31 14:30:00;RELAIS;R3 ventilatie AAN (schema)
```

Schrijfacties openen het bestand, schrijven één regel en sluiten het direct weer, zodat er bij stroomuitval hooguit één regel verloren gaat.

---

## 10. Foutafhandeling

| Situatie | Gedrag |
|---|---|
| DHT22 gestoord | R1 en R2 uit; R3 alleen vast schema; alarm + SMS; scherm toont `--.-` |
| DS18B20 gestoord | R1 uit; alarm + SMS |
| Warmtebron > 30 °C | R1 direct uit (ook bij handbediening); alarm + SMS |
| SD ontbreekt/defect | Config uit EEPROM of standaard; loggen gepauzeerd; elke 60 s opnieuw proberen te mounten; alarm + SMS |
| RTC defect/ongeldig | Tijd uit NTP indien WiFi; zonder geldige tijd: R4 uit, schema-afhankelijke functies (dag/nacht, ventilatievenster) gebruiken nachtwaarden; alarm + SMS |
| WiFi weg | Regeling werkt door; herverbinden elke 60 s; na 30 min alarm + SMS |
| GSM/modem defect | Alarmen worden wel gelogd, op scherm en web getoond; modem wordt periodiek opnieuw geïnitialiseerd |
| Config ongeldig | Ongeldige velden → standaardwaarde; event + melding op webpagina |
| Hoofdlus hangt | Watchdog (30 s) → herstart → herstart-SMS |

---

## 11. Teststrategie

1. **Pc-tests (g++)** in `test/` voor `control` en `alarms`: hysterese, dag/nacht-overgang, ventilatievenster, max-aan-tijd bevochtiging, veiligheidsregels, voorrang veiligheid boven handbediening, alarmvertragingen, SMS-limiet, hersteld-meldingen.
2. **Hardware-bring-up per onderdeel**: aparte kleine testschetsen of een `#define TEST_<ONDERDEEL>` voor RTC, sensoren, relais, scherm, SD, SIM7600.
3. **Integratietest op de bank**: sensoren verwarmen/afkoelen (hand, föhn, ijsblokje) en controleren dat relais, scherm, log en SMS kloppen.
4. **Duurtest**: minimaal 72 uur onbeheerd draaien; controleren op herstarts, vrije heap (wordt gelogd in events bij opstart en dagelijks) en volledige logbestanden.

---

## 12. Acceptatiecriteria

- Alle FR's in §2.1 aantoonbaar werkend (afvinklijst per ID).
- Pc-tests voor `control` en `alarms` slagen.
- 72-uurs duurtest zonder ongeplande herstart en zonder groei van heapgebruik.
- Stroom eraf/erop: binnen 10 s weer regelend, herstart-SMS ontvangen.
- SD-kaart eruit tijdens bedrijf: regeling blijft werken, SMS ontvangen; kaart terug: loggen hervat.
