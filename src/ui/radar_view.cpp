#include "radar_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app_clock.h"
#include "config.h"
#include "format.h"
#include "places.h"
#include "theme.h"
#include "tracker.h"

namespace ui {
namespace {

constexpr int W = 466;
constexpr int H = 466;
constexpr uint16_t kNoSweep = 0xFFFF;
constexpr uint32_t kBlipUpdateMs = 200;

inline uint16_t rgb565(uint32_t rgb) {
  return static_cast<uint16_t>(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

inline uint16_t mix565(uint16_t a, uint16_t b, int alpha) {
  if (alpha <= 0) return a;
  if (alpha >= 255) return b;
  const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  const int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  const int r = ar + ((br - ar) * alpha + 127) / 255;
  const int g = ag + ((bg - ag) * alpha + 127) / 255;
  const int bl = ab + ((bb - ab) * alpha + 127) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

inline uint16_t add565(uint16_t a, uint16_t b) {
  int r = (a >> 11) + (b >> 11);
  int g = ((a >> 5) & 63) + ((b >> 5) & 63);
  int bl = (a & 31) + (b & 31);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (bl > 31) bl = 31;
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

inline void storePixel(lv_color_t& dst, uint16_t c) {
#if LV_COLOR_16_SWAP
  dst.full = static_cast<uint16_t>((c >> 8) | (c << 8));
#else
  dst.full = c;
#endif
}

inline int aa(float coverage) {
  if (coverage <= 0) return 0;
  if (coverage >= 1) return 255;
  return static_cast<int>(coverage * 255.0f);
}

// Anti-aliased stroke of a segment into an 8-bit coverage buffer.
void strokeSegment(uint8_t* buf, float x0, float y0, float x1, float y1, float width) {
  const float pad = width * 0.5f + 1.0f;
  int bx0 = static_cast<int>(std::floor(std::min(x0, x1) - pad));
  int bx1 = static_cast<int>(std::ceil(std::max(x0, x1) + pad));
  int by0 = static_cast<int>(std::floor(std::min(y0, y1) - pad));
  int by1 = static_cast<int>(std::ceil(std::max(y0, y1) + pad));
  if (bx1 < 0 || by1 < 0 || bx0 >= W || by0 >= H) return;
  bx0 = std::max(bx0, 0);
  by0 = std::max(by0, 0);
  bx1 = std::min(bx1, W - 1);
  by1 = std::min(by1, H - 1);
  const float vx = x1 - x0, vy = y1 - y0;
  const float len2 = vx * vx + vy * vy;
  for (int y = by0; y <= by1; ++y) {
    for (int x = bx0; x <= bx1; ++x) {
      const float px = x + 0.5f - x0, py = y + 0.5f - y0;
      float t = len2 > 0 ? (px * vx + py * vy) / len2 : 0;
      t = t < 0 ? 0 : (t > 1 ? 1 : t);
      const float ex = px - t * vx, ey = py - t * vy;
      const float d = std::sqrt(ex * ex + ey * ey);
      const int a = aa(width * 0.5f + 0.5f - d);
      uint8_t& dst = buf[y * W + x];
      if (a > dst) dst = static_cast<uint8_t>(a);
    }
  }
}

// Even-odd scanline fill of a polygon into a 0/1 mask (XOR so that holes
// and overlapping rings behave).
void fillPolygon(uint8_t* mask, const std::vector<float>& xs, const std::vector<float>& ys) {
  const size_t n = xs.size();
  if (n < 3) return;
  float minY = ys[0], maxY = ys[0];
  for (float y : ys) {
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
  }
  const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
  const int y1 = std::min(H - 1, static_cast<int>(std::ceil(maxY)));
  std::vector<float> hits;
  for (int y = y0; y <= y1; ++y) {
    const float sy = y + 0.5f;
    hits.clear();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
      if ((ys[i] > sy) != (ys[j] > sy)) {
        hits.push_back(xs[i] + (sy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]));
      }
    }
    std::sort(hits.begin(), hits.end());
    for (size_t k = 0; k + 1 < hits.size(); k += 2) {
      int xa = static_cast<int>(std::ceil(hits[k] - 0.5f));
      int xb = static_cast<int>(std::floor(hits[k + 1] - 0.5f));
      xa = std::max(xa, 0);
      xb = std::min(xb, W - 1);
      for (int x = xa; x <= xb; ++x) mask[y * W + x] ^= 1;
    }
  }
}

void textSize(const char* txt, const lv_font_t* font, lv_point_t& out) {
  lv_txt_get_size(&out, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
}

void drawText(lv_draw_ctx_t* dc, const lv_area_t& a, const char* txt, uint32_t color,
              const lv_font_t* font, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.color = col(color);
  d.font = font;
  d.opa = opa;
  lv_draw_label(dc, &d, &a, txt, nullptr);
}

void drawLine(lv_draw_ctx_t* dc, lv_coord_t x0, lv_coord_t y0, lv_coord_t x1, lv_coord_t y1,
              lv_color_t c, lv_coord_t w, lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = w;
  d.opa = opa;
  d.round_start = 1;
  d.round_end = 1;
  lv_point_t a{x0, y0}, b{x1, y1};
  lv_draw_line(dc, &d, &a, &b);
}

void drawDot(lv_draw_ctx_t* dc, float x, float y, int r, lv_color_t c, lv_opa_t opa,
             int border = 0, lv_color_t borderColor = lv_color_black()) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = c;
  d.bg_opa = opa;
  if (border) {
    d.border_width = static_cast<lv_coord_t>(border);
    d.border_color = borderColor;
    d.border_opa = LV_OPA_COVER;
  }
  const lv_coord_t cx = static_cast<lv_coord_t>(std::lround(x));
  const lv_coord_t cy = static_cast<lv_coord_t>(std::lround(y));
  lv_area_t a{static_cast<lv_coord_t>(cx - r), static_cast<lv_coord_t>(cy - r),
              static_cast<lv_coord_t>(cx + r), static_cast<lv_coord_t>(cy + r)};
  lv_draw_rect(dc, &d, &a);
}

bool areasOverlap(const lv_area_t& a, const lv_area_t& b) {
  return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}

void growArea(lv_area_t& a, lv_coord_t x, lv_coord_t y) {
  a.x1 = std::min(a.x1, x);
  a.y1 = std::min(a.y1, y);
  a.x2 = std::max(a.x2, x);
  a.y2 = std::max(a.y2, y);
}

}  // namespace

double ringStepNm(double rangeNm) {
  static const double kSteps[] = {0.5, 1, 2, 2.5, 5, 10, 20, 25, 50, 100};
  for (double s : kSteps) {
    if (rangeNm / s <= 5.0 + 1e-9) return s;
  }
  return rangeNm / 4;
}

bool RadarView::create(lv_obj_t* parent) {
  layer_ = static_cast<Px*>(std::malloc(sizeof(Px) * W * H));
  if (!layer_) return false;
  std::memset(layer_, 0, sizeof(Px) * W * H);
  obj_ = lv_obj_create(parent);
  lv_obj_remove_style_all(obj_);
  lv_obj_set_size(obj_, W, H);
  lv_obj_set_pos(obj_, 0, 0);
  lv_obj_clear_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj_, eventCb, LV_EVENT_ALL, this);

  const Theme& t = theme();
  trailLen_ = static_cast<uint16_t>(t.sweepTrailDeg / 360.0f * 4096.0f);
  const uint32_t tc = t.sweepTrail;
  for (int d = 0; d < trailLen_; ++d) {
    const float f = 1.0f - static_cast<float>(d) / trailLen_;
    const float k = t.sweepTrailMax * f * f;
    const uint32_t r = static_cast<uint32_t>(((tc >> 16) & 0xFF) * k);
    const uint32_t g = static_cast<uint32_t>(((tc >> 8) & 0xFF) * k);
    const uint32_t b = static_cast<uint32_t>((tc & 0xFF) * k);
    lut_[d] = rgb565((r << 16) | (g << 8) | b);
  }
  return true;
}

void RadarView::configure(const radar::Settings& s, int shiftX, int shiftY) {
  const double up = s.effectiveUpBearing();
  const bool same = configured_ && s.rangeNm == cfg_.rangeNm && s.centerLat == cfg_.centerLat &&
                    s.centerLon == cfg_.centerLon && s.observerLat == cfg_.observerLat &&
                    s.observerLon == cfg_.observerLon &&
                    std::strcmp(s.centerName, cfg_.centerName) == 0 &&
                    std::fabs(up - proj_.upBearingDeg) < 1e-6 && shiftX == shiftX_ &&
                    shiftY == shiftY_;
  if (same) return;
  cfg_ = s;
  shiftX_ = shiftX;
  shiftY_ = shiftY;
  proj_.center = s.center();
  proj_.rangeNm = s.rangeNm;
  proj_.upBearingDeg = up;
  proj_.radiusPx = static_cast<float>(theme().scopeRadius);
  proj_.cx = W / 2.0f + shiftX;
  proj_.cy = H / 2.0f + shiftY;
  buildLayer();
  buildAnnotations();
  configured_ = true;
  lastBlipUpdateMs_ = 0;
  blips_.clear();
  if (obj_) lv_obj_invalidate(obj_);
}

void RadarView::buildLayer() {
  const Theme& t = theme();
  const float R = proj_.radiusPx;
  const float cx = proj_.cx, cy = proj_.cy;
  std::vector<uint8_t> land(W * H, 0), coast(W * H, 0), grid(W * H, 0);

  std::vector<float> xs, ys;
  for (size_t p = 0; p < radar::kCoastlineCount; ++p) {
    const radar::CoastPolygon& poly = radar::kCoastline[p];
    xs.resize(poly.count);
    ys.resize(poly.count);
    for (size_t i = 0; i < poly.count; ++i) {
      proj_.project({poly.latlon[2 * i], poly.latlon[2 * i + 1]}, xs[i], ys[i]);
    }
    fillPolygon(land.data(), xs, ys);
    for (size_t i = 0, j = poly.count - 1; i < poly.count; j = i++) {
      strokeSegment(coast.data(), xs[j], ys[j], xs[i], ys[i], t.coastWidth);
    }
  }

  // Bearing ticks every 10 degrees (true), longer every 30.
  for (int deg = 0; deg < 360; deg += 10) {
    const double a = radar::deg2rad(proj_.screenAngle(deg));
    const float len = deg % 30 == 0 ? 12.0f : 6.0f;
    const float sx = static_cast<float>(std::sin(a)), sy = static_cast<float>(-std::cos(a));
    strokeSegment(grid.data(), cx + sx * (R - len), cy + sy * (R - len), cx + sx * R, cy + sy * R,
                  deg % 90 == 0 ? 2.0f : 1.2f);
  }

  const double step = ringStepNm(proj_.rangeNm);
  std::vector<float> rings;
  for (double d = step; d < proj_.rangeNm - 1e-6; d += step) {
    rings.push_back(static_cast<float>(d / proj_.rangeNm * R));
  }

  const uint16_t cCenter = rgb565(t.scopeCenter), cEdge = rgb565(t.scopeEdge);
  const uint16_t cLand = rgb565(t.land), cCoast = rgb565(t.coast), cRing = rgb565(t.ring);
  const uint16_t cOuter = rgb565(t.ringOuter), cTick = rgb565(t.tick);
  const float outerHalf = t.outerRingWidth * 0.5f;

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float r = std::sqrt(dx * dx + dy * dy);
      Px& px = layer_[y * W + x];
      const float outerCov = outerHalf + 0.5f - std::fabs(r - R);
      if (r > R) {
        px.rgb = mix565(0, cOuter, aa(outerCov));
        px.angle = kNoSweep;
        continue;
      }
      uint16_t c = mix565(cCenter, cEdge, static_cast<int>(r / R * 255.0f));
      const int i = y * W + x;
      if (land[i]) c = cLand;
      c = mix565(c, cCoast, coast[i]);
      for (float rr : rings) {
        const float cov = t.ringWidth * 0.5f + 0.5f - std::fabs(r - rr);
        if (cov > 0) c = mix565(c, cRing, aa(cov));
      }
      c = mix565(c, cTick, grid[i]);
      c = mix565(c, cOuter, aa(outerCov));
      px.rgb = c;
      float a = std::atan2(dx, -dy);
      if (a < 0) a += 2.0f * static_cast<float>(radar::kPi);
      px.angle = static_cast<uint16_t>(static_cast<int>(a * (4096.0f / (2.0f * static_cast<float>(radar::kPi)))) & 4095);
    }
  }
}

