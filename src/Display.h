#pragma once

#include <Arduino.h>

#include "History.h"

namespace Display {

void begin();

// Redraws the static "chrome" (chart border, settings gear) — call after
// showInfoScreen() to restore the normal view, since that clears the
// whole screen.
void drawChrome();

// Boot/error status line at the top of the screen.
void showStatus(const char* msg);

// Big live reading. NAN tempC (or sensorOk == false) shows a fault state.
void showLiveTemperature(float tempC, bool sensorOk);

// Redraws the 7-day trend chart from a set of (already time-sorted) samples.
void showChart(const HistorySample* samples, size_t count, uint32_t nowEpoch,
                uint32_t windowSeconds);

// Polls the touchscreen. Returns true and fills x/y (screen coords) if
// currently touched.
bool readTouch(int16_t& x, int16_t& y);

// Whether a touch at x,y landed on the settings gear icon (top-right).
bool isInGearZone(int16_t x, int16_t y);

// Everything the settings/info screen shows — gathered by main.cpp (it has
// access to Wi-Fi/Clock/History), formatted and rendered by Display.
struct DeviceInfo {
  const char* fwVersion;
  uint32_t uptimeSeconds;
  bool wifiConnected;
  char wifiSsid[33];
  char ipAddress[16];
  int rssi;
  bool hasTime;
  bool timeSynced;  // NTP-confirmed vs anchor-estimated (only meaningful if hasTime)
  uint32_t freeHeapBytes;
  uint32_t totalHeapBytes;
  uint32_t usedFsBytes;
  uint32_t totalFsBytes;
  uint32_t historySamples;
  uint32_t historyCapacity;
  uint32_t lastStoreEpoch;  // 0 = never
};

// Full-screen info panel. Tap anywhere to return to the normal view (see
// drawChrome()).
void showInfoScreen(const DeviceInfo& info);

}  // namespace Display
