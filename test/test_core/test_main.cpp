// Host-side tests for lib/radar_core. Run with: pio test -e native
#include <ArduinoJson.h>
#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "adsb_parser.h"
#include "fixtures.h"
#include "format.h"
#include "geo.h"
#include "places.h"
#include "rotation.h"
#include "settings.h"
#include "tracker.h"
#include "traffic.h"

using namespace radar;

static const LatLon kQueryPoint{19.739, -156.046};  // point the fixtures were fetched for
static const LatLon kKoa{19.738765, -156.045631};
// A made-up observer 4 NM east-southeast of the airport, facing it.
static const LatLon kObserver = destination(kKoa, 115, 4.0);

void setUp() {}
void tearDown() {}

static double angleDiff(double a, double b) {
  double d = std::fmod(std::fabs(a - b), 360.0);
  return d > 180 ? 360 - d : d;
}

static const Aircraft* findByCallsign(const std::vector<Aircraft>& v, const char* cs) {
  for (const Aircraft& a : v)
    if (std::strcmp(a.callsign, cs) == 0) return &a;
  return nullptr;
}

// ---------------------------------------------------------------- parsing

void test_parse_adsbfi_v3() {
  std::vector<Aircraft> list;
  ParseResult r = parseAdsbJson(kFixtureAdsbFi, std::strlen(kFixtureAdsbFi), list);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_EQUAL(37, r.listed);
  TEST_ASSERT_EQUAL(37, r.accepted);
  TEST_ASSERT_TRUE(r.serverTimeMs > 1.7e12);

  const Aircraft* a = findByCallsign(list, "ASA1037");
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_EQUAL_STRING("a5d814", a->hex);
  TEST_ASSERT_EQUAL_STRING("N476HA", a->reg);
  TEST_ASSERT_EQUAL_STRING("B712", a->type);
  TEST_ASSERT_EQUAL_STRING("BOEING 717-200", a->desc);
  TEST_ASSERT_EQUAL_STRING("3616", a->squawk);
  TEST_ASSERT_TRUE(a->hasAlt);
  TEST_ASSERT_FALSE(a->onGround);
  TEST_ASSERT_EQUAL(3900, a->altFt);
  TEST_ASSERT_TRUE(a->hasGs);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 180.9, a->gsKt);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 38.26, a->trackDeg);
  TEST_ASSERT_EQUAL(-512, a->vertRateFpm);
  TEST_ASSERT_FALSE(a->emergency);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 21.133896, a->lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -158.112638, a->lon);

  int ground = 0;
  for (const Aircraft& x : list) ground += x.onGround;
  TEST_ASSERT_EQUAL(5, ground);

  // The C-17 has no callsign: display name falls back to the registration.
  bool foundNoCallsign = false;
  for (const Aircraft& x : list) {
    if (std::strcmp(x.hex, "ae07d2") == 0) {
      char buf[16];
      TEST_ASSERT_EQUAL_STRING("", x.callsign);
      TEST_ASSERT_EQUAL_STRING("90-0533", displayName(x, buf, sizeof buf));
      foundNoCallsign = true;
    }
  }
  TEST_ASSERT_TRUE(foundNoCallsign);
}

void test_parse_adsblol_v2() {
  std::vector<Aircraft> list;
  ParseResult r = parseAdsbJson(kFixtureAdsbLol, std::strlen(kFixtureAdsbLol), list);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_EQUAL(32, r.accepted);
  const Aircraft* a = findByCallsign(list, "ASA832");
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_EQUAL_STRING("A332", a->type);
  TEST_ASSERT_EQUAL_STRING("", a->desc);  // adsb.lol has no description
  char buf[48];
  TEST_ASSERT_EQUAL_STRING("Airbus A330-200", aircraftTypeName(*a, buf, sizeof buf));
}

