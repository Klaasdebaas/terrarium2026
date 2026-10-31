#include "clock.h"
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"

static RTC_DS3231 rtc;
static bool rtcReachable = false;
static bool rtcValid = false;
static bool ntpSyncedSinceBoot = false;
static bool syncEvent = false;
static bool ntpPending = false;      // configTzTime gedaan, wacht op sync
static bool ntpStarted = false;
static uint32_t lastNtpStartMs = 0;
static uint32_t lastRtcReadMs = 0;
static char tzStr[64] = "UTC0";
static char ntpStr[64] = "pool.ntp.org";

static const uint32_t NTP_INTERVAL_MS = 24UL * 3600UL * 1000UL;
static const uint32_t RTC_REREAD_MS = 10UL * 60UL * 1000UL;

static void setSystemTimeUtc(time_t t) {
  struct timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
}

static bool readRtcIntoSystem() {
  if (!rtcReachable) return false;
  DateTime d = rtc.now();
  if (d.year() < 2024) { rtcValid = false; return false; }
  rtcValid = true;
  setSystemTimeUtc((time_t)d.unixtime());
  return true;
}

bool clockBegin(const char* tz, const char* ntpServer) {
  strncpy(tzStr, tz, sizeof(tzStr) - 1);
  tzStr[sizeof(tzStr) - 1] = '\0';
  strncpy(ntpStr, ntpServer, sizeof(ntpStr) - 1);
  ntpStr[sizeof(ntpStr) - 1] = '\0';
  setenv("TZ", tzStr, 1);
  tzset();
  rtcReachable = rtc.begin(&Wire);
  rtcValid = false;
  if (rtcReachable && !rtc.lostPower()) readRtcIntoSystem();
  return rtcReachable;
}

void clockUpdate(uint32_t nowMs, bool wifiOk) {
  if (wifiOk && (!ntpStarted || (uint32_t)(nowMs - lastNtpStartMs) >= NTP_INTERVAL_MS)) {
    ntpStarted = true;
    ntpPending = true;
    lastNtpStartMs = nowMs;
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(tzStr, ntpStr);
  }
  if (ntpPending && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    ntpPending = false;
    ntpSyncedSinceBoot = true;
    syncEvent = true;
    if (rtcReachable) {
      rtc.adjust(DateTime((uint32_t)time(nullptr)));
      rtcValid = true;
    }
  }
  if (!ntpSyncedSinceBoot && (uint32_t)(nowMs - lastRtcReadMs) >= RTC_REREAD_MS) {
    lastRtcReadMs = nowMs;
    readRtcIntoSystem();
  }
}

bool clockNow(LocalTime& out) {
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  if (tmv.tm_year + 1900 < 2024) return false;
  out.year = tmv.tm_year + 1900;
  out.month = tmv.tm_mon + 1;
  out.day = tmv.tm_mday;
  out.hour = tmv.tm_hour;
  out.minute = tmv.tm_min;
  out.second = tmv.tm_sec;
  return true;
}

bool clockRtcOk() { return rtcReachable && rtcValid; }

bool clockSetLocal(const LocalTime& t) {
  struct tm tmv = {};
  tmv.tm_year = t.year - 1900;
  tmv.tm_mon = t.month - 1;
  tmv.tm_mday = t.day;
  tmv.tm_hour = t.hour;
  tmv.tm_min = t.minute;
  tmv.tm_sec = t.second;
  tmv.tm_isdst = -1;
  time_t utc = mktime(&tmv);
  if (utc == (time_t)-1 || t.year < 2024) return false;
  setSystemTimeUtc(utc);
  if (rtcReachable) {
    rtc.adjust(DateTime((uint32_t)utc));
    rtcValid = true;
  }
  return true;
}

bool clockTakeSyncEvent() {
  bool e = syncEvent;
  syncEvent = false;
  return e;
}
