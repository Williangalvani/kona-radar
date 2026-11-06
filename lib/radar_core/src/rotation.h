// Software display rotation in quarter turns. The AMOLED controllers on this
// board cannot swap rows/columns in hardware, so the flush callback rotates
// each rendered block and the touch driver applies the inverse mapping.
//
// "Logical" coordinates are what the UI draws in (W x H); "physical" are the
// panel's native coordinates. rot = clockwise quarter turns of the picture.
#pragma once

#include <cstdint>

namespace radar {

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
      for (int j = 0; j < lh; ++j)
        for (int i = 0; i < lw; ++i) dst[i * lh + (lh - 1 - j)] = src[j * lw + i];
      break;
    case 2:
      for (int j = 0; j < lh; ++j)
        for (int i = 0; i < lw; ++i) dst[(lh - 1 - j) * lw + (lw - 1 - i)] = src[j * lw + i];
      break;
    case 3:
      for (int j = 0; j < lh; ++j)
        for (int i = 0; i < lw; ++i) dst[(lw - 1 - i) * lh + j] = src[j * lw + i];
      break;
    default:
      for (int k = 0; k < lw * lh; ++k) dst[k] = src[k];
      break;
  }
}

}  // namespace radar
