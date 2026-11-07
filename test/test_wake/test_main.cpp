// Host-side tests for waking the screen for landings. Run with: pio test -e native
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "geo.h"
#include "landing_wake.h"
#include "places.h"
#include "settings.h"
#include "tracker.h"
#include "traffic.h"

using namespace radar;

void setUp() {}
void tearDown() {}

static const Place* koa() { return &kAirports[0]; }

// An aircraft `distNm` from KOA on true bearing `fromDeg`, flying `trackDeg`.
static Track inbound(double fromDeg, double distNm, int altFt, float gsKt, float trackDeg, int vrFpm) {
  Track t;
  const LatLon p = destination(koa()->pos, fromDeg, distNm);
  std::strcpy(t.ac.hex, "abc123");
  std::strcpy(t.ac.callsign, "TEST1");
  std::strcpy(t.ac.type, "B738");
  t.ac.lat = p.lat;
  t.ac.lon = p.lon;
  t.ac.hasAlt = true;
  t.ac.altFt = altFt;
  t.ac.hasGs = true;
  t.ac.gsKt = gsKt;
  t.ac.hasTrack = true;
  t.ac.trackDeg = trackDeg;
  t.ac.hasVertRate = vrFpm != 0;
  t.ac.vertRateFpm = vrFpm;
  return t;
}

static float eta(const Track& t, float ageS = 0) { return inboundEtaS(t, {t.ac.lat, t.ac.lon}, ageS, koa()); }

// ---------------------------------------------------------------- policy

void test_wake_off_when_zero_minutes() {
  LandingWake w;
  TEST_ASSERT_FALSE(w.update({{"a1", 60}}, 0));
  TEST_ASSERT_FALSE(w.pending());
}

void test_wake_once_per_aircraft_and_pending_until_gone() {
  LandingWake w;
  TEST_ASSERT_FALSE(w.update({{"a1", 900}}, 10));  // 15 min out
  TEST_ASSERT_FALSE(w.pending());
  TEST_ASSERT_TRUE(w.update({{"a1", 600}}, 10));  // exactly 10 min: inside
  TEST_ASSERT_EQUAL_STRING("a1", w.lastHex().c_str());
  TEST_ASSERT_TRUE(w.pending());
  // Swiping the screen quiet again must stick for the same aircraft.
  TEST_ASSERT_FALSE(w.update({{"a1", 300}}, 10));
  TEST_ASSERT_TRUE(w.pending());
  // Estimate jumps out of the window and back: still no second wake.
  TEST_ASSERT_FALSE(w.update({{"a1", 700}}, 10));
  TEST_ASSERT_FALSE(w.pending());
  TEST_ASSERT_FALSE(w.update({{"a1", 200}}, 10));
  // Landed / gone.
  TEST_ASSERT_FALSE(w.update({}, 10));
  TEST_ASSERT_FALSE(w.pending());
}

void test_wake_for_each_new_aircraft() {
  LandingWake w;
  TEST_ASSERT_TRUE(w.update({{"a1", 400}}, 10));
  TEST_ASSERT_FALSE(w.update({{"a1", 380}, {"b2", 800}}, 10));
  TEST_ASSERT_TRUE(w.update({{"a1", 360}, {"b2", 590}}, 10));
  TEST_ASSERT_EQUAL_STRING("b2", w.lastHex().c_str());
}

void test_wake_ignores_unknown_eta_and_blank_hex() {
  LandingWake w;
  TEST_ASSERT_FALSE(w.update({{"a1", -1}, {"", 30}, {nullptr, 30}}, 10));
  TEST_ASSERT_FALSE(w.pending());
}

void test_wake_window_follows_setting() {
  LandingWake w;
  TEST_ASSERT_FALSE(w.update({{"a1", 200}}, 3));
  TEST_ASSERT_TRUE(w.update({{"a1", 170}}, 3));
  LandingWake w2;
  TEST_ASSERT_TRUE(w2.update({{"a1", 1700}}, 30));
}

