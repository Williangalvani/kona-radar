// Capacitive touch on the ESP32-S3-Touch-AMOLED-1.43: a FocalTech FT3168 at
// I2C 0x38 (Waveshare's 1.43 demo, touch_bsp.c / FT3168.cpp). The schematic
// leaves the panel connector's touch INT and RESET pins unconnected (J9 pins
// 7 and 9), so the controller is polled and cannot be hardware-reset by the
// ESP32. Its supply is the always-on 3V3 rail.
//
// FT3168 datasheet, 2.3: in Monitor/Sleep mode the controller stops answering
// I2C (after other traffic on the bus) until a touch wakes it or its firmware
// clears the I2C state machine. It drops into Monitor mode after a few idle
// seconds unless told not to, which is what configure() does, again after
// every reconnect. A soft reboot does not reset it, so it may still be asleep
// when the firmware starts.
//
// CST816-family controllers (0x15) use the same point registers and are
// accepted too, in case a board revision ships one.
#pragma once

#include <stdint.h>

#include "i2c_bus.h"

namespace hal {

class TouchFt3168 {
 public:
  // Looks for the controller for up to `waitMs` (it needs >= 70 ms after
  // power-on and may be asleep). Returns true if it answered. If not, read()
  // keeps looking in the background.
  bool begin(I2cBus& bus, uint32_t waitMs);
  // Keeps trying for up to `waitMs` more (after a panel reset, for example).
  bool retry(uint32_t waitMs);
  // True while a finger is down; x/y in native panel coordinates.
  bool read(uint16_t& x, uint16_t& y);
  bool present() const { return present_; }
  uint8_t address() const { return addr_; }

 private:
  bool attach();
  bool configure();

  I2cBus* bus_ = nullptr;
  bool present_ = false;
  bool everFound_ = false;
  uint8_t addr_ = 0;
  uint8_t errors_ = 0;
  uint32_t nextProbeMs_ = 0;
  uint32_t lastConfigMs_ = 0;
  uint32_t lostSinceMs_ = 0;
  uint32_t lastLostLogMs_ = 0;
};

}  // namespace hal
