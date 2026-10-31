// ST7565R LCD 128x64 via U8g2 (hardware-SPI, gedeeld met de SD-kaart).
#include <Arduino.h>
#include <U8g2lib.h>
#include "display.h"
#include "display_format.h"
#include "pins.h"

#define LCD_CONTRAST 0  // 0..255; verhogen of verlagen als het beeld te licht/donker is

// Ander schermtype bij leeg of verschoven beeld: probeer een van deze constructors.
//   U8G2_ST7565_ERC12864_F_4W_HW_SPI
//   U8G2_ST7565_NHD_C12864_F_4W_HW_SPI
static U8G2_ST7565_ERC12864_ALT_F_4W_HW_SPI u8g2(U8G2_R0, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);

static uint32_t lastDrawMs = 0;
static bool drawnOnce = false;
static uint32_t alarmPhaseStartMs = 0;
static bool showAlarm = false;

void displayBegin() {
  u8g2.begin();
  u8g2.setContrast(LCD_CONTRAST);
  u8g2.setFont(u8g2_font_6x10_tf);
}

void displayUpdate(const SystemState& s, uint32_t nowMs) {
  if (drawnOnce && (uint32_t)(nowMs - lastDrawMs) < 1000) return;
  if (!drawnOnce) alarmPhaseStartMs = nowMs;
  drawnOnce = true;
  lastDrawMs = nowMs;

  bool anyAlarm = false;
  for (uint8_t i = 0; i < ALARM_COUNT; i++) if (s.alarmActive[i]) anyAlarm = true;
  if (!anyAlarm) {
    showAlarm = false;
    alarmPhaseStartMs = nowMs;
  } else if ((uint32_t)(nowMs - alarmPhaseStartMs) >= 2000) {
    showAlarm = !showAlarm;
    alarmPhaseStartMs = nowMs;
  }

  char lines[DISPLAY_LINES][DISPLAY_COLS + 1];
  formatDisplayLines(s, showAlarm, lines);
  u8g2.clearBuffer();
  for (uint8_t i = 0; i < DISPLAY_LINES; i++) u8g2.drawStr(0, 10 * (i + 1), lines[i]);
  u8g2.sendBuffer();
}
