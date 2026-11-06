// Turns tracked aircraft into what the screen needs: where each one is from
// the observer, which way to look, and whether it is likely landing at or
// departing from the reference airport. The arrival / departure cues are
// heuristics built only from the aircraft's own position, track, altitude
// and vertical rate relative to the runway geometry - the free feeds do not
// carry flight plans.
#pragma once

#include <cstdint>
#include <vector>

#include "geo.h"
#include "places.h"
#include "settings.h"
#include "tracker.h"

namespace radar {

enum class FlightPhase : uint8_t { Cruise, Arrival, Departure, Helicopter, Ground };

struct FlightCue {
  FlightPhase phase = FlightPhase::Cruise;
  const char* runway = nullptr;  // landing / departure runway ident
  LatLon threshold{0, 0};        // landing threshold (arrivals)
  float etaS = -1;               // seconds to the threshold (arrivals), else -1
};

struct Target {
  const Track* track = nullptr;
  LatLon pos{0, 0};        // estimated current position
  double distNm = 0;       // ground distance from the observer
  double bearingDeg = 0;   // true bearing from the observer
  double relDeg = 0;       // bearing relative to the facing direction (-180, 180], NAN if unknown
  double elevationDeg = 0; // look-up angle from the observer
  float ageS = 0;          // age of the last reported position
  bool stale = false;      // older than RADAR_STALE_MUTE_S: drawn muted
  bool inRange = false;    // inside the outer ring
  FlightCue cue;
};

struct TrafficSummary {
  int inRange = 0;          // airborne targets inside the outer ring
  int outside = 0;          // airborne targets beyond it (up to twice the range)
  int nextArrival = -1;     // index of the arrival with the smallest ETA
  float nextArrivalEtaS = -1;
  float freshestAgeS = -1;  // age of the newest position, -1 with no traffic
};

bool isHelicopter(const Aircraft& a);

// Classifies one aircraft at `pos` (its current estimated position) whose
// last position is `ageS` seconds old, relative to `airport`'s runways.
FlightCue classifyFlight(const Track& t, const LatLon& pos, float ageS, const Place* airport);

// Builds targets for every tracked aircraft. `out` is cleared first.
void analyzeTraffic(const Tracker& tracker, const Settings& s, uint32_t nowMs,
                    std::vector<Target>& out, TrafficSummary& summary);

// Signed angle in (-180, 180].
double wrap180(double deg);

}  // namespace radar
