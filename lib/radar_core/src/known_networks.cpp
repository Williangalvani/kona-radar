#include "known_networks.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>

namespace radar {

bool KnownNetworks::validSsid(const std::string& ssid) { return !ssid.empty() && ssid.size() <= 32; }

bool KnownNetworks::validPassword(const std::string& pass) {
  if (pass.empty()) return true;
  if (pass.size() == 64)
    return std::all_of(pass.begin(), pass.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
  return pass.size() >= 8 && pass.size() <= 63;
}

bool KnownNetworks::add(const std::string& ssid, const std::string& pass) {
  if (!validSsid(ssid) || !validPassword(pass)) return false;
  remove(ssid);
  list_.insert(list_.begin(), KnownNetwork{ssid, pass});
  if (list_.size() > kMax) list_.resize(kMax);
  return true;
}

bool KnownNetworks::remove(const std::string& ssid) {
  for (auto it = list_.begin(); it != list_.end(); ++it) {
    if (it->ssid == ssid) {
      list_.erase(it);
      return true;
    }
  }
  return false;
}

bool KnownNetworks::markUsed(const std::string& ssid) {
  for (size_t i = 0; i < list_.size(); ++i) {
    if (list_[i].ssid != ssid) continue;
    if (i == 0) return false;
    std::rotate(list_.begin(), list_.begin() + static_cast<long>(i), list_.begin() + static_cast<long>(i) + 1);
    return true;
  }
  return false;
}

const KnownNetwork* KnownNetworks::find(const std::string& ssid) const {
  for (const KnownNetwork& n : list_)
    if (n.ssid == ssid) return &n;
  return nullptr;
}

std::vector<size_t> KnownNetworks::joinOrder(const std::vector<ScanResult>& scan, bool includeUnseen) const {
  struct Seen {
    size_t idx;
    int rssi;
  };
  std::vector<Seen> seen;
  std::vector<size_t> unseen;
  for (size_t i = 0; i < list_.size(); ++i) {
    int best = -1000;
    bool found = false;
    for (const ScanResult& r : scan) {
      if (r.ssid == list_[i].ssid && r.rssi > best) {
        best = r.rssi;
        found = true;
      }
    }
    if (found)
      seen.push_back({i, best});
    else
      unseen.push_back(i);
  }
  std::stable_sort(seen.begin(), seen.end(), [](const Seen& a, const Seen& b) { return a.rssi > b.rssi; });
  std::vector<size_t> out;
  for (const Seen& s : seen) out.push_back(s.idx);
  if (includeUnseen) out.insert(out.end(), unseen.begin(), unseen.end());
  return out;
}

std::string KnownNetworks::toJson() const {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (const KnownNetwork& n : list_) {
    JsonObject o = a.add<JsonObject>();
    o["s"] = n.ssid;
    o["p"] = n.pass;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool KnownNetworks::fromJson(const char* json, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc.is<JsonArrayConst>()) return false;
  std::vector<KnownNetwork> next;
  for (JsonVariantConst v : doc.as<JsonArrayConst>()) {
    if (next.size() >= kMax) break;
    const char* s = v["s"] | "";
    const char* p = v["p"] | "";
    KnownNetwork n{s, p};
    if (!validSsid(n.ssid) || !validPassword(n.pass)) continue;
    if (std::any_of(next.begin(), next.end(), [&](const KnownNetwork& x) { return x.ssid == n.ssid; })) continue;
    next.push_back(n);
  }
  list_ = next;
  return true;
}

std::string KnownNetworks::toPublicJson() const {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (const KnownNetwork& n : list_) {
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["open"] = n.pass.empty();
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace radar
