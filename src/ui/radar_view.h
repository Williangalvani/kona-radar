// The observer-centered scope (design screens 06, 07 and 10): a full-screen
// LVGL object with a custom draw routine.
//
//  * Coastline, rings, ticks, runway and the observer marker are rendered
//    into a full-screen layer in PSRAM whenever range, orientation, location
//    or palette change. Drawing copies the needed rows from it.
//  * Aircraft, trails, selection rings and labels are drawn on top; only the
//    small areas that changed are invalidated as aircraft move.
#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "draw.h"
#include "geo.h"
#include "settings.h"
#include "ui_model.h"

namespace ui {

class RadarView {
 public:
  static constexpr int kCx = 233;
  static constexpr int kCy = 252;
  static constexpr int kRadius = 166;

  bool create(lv_obj_t* parent);
  lv_obj_t* obj() const { return obj_; }

  // Rebuilds the background layer if anything it depends on changed.
  void configure(const radar::Settings& s, bool night);
  // Recomputes aircraft (at most every 250 ms unless `force`).
  void update(const Model& m, const char* focusHex, bool force = false);
  // Areas covered by widgets on top; aircraft labels avoid them.
  void setKeepOut(const std::vector<lv_area_t>& areas) { keepOut_ = areas; }

  // Aircraft whose icon (44 x 44 px target) or label contains the point,
  // nearest first.
  std::vector<std::string> hitTest(int x, int y) const;
  const radar::ScopeProjection& projection() const { return proj_; }

 private:
  struct Blip {
    char hex[8];
    float x, y;
    uint8_t iconIdx;  // heading in 5 degree steps
    uint8_t kind;     // 0 plane, 1 helicopter, 2 on ground
    uint32_t color;
    bool focus;
    lv_point_t trail[3];
    uint8_t trailN;
    char label[16];
    lv_area_t labelArea;
    bool showLabel;
    bool arrivalLine;
    float tx, ty;  // threshold for the arrival line
    float dist;
    int priority;
  };
  struct StaticText {
    char text[8];
    const lv_font_t* font;
    uint32_t color;
    lv_area_t area;
    bool hidden;  // an aircraft icon is on top of it
  };

  static void eventCb(lv_event_t* e);
  void drawMain(lv_event_t* e);
  void renderLayer();
  void placeLabels(std::vector<Blip>& blips) const;
  lv_area_t bounds(const Blip& b) const;
  void addStaticText(const char* txt, const lv_font_t* f, uint32_t color, float cx, float top);
  const draw::Icon& icon(uint8_t idx);

  lv_obj_t* obj_ = nullptr;
  bool configured_ = false;
  bool night_ = false;
  radar::Settings cfg_{};
  double facing_ = 0;
  bool hasFacing_ = false;
  radar::ScopeProjection proj_;
  std::vector<Blip> blips_;
  std::vector<StaticText> texts_;
  std::vector<lv_area_t> keepOut_;
  uint32_t lastUpdateMs_ = 0;
  std::string focus_;
};

}  // namespace ui
