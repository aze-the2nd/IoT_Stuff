#include "LanOta.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "Config.h"
#include "Display.h"
#include "secrets.h"

namespace {
bool g_started = false;

void setupCallbacks() {
  ArduinoOTA.onStart([]() { Display::showStatus("LAN OTA: update starting..."); });
  ArduinoOTA.onEnd([]() { Display::showStatus("LAN OTA: done, rebooting..."); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    char buf[32];
    snprintf(buf, sizeof(buf), "LAN OTA: %u%%", (done * 100) / total);
    Display::showStatus(buf);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    char buf[32];
    snprintf(buf, sizeof(buf), "LAN OTA error (%d)", static_cast<int>(error));
    Display::showStatus(buf);
  });
}
}  // namespace

void LanOta::poll() {
  if (!g_started) {
    if (WiFi.status() != WL_CONNECTED) return;
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    setupCallbacks();
    ArduinoOTA.begin();
    g_started = true;
    Serial.printf("[LanOta] started at %s (%s)\n", WiFi.localIP().toString().c_str(),
                  OTA_HOSTNAME);
  }
  ArduinoOTA.handle();
}