void RadarView::buildAnnotations() {
  const Theme& t = theme();
  notes_.clear();
  const float R = proj_.radiusPx;
  auto add = [&](float x, float y, const char* txt, uint32_t color, const lv_font_t* font,
                 uint8_t kind, int dxText, int dyText, bool centered) {
    Annotation n{};
    std::snprintf(n.text, sizeof(n.text), "%s", txt);
    n.color = color;
    n.font = font;
    n.kind = kind;
    n.anchor = {static_cast<lv_coord_t>(std::lround(x)), static_cast<lv_coord_t>(std::lround(y))};
    lv_point_t sz{0, 0};
    if (txt[0]) textSize(txt, font, sz);
    lv_coord_t tx = static_cast<lv_coord_t>(n.anchor.x + dxText - (centered ? sz.x / 2 : 0));
    lv_coord_t ty = static_cast<lv_coord_t>(n.anchor.y + dyText - (centered ? sz.y / 2 : 0));
    n.textArea = {tx, ty, static_cast<lv_coord_t>(tx + sz.x), static_cast<lv_coord_t>(ty + sz.y)};
    n.area = n.textArea;
    if (kind != 0) {
      growArea(n.area, static_cast<lv_coord_t>(n.anchor.x - 8), static_cast<lv_coord_t>(n.anchor.y - 8));
      growArea(n.area, static_cast<lv_coord_t>(n.anchor.x + 8), static_cast<lv_coord_t>(n.anchor.y + 8));
    }
    notes_.push_back(n);
  };

  // Cardinal letters just inside the ticks.
  const char* names[] = {"N", "E", "S", "W"};
  for (int k = 0; k < 4; ++k) {
    float x, y;
    proj_.polar(proj_.rangeNm * (R - 24) / R, k * 90.0, x, y);
    add(x, y, names[k], t.cardinal, t.fontBody, 0, 0, 0, true);
  }

  // Ring distances along the lower-left diagonal of the screen.
  const double step = ringStepNm(proj_.rangeNm);
  const double labelBearing = proj_.upBearingDeg + 225.0;
  for (double d = step; d <= proj_.rangeNm + 1e-6; d += step) {
    float x, y;
    proj_.polar(d, labelBearing, x, y);
    char buf[16];
    if (d + 1e-6 >= proj_.rangeNm) {
      std::snprintf(buf, sizeof(buf), "%.4g NM", d);
      proj_.polar(d - step * 0.25, labelBearing, x, y);
    } else {
      std::snprintf(buf, sizeof(buf), "%.4g", d);
    }
    add(x + 4, y - 4, buf, t.ringLabel, t.fontSmall, 0, 0, 0, true);
  }

  // Other airports inside the outer ring.
  for (size_t i = 0; i < radar::kAirportCount; ++i) {
    const radar::Place& p = radar::kAirports[i];
    double d;
    float x, y;
    proj_.project(p.pos, x, y, &d);
    if (d < 0.5 || d > proj_.rangeNm * 0.97) continue;
    add(x, y, p.code, t.airport, t.fontSmall, 1, 7, -8, false);
  }

  // Observer and center.
  {
    double d;
    float x, y;
    proj_.project(cfg_.observer(), x, y, &d);
    if (d <= proj_.rangeNm * 0.97 && d * proj_.pxPerNm() > 12) {
      add(x, y, "YOU", t.observer, t.fontSmall, 2, 10, -8, false);
    }
  }
  lv_point_t sz;
  textSize(cfg_.centerName, t.fontSmall, sz);
  add(proj_.cx, proj_.cy, cfg_.centerName, t.centerMark, t.fontSmall, 3,
      -8 - sz.x, -6 - sz.y, false);
}

