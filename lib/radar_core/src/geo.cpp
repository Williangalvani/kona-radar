#include "geo.h"

#include <cmath>

namespace radar {

double wrap360(double deg) {
  double r = std::fmod(deg, 360.0);
  if (r < 0) r += 360.0;
  if (r >= 360.0) r -= 360.0;
  return r;
}

double distanceNm(LatLon a, LatLon b) {
  const double p1 = deg2rad(a.lat), p2 = deg2rad(b.lat);
  const double dp = p2 - p1;
  const double dl = deg2rad(b.lon - a.lon);
  const double s = std::sin(dp / 2) * std::sin(dp / 2) +
                   std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2.0 * kEarthRadiusNm * std::atan2(std::sqrt(s), std::sqrt(1.0 - s));
}

double bearingDeg(LatLon from, LatLon to) {
  const double p1 = deg2rad(from.lat), p2 = deg2rad(to.lat);
  const double dl = deg2rad(to.lon - from.lon);
  const double y = std::sin(dl) * std::cos(p2);
  const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
  return wrap360(rad2deg(std::atan2(y, x)));
}

LatLon destination(LatLon from, double brgDeg, double distNm) {
  const double d = distNm / kEarthRadiusNm;
  const double b = deg2rad(brgDeg);
  const double p1 = deg2rad(from.lat), l1 = deg2rad(from.lon);
  const double p2 = std::asin(std::sin(p1) * std::cos(d) + std::cos(p1) * std::sin(d) * std::cos(b));
  const double l2 = l1 + std::atan2(std::sin(b) * std::sin(d) * std::cos(p1),
                                    std::cos(d) - std::sin(p1) * std::sin(p2));
  double lon = rad2deg(l2);
  lon = std::fmod(lon + 540.0, 360.0) - 180.0;
  return {rad2deg(p2), lon};
}

void ScopeProjection::polar(double distNm, double brgDeg, float& x, float& y) const {
  const double r = distNm / rangeNm * radiusPx;
  const double b = deg2rad(brgDeg - upBearingDeg);
  x = static_cast<float>(cx + r * std::sin(b));
  y = static_cast<float>(cy - r * std::cos(b));
}

void ScopeProjection::unproject(float x, float y, double& distNm, double& brgDeg) const {
  const double dx = x - cx, dy = y - cy;
  distNm = std::sqrt(dx * dx + dy * dy) / radiusPx * rangeNm;
  brgDeg = wrap360(rad2deg(std::atan2(dx, -dy)) + upBearingDeg);
}

LookAngles lookAngles(LatLon observer, double obsAltFt, LatLon target, double tgtAltFt) {
  LookAngles out{};
  out.groundDistNm = distanceNm(observer, target);
  out.bearingDeg = out.groundDistNm > 1e-9 ? bearingDeg(observer, target) : 0.0;

  const double h1 = obsAltFt / kFeetPerNm;
  const double h2 = tgtAltFt / kFeetPerNm;
  // Effective Earth radius for optical refraction coefficient k = 1/7.
  const double re = kEarthRadiusNm * 7.0 / 6.0;
  const double theta = out.groundDistNm / re;
  const double horiz = (re + h2) * std::sin(theta);
  const double vert = (re + h2) * std::cos(theta) - (re + h1);
  out.elevationDeg = rad2deg(std::atan2(vert, horiz));

  const double tg = out.groundDistNm / kEarthRadiusNm;
  const double sx = (kEarthRadiusNm + h2) * std::sin(tg);
  const double sy = (kEarthRadiusNm + h2) * std::cos(tg) - (kEarthRadiusNm + h1);
  out.slantDistNm = std::sqrt(sx * sx + sy * sy);
  return out;
}

void ScopeProjection::project(LatLon p, float& x, float& y, double* distOut,
                              double* brgOut) const {
  const double d = distanceNm(center, p);
  const double b = d > 1e-9 ? bearingDeg(center, p) : 0.0;
  polar(d, b, x, y);
  if (distOut) *distOut = d;
  if (brgOut) *brgOut = b;
}

}  // namespace radar
