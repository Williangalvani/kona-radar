// HTTP server on port 80:
//  * setup mode: captive portal (OS connectivity probes are redirected to the
//    Wi-Fi picker page) plus the JSON endpoints that page uses;
//  * normal mode: the settings page at http://kona-radar.local/ for anyone on
//    the same network, editing the NVS-stored settings.
#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <functional>

#include "settings.h"

namespace net {

class WifiManager;

class WebUi {
 public:
  struct Hooks {
    std::function<radar::Settings()> getSettings;
    // Applies (and persists) new settings.
    std::function<void(const radar::Settings&)> applySettings;
    std::function<void()> resetSettings;
    // Extra fields for /api/status as a JSON object body ("{...}").
    std::function<String()> statusJson;
  };

  void begin(WifiManager* wifi, Hooks hooks);
  void loop();

 private:
  bool redirectToPortal();
  void sendJson(int code, const String& body);
  void handleRoot();
  void handleWifiStatus();
  void handleWifiConnect();
  void handleScan();
  void handleGetSettings();
  void handlePostSettings();
  void handleReset();
  void handleStatus();
  void handleNotFound();

  WebServer server_{80};
  WifiManager* wifi_ = nullptr;
  Hooks hooks_;
  bool mdnsStarted_ = false;
};

}  // namespace net