void test_parse_legacy_aircraft_key_and_edge_cases() {
  const char* json = R"({"now": 1793993906.001, "aircraft": [
      {"hex": "ABC123", "flight": "  HAL21 ", "lat": 19.8, "lon": -156.1, "alt_baro": "ground"},
      {"hex": "def456", "lat": 19.9, "lon": -156.2, "alt_geom": 12000, "true_heading": 90,
       "geom_rate": 1500, "squawk": "7700"},
      {"hex": "nopos1", "flight": "X1"},
      {"hex": "badlat", "lat": 123.0, "lon": 0}
  ]})";
  std::vector<Aircraft> list;
  ParseResult r = parseAdsbJson(json, std::strlen(json), list);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_EQUAL(4, r.listed);
  TEST_ASSERT_EQUAL(2, r.accepted);
  TEST_ASSERT_DOUBLE_WITHIN(1, 1793993906001.0, r.serverTimeMs);  // seconds -> ms
  TEST_ASSERT_EQUAL_STRING("abc123", list[0].hex);
  TEST_ASSERT_EQUAL_STRING("HAL21", list[0].callsign);
  TEST_ASSERT_TRUE(list[0].onGround);
  TEST_ASSERT_EQUAL(12000, list[1].altFt);
  TEST_ASSERT_TRUE(list[1].hasTrack);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 90, list[1].trackDeg);
  TEST_ASSERT_EQUAL(1500, list[1].vertRateFpm);
  TEST_ASSERT_TRUE(list[1].emergency);
}

void test_parse_errors() {
  std::vector<Aircraft> list;
  TEST_ASSERT_FALSE(parseAdsbJson("", 0, list).ok);
  const char* broken = "{\"ac\": [ {\"hex\": ";
  TEST_ASSERT_FALSE(parseAdsbJson(broken, std::strlen(broken), list).ok);
  const char* noList = "{\"error\": \"Please contact us\"}";
  TEST_ASSERT_FALSE(parseAdsbJson(noList, std::strlen(noList), list).ok);
  const char* empty = "{\"ac\": [], \"msg\": \"No error\"}";
  ParseResult r = parseAdsbJson(empty, std::strlen(empty), list);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(0, r.accepted);
}

void test_parse_respects_max() {
  std::vector<Aircraft> list;
  ParseResult r = parseAdsbJson(kFixtureAdsbFi, std::strlen(kFixtureAdsbFi), list, 10);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(10, list.size());
  TEST_ASSERT_EQUAL(37, r.listed);
}

// ---------------------------------------------------------------- geo

// The API reports its own distance ("dst", NM) and bearing ("dir", deg) from
// the query point; our great-circle math must agree for every aircraft.
void test_geo_matches_server_dst_dir() {
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, kFixtureAdsbFi));
  int checked = 0;
  for (JsonObject o : doc["ac"].as<JsonArray>()) {
    if (!o["dst"].is<double>() || !o["dir"].is<double>()) continue;
    LatLon p{o["lat"].as<double>(), o["lon"].as<double>()};
    const double d = distanceNm(kQueryPoint, p);
    TEST_ASSERT_DOUBLE_WITHIN(0.02 * o["dst"].as<double>() + 0.05, o["dst"].as<double>(), d);
    if (d > 1.0) TEST_ASSERT_TRUE(angleDiff(bearingDeg(kQueryPoint, p), o["dir"].as<double>()) < 0.5);
    ++checked;
  }
  TEST_ASSERT_TRUE(checked > 30);
}

void test_geo_basics() {
  // One degree of latitude is 60 NM.
  TEST_ASSERT_DOUBLE_WITHIN(0.1, 60.0, distanceNm({19, -156}, {20, -156}));
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, bearingDeg({19, -156}, {20, -156}));
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 90.0, bearingDeg({19.7, -156}, {19.7, -155}));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 350.0, wrap360(-10));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, wrap360(730));

  // destination() is the inverse of distance/bearing.
  for (double brg = 0; brg < 360; brg += 37) {
    LatLon q = destination(kKoa, brg, 42.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 42.0, distanceNm(kKoa, q));
    TEST_ASSERT_TRUE(angleDiff(brg, bearingDeg(kKoa, q)) < 1e-6);
  }

  // KOA -> Hilo is ~56 NM, roughly due east.
  const double koaIto = distanceNm(kKoa, kAirports[1].pos);
  TEST_ASSERT_DOUBLE_WITHIN(1.5, 56.4, koaIto);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, 91.0, bearingDeg(kKoa, kAirports[1].pos));
}

