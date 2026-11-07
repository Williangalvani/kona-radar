// Screens beyond the original design set: the menu, the gesture guide and
// the firmware-update progress. Same black, low-light style.
#include <algorithm>
#include <cstdio>

#include "rotation.h"
#include "screens.h"
#include "theme.h"

namespace ui {
namespace {

void backChevron(draw::Surface& s) {
  const draw::Point pts[3] = {{113, 70}, {104, 79}, {113, 88}};
  draw::polyline(s, pts, 3, 2, col(palette().muted));
}

bool onBack(int x, int y) { return x < 170 && y < 106; }

constexpr int kRowX0 = 103, kRowX1 = 363, kRowTop = 106, kRowH = 38, kRowGap = 6;

int rowTop(int i) { return kRowTop + i * (kRowH + kRowGap); }

const char* const kMenuLabels[MenuScreen::kItems] = {
    "Settings on phone", "Balcony alignment", "Night colors", "Screen rotation", "Quiet screen", "Gestures guide",
};

enum MenuRow { kRowPhone, kRowAlignment, kRowNight, kRowRotation, kRowQuiet, kRowGuide };

uint32_t pressedFill() {
  const Palette& p = palette();
  return lv_color_to32(lv_color_mix(col(p.panelLine), col(p.panel), 110)) & 0xFFFFFF;
}

}  // namespace

// ------------------------------------------------------------------ menu

void MenuScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "MENU");
  art_.create(root_, [](draw::Surface& s) { backChevron(s); });
  for (int i = 0; i < kItems; ++i) {
    const int y = rowTop(i);
    rows_[i] = panel(root_, kRowX0, y, kRowX1, y + kRowH, 12, p.panel, p.panelLine);
    labels_[i].create(root_, kRowX0 + 18, y + 9, font::medium(18), p.text, Align::Left);
    labels_[i].set(kMenuLabels[i]);
    values_[i].create(root_, kRowX1 - 18, y + 13, font::mono(11), p.muted, Align::Right, 1);
  }
  ip_.create(root_, 233, 378, font::mono(13), p.accent);
  ota_.create(root_, 233, 401, font::mono(11), p.muted, Align::Center, 1);
  ota_.setMaxWidth(300);
  footer(root_, footer_, "", p.quiet);
  pressed_ = -1;
}

void MenuScreen::refresh(const Model& m) {
  const Palette& p = palette();
  values_[kRowPhone].set("QR \xE2\x86\x92");
  values_[kRowAlignment].set("\xE2\x86\x92");
  values_[kRowNight].set(m.night ? "ON" : "OFF");
  values_[kRowNight].setColor(m.night ? p.amber : p.muted);
  rotation_ = m.settings.screenRotation & 3;
  char deg[8];
  std::snprintf(deg, sizeof(deg), "%d\xC2\xB0", radar::rotationDegrees(rotation_));
  values_[kRowRotation].set(deg);
  values_[kRowRotation].setColor(rotation_ ? p.accent : p.muted);
  values_[kRowQuiet].set("\xE2\x86\x93");
  values_[kRowGuide].set("?");
  if (m.online && !m.lanIp.empty())
    ip_.set("http://" + m.lanIp + "/");
  else
    ip_.set("NOT CONNECTED");
  ip_.setColor(m.online ? p.accent : p.quiet);
  ota_.set(m.otaPassword.empty() ? "" : "UPDATE PASSWORD  " + m.otaPassword);
  footer_.set(m.firmwareVersion.empty() ? "KONA RADAR" : "FIRMWARE " + m.firmwareVersion);
}

int MenuScreen::itemAt(int x, int y) const {
  if (x < kRowX0 - 6 || x > kRowX1 + 6) return -1;
  for (int i = 0; i < kItems; ++i)
    if (y >= rowTop(i) - kRowGap / 2 && y < rowTop(i) + kRowH + kRowGap / 2) return i;
  return -1;
}

void MenuScreen::press(int x, int y, bool down) {
  const int i = down ? itemAt(x, y) : -1;
  if (i == pressed_) return;
  const Palette& p = palette();
  if (pressed_ >= 0) setPanelColors(rows_[pressed_], p.panel, p.panelLine);
  pressed_ = i;
  if (i >= 0) setPanelColors(rows_[i], pressedFill(), p.panelLine);
}

void MenuScreen::tap(int x, int y) {
  if (onBack(x, y)) {
    if (nav_) nav_->back();
    return;
  }
  switch (itemAt(x, y)) {
    case kRowPhone:
      if (nav_) nav_->openPhoneLink();
      break;
    case kRowAlignment:
      if (nav_) nav_->openAlignment();
      break;
    case kRowNight:
      if (actions_) actions_->toggleNight();
      break;
    case kRowRotation:
      // The menu stays open, so tapping again keeps turning until the text
      // reads upright. The row is at the same place on the turned picture.
      if (actions_) actions_->setScreenRotation((rotation_ + 1) & 3);
      break;
    case kRowQuiet:
      if (nav_) nav_->back();
      if (actions_) actions_->goIdle();
      break;
    case kRowGuide:
      if (nav_) nav_->openGuide();
      break;
    default:
      break;
  }
}

