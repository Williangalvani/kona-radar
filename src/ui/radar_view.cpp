#include "radar_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "format.h"
#include "places.h"
#include "theme.h"
#include "traffic.h"

namespace ui {
namespace {

constexpr int W = 466;
constexpr int H = 466;
constexpr uint32_t kUpdateMs = 250;
constexpr int kIconSteps = 72;

// Shared across screen rebuilds (palette changes recreate the LVGL objects).
lv_color_t* g_layer = nullptr;
draw::Icon* g_icons = nullptr;
bool g_iconReady[kIconSteps] = {};

constexpr float kDeg = 3.14159265358979f / 180.0f;

bool overlaps(const lv_area_t& a, const lv_area_t& b) {
  return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}

int overlapArea(const lv_area_t& a, const lv_area_t& b) {
  const int w = std::min(a.x2, b.x2) - std::max(a.x1, b.x1) + 1;
  const int h = std::min(a.y2, b.y2) - std::max(a.y1, b.y1) + 1;
  return w > 0 && h > 0 ? w * h : 0;
}

void grow(lv_area_t& a, int x, int y) {
  a.x1 = static_cast<lv_coord_t>(std::min<int>(a.x1, x));
  a.y1 = static_cast<lv_coord_t>(std::min<int>(a.y1, y));
  a.x2 = static_cast<lv_coord_t>(std::max<int>(a.x2, x));
  a.y2 = static_cast<lv_coord_t>(std::max<int>(a.y2, y));
}

lv_area_t box(float x, float y, int r) {
  const int cx = static_cast<int>(std::lround(x)), cy = static_cast<int>(std::lround(y));
  return {static_cast<lv_coord_t>(cx - r), static_cast<lv_coord_t>(cy - r),
          static_cast<lv_coord_t>(cx + r), static_cast<lv_coord_t>(cy + r)};
}

bool insideScreen(const lv_area_t& a, float margin) {
  const float r = 233 - margin;
  for (int k = 0; k < 4; ++k) {
    const float px = (k & 1 ? a.x2 : a.x1) - 233.0f, py = (k & 2 ? a.y2 : a.y1) - 233.0f;
    if (px * px + py * py > r * r) return false;
  }
  return true;
}

const lv_font_t* labelFont() { return font::mono(12); }

}  // namespace

bool RadarView::create(lv_obj_t* parent) {
  if (!g_layer) g_layer = static_cast<lv_color_t*>(std::malloc(sizeof(lv_color_t) * W * H));
  if (!g_icons) g_icons = static_cast<draw::Icon*>(std::malloc(sizeof(draw::Icon) * kIconSteps));
  if (!g_layer || !g_icons) return false;
  obj_ = lv_obj_create(parent);
  lv_obj_remove_style_all(obj_);
  lv_obj_set_size(obj_, W, H);
  lv_obj_set_pos(obj_, 0, 0);
  lv_obj_clear_flag(obj_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj_, eventCb, LV_EVENT_ALL, this);
  configured_ = false;
  blips_.clear();
  return true;
}

void RadarView::configure(const radar::Settings& s, bool night) {
  const double up = s.effectiveUpBearing();
  const double facing = s.facingBearing();
  const bool hasFacing = !std::isnan(facing);
  const bool same = configured_ && night == night_ && s.rangeNm == cfg_.rangeNm &&
                    s.centerLat == cfg_.centerLat && s.centerLon == cfg_.centerLon &&
                    s.observerLat == cfg_.observerLat && s.observerLon == cfg_.observerLon &&
                    std::strcmp(s.centerName, cfg_.centerName) == 0 &&
                    std::fabs(up - proj_.upBearingDeg) < 1e-6 && hasFacing == hasFacing_ &&
                    (!hasFacing || std::fabs(facing - facing_) < 1e-6);
  if (same) return;
  cfg_ = s;
  night_ = night;
  facing_ = hasFacing ? facing : 0;
  hasFacing_ = hasFacing;
  proj_.center = s.observer();
  proj_.rangeNm = s.rangeNm;
  proj_.upBearingDeg = up;
  proj_.radiusPx = kRadius;
  proj_.cx = kCx;
  proj_.cy = kCy;
  renderLayer();
  configured_ = true;
  blips_.clear();
  lastUpdateMs_ = 0;
  if (obj_) lv_obj_invalidate(obj_);
}

void RadarView::addStaticText(const char* txt, const lv_font_t* f, uint32_t color, float cx, float top) {
  StaticText t{};
  std::snprintf(t.text, sizeof(t.text), "%s", txt);
  t.font = f;
  t.color = color;
  const int w = lv_txt_get_width(txt, static_cast<uint32_t>(std::strlen(txt)), f, 0, LV_TEXT_FLAG_NONE);
  const int x = static_cast<int>(std::lround(cx - w / 2.0f));
  const int y = static_cast<int>(std::lround(top)) + designTopOffset(f);
  t.area = {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y), static_cast<lv_coord_t>(x + w),
            static_cast<lv_coord_t>(y + f->line_height - 1)};
  for (const lv_area_t& k : keepOut_)
    if (overlaps(t.area, k)) return;
  if (!insideScreen(t.area, 6)) return;
  texts_.push_back(t);
}

