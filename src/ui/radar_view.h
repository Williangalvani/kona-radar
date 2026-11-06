// The radar scope: a full-screen LVGL object with a custom draw routine.
//
// Rendering strategy (keeps the frame rate up on a 466x466 panel):
//  * Land, coastline, range rings and ticks are rendered once per
//    range/view/pixel-shift change into a per-pixel layer in PSRAM, which
//    also stores each pixel's screen angle.
//  * Each frame only the bounding box of the sweep sector is invalidated;
//    the draw routine copies the layer into LVGL's buffer and adds the
//    sweep afterglow from a lookup table indexed by angle.
//  * Aircraft, labels and annotations are drawn with LVGL primitives on top
//    and only their own small areas are invalidated when they change.
#pragma once

#include <lvgl.h>

#include <vector>

#include "geo.h"
#include "settings.h"
#include "ui_model.h"

namespace ui {

class RadarView {
 public:
  bool create(lv_obj_t* parent);
  lv_obj_t* obj() const { return obj_; }
  void setModel(const Model* m) { model_ = m; }

  // Rebuilds the background layer if range, center, view or shift changed.
  void configure(const radar::Settings& s, int shiftX, int shiftY);
  // Advances the sweep and aircraft; call once per frame.
  void tick(uint32_t nowMs);
  void setAnimating(bool on);
  // Screen areas covered by overlay widgets; aircraft labels avoid them.
  void setKeepOut(const std::vector<lv_area_t>& areas) { keepOut_ = areas; }

  // ICAO hex of the aircraft nearest to a tap, or nullptr.
  const char* hitTest(lv_coord_t x, lv_coord_t y) const;
  void setSelected(const char* hex);
  const char* selected() const { return selected_[0] ? selected_ : nullptr; }
  const radar::ScopeProjection& projection() const { return proj_; }
  size_t visibleCount() const { return blips_.size(); }

 private:
  struct Px {
    uint16_t rgb;    // native RGB565
    uint16_t angle;  // screen angle, 4096 units per turn; 0xFFFF = no sweep
  };
  struct Blip {
    char hex[8];
    float x, y;
    float dx, dy;      // unit vector of the track on screen
    float leaderPx;    // 0 = no leader
    bool ground, emergency, isSelected;
    lv_point_t trail[6];
    uint8_t trailN;
    char line1[16];
    char line2[24];
    lv_area_t labelArea;
    bool showLabel;
    uint8_t glowLevel;  // 0..7
    uint16_t angle12;   // screen angle for glow timing
    double distNm;
  };
  struct Annotation {
    lv_area_t area;      // full bounds (marker + text)
    lv_area_t textArea;
    char text[16];
    uint32_t color;
    const lv_font_t* font;
    uint8_t kind;  // 0 text, 1 airport square + text, 2 observer marker, 3 center mark
    lv_point_t anchor;
  };

  static void eventCb(lv_event_t* e);
  void drawMain(lv_event_t* e);
  void composeBackground(lv_draw_ctx_t* dc);
  void drawAnnotations(lv_draw_ctx_t* dc);
  void drawBlips(lv_draw_ctx_t* dc);
  void buildLayer();
  void buildAnnotations();
  void updateBlips(uint32_t nowMs);
  void invalidateSweep(uint16_t from12, uint16_t to12);
  static lv_area_t blipBounds(const Blip& b);

  lv_obj_t* obj_ = nullptr;
  const Model* model_ = nullptr;
  Px* layer_ = nullptr;
  uint16_t lut_[4096] = {};
  uint16_t trailLen_ = 1024;
  uint16_t sweep_ = 0;
  bool animating_ = true;
  bool configured_ = false;
  radar::ScopeProjection proj_;
  radar::Settings cfg_{};
  int shiftX_ = 0, shiftY_ = 0;
  std::vector<Blip> blips_;
  std::vector<Annotation> notes_;
  std::vector<lv_area_t> keepOut_;
  uint32_t lastBlipUpdateMs_ = 0;
  char selected_[8] = {};
  bool selectionDirty_ = false;
};

// Distance between range rings for a given range, NM.
double ringStepNm(double rangeNm);

}  // namespace ui
