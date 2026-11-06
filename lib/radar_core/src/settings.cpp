#include "settings.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "config.h"

namespace radar {
namespace {

constexpr uint16_t kRangeSteps[] = RADAR_RANGE_STEPS_NM;

template <typename T>
bool clampField(T& v, T lo, T hi) {
  if (v < lo) {
    v = lo;
    return true;
  }
  if (v > hi) {
    v = hi;
    return true;
  }
  return false;
}

template <typename T>
void readField(JsonObjectConst o, const char* key, T& field) {
  JsonVariantConst v = o[key];
  if (v.is<double>() || v.is<long long>()) {
    double d = v.as<double>();
    if (!std::isfinite(d)) return;
    const double lo = static_cast<double>(std::numeric_limits<T>::lowest());
    const double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (d < lo) d = lo;
    if (d > hi) d = hi;
    field = static_cast<T>(d);
  }
}

}  // namespace

Settings Settings::defaults() {
  Settings s{};
  s.centerLat = RADAR_CENTER_LAT;
  s.centerLon = RADAR_CENTER_LON;
  std::strncpy(s.centerName, RADAR_CENTER_NAME, sizeof(s.centerName) - 1);
  s.observerLat = RADAR_OBSERVER_LAT;
  s.observerLon = RADAR_OBSERVER_LON;
  s.observerAltFt = RADAR_OBSERVER_ALT_FT;
  s.viewBearingDeg = RADAR_VIEW_BEARING_DEG;
  s.screenRotation = RADAR_SCREEN_ROTATION;
  s.rangeNm = RADAR_DEFAULT_RANGE_NM;
  s.refreshS = RADAR_REFRESH_INTERVAL_S;
  s.source = RADAR_PRIMARY_SOURCE;
  s.brightness = RADAR_DEFAULT_BRIGHTNESS;
  s.idleDimS = RADAR_IDLE_DIM_S;
  s.idleBrightness = RADAR_IDLE_BRIGHTNESS;
  s.wakeRangeNm = RADAR_WAKE_RANGE_NM;
  s.nightStartH = RADAR_NIGHT_START_H;
  s.nightEndH = RADAR_NIGHT_END_H;
  s.nightBrightness = RADAR_NIGHT_BRIGHTNESS;
  s.sanitize();
  return s;
}

double Settings::effectiveUpBearing() const {
  if (viewBearingDeg >= 0) return viewBearingDeg;
  if (distanceNm(observer(), center()) < 0.05) return 0;
  return bearingDeg(observer(), center());
}

bool Settings::sanitize() {
  bool changed = false;
  if (!std::isfinite(centerLat) || !std::isfinite(centerLon)) {
    centerLat = RADAR_CENTER_LAT;
    centerLon = RADAR_CENTER_LON;
    changed = true;
  }
  if (!std::isfinite(observerLat) || !std::isfinite(observerLon)) {
    observerLat = RADAR_OBSERVER_LAT;
    observerLon = RADAR_OBSERVER_LON;
    changed = true;
  }
  changed |= clampField(centerLat, -89.0, 89.0);
  changed |= clampField(centerLon, -180.0, 180.0);
  changed |= clampField(observerLat, -89.0, 89.0);
  changed |= clampField(observerLon, -180.0, 180.0);
  changed |= clampField<int32_t>(observerAltFt, -1000, 30000);
  if (viewBearingDeg < -1 || viewBearingDeg > 359) {
    viewBearingDeg = static_cast<int16_t>(((viewBearingDeg % 360) + 360) % 360);
    changed = true;
  }
  changed |= clampField<uint8_t>(screenRotation, 0, 3);

  const uint16_t snapped = kRangeSteps[nearestRangeStep(rangeNm)];
  if (snapped != rangeNm) {
    rangeNm = snapped;
    changed = true;
  }
  changed |= clampField<uint16_t>(refreshS, 2, 300);
  changed |= clampField<uint8_t>(source, 0, 1);
  changed |= clampField<uint8_t>(brightness, 5, 100);
  changed |= clampField<uint16_t>(idleDimS, 0, 36000);
  changed |= clampField<uint8_t>(idleBrightness, 1, 100);
  changed |= clampField<uint16_t>(wakeRangeNm, 0, 100);
  changed |= clampField<uint8_t>(nightStartH, 0, 23);
  changed |= clampField<uint8_t>(nightEndH, 0, 23);
  changed |= clampField<uint8_t>(nightBrightness, 1, 100);

  centerName[sizeof(centerName) - 1] = 0;
  size_t n = 0;
  for (size_t i = 0; centerName[i]; ++i) {
    const unsigned char c = static_cast<unsigned char>(centerName[i]);
    if (std::isalnum(c) || c == '-' || c == ' ') centerName[n++] = static_cast<char>(std::toupper(c));
  }
  if (n == 0) {
    std::strncpy(centerName, RADAR_CENTER_NAME, sizeof(centerName) - 1);
    n = std::strlen(centerName);
  }
  centerName[n] = 0;
  return changed;
}

const uint16_t* rangeSteps(size_t& count) {
  count = sizeof(kRangeSteps) / sizeof(kRangeSteps[0]);
  return kRangeSteps;
}

size_t nearestRangeStep(uint16_t nm) {
  size_t best = 0;
  for (size_t i = 1; i < sizeof(kRangeSteps) / sizeof(kRangeSteps[0]); ++i) {
    if (std::abs(static_cast<int>(kRangeSteps[i]) - nm) <
        std::abs(static_cast<int>(kRangeSteps[best]) - nm))
      best = i;
  }
  return best;
}

std::string settingsToJson(const Settings& s) {
  JsonDocument doc;
  doc["centerLat"] = s.centerLat;
  doc["centerLon"] = s.centerLon;
  doc["centerName"] = s.centerName;
  doc["observerLat"] = s.observerLat;
  doc["observerLon"] = s.observerLon;
  doc["observerAltFt"] = s.observerAltFt;
  doc["viewBearingDeg"] = s.viewBearingDeg;
  doc["screenRotation"] = s.screenRotation;
  doc["rangeNm"] = s.rangeNm;
  doc["refreshS"] = s.refreshS;
  doc["source"] = s.source;
  doc["brightness"] = s.brightness;
  doc["idleDimS"] = s.idleDimS;
  doc["idleBrightness"] = s.idleBrightness;
  doc["wakeRangeNm"] = s.wakeRangeNm;
  doc["nightStartH"] = s.nightStartH;
  doc["nightEndH"] = s.nightEndH;
  doc["nightBrightness"] = s.nightBrightness;
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool settingsFromJson(const char* json, size_t len, Settings& s, std::string* error) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err || !doc.is<JsonObject>()) {
    if (error) *error = err ? err.c_str() : "expected a JSON object";
    return false;
  }
  JsonObjectConst o = doc.as<JsonObjectConst>();
  Settings n = s;
  readField(o, "centerLat", n.centerLat);
  readField(o, "centerLon", n.centerLon);
  if (o["centerName"].is<const char*>()) {
    std::memset(n.centerName, 0, sizeof(n.centerName));
    std::strncpy(n.centerName, o["centerName"].as<const char*>(), sizeof(n.centerName) - 1);
  }
  readField(o, "observerLat", n.observerLat);
  readField(o, "observerLon", n.observerLon);
  readField(o, "observerAltFt", n.observerAltFt);
  readField(o, "viewBearingDeg", n.viewBearingDeg);
  readField(o, "screenRotation", n.screenRotation);
  readField(o, "rangeNm", n.rangeNm);
  readField(o, "refreshS", n.refreshS);
  readField(o, "source", n.source);
  readField(o, "brightness", n.brightness);
  readField(o, "idleDimS", n.idleDimS);
  readField(o, "idleBrightness", n.idleBrightness);
  readField(o, "wakeRangeNm", n.wakeRangeNm);
  readField(o, "nightStartH", n.nightStartH);
  readField(o, "nightEndH", n.nightEndH);
  readField(o, "nightBrightness", n.nightBrightness);
  n.sanitize();
  s = n;
  return true;
}

}  // namespace radar
