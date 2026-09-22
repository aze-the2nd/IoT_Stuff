#pragma once

#include <Adafruit_MAX31865.h>

#include "Config.h"

class RtdSensor {
 public:
  bool begin() { return thermo_.begin(MAX31865_4WIRE); }

  // Returns true and sets outTempC on success; false on a sensor fault
  // (open circuit, short, etc. — reported by the MAX31865's fault register).
  bool read(float& outTempC) {
    uint8_t fault = thermo_.readFault();
    if (fault) {
      thermo_.clearFault();
      return false;
    }
    outTempC = thermo_.temperature(RTD_NOMINAL_OHMS, RTD_REF_OHMS);
    return true;
  }

 private:
  // Bit-banged (software) SPI rather than a hardware peripheral — keeps
  // the RTD read fully independent of the display/touch HSPI bus. A 1 Hz
  // read has no meaningful performance need for hardware SPI anyway.
  Adafruit_MAX31865 thermo_{PIN_RTD_CS, PIN_RTD_MOSI, PIN_RTD_MISO, PIN_RTD_SCLK};
};
