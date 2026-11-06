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

const Palette kNight = {
    true,
    0xD1945D,  // text
    0xA66C45,  // title
    0xA66C45,  // muted
    0x6E4A30,  // quiet
    0xA66C45,  // accent
    0x4E2D1E,  // accentDim
    0xD1945D,  // amber
    0xD1945D,  // sage
    0xD1945D,  // coral
    0x291C14,  // grid
    0x342419,  // coast
    0x100B07,  // panel
    0x472D1A,  // panelLine
    0x1D1A12,  // cone
    0xA66C45,  // runway
    0x493020,  // arrivalLine
    0xD1945D,  // plane
    0xD1945D,  // planeDeparture
    0xD1945D,  // planeFocus
    0xD1945D,  // observer
    0x4A3222,  // mutedTarget
    0xA66C45,  // northLabel
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
