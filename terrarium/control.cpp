#include "control.h"

bool isDay(uint16_t minuteOfDay, uint16_t dayStartMin, uint16_t nightStartMin) {
  if (dayStartMin <= nightStartMin) return minuteOfDay >= dayStartMin && minuteOfDay < nightStartMin;
  return minuteOfDay >= dayStartMin || minuteOfDay < nightStartMin;
}

bool inVentWindow(uint16_t minuteOfDay, uint16_t intervalMin, uint16_t durationMin) {
  if (intervalMin == 0) return false;
  return (minuteOfDay % intervalMin) < durationMin;
}

void controlInit(ControlMemory& m) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) m.autoOn[i] = false;
  m.humOnSinceMs = 0; m.humPauseSinceMs = 0; m.humPausing = false; m.sourceTripped = false;
}

ControlOutput decide(const ControlInput& in, const Settings& s, ControlMemory& m) {
  ControlOutput out;
  const bool day = in.timeValid && isDay(in.minuteOfDay, s.dayStartMin, s.nightStartMin);
  out.isDay = day;

  // Bronbeveiliging met hysterese (1 C).
  if (in.dsOk) {
    if (in.heaterTemp > s.sourceMaxC) m.sourceTripped = true;
    else if (in.heaterTemp < s.sourceMaxC - 1.0f) m.sourceTripped = false;
  }

  // (1) Automatische stand.
  if (in.dhtOk && in.dsOk) {
    const float target = day ? s.heatDayC : s.heatNightC;
    if (in.airTemp < target - s.heatHystC) m.autoOn[R_HEAT] = true;
    else if (in.airTemp > target + s.heatHystC) m.autoOn[R_HEAT] = false;
  } else {
    m.autoOn[R_HEAT] = false;
  }

  if (in.dhtOk) {
    if (m.humPausing) {
      if ((uint32_t)(in.nowMs - m.humPauseSinceMs) >= (uint32_t)s.humPauseMin * 60000u) m.humPausing = false;
    }
    if (m.humPausing) {
      m.autoOn[R_HUMID] = false;
    } else {
      bool wasOn = m.autoOn[R_HUMID];
      if (in.rh < s.humOnBelow) m.autoOn[R_HUMID] = true;
      else if (in.rh > s.humOffAbove) m.autoOn[R_HUMID] = false;
      if (m.autoOn[R_HUMID] && !wasOn) m.humOnSinceMs = in.nowMs;
      if (m.autoOn[R_HUMID] &&
          (uint32_t)(in.nowMs - m.humOnSinceMs) >= (uint32_t)s.humMaxOnMin * 60000u) {
        m.autoOn[R_HUMID] = false;
        m.humPausing = true;
        m.humPauseSinceMs = in.nowMs;
      }
    }
  } else {
    m.autoOn[R_HUMID] = false;
    m.humPausing = false;
  }

  const bool window = in.timeValid && inVentWindow(in.minuteOfDay, s.ventIntervalMin, s.ventDurationMin);
  if (window) m.autoOn[R_VENT] = true;
  else if (!in.dhtOk) m.autoOn[R_VENT] = false;
  else if (in.rh > s.ventOnAbove) m.autoOn[R_VENT] = true;
  else if (in.rh < s.ventOffBelow) m.autoOn[R_VENT] = false;

  m.autoOn[R_LIGHT] = day;

  // (2) Handbediening.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (in.mode[i] == RelayMode::ManualOn) out.on[i] = true;
    else if (in.mode[i] == RelayMode::ManualOff) out.on[i] = false;
    else out.on[i] = m.autoOn[i];
  }

  // (3) Veiligheid.
  if (!in.dhtOk || !in.dsOk || m.sourceTripped) out.on[R_HEAT] = false;
  if (!in.dhtOk) out.on[R_HUMID] = false;
  return out;
}

void overridesInit(Overrides& o) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) { o.mode[i] = RelayMode::Auto; o.sinceMs[i] = 0; }
}

void setOverride(Overrides& o, RelayId r, RelayMode mode, uint32_t nowMs) {
  o.mode[r] = mode; o.sinceMs[r] = nowMs;
}

void expireOverrides(Overrides& o, uint32_t nowMs, uint16_t timeoutMin) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    if (o.mode[i] != RelayMode::Auto && (uint32_t)(nowMs - o.sinceMs[i]) >= (uint32_t)timeoutMin * 60000u)
      o.mode[i] = RelayMode::Auto;
}

uint32_t overrideRemainingS(const Overrides& o, RelayId r, uint32_t nowMs, uint16_t timeoutMin) {
  if (o.mode[r] == RelayMode::Auto) return 0;
  uint32_t total = (uint32_t)timeoutMin * 60000u;
  uint32_t el = (uint32_t)(nowMs - o.sinceMs[r]);
  return el >= total ? 0 : (total - el + 999u) / 1000u;
}
