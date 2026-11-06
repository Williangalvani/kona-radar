// FocalTech FT3168 capacitive touch controller (I2C 0x38). The board does not
// wire the touch interrupt, so it is polled.
#pragma once

#include <Wire.h>
#include <stdint.h>

namespace hal {

class TouchFt3168 {
 public:
  bool begin(TwoWire& wire);
  // True while a finger is down; x/y in native panel coordinates.
  bool read(uint16_t& x, uint16_t& y);
  bool present() const { return present_; }

 private:
  bool writeReg(uint8_t reg, uint8_t val);
  bool configure();

  TwoWire* wire_ = nullptr;
  bool present_ = false;
  uint8_t errors_ = 0;
  uint32_t lastRecoverMs_ = 0;
};

}  // namespace hal
