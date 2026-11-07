// NVS persistence: radar::Settings (one JSON blob of the changed fields), the
// saved Wi-Fi networks, and per-device secrets (setup network and OTA
// passwords). Missing namespaces and keys are normal on first boot and are
// not logged.
#pragma once

#include <Arduino.h>

#include "known_networks.h"
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

  // Saved Wi-Fi networks. A single network saved by firmware 2.0 and older is
  // moved into the list on first load.
  radar::KnownNetworks loadNetworks();
  void saveNetworks(const radar::KnownNetworks& n);
  void forgetNetworks();

  // Setup-network password, created randomly on first use and kept across
  // Wi-Fi resets so the printed/remembered QR code stays valid.
  String setupPassword();
  // Password for firmware updates (web page and espota), random per device.
  String otaPassword();
  void setOtaPassword(const String& pass);

  // The first-run gesture guide was dismissed.
  bool guideSeen();
  void setGuideSeen(bool seen);

 private:
  void writeNow();

  radar::Settings pending_{};
  bool dirty_ = false;
  uint32_t dueMs_ = 0;
};

}  // namespace app
