#include "pins.h"
#include "types.h"
#include "state.h"

// Tijdelijke testcode: haal commentaar weg om te gebruiken.
//#define TEST_CLOCK
//#define TEST_IO

#if defined(TEST_CLOCK)
#include <Wire.h>
#include "clock.h"
#endif
#if defined(TEST_IO)
#include "sensors.h"
#include "relays.h"
#endif

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
