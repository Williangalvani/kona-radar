// When to wake the screen for a landing: once per aircraft, as soon as it is
// predicted to land within the configured number of minutes. While any such
// aircraft is still on its way, pending() keeps the screen from going quiet.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace radar {

struct LandingEta {
  const char* hex;  // ICAO address
  float etaS;       // predicted seconds to landing, < 0 = unknown
};

class LandingWake {
 public:
  static constexpr size_t kMemory = 32;

  // Returns true when an aircraft not seen in the window before is now
  // within `minutes` of landing. `minutes` = 0 turns it off.
  bool update(const std::vector<LandingEta>& etas, uint8_t minutes);

  bool pending() const { return pending_; }
  // The aircraft behind the last wake.
  const std::string& lastHex() const { return lastHex_; }
  void reset();

 private:
  std::vector<std::string> woken_;  // oldest first
  std::string lastHex_;
  bool pending_ = false;
};

}  // namespace radar