void test_airport_and_runway_data() {
  const Place* koa = airportNear({19.74, -156.04});
  TEST_ASSERT_NOT_NULL(koa);
  TEST_ASSERT_EQUAL_STRING("KOA", koa->code);
  TEST_ASSERT_NULL(airportNear({19.5, -156.5}));
  TEST_ASSERT_EQUAL(1, koa->runwayCount);
  const Runway& rw = koa->runways[0];
  // Runway 17/35 is 11,000 ft long and points 170/350 magnetic (~180 true).
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 11000.0 / kFeetPerNm, distanceNm(rw.threshold[0], rw.threshold[1]));
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 185.0, bearingDeg(rw.threshold[0], rw.threshold[1]));
  TEST_ASSERT_TRUE(distanceNm(koa->pos, rw.threshold[0]) < 1.0);
}

void test_projection_north_up() {
  ScopeProjection p;
  p.center = kKoa;
  p.rangeNm = 50;
  p.radiusPx = 200;
  p.cx = 233;
  p.cy = 233;
  float x, y;
  p.polar(25, 0, x, y);  // halfway north -> straight up
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 233, x);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 133, y);
  p.polar(50, 90, x, y);  // range east -> right edge
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 433, x);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 233, y);

  double d, b;
  p.project(kAirports[1].pos, x, y, &d, &b);  // Hilo, ~56 NM east: just outside 50 NM
  TEST_ASSERT_TRUE(x > 433);
  TEST_ASSERT_FLOAT_WITHIN(10, 233, y);
  p.unproject(x, y, d, b);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, distanceNm(kKoa, kAirports[1].pos), d);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, bearingDeg(kKoa, kAirports[1].pos), b);
}

void test_projection_view_aligned() {
  ScopeProjection p;
  p.center = kKoa;
  p.rangeNm = 10;
  p.radiusPx = 200;
  p.upBearingDeg = 281;  // an observer east of KOA facing it
  float x, y;
  // Something due 281 deg from the center is drawn straight up...
  p.polar(5, 281, x, y);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 233, x);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 133, y);
  // ...and a point behind you (101 deg from KOA) straight down.
  double d, b;
  p.project(destination(kKoa, 101, 4.0), x, y, &d, &b);
  TEST_ASSERT_FLOAT_WITHIN(2.0, 233, x);
  TEST_ASSERT_TRUE(y > 233 + 60);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 79.0, p.screenAngle(0));  // north shows up 79 deg clockwise
  // Round trip through unproject().
  double d2, b2;
  p.unproject(x, y, d2, b2);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, d, d2);
  TEST_ASSERT_TRUE(angleDiff(b, b2) < 1e-3);
}

void test_look_angles() {
  // 10 NM away at 6,076 ft: ~5.6 deg up after curvature.
  LatLon tgt = destination(kObserver, 281, 10.0);
  LookAngles la = lookAngles(kObserver, 0, tgt, 6076.12);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 10.0, la.groundDistNm);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 281.0, la.bearingDeg);
  TEST_ASSERT_DOUBLE_WITHIN(0.15, rad2deg(std::atan(0.1)) - 0.07, la.elevationDeg);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, std::sqrt(100.0 + 1.0), la.slantDistNm);
  // Overhead-ish traffic is high in the sky; far, low traffic is below the horizon.
  // 0.5 NM out at 5,000 ft (4,900 ft above the eye): atan(4900 / 3038) = 58.2 deg.
  TEST_ASSERT_DOUBLE_WITHIN(0.3, 58.2,
                            lookAngles(kObserver, 100, destination(kObserver, 0, 0.5), 5000).elevationDeg);
  TEST_ASSERT_TRUE(lookAngles(kObserver, 100, destination(kObserver, 0, 150), 500).elevationDeg < 0);
}

// ---------------------------------------------------------------- tracker

static Aircraft mkAircraft(const char* hex, LatLon pos, float gs, float trk, float seen = 0) {
  Aircraft a;
  std::strcpy(a.hex, hex);
  a.lat = pos.lat;
  a.lon = pos.lon;
  a.hasGs = gs > 0;
  a.gsKt = gs;
  a.hasTrack = true;
  a.trackDeg = trk;
  a.hasAlt = true;
  a.altFt = 10000;
  a.seenPosS = seen;
  return a;
}

