#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <stdio.h>
#include <string.h>
#include "network.h"

static bool g_ap = false;
static bool g_connected = false;     // laatst gemeld
static bool g_eventPending = false;
static bool g_mdnsStarted = false;
static uint32_t g_lastTryMs = 0;
static char g_hostname[33] = "terrarium";

static const uint32_t RECONNECT_MS = 60000;

void networkBegin(const Config& c) {
  strncpy(g_hostname, c.cred.hostname, sizeof(g_hostname) - 1);
  g_hostname[sizeof(g_hostname) - 1] = 0;
  g_connected = false; g_eventPending = false; g_mdnsStarted = false;
  g_lastTryMs = millis();
  if (c.cred.wifiSsid[0] == 0) {
    g_ap = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Terrarium-Setup");   // open netwerk, standaard 192.168.4.1
    return;
  }
  g_ap = false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_hostname);
  WiFi.setAutoReconnect(true);
  WiFi.begin(c.cred.wifiSsid, c.cred.wifiPass);   // niet-blokkerend
}

void networkUpdate(uint32_t nowMs) {
  if (g_ap) return;
  bool now = WiFi.status() == WL_CONNECTED;
  if (now != g_connected) {
    g_connected = now;
    g_eventPending = true;
  }
  if (now) {
    if (!g_mdnsStarted) {
      if (MDNS.begin(g_hostname)) { MDNS.addService("http", "tcp", 80); g_mdnsStarted = true; }
    }
    g_lastTryMs = nowMs;
  } else {
    g_mdnsStarted = false;
    if ((uint32_t)(nowMs - g_lastTryMs) >= RECONNECT_MS) {
      g_lastTryMs = nowMs;
      WiFi.reconnect();
    }
  }
}

bool networkConnected() { return !g_ap && WiFi.status() == WL_CONNECTED; }
bool networkApMode() { return g_ap; }
int8_t networkRssi() { return networkConnected() ? (int8_t)WiFi.RSSI() : 0; }

void networkIp(char* buf, size_t len) {
  if (!buf || len == 0) return;
  IPAddress ip = g_ap ? WiFi.softAPIP() : (networkConnected() ? WiFi.localIP() : IPAddress(0, 0, 0, 0));
  snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool networkTakeEvent(bool& connected) {
  if (!g_eventPending) return false;
  g_eventPending = false;
  connected = g_connected;
  return true;
}
