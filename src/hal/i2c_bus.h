// The board's shared I2C bus (touch, IMU, RTC, the 4-pin header) on ESP-IDF's
// i2c_master driver. Every call is safe from any task: transactions are
// serialized by the bus mutex, and lock() holds the bus across a sequence.
//
// Failed transfers are reported by return value only. A touch controller that
// sleeps NACKs every poll, and the driver would otherwise log each one.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stddef.h>
#include <stdint.h>

namespace hal {

class I2cBus {
 public:
  bool begin(int sda, int scl, uint32_t hz);
  bool ready() const { return bus_ != nullptr; }

  // True if a device acknowledges `addr`.
  bool probe(uint8_t addr);
  // Logs every address that answers (with the part it usually is on this
  // board) at info level. Returns the number found.
  int scanAndLog();

  bool writeReg(uint8_t addr, uint8_t reg, uint8_t value);
  bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len);

  class Lock {
   public:
    explicit Lock(I2cBus& b) : b_(b) { xSemaphoreTakeRecursive(b_.mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGiveRecursive(b_.mutex_); }

   private:
    I2cBus& b_;
  };

 private:
  void* device(uint8_t addr);  // i2c_master_dev_handle_t, added on first use

  SemaphoreHandle_t mutex_ = xSemaphoreCreateRecursiveMutex();
  void* bus_ = nullptr;  // i2c_master_bus_handle_t
  void* devs_[128] = {};
  uint32_t hz_ = 400000;
};

}  // namespace hal