lv_area_t RadarView::blipBounds(const Blip& b) {
  const lv_coord_t x = static_cast<lv_coord_t>(std::lround(b.x));
  const lv_coord_t y = static_cast<lv_coord_t>(std::lround(b.y));
  const lv_coord_t r = static_cast<lv_coord_t>(b.isSelected ? 14 : theme().blipRadius + 4);
  lv_area_t a{static_cast<lv_coord_t>(x - r), static_cast<lv_coord_t>(y - r),
              static_cast<lv_coord_t>(x + r), static_cast<lv_coord_t>(y + r)};
  if (b.leaderPx > 0) {
    growArea(a, static_cast<lv_coord_t>(std::lround(b.x + b.dx * b.leaderPx)),
             static_cast<lv_coord_t>(std::lround(b.y + b.dy * b.leaderPx)));
  }
  for (int i = 0; i < b.trailN; ++i) growArea(a, b.trail[i].x, b.trail[i].y);
  if (b.showLabel) {
    growArea(a, b.labelArea.x1, b.labelArea.y1);
    growArea(a, b.labelArea.x2, b.labelArea.y2);
  }
  a.x1 = static_cast<lv_coord_t>(a.x1 - 3);
  a.y1 = static_cast<lv_coord_t>(a.y1 - 3);
  a.x2 = static_cast<lv_coord_t>(a.x2 + 3);
  a.y2 = static_cast<lv_coord_t>(a.y2 + 3);
  return a;
}

