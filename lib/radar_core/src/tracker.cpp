#include "tracker.h"

#include <algorithm>
#include <cstring>

namespace radar {

const Track* Tracker::find(const char* hex) const {
  if (!hex || !*hex) return nullptr;
  for (const Track& t : tracks_)
    if (std::strcmp(t.ac.hex, hex) == 0) return &t;
  return nullptr;
}

void Tracker::update(const std::vector<Aircraft>& reports, uint32_t nowMs) {
  for (const Aircraft& a : reports) {
    if (!a.hex[0]) continue;
    const uint32_t seenMs = static_cast<uint32_t>(std::max(0.0f, a.seenPosS) * 1000.0f);
    const uint32_t posTime = nowMs - std::min(seenMs, nowMs);

    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                           [&](const Track& t) { return std::strcmp(t.ac.hex, a.hex) == 0; });
    if (it == tracks_.end()) {
      Track t;
      t.ac = a;
      t.posTimeMs = posTime;
      t.lastSeenMs = nowMs;
      t.trailTimeMs = posTime;
      tracks_.push_back(t);
      continue;
    }

    Track& t = *it;
    // Ignore a report whose position is older than the one we already have.
    if (static_cast<int32_t>(posTime - t.posTimeMs) < 0) {
      t.lastSeenMs = nowMs;
      continue;
    }
    if (posTime - t.trailTimeMs >= kTrailSpacingMs &&
        (t.ac.lat != a.lat || t.ac.lon != a.lon)) {
      std::memmove(&t.trail[1], &t.trail[0], sizeof(LatLon) * (Track::kTrailLen - 1));
      t.trail[0] = {t.ac.lat, t.ac.lon};
      if (t.trailCount < Track::kTrailLen) ++t.trailCount;
      t.trailTimeMs = posTime;
    }
    t.ac = a;
    t.posTimeMs = posTime;
    t.lastSeenMs = nowMs;
  }
}

void Tracker::expire(uint32_t nowMs, uint32_t staleMs) {
  tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                               [&](const Track& t) { return nowMs - t.posTimeMs > staleMs; }),
                tracks_.end());
}

LatLon Tracker::predict(const Track& t, uint32_t nowMs) {
  const LatLon reported{t.ac.lat, t.ac.lon};
  if (t.ac.onGround || !t.ac.hasGs || !t.ac.hasTrack || t.ac.gsKt <= 0) return reported;
  int32_t dt = static_cast<int32_t>(nowMs - t.posTimeMs);
  if (dt <= 0) return reported;
  if (dt > static_cast<int32_t>(kMaxExtrapolationMs)) dt = kMaxExtrapolationMs;
  const double distNm = t.ac.gsKt * (dt / 3600000.0);
  return destination(reported, t.ac.trackDeg, distNm);
}

}  // namespace radar
