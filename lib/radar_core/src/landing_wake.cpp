#include "landing_wake.h"

#include <algorithm>

namespace radar {

bool LandingWake::update(const std::vector<LandingEta>& etas, uint8_t minutes) {
  pending_ = false;
  if (minutes == 0) return false;
  const float window = minutes * 60.0f;
  bool wake = false;
  for (const LandingEta& e : etas) {
    if (!e.hex || !*e.hex || e.etaS < 0 || e.etaS > window) continue;
    pending_ = true;
    if (std::find(woken_.begin(), woken_.end(), e.hex) != woken_.end()) continue;
    woken_.emplace_back(e.hex);
    if (woken_.size() > kMemory) woken_.erase(woken_.begin());
    lastHex_ = e.hex;
    wake = true;
  }
  return wake;
}

void LandingWake::reset() {
  woken_.clear();
  lastHex_.clear();
  pending_ = false;
}

}  // namespace radar