void RadarView::updateBlips(uint32_t nowMs) {
  const Theme& t = theme();
  std::vector<Blip> next;
  if (model_ && model_->tracker) {
    const float R = proj_.radiusPx;
    for (const radar::Track& tr : model_->tracker->tracks()) {
      const radar::LatLon pos = radar::Tracker::predict(tr, nowMs);
      Blip b{};
      double dist, brg;
      proj_.project(pos, b.x, b.y, &dist, &brg);
      if (dist > proj_.rangeNm * 1.01) continue;
      std::snprintf(b.hex, sizeof(b.hex), "%s", tr.ac.hex);
      b.distNm = dist;
      b.ground = tr.ac.onGround;
      b.emergency = tr.ac.emergency;
      b.isSelected = std::strcmp(selected_, b.hex) == 0;
      const double sa = proj_.screenAngle(brg);
      b.angle12 = static_cast<uint16_t>(static_cast<int>(sa / 360.0 * 4096.0) & 4095);
      if (tr.ac.hasTrack && tr.ac.hasGs && !b.ground && tr.ac.gsKt > 30) {
        const double a = radar::deg2rad(proj_.screenAngle(tr.ac.trackDeg));
        b.dx = static_cast<float>(std::sin(a));
        b.dy = static_cast<float>(-std::cos(a));
        // Leader shows where the aircraft will be in one minute.
        b.leaderPx = std::min(70.0f, std::max(7.0f, tr.ac.gsKt / 60.0f * proj_.pxPerNm()));
      }
      for (int i = 0; i < tr.trailCount; ++i) {
        float x, y;
        proj_.project(tr.trail[i], x, y);
        const float ddx = x - proj_.cx, ddy = y - proj_.cy;
        if (ddx * ddx + ddy * ddy > R * R) break;
        b.trail[b.trailN++] = {static_cast<lv_coord_t>(std::lround(x)),
                               static_cast<lv_coord_t>(std::lround(y))};
      }
      radar::displayName(tr.ac, b.line1, sizeof(b.line1));
      char alt[20];
      radar::formatAltitude(tr.ac, alt, sizeof(alt));
      const int trend = radar::verticalTrend(tr.ac);
      std::snprintf(b.line2, sizeof(b.line2), "%s%s", alt,
                    trend > 0 ? " " LV_SYMBOL_UP : (trend < 0 ? " " LV_SYMBOL_DOWN : ""));
      next.push_back(b);
    }
  }

  // Label placement: most important first, four candidate corners, skip
  // labels that would collide or leave the scope.
  std::vector<size_t> order(next.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    if (next[a].isSelected != next[b].isSelected) return next[a].isSelected;
    if (next[a].emergency != next[b].emergency) return next[a].emergency;
    if (next[a].ground != next[b].ground) return next[b].ground;
    return next[a].distNm < next[b].distNm;
  });
  std::vector<lv_area_t> taken;
  for (const Blip& b : next) {
    const lv_coord_t x = static_cast<lv_coord_t>(std::lround(b.x));
    const lv_coord_t y = static_cast<lv_coord_t>(std::lround(b.y));
    taken.push_back({static_cast<lv_coord_t>(x - 5), static_cast<lv_coord_t>(y - 5),
                     static_cast<lv_coord_t>(x + 5), static_cast<lv_coord_t>(y + 5)});
  }
  for (const Annotation& n : notes_) taken.push_back(n.area);
  for (lv_area_t k : keepOut_) {
    lv_area_move(&k, static_cast<lv_coord_t>(shiftX_), static_cast<lv_coord_t>(shiftY_));
    taken.push_back(k);
  }
  const float R = proj_.radiusPx;
  for (size_t idx : order) {
    Blip& b = next[idx];
    lv_point_t s1, s2;
    textSize(b.line1, t.fontLabel, s1);
    textSize(b.line2, t.fontLabel, s2);
    const lv_coord_t w = std::max(s1.x, s2.x), h = static_cast<lv_coord_t>(s1.y + s2.y);
    const lv_coord_t x = static_cast<lv_coord_t>(std::lround(b.x));
    const lv_coord_t y = static_cast<lv_coord_t>(std::lround(b.y));
    const lv_coord_t cand[4][2] = {{static_cast<lv_coord_t>(x + 9), static_cast<lv_coord_t>(y - h - 3)},
                                   {static_cast<lv_coord_t>(x + 9), static_cast<lv_coord_t>(y + 5)},
                                   {static_cast<lv_coord_t>(x - 9 - w), static_cast<lv_coord_t>(y - h - 3)},
                                   {static_cast<lv_coord_t>(x - 9 - w), static_cast<lv_coord_t>(y + 5)}};
    b.showLabel = false;
    for (const auto& c : cand) {
      lv_area_t a{c[0], c[1], static_cast<lv_coord_t>(c[0] + w), static_cast<lv_coord_t>(c[1] + h)};
      bool inside = true;
      for (int k = 0; k < 4 && inside; ++k) {
        const float px = (k & 1 ? a.x2 : a.x1) - proj_.cx, py = (k & 2 ? a.y2 : a.y1) - proj_.cy;
        inside = px * px + py * py < (R - 3) * (R - 3);
      }
      if (!inside) continue;
      bool clash = false;
      for (const lv_area_t& o : taken) {
        if (areasOverlap(a, o)) {
          // A blip's own dot does not block its label.
          if (o.x1 == x - 5 && o.y1 == y - 5) continue;
          clash = true;
          break;
        }
      }
      if (clash && !b.isSelected) continue;
      b.labelArea = a;
      b.showLabel = true;
      taken.push_back(a);
      break;
    }
  }

  // Invalidate only what changed.
  for (const Blip& old : blips_) {
    const Blip* match = nullptr;
    for (const Blip& n : next) {
      if (std::strcmp(n.hex, old.hex) == 0) {
        match = &n;
        break;
      }
    }
    if (match && std::fabs(match->x - old.x) < 0.5f && std::fabs(match->y - old.y) < 0.5f &&
        match->showLabel == old.showLabel && match->isSelected == old.isSelected &&
        match->trailN == old.trailN && std::strcmp(match->line2, old.line2) == 0 &&
        (!match->showLabel || std::memcmp(&match->labelArea, &old.labelArea, sizeof(lv_area_t)) == 0)) {
      continue;
    }
    lv_area_t a = blipBounds(old);
    lv_obj_invalidate_area(obj_, &a);
    if (match) {
      lv_area_t b = blipBounds(*match);
      lv_obj_invalidate_area(obj_, &b);
    }
  }
  for (Blip& n : next) {
    bool existed = false;
    for (const Blip& old : blips_) {
      if (std::strcmp(n.hex, old.hex) == 0) {
        existed = true;
        n.glowLevel = old.glowLevel;
        break;
      }
    }
    if (!existed || selectionDirty_) {
      lv_area_t a = blipBounds(n);
      lv_obj_invalidate_area(obj_, &a);
    }
  }
  selectionDirty_ = false;
  blips_.swap(next);
}

