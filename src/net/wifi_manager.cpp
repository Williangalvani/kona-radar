#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>

#include <algorithm>

#include "app/settings_store.h"
#include "config.h"

namespace net {
namespace {

constexpr uint32_t kTryTimeoutMs = 20000;      // portal attempt
constexpr uint32_t kJoinTimeoutMs = 12000;     // one saved network
constexpr uint32_t kReasonGraceMs = 3000;      // ignore early disconnect reasons
constexpr uint32_t kDoneLingerMs = 10000;
constexpr uint32_t kLostGraceMs = 8000;        // let the driver reconnect first
constexpr uint32_t kRetryMinMs = 10000;
constexpr uint32_t kRetryMaxMs = 60000;
// Setup opened by accident (saved networks exist, no phone joins) closes again.
constexpr uint32_t kIdlePortalMs = 5 * 60 * 1000;

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

bool realDisconnect(uint8_t reason) { return reason != 0 && reason != WIFI_REASON_ASSOC_LEAVE; }

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

  networks_ = store_->loadNetworks();
  log_i("wifi: %u saved network(s)", static_cast<unsigned>(networks_.size()));
  if (hasSavedNetwork()) {
    setState(WifiState::Connecting);
    connectKnown();
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

String WifiManager::currentSsid() const {
  if (online()) return WiFi.SSID();
  if (trySsid_.length()) return trySsid_;
  return networks_.empty() ? String() : String(networks_.list()[0].ssid.c_str());
}

// The driver logs ESP_ERR_WIFI_NOT_INIT if STA is not running yet.
void WifiManager::staDisconnect() {
  if (WiFi.getMode() & WIFI_MODE_STA) WiFi.disconnect(false, false);
}

void WifiManager::saveNetworks() {
  if (store_) store_->saveNetworks(networks_);
}

// ---- background joining ---------------------------------------------------

void WifiManager::connectKnown() {
  if (networks_.empty()) {
    link_ = Link::Idle;
    return;
  }
  if (WiFi.getMode() != WIFI_MODE_STA) WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  link_ = Link::Scanning;
  linkSince_ = millis();
  scanGenWanted_ = scanGen_ + 1;
  scanRequested_ = true;
}

void WifiManager::tryNext(uint32_t now) {
  while (attempt_ < order_.size()) {
    const size_t idx = order_[attempt_++];
    if (idx >= networks_.size()) continue;
    const radar::KnownNetwork& n = networks_.list()[idx];
    trySsid_ = n.ssid.c_str();
    tryPass_ = n.pass.c_str();
    log_i("wifi: trying \"%s\"", n.ssid.c_str());
    staDisconnect();
    g_lastReason = 0;
    WiFi.begin(n.ssid.c_str(), n.pass.c_str());
    link_ = Link::Trying;
    linkSince_ = now;
    return;
  }
  // Every candidate failed: wait, then scan again (backing off to a minute).
  waitMs_ = waitMs_ ? std::min(kRetryMaxMs, waitMs_ * 2) : kRetryMinMs;
  log_i("wifi: no saved network joined; scanning again in %lu s", static_cast<unsigned long>(waitMs_ / 1000));
  staDisconnect();
  link_ = Link::Waiting;
  linkSince_ = now;
}

void WifiManager::linkLoop(uint32_t now) {
  switch (link_) {
    case Link::Scanning:
      if (scanGen_ >= scanGenWanted_ && !scanRunning_) {
        std::vector<radar::ScanResult> seen;
        for (const SeenNet& s : lastScan_) seen.push_back({s.ssid.c_str(), s.rssi});
        order_ = networks_.joinOrder(seen);
        attempt_ = 0;
        tryNext(now);
      }
      break;

    case Link::Trying:
      if (online()) {
        connectedSsid_ = trySsid_;
        log_i("wifi: joined \"%s\" (%d dBm), %s", trySsid_.c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
        if (networks_.markUsed(trySsid_.c_str())) saveNetworks();
        WiFi.setAutoReconnect(true);
        link_ = Link::Up;
        lostSinceMs_ = 0;
        waitMs_ = 0;
      } else if (now - linkSince_ > kJoinTimeoutMs ||
                 (realDisconnect(g_lastReason) && now - linkSince_ > kReasonGraceMs)) {
        log_i("wifi: \"%s\" failed (reason %u)", trySsid_.c_str(), static_cast<unsigned>(g_lastReason));
        tryNext(now);
      }
      break;

    case Link::Up:
      if (online()) {
        lostSinceMs_ = 0;
      } else if (lostSinceMs_ == 0) {
        lostSinceMs_ = now;
      } else if (now - lostSinceMs_ > kLostGraceMs) {
        log_i("wifi: lost \"%s\"; looking for saved networks", connectedSsid_.c_str());
        connectKnown();
      }
      break;

    case Link::Waiting:
      if (now - linkSince_ > waitMs_) connectKnown();
      break;

    case Link::Idle:
    default:
      break;
  }
}

// ---- scanning ---------------------------------------------------------------

void WifiManager::startScan() { scanRequested_ = true; }

void WifiManager::scanLoop(uint32_t now) {
  (void)now;
  if (scanRunning_) {
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    lastScan_.clear();
    for (int i = 0; i < n; ++i) {
      const String s = WiFi.SSID(i);
      if (s.length() == 0) continue;
      const int rssi = WiFi.RSSI(i);
      auto it = std::find_if(lastScan_.begin(), lastScan_.end(), [&](const SeenNet& x) { return x.ssid == s; });
      if (it == lastScan_.end())
        lastScan_.push_back({s, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
      else if (rssi > it->rssi)
        it->rssi = rssi;
    }
    std::sort(lastScan_.begin(), lastScan_.end(), [](const SeenNet& a, const SeenNet& b) { return a.rssi > b.rssi; });
    WiFi.scanDelete();
    scanRunning_ = false;
    ++scanGen_;
    return;
  }
  // Scanning while associating breaks the attempt.
  if (!scanRequested_ || link_ == Link::Trying || state_ == WifiState::PortalTrying) return;
  scanRequested_ = false;
  if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
  const int r = WiFi.scanNetworks(true);
  if (r == WIFI_SCAN_RUNNING) {
    scanRunning_ = true;
  } else {
    // Failed to start (or finished synchronously): report an empty scan.
    lastScan_.clear();
    WiFi.scanDelete();
    ++scanGen_;
  }
}

String WifiManager::scanJson() {
  JsonDocument doc;
  doc["scanning"] = scanRunning_ || scanRequested_;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (const SeenNet& x : lastScan_) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = x.ssid;
    o["rssi"] = x.rssi;
    o["secure"] = x.secure;
    o["saved"] = networks_.find(x.ssid.c_str()) != nullptr;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

// ---- saved networks (settings page) ----------------------------------------

String WifiManager::networksJson() const {
  JsonDocument doc;
  doc["max"] = radar::KnownNetworks::kMax;
  const String cur = online() ? WiFi.SSID() : String();
  doc["current"] = cur;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (const radar::KnownNetwork& n : networks_.list()) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["open"] = n.pass.empty();
    o["current"] = cur == n.ssid.c_str();
    auto it = std::find_if(lastScan_.begin(), lastScan_.end(), [&](const SeenNet& x) { return x.ssid == n.ssid.c_str(); });
    if (it != lastScan_.end())
      o["rssi"] = it->rssi;
    else
      o["rssi"] = nullptr;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

bool WifiManager::addNetwork(const String& ssid, const String& pass, String& error) {
  if (!radar::KnownNetworks::validSsid(ssid.c_str())) {
    error = "The network name must be 1 to 32 characters.";
    return false;
  }
  if (!radar::KnownNetworks::validPassword(pass.c_str())) {
    error = "Wi-Fi passwords are 8 to 63 characters (or empty for an open network).";
    return false;
  }
  const bool full = networks_.size() >= radar::KnownNetworks::kMax && !networks_.find(ssid.c_str());
  networks_.add(ssid.c_str(), pass.c_str());
  // The one in use stays first so a reboot rejoins it quickly.
  if (online()) networks_.markUsed(WiFi.SSID().c_str());
  saveNetworks();
  if (full) log_i("wifi: list full; dropped the least recently used network");
  log_i("wifi: saved \"%s\" (%u networks)", ssid.c_str(), static_cast<unsigned>(networks_.size()));
  if (!portalActive() && (link_ == Link::Waiting || link_ == Link::Idle)) connectKnown();
  return true;
}

bool WifiManager::removeNetwork(const String& ssid) {
  if (!networks_.remove(ssid.c_str())) return false;
  saveNetworks();
  log_i("wifi: removed \"%s\"", ssid.c_str());
  return true;
}

// ---- setup portal -------------------------------------------------------------

void WifiManager::startPortal() {
  if (!store_ || portalActive()) return;
  lastError_ = "";
  setupError_ = SetupError::None;
  link_ = Link::Idle;
  WiFi.setAutoReconnect(false);
  staDisconnect();
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
  if (!store_) return;
  log_i("wifi: forgetting all %u saved network(s)", static_cast<unsigned>(networks_.size()));
  store_->forgetNetworks();
  networks_.clear();
  connectedSsid_ = trySsid_ = tryPass_ = "";
  if (portalActive()) {
    staDisconnect();
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
  staDisconnect();
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
  if ((state_ != WifiState::Portal && state_ != WifiState::PortalFailed) ||
      !radar::KnownNetworks::validSsid(ssid.c_str()) || !radar::KnownNetworks::validPassword(pass.c_str())) {
    return false;
  }
  trySsid_ = ssid;
  tryPass_ = pass;
  lastError_ = "";
  setupError_ = SetupError::None;
  g_lastReason = 0;
  staDisconnect();
  WiFi.setAutoReconnect(false);
  WiFi.begin(trySsid_.c_str(), tryPass_.c_str());
  setState(WifiState::PortalTrying);
  return true;
}

void WifiManager::loop() {
  if (!store_) return;
  const uint32_t now = millis();
  if (portalActive()) dns_.processNextRequest();
  scanLoop(now);
  if (!portalActive()) linkLoop(now);

  switch (state_) {
    case WifiState::Connecting:
      if (online()) {
        setState(WifiState::Connected);
      } else if (now - stateSince_ > RADAR_WIFI_CONNECT_TIMEOUT_S * 1000UL) {
        setState(WifiState::Offline);
      }
      break;

    case WifiState::Offline:
      if (online()) setState(WifiState::Connected);
      break;

    case WifiState::Portal:
      if (apClientCount() > 0) portalUsedMs_ = now;
      if (hasSavedNetwork() && apClientCount() == 0 && now - std::max(stateSince_, portalUsedMs_) > kIdlePortalMs) {
        log_i("wifi: setup unused; back to the saved networks");
        stopAccessPoint();
        setState(WifiState::Connecting);
        connectKnown();
      }
      break;

    case WifiState::PortalTrying:
      if (online()) {
        // Added to the list (not replacing it), most recent first.
        networks_.add(trySsid_.c_str(), tryPass_.c_str());
        saveNetworks();
        connectedSsid_ = trySsid_;
        WiFi.setAutoReconnect(true);
        setState(WifiState::PortalOnline);
      } else if (now - stateSince_ > kTryTimeoutMs ||
                 (realDisconnect(g_lastReason) && now - stateSince_ > kReasonGraceMs)) {
        // Auto-reconnect is off while testing, so any real disconnect reason
        // means this attempt is over.
        setupError_ = reasonError(g_lastReason);
        if (g_lastReason == 0) setupError_ = SetupError::NotFound;
        lastError_ = errorText(setupError_);
        staDisconnect();
        setState(WifiState::PortalFailed);
      }
      break;

    case WifiState::PortalDone:
      if (now - stateSince_ > kDoneLingerMs) {
        stopAccessPoint();
        WiFi.setAutoReconnect(true);
        link_ = Link::Up;
        lostSinceMs_ = 0;
        setState(WifiState::Connected);
      }
      break;

    default:
      break;
  }
}

}  // namespace net