void test_tracker_dead_reckoning() {
  Tracker t;
  t.update({mkAircraft("aaaaaa", kKoa, 360, 90, 2.0f)}, 100000);
  TEST_ASSERT_EQUAL(1, t.tracks().size());
  const Track* tr = t.find("aaaaaa");
  TEST_ASSERT_NOT_NULL(tr);
  TEST_ASSERT_EQUAL_UINT32(98000, tr->posTimeMs);  // position was 2 s old
  // 360 kt for 10 s (8 s after receipt + 2 s age) = 1 NM east.
  LatLon p = Tracker::predict(*tr, 108000);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 1.0, distanceNm(kKoa, p));
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 90.0, bearingDeg(kKoa, p));
  // Extrapolation is capped.
  const LatLon far = Tracker::predict(*tr, 98000 + 10 * 60000);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 3.0, distanceNm(kKoa, far));
}

void test_tracker_ground_and_no_speed_do_not_move() {
  Tracker t;
  Aircraft g = mkAircraft("bbbbbb", kKoa, 15, 180);
  g.onGround = true;
  Aircraft n = mkAircraft("cccccc", kKoa, 0, 180);
  t.update({g, n}, 1000);
  for (const Track& tr : t.tracks()) {
    LatLon p = Tracker::predict(tr, 21000);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, distanceNm(kKoa, p));
  }
}

void test_tracker_merge_trail_and_expire() {
  Tracker t;
  LatLon p0 = kKoa;
  t.update({mkAircraft("aaaaaa", p0, 300, 0)}, 0);
  t.update({mkAircraft("aaaaaa", destination(p0, 0, 0.4), 300, 0)}, 5000);  // < spacing: no trail
  TEST_ASSERT_EQUAL(0, t.find("aaaaaa")->trailCount);
  t.update({mkAircraft("aaaaaa", destination(p0, 0, 1.0), 300, 0)}, 12000);
  TEST_ASSERT_EQUAL(1, t.find("aaaaaa")->trailCount);
  for (uint32_t ms = 24000; ms <= 120000; ms += 12000)
    t.update({mkAircraft("aaaaaa", destination(p0, 0, ms / 12000.0), 300, 0)}, ms);
  TEST_ASSERT_EQUAL(Track::kTrailLen, t.find("aaaaaa")->trailCount);
  // trail[0] is the newest history point and lies south of the current position.
  const Track* tr = t.find("aaaaaa");
  TEST_ASSERT_TRUE(tr->trail[0].lat < tr->ac.lat);
  TEST_ASSERT_TRUE(tr->trail[1].lat < tr->trail[0].lat);

  // An older (out-of-order) report must not move the aircraft backwards.
  const double latNow = tr->ac.lat;
  t.update({mkAircraft("aaaaaa", p0, 300, 0, 60.0f)}, 121000);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, latNow, t.find("aaaaaa")->ac.lat);

  t.update({mkAircraft("dddddd", p0, 100, 0)}, 150000);
  t.expire(185000, 60000);
  TEST_ASSERT_NULL(t.find("aaaaaa"));
  TEST_ASSERT_NOT_NULL(t.find("dddddd"));
}

// ---------------------------------------------------------------- format

