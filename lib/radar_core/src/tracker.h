// Keeps aircraft between downloads: merges successive reports by ICAO
// address, dead-reckons positions between them, keeps a short position
// history and drops aircraft that have gone quiet.
#pragma once

#include <cstdint>
#include <vector>

#include "aircraft.h"
#include "geo.h"

namespace radar {

struct Track {
  static constexpr int kTrailLen = 6;

  Aircraft ac;               // latest report
  uint32_t posTimeMs = 0;    // local clock time the reported position was valid
  uint32_t lastSeenMs = 0;   // local clock time of the latest report
  LatLon trail[kTrailLen];   // older reported positions, newest first
  uint8_t trailCount = 0;
  uint32_t trailTimeMs = 0;  // when trail[0] was recorded
};

class Tracker {
 public:
  // Minimum spacing between history points.
  static constexpr uint32_t kTrailSpacingMs = 10000;
  // Dead reckoning stops after this long without a fresh position.
  static constexpr uint32_t kMaxExtrapolationMs = 30000;

  // Merges a fresh download received at local time `nowMs`.
  void update(const std::vector<Aircraft>& reports, uint32_t nowMs);

  // Removes tracks whose last known position is older than `staleMs`.
  void expire(uint32_t nowMs, uint32_t staleMs);

  // Best estimate of where the aircraft is at `nowMs`.
  static LatLon predict(const Track& t, uint32_t nowMs);

  const std::vector<Track>& tracks() const { return tracks_; }
  const Track* find(const char* hex) const;
  void clear() { tracks_.clear(); }

 private:
  std::vector<Track> tracks_;
};

}  // namespace radar
