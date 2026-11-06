// Design screens 08, 09, 11 and 12: aircraft detail, look-up alert, dim
// idle and observer alignment.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "format.h"
#include "places.h"
#include "screens.h"
#include "theme.h"
#include "traffic.h"

namespace ui {
namespace {

constexpr const char* kDash = "\xE2\x80\x94";

void backChevron(draw::Surface& s) {
  const draw::Point pts[3] = {{113, 70}, {104, 79}, {113, 88}};
  draw::polyline(s, pts, 3, 2, col(palette().muted));
}

bool onBack(int x, int y) { return x < 160 && y < 115; }

std::string lookText(const radar::Target& t) {
  char buf[24];
  if (std::isnan(t.relDeg))
    radar::bearingText(t.bearingDeg, buf, sizeof(buf));
  else
    radar::relativeDirection(t.relDeg, buf, sizeof(buf));
  return buf;
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

// ------------------------------------------------------------------ detail

void DetailScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "AIRCRAFT");
  art_.create(root_, [](draw::Surface& s) {
    const Palette& p = palette();
    backChevron(s);
    draw::segment(s, 89, 221.5f, 377, 221.5f, 1, col(p.grid));
    draw::segment(s, 233.5f, 239, 233.5f, 316, 1, col(p.grid));
  });
  callsign_.create(root_, 233, 83, font::bold(46), p.text, Align::Center, 1);
  sub_.create(root_, 233, 139, font::medium(18), p.muted);
  tag_.create(root_, 233, 171, 11);
  const int xs[2] = {150, 318};
  const char* names[2] = {"ALTITUDE", "GROUND SPEED"};
  const char* units[2] = {"FT", "KT"};
  for (int i = 0; i < 2; ++i) {
    labels_[i].create(root_, xs[i], 237, font::mono(11), p.muted, Align::Center, 1);
    labels_[i].set(names[i]);
    values_[i].create(root_, xs[i], 261, font::bold(36), p.text);
    units_[i].create(root_, xs[i], 303, font::mono(12), p.quiet);
    units_[i].set(units[i]);
  }
  vrate_.create(root_, 150, 335, font::medium(16), p.muted);
  track_.create(root_, 318, 335, font::medium(16), p.muted);
  lookBox_ = panel(root_, 100, 371, 366, 414, 11, p.panel, p.panelLine);
  look_.create(root_, 233, 383, font::bold(20), p.amber);
  footer(root_, footer_, "", p.quiet);
}

void DetailScreen::refresh(const Model& m) {
  const Palette& p = palette();
  const radar::Target* t = findTarget(m, hex_);
  if (!t) {
    tag_.set("SIGNAL LOST", p.quiet, p.panel, p.panelLine);
    for (auto& v : values_) v.set(kDash);
    look_.set(kDash);
    return;
  }
  const radar::Aircraft& a = t->track->ac;
  char buf[64], buf2[32];
  callsign_.set(radar::displayName(a, buf, sizeof(buf)));
  radar::operatorName(a, buf, sizeof(buf));
  radar::aircraftTypeName(a, buf2, sizeof(buf2));
  std::string sub = buf;
  if (buf2[0]) sub += sub.empty() ? std::string(buf2) : std::string("  \xC2\xB7  ") + buf2;
  if (sub.empty()) sub = a.reg[0] ? a.reg : "Unknown aircraft";
  if (sub.size() > 34) sub = sub.substr(0, 33) + "\xE2\x80\xA6";
  sub_.set(sub);

  const radar::FlightCue& c = t->cue;
  if (a.emergency) {
    std::snprintf(buf, sizeof(buf), "EMERGENCY \xC2\xB7 %s", a.squawk);
    tag_.set(buf, p.coral, p.panel, p.panelLine);
  } else if (t->stale) {
    std::snprintf(buf, sizeof(buf), "STALE %ds", static_cast<int>(t->ageS));
    tag_.set(buf, p.amber, p.panel, p.panelLine);
  } else if (c.phase == radar::FlightPhase::Arrival) {
    std::snprintf(buf, sizeof(buf), "LIKELY ARRIVAL \xC2\xB7 RWY %s", c.runway);
    tag_.set(buf, p.amber, p.panel, p.panelLine);
  } else if (c.phase == radar::FlightPhase::Departure) {
    std::snprintf(buf, sizeof(buf), "LIKELY DEPARTURE \xC2\xB7 RWY %s", c.runway);
    tag_.set(buf, p.sage, p.panel, p.panelLine);
  } else {
    tag_.set(c.phase == radar::FlightPhase::Helicopter ? "HELICOPTER"
             : c.phase == radar::FlightPhase::Ground   ? "ON GROUND"
                                                       : "TRAFFIC",
             p.muted, p.panel, p.panelLine);
  }

  if (a.onGround) {
    values_[0].set("GND");
  } else if (a.hasAlt) {
    radar::withThousands(std::lround(a.altFt / 100.0) * 100, buf, sizeof(buf));
    values_[0].set(buf);
  } else {
    values_[0].set(kDash);
  }
  if (a.hasGs) {
    std::snprintf(buf, sizeof(buf), "%ld", std::lround(a.gsKt));
    values_[1].set(buf);
  } else {
    values_[1].set(kDash);
  }
  if (!a.hasVertRate || a.onGround) {
    vrate_.set(kDash);
  } else if (std::abs(a.vertRateFpm) < 100) {
    vrate_.set("LEVEL");
  } else {
    radar::withThousands(std::lround(std::abs(a.vertRateFpm) / 100.0) * 100, buf2, sizeof(buf2));
    std::snprintf(buf, sizeof(buf), "%s %s FT/MIN", a.vertRateFpm > 0 ? "\xE2\x86\x91" : "\xE2\x86\x93", buf2);
    vrate_.set(buf);
  }
  if (a.hasTrack) {
    std::snprintf(buf, sizeof(buf), "%03ld\xC2\xB0 TRUE", std::lround(a.trackDeg) % 360);
    track_.set(buf);
  } else {
    track_.set(kDash);
  }

  std::string look = lookText(*t);
  if (c.phase == radar::FlightPhase::Arrival && !t->stale) {
    radar::etaText(c.etaS, buf, sizeof(buf));
  } else {
    std::snprintf(buf, sizeof(buf), "%.1f NM", t->distNm);
  }
  look += std::string("  \xC2\xB7  ") + buf;
  look_.set(look);
  look_.setColor(c.phase == radar::FlightPhase::Arrival ? p.amber : p.text);

  std::string f = m.exampleData ? "EXAMPLE \xC2\xB7 " : "";
  if (a.squawk[0]) f += std::string("SQUAWK ") + a.squawk + " \xC2\xB7 ";
  std::snprintf(buf, sizeof(buf), "%ld\xC2\xB0 UP", std::lround(std::max(0.0, t->elevationDeg)));
  footer_.set(f + buf);
}

void DetailScreen::tap(int x, int y) {
  if (onBack(x, y) && nav_) nav_->back();
}

void DetailScreen::swipe(Swipe s) {
  if (s == Swipe::Right && nav_) nav_->back();
}

// ------------------------------------------------------------------ alert

void AlertScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    const float r = rel_;
    draw::arc(s, 233, 233, 219, 4, r - 90 - 18, r - 90 + 18, col(p.amber));
    draw::ring(s, 233, 248, 60, 1, col(p.grid));
    if (r >= 0)
      draw::arc(s, 233, 248, 60, 2, 270, 270 + r, col(p.amber));
    else
      draw::arc(s, 233, 248, 60, 2, 270 + r, 270, col(p.amber));
    draw::segment(s, 233.5f, 179, 233.5f, 188, 1, col(p.quiet));
    draw::arrow(s, 233, 248, r, 65, 5, col(p.amber));
    draw::fillCircle(s, 233, 248, 4, col(p.text));
  });
  kind_.create(root_, 233, 51, font::mono(12), p.amber);
  look_.create(root_, 233, 93, font::bold(46), p.text);
  sub_.create(root_, 233, 148, font::medium(18), p.muted);
  eta_.create(root_, 233, 325, font::bold(46), p.amber);
  who_.create(root_, 233, 378, font::medium(20), p.text);
  data_.create(root_, 233, 409, font::mono(14), p.muted);
  footer(root_, footer_, "ESTIMATE ONLY", p.quiet);
}

