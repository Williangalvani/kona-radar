#include "draw.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ui {
namespace draw {
namespace {

constexpr float kPi = 3.14159265358979f;

inline uint8_t cov(float c) {
  if (c <= 0) return 0;
  if (c >= 1) return 255;
  return static_cast<uint8_t>(c * 255.0f + 0.5f);
}

inline uint8_t scale(uint8_t a, uint8_t opa) {
  return opa == 255 ? a : static_cast<uint8_t>((a * opa + 127) / 255);
}

float segDistance(float px, float py, float x0, float y0, float vx, float vy, float len2) {
  float t = len2 > 0 ? ((px - x0) * vx + (py - y0) * vy) / len2 : 0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const float ex = px - x0 - t * vx, ey = py - y0 - t * vy;
  return std::sqrt(ex * ex + ey * ey);
}

bool insidePolygon(const Point* p, int n, float x, float y) {
  bool in = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    if ((p[i].y > y) != (p[j].y > y) &&
        x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x)
      in = !in;
  }
  return in;
}

// Visits the dashes of a dashed line.
template <typename F>
void forDashes(float x0, float y0, float x1, float y1, float dash, float gap, F&& f) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0) return;
  for (float t = 0; t < len; t += dash + gap) {
    const float e = std::min(len, t + dash);
    f(x0 + dx * t / len, y0 + dy * t / len, x0 + dx * e / len, y0 + dy * e / len);
  }
}

}  // namespace

Surface Surface::fromDrawCtx(lv_draw_ctx_t* dc, const lv_obj_t* obj) {
  Surface s;
  s.buf = static_cast<lv_color_t*>(dc->buf);
  s.bufArea = *dc->buf_area;
  s.stride = lv_area_get_width(dc->buf_area);
  s.clip = *dc->clip_area;
  if (obj) {
    s.ox = obj->coords.x1;
    s.oy = obj->coords.y1;
  }
  return s;
}

Surface Surface::fromLayer(lv_color_t* px, int w, int h) {
  Surface s;
  s.buf = px;
  s.stride = w;
  s.bufArea = {0, 0, static_cast<lv_coord_t>(w - 1), static_cast<lv_coord_t>(h - 1)};
  s.clip = s.bufArea;
  return s;
}

void Surface::blend(int x, int y, lv_color_t c, uint8_t alpha) {
  if (alpha == 0 || x < clip.x1 || x > clip.x2 || y < clip.y1 || y > clip.y2) return;
  lv_color_t& d = buf[(y - bufArea.y1) * stride + (x - bufArea.x1)];
  d = alpha == 255 ? c : lv_color_mix(c, d, alpha);
}

bool Surface::bounds(float x0, float y0, float x1, float y1, int& bx0, int& by0, int& bx1,
                     int& by1) const {
  bx0 = std::max<int>(clip.x1, static_cast<int>(std::floor(x0)));
  by0 = std::max<int>(clip.y1, static_cast<int>(std::floor(y0)));
  bx1 = std::min<int>(clip.x2, static_cast<int>(std::ceil(x1)));
  by1 = std::min<int>(clip.y2, static_cast<int>(std::ceil(y1)));
  return bx0 <= bx1 && by0 <= by1;
}

void fillCircle(Surface& s, float cx, float cy, float r, lv_color_t c, uint8_t opa) {
  cx += s.ox;
  cy += s.oy;
  int x0, y0, x1, y1;
  if (!s.bounds(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, x0, y0, x1, y1)) return;
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
      s.blend(x, y, c, scale(cov(r + 0.5f - d), opa));
    }
}

void ring(Surface& s, float cx, float cy, float r, float width, lv_color_t c, uint8_t opa) {
  arc(s, cx, cy, r, width, 0, 360, c, opa);
}

void arc(Surface& s, float cx, float cy, float r, float width, float startDeg, float endDeg,
         lv_color_t c, uint8_t opa) {
  cx += s.ox;
  cy += s.oy;
  const float outer = r + width * 0.5f + 1;
  int x0, y0, x1, y1;
  if (!s.bounds(cx - outer, cy - outer, cx + outer, cy + outer, x0, y0, x1, y1)) return;
  const bool full = endDeg - startDeg >= 360;
  float a0 = std::fmod(startDeg, 360.0f);
  if (a0 < 0) a0 += 360;
  const float span = endDeg - startDeg;
  if (span <= 0) return;
  const float inner2 = std::max(0.0f, r - width * 0.5f - 1);
  for (int y = y0; y <= y1; ++y) {
    const float py = y + 0.5f - cy;
    for (int x = x0; x <= x1; ++x) {
      const float px = x + 0.5f - cx;
      const float d = std::sqrt(px * px + py * py);
      if (d < inner2 || d > outer) continue;
      uint8_t a = cov(width * 0.5f + 0.5f - std::fabs(d - r));
      if (!a) continue;
      if (!full) {
        // Square ends, antialiased: coverage from the pixel's distance (along
        // the arc) to the nearer end.
        float ang = std::atan2(py, px) * 180.0f / kPi;
        if (ang < 0) ang += 360;
        float rel = ang - a0;
        if (rel < 0) rel += 360;
        const float inside = rel <= span ? std::min(rel, span - rel) : -std::min(rel - span, 360 - rel);
        const uint8_t e = cov(0.5f + inside * kPi / 180.0f * d);
        if (!e) continue;
        a = static_cast<uint8_t>((a * e + 127) / 255);
      }
      s.blend(x, y, c, scale(a, opa));
    }
  }
}

