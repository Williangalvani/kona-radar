#include "settings_store.h"

#include <esp_random.h>
#include <nvs.h>

#include <string>

#include "config.h"

namespace app {
namespace {

constexpr const char* kNsSettings = "radar";
constexpr const char* kNsWifi = "wifi";
constexpr const char* kNsSetup = "setup";

// Thin nvs_* wrapper. Unlike Arduino's Preferences it treats a missing
// namespace or key as a plain "not there" instead of logging an error.
class Nvs {
 public:
  Nvs(const char* ns, bool write) {
    const esp_err_t err = nvs_open(ns, write ? NVS_READWRITE : NVS_READONLY, &h_);
    ok_ = err == ESP_OK;
    if (!ok_ && err != ESP_ERR_NVS_NOT_FOUND) log_w("nvs_open(%s): %s", ns, esp_err_to_name(err));
  }
  ~Nvs() {
    if (ok_) nvs_close(h_);
  }
  bool ok() const { return ok_; }

  bool get(const char* key, std::string& out) {
    if (!ok_) return false;
    size_t len = 0;
    if (nvs_get_str(h_, key, nullptr, &len) != ESP_OK || len == 0) return false;
    std::string s(len, '\0');
    if (nvs_get_str(h_, key, &s[0], &len) != ESP_OK) return false;
    s.resize(len - 1);
    out = s;
    return true;
  }
  uint8_t getU8(const char* key, uint8_t dflt) {
    uint8_t v = dflt;
    if (ok_) nvs_get_u8(h_, key, &v);
    return v;
  }
  bool set(const char* key, const std::string& v) { return ok_ && check(nvs_set_str(h_, key, v.c_str()), key); }
  bool setU8(const char* key, uint8_t v) { return ok_ && check(nvs_set_u8(h_, key, v), key); }
  void erase(const char* key) {
    if (ok_) nvs_erase_key(h_, key);
  }
  void eraseAll() {
    if (ok_) nvs_erase_all(h_);
  }
  void commit() {
    if (ok_) check(nvs_commit(h_), "commit");
  }

 private:
  static bool check(esp_err_t err, const char* what) {
    if (err != ESP_OK) log_w("nvs %s: %s", what, esp_err_to_name(err));
    return err == ESP_OK;
  }
  nvs_handle_t h_ = 0;
  bool ok_ = false;
};

String randomPassword(const char* prefix, int digits) {
  char buf[40];
  unsigned v = esp_random() % (digits >= 8 ? 100000000u : 10000u);
  snprintf(buf, sizeof(buf), digits >= 8 ? "%s%08u" : "%s%04u", prefix, v);
  return buf;
}

}  // namespace

radar::Settings SettingsStore::load() {
  radar::Settings s = radar::Settings::defaults();
  Nvs nvs(kNsSettings, false);
  std::string json;
  if (nvs.get("cfg", json) && !radar::settingsFromJson(json.c_str(), json.size(), s)) {
    log_w("stored settings unreadable, using defaults");
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
  Nvs nvs(kNsSettings, true);
  if (nvs.set("cfg", json)) nvs.commit();
  log_i("settings saved: %s", json.c_str());
}

void SettingsStore::resetToDefaults() {
  dirty_ = false;
  Nvs nvs(kNsSettings, true);
  nvs.eraseAll();
  nvs.commit();
}

radar::KnownNetworks SettingsStore::loadNetworks() {
  radar::KnownNetworks list;
  std::string json, ssid, pass;
  bool migrate = false;
  {
    Nvs nvs(kNsWifi, false);
    if (nvs.get("nets", json)) {
      if (!list.fromJson(json.c_str(), json.size())) log_w("saved Wi-Fi list unreadable");
    } else if (nvs.get("ssid", ssid)) {
      nvs.get("pass", pass);
      migrate = true;
    }
  }
  if (migrate) {
    if (list.add(ssid, pass)) log_i("wifi: moved saved network \"%s\" into the network list", ssid.c_str());
    Nvs nvs(kNsWifi, true);
    nvs.set("nets", list.toJson());
    nvs.erase("ssid");
    nvs.erase("pass");
    nvs.commit();
  }
  return list;
}

void SettingsStore::saveNetworks(const radar::KnownNetworks& n) {
  Nvs nvs(kNsWifi, true);
  if (nvs.set("nets", n.toJson())) nvs.commit();
}

void SettingsStore::forgetNetworks() {
  Nvs nvs(kNsWifi, true);
  nvs.eraseAll();
  nvs.commit();
}

String SettingsStore::setupPassword() {
  std::string pass;
  Nvs nvs(kNsSetup, true);
  if (nvs.get("appass", pass) && pass.size() >= 8) return pass.c_str();
  const String fresh = randomPassword(RADAR_AP_PASS_PREFIX, 4);
  nvs.set("appass", fresh.c_str());
  nvs.commit();
  return fresh;
}

String SettingsStore::otaPassword() {
  std::string pass;
  Nvs nvs(kNsSetup, true);
  if (nvs.get("otapass", pass) && pass.size() >= 8) return pass.c_str();
  const String fresh = randomPassword("", 8);
  nvs.set("otapass", fresh.c_str());
  nvs.commit();
  return fresh;
}

void SettingsStore::setOtaPassword(const String& pass) {
  Nvs nvs(kNsSetup, true);
  if (nvs.set("otapass", pass.c_str())) nvs.commit();
}

bool SettingsStore::guideSeen() {
  Nvs nvs(kNsSetup, false);
  return nvs.getU8("guide", 0) != 0;
}

void SettingsStore::setGuideSeen(bool seen) {
  Nvs nvs(kNsSetup, true);
  if (nvs.setU8("guide", seen ? 1 : 0)) nvs.commit();
}

}  // namespace app
