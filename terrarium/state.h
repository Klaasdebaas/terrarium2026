#ifndef TERRARIUM_STATE_H
#define TERRARIUM_STATE_H
#include <stdint.h>
#include "types.h"

struct SystemState {
  LocalTime time; bool timeValid; bool isDay;
  float airTemp, rh, heaterTemp; bool dhtOk, dsOk;        // laatste geldige waarde; *Ok = bruikbaar
  bool relayOn[RELAY_COUNT]; RelayMode relayMode[RELAY_COUNT]; uint32_t manualRemainingS[RELAY_COUNT];
  bool alarmActive[ALARM_COUNT];
  bool wifiOk; bool apMode; int8_t wifiRssi; char ip[16];
  bool gsmOk; int8_t gsmSignal;                            // 0..31, -1 onbekend
  bool sdOk, rtcOk; uint32_t uptimeS;
};

#endif  // TERRARIUM_STATE_H
