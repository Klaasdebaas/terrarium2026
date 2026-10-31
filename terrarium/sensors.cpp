#include "sensors.h"
#include <Arduino.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include "pins.h"
#include "sensor_logic.h"

static const uint32_t SENSOR_INTERVAL_MS = 5000;

static DHT dht(PIN_DHT, DHT22);
static OneWire oneWire(PIN_DS18B20);
static DallasTemperature ds(&oneWire);
static FaultCounter dhtFault, dsFault;
static float airTemp = NAN, rh = NAN, heaterTemp = NAN;
static bool started = false;       // eerste cyclus gehad
static bool dsRequested = false;   // er loopt een conversie
static uint32_t lastMs = 0;

void sensorsBegin() {
  dht.begin();
  ds.begin();
  ds.setWaitForConversion(false);
  ds.setResolution(12);
}

void sensorsUpdate(uint32_t nowMs) {
  if (started && (uint32_t)(nowMs - lastMs) < SENSOR_INTERVAL_MS) return;
  started = true;
  lastMs = nowMs;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  bool ok = validDht(t, h);
  dhtFault.record(ok);
  if (ok) { airTemp = t; rh = h; }

  if (dsRequested) {
    float d = ds.getTempCByIndex(0);
    bool dok = validDs(d);
    dsFault.record(dok);
    if (dok) heaterTemp = d;
  }
  ds.requestTemperatures();
  dsRequested = true;
}

SensorSnapshot sensorsGet() {
  SensorSnapshot s;
  s.airTemp = airTemp;
  s.rh = rh;
  s.heaterTemp = heaterTemp;
  s.dhtUsable = dhtFault.usable();
  s.dsUsable = dsFault.usable();
  s.dhtFaulty = dhtFault.faulty();
  s.dsFaulty = dsFault.faulty();
  return s;
}
