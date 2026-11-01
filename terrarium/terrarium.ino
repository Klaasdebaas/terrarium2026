#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <esp_task_wdt.h>
#include "pins.h"
#include "types.h"
#include "state.h"
#include "config.h"
#include "config_store.h"
#include "clock.h"
#include "sensors.h"
#include "relays.h"
#include "control.h"
#include "alarms.h"
#include "sms_modem.h"
#include "sms_port.h"
#include "logger.h"
#include "logformat.h"
#include "display.h"
#include "net_wifi.h"
#include "web.h"

// Tijdelijke testcode voor bring-up: haal commentaar weg om te gebruiken.
// Dan vervangt die setup()/loop() van de hoofdapplicatie.
//#define TEST_CLOCK
//#define TEST_IO

#if defined(TEST_CLOCK) || defined(TEST_IO)

void setup() {
  Serial.begin(115200);
#ifdef TEST_CLOCK
  Wire.begin(PIN_SDA, PIN_SCL);
  clockBegin("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
#endif
#ifdef TEST_IO
  relaysBegin(true);
  sensorsBegin();
#endif
}

void loop() {
#ifdef TEST_CLOCK
  static uint32_t last = 0;
  clockUpdate(millis(), false);
  if (millis() - last >= 1000) {
    last = millis();
    LocalTime t;
    if (clockNow(t))
      Serial.printf("%04d-%02d-%02d %02d:%02d:%02d rtcOk=%d\n", t.year, t.month, t.day,
                    t.hour, t.minute, t.second, clockRtcOk());
    else
      Serial.printf("geen geldige tijd rtcOk=%d\n", clockRtcOk());
  }
#endif
#ifdef TEST_IO
  static uint32_t lastP = 0, lastR = 0;
  static uint8_t cur = 0;
  uint32_t now = millis();
  sensorsUpdate(now);
  if (now - lastP >= 5000) {
    lastP = now;
    SensorSnapshot s = sensorsGet();
    Serial.printf("air=%.1f rh=%.1f heater=%.1f dht=%d/%d ds=%d/%d\n", s.airTemp, s.rh,
                  s.heaterTemp, s.dhtUsable, s.dhtFaulty, s.dsUsable, s.dsFaulty);
  }
  if (now - lastR >= 1000) {
    lastR = now;
    bool on[RELAY_COUNT] = {false, false, false, false}, ch[RELAY_COUNT];
    on[cur] = true;
    cur = (cur + 1) % RELAY_COUNT;
    relaysApply(on, ch);
  }
#endif
}

#else  // hoofdapplicatie

static const char* const RELAY_NAME[RELAY_COUNT] = {"verwarming", "bevochtiging", "ventilatie", "verlichting"};

static Config config;
static SystemState state;
static Overrides overrides;
static ControlMemory ctlMem;
static AlarmState alarmState;
static SerialModemIO modemIO(Serial1);
static SmsModem modem(modemIO);
static WebContext webCtx;

// Kopie van de instellingen die een herstart (of relais-herinit) vereisen.
struct Applied {
  bool relayActiveLow;
  char ssid[33], pass[65], host[33], ntp[64], tz[64], phone[20], pin[9];
};
static Applied applied;
static bool restartPending = false;
static uint32_t restartAtMs = 0;

static void captureApplied() {
  applied.relayActiveLow = config.settings.relayActiveLow;
  strlcpy(applied.ssid, config.cred.wifiSsid, sizeof(applied.ssid));
  strlcpy(applied.pass, config.cred.wifiPass, sizeof(applied.pass));
  strlcpy(applied.host, config.cred.hostname, sizeof(applied.host));
  strlcpy(applied.ntp, config.ntpServer, sizeof(applied.ntp));
  strlcpy(applied.tz, config.tz, sizeof(applied.tz));
  strlcpy(applied.phone, config.cred.smsPhone, sizeof(applied.phone));
  strlcpy(applied.pin, config.cred.simPin, sizeof(applied.pin));
}

static LocalTime currentTime() {
  LocalTime t = {0, 0, 0, 0, 0, 0};
  if (state.timeValid) t = state.time;
  return t;
}

// Event-tekst zonder ';' en regeleinden (CSV-veilig).
static void logEvent(const char* kind, const char* text) {
  char buf[128];
  size_t i = 0;
  for (; text[i] && i < sizeof(buf) - 1; i++) {
    char c = text[i];
    buf[i] = (c == ';') ? ',' : (c == '\r' || c == '\n') ? ' ' : c;
  }
  buf[i] = '\0';
  loggerEvent(currentTime(), kind, buf);
}

// Zonder telefoonnummer geen SMS in de wachtrij; eenmalig per opstart een event.
static void queueSms(const char* sms) {
  static bool warned = false;
  if (config.cred.smsPhone[0] == '\0') {
    if (!warned) { warned = true; logEvent("SMS", "geen telefoonnummer ingesteld"); }
    return;
  }
  modem.enqueue(sms);
}

static void enqueueInfo(const char* text) {
  char sms[SmsModem::TEXT_MAX];
  formatInfoSms(sms, sizeof(sms), text, currentTime());
  queueSms(sms);
}

static void onSmsTest() { enqueueInfo("Test-SMS"); }

static void onConfigChanged() {
  logEvent("CONFIG", "instellingen gewijzigd");
  if (config.settings.relayActiveLow != applied.relayActiveLow) relaysBegin(config.settings.relayActiveLow);
  if (strcmp(applied.ssid, config.cred.wifiSsid) || strcmp(applied.pass, config.cred.wifiPass) ||
      strcmp(applied.host, config.cred.hostname) || strcmp(applied.ntp, config.ntpServer) ||
      strcmp(applied.tz, config.tz) ||
      strcmp(applied.phone, config.cred.smsPhone) || strcmp(applied.pin, config.cred.simPin)) {
    restartPending = true;
    restartAtMs = millis();
  }
  captureApplied();
}

static void logConfigRejected(uint8_t n, const char* err) {
  char txt[128];
  snprintf(txt, sizeof(txt), "%u waarde(n) geweigerd: %s", (unsigned)n, err);
  logEvent("CONFIG", txt);
}

static void onRelayCommand(RelayId r, RelayMode m) {
  char txt[64];
  const char* what = m == RelayMode::ManualOn ? "AAN" : m == RelayMode::ManualOff ? "UIT" : "AUTO";
  snprintf(txt, sizeof(txt), "R%u %s %s (web)", (unsigned)r + 1, RELAY_NAME[r], what);
  logEvent("HAND", txt);
}

static void watchdogBegin() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = 30000;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}

