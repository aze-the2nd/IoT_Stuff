// Smart RTD sensor: reads a Pt-1000 (4-wire, via MAX31865) once a second for
// the live display, persists one sample per minute to a 7-day ring buffer on
// flash, and renders a trend chart of that history.

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "Clock.h"
#include "Config.h"
#include "Display.h"
#include "GithubOta.h"
#include "History.h"
#include "LanOta.h"
#include "RtdSensor.h"

namespace {

RtdSensor g_rtd;
// Heap-allocated (not a static array): HISTORY_CAPACITY * sizeof(HistorySample)
// is ~80KB, which overflows the ESP32's statically-linked DRAM segment
// alongside the Wi-Fi/BT stacks if declared as a global array.
HistorySample* g_windowBuf = nullptr;

uint32_t g_lastSampleMs = 0;
uint32_t g_lastStoreMs = 0;
float g_lastTempC = NAN;
bool g_sensorFault = true;
uint32_t g_lastStoreEpoch = 0;

bool g_settingsOpen = false;
bool g_prevTouched = false;
uint32_t g_lastTouchActionMs = 0;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 400;

void refreshChart() {
  time_t now = time(nullptr);
  size_t count = History::readWindow(static_cast<uint32_t>(now), HISTORY_WINDOW_SECONDS,
                                      g_windowBuf, HISTORY_CAPACITY);
  Display::showChart(g_windowBuf, count, static_cast<uint32_t>(now), HISTORY_WINDOW_SECONDS);
}

void updateStatus() {
  if (Clock::isRealTimeSynced()) {
    Display::showStatus("Wi-Fi + time OK");
  } else if (Clock::hasTime()) {
    Display::showStatus("No Wi-Fi - using last known time");
  } else {
    Display::showStatus("No Wi-Fi/time - history paused");
  }
}

// Redraws the normal (non-settings) view from scratch — needed after
// closing the info screen, since that clears the whole display.
void redrawMainView() {
  Display::drawChrome();
  updateStatus();
  Display::showLiveTemperature(g_lastTempC, !g_sensorFault);
  refreshChart();
}

Display::DeviceInfo gatherDeviceInfo() {
  Display::DeviceInfo info{};
  info.fwVersion = FW_VERSION;
  info.uptimeSeconds = millis() / 1000;

  info.wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (info.wifiConnected) {
    strncpy(info.wifiSsid, WiFi.SSID().c_str(), sizeof(info.wifiSsid) - 1);
    strncpy(info.ipAddress, WiFi.localIP().toString().c_str(), sizeof(info.ipAddress) - 1);
    info.rssi = WiFi.RSSI();
  }

  info.hasTime = Clock::hasTime();
  info.timeSynced = Clock::isRealTimeSynced();

  info.freeHeapBytes = ESP.getFreeHeap();
  info.totalHeapBytes = ESP.getHeapSize();

  History::filesystemUsage(info.usedFsBytes, info.totalFsBytes);
  info.historySamples = History::filledCount();
  info.historyCapacity = HISTORY_CAPACITY;
  info.lastStoreEpoch = g_lastStoreEpoch;

  return info;
}

}  // namespace

void setup() {
  Serial.begin(115200);

  g_windowBuf = new HistorySample[HISTORY_CAPACITY];

  Display::begin();
  Display::showStatus("Connecting Wi-Fi...");

  // Must run before Clock::begin(): Clock persists its anchor timestamp on
  // the same LittleFS volume, and needs it mounted first.
  if (!History::begin()) {
    Display::showStatus("Storage init failed!");
  }

  Clock::begin();
  updateStatus();

  if (!g_rtd.begin()) {
    Display::showStatus("RTD sensor not responding - check wiring");
  }

  refreshChart();
}

void loop() {
  uint32_t now = millis();

  Clock::poll();
  LanOta::poll();
  GithubOta::poll();

  uint32_t correctionFloor;
  int32_t correctionDelta;
  if (Clock::consumeCorrection(correctionFloor, correctionDelta)) {
    History::shiftEpochsFrom(correctionFloor, correctionDelta);
    if (!g_settingsOpen) {
      refreshChart();
      updateStatus();
    }
  }

  if (now - g_lastSampleMs >= SAMPLE_INTERVAL_MS) {
    g_lastSampleMs = now;

    float tempC;
    g_sensorFault = !g_rtd.read(tempC);
    if (!g_sensorFault) {
      g_lastTempC = tempC;
    }
    if (!g_settingsOpen) {
      Display::showLiveTemperature(g_lastTempC, !g_sensorFault);
    }
  }

  if (Clock::hasTime() && !g_sensorFault && now - g_lastStoreMs >= STORE_INTERVAL_MS) {
    g_lastStoreMs = now;
    g_lastStoreEpoch = static_cast<uint32_t>(time(nullptr));
    History::append(g_lastStoreEpoch, g_lastTempC);
    Clock::maybeRefreshAnchor();
    if (!g_settingsOpen) {
      refreshChart();
    }
  }

  // Settings gear: tap it to open the info screen; tap anywhere to close it
  // again. Debounced to one action per physical press (getTouch() reports
  // "touched" for the whole duration of a press, which loop() would
  // otherwise see as many repeated events).
  int16_t tx, ty;
  bool touched = Display::readTouch(tx, ty);
  if (touched && !g_prevTouched && (now - g_lastTouchActionMs) > TOUCH_DEBOUNCE_MS) {
    if (g_settingsOpen) {
      g_lastTouchActionMs = now;
      g_settingsOpen = false;
      redrawMainView();
    } else if (Display::isInGearZone(tx, ty)) {
      g_lastTouchActionMs = now;
      g_settingsOpen = true;
      Display::showInfoScreen(gatherDeviceInfo());
    }
  }
  g_prevTouched = touched;
}
