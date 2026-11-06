#include "widgets.h"

#include "theme.h"

namespace ui {

void Text::create(lv_obj_t* parent, int x, int y, const lv_font_t* font, uint32_t color, Align align,
                  int tracking) {
  obj_ = lv_label_create(parent);
  lv_obj_remove_style_all(obj_);
  lv_label_set_long_mode(obj_, LV_LABEL_LONG_CLIP);
  x_ = x;
  y_ = y;
  align_ = align;
  tracking_ = tracking;
  font_ = font;
  text_.clear();
  color_ = 0xFFFFFFFF;
  lv_obj_set_style_text_font(obj_, font, 0);
  lv_obj_set_style_text_letter_space(obj_, static_cast<lv_coord_t>(tracking), 0);
  setColor(color);
  lv_label_set_text(obj_, "");
  place();
}

int Text::width() const {
  return lv_txt_get_width(text_.c_str(), static_cast<uint32_t>(text_.size()), font_,
                          static_cast<lv_coord_t>(tracking_), LV_TEXT_FLAG_NONE);
}

void Text::place() {
  if (!obj_) return;
  const int w = width();
  int x = x_;
  if (align_ == Align::Center) x -= w / 2;
  if (align_ == Align::Right) x -= w;
  lv_obj_set_size(obj_, static_cast<lv_coord_t>(w + 2), font_->line_height);
  lv_obj_set_pos(obj_, static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y_ + designTopOffset(font_)));
}

void Text::set(const char* s) {
  if (!obj_ || text_ == s) return;
  text_ = s;
  lv_label_set_text(obj_, s);
  place();
}

void Text::setColor(uint32_t color) {
  if (!obj_ || color == color_) return;
  color_ = color;
  lv_obj_set_style_text_color(obj_, col(color), 0);
}

void Text::setFont(const lv_font_t* font) {
  if (!obj_ || font == font_) return;
  font_ = font;
  lv_obj_set_style_text_font(obj_, font, 0);
  place();
}

void Text::moveTo(int x, int y) {
  if (x == x_ && y == y_) return;
  x_ = x;
  y_ = y;
  place();
}

void Text::show(bool on) {
  if (!obj_) return;
  if (on)
    lv_obj_clear_flag(obj_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(obj_, LV_OBJ_FLAG_HIDDEN);
}

void Sketch::create(lv_obj_t* parent, DrawFn fn) {
  fn_ = std::move(fn);
  obj_ = lv_obj_create(parent);
  lv_obj_remove_style_all(obj_);
  lv_obj_set_size(obj_, 466, 466);
  lv_obj_set_pos(obj_, 0, 0);
  lv_obj_clear_flag(obj_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(obj_, onDraw, LV_EVENT_DRAW_MAIN, this);
}

void Sketch::onDraw(lv_event_t* e) {
  auto* self = static_cast<Sketch*>(lv_event_get_user_data(e));
  draw::Surface s = draw::Surface::fromDrawCtx(lv_event_get_draw_ctx(e), self->obj_);
  if (self->fn_) self->fn_(s);
}

void Sketch::invalidate() {
  if (obj_) lv_obj_invalidate(obj_);
}

void Sketch::invalidate(int x0, int y0, int x1, int y1) {
  if (!obj_) return;
  lv_area_t a{static_cast<lv_coord_t>(obj_->coords.x1 + x0), static_cast<lv_coord_t>(obj_->coords.y1 + y0),
              static_cast<lv_coord_t>(obj_->coords.x1 + x1), static_cast<lv_coord_t>(obj_->coords.y1 + y1)};
  lv_obj_invalidate_area(obj_, &a);
}

lv_obj_t* panel(lv_obj_t* parent, int x0, int y0, int x1, int y1, int radius, uint32_t fill,
                uint32_t outline, bool hasOutline) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_style_all(p);
  lv_obj_set_pos(p, static_cast<lv_coord_t>(x0), static_cast<lv_coord_t>(y0));
  lv_obj_set_size(p, static_cast<lv_coord_t>(x1 - x0 + 1), static_cast<lv_coord_t>(y1 - y0 + 1));
  lv_obj_set_style_radius(p, static_cast<lv_coord_t>(radius), 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(p, hasOutline ? 1 : 0, 0);
  lv_obj_set_style_border_opa(p, LV_OPA_COVER, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  setPanelColors(p, fill, outline);
  return p;
}

void setPanelColors(lv_obj_t* p, uint32_t fill, uint32_t outline) {
  lv_obj_set_style_bg_color(p, col(fill), 0);
  lv_obj_set_style_border_color(p, col(outline), 0);
}

void Button::create(lv_obj_t* parent, int y, const char* text, uint32_t c, int width) {
  x0 = 233 - width / 2;
  x1 = 233 + width / 2;
  y0 = y;
  y1 = y + 48;
  box = panel(parent, x0, y0, x1, y1, 12, palette().panel, c);
  label.create(parent, 233, y + 14, font::bold(18), c, Align::Center, 0);
  label.set(text);
  color = c;
}

void Button::setColor(uint32_t c) {
  color = c;
  setPanelColors(box, palette().panel, c);
  label.setColor(c);
}

void Button::setPressed(bool on) {
  setPanelColors(box, on ? lv_color_to32(lv_color_mix(col(color), col(palette().panel), 48)) & 0xFFFFFF
                         : palette().panel,
                 color);
}

void Tag::create(lv_obj_t* parent, int x, int top, int fontPx) {
  cx = x;
  y = top;
  box = panel(parent, x - 30, top, x + 30, top + 24, 6, palette().panel, palette().panelLine);
  label.create(parent, x, top + 6, font::mono(fontPx), palette().muted, Align::Center, 0);
}

void Tag::set(const char* text, uint32_t color, uint32_t fill, uint32_t outline) {
  label.set(text);
  label.setColor(color);
  const int w = label.width() + 20;
  lv_obj_set_pos(box, static_cast<lv_coord_t>(cx - w / 2), static_cast<lv_coord_t>(y));
  lv_obj_set_size(box, static_cast<lv_coord_t>(w), 25);
  setPanelColors(box, fill, outline);
}

void Tag::show(bool on) {
  label.show(on);
  if (on)
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* screenRoot(lv_obj_t* parent) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, 466, 466);
  lv_obj_set_pos(r, 0, 0);
  lv_obj_set_style_bg_color(r, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

void topHeader(lv_obj_t* parent, Text& title, const char* text) {
  lv_obj_t* rule = panel(parent, 207, 26, 259, 27, 1, palette().accent, palette().accent, false);
  (void)rule;
  title.create(parent, 233, 48, font::bold(16), palette().muted, Align::Center, 2);
  title.set(text);
}

void footer(lv_obj_t* parent, Text& t, const char* text, uint32_t color) {
  t.create(parent, 233, 429, font::mono(11), color, Align::Center, 1);
  t.set(text);
}

}  // namespace ui
