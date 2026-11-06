#include "touch_ft3168.h"

#include <Arduino.h>

#include "board_pins.h"

namespace hal {
namespace {
constexpr uint8_t kRegDeviceMode = 0x00;
constexpr uint8_t kRegTdStatus = 0x02;  // followed by P1 XH, XL, YH, YL
constexpr uint8_t kRegAutoMonitor = 0x86;
constexpr uint8_t kRegPowerMode = 0xA5;
}  // namespace

bool TouchFt3168::writeReg(uint8_t reg, uint8_t val) {
  wire_->beginTransmission(TOUCH_I2C_ADDR);
  wire_->write(reg);
  wire_->write(val);
  return wire_->endTransmission() == 0;
}

bool TouchFt3168::configure() {
  // Stay in active mode: in "auto monitor" mode the controller answers
  // slowly after a period without touches.
  bool ok = writeReg(kRegPowerMode, 0x00);
  ok &= writeReg(kRegAutoMonitor, 0x00);
  ok &= writeReg(kRegDeviceMode, 0x00);
  return ok;
}

bool TouchFt3168::begin(TwoWire& wire) {
  wire_ = &wire;
  present_ = configure();
  if (!present_) log_w("FT3168 touch not responding at 0x%02x", TOUCH_I2C_ADDR);
  return present_;
}

bool TouchFt3168::read(uint16_t& x, uint16_t& y) {
  if (!wire_) return false;
  wire_->beginTransmission(TOUCH_I2C_ADDR);
  wire_->write(kRegTdStatus);
  uint8_t buf[5];
  bool ok = wire_->endTransmission(false) == 0 &&
            wire_->requestFrom(static_cast<uint8_t>(TOUCH_I2C_ADDR), static_cast<uint8_t>(5)) == 5;
  if (ok) {
    for (uint8_t& b : buf) b = static_cast<uint8_t>(wire_->read());
  }
  if (!ok) {
    // Re-initialize after a run of bus errors (e.g. ESD or brown-out).
    if (++errors_ >= 20 && millis() - lastRecoverMs_ > 2000) {
      lastRecoverMs_ = millis();
      errors_ = 0;
      present_ = configure();
    }
    return false;
  }
  errors_ = 0;
  present_ = true;
  const uint8_t points = buf[0] & 0x0F;
  if (points == 0 || points > 2) return false;
  x = static_cast<uint16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
  y = static_cast<uint16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
  if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
  if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
  return true;
}

}  // namespace hal
