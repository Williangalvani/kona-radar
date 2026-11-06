// Visual theme: every color, font and size the UI uses lives here so the
// look can be restyled without touching layout or logic.
#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace ui {

// Colors are 0xRRGGBB.
struct Theme {
  // Scope background layer (pre-rendered per pixel)
  uint32_t scopeCenter = 0x02140A;  // face color at the center
  uint32_t scopeEdge = 0x010A05;    // face color at the outer ring
  uint32_t land = 0x0B2A16;
  uint32_t coast = 0x2C7A47;
  uint32_t ring = 0x1C5C33;
  uint32_t ringOuter = 0x35A060;
  uint32_t tick = 0x2E8A52;
  float coastWidth = 1.3f;
  float ringWidth = 1.1f;
  float outerRingWidth = 2.2f;

  // Sweep
  uint32_t sweepTrail = 0x26FF73;  // additive glow color (scaled by intensity)
  float sweepTrailMax = 0.42f;     // peak intensity right behind the beam
  float sweepTrailDeg = 90.0f;
  uint32_t sweepBeam = 0x7DFFAA;

  // Scope annotations drawn by LVGL
  uint32_t cardinal = 0x5CD68A;
  uint32_t ringLabel = 0x3A8F5C;
  uint32_t airport = 0x4C9A6A;
  uint32_t observer = 0x9FE8FF;
  uint32_t centerMark = 0x7DFFAA;

  // Aircraft
  uint32_t blip = 0x8CFFB4;
  uint32_t blipDim = 0x2C8A50;
  uint32_t blipGround = 0x4F7A60;
  uint32_t label = 0xB8FFD2;
  uint32_t labelDim = 0x6FB98C;
  uint32_t selected = 0xFFC233;
  uint32_t emergency = 0xFF4D4D;
  uint32_t trail = 0x5CD68A;

  // Overlay widgets
  uint32_t text = 0xD8FFE6;
  uint32_t textDim = 0x7FB394;
  uint32_t accent = 0x3DFF8B;
  uint32_t warn = 0xFFB020;
  uint32_t error = 0xFF5A5A;
  uint32_t panelBg = 0x000000;
  uint8_t panelOpa = 232;
  uint32_t buttonBg = 0x0A2A17;
  uint32_t buttonBorder = 0x2E8A52;

  const lv_font_t* fontLabel = &lv_font_montserrat_14;    // aircraft data blocks
  const lv_font_t* fontSmall = &lv_font_montserrat_14;
  const lv_font_t* fontBody = &lv_font_montserrat_16;
  const lv_font_t* fontLarge = &lv_font_montserrat_20;
  const lv_font_t* fontTitle = &lv_font_montserrat_24;
  const lv_font_t* fontHuge = &lv_font_montserrat_32;

  // Geometry
  int scopeRadius = 224;  // px, outer ring; leaves room for pixel shift
  int blipRadius = 4;
  int hitRadius = 34;     // tap tolerance around a blip
};

const Theme& theme();

inline lv_color_t col(uint32_t rgb) { return lv_color_hex(rgb); }

}  // namespace ui