void RadarView::invalidateSweep(uint16_t from12, uint16_t to12) {
  const float R = proj_.radiusPx + 3;
  const uint16_t span = static_cast<uint16_t>((to12 - from12) & 4095);
  lv_area_t a{static_cast<lv_coord_t>(proj_.cx), static_cast<lv_coord_t>(proj_.cy),
              static_cast<lv_coord_t>(proj_.cx), static_cast<lv_coord_t>(proj_.cy)};
  auto addAngle = [&](uint16_t ang) {
    const float rad = ang * (2.0f * static_cast<float>(radar::kPi) / 4096.0f);
    growArea(a, static_cast<lv_coord_t>(std::floor(proj_.cx + R * std::sin(rad))),
             static_cast<lv_coord_t>(std::floor(proj_.cy - R * std::cos(rad))));
    growArea(a, static_cast<lv_coord_t>(std::ceil(proj_.cx + R * std::sin(rad))),
             static_cast<lv_coord_t>(std::ceil(proj_.cy - R * std::cos(rad))));
  };
  addAngle(from12);
  addAngle(to12);
  for (uint16_t q = 0; q < 4096; q += 1024) {
    if (static_cast<uint16_t>((q - from12) & 4095) <= span) addAngle(q);
  }
  a.x1 = static_cast<lv_coord_t>(a.x1 - 2);
  a.y1 = static_cast<lv_coord_t>(a.y1 - 2);
  a.x2 = static_cast<lv_coord_t>(a.x2 + 2);
  a.y2 = static_cast<lv_coord_t>(a.y2 + 2);
  lv_obj_invalidate_area(obj_, &a);
}

