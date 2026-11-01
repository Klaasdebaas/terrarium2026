#ifndef TERRARIUM_PINS_H
#define TERRARIUM_PINS_H
#include <stdint.h>
#include "types.h"

constexpr uint8_t PIN_SDA = 23, PIN_SCL = 22, PIN_SCK = 21, PIN_MOSI = 19, PIN_MISO = 20,
                  PIN_SD_CS = 18, PIN_LCD_CS = 6, PIN_LCD_DC = 5, PIN_LCD_RST = 4,
                  PIN_DHT = 3, PIN_DS18B20 = 2, PIN_MODEM_RX = 7, PIN_MODEM_TX = 15;
constexpr uint8_t PIN_RELAY[RELAY_COUNT] = {0, 1, 10, 11};

#endif  // TERRARIUM_PINS_H
