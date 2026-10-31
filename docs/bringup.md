# Bring-up- en acceptatiechecklist

Doel: elk onderdeel apart aansluiten en testen, daarna het geheel, daarna de duurtest en acceptatie.
Vink af (`[x]`) wat gelukt is. Werk met **losgekoppelde 230 V**: test relais alleen met de LED's op de
relaismodule. Zie ook de [specificatie](superpowers/specs/2026-10-31-terrarium-design.md) (§3 hardware, §11 tests, §12 acceptatie).

## 0. Voorbereiding
- [ ] Arduino IDE ingesteld volgens [README](../README.md): *ESP32C6 Dev Module*, *USB CDC On Boot: Enabled*, partition scheme *Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)*.
- [ ] Libraries geinstalleerd (U8g2, DHT + Unified Sensor, OneWire, DallasTemperature, RTClib + BusIO, ArduinoJson 7).
- [ ] Pc-tests slagen: `make -C test`.
- [ ] Sketch compileert en uploaden via de native USB-poort lukt; Serial Monitor op 115200 baud.
- [ ] Voeding: hoofdvoeding 5 V / 3 A of meer; SIM7600 op eigen step-down (2 A piek) met 1000 uF of meer dicht bij de module; gemeenschappelijke GND.

### Testmacro's
Bovenaan `terrarium/terrarium.ino` staan twee regels, standaard uitgecommentarieerd:

```cpp
//#define TEST_CLOCK
//#define TEST_IO
```
Haal `//` weg om er een te gebruiken; de testcode vervangt dan `setup()`/`loop()` van de hoofdapplicatie.
Zet ze na afloop weer terug als commentaar.

- `TEST_CLOCK`: start I2C en de klok; print elke seconde `JJJJ-MM-DD UU:MM:SS rtcOk=1` (of `geen geldige tijd rtcOk=0`).
- `TEST_IO`: zet de relais aan (aangenomen active-low), start de sensoren; print elke 5 s
  `air=.. rh=.. heater=.. dht=bruikbaar/gestoord ds=bruikbaar/gestoord` en laat elke seconde een volgend
  relais (R1 tot R4) aangaan, de rest uit.

## 1. RTC + EEPROM (DS3231 + AT24C32)
- [ ] Aansluiten: SDA = GPIO23, SCL = GPIO22, 3,3 V (of 5 V volgens module), GND. Knoopcel geplaatst.
- [ ] `TEST_CLOCK` aan, uploaden.
- [ ] Verwacht: elke seconde een regel met tijd en `rtcOk=1`. Tijd loopt door na stroom eraf/erop (knoopcel).
- [ ] Zonder WiFi/NTP kan de tijd nog 2000 of fout zijn: dat is acceptabel; stel de tijd later via de webpagina in of via NTP.
- [ ] Met WiFi (zet SSID in de code of test later in de hoofdapplicatie): tijd klopt met Europe/Amsterdam na NTP-sync.
- [ ] EEPROM: later in stap 9 testen (config-backup, zie 9.6).

## 2. DHT22
- [ ] Aansluiten: data = GPIO3, 3,3 V, GND; 10 kOhm pull-up naar 3,3 V (niet nodig als de module die al heeft).
- [ ] `TEST_IO` aan, uploaden.
- [ ] Verwacht: `air` en `rh` zijn plausibel (kamertemperatuur, 30-70 %), `dht=1/0`.
- [ ] Adem op de sensor: `rh` stijgt binnen enkele metingen. Haal data-draad los: na 3 mislukte metingen (circa 15 s) `dht=0/1` (gestoord).

## 3. DS18B20
- [ ] Aansluiten: data = GPIO2, 3,3 V, GND; **4,7 kOhm pull-up** naar 3,3 V.
- [ ] `TEST_IO` aan.
- [ ] Verwacht: `heater` plausibel, `ds=1/0`. Vingers om de sensor: waarde stijgt.
- [ ] Draad los: na 3 mislukte metingen `ds=0/1`. Waarden -127 of 85 worden genegeerd.
- [ ] Meet de lusduur: een DS18B20-uitlezing kan de lus eenmalig per 5 s circa 10-20 ms blokkeren (zie beperkingen); controleer dat dit acceptabel blijft.

## 4. Relais (active-low check)
- [ ] Aansluiten: R1 = GPIO0, R2 = GPIO1, R3 = GPIO10, R4 = GPIO11; voeding en GND volgens de module. Geen 230 V!
- [ ] `TEST_IO` aan. De test gaat uit van active-low.
- [ ] Verwacht: elke seconde brandt precies een relais-LED, in volgorde R1, R2, R3, R4, daarna opnieuw.
- [ ] Zijn juist alle relais aan behalve een? Dan is de module active-high: zet later `"relay_active_low": false` in `config.json`.
- [ ] Bij opstart/reset zijn alle relais direct uit (FR-N4), ook voordat de config geladen is.

