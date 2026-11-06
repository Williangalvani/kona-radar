// Design screens 06, 07 and 10: the observer-centered radar.
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "format.h"
#include "screens.h"
#include "theme.h"

namespace ui {

std::string rangeText(uint16_t nm) {
  char buf[12];
  std::snprintf(buf, sizeof(buf), "%02u NM", nm);
  return buf;
}

const radar::Target* findTarget(const Model& m, const std::string& hex) {
  if (!m.targets || hex.empty()) return nullptr;
  for (const radar::Target& t : *m.targets)
    if (hex == t.track->ac.hex) return &t;
  return nullptr;
}

namespace {

std::string altitudeText(const radar::Aircraft& a, bool arrow) {
  if (a.onGround) return "ON GROUND";
  if (!a.hasAlt) return "\xE2\x80\x94 FT";
  char num[16];
  radar::withThousands(std::lround(a.altFt / 100.0) * 100, num, sizeof(num));
  const int trend = radar::verticalTrend(a);
  std::string s = arrow && trend > 0 ? "\xE2\x86\x91 " : arrow && trend < 0 ? "\xE2\x86\x93 " : "";
  return s + num + " FT";
}

std::string speedText(const radar::Aircraft& a) {
  if (!a.hasGs) return "\xE2\x80\x94 KT";
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%ld KT", std::lround(a.gsKt));
  return buf;
}

}  // namespace

void RadarScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  view_.create(root_);
  view_.setKeepOut({{100, 34, 366, 95},
                    {58, 112, 126, 141},
                    {88, 334, 378, 412},
                    {140, 424, 326, 446},
                    {348, 172, 400, 210}});

  headerBox_ = panel(root_, 100, 34, 366, 91, 8, 0x000000, 0x000000, false);
  title_.create(root_, 233, 41, font::bold(26), p.title, Align::Center, 2);
  mode_.create(root_, 233, 73, font::mono(13), p.accent);
  count_.create(root_, 92, 115, 11);
  outsideCount_.create(root_, 374, 176, font::mono(13), p.amber);
  outsideLabel_.create(root_, 374, 194, font::mono(10), p.muted);
  outsideLabel_.set("OUTSIDE");

  dock_ = panel(root_, 91, 337, 375, 409, 15, p.panel, p.panelLine);
  dockKind_.create(root_, 109, 349, font::mono(11), p.amber, Align::Left, 0);
  dockMain_.create(root_, 109, 370, font::bold(29), p.text, Align::Left);
  dockTopRight_.create(root_, 354, 350, font::medium(16), p.muted, Align::Right);
  dockBottomRight_.create(root_, 354, 377, font::bold(18), p.amber, Align::Right);
  footer(root_, footer_, "", p.quiet);
}

void RadarScreen::refresh(const Model& m) {
  m_ = &m;
  const Palette& p = palette();
  const radar::Settings& s = m.settings;
  view_.configure(s, m.night);

  const radar::Target* user = findTarget(m, userFocus_);
  if (!user || user->distNm > s.rangeNm) {
    userFocus_.clear();
    user = nullptr;
  }
  const radar::Target* focus = user;
  if (!focus && m.targets && m.summary.nextArrival >= 0) {
    const radar::Target& a = (*m.targets)[m.summary.nextArrival];
    if (a.distNm <= s.rangeNm && !a.stale) focus = &a;
  }
  focus_ = focus ? focus->track->ac.hex : "";
  view_.update(m, focus_.c_str());

  title_.set(s.centerName);
  const double facing = s.facingBearing();
  char mode[40];
  if (s.northUp || std::isnan(facing))
    std::snprintf(mode, sizeof(mode), "NORTH UP \xC2\xB7 %s", rangeText(s.rangeNm).c_str());
  else
    std::snprintf(mode, sizeof(mode), "VIEW %ld\xC2\xB0T \xC2\xB7 %s", std::lround(facing) % 360,
                  rangeText(s.rangeNm).c_str());
  mode_.set(mode);

  char buf[24];
  std::snprintf(buf, sizeof(buf), "%d ACFT", m.summary.inRange);
  count_.set(buf, p.muted, p.panel, p.panelLine);
  const bool outside = m.summary.outside > 0;
  std::snprintf(buf, sizeof(buf), "+%d", m.summary.outside);
  outsideCount_.set(buf);
  outsideCount_.show(outside);
  outsideLabel_.show(outside);

  refreshDock(m, focus);

  std::string f;
  uint32_t fc = p.quiet;
  if (m.exampleData) {
    f = "EXAMPLE DATA";
  } else if (!m.online) {
    f = "WI-FI LOST \xC2\xB7 RECONNECTING";
    fc = p.amber;
  } else if (!m.feedEverOk) {
    f = "WAITING FOR DATA";
  } else if (m.feedAgeS > RADAR_STALE_MUTE_S) {
    std::snprintf(buf, sizeof(buf), "STALE %us", static_cast<unsigned>(m.feedAgeS));
    f = buf;
    fc = p.amber;
  } else {
    for (char c : m.feedSource) f += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    f += s.observerSet() ? " \xC2\xB7 LIVE" : " \xC2\xB7 SET LOCATION ON PHONE";
  }
  if (m.night) f = "NIGHT \xC2\xB7 " + f;
  footer_.set(f);
  footer_.setColor(fc);
}