void setup() {
  relaysBegin(true);
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  SD.begin(PIN_SD_CS);
  ConfigResult cfgRes;
  ConfigSource src = configLoad(config, &cfgRes);
  if (!config.settings.relayActiveLow) relaysBegin(false);
  captureApplied();
  clockBegin(config.tz, config.ntpServer);
  loggerBegin();
  sensorsBegin();
  displayBegin();
  smsPortBegin();
  modem.begin(config.cred.simPin, config.cred.smsPhone);
  networkBegin(config);
  webCtx.config = &config;
  webCtx.overrides = &overrides;
  webCtx.state = &state;
  webCtx.onSmsTest = onSmsTest;
  webCtx.onConfigChanged = onConfigChanged;
  webCtx.onConfigRejected = logConfigRejected;
  webCtx.onRelayCommand = onRelayCommand;
  webBegin(webCtx);
  watchdogBegin();
  controlInit(ctlMem);
  overridesInit(overrides);
  alarmsInit(alarmState);
  memset(&state, 0, sizeof(state));
  state.gsmSignal = -1;

  LocalTime t;
  state.timeValid = clockNow(t);
  if (state.timeValid) state.time = t;
  logEvent("START", "opstart");
  char heap[32];
  snprintf(heap, sizeof(heap), "vrij geheugen %u", (unsigned)ESP.getFreeHeap());
  logEvent("HEAP", heap);
  if (src == ConfigSource::Eeprom) logEvent("CONFIG", "bron eeprom");
  else if (src == ConfigSource::Defaults) logEvent("CONFIG", "bron standaard");
  if (cfgRes.rejected > 0) logConfigRejected(cfgRes.rejected, cfgRes.error);
  char last[32], msg[64];
  if (loggerLastTimestamp(last, sizeof(last)) && last[0]) snprintf(msg, sizeof(msg), "Herstart. Laatste log %s", last);
  else snprintf(msg, sizeof(msg), "Herstart");
  enqueueInfo(msg);
}

