// Firmware updates over Wi-Fi: the password-protected upload on the web
// page (/update) and ArduinoOTA for espota / `pio run -e kona-radar-ota -t
// upload`. Both use the same per-device password.
//
// A new image boots "pending verify". It is confirmed once it has run for a
// while and reached the network (or opened setup); if it resets before
// that - a crash loop, a hang caught by the watchdog - the bootloader goes
// back to the previous firmware.
#pragma once

#include <Arduino.h>

#include <functional>

namespace app {
class SettingsStore;
}

namespace net {

class Ota {
 public:
  // pct 0..100 while an update runs, -1 when it ended (either way). Called
  // from inside the upload, which blocks the main loop meanwhile.
  using ProgressFn = std::function<void(int pct)>;

  void begin(app::SettingsStore* store, ProgressFn progress);
  void loop(bool online, bool setupActive);

  const String& password() const { return password_; }
  bool checkPassword(const String& candidate) const;
  // 8-32 printable characters, no spaces.
  bool setPassword(const String& pass, String& error);

  // Web upload, chunk by chunk. `size` 0 = unknown.
  bool uploadStart(size_t size, String& error);
  bool uploadWrite(const uint8_t* data, size_t len, String& error);
  bool uploadEnd(String& error);
  void uploadAbort();
  void rebootSoon(uint32_t delayMs = 1500);

  bool busy() const { return busy_; }
  int progress() const { return busy_ ? pct_ : -1; }
  // The running image still waits for confirmation.
  bool pendingVerify() const { return pendingVerify_; }
  const char* runningPartition() const;

 private:
  void report(int pct);

  app::SettingsStore* store_ = nullptr;
  ProgressFn progress_;
  String password_;
  bool arduinoOtaStarted_ = false;
  bool pendingVerify_ = false;
  bool busy_ = false;
  int pct_ = -1;
  size_t total_ = 0, written_ = 0;
  bool headerChecked_ = false;
  uint32_t lastReportMs_ = 0;
  uint32_t rebootAtMs_ = 0;
  uint32_t onlineSinceMs_ = 0;
};

}  // namespace net
