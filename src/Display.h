#pragma once

#include "History.h"

namespace Display {

enum class TempSource {
  kRtd,               // real Pt-1000/MAX31865 reading
  kInternalFallback,  // RTD unavailable — showing the ESP32's die temp instead
  kNone,              // no reading at all (yet, or fallback failed too)
};

void begin();

// Boot/error status line at the top of the screen.
void showStatus(const char* msg);

// Big live reading. source selects color/labeling; kNone shows a fault
// state regardless of tempC.
void showLiveTemperature(float tempC, TempSource source);

// Redraws the 7-day trend chart from a set of (already time-sorted) samples.
void showChart(const HistorySample* samples, size_t count, uint32_t nowEpoch,
                uint32_t windowSeconds);

}  // namespace Display
