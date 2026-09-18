#pragma once

#include <Arduino.h>

// ESP32 die temperature — not calibrated, not ambient. Reflects chip
// self-heating (Wi-Fi/CPU load) far more than the room, easily 10-20°C+
// off. Only useful as a rough live fallback/comparison value when the RTD
// is unavailable — never store this in the history alongside real RTD
// readings.
namespace InternalTemp {
inline float readC() { return temperatureRead(); }
}  // namespace InternalTemp