void test_format() {
  char buf[64];
  Aircraft a;
  TEST_ASSERT_EQUAL_STRING("--", formatAltitude(a, buf, sizeof buf));
  a.hasAlt = true;
  a.altFt = 35012;
  TEST_ASSERT_EQUAL_STRING("35,000 ft", formatAltitude(a, buf, sizeof buf));
  a.altFt = 4150;
  TEST_ASSERT_EQUAL_STRING("4,200 ft", formatAltitude(a, buf, sizeof buf));
  a.altFt = 950;
  TEST_ASSERT_EQUAL_STRING("1,000 ft", formatAltitude(a, buf, sizeof buf));
  a.onGround = true;
  TEST_ASSERT_EQUAL_STRING("GND", formatAltitude(a, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("-1,234,567", withThousands(-1234567, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("999", withThousands(999, buf, sizeof buf));

  a.hasGs = true;
  a.gsKt = 182.6f;
  TEST_ASSERT_EQUAL_STRING("183 kt", formatSpeed(a, buf, sizeof buf));

  TEST_ASSERT_EQUAL_STRING("N", compassPoint(359));
  TEST_ASSERT_EQUAL_STRING("NE", compassPoint(44));
  TEST_ASSERT_EQUAL_STRING("W", compassPoint(281));
  TEST_ASSERT_EQUAL_STRING("SE", compassPoint(-225));

  TEST_ASSERT_EQUAL_STRING("Boeing 717-200", titleCase("BOEING 717-200", buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("McDonnell Douglas C-17A Globemaster III",
                           titleCase("McDonnell Douglas C-17A Globemaster III", buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("Hawaiian Airlines Inc", titleCase("HAWAIIAN AIRLINES INC", buf, sizeof buf));

  Aircraft b;
  std::strcpy(b.callsign, "ASA1037");
  std::strcpy(b.owner, "HAWAIIAN AIRLINES INC");
  TEST_ASSERT_EQUAL_STRING("Alaska", operatorName(b, buf, sizeof buf));
  std::strcpy(b.callsign, "N476HA");  // registration-style callsign: use owner
  TEST_ASSERT_EQUAL_STRING("Hawaiian Airlines Inc", operatorName(b, buf, sizeof buf));

  Aircraft c;
  c.hasVertRate = true;
  c.vertRateFpm = -512;
  TEST_ASSERT_EQUAL(-1, verticalTrend(c));
  c.vertRateFpm = 100;
  TEST_ASSERT_EQUAL(0, verticalTrend(c));

  TEST_ASSERT_EQUAL_STRING("RIGHT 42\u00b0", relativeDirection(42.4, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("LEFT 36\u00b0", relativeDirection(-36.2, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("AHEAD", relativeDirection(-6, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("BEHIND", relativeDirection(170, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("", relativeDirection(NAN, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("LOOK RIGHT", lookCue(36, 0, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("LOOK LEFT", lookCue(-90, 0, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("LOOK NW", lookCue(NAN, 305, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("~3 MIN", etaText(141, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("~1 MIN", etaText(45, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("NOW", etaText(10, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("NW 305\u00b0T", bearingText(305.3, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("N 005\u00b0T", bearingText(365, buf, sizeof buf));
}

// ---------------------------------------------------------------- traffic

// The example traffic from docs/design/manifest.json.
static Aircraft designAircraft(const char* cs, double lat, double lon, int alt, float gs,
                               float trk, int vr, const char* type) {
  Aircraft a;
  std::snprintf(a.hex, sizeof a.hex, "%06x", static_cast<unsigned>(std::hash<std::string>{}(cs) & 0xffffff));
  std::strcpy(a.callsign, cs);
  std::strcpy(a.type, type);
  a.lat = lat;
  a.lon = lon;
  a.hasAlt = true;
  a.altFt = alt;
  a.hasGs = true;
  a.gsKt = gs;
  a.hasTrack = true;
  a.trackDeg = trk;
  a.hasVertRate = vr != 0;
  a.vertRateFpm = vr;
  return a;
}

// Feeds `a` at `nowMs` after a report 12 s earlier along its track, so the
// tracker has one history point, as it would after a couple of downloads.
static void feed(Tracker& t, std::vector<Aircraft> list, uint32_t nowMs, float ageS = 0) {
  std::vector<Aircraft> before = list;
  for (Aircraft& b : before) {
    const LatLon p = destination({b.lat, b.lon}, b.trackDeg + 180, b.gsKt * 12.0 / 3600.0);
    b.lat = p.lat;
    b.lon = p.lon;
    b.seenPosS = ageS;
  }
  for (Aircraft& a : list) a.seenPosS = ageS;
  t.update(before, nowMs - 12000);
  t.update(list, nowMs);
}

static std::vector<Aircraft> designTraffic() {
  return {designAircraft("HAL208", 19.840, -156.036, 1900, 132, 185, -600, "B712"),
          designAircraft("SWA250", 19.786, -156.043, 2300, 155, 5, 1400, "B38M"),
          designAircraft("ASA875", 19.700, -156.125, 4400, 210, 55, -700, "A21N"),
          designAircraft("HAL123", 19.670, -156.052, 3600, 178, 5, -500, "A332"),
          designAircraft("MHO507", 19.759, -155.951, 2800, 110, 210, 0, "C208"),
          designAircraft("N123BH", 19.707, -156.025, 900, 85, 340, 0, "EC30")};
}

static const Target* findTarget(const std::vector<Target>& v, const char* cs) {
  for (const Target& g : v)
    if (std::strcmp(g.track->ac.callsign, cs) == 0) return &g;
  return nullptr;
}

void test_traffic_arrival_departure_cues() {
  Tracker t;
  feed(t, designTraffic(), 100000);
  Settings s = Settings::defaults();
  s.observerLat = kObserver.lat;
  s.observerLon = kObserver.lon;
  std::vector<Target> targets;
  TrafficSummary sum;
  analyzeTraffic(t, s, 100000, targets, sum);
  TEST_ASSERT_EQUAL(6, targets.size());

  const Target* hal = findTarget(targets, "HAL208");
  TEST_ASSERT_EQUAL(FlightPhase::Arrival, hal->cue.phase);
  TEST_ASSERT_EQUAL_STRING("17", hal->cue.runway);
  // ~5.2 NM to the runway 17 threshold at 132 kt.
  TEST_ASSERT_FLOAT_WITHIN(6, 142, hal->cue.etaS);
  TEST_ASSERT_EQUAL(hal - targets.data(), sum.nextArrival);

  const Target* swa = findTarget(targets, "SWA250");
  TEST_ASSERT_EQUAL(FlightPhase::Departure, swa->cue.phase);
  TEST_ASSERT_EQUAL_STRING("35", swa->cue.runway);

  // Aligned with runway 35 but far too high for an approach.
  TEST_ASSERT_EQUAL(FlightPhase::Cruise, findTarget(targets, "HAL123")->cue.phase);
  TEST_ASSERT_EQUAL(FlightPhase::Cruise, findTarget(targets, "ASA875")->cue.phase);
  TEST_ASSERT_EQUAL(FlightPhase::Helicopter, findTarget(targets, "N123BH")->cue.phase);

  // Facing defaults to "toward the airport" once the observer is set.
  TEST_ASSERT_TRUE(s.observerSet());
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 295.0, s.facingBearing());
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, wrap180(hal->bearingDeg - s.facingBearing()), hal->relDeg);
  TEST_ASSERT_TRUE(hal->relDeg > 0);  // north of the airport: to the right
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, distanceNm(kObserver, hal->pos), hal->distNm);
  TEST_ASSERT_EQUAL(6, sum.inRange + sum.outside);
}

void test_traffic_needs_history_and_fresh_data() {
  Settings s = Settings::defaults();
  std::vector<Target> targets;
  TrafficSummary sum;
  const Aircraft hal = designTraffic()[0];

  // A single report cannot show the aircraft is closing on the runway.
  Tracker once;
  once.update({hal}, 1000);
  analyzeTraffic(once, s, 1000, targets, sum);
  TEST_ASSERT_EQUAL(FlightPhase::Cruise, targets[0].cue.phase);
  TEST_ASSERT_EQUAL(-1, sum.nextArrival);
  // Observer not set: no relative direction, north-up.
  TEST_ASSERT_TRUE(std::isnan(targets[0].relDeg));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, s.effectiveUpBearing());

  // 45 s old: muted but still an arrival. 70 s: no estimate. 200 s: gone.
  Tracker t;
  feed(t, {hal}, 100000, 45);
  analyzeTraffic(t, s, 100000, targets, sum);
  TEST_ASSERT_TRUE(targets[0].stale);
  TEST_ASSERT_EQUAL(FlightPhase::Arrival, targets[0].cue.phase);
  analyzeTraffic(t, s, 125000, targets, sum);
  TEST_ASSERT_EQUAL(FlightPhase::Cruise, targets[0].cue.phase);
  analyzeTraffic(t, s, 260000, targets, sum);
  TEST_ASSERT_EQUAL(0, targets.size());

  // Climbing on the approach course is not an arrival.
  Aircraft climbing = hal;
  climbing.vertRateFpm = 1200;
  Tracker c;
  feed(c, {climbing}, 100000);
  analyzeTraffic(c, s, 100000, targets, sum);
  TEST_ASSERT_TRUE(targets[0].cue.phase != FlightPhase::Arrival);
}

// ---------------------------------------------------------------- rotation

void test_rotation_consistency() {
  const int W = 466, H = 466;
  for (uint8_t rot = 0; rot < 4; ++rot) {
    for (int x = 0; x < W; x += 31) {
      for (int y = 0; y < H; y += 29) {
        int px, py, bx, by;
        rotatePoint(rot, W, H, x, y, px, py);
        TEST_ASSERT_TRUE(px >= 0 && px < W && py >= 0 && py < H);
        unrotatePoint(rot, W, H, px, py, bx, by);
        TEST_ASSERT_EQUAL(x, bx);
        TEST_ASSERT_EQUAL(y, by);
      }
    }
    // A block rotated with rotateBlock16 lands where rotatePoint says.
    const int x1 = 100, y1 = 40, lw = 6, lh = 4;
    uint16_t src[lw * lh], dst[lw * lh];
    for (int k = 0; k < lw * lh; ++k) src[k] = static_cast<uint16_t>(k);
    rotateBlock16(rot, src, dst, lw, lh);
    int ax1, ay1, ax2, ay2;
    rotateArea(rot, W, H, x1, y1, x1 + lw - 1, y1 + lh - 1, ax1, ay1, ax2, ay2);
    const int pw = ax2 - ax1 + 1;
    TEST_ASSERT_EQUAL(lw * lh, pw * (ay2 - ay1 + 1));
    // Rounded (even start, odd end) areas stay aligned after rotation.
    TEST_ASSERT_EQUAL(0, ax1 % 2);
    TEST_ASSERT_EQUAL(0, ay1 % 2);
    TEST_ASSERT_EQUAL(1, ax2 % 2);
    TEST_ASSERT_EQUAL(1, ay2 % 2);
    for (int j = 0; j < lh; ++j) {
      for (int i = 0; i < lw; ++i) {
        int px, py;
        rotatePoint(rot, W, H, x1 + i, y1 + j, px, py);
        TEST_ASSERT_EQUAL(src[j * lw + i], dst[(py - ay1) * pw + (px - ax1)]);
      }
    }
  }
}

// ---------------------------------------------------------------- settings

void test_settings_defaults_and_json() {
  Settings s = Settings::defaults();
  // Neutral default: the observer is the airport itself, i.e. not set.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, s.centerLat, s.observerLat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, s.centerLon, s.observerLon);
  TEST_ASSERT_FALSE(s.observerSet());
  TEST_ASSERT_EQUAL(-1, s.viewBearingDeg);
  TEST_ASSERT_TRUE(std::isnan(s.facingBearing()));
  TEST_ASSERT_EQUAL(10, s.rangeNm);
  TEST_ASSERT_EQUAL_STRING("KOA", s.centerName);

  // Auto facing = observer -> center; an explicit bearing wins; north-up
  // overrides what is drawn at the top but not the facing direction.
  s.observerLat = kObserver.lat;
  s.observerLon = kObserver.lon;
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 295.0, s.effectiveUpBearing());
  s.viewBearingDeg = 281;
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 281.0, s.effectiveUpBearing());
  s.northUp = 1;
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, s.effectiveUpBearing());
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 281.0, s.facingBearing());
  s.northUp = 0;
  s.viewBearingDeg = -1;

  std::string json = settingsToJson(s);
  Settings r = Settings::defaults();
  TEST_ASSERT_TRUE(settingsFromJson(json.data(), json.size(), r));
  TEST_ASSERT_EQUAL(-1, r.viewBearingDeg);
  TEST_ASSERT_EQUAL(s.rangeNm, r.rangeNm);
  TEST_ASSERT_EQUAL_STRING(s.centerName, r.centerName);

  // Partial update with out-of-range values gets clamped / snapped.
  const char* upd = R"({"rangeNm": 30, "refreshS": 0, "brightness": -5, "viewBearingDeg": 450,
                       "observerLat": 19.5, "centerName": "itO!", "screenRotation": 9,
                       "northUp": true, "nightMode": 7})";
  TEST_ASSERT_TRUE(settingsFromJson(upd, std::strlen(upd), r));
  TEST_ASSERT_EQUAL(20, r.rangeNm);
  TEST_ASSERT_EQUAL(1, r.northUp);
  TEST_ASSERT_EQUAL(2, r.nightMode);
  TEST_ASSERT_EQUAL(2, r.refreshS);
  TEST_ASSERT_EQUAL(5, r.brightness);
  TEST_ASSERT_EQUAL(90, r.viewBearingDeg);
  TEST_ASSERT_EQUAL(3, r.screenRotation);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 19.5, r.observerLat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, kObserver.lon, r.observerLon);  // untouched
  TEST_ASSERT_EQUAL_STRING("ITO", r.centerName);

  // Garbage is rejected without modifying the settings.
  Settings before = r;
  TEST_ASSERT_FALSE(settingsFromJson("not json", 8, r));
  TEST_ASSERT_EQUAL(before.rangeNm, r.rangeNm);
  TEST_ASSERT_FALSE(settingsFromJson("[1,2]", 5, r));
}

void test_settings_diff_stores_only_changes() {
  const Settings def = Settings::defaults();
  TEST_ASSERT_EQUAL_STRING("{}", settingsDiffToJson(def, def).c_str());

  Settings s = def;
  s.rangeNm = 20;
  s.observerLat = 19.7;
  const std::string diff = settingsDiffToJson(s, def);
  TEST_ASSERT_EQUAL_STRING(R"({"observerLat":19.7,"rangeNm":20})", diff.c_str());

  // Loading the diff over (possibly newer) defaults restores the changes and
  // keeps every other default.
  Settings newer = def;
  newer.brightness = 60;
  TEST_ASSERT_TRUE(settingsFromJson(diff.data(), diff.size(), newer));
  TEST_ASSERT_EQUAL(20, newer.rangeNm);
  // ArduinoJson parses decimals with <= 7 significant digits as float
  // (~0.2 m of latitude here), longer ones as double.
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, 19.7, newer.observerLat);
  TEST_ASSERT_EQUAL(60, newer.brightness);
}

void test_coastline_projects_onto_big_island() {
  // The Big Island polygon must surround a point just inland of KOA and the
  // coastline must pass within a few NM of KOA (it sits on the shore).
  TEST_ASSERT_TRUE(kCoastlineCount >= 5);
  const CoastPolygon& big = kCoastline[0];
  TEST_ASSERT_EQUAL_STRING("Hawaii", big.name);
  double nearest = 1e9;
  for (size_t i = 0; i < big.count; ++i) {
    LatLon p{big.latlon[2 * i], big.latlon[2 * i + 1]};
    nearest = std::fmin(nearest, distanceNm(kKoa, p));
  }
  TEST_ASSERT_TRUE(nearest < 1.5);

  // Even-odd point-in-polygon for Mauna Kea's summit (19.82 N, 155.47 W).
  const double px = -155.47, py = 19.82;
  bool inside = false;
  for (size_t i = 0, j = big.count - 1; i < big.count; j = i++) {
    const double yi = big.latlon[2 * i], xi = big.latlon[2 * i + 1];
    const double yj = big.latlon[2 * j], xj = big.latlon[2 * j + 1];
    if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) inside = !inside;
  }
  TEST_ASSERT_TRUE(inside);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_adsbfi_v3);
  RUN_TEST(test_parse_adsblol_v2);
  RUN_TEST(test_parse_legacy_aircraft_key_and_edge_cases);
  RUN_TEST(test_parse_errors);
  RUN_TEST(test_parse_respects_max);
  RUN_TEST(test_geo_matches_server_dst_dir);
  RUN_TEST(test_geo_basics);
  RUN_TEST(test_airport_and_runway_data);
  RUN_TEST(test_projection_north_up);
  RUN_TEST(test_projection_view_aligned);
  RUN_TEST(test_look_angles);
  RUN_TEST(test_tracker_dead_reckoning);
  RUN_TEST(test_tracker_ground_and_no_speed_do_not_move);
  RUN_TEST(test_tracker_merge_trail_and_expire);
  RUN_TEST(test_format);
  RUN_TEST(test_traffic_arrival_departure_cues);
  RUN_TEST(test_traffic_needs_history_and_fresh_data);
  RUN_TEST(test_rotation_consistency);
  RUN_TEST(test_settings_defaults_and_json);
  RUN_TEST(test_settings_diff_stores_only_changes);
  RUN_TEST(test_coastline_projects_onto_big_island);
  return UNITY_END();
}