void AlertScreen::refresh(const Model& m) {
  const radar::Target* t = findTarget(m, m.alertHex);
  if (!t) return;
  hex_ = m.alertHex;
  const radar::Aircraft& a = t->track->ac;
  const bool hasRel = !std::isnan(t->relDeg);
  const float rel = static_cast<float>(hasRel ? t->relDeg : radar::wrap180(t->bearingDeg));
  if (std::fabs(rel - rel_) >= 1) {
    rel_ = rel;
    art_.invalidate();
  }
  char buf[48];
  std::snprintf(buf, sizeof(buf), "LIKELY LANDING \xC2\xB7 RWY %s", t->cue.runway ? t->cue.runway : kDash);
  kind_.set(buf);
  look_.set(radar::lookCue(t->relDeg, t->bearingDeg, buf, sizeof(buf)));
  const long ar = std::lround(std::fabs(rel));
  if (!hasRel)
    std::snprintf(buf, sizeof(buf), "%ld\xC2\xB0 true from you", std::lround(t->bearingDeg) % 360);
  else if (ar <= 7)
    std::snprintf(buf, sizeof(buf), "Straight out from your balcony");
  else if (ar >= 165)
    std::snprintf(buf, sizeof(buf), "Behind your balcony view");
  else
    std::snprintf(buf, sizeof(buf), "%ld\xC2\xB0 from your balcony view", ar);
  sub_.set(buf);
  eta_.set(radar::etaText(t->cue.etaS, buf, sizeof(buf)));
  char name[12], brg[16];
  radar::displayName(a, name, sizeof(name));
  radar::bearingText(t->bearingDeg, brg, sizeof(brg));
  who_.set(std::string(name) + "  \xC2\xB7  " + brg);
  char alt[16];
  radar::withThousands(a.hasAlt ? std::lround(a.altFt / 50.0) * 50 : 0, alt, sizeof(alt));
  std::snprintf(buf, sizeof(buf), "%s FT  \xC2\xB7  %ld KT", a.hasAlt ? alt : kDash, std::lround(a.gsKt));
  data_.set(buf);
  footer_.set(m.exampleData ? "EXAMPLE \xC2\xB7 ESTIMATE ONLY" : "ESTIMATE ONLY");
}

