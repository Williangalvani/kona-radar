#include "ui.h"

#include <cmath>
#include <cstdlib>

#include "config.h"
#include "theme.h"

namespace ui {
namespace {

constexpr int kTapMovePx = 18;
constexpr int kSwipePx = 45;
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
  stage_ = screenRoot(lv_scr_act());
  lv_obj_set_pos(stage_, static_cast<lv_coord_t>(shiftX_), static_cast<lv_coord_t>(shiftY_));
  Screen* all[] = {&splash_, &setupQr_, &link_, &connecting_, &result_, &radar_,
                   &detail_, &alert_, &idle_, &alignment_};
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
}

void Ui::setPixelShift(int dx, int dy) {
  if (dx == shiftX_ && dy == shiftY_) return;
  shiftX_ = dx;
  shiftY_ = dy;
  if (stage_) lv_obj_set_pos(stage_, static_cast<lv_coord_t>(dx), static_cast<lv_coord_t>(dy));
}

Screen* Ui::route(const Model& m) {
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
    view_ = View::Radar;
    return &idle_;
  }
  if (!m.alertHex.empty() && findTarget(m, m.alertHex)) return &alert_;
  switch (view_) {
    case View::Detail:
      return &detail_;
    case View::Alignment:
      return &alignment_;
    case View::PhoneLink:
      return &link_;
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
  Screen* s = route(m_);
  if (s != active_) {
    if (active_) active_->show(false);
    s->show(true);
    active_ = s;
  }
  active_->refresh(m_);
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
  } else if (std::abs(dy) > kSwipePx) {
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

void Ui::openAlignment() { view_ = View::Alignment; }

void Ui::openPhoneLink() {
  link_.showLink("http://" + (m_.lanIp.empty() ? m_.lanHost : m_.lanIp) + "/");
  view_ = View::PhoneLink;
}

void Ui::back() { view_ = View::Radar; }

}  // namespace ui