void RadarView::renderLayer() {
  const Palette& p = palette();
  draw::Surface s = draw::Surface::fromLayer(g_layer, W, H);
  std::memset(g_layer, 0, sizeof(lv_color_t) * W * H);
  texts_.clear();

  // Coastline (land stays black: no large lit areas on the AMOLED).
  {
    draw::Mask m(W, H);
    for (size_t k = 0; k < radar::kCoastlineCount; ++k) {
      const radar::CoastPolygon& poly = radar::kCoastline[k];
      float px = 0, py = 0;
      for (size_t i = 0; i <= poly.count; ++i) {
        const size_t j = i % poly.count;
        float x, y;
        proj_.project({poly.latlon[2 * j], poly.latlon[2 * j + 1]}, x, y);
        const bool visible = (x > -40 && x < W + 40 && y > -40 && y < H + 40) ||
                             (px > -40 && px < W + 40 && py > -40 && py < H + 40);
        if (i > 0 && visible && std::fabs(x - px) + std::fabs(y - py) < 2000) m.segment(px, py, x, y, 1.2f);
        px = x;
        py = y;
      }
    }
    m.composite(s, col(p.coast));
  }

  for (int k = 1; k <= 3; ++k) draw::ring(s, kCx, kCy, kRadius * k / 3.0f, 1.0f, col(p.grid));
  {
    draw::Mask g(W, H);
    g.dashed(kCx, 87, kCx, 329, 0.7f, 3, 8);
    g.dashed(68, kCy, 398, kCy, 0.7f, 3, 8);
    for (int b = 0; b < 360; b += 15) {
      const float a = static_cast<float>(proj_.screenAngle(b)) * kDeg;
      const float r1 = b % 45 ? 161.0f : 156.0f;
      g.segment(kCx + std::sin(a) * r1, kCy - std::cos(a) * r1, kCx + std::sin(a) * kRadius,
                kCy - std::cos(a) * kRadius, 1.0f);
    }
    g.composite(s, col(p.grid));
  }

  const bool observerSet = cfg_.observerSet();
  auto rotated = [](float a, float x, float y) {
    return draw::Point{kCx + x * std::cos(a) - y * std::sin(a), kCy + x * std::sin(a) + y * std::cos(a)};
  };
  if (hasFacing_) {
    const float a = static_cast<float>(proj_.screenAngle(facing_)) * kDeg;
    if (observerSet) {
      const draw::Point cone[3] = {rotated(a, 0, -13), rotated(a, -16, -36), rotated(a, 16, -36)};
      draw::polygon(s, cone, 3, col(p.cone));
    }
    const draw::Point mark[3] = {rotated(a, 0, -162), rotated(a, -4, -154), rotated(a, 4, -154)};
    draw::polygon(s, mark, 3, col(p.accent));
  }

  // Runways from the real threshold coordinates.
  if (const radar::Place* ap = radar::airportNear(cfg_.center())) {
    const float width = cfg_.rangeNm <= 5 ? 5.0f : (cfg_.rangeNm >= 20 ? 3.0f : 4.0f);
    for (size_t r = 0; r < ap->runwayCount; ++r) {
      const radar::Runway& rw = ap->runways[r];
      float x0, y0, x1, y1;
      proj_.project(rw.threshold[0], x0, y0);
      proj_.project(rw.threshold[1], x1, y1);
      draw::segment(s, x0, y0, x1, y1, width, col(p.runway));
      if (width >= 4) draw::dashed(s, x0, y0, x1, y1, 1.0f, 2, 3, lv_color_black());
      const float dx = x0 - x1, dy = y0 - y1, len = std::max(1.0f, std::hypot(dx, dy));
      const float off = cfg_.rangeNm >= 20 ? 10.0f : 14.0f;
      addStaticText(rw.ident[0], font::mono(10), p.muted, x0 + dx / len * off, y0 + dy / len * off - 7);
      addStaticText(rw.ident[1], font::mono(10), p.muted, x1 - dx / len * off, y1 - dy / len * off - 7);
    }
    float ax, ay;
    proj_.project(ap->pos, ax, ay);
    addStaticText(cfg_.centerName, font::bold(14), p.runway, ax, ay - 27);
  }

  if (observerSet) {
    draw::fillCircle(s, kCx, kCy, 8, lv_color_black());
    draw::ring(s, kCx, kCy, 7.3f, 1.4f, col(p.observer));
    draw::fillCircle(s, kCx, kCy, 3, col(p.observer));
    addStaticText("YOU", font::mono(11), p.observer, kCx, kCy + 15);
  }

  const float na = static_cast<float>(proj_.screenAngle(0)) * kDeg;
  addStaticText("N", font::mono(13), p.northLabel, kCx + std::sin(na) * 182, kCy - std::cos(na) * 182 - 7);
}

