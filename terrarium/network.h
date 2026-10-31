#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

void networkBegin(const Config& c);      // lege ssid -> AP "Terrarium-Setup" (open, 192.168.4.1); anders STA + hostname + mDNS
void networkUpdate(uint32_t nowMs);      // STA: bij geen verbinding elke 60 s WiFi.reconnect()
bool networkConnected();
bool networkApMode();
int8_t networkRssi();
void networkIp(char* buf, size_t len);
bool networkTakeEvent(bool& connected);  // true bij verandering van verbinding
