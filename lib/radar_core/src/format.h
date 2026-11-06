// Text helpers shared by the UI and the web pages. All functions write a
// NUL-terminated string into `buf` (capacity `cap`) and return `buf`.
#pragma once

#include <cstddef>

#include "aircraft.h"

namespace radar {

// Best short name: callsign, else registration, else uppercase hex.
const char* displayName(const Aircraft& a, char* buf, size_t cap);

// "GND", "4,100 ft", "35,000 ft" (rounded to 100 ft); "--" if unknown.
const char* formatAltitude(const Aircraft& a, char* buf, size_t cap);

// "182 kt"; "--" if unknown.
const char* formatSpeed(const Aircraft& a, char* buf, size_t cap);

// +1 climbing, -1 descending, 0 level/unknown (threshold 300 ft/min).
int verticalTrend(const Aircraft& a);

// 8-point compass name for a bearing ("N", "NE", ... "NW").
const char* compassPoint(double deg);

// "BOEING 717-200" -> "Boeing 717-200"; mixed-case words and words
// containing digits are left alone.
const char* titleCase(const char* in, char* buf, size_t cap);

// Friendly aircraft type: adsb.fi description if present, else a built-in
// table keyed by ICAO type code, else the raw code; "" when unknown.
const char* aircraftTypeName(const Aircraft& a, char* buf, size_t cap);

// Airline from the 3-letter ICAO prefix of an airline callsign
// ("ASA1037" -> "Alaska"), else the title-cased owner; "" if none.
const char* operatorName(const Aircraft& a, char* buf, size_t cap);

// Integer with thousands separators: 35000 -> "35,000".
const char* withThousands(long v, char* buf, size_t cap);

// Where to look relative to the facing direction (degrees, -180..180):
// "AHEAD", "RIGHT 42°", "LEFT 12°", "BEHIND". "" when `relDeg` is NAN.
const char* relativeDirection(double relDeg, char* buf, size_t cap);

// Alert headline: "LOOK RIGHT", "LOOK LEFT", "LOOK AHEAD", "LOOK BEHIND", or
// "LOOK NW" (compass point of `bearingDeg`) when `relDeg` is NAN.
const char* lookCue(double relDeg, double bearingDeg, char* buf, size_t cap);

// Rounded-up arrival estimate: "~3 MIN", "~1 MIN", "NOW" (under 20 s).
const char* etaText(float etaS, char* buf, size_t cap);

// "NW 305°T".
const char* bearingText(double bearingDeg, char* buf, size_t cap);

}  // namespace radar
