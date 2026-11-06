#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>

#include <algorithm>
#include <vector>

#include "app/settings_store.h"
#include "config.h"

namespace net {
namespace {

constexpr uint32_t kTryTimeoutMs = 20000;
constexpr uint32_t kDoneLingerMs = 10000;
constexpr uint32_t kReconnectKickMs = 30000;

volatile uint8_t g_lastReason = 0;

void onStaDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  g_lastReason = info.wifi_sta_disconnected.reason;
}

SetupError reasonError(uint8_t r) {
  switch (r) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return SetupError::WrongPassword;
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
      return SetupError::NotFound;
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_CONNECTION_FAIL:
      return SetupError::Refused;
    default:
      return SetupError::WrongPassword;
  }
}

const char* errorText(SetupError e) {
  switch (e) {
    case SetupError::WrongPassword:
      return "Wrong password - please check it and try again.";
    case SetupError::NotFound:
      return "Network not found. It must be a 2.4 GHz network within range.";
    case SetupError::Refused:
      return "The router refused the connection. Try again.";
    case SetupError::NoInternet:
      return "Joined the Wi-Fi, but flight data could not be downloaded. Check the router's internet connection.";
    default:
      return "";
  }
}

}  // namespace

void WifiManager::begin(app::SettingsStore* store) {
  store_ = store;
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[40];
  snprintf(buf, sizeof(buf), "%s-%02X%02X", RADAR_AP_PREFIX, mac[4], mac[5]);
  apSsid_ = buf;
  apPass_ = store_->setupPassword();
  if (apPass_.length() < 8) {
    snprintf(buf, sizeof(buf), "%s%04u", RADAR_AP_PASS_PREFIX, static_cast<unsigned>(esp_random() % 10000));
    apPass_ = buf;
  }

  WiFi.persistent(false);
  WiFi.onEvent(onStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.setHostname(RADAR_HOSTNAME);

  store_->loadWifi(savedSsid_, savedPass_);
  if (hasSavedNetwork()) {
    connectSaved();
  } else {
    startPortal();
  }
}

void WifiManager::setState(WifiState s) {
  if (state_ == s) return;
  log_i("wifi state %d -> %d", static_cast<int>(state_), static_cast<int>(s));
  state_ = s;
  stateSince_ = millis();
}

bool WifiManager::online() const { return WiFi.status() == WL_CONNECTED; }

int WifiManager::apClientCount() const { return portalActive() ? WiFi.softAPgetStationNum() : 0; }

String WifiManager::localIp() const { return online() ? WiFi.localIP().toString() : String(); }

void WifiManager::connectSaved() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(savedSsid_.c_str(), savedPass_.c_str());
  lastKickMs_ = millis();
  setState(WifiState::Connecting);
}

void WifiManager::startPortal() {
  if (portalActive()) return;
  lastError_ = "";
  setupError_ = SetupError::None;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  const IPAddress ip(192, 168, 4, 1);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid_.c_str(), apPass_.c_str(), 1, 0, 4);
  WiFi.AP.enableDhcpCaptivePortal();
  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  dns_.start(53, "*", ip);
  setState(WifiState::Portal);
  startScan();
}