const draw::Icon& RadarView::icon(uint8_t idx) {
  idx %= kIconSteps;
  if (!g_iconReady[idx]) {
    draw::renderPlane(g_icons[idx], idx * (360.0f / kIconSteps), 16);
    g_iconReady[idx] = true;
  }
  return g_icons[idx];
}

void RadarView::update(const Model& m, const char* focusHex, bool force) {
  if (!configured_ || !obj_) return;
  const std::string focus = focusHex ? focusHex : "";
  if (!force && focus == focus_ && lastUpdateMs_ != 0 && m.nowMs - lastUpdateMs_ < kUpdateMs) return;
  focus_ = focus;
  lastUpdateMs_ = m.nowMs ? m.nowMs : 1;

  const Palette& p = palette();
  std::vector<Blip> next;
  if (m.targets) {
    for (const radar::Target& g : *m.targets) {
      if (g.distNm > cfg_.rangeNm) continue;
      const radar::Aircraft& a = g.track->ac;
      Blip b{};
      std::snprintf(b.hex, sizeof(b.hex), "%s", a.hex);
      proj_.project(g.pos, b.x, b.y);
      b.kind = a.onGround ? 2 : (g.cue.phase == radar::FlightPhase::Helicopter ? 1 : 0);
      const double rot = a.hasTrack ? proj_.screenAngle(a.trackDeg) : 0;
      b.iconIdx = static_cast<uint8_t>(static_cast<int>(std::lround(rot / (360.0 / kIconSteps))) % kIconSteps);
      b.focus = focus == a.hex;
      const bool departure = g.cue.phase == radar::FlightPhase::Departure;
      b.color = g.stale ? p.mutedTarget
                : b.focus ? p.planeFocus
                : a.emergency ? p.coral
                : departure ? p.planeDeparture
                : b.kind == 2 ? p.quiet
                : p.plane;
      float lastX = b.x, lastY = b.y;
      for (int i = 0; i < g.track->trailCount && b.trailN < 3; ++i) {
        float x, y;
        proj_.project(g.track->trail[i], x, y);
        if (std::hypot(x - b.x, y - b.y) < 15 || std::hypot(x - lastX, y - lastY) < 6) continue;
        if (std::hypot(x - kCx, y - kCy) > kRadius + 4) break;
        b.trail[b.trailN++] = {static_cast<lv_coord_t>(std::lround(x)), static_cast<lv_coord_t>(std::lround(y))};
        lastX = x;
        lastY = y;
      }
      char name[12];
      radar::displayName(a, name, sizeof(name));
      std::snprintf(b.label, sizeof(b.label), "%s%s", name, departure ? " \xE2\x86\x91" : "");
      if (g.cue.phase == radar::FlightPhase::Arrival && !g.stale) {
        b.arrivalLine = true;
        proj_.project(g.cue.threshold, b.tx, b.ty);
      }
      b.dist = static_cast<float>(g.distNm);
      b.priority = b.focus ? 0 : a.emergency ? 1 : g.cue.phase == radar::FlightPhase::Arrival ? 2 : departure ? 3 : 4;
      next.push_back(b);
    }
  }
  placeLabels(next);

  // Airport and runway labels give way to an aircraft icon sitting on them;
  // drawing both leaves neither readable. A wingtip grazing one is fine.
  for (StaticText& t : texts_) {
    const int area = lv_area_get_size(&t.area);
    bool covered = false;
    for (const Blip& b : next) covered |= overlapArea(t.area, box(b.x, b.y, 9)) * 100 >= area * 15;
    if (covered == t.hidden) continue;
    t.hidden = covered;
    lv_area_t a = t.area;
    lv_area_move(&a, obj_->coords.x1, obj_->coords.y1);
    lv_obj_invalidate_area(obj_, &a);
  }

  auto same = [](const Blip& a, const Blip& b) {
    return std::lround(a.x * 2) == std::lround(b.x * 2) && std::lround(a.y * 2) == std::lround(b.y * 2) &&
           a.iconIdx == b.iconIdx && a.kind == b.kind && a.color == b.color && a.focus == b.focus &&
           a.trailN == b.trailN && std::memcmp(a.trail, b.trail, sizeof(lv_point_t) * a.trailN) == 0 &&
           a.showLabel == b.showLabel && std::strcmp(a.label, b.label) == 0 &&
           (!a.showLabel || std::memcmp(&a.labelArea, &b.labelArea, sizeof(lv_area_t)) == 0) &&
           a.arrivalLine == b.arrivalLine;
  };
  auto inval = [&](const Blip& b) {
    lv_area_t a = bounds(b);
    lv_area_move(&a, obj_->coords.x1, obj_->coords.y1);
    lv_obj_invalidate_area(obj_, &a);
  };
  for (const Blip& o : blips_) {
    const Blip* n = nullptr;
    for (const Blip& c : next)
      if (std::strcmp(c.hex, o.hex) == 0) n = &c;
    if (n && same(o, *n)) continue;
    inval(o);
    if (n) inval(*n);
  }
  for (const Blip& n : next) {
    bool existed = false;
    for (const Blip& o : blips_) existed |= std::strcmp(o.hex, n.hex) == 0;
    if (!existed) inval(n);
  }
  blips_.swap(next);
}

