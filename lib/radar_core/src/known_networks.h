// The Wi-Fi networks the radar remembers, most recently used first, and the
// order to try them in. Stored in NVS as JSON; passwords never leave the
// device (toPublicJson() lists names only).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace radar {

struct KnownNetwork {
  std::string ssid;
  std::string pass;  // empty = open network
};

struct ScanResult {
  std::string ssid;
  int rssi;  // dBm
};

class KnownNetworks {
 public:
  static constexpr size_t kMax = 8;

  // A valid SSID is 1-32 bytes; a password is empty (open network), 8-63
  // characters, or 64 hex digits (a raw WPA key).
  static bool validSsid(const std::string& ssid);
  static bool validPassword(const std::string& pass);

  // Adds the network, or updates its password, and makes it the most recent.
  // When the list is full the least recently used network is dropped.
  // Returns false (and changes nothing) if the SSID or password is invalid.
  bool add(const std::string& ssid, const std::string& pass);
  bool remove(const std::string& ssid);
  // Moves a saved network to the front (after a successful connection).
  // Returns true if the order changed.
  bool markUsed(const std::string& ssid);
  void clear() { list_.clear(); }

  const KnownNetwork* find(const std::string& ssid) const;
  const std::vector<KnownNetwork>& list() const { return list_; }
  size_t size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }

  // Indexes into list() in the order to try: networks seen in `scan`
  // strongest first (ties: most recent first), then, if `includeUnseen`,
  // the rest in recency order (hidden networks never show up in a scan).
  std::vector<size_t> joinOrder(const std::vector<ScanResult>& scan, bool includeUnseen = true) const;

  // NVS form: [{"s":"ssid","p":"pass"}, ...], most recent first.
  std::string toJson() const;
  // Replaces the list. Invalid entries and duplicates are skipped; at most
  // kMax are kept. Returns false (list unchanged) on malformed JSON.
  bool fromJson(const char* json, size_t len);
  // For the settings page: [{"ssid":..,"open":bool}, ...] - no passwords.
  std::string toPublicJson() const;

 private:
  std::vector<KnownNetwork> list_;
};

}  // namespace radar
