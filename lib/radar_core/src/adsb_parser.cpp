#include "adsb_parser.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace radar {
namespace {

// Copies `src` into `dst` (capacity `cap`), trimming surrounding whitespace.
void copyTrimmed(char* dst, size_t cap, const char* src) {
  if (!src) {
    dst[0] = 0;
    return;
  }
  while (*src && std::isspace(static_cast<unsigned char>(*src))) ++src;
  size_t n = std::strlen(src);
  while (n > 0 && std::isspace(static_cast<unsigned char>(src[n - 1]))) --n;
  if (n >= cap) n = cap - 1;
  std::memcpy(dst, src, n);
  dst[n] = 0;
}

bool readNumber(JsonVariantConst v, double& out) {
  if (v.is<double>() || v.is<long long>()) {
    out = v.as<double>();
    return true;
  }
  return false;
}

JsonDocument makeFilter() {
  JsonDocument filter;
  const char* fields[] = {"hex",      "flight",   "r",         "t",        "desc",
                          "ownOp",    "squawk",   "category",  "lat",      "lon",
                          "alt_baro", "alt_geom", "gs",        "track",    "true_heading",
                          "calc_track", "baro_rate", "geom_rate", "seen_pos", "seen",
                          "emergency", "lastPosition"};
  for (const char* key : {"ac", "aircraft"}) {
    JsonObject item = filter[key].add<JsonObject>();
    for (const char* f : fields) item[f] = true;
  }
  filter["now"] = true;
  filter["msg"] = true;
  return filter;
}

bool parseOne(JsonObjectConst o, Aircraft& a) {
  double lat, lon;
  if (!readNumber(o["lat"], lat) || !readNumber(o["lon"], lon)) return false;
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
  a = Aircraft{};
  a.lat = lat;
  a.lon = lon;

  copyTrimmed(a.hex, sizeof(a.hex), o["hex"] | "");
  for (char* p = a.hex; *p; ++p) *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
  copyTrimmed(a.callsign, sizeof(a.callsign), o["flight"] | "");
  copyTrimmed(a.reg, sizeof(a.reg), o["r"] | "");
  copyTrimmed(a.type, sizeof(a.type), o["t"] | "");
  copyTrimmed(a.desc, sizeof(a.desc), o["desc"] | "");
  copyTrimmed(a.owner, sizeof(a.owner), o["ownOp"] | "");
  copyTrimmed(a.squawk, sizeof(a.squawk), o["squawk"] | "");
  copyTrimmed(a.category, sizeof(a.category), o["category"] | "");

  double v;
  JsonVariantConst alt = o["alt_baro"];
  if (alt.is<const char*>()) {
    a.onGround = std::strcmp(alt.as<const char*>(), "ground") == 0;
    a.hasAlt = a.onGround;
    a.altFt = 0;
  } else if (readNumber(alt, v) || readNumber(o["alt_geom"], v)) {
    a.hasAlt = true;
    a.altFt = static_cast<int32_t>(v);
  }

  if (readNumber(o["gs"], v)) {
    a.hasGs = true;
    a.gsKt = static_cast<float>(v);
  }
  if (readNumber(o["track"], v) || readNumber(o["true_heading"], v) ||
      readNumber(o["calc_track"], v)) {
    a.hasTrack = true;
    a.trackDeg = static_cast<float>(v);
  }
  if (readNumber(o["baro_rate"], v) || readNumber(o["geom_rate"], v)) {
    a.hasVertRate = true;
    a.vertRateFpm = static_cast<int32_t>(v);
  }
  if (readNumber(o["seen_pos"], v) || readNumber(o["seen"], v)) {
    a.seenPosS = static_cast<float>(v);
  }

  const char* emerg = o["emergency"] | "none";
  a.emergency = std::strcmp(emerg, "none") != 0 || std::strcmp(a.squawk, "7500") == 0 ||
                std::strcmp(a.squawk, "7600") == 0 || std::strcmp(a.squawk, "7700") == 0;
  return true;
}

}  // namespace

ParseResult parseAdsbJson(const char* json, size_t len, std::vector<Aircraft>& out,
                          size_t maxAircraft) {
  ParseResult res;
  out.clear();
  if (!json || len == 0) {
    std::snprintf(res.error, sizeof(res.error), "empty response");
    return res;
  }

  static const JsonDocument filter = makeFilter();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len, DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(8));
  if (err) {
    std::snprintf(res.error, sizeof(res.error), "bad JSON: %s", err.c_str());
    return res;
  }

  JsonArrayConst list = doc["ac"].as<JsonArrayConst>();
  if (list.isNull()) list = doc["aircraft"].as<JsonArrayConst>();
  if (list.isNull()) {
    const char* msg = doc["msg"] | "no aircraft list";
    std::snprintf(res.error, sizeof(res.error), "%s", msg);
    return res;
  }

  double now;
  if (readNumber(doc["now"], now)) {
    // Older endpoints send seconds, newer ones milliseconds.
    res.serverTimeMs = now < 1e11 ? now * 1000.0 : now;
  }

  out.reserve(list.size() < maxAircraft ? list.size() : maxAircraft);
  for (JsonObjectConst o : list) {
    ++res.listed;
    if (out.size() >= maxAircraft) continue;
    Aircraft a;
    if (parseOne(o, a)) out.push_back(a);
  }
  res.accepted = out.size();
  res.ok = true;
  return res;
}

}  // namespace radar