void AlertScreen::tap(int, int) {
  if (actions_) actions_->dismissAlert(hex_.c_str());
  if (nav_ && !hex_.empty()) nav_->openDetail(hex_);
}

// ------------------------------------------------------------------ idle

void IdleScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  dx_ = dy_ = 0;
  hasNext_ = false;
  title_.create(root_, 233, 126, font::bold(20), IdleColors::title, Align::Center, 2);
  count_.create(root_, 233, 184, font::bold(66), IdleColors::count);
  label_.create(root_, 233, 266, font::medium(18), IdleColors::text);
  label_.set("aircraft nearby");
  dot_ = panel(root_, 160, 324, 165, 329, 3, IdleColors::dot, IdleColors::dot, false);
  next_.create(root_, 246, 319, font::medium(16), IdleColors::arrival);
  rule_ = panel(root_, 207, 371, 259, 371, 0, IdleColors::rule, IdleColors::rule, false);
  wake_.create(root_, 233, 389, font::medium(16), IdleColors::text);
  wake_.set("Tap to wake");
  footer(root_, footer_, "DIM IDLE", IdleColors::footer);
}

void IdleScreen::layout(int dx, int dy) {
  dx_ = dx;
  dy_ = dy;
  title_.moveTo(233 + dx, 126 + dy);
  count_.moveTo(233 + dx, 184 + dy);
  label_.moveTo(233 + dx, 266 + dy);
  lv_obj_set_pos(dot_, static_cast<lv_coord_t>(160 + dx), static_cast<lv_coord_t>(324 + dy));
  next_.moveTo((hasNext_ ? 246 : 233) + dx, 319 + dy);
  lv_obj_set_pos(rule_, static_cast<lv_coord_t>(207 + dx), static_cast<lv_coord_t>(371 + dy));
  wake_.moveTo(233 + dx, 389 + dy);
}

