#include "traffic.h"

#include <cmath>
#include <cstring>

#include "config.h"

namespace radar {
namespace {

const char* const kHelicopterTypes[] = {"R22",  "R44",  "R66",  "AS50", "AS55", "AS65", "EC20",
                                        "EC30", "EC35", "EC45", "B06",  "B407", "B429", "B505",
                                        "S76",  "A109", "A119", "H500", "H60",  "UH1"};

// Position of `p` in a runway-aligned frame with its origin at `origin`:
// `along` is positive in direction `dirDeg`, `cross` positive to the right.
void runwayFrame(const LatLon& origin, double dirDeg, const LatLon& p, double& along,
                 double& cross) {
  const double d = distanceNm(origin, p);
  if (d < 1e-6) {
    along = cross = 0;
    return;
  }
  const double a = deg2rad(bearingDeg(origin, p) - dirDeg);
  along = d * std::cos(a);
  cross = d * std::sin(a);
}

}  // namespace

double wrap180(double deg) {
  double w = wrap360(deg);
  if (w > 180) w -= 360;
  return w;
}

bool isHelicopter(const Aircraft& a) {
  if (std::strcmp(a.category, "A7") == 0) return true;
  for (const char* t : kHelicopterTypes)
    if (std::strcmp(a.type, t) == 0) return true;
  return false;
}

FlightCue classifyFlight(const Track& t, const LatLon& pos, float ageS, const Place* airport) {
  FlightCue cue;
  const Aircraft& a = t.ac;
  if (a.onGround) {
    cue.phase = FlightPhase::Ground;
    return cue;
  }
  if (isHelicopter(a)) {
    cue.phase = FlightPhase::Helicopter;
    return cue;
  }
  if (!airport || !a.hasTrack || !a.hasGs || !a.hasAlt || a.gsKt < 50) return cue;

  const double vr = a.hasVertRate ? a.vertRateFpm : 0;
  double bestCross = 1e9;
  for (size_t r = 0; r < airport->runwayCount; ++r) {
    const Runway& rw = airport->runways[r];
    for (int end = 0; end < 2; ++end) {
      const LatLon& thr = rw.threshold[end];
      const LatLon& far = rw.threshold[1 - end];
      const double dir = bearingDeg(thr, far);  // landing / takeoff direction of this end
      if (std::fabs(wrap180(a.trackDeg - dir)) > 25) continue;

      double along, cross;
      // Arrival: before the threshold, near the extended centerline, below a
      // generous glide path, not climbing, getting closer, position fresh.
      runwayFrame(thr, dir, pos, along, cross);
      const double out = -along;
      bool arrival = ageS <= RADAR_STALE_NO_ETA_S && out >= -0.2 && out <= 15 &&
                     std::fabs(cross) <= 0.5 + 0.12 * std::fabs(out) &&
                     a.altFt <= 1500 + 350 * std::fabs(out) && vr <= 200 &&
                     (vr <= -200 || a.altFt <= 800 + 320 * std::fabs(out)) && t.trailCount > 0;
      if (arrival) {
        double prevAlong, prevCross;
        runwayFrame(thr, dir, t.trail[0], prevAlong, prevCross);
        arrival = prevAlong < along;
      }
      if (arrival && std::fabs(cross) < bestCross) {
        bestCross = std::fabs(cross);
        cue.phase = FlightPhase::Arrival;
        cue.runway = rw.ident[end];
        cue.threshold = thr;
        cue.etaS = static_cast<float>(out > 0 ? out / a.gsKt * 3600.0 : 0.0);
        continue;
      }
      if (cue.phase == FlightPhase::Arrival) continue;

      // Departure: climbing out past the far end of the runway it used.
      if (!a.hasVertRate || vr < 300 || a.altFt > 8000) continue;
      runwayFrame(far, dir, pos, along, cross);
      if (along >= -0.3 && along <= 12 && std::fabs(cross) <= 0.8 + 0.25 * std::fabs(along)) {
        cue.phase = FlightPhase::Departure;
        cue.runway = rw.ident[end];
      }
    }
  }
  return cue;
}

float inboundEtaS(const Track& t, const LatLon& pos, float ageS, const Place* airport) {
  const Aircraft& a = t.ac;
  if (!airport || a.onGround || isHelicopter(a) || !a.hasTrack || !a.hasGs || !a.hasAlt || a.gsKt < 80 ||
      ageS > RADAR_STALE_NO_ETA_S)
    return -1;
  const double d = distanceNm(pos, airport->pos);
  if (d < 2 || d > 120) return -1;
  if (std::fabs(wrap180(a.trackDeg - bearingDeg(pos, airport->pos))) > 60) return -1;
  const double vr = a.hasVertRate ? a.vertRateFpm : 0;
  // Not climbing, low enough to get down in time, and either descending or
  // already at pattern-ish height for the distance.
  if (vr > 100 || a.altFt > 1500 + 400 * d) return -1;
  if (vr > -300 && a.altFt > 2500 + 150 * d) return -1;
  // Path a bit longer than the straight line (joining the approach), flown a
  // bit slower than now (slowing down for final).
  return static_cast<float>((1.15 * d + 3.0) / (0.85 * a.gsKt) * 3600.0);
}

void analyzeTraffic(const Tracker& tracker, const Settings& s, uint32_t nowMs,
                    std::vector<Target>& out, TrafficSummary& summary) {
  out.clear();
  summary = TrafficSummary{};
  const Place* airport = airportNear(s.center());
  const LatLon obs = s.observer();
  const double facing = s.facingBearing();

  for (const Track& t : tracker.tracks()) {
    Target g;
    g.track = &t;
    g.pos = Tracker::predict(t, nowMs);
    g.ageS = static_cast<float>(static_cast<int32_t>(nowMs - t.posTimeMs)) / 1000.0f;
    if (g.ageS < 0) g.ageS = 0;
    if (g.ageS > RADAR_STALE_AFTER_S) continue;
    g.stale = g.ageS > RADAR_STALE_MUTE_S;

    const double altFt = t.ac.hasAlt && !t.ac.onGround ? t.ac.altFt : s.observerAltFt;
    const LookAngles look = lookAngles(obs, s.observerAltFt, g.pos, altFt);
    g.distNm = look.groundDistNm;
    g.bearingDeg = look.bearingDeg;
    g.elevationDeg = look.elevationDeg;
    g.relDeg = std::isnan(facing) ? NAN : wrap180(look.bearingDeg - facing);
    g.inRange = g.distNm <= s.rangeNm;
    g.cue = classifyFlight(t, g.pos, g.ageS, airport);

    if (summary.freshestAgeS < 0 || g.ageS < summary.freshestAgeS) summary.freshestAgeS = g.ageS;
    if (!t.ac.onGround) {
      if (g.inRange)
        ++summary.inRange;
      else if (g.distNm <= 2.0 * s.rangeNm)
        ++summary.outside;
    }
    if (g.cue.phase == FlightPhase::Arrival &&
        (summary.nextArrival < 0 || g.cue.etaS < summary.nextArrivalEtaS)) {
      summary.nextArrival = static_cast<int>(out.size());
      summary.nextArrivalEtaS = g.cue.etaS;
    }
    if (g.cue.phase == FlightPhase::Arrival)
      g.landEtaS = g.cue.etaS;
    else if (g.cue.phase == FlightPhase::Cruise)
      g.landEtaS = inboundEtaS(t, g.pos, g.ageS, airport);
    if (g.landEtaS >= 0 && (summary.nextLanding < 0 || g.landEtaS < summary.nextLandingEtaS)) {
      summary.nextLanding = static_cast<int>(out.size());
      summary.nextLandingEtaS = g.landEtaS;
    }
    out.push_back(g);
  }
}

}  // namespace radar
