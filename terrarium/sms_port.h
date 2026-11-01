#ifndef TERRARIUM_SMS_PORT_H
#define TERRARIUM_SMS_PORT_H
#include <Arduino.h>
#include "sms_modem.h"

class SerialModemIO : public ModemIO {
 public:
  explicit SerialModemIO(HardwareSerial& s) : s_(s) {}
  int available() override { return s_.available(); }
  int read() override { return s_.read(); }
  void write(const char* str) override { s_.print(str); }
  void writeByte(uint8_t b) override { s_.write(b); }
 private:
  HardwareSerial& s_;
};

void smsPortBegin();   // Serial1.begin(115200, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX)

#endif  // TERRARIUM_SMS_PORT_H
