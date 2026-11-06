// Small anti-aliased rasterizer used where LVGL 8's own primitives fall
// short (rotated polygons, thin dashed diagonals, partial rings with exact
// design geometry). Everything draws into a Surface: either LVGL's current
// draw buffer or a full-screen layer kept in PSRAM. Coordinates are screen
// pixels with pixel centers at +0.5.
#pragma once

#include <lvgl.h>
#include <stdint.h>

#include <vector>

namespace ui {
namespace draw {

struct Point {
  float x, y;
};

struct Surface {
  lv_color_t* buf = nullptr;
  int stride = 0;       // pixels per row of buf
  lv_area_t bufArea{};  // screen area covered by buf
  lv_area_t clip{};     // screen area that may be written
  int ox = 0, oy = 0;   // added to every coordinate (object position)

  // LVGL's buffer inside a DRAW_MAIN / DRAW_POST event; (ox, oy) is the
  // object's top-left corner so drawings use object-relative coordinates.
  static Surface fromDrawCtx(lv_draw_ctx_t* dc, const lv_obj_t* obj);
  static Surface fromLayer(lv_color_t* px, int w, int h);

  void blend(int x, int y, lv_color_t c, uint8_t alpha);
  bool bounds(float x0, float y0, float x1, float y1, int& bx0, int& by0, int& bx1, int& by1) const;
};

// Shapes. `opa` scales the coverage (255 = solid).
void fillCircle(Surface& s, float cx, float cy, float r, lv_color_t c, uint8_t opa = 255);
void ring(Surface& s, float cx, float cy, float r, float width, lv_color_t c, uint8_t opa = 255);
// Ring limited to an angle range. Angles follow the design tooling (Pillow):
// degrees clockwise from 3 o'clock; start < end, may exceed 360.
void arc(Surface& s, float cx, float cy, float r, float width, float startDeg, float endDeg,
         lv_color_t c, uint8_t opa = 255);
void ellipseRing(Surface& s, float cx, float cy, float rx, float ry, float width, lv_color_t c);
void segment(Surface& s, float x0, float y0, float x1, float y1, float width, lv_color_t c,
             uint8_t opa = 255);
void polyline(Surface& s, const Point* pts, int n, float width, lv_color_t c);
void dashed(Surface& s, float x0, float y0, float x1, float y1, float width, float dash, float gap,
            lv_color_t c);
// 4x4 supersampled even-odd fill.
void polygon(Surface& s, const Point* pts, int n, lv_color_t c, uint8_t opa = 255);
void fillRect(Surface& s, int x0, int y0, int x1, int y1, lv_color_t c);
// Copies an 8-bit alpha bitmap tinted with `c`.
void alphaMap(Surface& s, int x, int y, int w, int h, const uint8_t* alpha, lv_color_t c,
              uint8_t opa = 255);

// Arrow as drawn by the design: a shaft and an open head, rotated `deg`
// clockwise from pointing up.
void arrow(Surface& s, float x, float y, float deg, float size, float width, lv_color_t c);

// The design's aircraft silhouette, `size` px nose-to-center scale, rotated
// `deg` clockwise from pointing up, rendered into a square alpha bitmap.
struct Icon {
  static constexpr int kSize = 28;
  uint8_t alpha[kSize * kSize];
};
void renderPlane(Icon& out, float deg, float size);
// The same silhouette drawn directly, for larger one-off marks.
void plane(Surface& s, float x, float y, float deg, float size, lv_color_t c);

// Coverage accumulator for drawing many overlapping strokes of one color
// without darkening the joints; composite once when done.
class Mask {
 public:
  Mask(int w, int h) : w_(w), h_(h), a_(static_cast<size_t>(w) * h, 0) {}
  void segment(float x0, float y0, float x1, float y1, float width);
  void dashed(float x0, float y0, float x1, float y1, float width, float dash, float gap);
  void composite(Surface& s, lv_color_t c, uint8_t opa = 255) const;

 private:
  int w_, h_;
  std::vector<uint8_t> a_;
};

}  // namespace draw
}  // namespace ui