void RadarView::tick(uint32_t nowMs) {
  if (!configured_ || !obj_) return;
  if (animating_) {
    const uint32_t period = RADAR_SWEEP_PERIOD_S * 1000UL;
    const uint32_t frameMs = 1000UL / RADAR_SWEEP_FPS;
    const uint32_t t = nowMs - nowMs % frameMs;
    const uint16_t next = static_cast<uint16_t>(((t % period) * 4096ULL / period) & 4095);
    if (next != sweep_) {
      invalidateSweep(static_cast<uint16_t>((sweep_ - trailLen_ - 8) & 4095),
                      static_cast<uint16_t>((next + 8) & 4095));
      sweep_ = next;
    }
  }
  if (lastBlipUpdateMs_ == 0 || nowMs - lastBlipUpdateMs_ >= kBlipUpdateMs || selectionDirty_) {
    lastBlipUpdateMs_ = nowMs;
    updateBlips(nowMs);
  }
  // Afterglow: blips are brightest right after the beam passes.
  const int r = theme().blipRadius + 3;
  for (Blip& b : blips_) {
    const uint16_t age = static_cast<uint16_t>((sweep_ - b.angle12) & 4095);
    const uint8_t level = static_cast<uint8_t>(7 - age * 8 / 4096);
    if (level != b.glowLevel) {
      b.glowLevel = level;
      const lv_coord_t x = static_cast<lv_coord_t>(std::lround(b.x));
      const lv_coord_t y = static_cast<lv_coord_t>(std::lround(b.y));
      lv_area_t a{static_cast<lv_coord_t>(x - r), static_cast<lv_coord_t>(y - r),
                  static_cast<lv_coord_t>(x + r), static_cast<lv_coord_t>(y + r)};
      lv_obj_invalidate_area(obj_, &a);
    }
  }
}

