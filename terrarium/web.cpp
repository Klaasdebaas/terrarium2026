#include <Arduino.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include "web.h"
#include "webpage.h"
#include "network.h"
#include "config_store.h"
#include "web_json.h"
#include "logformat.h"
#include "clock.h"

static WebServer server(80);
static WebContext* g;
static bool g_restartPending = false;
static uint32_t g_restartAtMs = 0;

static const char* const NEED_PASSWORD = "Stel eerst een webwachtwoord in (config.json of setup)";

// true = doorgaan; false = antwoord is al verstuurd (403 of 401)
static bool requireAuth() {
  const Credentials& cr = g->config->cred;
  if (cr.webPass[0] == 0) {
    server.send(403, "text/plain; charset=utf-8", NEED_PASSWORD);
    return false;
  }
  if (!server.authenticate(cr.webUser, cr.webPass)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

static void sendJson(int code, const JsonDocument& d) {
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}

static void sendOk(int code, bool ok, const char* error) {
  JsonDocument d;
  d["ok"] = ok;
  if (error) d["error"] = error;
  sendJson(code, d);
}

static void handleRoot() { server.send_P(200, "text/html; charset=utf-8", PAGE_DASHBOARD); }
static void handleSettings() {
  if (!requireAuth()) return;
  server.send_P(200, "text/html; charset=utf-8", PAGE_SETTINGS);
}
static void handleSetupPage() { server.send_P(200, "text/html; charset=utf-8", PAGE_SETUP); }

static void handleStatus() {
  static char buf[1536];
  if (buildStatusJson(*g->state, buf, sizeof(buf)) == 0) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"status\"}");
    return;
  }
  server.send(200, "application/json", buf);
}

static void handleGetConfig() {
  if (!requireAuth()) return;
  static char buf[2048];
  if (serializeConfig(*g->config, buf, sizeof(buf), false) == 0) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"config\"}");
    return;
  }
  server.send(200, "application/json", buf);
}

static void handlePostConfig() {
  if (!requireAuth()) return;
  ConfigResult r = parseConfig(server.arg("plain").c_str(), *g->config, true);
  JsonDocument d;
  d["ok"] = r.ok;
  d["rejected"] = r.rejected;
  d["error"] = r.error;
  if (r.ok) {
    configSave(*g->config);
    if (g->onConfigChanged) g->onConfigChanged();
  }
  sendJson(r.ok ? 200 : 400, d);
}

static void handleRelay() {
  if (!requireAuth()) return;
  RelayId r; RelayMode m;
  if (!parseRelayCommand(server.arg("plain").c_str(), r, m)) { sendOk(400, false, "ongeldig commando"); return; }
  setOverride(*g->overrides, r, m, millis());
  sendOk(200, true, nullptr);
}

static void handleTime() {
  if (!requireAuth()) return;
  JsonDocument d;
  LocalTime t;
  if (deserializeJson(d, server.arg("plain")) != DeserializationError::Ok ||
      !d["datetime"].is<const char*>() || !parseIsoLocal(d["datetime"].as<const char*>(), t) ||
      !clockSetLocal(t)) {
    sendOk(400, false, "ongeldige tijd");
    return;
  }
  sendOk(200, true, nullptr);
}

static void handleSmsTest() {
  if (!requireAuth()) return;
  if (g->onSmsTest) g->onSmsTest();
  sendOk(200, true, nullptr);
}

static void handleLogs() {
  String html;
  html.reserve(2048);
  html = F("<!doctype html><html lang=nl><head><meta charset=utf-8>"
           "<meta name=viewport content='width=device-width,initial-scale=1'>"
           "<title>Logbestanden</title><style>body{font-family:sans-serif;margin:1em;max-width:30em}"
           "li{margin:.4em 0}</style></head><body><h1>Logbestanden</h1><p><a href='/'>&larr; Dashboard</a></p><ul>");
  File dir = SD.open("/log");
  int n = 0;
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      const char* name = f.name();
      if (!f.isDirectory() && isSafeLogName(name)) {
        html += F("<li><a href='/logs/"); html += name; html += F("'>"); html += name;
        html += F("</a> ("); html += (unsigned long)f.size(); html += F(" bytes)</li>");
        n++;
      }
      f.close();
      if (n >= 400) break;
    }
  }
  if (dir) dir.close();
  if (n == 0) html += F("<li>Geen logbestanden</li>");
  html += F("</ul></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

static void handleLogFile() {
  String name = server.pathArg(0);
  if (!isSafeLogName(name.c_str())) { server.send(404, "text/plain", "Niet gevonden"); return; }
  String path = "/log/" + name;
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); server.send(404, "text/plain", "Niet gevonden"); return; }
  server.streamFile(f, "text/csv");
  f.close();
}

static void handleSetup() {
  char ssid[33], pass[65], webPass[65];
  if (!parseSetupRequest(server.arg("plain").c_str(), ssid, sizeof(ssid), pass, sizeof(pass),
                         webPass, sizeof(webPass))) {
    sendOk(400, false, "ongeldige invoer (ssid verplicht, webwachtwoord min. 6 tekens)");
    return;
  }
  Credentials& cr = g->config->cred;
  strncpy(cr.wifiSsid, ssid, sizeof(cr.wifiSsid) - 1); cr.wifiSsid[sizeof(cr.wifiSsid) - 1] = 0;
  strncpy(cr.wifiPass, pass, sizeof(cr.wifiPass) - 1); cr.wifiPass[sizeof(cr.wifiPass) - 1] = 0;
  strncpy(cr.webPass, webPass, sizeof(cr.webPass) - 1); cr.webPass[sizeof(cr.webPass) - 1] = 0;
  configSave(*g->config);
  sendOk(200, true, nullptr);
  g_restartPending = true;
  g_restartAtMs = millis();
}

static void handleNotFound() { server.send(404, "text/plain", "Niet gevonden"); }

void webBegin(WebContext& ctx) {
  g = &ctx;
  if (networkApMode()) {
    server.on("/", HTTP_GET, handleSetupPage);
    server.on("/api/setup", HTTP_POST, handleSetup);
  } else {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/logs", HTTP_GET, handleLogs);
    server.on(UriBraces("/logs/{}"), HTTP_GET, handleLogFile);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.on("/api/relay", HTTP_POST, handleRelay);
    server.on("/api/time", HTTP_POST, handleTime);
    server.on("/api/sms/test", HTTP_POST, handleSmsTest);
  }
  server.onNotFound(handleNotFound);
  server.begin();
}

void webUpdate() {
  server.handleClient();
  if (g_restartPending && (uint32_t)(millis() - g_restartAtMs) >= 1000) ESP.restart();
}
