// QSPI AMOLED panel driver for the ESP32-S3-Touch-AMOLED-1.43.
//
// Waveshare ships this board with one of two controllers that speak the same
// QSPI protocol but need different init sequences and column offsets:
//   SH8601  (ID register 0xDA reads 0x86) - no offset
//   CO5300  (any other ID, typically 0xFF) - 6 column offset
// The ID is read at boot over a bit-banged 3-wire SPI before the QSPI bus is
// set up, exactly like Waveshare's reference code.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal {

enum class PanelType : uint8_t { SH8601, CO5300 };

class AmoledPanel {
 public:
  // Called from interrupt context when a drawBitmap() transfer finished.
  using FlushDoneCb = void (*)(void* ctx);

  // `maxTransferBytes` is the largest single drawBitmap() payload.
  bool begin(size_t maxTransferBytes, FlushDoneCb onDone, void* ctx);

  PanelType type() const { return type_; }
  uint8_t rawId() const { return rawId_; }
  const char* name() const { return type_ == PanelType::SH8601 ? "SH8601" : "CO5300"; }

  // Sends an inclusive rectangle of RGB565 big-endian pixels (asynchronous,
  // DMA). The buffer must stay valid until the done callback fires. x1/y1
  // must be even and x2/y2 odd (the controllers work on 2x2 pixel pairs).
  void drawBitmap(int x1, int y1, int x2, int y2, const void* pixels);

  // 0..100 %. 0 keeps the panel on at its minimum level.
  void setBrightness(uint8_t percent);
  uint8_t brightness() const { return brightness_; }

  void setDisplayOn(bool on);

 private:
  void* io_ = nullptr;     // esp_lcd_panel_io_handle_t
  void* panel_ = nullptr;  // esp_lcd_panel_handle_t
  PanelType type_ = PanelType::CO5300;
  uint8_t rawId_ = 0;
  uint8_t brightness_ = 0;
};

}  // namespace hal
