# terrarium2026

Terrarium-besturing met een ESP32-C6, gebouwd en geprogrammeerd met de Arduino IDE:
temperatuur- en vochtigheidsregeling (verwarming, bevochtiging, ventilatie), verlichtingsschema,
logboek op SD-kaart, storingsmeldingen per SMS (SIM7600) en een lokale webinterface.

## Hardware (kort)
ESP32-C6-DevKitC-1, DS3231 + AT24C32 (klok en config-backup), DHT22 (lucht), DS18B20 (warmtebron),
4-kanaals relaismodule, SIM7600 (4G) met eigen voeding, ST7565R-scherm 128x64 (SPI, 3,3 V),
micro-SD-breakout zonder regelaar. Pinnen: `terrarium/pins.h`; schema's en voedingseisen: spec §3.

## Arduino IDE instellen
1. Voeg bij *Bestand > Voorkeuren > Aanvullende borden-URL's* toe:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Installeer via de Boardmanager **esp32 by Espressif Systems**, versie 3.x (getest met 3.3.0).
3. Kies bij *Hulpmiddelen*:
   - Board: **ESP32C6 Dev Module**
   - USB CDC On Boot: **Enabled** (serial via de native USB-poort)
   - Partition Scheme: **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**. De sketch is ongeveer
     1,2 MB en past **niet** in het standaardschema.
4. Open de map `terrarium/` (bestand `terrarium.ino`) in de IDE.

## Libraries (Library Manager)
- U8g2
- DHT sensor library (Adafruit) + Adafruit Unified Sensor
- OneWire
- DallasTemperature
- RTClib (Adafruit) + Adafruit BusIO
- ArduinoJson (versie 7)

## SD-kaart voorbereiden
1. Formatteer de kaart als **FAT32**.
2. Kopieer `sd-template/config.json` naar de root van de kaart als `/config.json`.
3. Vul in: `wifi.ssid` en `wifi.password`, `web.password`, `sms.phone` (formaat `+316XXXXXXXX`) en
   `sms.sim_pin`. Gebruik nooit echte gegevens in de repository; `config.json` hoort alleen op de kaart.
   Laat je de placeholder `+316XXXXXXXX` staan, dan wordt die geweigerd (event `CONFIG ... geweigerd`)
   en worden er geen SMS'en verstuurd.
4. De map `/log` wordt automatisch aangemaakt: meetbestanden per dag (`JJJJ-MM-DD.csv`) en
   gebeurtenissen per maand (`events-JJJJ-MM.csv`).

## Eerste start
- Zonder `wifi.ssid` start de ESP32 het access point **Terrarium-Setup**; ga naar `http://192.168.4.1`.
- Anders is de webpagina bereikbaar op `http://terrarium.local` (of het IP-adres op het scherm).
- Kijken kan zonder inloggen. Wijzigen (instellingen, relais, tijd, test-SMS) vraagt inloggen met de
  gebruiker uit de config (standaard `admin`). Zonder ingesteld webwachtwoord worden wijzigingen geweigerd.
- Test eerst alles per onderdeel met [docs/bringup.md](docs/bringup.md) voordat je 230 V-apparaten aansluit.

## Pc-tests
`make -C test` (vereist `g++`; downloadt eenmalig de ArduinoJson-header). Optioneel compileert
`tools/compile-esp32.sh` de sketch met arduino-cli.

## Documentatie
- [Specificatie & ontwerp](docs/superpowers/specs/2026-10-31-terrarium-design.md)
- [Implementatieplan](docs/superpowers/plans/2026-10-31-terrarium-implementatie.md)
- [Bring-up- en acceptatiechecklist](docs/bringup.md)
