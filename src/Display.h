#pragma once

#include "History.h"

namespace Display {

void begin();

// Boot/error status line at the top of the screen.
void showStatus(const char* msg);

// Big live reading. NAN tempC (or sensorOk == false) shows a fault state.
void showLiveTemperature(float tempC, bool sensorOk);

// Redraws the 7-day trend chart from a set of (already time-sorted) samples.
void showChart(const HistorySample* samples, size_t count, uint32_t nowEpoch,
                uint32_t windowSeconds);

}  // namespace Display