void IdleScreen::refresh(const Model& m) {
  // A different resting place every minute spreads the little wear there is.
  static const int8_t kSpots[][2] = {{0, 0}, {9, -12}, {-9, 10}, {11, 7}, {-11, -7}, {0, 14}, {0, -14}};
  const int spot = static_cast<int>((m.nowMs / 60000) % (sizeof(kSpots) / sizeof(kSpots[0])));
  title_.set(m.settings.centerName);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d", std::min(99, m.summary.inRange));
  count_.set(buf);
  const bool hasNext = m.summary.nextArrival >= 0;
  if (hasNext) {
    radar::etaText(m.summary.nextArrivalEtaS, buf, sizeof(buf));
    next_.set("Next arrival " + lower(buf));
  } else {
    next_.set("No arrivals soon");
  }
  const bool moved = hasNext != hasNext_ || kSpots[spot][0] != dx_ || kSpots[spot][1] != dy_;
  hasNext_ = hasNext;
  if (hasNext)
    lv_obj_clear_flag(dot_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
  if (moved) layout(kSpots[spot][0], kSpots[spot][1]);
}

void IdleScreen::tap(int, int) {
  if (actions_) actions_->wake();
}

// ------------------------------------------------------------------ alignment

void AlignmentScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "BALCONY ALIGNMENT");
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    backChevron(s);
    draw::Point cone[3] = {{233, 279}, {198, 232}, {268, 232}};
    draw::polygon(s, cone, 3, col(p.cone));
    draw::dashed(s, 233, 277, 233, 227, 1, 4, 4, col(p.accentDim));
    const float a = runwayRel_ * 3.14159265f / 180.0f;
    const float dx = std::sin(a) * 24, dy = -std::cos(a) * 24;
    draw::segment(s, 233 - dx, 224.5f - dy, 233 + dx, 224.5f + dy, 4, col(p.runway));
    draw::fillCircle(s, 233, 283, 8, lv_color_black());
    draw::ring(s, 233, 283, 7.3f, 1.4f, col(p.observer));
    draw::fillCircle(s, 233, 283, 3, col(p.observer));
  });
  heading_.create(root_, 233, 85, font::bold(29), p.text);
  bearing_.create(root_, 233, 127, font::bold(53), p.accent);
  mode_.create(root_, 233, 188, font::mono(12), p.muted);
  koa_.create(root_, 233, 205, font::mono(11), p.muted);
  distance_.create(root_, 233, 305, font::mono(12), p.muted);
  coords_.create(root_, 233, 337, font::mono(13), p.quiet);
  panel(root_, 110, 365, 356, 410, 12, p.panel, p.panelLine);
  segment_ = panel(root_, 233, 370, 351, 405, 8, p.accentDim, p.accentDim, false);
  north_.create(root_, 172, 381, font::bold(14), p.muted);
  north_.set("NORTH UP");
  view_.create(root_, 292, 381, font::bold(14), p.accent);
  view_.set("VIEW UP");
  footer(root_, footer_, "TAP HERE TO EDIT ON PHONE", p.quiet);
}

void AlignmentScreen::refresh(const Model& m) {
  const Palette& p = palette();
  const radar::Settings& s = m.settings;
  const double facing = s.facingBearing();
  const bool known = !std::isnan(facing);
  char buf[48];
  heading_.set(!s.observerSet() ? "Set your location" : s.viewBearingDeg < 0 ? "Face the airport" : "Your view");
  if (known) {
    std::snprintf(buf, sizeof(buf), "%ld\xC2\xB0", std::lround(facing) % 360);
    bearing_.set(buf);
  } else {
    bearing_.set(kDash);
  }
  mode_.set(s.northUp ? "TRUE BEARING \xC2\xB7 NORTH UP" : "TRUE BEARING \xC2\xB7 VIEW UP");
  koa_.set(s.centerName);

  float runwayRel = 0;
  if (const radar::Place* ap = radar::airportNear(s.center())) {
    if (ap->runwayCount)
      runwayRel = static_cast<float>(radar::bearingDeg(ap->runways[0].threshold[0], ap->runways[0].threshold[1]) -
                                     (known ? facing : 0));
  }
  if (std::fabs(runwayRel - runwayRel_) > 0.5f) {
    runwayRel_ = runwayRel;
    art_.invalidate(190, 190, 276, 260);
  }

  if (s.observerSet()) {
    const double nm = radar::distanceNm(s.observer(), s.center());
    const double rel = known ? radar::wrap180(radar::bearingDeg(s.observer(), s.center()) - facing) : 0;
    if (std::fabs(rel) <= 10) {
      std::snprintf(buf, sizeof(buf), "%.1f NM / %.1f KM AHEAD", nm, nm * 1.852);
    } else {
      char dir[20];
      radar::relativeDirection(rel, dir, sizeof(dir));
      std::snprintf(buf, sizeof(buf), "%.1f NM \xC2\xB7 AIRPORT %s", nm, dir);
    }
    distance_.set(buf);
    std::snprintf(buf, sizeof(buf), "%.6f, %.6f", s.observerLat, s.observerLon);
    coords_.set(buf);
  } else {
    distance_.set("LOCATION NOT SET");
    coords_.set("CENTERED ON THE AIRPORT");
  }

  if (s.northUp != northUp_ || known != known_) {
    northUp_ = s.northUp;
    known_ = known;
  }
  lv_obj_set_pos(segment_, static_cast<lv_coord_t>(s.northUp ? 115 : 233), 370);
  north_.setColor(s.northUp ? p.accent : p.muted);
  view_.setColor(s.northUp ? p.muted : p.accent);
}

void AlignmentScreen::tap(int x, int y) {
  if (onBack(x, y)) {
    if (nav_) nav_->back();
    return;
  }
  if (y >= 358 && y <= 416 && x >= 105 && x <= 361) {
    if (actions_) actions_->setNorthUp(x < 233);
    return;
  }
  if (y >= 418 && nav_) nav_->openPhoneLink();
}

void AlignmentScreen::swipe(Swipe s) {
  if (s == Swipe::Right && nav_) nav_->back();
}

}  // namespace ui