## 5. Scherm (ST7565R 128x64)
- [ ] Aansluiten: CS = GPIO6, A0/DC = GPIO5, RST = GPIO4, SCK = GPIO21, MOSI = GPIO19, 3,3 V, GND, backlight volgens module.
- [ ] Zet `TEST_*` uit, upload de hoofdapplicatie.
- [ ] Verwacht: 6 regels (datum/tijd en dag/nacht, lucht + RV, bron, relais, WiFi/GSM), elke seconde ververst.
- [ ] Leeg, verschoven of spiegelbeeld? Pas in `terrarium/display.cpp` de constructor aan; probeer in deze volgorde:
  - `U8G2_ST7565_ERC12864_ALT_F_4W_HW_SPI` (standaard)
  - `U8G2_ST7565_ERC12864_F_4W_HW_SPI`
  - `U8G2_ST7565_NHD_C12864_F_4W_HW_SPI`
- [ ] Te licht of te donker? Pas `LCD_CONTRAST` (0..255) bovenaan `display.cpp` aan.

## 6. SD-kaart
- [ ] Aansluiten: CS = GPIO18, SCK = GPIO21, MOSI = GPIO19, MISO = GPIO20 (gedeeld met het scherm), 3,3 V. Module zonder regelaar/niveau-omzetter.
- [ ] Kaart FAT32 met `/config.json` (zie README).
- [ ] Upload de hoofdapplicatie. Verwacht: geen SD-alarm; na opstart bestaat `/log/events.csv` met een `START`-regel.
- [ ] Na de eerstvolgende 5-minutengrens (xx:00, xx:05, ...) komt een regel in `/log/JJJJ-MM-DD.csv`.
- [ ] Kaart eruit: regeling blijft draaien, SD-alarm volgt. Kaart terug: loggen hervat binnen circa 60 s.

## 7. SIM7600 (SMS)
- [ ] Voeding: eigen step-down van 2 A piek of meer, 1000 uF of meer dicht bij de module, gemeenschappelijke GND. Antenne aangesloten.
- [ ] UART (3,3 V-niveau): SIM7600 TXD naar GPIO7 (ESP RX), GPIO15 (ESP TX) naar SIM7600 RXD, 115200 baud.
- [ ] SIM-kaart zonder of met pincode; zet `sim_pin` in `config.json`. Een **verkeerde pincode** wordt niet herhaald: de modem blijft geblokkeerd tot herstart (bescherming tegen PUK-vergrendeling). Controleer de pincode dus eerst in een telefoon.
- [ ] Telefoonnummer in `sms.phone` als `+316XXXXXXXX`.
- [ ] Verwacht: op het scherm `GSM:` met een signaalwaarde (niet `--`) binnen ongeveer een minuut.
- [ ] Test-SMS: log in op `http://terrarium.local/settings` en gebruik de knop voor een test-SMS (`POST /api/sms/test`). Verwacht: SMS "Test-SMS" op je telefoon en een `SMS verzonden`-regel in `events.csv`.
- [ ] Bij opstart komt ook een herstart-SMS aan.
- [ ] Voedingsproblemen (modem reset tijdens verzenden)? Controleer piekstroom en elco.

## 8. WiFi-setupmodus
- [ ] Verwijder `wifi.ssid` uit `config.json` (of gebruik een kaart zonder config).
- [ ] Verwacht: access point `Terrarium-Setup`; verbind en open `http://192.168.4.1`: configuratiepagina.
- [ ] Vul SSID/wachtwoord in en bewaar: het apparaat herstart na 2 s en verbindt met je netwerk.

## 9. Webinterface
- [ ] `http://terrarium.local` en `http://<ip>` tonen het dashboard (zonder inloggen), ververst elke 5 s.
- [ ] `/api/status` geeft JSON.
- [ ] `/logs` toont de lijst; download van een CSV werkt. (Het downloaden blokkeert de lus kort.)
- [ ] Wijzigen zonder inloggen wordt geweigerd; met gebruiker/wachtwoord uit de config lukt het.
- [ ] Zonder ingesteld webwachtwoord worden alle wijzigingen geweigerd.
- [ ] Relais op AAN/UIT/AUTO zetten werkt; handstand vervalt na 30 min.
- [ ] Tijd zetten via de webpagina werkt.
- [ ] 9.6 Config-backup: wijzig een instelling, haal de SD-kaart eruit en herstart: de instelling blijft behouden (uit EEPROM; event `CONFIG bron eeprom`).
- [ ] Wijzigen van wifi, ntp, tijdzone, hostnaam, sms-telefoon of pincode herstart het apparaat na 2 s (verwacht gedrag).