void ellipseRing(Surface& s, float cx, float cy, float rx, float ry, float width, lv_color_t c) {
  cx += s.ox;
  cy += s.oy;
  int x0, y0, x1, y1;
  const float pad = width + 1;
  if (!s.bounds(cx - rx - pad, cy - ry - pad, cx + rx + pad, cy + ry + pad, x0, y0, x1, y1)) return;
  const float m = std::min(rx, ry);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      const float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
      const float d = (std::sqrt(nx * nx + ny * ny) - 1.0f) * m;
      s.blend(x, y, c, cov(width * 0.5f + 0.5f - std::fabs(d)));
    }
}

void segment(Surface& s, float x0, float y0, float x1, float y1, float width, lv_color_t c,
             uint8_t opa) {
  x0 += s.ox;
  x1 += s.ox;
  y0 += s.oy;
  y1 += s.oy;
  const float pad = width * 0.5f + 1;
  int bx0, by0, bx1, by1;
  if (!s.bounds(std::min(x0, x1) - pad, std::min(y0, y1) - pad, std::max(x0, x1) + pad,
                std::max(y0, y1) + pad, bx0, by0, bx1, by1))
    return;
  const float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
  for (int y = by0; y <= by1; ++y)
    for (int x = bx0; x <= bx1; ++x) {
      const float d = segDistance(x + 0.5f, y + 0.5f, x0, y0, vx, vy, len2);
      s.blend(x, y, c, scale(cov(width * 0.5f + 0.5f - d), opa));
    }
}

void polyline(Surface& s, const Point* pts, int n, float width, lv_color_t c) {
  if (n < 2) return;
  float minX = pts[0].x, maxX = minX, minY = pts[0].y, maxY = minY;
  for (int i = 1; i < n; ++i) {
    minX = std::min(minX, pts[i].x);
    maxX = std::max(maxX, pts[i].x);
    minY = std::min(minY, pts[i].y);
    maxY = std::max(maxY, pts[i].y);
  }
  const float pad = width * 0.5f + 1;
  int bx0, by0, bx1, by1;
  if (!s.bounds(minX + s.ox - pad, minY + s.oy - pad, maxX + s.ox + pad, maxY + s.oy + pad, bx0,
                by0, bx1, by1))
    return;
  // Max coverage over all segments so joints are not drawn twice.
  for (int y = by0; y <= by1; ++y)
    for (int x = bx0; x <= bx1; ++x) {
      float best = 1e9f;
      const float px = x + 0.5f - s.ox, py = y + 0.5f - s.oy;
      for (int i = 0; i + 1 < n; ++i) {
        const float vx = pts[i + 1].x - pts[i].x, vy = pts[i + 1].y - pts[i].y;
        best = std::min(best, segDistance(px, py, pts[i].x, pts[i].y, vx, vy, vx * vx + vy * vy));
      }
      s.blend(x, y, c, cov(width * 0.5f + 0.5f - best));
    }
}

void dashed(Surface& s, float x0, float y0, float x1, float y1, float width, float dash, float gap,
            lv_color_t c) {
  forDashes(x0, y0, x1, y1, dash, gap,
            [&](float a, float b, float e, float f) { segment(s, a, b, e, f, width, c); });
}

void polygon(Surface& s, const Point* pts, int n, lv_color_t c, uint8_t opa) {
  if (n < 3) return;
  std::vector<Point> p(pts, pts + n);
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (Point& q : p) {
    q.x += s.ox;
    q.y += s.oy;
    minX = std::min(minX, q.x);
    maxX = std::max(maxX, q.x);
    minY = std::min(minY, q.y);
    maxY = std::max(maxY, q.y);
  }
  int x0, y0, x1, y1;
  if (!s.bounds(minX, minY, maxX, maxY, x0, y0, x1, y1)) return;
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      int hits = 0;
      for (int sy = 0; sy < 4; ++sy)
        for (int sx = 0; sx < 4; ++sx)
          hits += insidePolygon(p.data(), n, x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4);
      if (hits) s.blend(x, y, c, scale(static_cast<uint8_t>(hits * 255 / 16), opa));
    }
}

void fillRect(Surface& s, int x0, int y0, int x1, int y1, lv_color_t c) {
  x0 = std::max<int>(x0 + s.ox, s.clip.x1);
  x1 = std::min<int>(x1 + s.ox, s.clip.x2);
  y0 = std::max<int>(y0 + s.oy, s.clip.y1);
  y1 = std::min<int>(y1 + s.oy, s.clip.y2);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) s.blend(x, y, c, 255);
}

