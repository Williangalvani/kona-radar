// The only interface between the UI layer (src/ui) and the rest of the
// firmware. The app fills a Model every loop; the UI reads it and reports
// user intent through Actions. Nothing in src/ui touches Wi-Fi, HTTP, NVS or
// hardware, which is what lets the simulator run the same UI on a PC.
#pragma once

#include <stdint.h>

#include <string>

#include "settings.h"
#include "tracker.h"

namespace ui {

enum class Phase : uint8_t {
  Boot,         // splash
  Connecting,   // joining the saved network
  Offline,      // saved network unreachable for a while (still retrying)
  SetupJoin,    // setup AP up; waiting for a phone to join it
  SetupPortal,  // a phone joined; waiting for it to submit credentials
  SetupTrying,  // testing submitted credentials
  SetupDone,    // credentials worked
  Running,      // radar
};

struct Model {
  Phase phase = Phase::Boot;
  uint32_t nowMs = 0;

  // Wi-Fi
  std::string wifiSsid;      // saved network, or the one being tried
  std::string apSsid;        // setup network name
  std::string apPassword;    // setup network password
  std::string portalUrl;     // "http://192.168.4.1/"
  std::string setupError;    // why the last setup attempt failed ("" if none)
  bool setupCancellable = false;
  bool online = false;
  std::string lanIp;         // "192.168.1.23" when online
  std::string lanHost;       // "kona-radar.local"

  // Data feed
  bool feedEverOk = false;
  bool feedOk = false;
  uint32_t feedAgeS = 0;     // seconds since the last good download
  std::string feedSource;    // "adsb.fi"
  std::string feedError;

  // Local time "14:32" ("" until NTP sync)
  std::string clock;

  // Display state
  bool dimmed = false;

  radar::Settings settings;
  const radar::Tracker* tracker = nullptr;
};

class Actions {
 public:
  virtual ~Actions() = default;
  virtual void setRange(uint16_t nm) = 0;
  virtual void setBrightness(uint8_t percent) = 0;
  virtual void rotateScreen() = 0;            // +90 degrees physical mounting
  virtual void setViewBearing(int16_t deg) = 0;  // -1 auto, 0..359
  virtual void startWifiSetup() = 0;
  virtual void cancelWifiSetup() = 0;
};

}  // namespace ui
