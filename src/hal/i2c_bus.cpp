#include "i2c_bus.h"

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <esp_log.h>

#include <string>

namespace hal {
namespace {

constexpr int kTimeoutMs = 20;

const char* partName(uint8_t a) {
  switch (a) {
    case 0x38:
      return "FT3168 touch";
    case 0x15:
      return "CST816-family touch";
    case 0x5A:
      return "CST92xx touch";
    case 0x5D:
    case 0x14:
      return "GT911 touch";
    case 0x51:
      return "PCF85063 RTC";
    case 0x6A:
    case 0x6B:
      return "QMI8658 IMU";
    default:
      return "unknown";
  }
}

}  // namespace

bool I2cBus::begin(int sda, int scl, uint32_t hz) {
  if (bus_) return true;
  hz_ = hz;
  // NACKs are expected while the touch controller sleeps; callers handle them.
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = I2C_NUM_0;
  cfg.sda_io_num = static_cast<gpio_num_t>(sda);
  cfg.scl_io_num = static_cast<gpio_num_t>(scl);
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t h = nullptr;
  const esp_err_t err = i2c_new_master_bus(&cfg, &h);
  if (err != ESP_OK) {
    log_e("I2C bus init failed: %s", esp_err_to_name(err));
    return false;
  }
  bus_ = h;
  return true;
}

void* I2cBus::device(uint8_t addr) {
  if (!bus_ || addr >= 128) return nullptr;
  if (!devs_[addr]) {
    i2c_device_config_t d = {};
    d.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    d.device_address = addr;
    d.scl_speed_hz = hz_;
    i2c_master_dev_handle_t h = nullptr;
    if (i2c_master_bus_add_device(static_cast<i2c_master_bus_handle_t>(bus_), &d, &h) != ESP_OK) return nullptr;
    devs_[addr] = h;
  }
  return devs_[addr];
}

bool I2cBus::probe(uint8_t addr) {
  if (!bus_) return false;
  Lock l(*this);
  return i2c_master_probe(static_cast<i2c_master_bus_handle_t>(bus_), addr, kTimeoutMs) == ESP_OK;
}

int I2cBus::scanAndLog() {
  if (!bus_) {
    log_i("I2C scan: bus not available");
    return 0;
  }
  Lock l(*this);
  std::string found;
  int n = 0;
  for (uint8_t a = 0x08; a < 0x78; ++a) {
    if (i2c_master_probe(static_cast<i2c_master_bus_handle_t>(bus_), a, kTimeoutMs) != ESP_OK) continue;
    char buf[48];
    snprintf(buf, sizeof(buf), "%s0x%02X (%s)", n ? ", " : "", a, partName(a));
    found += buf;
    ++n;
  }
  log_i("I2C scan: %d device(s)%s%s", n, n ? ": " : "", found.c_str());
  return n;
}

bool I2cBus::writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Lock l(*this);
  void* dev = device(addr);
  if (!dev) return false;
  const uint8_t b[2] = {reg, value};
  return i2c_master_transmit(static_cast<i2c_master_dev_handle_t>(dev), b, 2, kTimeoutMs) == ESP_OK;
}

bool I2cBus::readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Lock l(*this);
  void* dev = device(addr);
  if (!dev) return false;
  return i2c_master_transmit_receive(static_cast<i2c_master_dev_handle_t>(dev), &reg, 1, buf, len, kTimeoutMs) ==
         ESP_OK;
}

}  // namespace hal