void RadarView::setAnimating(bool on) {
  if (on == animating_) return;
  animating_ = on;
  if (obj_) lv_obj_invalidate(obj_);
}

const char* RadarView::hitTest(lv_coord_t x, lv_coord_t y) const {
  const Blip* best = nullptr;
  float bestD = static_cast<float>(theme().hitRadius);
  for (const Blip& b : blips_) {
    float d = std::hypot(b.x - x, b.y - y);
    if (b.showLabel && x >= b.labelArea.x1 && x <= b.labelArea.x2 && y >= b.labelArea.y1 &&
        y <= b.labelArea.y2) {
      d = std::min(d, 8.0f);
    }
    if (d < bestD) {
      bestD = d;
      best = &b;
    }
  }
  return best ? best->hex : nullptr;
}

void RadarView::setSelected(const char* hex) {
  const char* h = hex ? hex : "";
  if (std::strcmp(h, selected_) == 0) return;
  std::snprintf(selected_, sizeof(selected_), "%s", h);
  selectionDirty_ = true;
  for (const Blip& b : blips_) {
    lv_area_t a = blipBounds(b);
    a.x1 = static_cast<lv_coord_t>(a.x1 - 12);
    a.y1 = static_cast<lv_coord_t>(a.y1 - 12);
    a.x2 = static_cast<lv_coord_t>(a.x2 + 12);
    a.y2 = static_cast<lv_coord_t>(a.y2 + 12);
    lv_obj_invalidate_area(obj_, &a);
  }
}

void RadarView::eventCb(lv_event_t* e) {
  auto* self = static_cast<RadarView*>(lv_event_get_user_data(e));
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_DRAW_MAIN) {
    self->drawMain(e);
  } else if (code == LV_EVENT_COVER_CHECK) {
    lv_event_set_cover_res(e, LV_COVER_RES_COVER);
  }
}

void RadarView::drawMain(lv_event_t* e) {
  lv_draw_ctx_t* dc = lv_event_get_draw_ctx(e);
  composeBackground(dc);
  drawAnnotations(dc);
  if (animating_) {
    const float rad = sweep_ * (2.0f * static_cast<float>(radar::kPi) / 4096.0f);
    const float R = proj_.radiusPx - 1;
    drawLine(dc, static_cast<lv_coord_t>(proj_.cx), static_cast<lv_coord_t>(proj_.cy),
             static_cast<lv_coord_t>(std::lround(proj_.cx + R * std::sin(rad))),
             static_cast<lv_coord_t>(std::lround(proj_.cy - R * std::cos(rad))),
             col(theme().sweepBeam), 2, 200);
  }
  drawBlips(dc);
}

void RadarView::composeBackground(lv_draw_ctx_t* dc) {
  const lv_area_t* clip = dc->clip_area;
  const lv_area_t* ba = dc->buf_area;
  const int x1 = std::max<int>(clip->x1, 0), x2 = std::min<int>(clip->x2, W - 1);
  const int y1 = std::max<int>(clip->y1, 0), y2 = std::min<int>(clip->y2, H - 1);
  if (x1 > x2 || y1 > y2) return;
  const int bw = lv_area_get_width(ba);
  auto* buf = static_cast<lv_color_t*>(dc->buf);
  const uint16_t sweep = animating_ ? sweep_ : 0;
  const uint16_t trail = animating_ ? trailLen_ : 0;
  const int n = x2 - x1 + 1;
  for (int y = y1; y <= y2; ++y) {
    const Px* s = layer_ + y * W + x1;
    lv_color_t* d = buf + (y - ba->y1) * bw + (x1 - ba->x1);
    for (int i = 0; i < n; ++i) {
      const Px p = s[i];
      uint16_t c = p.rgb;
      if (p.angle != kNoSweep) {
        const uint16_t dd = static_cast<uint16_t>((sweep - p.angle) & 4095);
        if (dd < trail) c = add565(c, lut_[dd]);
      }
      storePixel(d[i], c);
    }
  }
}

