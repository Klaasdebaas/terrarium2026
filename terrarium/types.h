#ifndef TERRARIUM_TYPES_H
#define TERRARIUM_TYPES_H
#include <stdint.h>

enum RelayId : uint8_t { R_HEAT = 0, R_HUMID = 1, R_VENT = 2, R_LIGHT = 3, RELAY_COUNT = 4 };
enum class RelayMode : uint8_t { Auto, ManualOn, ManualOff };
struct LocalTime { int16_t year; uint8_t month, day, hour, minute, second; };
enum AlarmId : uint8_t { A_TEMP_LOW, A_TEMP_HIGH, A_SOURCE_HOT, A_RH_LOW, A_RH_HIGH,
                         A_DHT_FAIL, A_DS_FAIL, A_SD_FAIL, A_RTC_FAIL, A_WIFI_LOST, ALARM_COUNT };

#endif  // TERRARIUM_TYPES_H
