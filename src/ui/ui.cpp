#include "ui.h"

#include <cmath>
#include <cstdlib>

#include "config.h"
#include "theme.h"

namespace ui {
namespace {

constexpr int kTapMovePx = 18;
constexpr int kSwipePx = 45;
// Up/down swipes switch night colors / the quiet screen, so they need a
// longer, clearly vertical stroke than the sideways zoom.
constexpr int kSwipeVerticalPx = 80;
constexpr uint32_t kToastMs = 3500;
constexpr uint32_t kTapMaxMs = 700;
constexpr uint32_t kHoldShowMs = 800;

}  // namespace

std::string wifiQrPayload(const std::string& ssid, const std::string& pass) {
  auto esc = [](const std::string& in) {
    std::string out;
    for (char c : in) {
      if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') out += '\\';
      out += c;
    }
    return out;
  };
  if (pass.empty()) return "WIFI:T:nopass;S:" + esc(ssid) + ";;";
  return "WIFI:T:WPA;S:" + esc(ssid) + ";P:" + esc(pass) + ";;";
}

bool Ui::begin(Actions* actions) {
  actions_ = actions;
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  build();
  return stage_ != nullptr;
}

void Ui::build() {
  setNightPalette(night_);
  if (stage_) lv_obj_del(stage_);
  if (hold_) lv_obj_del(hold_);
  if (toast_) lv_obj_del(toast_);
  toast_ = nullptr;
  stage_ = screenRoot(lv_scr_act());
  lv_obj_set_pos(stage_, static_cast<lv_coord_t>(shiftX_), static_cast<lv_coord_t>(shiftY_));
  Screen* all[] = {&splash_, &setupQr_, &link_, &connecting_, &result_, &radar_, &detail_,
                   &alert_, &idle_, &alignment_, &menu_, &guide_, &update_};
  for (Screen* s : all) {
    s->bind(actions_, this);
    s->build(stage_);
    s->show(true);
    s->show(false);
  }
  active_ = nullptr;

  const Palette& p = palette();
  hold_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(hold_);
  lv_obj_set_size(hold_, 466, 466);
  lv_obj_set_style_bg_color(hold_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(hold_, 235, 0);
  lv_obj_clear_flag(hold_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  holdArt_.create(hold_, [this](draw::Surface& s) {
    const Palette& p = palette();
    draw::ring(s, 233, 200, 64, 6, col(p.grid));
    if (holdProgress_ > 0) draw::arc(s, 233, 200, 64, 6, 270, 270 + 360 * holdProgress_, col(p.coral));
    draw::arc(s, 233, 196, 22, 2.4f, 220, 320, col(p.muted));
    draw::arc(s, 233, 196, 12, 2.4f, 220, 320, col(p.muted));
    draw::fillCircle(s, 233, 202, 3, col(p.muted));
  });
  holdTitle_.create(hold_, 233, 292, font::bold(26), p.text);
  holdTitle_.set("Keep holding");
  holdSub_.create(hold_, 233, 328, font::medium(18), p.muted);
  holdSub_.set("to reset Wi-Fi only");
  holdHint_.create(hold_, 233, 372, font::mono(12), p.quiet, Align::Center, 1);
  holdHint_.set("RELEASE TO CANCEL");
  lv_obj_add_flag(hold_, LV_OBJ_FLAG_HIDDEN);
  holdProgress_ = -1;
  buildToast();
}

void Ui::buildToast() {
  const Palette& p = palette();
  toast_ = panel(lv_layer_top(), 93, 262, 373, 330, 14, p.panel, p.panelLine);
  lv_obj_clear_flag(toast_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  // Children sit inside the 1 px outline.
  toastTitle_.create(toast_, 139, 11, font::bold(20), p.text);
  toastSub_.create(toast_, 139, 42, font::mono(11), p.muted, Align::Center, 1);
  toastSub_.setMaxWidth(260);
  toastTitle_.set(toastTitleText_);
  toastSub_.set(toastSubText_);
  toastShown_ = true;
  updateToast();
}

void Ui::updateToast() {
  if (!toast_) return;
  const bool show = haveModel_ && static_cast<int32_t>(toastUntilMs_ - m_.nowMs) > 0 && active_ != &idle_ &&
                    active_ != &update_;
  if (show == toastShown_) return;
  toastShown_ = show;
  if (show)
    lv_obj_clear_flag(toast_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
}

void Ui::notify(const std::string& title, const std::string& sub) {
  toastTitleText_ = title;
  toastSubText_ = sub;
  toastUntilMs_ = m_.nowMs + kToastMs;
  if (!toast_) return;
  toastTitle_.set(title);
  toastSub_.set(sub);
  toastShown_ = false;
  lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
  updateToast();
}

void Ui::setPixelShift(int dx, int dy) {
  if (dx == shiftX_ && dy == shiftY_) return;
  shiftX_ = dx;
  shiftY_ = dy;
  if (stage_) lv_obj_set_pos(stage_, static_cast<lv_coord_t>(dx), static_cast<lv_coord_t>(dy));
}

Screen* Ui::route(const Model& m) {
  if (m.otaProgress >= 0) return &update_;
  if (m.phase != Phase::Running) view_ = View::Radar;
  switch (m.phase) {
    case Phase::Boot:
      return &splash_;
    case Phase::Connecting:
    case Phase::SetupJoining:
    case Phase::SetupOnline:
      return &connecting_;
    case Phase::Offline:
    case Phase::SetupDone:
    case Phase::SetupFailed:
      return &result_;
    case Phase::SetupJoin:
    case Phase::SetupPortal:
      return &setupQr_;
    case Phase::Running:
    default:
      break;
  }
  if (m.idle || m.blackout) {
    if (view_ != View::Guide) view_ = View::Radar;
    return &idle_;
  }
  if (!m.alertHex.empty() && findTarget(m, m.alertHex)) return &alert_;
  if (m.guidePending && !guideOffered_) {
    guideOffered_ = true;
    openGuide();
  }
  if (!radarSeen_ && view_ == View::Radar) {
    radarSeen_ = true;
    radar_.showHints();
  }
  switch (view_) {
    case View::Detail:
      return &detail_;
    case View::Alignment:
      return &alignment_;
    case View::PhoneLink:
      return &link_;
    case View::Menu:
      return &menu_;
    case View::Guide:
      return &guide_;
    default:
      return &radar_;
  }
}

void Ui::update(const Model& m) {
  m_ = m;
  haveModel_ = true;
  if (m.night != night_) {
    night_ = m.night;
    build();
  }
  pollTouch(m.nowMs);
  if (guide_.finished()) closeGuide();
  Screen* s = route(m_);
  if (s != active_) {
    if (active_) active_->show(false);
    s->show(true);
    active_ = s;
  }
  active_->refresh(m_);
  updateToast();
}

void Ui::pollTouch(uint32_t now) {
  lv_indev_t* indev = lv_indev_get_next(nullptr);
  if (!indev) return;
  const bool pressed = indev->proc.state == LV_INDEV_STATE_PRESSED;
  lv_point_t pt;
  lv_indev_get_point(indev, &pt);
  const int sx = shiftX_, sy = shiftY_;

  if (pressed && !down_) {
    down_ = true;
    holdFired_ = false;
    moved_ = false;
    start_ = last_ = pt;
    downMs_ = now;
    if (active_) active_->press(pt.x - sx, pt.y - sy, true);
    return;
  }
  if (pressed) {
    last_ = pt;
    if (std::abs(pt.x - start_.x) > kTapMovePx || std::abs(pt.y - start_.y) > kTapMovePx) {
      if (!moved_ && active_) active_->press(pt.x - sx, pt.y - sy, false);
      moved_ = true;
    }
    const uint32_t held = now - downMs_;
    if (moved_ || holdFired_) {
      hideHold();
    } else if (held >= RADAR_RESET_HOLD_S * 1000UL) {
      holdFired_ = true;
      hideHold();
      if (actions_) actions_->resetWifi();
    } else if (held >= kHoldShowMs) {
      showHold(static_cast<float>(held) / (RADAR_RESET_HOLD_S * 1000.0f));
    }
    return;
  }
  if (!down_) return;
  down_ = false;
  hideHold();
  if (active_) active_->press(last_.x - sx, last_.y - sy, false);
  if (holdFired_ || !active_) return;
  const int dx = last_.x - start_.x, dy = last_.y - start_.y;
  if (std::abs(dx) > kSwipePx && std::abs(dx) >= std::abs(dy)) {
    active_->swipe(dx < 0 ? Swipe::Left : Swipe::Right);
    if (active_ == &link_ && dx > 0) back();
  } else if (std::abs(dy) > kSwipeVerticalPx && std::abs(dy) * 10 > std::abs(dx) * 13) {
    active_->swipe(dy < 0 ? Swipe::Up : Swipe::Down);
  } else if (!moved_ && now - downMs_ < kTapMaxMs) {
    if (active_ == &link_)
      back();
    else
      active_->tap(start_.x - sx, start_.y - sy);
  }
}

void Ui::showHold(float progress) {
  if (!hold_) return;
  if (progress > 1) progress = 1;
  if (holdProgress_ < 0) lv_obj_clear_flag(hold_, LV_OBJ_FLAG_HIDDEN);
  if (std::fabs(progress - holdProgress_) >= 0.01f) {
    holdProgress_ = progress;
    holdArt_.invalidate(160, 127, 306, 273);
  }
}

void Ui::hideHold() {
  if (!hold_ || holdProgress_ < 0) return;
  holdProgress_ = -1;
  lv_obj_add_flag(hold_, LV_OBJ_FLAG_HIDDEN);
}

void Ui::openDetail(const std::string& hex) {
  detail_.setTarget(hex);
  radar_.select(hex);
  view_ = View::Detail;
}

void Ui::openAlignment() {
  from_ = view_ == View::Menu ? View::Menu : View::Radar;
  view_ = View::Alignment;
}

void Ui::openPhoneLink() {
  link_.showLink(true);
  from_ = view_ == View::Menu ? View::Menu : View::Radar;
  view_ = View::PhoneLink;
}

void Ui::openMenu() {
  from_ = View::Radar;
  view_ = View::Menu;
}

void Ui::openGuide() {
  guide_.restart();
  from_ = View::Radar;
  view_ = View::Guide;
}

void Ui::closeGuide() {
  guide_.restart();
  if (actions_) actions_->guideDone();
  view_ = View::Radar;
  radar_.showHints();
}

void Ui::back() {
  const bool sub = view_ == View::Alignment || view_ == View::PhoneLink;
  view_ = sub ? from_ : View::Radar;
  from_ = View::Radar;
}

}  // namespace ui
