// The only interface between the UI layer (src/ui) and the rest of the
// firmware. The app fills a Model every loop; the UI reads it and reports
// user intent through Actions. Nothing in src/ui touches Wi-Fi, HTTP, NVS or
// hardware, which is what lets the simulator run the same UI on a PC.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "settings.h"
#include "tracker.h"
#include "traffic.h"

namespace ui {

enum class Phase : uint8_t {
  Boot,          // splash
  Connecting,    // joining the saved network after power-on
  Offline,       // saved network unreachable for a while (still retrying)
  SetupJoin,     // setup network up; waiting for a phone to join it
  SetupPortal,   // a phone joined; waiting for the sign-in page to submit
  SetupJoining,  // joining the home network with the submitted password
  SetupOnline,   // joined; waiting for the first flight data
  SetupDone,     // Wi-Fi and internet work
  SetupFailed,   // the attempt failed; see Model::setupFailure
  Running,       // radar
};

enum class SetupFailure : uint8_t {
  None,
  WrongPassword,
  NotFound,
  Refused,
  NoInternet,  // Wi-Fi joined, but no flight data could be downloaded
};

struct Model {
  Phase phase = Phase::Boot;
  uint32_t nowMs = 0;

  // Wi-Fi
  std::string wifiSsid;    // current/last network, or the one being set up
  int savedNetworks = 0;
  std::string apSsid;      // setup network name ("Kona-Radar-7A2C")
  std::string apPassword;  // setup network password ("look-up-7412")
  std::string portalIp;    // "192.168.4.1"
  SetupFailure setupFailure = SetupFailure::None;
  bool online = false;
  std::string lanIp;       // "192.168.1.23" when online
  std::string lanHost;     // "kona-radar.local"
  bool ipNotice = false;   // just connected: show the address for a moment

  // Device
  std::string firmwareVersion;  // "2.2.0"
  std::string otaPassword;      // firmware-update password (shown on the device only)
  int otaProgress = -1;         // 0..100 while a firmware update is received
  bool guidePending = false;    // first run: the gesture guide was never closed

  // Data feed
  bool feedEverOk = false;
  bool feedOk = false;
  uint32_t feedAgeS = 0;   // seconds since the last good download
  std::string feedSource;  // "adsb.fi"
  bool exampleData = false;  // simulator: label the screen as example data

  // Display state (decided by the app)
  bool night = false;      // warm palette
  bool idle = false;       // dim idle screen
  bool blackout = false;   // panel off
  std::string alertHex;    // aircraft to show the full-screen alert for

  radar::Settings settings;
  const radar::Tracker* tracker = nullptr;
  const std::vector<radar::Target>* targets = nullptr;
  radar::TrafficSummary summary;
};

class Actions {
 public:
  virtual ~Actions() = default;
  virtual void setRange(uint16_t nm) = 0;
  virtual void setNorthUp(bool northUp) = 0;
  virtual void toggleNight() = 0;  // manual day/night until the next scheduled change
  virtual void goIdle() = 0;
  virtual void wake() = 0;
  virtual void openRadar() = 0;        // "OPEN RADAR" after setup
  virtual void retrySetup() = 0;       // "TRY AGAIN" / set up Wi-Fi again
  virtual void resetWifi() = 0;        // 5 s hold: forget Wi-Fi, start setup
  virtual void dismissAlert(const char* hex) = 0;
  virtual void guideDone() = 0;        // the gesture guide was closed
  // Physical mounting: clockwise quarter turns of the picture (0..3). The
  // radar's view-up / north-up orientation is separate and unaffected.
  virtual void setScreenRotation(uint8_t quarterTurns) = 0;
};

}  // namespace ui
