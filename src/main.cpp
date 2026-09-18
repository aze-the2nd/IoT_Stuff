// Smart RTD sensor: reads a Pt-1000 (4-wire, via MAX31865) once a second for
// the live display, persists one sample per minute to a 7-day ring buffer on
// flash, and renders a trend chart of that history.

#include <Arduino.h>
#include <time.h>

#include "Clock.h"
#include "Config.h"
#include "Display.h"
#include "GithubOta.h"
#include "History.h"
#include "InternalTemp.h"
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
float g_lastTempC = NAN;         // most recent value eligible for storage
bool g_haveStorableValue = false;
bool g_sensorFault = true;

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
    refreshChart();
    updateStatus();
  }

  if (now - g_lastSampleMs >= SAMPLE_INTERVAL_MS) {
    g_lastSampleMs = now;

    float tempC;
    g_sensorFault = !g_rtd.read(tempC);

    if (!g_sensorFault) {
      g_lastTempC = tempC;
      g_haveStorableValue = true;
      Display::showLiveTemperature(g_lastTempC, Display::TempSource::kRtd);
    } else {
      // RTD unavailable — show the ESP32's internal die temperature as a
      // rough live/comparison value. Uncalibrated and dominated by chip
      // self-heating, not the room — whether it also gets persisted to
      // history (e.g. just to see what the chart looks like before the RTD
      // is wired up) is controlled by STORE_INTERNAL_FALLBACK_IN_HISTORY in
      // Config.h; turn that off once real data matters.
      float internalC = InternalTemp::readC();
      Display::showLiveTemperature(internalC, Display::TempSource::kInternalFallback);
      g_haveStorableValue = STORE_INTERNAL_FALLBACK_IN_HISTORY;
      if (g_haveStorableValue) {
        g_lastTempC = internalC;
      }
    }
  }

  if (Clock::hasTime() && g_haveStorableValue && now - g_lastStoreMs >= STORE_INTERVAL_MS) {
    g_lastStoreMs = now;
    History::append(static_cast<uint32_t>(time(nullptr)), g_lastTempC);
    Clock::maybeRefreshAnchor();
    refreshChart();
  }
}