void loop() {
  static uint32_t lastLogKey = 0xFFFFFFFFu, lastMaintDay = 0, lastSdRetryMs = 0;
  uint32_t now = millis();
  const Settings& s = config.settings;
  char txt[96];

  // 1. Klok
  bool wifiConn = networkConnected();
  clockUpdate(now, wifiConn);
  LocalTime t;
  state.timeValid = clockNow(t);
  if (state.timeValid) state.time = t;
  state.rtcOk = clockRtcOk();
  if (clockTakeSyncEvent()) logEvent("NTP", "tijd gesynchroniseerd");

  // 2. Sensoren
  sensorsUpdate(now);
  SensorSnapshot snap = sensorsGet();
  state.airTemp = snap.airTemp; state.rh = snap.rh; state.heaterTemp = snap.heaterTemp;
  state.dhtOk = snap.dhtUsable; state.dsOk = snap.dsUsable;

  // 3. Besturing
  uint16_t minuteOfDay = state.timeValid ? (uint16_t)(state.time.hour * 60 + state.time.minute) : 0;
  RelayMode prevMode[RELAY_COUNT];
  for (uint8_t i = 0; i < RELAY_COUNT; i++) prevMode[i] = overrides.mode[i];
  expireOverrides(overrides, now, s.manualTimeoutMin);
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    if (prevMode[i] != RelayMode::Auto && overrides.mode[i] == RelayMode::Auto) {
      snprintf(txt, sizeof(txt), "R%u %s handbediening verlopen", i + 1, RELAY_NAME[i]);
      logEvent("HAND", txt);
    }
  ControlInput ci = {};
  ci.nowMs = now; ci.minuteOfDay = minuteOfDay; ci.timeValid = state.timeValid;
  ci.airTemp = snap.airTemp; ci.rh = snap.rh; ci.heaterTemp = snap.heaterTemp;
  ci.dhtOk = snap.dhtUsable; ci.dsOk = snap.dsUsable;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) ci.mode[i] = overrides.mode[i];
  ControlOutput co = decide(ci, s, ctlMem);
  bool changed[RELAY_COUNT];
  relaysApply(co.on, changed);
  state.isDay = co.isDay;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    state.relayOn[i] = co.on[i];
    state.relayMode[i] = overrides.mode[i];
    state.manualRemainingS[i] = overrideRemainingS(overrides, (RelayId)i, now, s.manualTimeoutMin);
    // Veiligheidsingrepen zijn zonder wijziging van control.h niet te onderscheiden van auto.
    if (changed[i]) {
      snprintf(txt, sizeof(txt), "R%u %s %s (%s)", i + 1, RELAY_NAME[i], co.on[i] ? "AAN" : "UIT",
               overrides.mode[i] == RelayMode::Auto ? "auto" : "hand");
      logEvent("RELAIS", txt);
    }
  }

  // 4. Alarmen
  AlarmInput ai;
  ai.nowMs = now; ai.airTemp = snap.airTemp; ai.rh = snap.rh; ai.heaterTemp = snap.heaterTemp;
  ai.dhtUsable = snap.dhtUsable; ai.dsUsable = snap.dsUsable;
  ai.dhtFaulty = snap.dhtFaulty; ai.dsFaulty = snap.dsFaulty;
  ai.sdOk = loggerSdOk(); ai.rtcOk = clockRtcOk();
  ai.wifiOk = networkApMode() || wifiConn;
  AlarmEvent aev[ALARM_COUNT];
  uint8_t nev = evaluateAlarms(ai, s, alarmState, aev, ALARM_COUNT);
  for (uint8_t k = 0; k < nev; k++) {
    if (aev[k].transition) {
      AlarmId aid = aev[k].id;
      if (aid == A_TEMP_LOW || aid == A_TEMP_HIGH || aid == A_SOURCE_HOT || aid == A_RH_LOW || aid == A_RH_HIGH)
        snprintf(txt, sizeof(txt), "%s %.1f", alarmKey(aid),
                 (double)(aid == A_RH_LOW || aid == A_RH_HIGH ? snap.rh :
                          aid == A_SOURCE_HOT ? snap.heaterTemp : snap.airTemp));
      else
        snprintf(txt, sizeof(txt), "%s", alarmKey(aid));
      logEvent(aev[k].active ? "ALARM" : "HERSTELD", txt);
    }
    if (aev[k].sendSms) {
      char sms[SmsModem::TEXT_MAX];
      formatAlarmSms(sms, sizeof(sms), aev[k].id, aev[k].active, ai, currentTime());
      queueSms(sms);
    }
  }
  for (uint8_t i = 0; i < ALARM_COUNT; i++) state.alarmActive[i] = alarmState.active[i];

  // 5. Modem
  modem.update(now);
  state.gsmOk = modem.ready();
  state.gsmSignal = modem.signal();
  SmsEvent sev;
  while (modem.popEvent(sev)) {
    const char* what = sev.type == SmsEventType::Sent ? "verzonden" :
                       sev.type == SmsEventType::Failed ? "mislukt" : "modem reset";
    snprintf(txt, sizeof(txt), "%s %s", what, sev.text);
    logEvent("SMS", txt);
  }

  // 6. Logging
  state.sdOk = loggerSdOk();
  if (state.timeValid) {
    uint32_t key = ((uint32_t)state.time.day * 24 + state.time.hour) * 60 + state.time.minute;
    if (s.logIntervalMin > 0 && state.time.minute % s.logIntervalMin == 0 && key != lastLogKey) {
      lastLogKey = key;
      state.uptimeS = now / 1000;
      loggerMeasurement(state);
    }
    if (state.time.hour == 3 && state.time.minute == 0 && lastMaintDay != state.time.day) {
      lastMaintDay = state.time.day;
      loggerDailyMaintenance(state.time, s.retentionDays);
      snprintf(txt, sizeof(txt), "vrij geheugen %u", (unsigned)ESP.getFreeHeap());
      logEvent("HEAP", txt);
    }
  }
  if (!loggerSdOk() && (uint32_t)(now - lastSdRetryMs) >= 60000) {
    lastSdRetryMs = now;
    SD.end();   // SD.begin() geeft true als er nog (oud) gemount is: eerst afkoppelen
    if (SD.begin(PIN_SD_CS)) loggerBegin();
  }

  // 7. Display
  state.uptimeS = now / 1000;
  state.wifiOk = wifiConn;
  state.apMode = networkApMode();
  state.wifiRssi = networkRssi();
  networkIp(state.ip, sizeof(state.ip));
  displayUpdate(state, now);

  // 8. Netwerk en web
  networkUpdate(now);
  bool conn;
  if (networkTakeEvent(conn)) logEvent("WIFI", conn ? "verbonden" : "verbinding verloren");
  webUpdate();

  if (restartPending && (uint32_t)(now - restartAtMs) >= 2000) ESP.restart();

  // 9. Watchdog
  esp_task_wdt_reset();
  vTaskDelay(1);   // idle-taak en WiFi-stack lucht geven
}

#endif