## 10. Integratietests (bank, spec §11 stap 3)
Relaismodule nog zonder 230 V; kijk naar de LED's, het scherm, het log en de SMS.
- [ ] Verwarmen van de DHT22 (hand, haardroger op afstand) boven streef + 0,5: R1 uit; afkoelen (ijsblokje in zakje) onder streef - 0,5: R1 aan.
- [ ] DS18B20 boven 30 C houden: R1 direct uit, ook bij handstand AAN; alarm + SMS; weer aan pas onder 29 C.
- [ ] DHT22 loskoppelen: R1 en R2 uit, sensorstoring-alarm + SMS, scherm toont `--.-`.
- [ ] RV verlagen/verhogen (adem, droge lucht): R2 aan onder 70 %, uit boven 75 %; max. 10 min aan, dan 5 min pauze; R3 aan boven 95 %, uit onder 90 %.
- [ ] Ventilatievenster: R3 gaat op 00:00, 02:00, ... vijf minuten aan.
- [ ] Dag/nacht: R4 volgt het schema; streeftemperatuur wisselt op 08:00/20:00.
- [ ] Alarmen en hersteld-SMS komen aan, maximaal 1 per alarmsoort per 60 min.
- [ ] Logregels (meting en events) kloppen met wat er gebeurde.
- [ ] Wifi router uit: regeling werkt door, na 30 min WiFi-alarm; router weer aan: verbinding hersteld.

## 11. Duurtest (spec §11 stap 4)
- [ ] Minimaal **72 uur** onbeheerd draaien, met de echte belasting waar mogelijk.
- [ ] Geen ongeplande herstarts (geen herstart-SMS, geen `START` in `events.csv` buiten je eigen ingrepen).
- [ ] Vrij geheugen: de `HEAP`-regels in `events.csv` (dagelijks om 03:00) vertonen geen dalende trend.
- [ ] Logbestanden volledig: 288 regels per dag bij 5-minutenlog.
- [ ] Lusduur normaal < 50 ms (zie beperkingen).

## 12. Acceptatiecriteria (spec §12)
- [ ] Alle FR's in de tabel hieronder aantoonbaar werkend.
- [ ] Pc-tests voor `control` en `alarms` slagen (`make -C test`).
- [ ] 72-uurs duurtest zonder ongeplande herstart en zonder groei van heapgebruik.
- [ ] Stroom eraf/erop: binnen 10 s weer regelend, herstart-SMS ontvangen.
- [ ] SD-kaart eruit tijdens bedrijf: regeling blijft werken, SMS ontvangen; kaart terug: loggen hervat.

## Bekende beperkingen (let hierop bij bring-up)
- De DS18B20-uitlezing kan per 5 s-cyclus circa 10-20 ms blokkeren: meten en beoordelen.
- Het downloaden van een logbestand blokkeert de hoofdlus kort.
- Verkeerde SIM-pincode: de modem blijft geblokkeerd tot herstart; er zijn bewust geen herhaalpogingen (voorkomt PUK-vergrendeling).
- Wijzigen van wifi, NTP-server, tijdzone, hostnaam, sms-telefoonnummer of pincode via de webpagina herstart het apparaat na 2 s.

## FR-afvinklijst

