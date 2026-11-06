// Great-circle helpers, the scope projection and observer look angles.
#pragma once

namespace radar {

constexpr double kEarthRadiusNm = 3440.065;
constexpr double kFeetPerNm = 6076.12;
constexpr double kPi = 3.14159265358979323846;

struct LatLon {
  double lat;
  double lon;
};

inline double deg2rad(double d) { return d * kPi / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / kPi; }

// Normalizes any angle to [0, 360).
double wrap360(double deg);

// Haversine great-circle distance in nautical miles.
double distanceNm(LatLon a, LatLon b);

// Initial great-circle bearing from `from` to `to`, degrees true [0, 360).
double bearingDeg(LatLon from, LatLon to);

// Point reached by travelling `distNm` from `from` on initial bearing `brgDeg`.
LatLon destination(LatLon from, double brgDeg, double distNm);

// Azimuthal-equidistant projection centered on the scope center: distance and
// bearing from the center are preserved exactly, which is what a radar shows.
// `upBearingDeg` is the compass bearing drawn at the top of the screen
// (0 = north-up), used to align the scope with the observer's view.
struct ScopeProjection {
  LatLon center{0, 0};
  double rangeNm = 50;      // distance mapped to radiusPx
  double upBearingDeg = 0;  // compass bearing at 12 o'clock
  float radiusPx = 226;     // pixel radius of the outer ring
  float cx = 233;           // pixel position of the center
  float cy = 233;

  float pxPerNm() const { return static_cast<float>(radiusPx / rangeNm); }

  // Screen position of `p` (x right, y down). Also returns the distance /
  // true bearing from the center when the out-params are non-null.
  void project(LatLon p, float& x, float& y, double* distNm = nullptr,
               double* brgDeg = nullptr) const;

  // Screen position for a point given as distance / true bearing from center.
  void polar(double distNm, double brgDeg, float& x, float& y) const;

  // Screen angle (degrees clockwise from 12 o'clock) of a true bearing.
  double screenAngle(double brgDeg) const { return wrap360(brgDeg - upBearingDeg); }

  // Inverse of polar(): distance / true bearing from the center of a pixel.
  void unproject(float x, float y, double& distNm, double& brgDeg) const;
};

// Where to look from the observer to see a target.
struct LookAngles {
  double groundDistNm;  // great-circle distance along the surface
  double slantDistNm;   // straight-line distance
  double bearingDeg;    // compass bearing, degrees true
  double elevationDeg;  // angle above the horizon (negative = below)
};

// Look angles from an observer at `obsAltFt` to a target at `tgtAltFt`.
// Includes Earth curvature with standard optical refraction (k = 1/7).
LookAngles lookAngles(LatLon observer, double obsAltFt, LatLon target, double tgtAltFt);

}  // namespace radar