void RadarView::drawAnnotations(lv_draw_ctx_t* dc) {
  for (const Annotation& n : notes_) {
    if (!areasOverlap(n.area, *dc->clip_area)) continue;
    const lv_color_t c = col(n.color);
    const lv_coord_t x = n.anchor.x, y = n.anchor.y;
    switch (n.kind) {
      case 1: {  // airport: small hollow square
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_opa = LV_OPA_TRANSP;
        d.border_color = c;
        d.border_width = 1;
        d.border_opa = LV_OPA_COVER;
        lv_area_t a{static_cast<lv_coord_t>(x - 3), static_cast<lv_coord_t>(y - 3),
                    static_cast<lv_coord_t>(x + 3), static_cast<lv_coord_t>(y + 3)};
        lv_draw_rect(dc, &d, &a);
        break;
      }
      case 2:  // observer: ring with a dot
        drawDot(dc, x, y, 6, c, LV_OPA_TRANSP, 2, c);
        drawDot(dc, x, y, 1, c, LV_OPA_COVER);
        break;
      case 3:  // center: cross
        drawLine(dc, static_cast<lv_coord_t>(x - 6), y, static_cast<lv_coord_t>(x + 6), y, c, 2);
        drawLine(dc, x, static_cast<lv_coord_t>(y - 6), x, static_cast<lv_coord_t>(y + 6), c, 2);
        break;
      default:
        break;
    }
    if (n.text[0]) drawText(dc, n.textArea, n.text, n.color, n.font);
  }
}

void RadarView::drawBlips(lv_draw_ctx_t* dc) {
  const Theme& t = theme();
  const lv_area_t* clip = dc->clip_area;
  for (const Blip& b : blips_) {
    const lv_area_t bb = blipBounds(b);
    if (!areasOverlap(bb, *clip)) continue;
    const uint32_t base = b.emergency ? t.emergency : (b.isSelected ? t.selected : t.blip);

    for (int i = 0; i < b.trailN; ++i) {
      const lv_opa_t opa = static_cast<lv_opa_t>(170 - i * 25);
      drawDot(dc, b.trail[i].x, b.trail[i].y, 1, col(b.isSelected ? t.selected : t.trail), opa);
    }
    if (b.leaderPx > 0) {
      drawLine(dc, static_cast<lv_coord_t>(std::lround(b.x)), static_cast<lv_coord_t>(std::lround(b.y)),
               static_cast<lv_coord_t>(std::lround(b.x + b.dx * b.leaderPx)),
               static_cast<lv_coord_t>(std::lround(b.y + b.dy * b.leaderPx)), col(base), 1, 190);
    }
    if (b.isSelected) drawDot(dc, b.x, b.y, 11, col(t.selected), LV_OPA_TRANSP, 2, col(t.selected));

    lv_color_t dot;
    if (b.ground) {
      dot = col(t.blipGround);
    } else if (b.emergency || b.isSelected) {
      dot = col(base);
    } else {
      dot = lv_color_mix(col(t.blip), col(t.blipDim), static_cast<lv_opa_t>(110 + b.glowLevel * 20));
    }
    if (b.ground) {
      lv_draw_rect_dsc_t d;
      lv_draw_rect_dsc_init(&d);
      d.bg_opa = LV_OPA_TRANSP;
      d.border_color = dot;
      d.border_width = 2;
      const lv_coord_t x = static_cast<lv_coord_t>(std::lround(b.x));
      const lv_coord_t y = static_cast<lv_coord_t>(std::lround(b.y));
      lv_area_t a{static_cast<lv_coord_t>(x - 3), static_cast<lv_coord_t>(y - 3),
                  static_cast<lv_coord_t>(x + 3), static_cast<lv_coord_t>(y + 3)};
      lv_draw_rect(dc, &d, &a);
    } else {
      if (b.glowLevel >= 6) drawDot(dc, b.x, b.y, t.blipRadius + 3, dot, 70);
      drawDot(dc, b.x, b.y, t.blipRadius, dot, LV_OPA_COVER);
    }

    if (b.showLabel && areasOverlap(b.labelArea, *clip)) {
      const uint32_t lc = b.isSelected ? t.selected : (b.emergency ? t.emergency : t.label);
      lv_point_t s1;
      textSize(b.line1, t.fontLabel, s1);
      lv_area_t a1{b.labelArea.x1, b.labelArea.y1, b.labelArea.x2, static_cast<lv_coord_t>(b.labelArea.y1 + s1.y)};
      lv_area_t a2{b.labelArea.x1, static_cast<lv_coord_t>(b.labelArea.y1 + s1.y), b.labelArea.x2, b.labelArea.y2};
      drawText(dc, a1, b.line1, lc, t.fontLabel);
      drawText(dc, a2, b.line2, b.isSelected ? t.selected : t.labelDim, t.fontLabel);
    }
  }
}

}  // namespace ui
