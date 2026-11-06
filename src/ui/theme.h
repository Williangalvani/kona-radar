// Visual theme from docs/design/DESIGN.md: palettes, fonts and the helpers
// that place text the way the design renders it. Restyle here; screen
// layouts live in the screen files.
#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "fonts/fonts.h"

namespace ui {

// Colors are 0xRRGGBB. Each role has a day and a night (warm, low
// luminance) value; see Palette::forDay() / Palette::forNight().
struct Palette {
  bool night;
  uint32_t text;        // primary text, big numbers
  uint32_t title;       // screen titles
  uint32_t muted;       // secondary text
  uint32_t quiet;       // tertiary text, footers
  uint32_t accent;      // cyan: mode line, active step, view marker
  uint32_t accentDim;   // trails, outer selection ring
  uint32_t amber;       // arrivals, focus, alerts
  uint32_t sage;        // departures, success
  uint32_t coral;       // failure
  uint32_t grid;        // rings, ticks, dividers
  uint32_t coast;
  uint32_t panel;       // dock / tag / button fill
  uint32_t panelLine;   // their outline
  uint32_t cone;        // observer view cone
  uint32_t runway;
  uint32_t arrivalLine; // dashed line from an arrival to its threshold
  uint32_t plane;
  uint32_t planeDeparture;
  uint32_t planeFocus;
  uint32_t observer;
  uint32_t mutedTarget; // stale targets
  uint32_t northLabel;

  static const Palette& forDay();
  static const Palette& forNight();
};

// Dim idle screen colors (shared by day and night).
struct IdleColors {
  static constexpr uint32_t title = 0x354044;
  static constexpr uint32_t count = 0x465354;
  static constexpr uint32_t text = 0x3C484A;
  static constexpr uint32_t dot = 0x634D32;
  static constexpr uint32_t arrival = 0x5E5140;
  static constexpr uint32_t rule = 0x202D30;
  static constexpr uint32_t footer = 0x2C3638;
};

const Palette& palette();
void setNightPalette(bool night);

inline lv_color_t col(uint32_t rgb) { return lv_color_hex(rgb); }

// Fonts by design role (IBM Plex; "semi" = the design's "bold").
namespace font {
inline const lv_font_t* mono(int px) {
  switch (px) {
    case 10: return &plex_mono_10;
    case 11: return &plex_mono_11;
    case 12: return &plex_mono_12;
    case 13: return &plex_mono_13;
    case 14: return &plex_mono_14;
    default: return &plex_mono_16;
  }
}
inline const lv_font_t* medium(int px) {
  return px <= 16 ? &plex_med_16 : px <= 18 ? &plex_med_18 : &plex_med_20;
}
inline const lv_font_t* regular18() { return &plex_reg_18; }
inline const lv_font_t* bold(int px) {
  switch (px) {
    case 14: return &plex_semi_14;
    case 16: return &plex_semi_16;
    case 18: return &plex_semi_18;
    case 20: return &plex_semi_20;
    case 26: return &plex_semi_26;
    case 29: return &plex_semi_29;
    case 32: return &plex_semi_32;
    case 36: return &plex_semi_36;
    case 46: return &plex_semi_46;
    case 53: return &plex_semi_53;
    case 66: return &plex_semi_66;
    default: return &plex_semi_20;
  }
}
}  // namespace font

// The design's text y coordinates are the top of the capital letters; LVGL
// positions the top of its line box. This is the difference in pixels to add
// to a design y coordinate.
int designTopOffset(const lv_font_t* f);

}  // namespace ui
