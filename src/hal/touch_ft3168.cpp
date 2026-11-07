#include "touch_ft3168.h"

#include <Arduino.h>

#include "board_pins.h"

namespace hal {
namespace {

constexpr uint8_t kAddrFocalTech = TOUCH_I2C_ADDR;
constexpr uint8_t kAddrCst816 = 0x15;

// FocalTech register map (FT3168 / FT6x36 family).
constexpr uint8_t kRegDeviceMode = 0x00;
constexpr uint8_t kRegTdStatus = 0x02;  // followed by P1 XH, XL, YH, YL
constexpr uint8_t kRegCtrl = 0x86;      // 0 = stay active, 1 = monitor mode when idle
constexpr uint8_t kRegChipId = 0xA3;
constexpr uint8_t kRegPowerMode = 0xA5;  // 0 = active
constexpr uint8_t kRegFirmware = 0xA6;
constexpr uint8_t kRegVendor = 0xA8;
// CST816: auto sleep off (it stops answering I2C while asleep).
constexpr uint8_t kRegCstDisAutoSleep = 0xFE;
constexpr uint8_t kRegCstChipId = 0xA7;

constexpr uint32_t kProbeEveryMs = 100;
constexpr uint32_t kReconfigureEveryMs = 10000;
constexpr uint8_t kErrorsUntilLost = 3;

}  // namespace

bool TouchFt3168::configure() {
  if (addr_ == kAddrCst816) return bus_->writeReg(addr_, kRegCstDisAutoSleep, 0x01);
  bool ok = bus_->writeReg(addr_, kRegDeviceMode, 0x00);
  ok &= bus_->writeReg(addr_, kRegCtrl, 0x00);
  ok &= bus_->writeReg(addr_, kRegPowerMode, 0x00);
  return ok;
}

// Once a controller has been seen, only its address is probed: touching other
// addresses keeps a sleeping FT3168 from answering (datasheet 2.3).
bool TouchFt3168::attach() {
  const uint8_t candidates[2] = {kAddrFocalTech, kAddrCst816};
  for (uint8_t a : candidates) {
    if (everFound_ && a != addr_) continue;
    if (!bus_->probe(a)) continue;
    addr_ = a;
    if (!configure()) continue;
    uint8_t id[3] = {};
    if (a == kAddrCst816) {
      bus_->readRegs(a, kRegCstChipId, id, 1);
    } else {
      bus_->readRegs(a, kRegChipId, &id[0], 1);
      bus_->readRegs(a, kRegFirmware, &id[1], 1);
      bus_->readRegs(a, kRegVendor, &id[2], 1);
    }
    if (!everFound_) {
      if (a == kAddrCst816)
        log_i("touch: CST816-family controller at 0x%02X (chip 0x%02X)", a, id[0]);
      else
        log_i("touch: FocalTech controller at 0x%02X (chip 0x%02X, fw 0x%02X, vendor 0x%02X)", a, id[0], id[1], id[2]);
    } else {
      log_i("touch: controller answering again after %lu ms", static_cast<unsigned long>(millis() - lostSinceMs_));
    }
    everFound_ = present_ = true;
    errors_ = 0;
    lastConfigMs_ = millis();
    return true;
  }
  return false;
}

bool TouchFt3168::begin(I2cBus& bus, uint32_t waitMs) {
  bus_ = &bus;
  return retry(waitMs);
}

bool TouchFt3168::retry(uint32_t waitMs) {
  if (!bus_ || !bus_->ready()) return false;
  const uint32_t start = millis();
  for (;;) {
    if (attach()) return true;
    if (millis() - start >= waitMs) break;
    delay(50);
  }
  log_w("touch: no controller at 0x%02X or 0x%02X after %lu ms; still looking (a tap wakes a sleeping FT3168)",
        kAddrFocalTech, kAddrCst816, static_cast<unsigned long>(waitMs));
  nextProbeMs_ = millis() + kProbeEveryMs;
  return false;
}

bool TouchFt3168::read(uint16_t& x, uint16_t& y) {
  if (!bus_) return false;
  const uint32_t now = millis();
  if (!present_) {
    if (static_cast<int32_t>(now - nextProbeMs_) < 0) return false;
    nextProbeMs_ = now + kProbeEveryMs;
    if (!attach()) return false;
  }
  uint8_t buf[5];
  if (!bus_->readRegs(addr_, kRegTdStatus, buf, sizeof(buf))) {
    if (++errors_ >= kErrorsUntilLost) {
      present_ = false;
      lostSinceMs_ = now;
      nextProbeMs_ = now + kProbeEveryMs;
      if (now - lastLostLogMs_ > 60000) {
        lastLostLogMs_ = now;
        log_w("touch: controller stopped answering; reconnecting");
      }
    }
    return false;
  }
  errors_ = 0;
  // In case the controller reset itself (ESD, brown-out) and fell back to its
  // idle-sleep default.
  if (now - lastConfigMs_ > kReconfigureEveryMs) {
    lastConfigMs_ = now;
    configure();
  }
  const uint8_t points = buf[0] & 0x0F;
  if (points == 0 || points > 2) return false;
  x = static_cast<uint16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
  y = static_cast<uint16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
  if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
  if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
  return true;
}

}  // namespace hal
