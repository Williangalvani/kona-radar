// Building blocks that place things at the pixel coordinates used in
// docs/design (466 x 466 screen, origin top-left).
#pragma once

#include <lvgl.h>

#include <functional>
#include <string>

#include "draw.h"

namespace ui {

enum class Align : uint8_t { Left, Center, Right };

// A label anchored like the design's text(): x is the left edge, center or
// right edge depending on `align`; y is the top of the font's ascender.
class Text {
 public:
  void create(lv_obj_t* parent, int x, int y, const lv_font_t* font, uint32_t color,
              Align align = Align::Center, int tracking = 0);
  void set(const char* s);
  void set(const std::string& s) { set(s.c_str()); }
  void setColor(uint32_t color);
  void setFont(const lv_font_t* font);
  void moveTo(int x, int y);
  void show(bool on);
  int width() const;
  lv_obj_t* obj() const { return obj_; }
  const std::string& text() const { return text_; }

 private:
  void place();
  lv_obj_t* obj_ = nullptr;
  const lv_font_t* font_ = nullptr;
  int x_ = 0, y_ = 0, tracking_ = 0;
  Align align_ = Align::Center;
  uint32_t color_ = 0xFFFFFFFF;
  std::string text_;
};

// A full-screen transparent object whose content is drawn by `fn` with the
// draw:: rasterizer, in design coordinates.
class Sketch {
 public:
  using DrawFn = std::function<void(draw::Surface&)>;
  void create(lv_obj_t* parent, DrawFn fn);
  void invalidate();
  void invalidate(int x0, int y0, int x1, int y1);
  lv_obj_t* obj() const { return obj_; }

 private:
  static void onDraw(lv_event_t* e);
  lv_obj_t* obj_ = nullptr;
  DrawFn fn_;
};

// Rounded rectangle with fill and 1 px outline, design box (x0,y0)-(x1,y1).
lv_obj_t* panel(lv_obj_t* parent, int x0, int y0, int x1, int y1, int radius, uint32_t fill,
                uint32_t outline, bool hasOutline = true);
void setPanelColors(lv_obj_t* p, uint32_t fill, uint32_t outline);

// The design's button(): 48 px tall, centered at x = 233.
struct Button {
  lv_obj_t* box = nullptr;
  Text label;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  void create(lv_obj_t* parent, int y, const char* text, uint32_t color, int width);
  void setColor(uint32_t color);
  void setPressed(bool on);
  bool contains(int x, int y) const { return x >= x0 - 8 && x <= x1 + 8 && y >= y0 - 8 && y <= y1 + 8; }
  uint32_t color = 0;
};

// The design's tag(): small rounded label 24 px tall centered at x.
struct Tag {
  lv_obj_t* box = nullptr;
  Text label;
  int cx = 0, y = 0;
  void create(lv_obj_t* parent, int cx, int y, int fontPx);
  void set(const char* text, uint32_t color, uint32_t fill, uint32_t outline);
  void show(bool on);
};

// Full-screen black container.
lv_obj_t* screenRoot(lv_obj_t* parent);

// The design's top(): short cyan rule and a tracked title.
void topHeader(lv_obj_t* parent, Text& title, const char* text);
// The design's footer(): 11 px mono at y = 429.
void footer(lv_obj_t* parent, Text& t, const char* text, uint32_t color);

}  // namespace ui
