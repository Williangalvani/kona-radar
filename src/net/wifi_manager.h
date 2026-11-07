// Wi-Fi connection and first-time setup.
//
// Normal operation: the device remembers up to 8 networks (radar::KnownNetworks
// in NVS). It scans, joins the strongest saved network in range, falls back
// through the rest (including hidden ones), and does the same again whenever
// the link stays down for a few seconds, so it roams between saved networks.
//
// Setup ("portal") mode: the device opens a WPA2 access point whose name and
// password are shown on screen as a QR code. Phones that join get a captive
// portal page (served by WebUi) where they pick a network. The device tests
// the credentials while keeping the access point up, adds them to the list
// once the network accepts them, then waits for the app to confirm that
// flight data can actually be downloaded before calling setup done.
#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#include <vector>

#include "known_networks.h"

namespace app {
class SettingsStore;
}

namespace net {

enum class WifiState : uint8_t {
  Off,           // not started
  Connecting,    // first attempt with the saved networks after boot
  Connected,     // normal operation (link may still drop; see online())
  Offline,       // no saved network reachable after the boot timeout; retrying
  Portal,        // setup access point up, waiting for credentials
  PortalTrying,  // testing credentials submitted from the portal
  PortalOnline,  // joined and saved; waiting for confirmInternet()/failInternet()
  PortalDone,    // everything works; access point closes shortly
  PortalFailed,  // last attempt failed (see setupError()); access point stays up
};

enum class SetupError : uint8_t { None, WrongPassword, NotFound, Refused, NoInternet };

class WifiManager {
 public:
  void begin(app::SettingsStore* store);
  void loop();

  void startPortal();
  // Forgets every saved network and opens setup (the 5 s hold).
  void resetAndStartPortal();
  // After a failure: back to waiting for the phone, or (no internet) check again.
  void retry();
  // Called by the app while PortalOnline.
  void confirmInternet();
  void failInternet();
  // Called by the portal page. Returns false if the request is invalid.
  bool submitCredentials(const String& ssid, const String& pass);
  void startScan();
  // JSON: {"scanning":bool,"networks":[{"ssid":..,"rssi":..,"secure":..,"saved":..}]}
  String scanJson();

  // Saved networks for the settings page (names only, never passwords):
  // {"max":8,"current":"..","networks":[{"ssid":..,"open":..,"current":..,"rssi":..|null}]}
  String networksJson() const;
  // Adds or updates a saved network without dropping the current connection.
  // `error` explains a rejection.
  bool addNetwork(const String& ssid, const String& pass, String& error);
  bool removeNetwork(const String& ssid);

  WifiState state() const { return state_; }
  uint32_t stateSinceMs() const { return stateSince_; }
  bool online() const;
  bool portalActive() const {
    return state_ == WifiState::Portal || state_ == WifiState::PortalTrying ||
           state_ == WifiState::PortalOnline || state_ == WifiState::PortalDone ||
           state_ == WifiState::PortalFailed;
  }
  size_t savedCount() const { return networks_.size(); }
  bool hasSavedNetwork() const { return !networks_.empty(); }
  // The network in use, else the one being tried, else the most recent one.
  String currentSsid() const;
  const String& tryingSsid() const { return trySsid_; }
  const String& apSsid() const { return apSsid_; }
  const String& apPassword() const { return apPass_; }
  int apClientCount() const;
  SetupError setupError() const { return setupError_; }
  // Human-readable reason for the last failed portal attempt ("" if none).
  const String& lastError() const { return lastError_; }
  String localIp() const;
  static constexpr const char* kPortalIp = "192.168.4.1";

 private:
  // Background joining of saved networks (outside setup).
  enum class Link : uint8_t { Idle, Scanning, Trying, Up, Waiting };
  struct SeenNet {
    String ssid;
    int rssi;
    bool secure;
  };

  void setState(WifiState s);
  void connectKnown();
  void linkLoop(uint32_t now);
  void tryNext(uint32_t now);
  void scanLoop(uint32_t now);
  void stopAccessPoint();
  void staDisconnect();
  void saveNetworks();

  app::SettingsStore* store_ = nullptr;
  WifiState state_ = WifiState::Off;
  uint32_t stateSince_ = 0;
  uint32_t portalUsedMs_ = 0;  // last time a phone was on the setup network
  radar::KnownNetworks networks_;
  String trySsid_, tryPass_;
  String connectedSsid_;
  String apSsid_, apPass_;
  String lastError_;
  SetupError setupError_ = SetupError::None;
  DNSServer dns_;

  Link link_ = Link::Idle;
  std::vector<size_t> order_;
  size_t attempt_ = 0;
  uint32_t linkSince_ = 0;
  uint32_t waitMs_ = 0;
  uint32_t lostSinceMs_ = 0;
  uint32_t scanGenWanted_ = 0;

  bool scanRequested_ = false;
  bool scanRunning_ = false;
  uint32_t scanGen_ = 0;
  std::vector<SeenNet> lastScan_;
};

}  // namespace net
