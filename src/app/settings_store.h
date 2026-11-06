// NVS persistence for radar::Settings (stored as one JSON blob) and the
// Wi-Fi credentials.
#pragma once

#include <Arduino.h>

#include "settings.h"

namespace app {

class SettingsStore {
 public:
  // Loads saved settings over the factory defaults.
  radar::Settings load();
  // Saves after `delayMs` of quiet (coalesces rapid changes such as range
  // button taps to limit flash wear). 0 saves immediately.
  void save(const radar::Settings& s, uint32_t delayMs = 0);
  // Call regularly; performs delayed saves.
  void loop();
  void resetToDefaults();

  bool loadWifi(String& ssid, String& pass);
  void saveWifi(const String& ssid, const String& pass);
  void forgetWifi();
  // Setup-network password, created randomly on first use and kept across
  // Wi-Fi resets so the printed/remembered QR code stays valid.
  String setupPassword();

 private:
  void writeNow();

  radar::Settings pending_{};
  bool dirty_ = false;
  uint32_t dueMs_ = 0;
};

}  // namespace app