| ID | Omschrijving (kort) | Hoe testen | Getest op / door |
|---|---|---|---|
| FR-M1 | DHT22 meet temp en RV elke 5 s | Stap 2: Serial-uitvoer `TEST_IO` | |
| FR-M2 | DS18B20 meet warmtebron elke 5 s | Stap 3: Serial-uitvoer `TEST_IO` | |
| FR-M3 | Onmogelijke meting is ongeldig, genegeerd | Sensor loskoppelen; pc-test sensor_logic | |
| FR-M4 | 3 ongeldige metingen: gestoord; 1 geldige heft op | Draad los en weer aan (stap 2/3) | |
| FR-D1 | Dag 08:00-20:00 (instelbaar) | Scherm toont DAG/NACHT op de grens | |
| FR-D2 | R4 aan bij dag, uit bij nacht | Stap 10: R4-LED rond 08:00/20:00 | |
| FR-V1 | Streef 29 C dag, 22 C nacht (instelbaar) | Stap 10: schakelpunt overdag en 's nachts | |
| FR-V2 | R1 aan < streef-0,5, uit > streef+0,5 | Stap 10: verwarmen/afkoelen DHT22 | |
| FR-V3 | R1 uit bij DS18B20 > 30 C, weer aan < 29 C | Stap 10: DS18B20 verwarmen | |
| FR-V4 | R1 uit bij gestoorde DHT22 of DS18B20 | Stap 10: sensor loskoppelen | |
| FR-B1 | R2 aan < 70 % RV, uit > 75 % | Stap 10: RV variëren | |
| FR-B2 | R2 max 10 min aan, dan 5 min pauze | Stap 10: lange droge periode | |
| FR-B3 | R2 uit bij gestoorde DHT22 | Stap 10: DHT22 loskoppelen | |
| FR-VE1 | R3 aan > 95 % RV, uit < 90 % | Stap 10: RV verhogen | |
| FR-VE2 | R3 elke 2 u gedurende 5 min vanaf 00:00 | Stap 10: wachten op even uur | |
| FR-VE3 | R1 en R3 mogen tegelijk aan | Stap 10: beide condities tegelijk | |
| FR-H1 | Handstand AAN/UIT/AUTO per relais via web | Stap 9: knoppen op dashboard | |
| FR-H2 | Handstand vervalt na 30 min (instelbaar) | Stap 9: 30 min wachten of timeout verlagen | |
| FR-H3 | Veiligheidsregels gaan voor handstand | Stap 10: R1 AAN met DS18B20 > 30 C | |
| FR-A1 | Alarm temp te laag: < 20 C gedurende 5 min | Stap 10: DHT22 koelen | |
| FR-A2 | Alarm temp te hoog: > 30 C gedurende 5 min | Stap 10: DHT22 verwarmen | |
| FR-A3 | Alarm oververhitting bron: DS18B20 > 30 C direct | Stap 10: DS18B20 verwarmen | |
| FR-A4 | Alarm RV te laag: < 70 % gedurende 30 min | Pc-test alarms; of drempel tijdelijk verlagen | |
| FR-A5 | Alarm RV te hoog: > 95 % gedurende 30 min | Pc-test alarms; of drempel tijdelijk verlagen | |
| FR-A6 | Alarm sensorstoring per sensor | Stap 2/3: sensor loskoppelen | |
| FR-A7 | Alarm SD-kaart | Stap 6: kaart eruit | |
| FR-A8 | Alarm RTC onbereikbaar of tijd vóór 2024 | RTC loskoppelen (spanningsloos) | |
| FR-A9 | Alarm WiFi weg > 30 min | Stap 10: router uit | |
| FR-A10 | Herstart-melding met laatste logtijdstip | Stroom eraf/erop; SMS controleren | |
| FR-A11 | Hersteld-SMS bij herstel alarm | Stap 10: storing opheffen | |
| FR-A12 | Max 1 SMS per alarmsoort per 60 min | Stap 10: alarm herhaald opwekken | |
| FR-A13 | SMS naar één nummer, +31-formaat | Stap 7: test-SMS | |
| FR-A14 | Alarmgrenzen en vertragingen instelbaar | Stap 9: instellingenpagina | |
| FR-L1 | Meetregel elke 5 min in `/log/JJJJ-MM-DD.csv` | Stap 6/11: bestand controleren | |
| FR-L2 | Gebeurtenissen in `/log/events.csv` | Stap 10: events vergelijken met acties | |
| FR-L3 | Meetbestanden > 365 d verwijderd om 03:00; events jaarlijks geroteerd | Retentie tijdelijk op 1 dag; jaarwisseling | |
| FR-L4 | Logbestanden downloadbaar via web | Stap 9: `/logs` | |
| FR-S1 | Eén scherm met alle statusinfo | Stap 5 | |
| FR-S2 | Onderste regel knippert met alarmtekst bij alarm | Stap 10: alarm opwekken, scherm bekijken | |
| FR-S3 | Scherm ververst elke seconde | Stap 5: klok op scherm | |
| FR-W1 | Webpagina op `terrarium.local` en IP | Stap 9 | |
| FR-W2 | Kijken zonder inloggen | Stap 9 | |
| FR-W3 | Wijzigen vereist inloggen | Stap 9 | |
| FR-W4 | JSON-API `/api/status` | Stap 9 | |
| FR-N1 | Regeling werkt door bij WiFi-verlies; herverbinden elke 60 s | Stap 10: router uit/aan | |
| FR-N2 | Zonder WiFi-gegevens AP `Terrarium-Setup` op 192.168.4.1 | Stap 8 | |
| FR-N3 | DS3231 is klok; NTP dagelijks en bij opstart; Europe/Amsterdam | Stap 1; zomer/wintertijd controleren | |
| FR-N4 | Relais bij opstart direct uit; regeling binnen 10 s | Stap 4; stroom eraf/erop | |
| FR-N5 | Zonder SD: config uit EEPROM, anders standaard | Stap 9.6: kaart eruit + herstart | |
| FR-N6 | Watchdog herstart bij lus > 30 s | Testlus tijdelijk laten hangen (`while(1);`) | |
