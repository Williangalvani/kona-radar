#include "settings_store.h"

#include <Preferences.h>

namespace app {
namespace {
constexpr const char* kNsSettings = "radar";
constexpr const char* kNsWifi = "wifi";
}  // namespace

radar::Settings SettingsStore::load() {
  radar::Settings s = radar::Settings::defaults();
  Preferences p;
  if (p.begin(kNsSettings, true)) {
    String json = p.getString("cfg", "");
    p.end();
    if (json.length() && !radar::settingsFromJson(json.c_str(), json.length(), s)) {
      log_w("stored settings unreadable, using defaults");
    }
  }
  return s;
}

void SettingsStore::save(const radar::Settings& s, uint32_t delayMs) {
  pending_ = s;
  dirty_ = true;
  dueMs_ = millis() + delayMs;
  if (delayMs == 0) writeNow();
}

void SettingsStore::loop() {
  if (dirty_ && static_cast<int32_t>(millis() - dueMs_) >= 0) writeNow();
}

void SettingsStore::writeNow() {
  dirty_ = false;
  const std::string json = radar::settingsDiffToJson(pending_, radar::Settings::defaults());
  Preferences p;
  if (p.begin(kNsSettings, false)) {
    p.putString("cfg", json.c_str());
    p.end();
  }
  log_i("settings saved: %s", json.c_str());
}

void SettingsStore::resetToDefaults() {
  dirty_ = false;
  Preferences p;
  if (p.begin(kNsSettings, false)) {
    p.clear();
    p.end();
  }
}

bool SettingsStore::loadWifi(String& ssid, String& pass) {
  Preferences p;
  if (!p.begin(kNsWifi, true)) return false;
  ssid = p.getString("ssid", "");
  pass = p.getString("pass", "");
  p.end();
  return ssid.length() > 0;
}

void SettingsStore::saveWifi(const String& ssid, const String& pass) {
  Preferences p;
  if (p.begin(kNsWifi, false)) {
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
  }
}

void SettingsStore::forgetWifi() {
  Preferences p;
  if (p.begin(kNsWifi, false)) {
    p.clear();
    p.end();
  }
}

}  // namespace app