void RadarScreen::refreshDock(const Model& m, const radar::Target* t) {
  const Palette& p = palette();
  char buf[40];
  if (!t) {
    dockKind_.setColor(p.quiet);
    dockMain_.setFont(font::bold(26));
    dockMain_.setColor(p.muted);
    dockTopRight_.set("");
    if (m.summary.inRange == 0) {
      dockKind_.set("QUIET SKY");
      dockMain_.set("No aircraft");
      dockBottomRight_.setFont(font::mono(16));
      dockBottomRight_.setColor(p.quiet);
      dockBottomRight_.set("IN " + rangeText(m.settings.rangeNm));
    } else {
      dockKind_.set("NO ARRIVALS SOON");
      dockMain_.set("Tap a plane");
      dockBottomRight_.set("");
    }
    return;
  }
  const radar::Aircraft& a = t->track->ac;
  char name[12];
  radar::displayName(a, name, sizeof(name));
  char dir[24];
  if (std::isnan(t->relDeg))
    radar::bearingText(t->bearingDeg, dir, sizeof(dir));
  else
    radar::relativeDirection(t->relDeg, dir, sizeof(dir));

  if (t->cue.phase == radar::FlightPhase::Arrival) {
    std::snprintf(buf, sizeof(buf), "APPROACHING %s%s", m.settings.centerName, t->stale ? " \xC2\xB7 STALE" : "");
    dockKind_.set(buf);
    dockKind_.setColor(p.amber);
    dockMain_.setFont(font::bold(29));
    dockMain_.setColor(p.night ? p.amber : p.text);
    radar::etaText(t->cue.etaS, buf, sizeof(buf));
    dockMain_.set(buf);
    dockTopRight_.set(name);
    dockTopRight_.setColor(p.muted);
    dockBottomRight_.setFont(font::bold(18));
    dockBottomRight_.setColor(p.amber);
    dockBottomRight_.set(dir);
    return;
  }

  dockMain_.setFont(font::bold(26));
  dockMain_.setColor(p.text);
  dockMain_.set(name);
  dockBottomRight_.setFont(font::mono(16));
  dockBottomRight_.setColor(p.muted);
  if (t->cue.phase == radar::FlightPhase::Departure) {
    dockKind_.set("DEPARTING");
    dockKind_.setColor(p.sage);
    dockTopRight_.set(altitudeText(a, true));
    dockTopRight_.setColor(p.sage);
    dockBottomRight_.set(speedText(a));
    return;
  }
  if (a.emergency) {
    std::snprintf(buf, sizeof(buf), "EMERGENCY \xC2\xB7 %s", a.squawk);
    dockKind_.set(buf);
    dockKind_.setColor(p.coral);
  } else {
    dockKind_.set(t->stale ? "STALE"
                  : t->cue.phase == radar::FlightPhase::Helicopter ? "HELICOPTER"
                  : t->cue.phase == radar::FlightPhase::Ground     ? "ON GROUND"
                                                                   : "TRAFFIC");
    dockKind_.setColor(p.muted);
  }
  dockTopRight_.set(altitudeText(a, true));
  dockTopRight_.setColor(p.muted);
  dockBottomRight_.set(dir);
}

void RadarScreen::cycleRange(int dir) {
  if (!m_ || !actions_) return;
  size_t n;
  const uint16_t* steps = radar::rangeSteps(n);
  const size_t i = radar::nearestRangeStep(m_->settings.rangeNm);
  size_t j;
  if (dir > 0)
    j = (i + 1) % n;
  else
    j = i == 0 ? 0 : i - 1;
  if (dir == 2) j = i + 1 < n ? i + 1 : i;
  if (steps[j] != m_->settings.rangeNm) actions_->setRange(steps[j]);
}

void RadarScreen::tap(int x, int y) {
  if (y >= 30 && y <= 100 && x >= 100 && x <= 366) {
    if (x >= 255)
      cycleRange(1);
    else if (nav_)
      nav_->openAlignment();
    return;
  }
  if (x >= 91 && x <= 375 && y >= 337 && y <= 409) {
    if (!focus_.empty() && nav_) nav_->openDetail(focus_);
    return;
  }
  const std::vector<std::string> hits = view_.hitTest(x, y);
  if (hits.empty()) {
    userFocus_.clear();
    return;
  }
  for (size_t i = 0; i < hits.size(); ++i) {
    if (hits[i] != focus_) continue;
    if (hits.size() == 1) {
      if (nav_) nav_->openDetail(focus_);
    } else {
      userFocus_ = hits[(i + 1) % hits.size()];
    }
    return;
  }
  userFocus_ = hits[0];
}

void RadarScreen::swipe(Swipe s) {
  switch (s) {
    case Swipe::Left:
      cycleRange(2);
      break;
    case Swipe::Right:
      cycleRange(-1);
      break;
    case Swipe::Up:
      if (actions_) actions_->toggleNight();
      break;
    case Swipe::Down:
      if (actions_) actions_->goIdle();
      break;
  }
}

}  // namespace ui