void MenuScreen::swipe(Swipe s) {
  if (s == Swipe::Right && nav_) nav_->back();
}

// ------------------------------------------------------------------ guide

namespace {

struct GuidePage {
  const char* heading;
  const char* lines[3];
};

const GuidePage kGuide[GuideScreen::kPages] = {
    {"Tap",
     {"Tap a plane for its details", "Tap the title for balcony alignment", "Tap MENU for settings and more"}},
    {"Swipe",
     {"\xE2\x86\x90 \xE2\x86\x92  Zoom in and out", "\xE2\x86\x91  Night colors on and off",
      "\xE2\x86\x93  Quiet screen (tap to wake)"}},
    {"Hold",
     {"Hold 5 seconds to reset Wi-Fi", "MENU: phone address, rotation",
      "MENU \xE2\x86\x92 Gestures guide shows this again"}},
};

}  // namespace

void GuideScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "HOW TO USE");
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    const float cx = 233, cy = 168;
    if (page_ == 0) {
      draw::ring(s, cx, cy, 34, 1.5f, col(p.grid));
      draw::ring(s, cx, cy, 22, 2, col(p.accentDim));
      draw::fillCircle(s, cx, cy, 9, col(p.accent));
    } else if (page_ == 1) {
      draw::fillCircle(s, cx, cy, 8, col(p.accent));
      draw::arrow(s, cx - 40, cy, 270, 22, 3, col(p.accent));
      draw::arrow(s, cx + 40, cy, 90, 22, 3, col(p.accent));
      draw::arrow(s, cx, cy - 40, 0, 22, 3, col(p.amber));
      draw::arrow(s, cx, cy + 40, 180, 22, 3, col(p.muted));
    } else {
      draw::ring(s, cx, cy, 32, 5, col(p.grid));
      draw::arc(s, cx, cy, 32, 5, 270, 270 + 250, col(p.coral));
      draw::fillCircle(s, cx, cy, 8, col(p.accent));
    }
  });
  heading_.create(root_, 233, 74, font::bold(29), p.text);
  for (int i = 0; i < 3; ++i) lines_[i].create(root_, 233, 236 + i * 40, font::medium(18), i == 0 ? p.text : p.muted);
  footer(root_, footer_, "", p.quiet);
  setPage(page_);
}

void GuideScreen::setPage(int page) {
  page_ = std::max(0, std::min(kPages - 1, page));
  heading_.set(kGuide[page_].heading);
  for (int i = 0; i < 3; ++i) lines_[i].set(kGuide[page_].lines[i]);
  char buf[40];
  if (page_ == kPages - 1)
    std::snprintf(buf, sizeof(buf), "TAP TO START \xC2\xB7 %d/%d", page_ + 1, kPages);
  else
    std::snprintf(buf, sizeof(buf), "TAP TO CONTINUE \xC2\xB7 %d/%d", page_ + 1, kPages);
  footer_.set(buf);
  art_.invalidate(180, 115, 286, 221);
}

void GuideScreen::restart() {
  finished_ = false;
  if (root_) setPage(0);
  page_ = 0;
}

void GuideScreen::refresh(const Model&) {}

void GuideScreen::tap(int, int) {
  if (page_ + 1 < kPages)
    setPage(page_ + 1);
  else
    finished_ = true;
}

void GuideScreen::swipe(Swipe s) {
  if (s == Swipe::Right && page_ > 0) setPage(page_ - 1);
  if (s == Swipe::Left) tap(0, 0);
}

// ------------------------------------------------------------------ update

void UpdateScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "FIRMWARE");
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    draw::ring(s, 233, 236, 78, 6, col(p.grid));
    if (pct_ > 0) draw::arc(s, 233, 236, 78, 6, 270, 270 + 3.6f * std::min(pct_, 100), col(p.accent));
  });
  heading_.create(root_, 233, 83, font::bold(32), p.text);
  percent_.create(root_, 233, 207, font::bold(53), p.accent);
  body_.create(root_, 233, 340, font::medium(18), p.muted);
  footer(root_, footer_, "DO NOT UNPLUG", p.quiet);
  pct_ = -1;
}

void UpdateScreen::refresh(const Model& m) {
  const int pct = std::max(0, std::min(100, m.otaProgress));
  if (pct == pct_) return;
  pct_ = pct;
  heading_.set(pct >= 100 ? "Restarting" : "Updating");
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%d%%", pct);
  percent_.set(buf);
  body_.set(pct >= 100 ? "Back in a few seconds" : "Keep the radar plugged in");
  art_.invalidate(147, 150, 319, 322);
}

}  // namespace ui