void RadarView::placeLabels(std::vector<Blip>& blips) const {
  std::vector<size_t> order(blips.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    if (blips[a].priority != blips[b].priority) return blips[a].priority < blips[b].priority;
    return blips[a].dist < blips[b].dist;
  });
  std::vector<lv_area_t> taken(keepOut_);
  for (const StaticText& t : texts_) taken.push_back(t.area);
  std::vector<lv_area_t> icons;
  for (const Blip& b : blips) icons.push_back(box(b.x, b.y, 9));
  const lv_font_t* f = labelFont();
  for (size_t idx : order) {
    Blip& b = blips[idx];
    b.showLabel = false;
    if (b.kind == 2 && !b.focus) continue;
    lv_point_t sz;
    lv_txt_get_size(&sz, b.label, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int w = sz.x, h = f->line_height;
    const int x = static_cast<int>(std::lround(b.x)), y = static_cast<int>(std::lround(b.y));
    const int off = b.focus ? 25 : 13;
    const int cand[4][2] = {{x - w / 2, y + off}, {x - w / 2, y - off - h}, {x + off, y - h / 2}, {x - off - w, y - h / 2}};
    const lv_area_t* fallback = nullptr;
    lv_area_t areas[4];
    for (int c = 0; c < 4; ++c) {
      lv_area_t a{static_cast<lv_coord_t>(cand[c][0]), static_cast<lv_coord_t>(cand[c][1]),
                  static_cast<lv_coord_t>(cand[c][0] + w), static_cast<lv_coord_t>(cand[c][1] + h - 1)};
      areas[c] = a;
      if (!insideScreen(a, 10)) continue;
      bool clash = false;
      for (const lv_area_t& o : taken) clash |= overlaps(a, o);
      for (size_t k = 0; k < icons.size() && !clash; ++k) clash |= k != idx && overlaps(a, icons[k]);
      if (!clash) {
        b.labelArea = a;
        b.showLabel = true;
        break;
      }
      if (!fallback) fallback = &areas[c];
    }
    if (!b.showLabel && b.focus && fallback) {
      b.labelArea = *fallback;
      b.showLabel = true;
    }
    if (b.showLabel) taken.push_back(b.labelArea);
  }
}