void test_wake_memory_is_bounded() {
  LandingWake w;
  char hex[8];
  for (int i = 0; i <= static_cast<int>(LandingWake::kMemory); ++i) {
    std::snprintf(hex, sizeof hex, "h%02d", i);
    TEST_ASSERT_TRUE(w.update({{hex, 100}}, 10));
  }
  // h00 was forgotten, h01 was not.
  TEST_ASSERT_TRUE(w.update({{"h00", 100}}, 10));
  TEST_ASSERT_FALSE(w.update({{"h05", 100}}, 10));
}

// ---------------------------------------------------------------- inbound estimate

void test_inbound_jet_descending_toward_airport() {
  // 30 NM northeast, 9000 ft, descending, pointed at the field.
  const Track t = inbound(45, 30, 9000, 280, 225, -1500);
  const float e = eta(t);
  TEST_ASSERT_TRUE(e > 7 * 60 && e < 11 * 60);
  // Farther out means later.
  TEST_ASSERT_TRUE(eta(inbound(45, 45, 13000, 280, 225, -1500)) > e);
}

void test_inbound_low_and_level_counts() {
  // Turboprop at 3000 ft, 12 NM out, level, heading in.
  TEST_ASSERT_TRUE(eta(inbound(160, 12, 3000, 180, 340, 0)) > 0);
}

void test_inbound_rejects_overflights_and_departures() {
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 30, 9000, 280, 45, -1500)));     // heading away
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 30, 9000, 280, 225, 1500)));     // climbing
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 30, 35000, 450, 225, 0)));       // cruising over
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 10, 12000, 280, 225, -1000)));   // far too high
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 30, 12000, 300, 225, 0)));       // level, high
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 30, 9000, 280, 225, -1500), 90)); // old position
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(inbound(45, 1, 500, 120, 225, -500)));       // overhead: final logic
  TEST_ASSERT_EQUAL_FLOAT(-1, inboundEtaS(inbound(45, 30, 9000, 280, 225, -1500), {0, 0}, 0, nullptr));

  Track heli = inbound(45, 10, 1500, 110, 225, -500);
  std::strcpy(heli.ac.category, "A7");
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(heli));
  Track ground = inbound(45, 10, 1500, 110, 225, -500);
  ground.ac.onGround = true;
  TEST_ASSERT_EQUAL_FLOAT(-1, eta(ground));
}

void test_analyze_traffic_reports_next_landing() {
  Settings s = Settings::defaults();
  const Track far = inbound(45, 30, 9000, 280, 225, -1500);
  Tracker tracker;
  tracker.update({far.ac}, 1000);
  std::vector<Target> targets;
  TrafficSummary sum;
  analyzeTraffic(tracker, s, 1000, targets, sum);
  TEST_ASSERT_EQUAL(1, targets.size());
  TEST_ASSERT_EQUAL(FlightPhase::Cruise, targets[0].cue.phase);
  TEST_ASSERT_EQUAL(0, sum.nextLanding);
  TEST_ASSERT_FLOAT_WITHIN(1, eta(far), sum.nextLandingEtaS);
  TEST_ASSERT_EQUAL(-1, sum.nextArrival);
}

void test_settings_landing_wake_default_and_clamp() {
  Settings s = Settings::defaults();
  TEST_ASSERT_EQUAL(10, s.landingWakeMin);
  s.landingWakeMin = 200;
  s.sanitize();
  TEST_ASSERT_EQUAL(30, s.landingWakeMin);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_wake_off_when_zero_minutes);
  RUN_TEST(test_wake_once_per_aircraft_and_pending_until_gone);
  RUN_TEST(test_wake_for_each_new_aircraft);
  RUN_TEST(test_wake_ignores_unknown_eta_and_blank_hex);
  RUN_TEST(test_wake_window_follows_setting);
  RUN_TEST(test_wake_memory_is_bounded);
  RUN_TEST(test_inbound_jet_descending_toward_airport);
  RUN_TEST(test_inbound_low_and_level_counts);
  RUN_TEST(test_inbound_rejects_overflights_and_departures);
  RUN_TEST(test_analyze_traffic_reports_next_landing);
  RUN_TEST(test_settings_landing_wake_default_and_clamp);
  return UNITY_END();
}
