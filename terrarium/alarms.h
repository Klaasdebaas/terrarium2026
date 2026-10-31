#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"
#include "config.h"

struct AlarmInput {
  uint32_t nowMs = 0; float airTemp = 25, rh = 80, heaterTemp = 25;
  bool dhtUsable = true, dsUsable = true, dhtFaulty = false, dsFaulty = false,
       sdOk = true, rtcOk = true, wifiOk = true;
};
enum class Notify : uint8_t { None, Alarm, Restored };
struct AlarmState {
  bool active[ALARM_COUNT]; bool condPrev[ALARM_COUNT]; uint32_t condSinceMs[ALARM_COUNT];
  Notify pending[ALARM_COUNT]; Notify lastSent[ALARM_COUNT];
  bool everSent[ALARM_COUNT]; uint32_t lastSmsMs[ALARM_COUNT];
};
struct AlarmEvent { AlarmId id; bool transition; bool active; bool sendSms; };

void alarmsInit(AlarmState& st);
// Rapporteert hooguit een event per alarm; geeft het aantal events in out terug.
uint8_t evaluateAlarms(const AlarmInput& in, const Settings& s, AlarmState& st, AlarmEvent* out, uint8_t maxOut);
const char* alarmKey(AlarmId id);
size_t formatAlarmSms(char* buf, size_t len, AlarmId id, bool active, const AlarmInput& in, const LocalTime& t);
size_t formatInfoSms(char* buf, size_t len, const char* text, const LocalTime& t);