lv_area_t RadarView::bounds(const Blip& b) const {
  lv_area_t a = box(b.x, b.y, b.focus ? 26 : 15);
  for (int i = 0; i < b.trailN; ++i) {
    grow(a, b.trail[i].x - 2, b.trail[i].y - 2);
    grow(a, b.trail[i].x + 2, b.trail[i].y + 2);
  }
  if (b.showLabel) {
    grow(a, b.labelArea.x1, b.labelArea.y1);
    grow(a, b.labelArea.x2 + 1, b.labelArea.y2 + 1);
  }
  if (b.arrivalLine) {
    grow(a, static_cast<int>(b.tx) - 2, static_cast<int>(b.ty) - 2);
    grow(a, static_cast<int>(b.tx) + 2, static_cast<int>(b.ty) + 2);
  }
  return a;
}

std::vector<std::string> RadarView::hitTest(int x, int y) const {
  std::vector<std::pair<float, std::string>> hits;
  for (const Blip& b : blips_) {
    const float dx = b.x - x, dy = b.y - y;
    float d = std::hypot(dx, dy);
    const bool onIcon = std::fabs(dx) <= 22 && std::fabs(dy) <= 22;
    const bool onLabel = b.showLabel && x >= b.labelArea.x1 - 4 && x <= b.labelArea.x2 + 4 &&
                         y >= b.labelArea.y1 - 4 && y <= b.labelArea.y2 + 4;
    if (!onIcon && !onLabel) continue;
    if (onLabel) d = std::min(d, 10.0f);
    hits.push_back({d, b.hex});
  }
  std::sort(hits.begin(), hits.end());
  std::vector<std::string> out;
  for (auto& h : hits) out.push_back(h.second);
  return out;
}

void RadarView::eventCb(lv_event_t* e) {
  auto* self = static_cast<RadarView*>(lv_event_get_user_data(e));
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_DRAW_MAIN) {
    self->drawMain(e);
  } else if (code == LV_EVENT_COVER_CHECK) {
    lv_cover_check_info_t* info = static_cast<lv_cover_check_info_t*>(lv_event_get_param(e));
    if (info->res == LV_COVER_RES_MASKED) return;
    info->res = _lv_area_is_in(info->area, &self->obj_->coords, 0) ? LV_COVER_RES_COVER : LV_COVER_RES_NOT_COVER;
  }
}

