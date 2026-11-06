// Wi-Fi connection and first-time setup.
//
// Normal operation: connect to the saved network, auto-reconnect forever.
// Setup ("portal") mode: the device opens a WPA2 access point whose name and
// password are shown on screen as a QR code. Phones that join get a captive
// portal page (served by WebUi) where they pick their home network. The
// device tests the credentials while keeping the access point up, reports
// success/failure both on screen and on the phone, and only then saves them.
#pragma once

#include <Arduino.h>
#include <DNSServer.h>

namespace app {
class SettingsStore;
}

namespace net {

enum class WifiState : uint8_t {
  Off,           // not started
  Connecting,    // first attempt with the saved network after boot
  Connected,     // normal operation (link may still drop; see online())
  Offline,       // saved network unreachable after the boot timeout; retrying
  Portal,        // setup access point up, waiting for credentials
  PortalTrying,  // testing credentials submitted from the portal
  PortalDone,    // credentials worked; access point closes shortly
};

class WifiManager {
 public:
  void begin(app::SettingsStore* store);
  void loop();

  void startPortal();
  // Leaves setup mode and goes back to the saved network (if any).
  void cancelPortal();
  // Called by the portal page. Returns false if the request is invalid.
  bool submitCredentials(const String& ssid, const String& pass);
  void startScan();
  // JSON: {"scanning":bool,"networks":[{"ssid":..,"rssi":..,"secure":..}]}
  String scanJson();

  WifiState state() const { return state_; }
  uint32_t stateSinceMs() const { return stateSince_; }
  bool online() const;
  bool portalActive() const {
    return state_ == WifiState::Portal || state_ == WifiState::PortalTrying ||
           state_ == WifiState::PortalDone;
  }
  bool hasSavedNetwork() const { return savedSsid_.length() > 0; }
  const String& savedSsid() const { return savedSsid_; }
  const String& tryingSsid() const { return trySsid_; }
  const String& apSsid() const { return apSsid_; }
  const String& apPassword() const { return apPass_; }
  int apClientCount() const;
  // Human-readable reason for the last failed portal attempt ("" if none).
  const String& lastError() const { return lastError_; }
  String localIp() const;
  static constexpr const char* kPortalIp = "192.168.4.1";

 private:
  void setState(WifiState s);
  void connectSaved();
  void stopAccessPoint();

  app::SettingsStore* store_ = nullptr;
  WifiState state_ = WifiState::Off;
  uint32_t stateSince_ = 0;
  String savedSsid_, savedPass_;
  String trySsid_, tryPass_;
  String apSsid_, apPass_;
  String lastError_;
  DNSServer dns_;
  uint32_t lastKickMs_ = 0;
  bool scanRequested_ = false;
};

}  // namespace net
