// Software display rotation in quarter turns. The AMOLED controllers on this
// board cannot exchange rows and columns (the CO5300's MADCTL only mirrors),
// so the flush callback rotates each rendered block and touch input gets the
// inverse mapping.
//
// "Logical" coordinates are what the UI draws in (W x H); "physical" are the
// panel's native coordinates, which the touch controller reports too.
// rot = clockwise quarter turns of the picture on the panel.
#pragma once

#include <cstdint>

namespace radar {

inline int rotationDegrees(uint8_t rot) { return (rot & 3) * 90; }

inline void rotatePoint(uint8_t rot, int w, int h, int x, int y, int& px, int& py) {
  switch (rot & 3) {
    case 1: px = h - 1 - y; py = x; break;
    case 2: px = w - 1 - x; py = h - 1 - y; break;
    case 3: px = y; py = w - 1 - x; break;
    default: px = x; py = y; break;
  }
}

inline void unrotatePoint(uint8_t rot, int w, int h, int px, int py, int& x, int& y) {
  switch (rot & 3) {
    case 1: x = py; y = h - 1 - px; break;
    case 2: x = w - 1 - px; y = h - 1 - py; break;
    case 3: x = w - 1 - py; y = px; break;
    default: x = px; y = py; break;
  }
}

// A raw touch point (panel coordinates) as a point on the rotated picture.
// Controllers may report one past the last pixel, which would otherwise map
// to -1 after rotation.
inline void touchToLogical(uint8_t rot, int w, int h, int rawX, int rawY, int& x, int& y) {
  if (rawX < 0) rawX = 0;
  if (rawX > w - 1) rawX = w - 1;
  if (rawY < 0) rawY = 0;
  if (rawY > h - 1) rawY = h - 1;
  unrotatePoint(rot, w, h, rawX, rawY, x, y);
}

// Rotates an inclusive logical rectangle into physical coordinates.
inline void rotateArea(uint8_t rot, int w, int h, int x1, int y1, int x2, int y2, int& ox1,
                       int& oy1, int& ox2, int& oy2) {
  int ax, ay, bx, by;
  rotatePoint(rot, w, h, x1, y1, ax, ay);
  rotatePoint(rot, w, h, x2, y2, bx, by);
  ox1 = ax < bx ? ax : bx;
  ox2 = ax < bx ? bx : ax;
  oy1 = ay < by ? ay : by;
  oy2 = ay < by ? by : ay;
}

// Copies a `lw` x `lh` block of 16-bit pixels into `dst` rotated by `rot`.
// For rot 1/3 the destination block is `lh` wide and `lw` tall.
inline void rotateBlock16(uint8_t rot, const uint16_t* src, uint16_t* dst, int lw, int lh) {
  switch (rot & 3) {
    case 1:
      for (int j = 0; j < lh; ++j) {
        uint16_t* d = dst + (lh - 1 - j);
        const uint16_t* s = src + j * lw;
        for (int i = 0; i < lw; ++i, d += lh) *d = s[i];
      }
      break;
    case 2: {
      const int n = lw * lh;
      for (int k = 0; k < n; ++k) dst[n - 1 - k] = src[k];
      break;
    }
    case 3:
      for (int j = 0; j < lh; ++j) {
        uint16_t* d = dst + (lw - 1) * lh + j;
        const uint16_t* s = src + j * lw;
        for (int i = 0; i < lw; ++i, d -= lh) *d = s[i];
      }
      break;
    default:
      for (int k = 0; k < lw * lh; ++k) dst[k] = src[k];
      break;
  }
}

// What the flush callback sends to the panel for one rendered block.
struct PanelBlock {
  int x1, y1, x2, y2;     // inclusive, physical
  const uint16_t* pixels;  // `src` itself when not rotated, else `scratch`
};

// `scratch` must hold as many pixels as the block. With an even panel size
// and LVGL's rounder (even start, odd end), the result keeps that alignment.
inline PanelBlock toPanel(uint8_t rot, int w, int h, int x1, int y1, int x2, int y2, const uint16_t* src,
                          uint16_t* scratch) {
  PanelBlock b{x1, y1, x2, y2, src};
  if ((rot & 3) == 0) return b;
  rotateArea(rot, w, h, x1, y1, x2, y2, b.x1, b.y1, b.x2, b.y2);
  rotateBlock16(rot, src, scratch, x2 - x1 + 1, y2 - y1 + 1);
  b.pixels = scratch;
  return b;
}

// Classifies a finished touch stroke by its displacement on the picture
// (logical coordinates). Sideways strokes need 45 px; up/down switch night
// colors and the quiet screen, so they need a longer, clearly vertical
// stroke.
enum class Stroke : uint8_t { None, Left, Right, Up, Down };

constexpr int kSwipePx = 45;
constexpr int kSwipeVerticalPx = 80;

inline Stroke classifyStroke(int dx, int dy) {
  const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  if (ax > kSwipePx && ax >= ay) return dx < 0 ? Stroke::Left : Stroke::Right;
  if (ay > kSwipeVerticalPx && ay * 10 > ax * 13) return dy < 0 ? Stroke::Up : Stroke::Down;
  return Stroke::None;
}

}  // namespace radar
