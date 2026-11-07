#include "web_ui.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "web_pages.h"
#include "wifi_manager.h"

namespace net {
namespace {

// Connectivity-check URLs used by Android, iOS/macOS, Windows and Firefox.
const char* const kProbePaths[] = {
    "/generate_204",  "/gen_204",       "/hotspot-detect.html", "/library/test/success.html",
    "/connecttest.txt", "/ncsi.txt",    "/redirect",            "/canonical.html",
    "/success.txt",   "/chat",          "/fwlink",
};

String rangesJson() {
  size_t n;
  const uint16_t* r = radar::rangeSteps(n);
  String s = "[";
  for (size_t i = 0; i < n; ++i) {
    if (i) s += ',';
    s += r[i];
  }
  return s + "]";
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 2);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<uint8_t>(c) < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", c);
      out += b;
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace

void WebUi::begin(WifiManager* wifi, Hooks hooks) {
  wifi_ = wifi;
  hooks_ = std::move(hooks);

  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  for (const char* p : kProbePaths) {
    server_.on(p, [this] {
      if (!redirectToPortal()) handleNotFound();
    });
  }
  server_.on("/api/scan", HTTP_GET, [this] { handleScan(); });
  server_.on("/api/scan", HTTP_POST, [this] {
    wifi_->startScan();
    sendJson(200, "{\"ok\":true}");
  });
  server_.on("/api/wifi/status", HTTP_GET, [this] { handleWifiStatus(); });
  server_.on("/api/wifi/connect", HTTP_POST, [this] { handleWifiConnect(); });
  server_.on("/api/settings", HTTP_GET, [this] { handleGetSettings(); });
  server_.on("/api/settings", HTTP_POST, [this] { handlePostSettings(); });
  server_.on("/api/reset", HTTP_POST, [this] { handleReset(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/networks", HTTP_GET, [this] { sendJson(200, wifi_->networksJson()); });
  server_.on("/api/networks", HTTP_POST, [this] { handleNetworksAdd(); });
  server_.on("/api/networks/remove", HTTP_POST, [this] { handleNetworksRemove(); });
  server_.onNotFound([this] { handleNotFound(); });
  server_.begin();
}

void WebUi::loop() {
  if (!wifi_) return;
  server_.handleClient();
  if (!mdnsStarted_ && wifi_->online()) {
    if (MDNS.begin(RADAR_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted_ = true;
      log_i("mDNS: http://%s.local/", RADAR_HOSTNAME);
    }
  }
}

bool WebUi::redirectToPortal() {
  if (!wifi_->portalActive()) return false;
  server_.sendHeader("Location", String("http://") + WifiManager::kPortalIp + "/", true);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(302, "text/plain", "");
  return true;
}

void WebUi::sendJson(int code, const String& body) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", body);
}

void WebUi::handleRoot() {
  server_.sendHeader("Cache-Control", "no-store");
  if (wifi_->portalActive()) {
    // A phone that joined the setup network may ask for any host name.
    if (server_.hostHeader() != WifiManager::kPortalIp && server_.hostHeader().length() > 0 &&
        !server_.hostHeader().startsWith("192.168.4.1")) {
      redirectToPortal();
      return;
    }
    server_.send_P(200, "text/html", kPortalPage);
    return;
  }
  String page = FPSTR(kSettingsPage);
  page.replace("%RANGES%", rangesJson());
  server_.send(200, "text/html", page);
}

void WebUi::handleWifiStatus() {
  const char* state = "connected";
  switch (wifi_->state()) {
    case WifiState::Portal:
      state = "portal";
      break;
    case WifiState::PortalTrying:
      state = "trying";
      break;
    case WifiState::PortalOnline:
      state = "online";
      break;
    case WifiState::PortalDone:
      state = "done";
      break;
    case WifiState::PortalFailed:
      state = "failed";
      break;
    case WifiState::Connecting:
    case WifiState::Offline:
      state = "connecting";
      break;
    default:
      break;
  }
  String body = String("{\"state\":\"") + state + "\",\"ssid\":\"" +
                jsonEscape(wifi_->state() == WifiState::PortalTrying ? wifi_->tryingSsid() : wifi_->currentSsid()) +
                "\",\"error\":\"" + jsonEscape(wifi_->lastError()) + "\",\"host\":\"" RADAR_HOSTNAME ".local\"}";
  sendJson(200, body);
}

void WebUi::handleWifiConnect() {
  if (!wifi_->portalActive()) {
    sendJson(409, "{\"ok\":false,\"error\":\"Setup mode is not active.\"}");
    return;
  }
  const String ssid = server_.arg("ssid");
  const String pass = server_.arg("pass");
  if (!wifi_->submitCredentials(ssid, pass)) {
    sendJson(400, "{\"ok\":false,\"error\":\"Check the network name and password (8 to 63 characters).\"}");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void WebUi::handleScan() { sendJson(200, wifi_->scanJson()); }

void WebUi::handleGetSettings() {
  sendJson(200, String(radar::settingsToJson(hooks_.getSettings()).c_str()));
}

void WebUi::handlePostSettings() {
  const String body = server_.arg("plain");
  radar::Settings s = hooks_.getSettings();
  std::string err;
  if (!radar::settingsFromJson(body.c_str(), body.length(), s, &err)) {
    sendJson(400, String("{\"error\":\"") + jsonEscape(String(err.c_str())) + "\"}");
    return;
  }
  hooks_.applySettings(s);
  handleGetSettings();
}

void WebUi::handleReset() {
  hooks_.resetSettings();
  handleGetSettings();
}

void WebUi::handleStatus() { sendJson(200, hooks_.statusJson()); }

void WebUi::handleNetworksAdd() {
  String err;
  if (!wifi_->addNetwork(server_.arg("ssid"), server_.arg("pass"), err)) {
    sendJson(400, String("{\"error\":\"") + jsonEscape(err) + "\"}");
    return;
  }
  sendJson(200, wifi_->networksJson());
}

void WebUi::handleNetworksRemove() {
  if (!wifi_->removeNetwork(server_.arg("ssid"))) {
    sendJson(404, "{\"error\":\"That network is not saved.\"}");
    return;
  }
  sendJson(200, wifi_->networksJson());
}

void WebUi::handleNotFound() {
  if (redirectToPortal()) return;
  server_.send(404, "text/plain", "Not found");
}

}  // namespace net
