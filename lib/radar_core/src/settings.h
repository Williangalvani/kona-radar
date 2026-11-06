// User-adjustable settings. Factory defaults come from include/config.h; the
// device stores the current values as JSON in NVS and the settings web page
// edits them through the same JSON form.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "geo.h"

namespace radar {

struct Settings {
  // Scope center
  double centerLat;
  double centerLon;
  char centerName[8];
  // Observer (where the radar is mounted; equal to the center = not set)
  double observerLat;
  double observerLon;
  int32_t observerAltFt;
  // Orientation
  int16_t viewBearingDeg;  // facing bearing: -1 = auto (observer -> center), else 0..359
  uint8_t northUp;         // 0 = facing bearing at the top, 1 = north at the top
  uint8_t screenRotation;  // physical mounting, clockwise quarter turns 0..3
  // Range / data
  uint16_t rangeNm;
  uint16_t refreshS;
  uint8_t source;  // 0 = adsb.fi, 1 = adsb.lol
  // Brightness / burn-in
  uint8_t brightness;
  uint16_t idleDimS;
  uint8_t idleBrightness;
  uint16_t wakeRangeNm;
  uint8_t nightStartH;
  uint8_t nightEndH;
  uint8_t nightBrightness;
  uint8_t nightMode;  // 0 = by the hours, 1 = always day, 2 = always night
  uint16_t blackoutS;
  uint8_t alerts;  // full-screen arrival alerts on/off

  static Settings defaults();

  LatLon center() const { return {centerLat, centerLon}; }
  LatLon observer() const { return {observerLat, observerLon}; }

  // False while the observer is still the neutral default (the center).
  bool observerSet() const;
  // Direction the viewer faces (degrees true), or NAN when it is "auto" and
  // the observer is not set.
  double facingBearing() const;
  // Compass bearing shown at the top of the screen.
  double effectiveUpBearing() const;

  // Clamps every field into its valid range. Returns true if anything changed.
  bool sanitize();
};

// Range steps the range control cycles through (from RADAR_RANGE_STEPS_NM).
const uint16_t* rangeSteps(size_t& count);
// Index of the step closest to `nm`.
size_t nearestRangeStep(uint16_t nm);

// JSON (de)serialization. fromJson() only touches keys that are present and
// sanitizes the result; it returns false (leaving `s` unchanged) on bad JSON.
std::string settingsToJson(const Settings& s);
// Only the fields of `s` that differ from `base` (what NVS stores, so new
// firmware defaults still apply to untouched settings).
std::string settingsDiffToJson(const Settings& s, const Settings& base);
bool settingsFromJson(const char* json, size_t len, Settings& s, std::string* error = nullptr);

}  // namespace radar