void RadarView::drawMain(lv_event_t* e) {
  lv_draw_ctx_t* dc = lv_event_get_draw_ctx(e);
  draw::Surface s = draw::Surface::fromDrawCtx(dc, obj_);
  const Palette& p = palette();

  // Background layer.
  const lv_area_t& clip = *dc->clip_area;
  for (int y = clip.y1; y <= clip.y2; ++y) {
    lv_color_t* dst = s.buf + (y - s.bufArea.y1) * s.stride + (clip.x1 - s.bufArea.x1);
    const int ly = y - s.oy;
    for (int x = clip.x1; x <= clip.x2; ++x, ++dst) {
      const int lx = x - s.ox;
      *dst = (ly >= 0 && ly < H && lx >= 0 && lx < W) ? g_layer[ly * W + lx] : lv_color_black();
    }
  }

  auto toScreen = [&](lv_area_t a) {
    lv_area_move(&a, static_cast<lv_coord_t>(s.ox), static_cast<lv_coord_t>(s.oy));
    return a;
  };
  for (const StaticText& t : texts_) {
    const lv_area_t a = toScreen(t.area);
    if (t.hidden || !overlaps(a, clip)) continue;
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = t.font;
    d.color = col(t.color);
    lv_draw_label(dc, &d, &a, t.text, nullptr);
  }

  for (const Blip& b : blips_) {
    if (!overlaps(toScreen(bounds(b)), clip)) continue;
    const lv_color_t c = col(b.color);
    if (b.arrivalLine) {
      const float dx = b.tx - b.x, dy = b.ty - b.y, len = std::hypot(dx, dy);
      // From just outside the icon, or outside the focus rings as designed.
      const float start = b.focus ? 25.0f : 14.0f;
      if (len > start + 6)
        draw::dashed(s, b.x + dx / len * start, b.y + dy / len * start, b.tx, b.ty, 1.0f, 4, 4, col(p.arrivalLine));
    }
    for (int i = 0; i < b.trailN; ++i) draw::fillCircle(s, b.trail[i].x, b.trail[i].y, 1.2f, col(p.accentDim));
    if (b.focus) {
      draw::ring(s, b.x, b.y, 19, 1.0f, c);
      draw::ring(s, b.x, b.y, 23, 0.8f, col(p.accentDim));
    }
    if (b.kind == 0) {
      const draw::Icon& ic = icon(b.iconIdx);
      const int ix = static_cast<int>(std::lround(b.x)) - draw::Icon::kSize / 2;
      const int iy = static_cast<int>(std::lround(b.y)) - draw::Icon::kSize / 2;
      draw::alphaMap(s, ix, iy, draw::Icon::kSize, draw::Icon::kSize, ic.alpha, c);
    } else if (b.kind == 1) {
      draw::ellipseRing(s, b.x, b.y - 0.5f, 5, 6.5f, 1.5f, c);
      draw::segment(s, b.x - 10, b.y - 2, b.x + 10, b.y - 2, 1.5f, c);
      const draw::Point tail[3] = {{b.x, b.y + 6}, {b.x, b.y + 14}, {b.x + 4, b.y + 14}};
      draw::polyline(s, tail, 3, 1.5f, c);
    } else {
      draw::ring(s, b.x, b.y, 3, 1.2f, c);
    }
    if (b.showLabel) {
      const lv_area_t a = toScreen(b.labelArea);
      lv_draw_label_dsc_t d;
      lv_draw_label_dsc_init(&d);
      d.font = labelFont();
      d.color = c;
      lv_draw_label(dc, &d, &a, b.label, nullptr);
    }
  }
}

}  // namespace ui
