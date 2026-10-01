#pragma once

#include <Adafruit_MAX31865.h>
#include <algorithm>

#include "Config.h"

class RtdSensor {
 public:
  bool begin() { return thermo_.begin(MAX31865_4WIRE); }

  // Returns true and sets outTempC on success; false on a sensor fault.
  //
  // Takes 3 independent raw reads and requires at least 2 of them to agree
  // within REL_TOLERANCE of each other before trusting the result — single
  // garbage reads (SPI framing desync producing nonsense like -242.02°C or
  // ~988°C, see Config.h's PIN_RTD_CS comment; the MAX31865's own fault
  // register doesn't catch this, it's not an electrical fault) get
  // outvoted instead of reaching History/the chart/the DB upload.
  bool read(float& outTempC) {
    float samples[3];
    int validCount = 0;
    for (int i = 0; i < 3; i++) {
      float t;
      if (readRaw(t)) samples[validCount++] = t;
    }
    if (validCount < 2) return false;

    std::sort(samples, samples + validCount);
    float median = samples[validCount / 2];

    // Average every sample that agrees with the median within tolerance —
    // covers both "all 3 agree" and "2 of 3 agree, 1 is garbage".
    float sum = 0.0f;
    int agreeing = 0;
    for (int i = 0; i < validCount; i++) {
      if (withinTolerance(samples[i], median)) {
        sum += samples[i];
        agreeing++;
      }
    }
    if (agreeing < 2) return false;  // no consensus — treat as a fault

    outTempC = sum / agreeing;
    return true;
  }

 private:
  // Relative agreement threshold between a sample and the median, e.g. 0.05
  // for "within 5%". Guarded with an absolute floor (ABS_TOLERANCE_C) so
  // this doesn't become unreasonably tight for readings near 0°C — not
  // expected for this cellar sensor, but cheap to guard against.
  static constexpr float REL_TOLERANCE = 0.05f;
  static constexpr float ABS_TOLERANCE_C = 1.0f;

  static bool withinTolerance(float value, float reference) {
    float tolerance = std::max(ABS_TOLERANCE_C, std::abs(reference) * REL_TOLERANCE);
    return std::abs(value - reference) <= tolerance;
  }

  bool readRaw(float& outTempC) {
    uint8_t fault = thermo_.readFault();
    if (fault) {
      thermo_.clearFault();
      return false;
    }
    outTempC = thermo_.temperature(RTD_NOMINAL_OHMS, RTD_REF_OHMS);
    return true;
  }

  // Bit-banged (software) SPI rather than a hardware peripheral — keeps
  // the RTD read fully independent of the display/touch HSPI bus. A 1 Hz
  // read has no meaningful performance need for hardware SPI anyway.
  Adafruit_MAX31865 thermo_{PIN_RTD_CS, PIN_RTD_MOSI, PIN_RTD_MISO, PIN_RTD_SCLK};
};