void WifiManager::stopAccessPoint() {
  dns_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

void WifiManager::resetAndStartPortal() {
  log_i("forgetting Wi-Fi \"%s\"", savedSsid_.c_str());
  store_->forgetWifi();
  savedSsid_ = savedPass_ = "";
  if (portalActive()) {
    WiFi.disconnect(false, false);
    setupError_ = SetupError::None;
    lastError_ = "";
    setState(WifiState::Portal);
    return;
  }
  startPortal();
}

void WifiManager::retry() {
  if (state_ != WifiState::PortalFailed) return;
  if (setupError_ == SetupError::NoInternet && online()) {
    setState(WifiState::PortalOnline);
    return;
  }
  WiFi.disconnect(false, false);
  setState(WifiState::Portal);
}

void WifiManager::confirmInternet() {
  if (state_ == WifiState::PortalOnline || (state_ == WifiState::PortalFailed && setupError_ == SetupError::NoInternet)) {
    setupError_ = SetupError::None;
    lastError_ = "";
    setState(WifiState::PortalDone);
  }
}

void WifiManager::failInternet() {
  if (state_ != WifiState::PortalOnline) return;
  setupError_ = SetupError::NoInternet;
  lastError_ = errorText(setupError_);
  setState(WifiState::PortalFailed);
}

bool WifiManager::submitCredentials(const String& ssid, const String& pass) {
  if ((state_ != WifiState::Portal && state_ != WifiState::PortalFailed) || ssid.length() == 0 || ssid.length() > 32 ||
      pass.length() > 63 || (pass.length() > 0 && pass.length() < 8)) {
    return false;
  }
  trySsid_ = ssid;
  tryPass_ = pass;
  lastError_ = "";
  setupError_ = SetupError::None;
  g_lastReason = 0;
  WiFi.disconnect(false, false);
  WiFi.setAutoReconnect(false);
  WiFi.begin(trySsid_.c_str(), tryPass_.c_str());
  setState(WifiState::PortalTrying);
  return true;
}

void WifiManager::startScan() { scanRequested_ = true; }

String WifiManager::scanJson() {
  JsonDocument doc;
  const int n = WiFi.scanComplete();
  doc["scanning"] = n == WIFI_SCAN_RUNNING || scanRequested_;
  JsonArray arr = doc["networks"].to<JsonArray>();
  if (n > 0) {
    struct Net {
      String ssid;
      int rssi;
      bool secure;
    };
    std::vector<Net> nets;
    for (int i = 0; i < n; ++i) {
      String s = WiFi.SSID(i);
      if (s.length() == 0) continue;
      auto it = std::find_if(nets.begin(), nets.end(), [&](const Net& x) { return x.ssid == s; });
      const int rssi = WiFi.RSSI(i);
      if (it == nets.end()) {
        nets.push_back({s, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
      } else if (rssi > it->rssi) {
        it->rssi = rssi;
      }
    }
    std::sort(nets.begin(), nets.end(), [](const Net& a, const Net& b) { return a.rssi > b.rssi; });
    for (const Net& x : nets) {
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = x.ssid;
      o["rssi"] = x.rssi;
      o["secure"] = x.secure;
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

void WifiManager::loop() {
  const uint32_t now = millis();
  if (portalActive()) dns_.processNextRequest();

  if (scanRequested_ && WiFi.scanComplete() != WIFI_SCAN_RUNNING &&
      state_ != WifiState::PortalTrying) {
    scanRequested_ = false;
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }

  switch (state_) {
    case WifiState::Connecting:
      if (online()) {
        setState(WifiState::Connected);
      } else if (now - stateSince_ > RADAR_WIFI_CONNECT_TIMEOUT_S * 1000UL) {
        setState(WifiState::Offline);
      }
      break;

    case WifiState::Connected:
    case WifiState::Offline:
      if (online()) {
        setState(WifiState::Connected);
        lastKickMs_ = now;
      } else if (now - lastKickMs_ > kReconnectKickMs) {
        // Auto-reconnect normally handles this; nudge it in case the driver
        // gave up (e.g. the router was off for a long time).
        lastKickMs_ = now;
        WiFi.reconnect();
      }
      break;

    case WifiState::PortalTrying:
      if (online()) {
        savedSsid_ = trySsid_;
        savedPass_ = tryPass_;
        store_->saveWifi(savedSsid_, savedPass_);
        WiFi.setAutoReconnect(true);
        setState(WifiState::PortalOnline);
      } else if (now - stateSince_ > kTryTimeoutMs ||
                 (g_lastReason != 0 && g_lastReason != WIFI_REASON_ASSOC_LEAVE &&
                  now - stateSince_ > 3000)) {
        // Auto-reconnect is off while testing, so any real disconnect reason
        // means this attempt is over.
        setupError_ = reasonError(g_lastReason);
        if (g_lastReason == 0) setupError_ = SetupError::NotFound;
        lastError_ = errorText(setupError_);
        WiFi.disconnect(false, false);
        setState(WifiState::PortalFailed);
      }
      break;

    case WifiState::PortalDone:
      if (now - stateSince_ > kDoneLingerMs) {
        stopAccessPoint();
        WiFi.setAutoReconnect(true);
        setState(WifiState::Connected);
      }
      break;

    default:
      break;
  }
}

}  // namespace net
