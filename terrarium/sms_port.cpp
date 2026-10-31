#include "sms_port.h"
#include "pins.h"

void smsPortBegin() {
  Serial1.begin(115200, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
}