void alphaMap(Surface& s, int x, int y, int w, int h, const uint8_t* alpha, lv_color_t c,
              uint8_t opa) {
  x += s.ox;
  y += s.oy;
  const int x0 = std::max<int>(x, s.clip.x1), x1 = std::min<int>(x + w - 1, s.clip.x2);
  const int y0 = std::max<int>(y, s.clip.y1), y1 = std::min<int>(y + h - 1, s.clip.y2);
  for (int yy = y0; yy <= y1; ++yy) {
    const uint8_t* row = alpha + (yy - y) * w;
    for (int xx = x0; xx <= x1; ++xx) s.blend(xx, yy, c, scale(row[xx - x], opa));
  }
}

void arrow(Surface& s, float x, float y, float deg, float size, float width, lv_color_t c) {
  const float a = deg * kPi / 180.0f, ca = std::cos(a), sa = std::sin(a);
  auto xy = [&](float px, float py) { return Point{x + px * ca - py * sa, y + px * sa + py * ca}; };
  const Point tail = xy(0, size * 0.65f), tip = xy(0, -size * 0.65f);
  segment(s, tail.x, tail.y, tip.x, tip.y, width, c);
  const Point head[3] = {xy(-size * 0.4f, -size * 0.18f), tip, xy(size * 0.4f, -size * 0.18f)};
  polyline(s, head, 3, width, c);
}

namespace {
const Point kPlaneShape[] = {
    {0, -.68f},    {.12f, -.38f}, {.12f, -.10f}, {.64f, .16f},  {.64f, .30f},   {.12f, .14f},
    {.10f, .47f},  {.27f, .62f},  {.27f, .73f},  {0, .62f},     {-.27f, .73f},  {-.27f, .62f},
    {-.10f, .47f}, {-.12f, .14f}, {-.64f, .30f}, {-.64f, .16f}, {-.12f, -.10f}, {-.12f, -.38f},
};
constexpr int kPlanePoints = sizeof(kPlaneShape) / sizeof(kPlaneShape[0]);

void planePoints(Point* p, float cx, float cy, float deg, float size) {
  const float a = deg * kPi / 180.0f, ca = std::cos(a), sa = std::sin(a);
  for (int i = 0; i < kPlanePoints; ++i) {
    const float x = kPlaneShape[i].x * size, y = kPlaneShape[i].y * size;
    p[i] = {cx + x * ca - y * sa, cy + x * sa + y * ca};
  }
}
}  // namespace

void plane(Surface& s, float x, float y, float deg, float size, lv_color_t c) {
  Point p[kPlanePoints];
  planePoints(p, x, y, deg, size);
  polygon(s, p, kPlanePoints, c);
}

void renderPlane(Icon& out, float deg, float size) {
  constexpr int n = kPlanePoints;
  Point p[n];
  planePoints(p, Icon::kSize / 2.0f, Icon::kSize / 2.0f, deg, size);
  for (int y = 0; y < Icon::kSize; ++y)
    for (int x = 0; x < Icon::kSize; ++x) {
      int hits = 0;
      for (int sy = 0; sy < 4; ++sy)
        for (int sx = 0; sx < 4; ++sx)
          hits += insidePolygon(p, n, x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4);
      out.alpha[y * Icon::kSize + x] = static_cast<uint8_t>(hits * 255 / 16);
    }
}

void Mask::segment(float x0, float y0, float x1, float y1, float width) {
  const float pad = width * 0.5f + 1;
  const int bx0 = std::max(0, static_cast<int>(std::floor(std::min(x0, x1) - pad)));
  const int by0 = std::max(0, static_cast<int>(std::floor(std::min(y0, y1) - pad)));
  const int bx1 = std::min(w_ - 1, static_cast<int>(std::ceil(std::max(x0, x1) + pad)));
  const int by1 = std::min(h_ - 1, static_cast<int>(std::ceil(std::max(y0, y1) + pad)));
  const float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
  for (int y = by0; y <= by1; ++y)
    for (int x = bx0; x <= bx1; ++x) {
      const uint8_t a = cov(width * 0.5f + 0.5f - segDistance(x + 0.5f, y + 0.5f, x0, y0, vx, vy, len2));
      uint8_t& d = a_[static_cast<size_t>(y) * w_ + x];
      if (a > d) d = a;
    }
}

void Mask::dashed(float x0, float y0, float x1, float y1, float width, float dash, float gap) {
  forDashes(x0, y0, x1, y1, dash, gap,
            [&](float a, float b, float e, float f) { segment(a, b, e, f, width); });
}

void Mask::composite(Surface& s, lv_color_t c, uint8_t opa) const {
  for (int y = 0; y < h_; ++y)
    for (int x = 0; x < w_; ++x) {
      const uint8_t a = a_[static_cast<size_t>(y) * w_ + x];
      if (a) s.blend(x + s.ox, y + s.oy, c, scale(a, opa));
    }
}

}  // namespace draw
}  // namespace ui
