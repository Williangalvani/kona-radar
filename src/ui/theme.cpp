#include "theme.h"

namespace ui {
namespace {

const Palette kDay = {
    false,
    0xF2F4EB,  // text
    0xF2F4EB,  // title
    0x97A7A9,  // muted
    0x64797C,  // quiet
    0x80E1E8,  // accent
    0x28535B,  // accentDim
    0xFFC271,  // amber
    0xB8E3AD,  // sage
    0xFF896D,  // coral
    0x23383C,  // grid
    0x294045,  // coast
    0x0C171B,  // panel
    0x294148,  // panelLine
    0x102A30,  // cone
    0xF2F4EB,  // runway
    0x684E29,  // arrivalLine
    0x80E1E8,  // plane
    0xB8E3AD,  // planeDeparture
    0xFFC271,  // planeFocus
    0xF2F4EB,  // observer
    0x3E5558,  // mutedTarget
    0x97A7A9,  // northLabel
};

// Warm and a step below day, but every role stays readable at arm's length
// on the real panel at the default night brightness.
const Palette kNight = {
    true,
    0xFFC48A,  // text
    0xF2B57C,  // title
    0xD9A06C,  // muted
    0xAE7C52,  // quiet
    0xF5AE68,  // accent
    0x80522F,  // accentDim
    0xFFB75E,  // amber
    0xE6C37E,  // sage
    0xFF8F66,  // coral
    0x4D3523,  // grid
    0x5C412B,  // coast
    0x1C120B,  // panel
    0x6B4629,  // panelLine
    0x33241A,  // cone
    0xE6AA70,  // runway
    0x8C5C33,  // arrivalLine
    0xEFB274,  // plane
    0xE6C37E,  // planeDeparture
    0xFFD07A,  // planeFocus
    0xFFD3A6,  // observer
    0x8F6642,  // mutedTarget
    0xD9A06C,  // northLabel
};

const Palette* g_current = &kDay;

}  // namespace

const Palette& Palette::forDay() { return kDay; }
const Palette& Palette::forNight() { return kNight; }
const Palette& palette() { return *g_current; }
void setNightPalette(bool night) { g_current = night ? &kNight : &kDay; }

int designTopOffset(const lv_font_t* f) {
  lv_font_glyph_dsc_t g;
  if (!lv_font_get_glyph_dsc(f, &g, 'H', 0) && !lv_font_get_glyph_dsc(f, &g, '0', 0)) return 0;
  const int capTop = (f->line_height - f->base_line) - (g.box_h + g.ofs_y);
  return -capTop;
}

}  // namespace ui
