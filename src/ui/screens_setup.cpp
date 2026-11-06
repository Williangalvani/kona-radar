// Design screens 01-05: splash, phone setup QR, connecting, connected, failed.
#include <cmath>
#include <cstdio>

#include "screens.h"
#include "theme.h"

namespace ui {

void Screen::show(bool on) {
  if (!root_ || on == visible_) return;
  visible_ = on;
  if (on)
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

// ------------------------------------------------------------------ splash

void SplashScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  art_.create(root_, [](draw::Surface& s) {
    const Palette& p = palette();
    draw::ring(s, 233, 151, 68.5f, 1, col(p.grid));
    draw::arc(s, 233, 151, 68.5f, 2, 218, 286, col(p.accent));
    for (int a = -60; a <= 60; a += 30) {
      const float r = a * 3.14159265f / 180.0f;
      draw::segment(s, 233 + std::sin(r) * 61, 151 - std::cos(r) * 61, 233 + std::sin(r) * 67,
                    151 - std::cos(r) * 67, 1, col(p.accentDim));
    }
    draw::segment(s, 206, 166, 260, 158, 5, col(p.runway));
    draw::dashed(s, 208, 166, 258, 158, 1, 4, 3, lv_color_black());
    draw::plane(s, 248, 129, 185 - 281, 25, col(p.amber));
    const float a = (185 - 281) * 3.14159265f / 180.0f;
    for (int i = 1; i <= 3; ++i)
      draw::fillCircle(s, 248 - std::sin(a) * (15 + i * 7), 129 + std::cos(a) * (15 + i * 7), 1.2f, col(p.accentDim));
  });
  const Palette& p = palette();
  kona_.create(root_, 233, 252, font::bold(46), p.text, Align::Center, 6);
  kona_.set("KONA");
  radar_.create(root_, 233, 308, font::medium(20), p.accent);
  radar_.set("R A D A R");
  tagline_.create(root_, 233, 357, font::regular18(), p.muted);
  tagline_.set("A window into the sky.");
  status_.create(root_, 233, 421, font::mono(10), p.quiet, Align::Center, 1);
  status_.set("STARTING");
  bar_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    draw::fillRect(s, 196, 399, 270, 401, col(p.grid));
    const int end = 196 + static_cast<int>(74 * progress_);
    if (end > 196) draw::fillRect(s, 196, 399, end, 401, col(p.accent));
  });
}

void SplashScreen::refresh(const Model& m) {
  if (startMs_ == 0) startMs_ = m.nowMs ? m.nowMs : 1;
  const float target = std::min(0.9f, 0.3f + (m.nowMs - startMs_) / 6000.0f);
  if (std::fabs(target - progress_) > 0.01f) {
    progress_ = target;
    bar_.invalidate(190, 395, 276, 405);
  }
}

// ------------------------------------------------------------------ setup QR

void SetupQrScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  qrData_.clear();
  const Palette& p = palette();
  topHeader(root_, title_, "FIRST FLIGHT");
  heading_.create(root_, 233, 79, font::bold(26), p.text);
  heading_.set("Set up with your phone");
  step1_.create(root_, 233, 113, font::medium(16), p.muted);
  step1_.set("1  Scan to join setup Wi-Fi");
  // Integer 4 px modules on white with a 4-module quiet zone, as designed.
  qrBox_ = panel(root_, 151, 145, 314, 308, 0, 0xFFFFFF, 0xFFFFFF, false);
  qr_ = lv_qrcode_create(root_, 132, lv_color_black(), lv_color_white());
  lv_obj_set_pos(qr_, 167, 161);
  step2_.create(root_, 233, 325, font::medium(16), p.text);
  step2_.set("2  Follow the sign-in page");
  ssid_.create(root_, 233, 371, font::mono(16), p.accent);
  pass_.create(root_, 233, 395, font::mono(13), p.muted);
  hint_.create(root_, 233, 420, font::mono(12), p.quiet);
}

void SetupQrScreen::setQr(const std::string& data) {
  if (data == qrData_) return;
  qrData_ = data;
  lv_qrcode_update(qr_, data.data(), static_cast<uint32_t>(data.size()));
}

void SetupQrScreen::showLink(const std::string& url) {
  linkMode_ = !url.empty();
  linkUrl_ = url;
}

std::string wifiQrPayload(const std::string& ssid, const std::string& pass);

void SetupQrScreen::refresh(const Model& m) {
  const Palette& p = palette();
  if (linkMode_) {
    title_.set("EDIT ON PHONE");
    heading_.set("Open the settings");
    step1_.set("1  Scan with your phone camera");
    step1_.setColor(p.muted);
    step2_.set("2  Use the same Wi-Fi as the radar");
    setQr(linkUrl_);
    ssid_.set(m.lanHost);
    pass_.set(linkUrl_.size() > 7 ? linkUrl_.substr(7) : linkUrl_);
    hint_.set("Swipe right to go back");
    return;
  }
  title_.set("FIRST FLIGHT");
  heading_.set("Set up with your phone");
  const bool joined = m.phase == Phase::SetupPortal;
  step1_.set(joined ? "1  Phone joined" : "1  Scan to join setup Wi-Fi");
  step1_.setColor(joined ? p.sage : p.muted);
  step2_.set("2  Follow the sign-in page");
  setQr(wifiQrPayload(m.apSsid, m.apPassword));
  ssid_.set(m.apSsid);
  pass_.set("PASS  " + m.apPassword);
  hint_.set("No page? " + m.portalIp);
}

// ------------------------------------------------------------------ connecting

void ConnectingScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "WI-FI SETUP");
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    draw::ring(s, 233, 240, 66, 2, col(p.grid));
    draw::arc(s, 233, 240, 66, 3, 238 + spin_, 334 + spin_, col(p.accent));
    for (float r : {35.0f, 35 * 0.67f, 35 * 0.34f}) draw::arc(s, 233, 254, r, 2.4f, 220, 320, col(p.accent));
    draw::fillCircle(s, 233, 254, 3, col(p.accent));
    draw::segment(s, 151, 380, 315, 380, 2, col(p.grid));
    const int xs[3] = {151, 233, 315};
    for (int i = 0; i < 3; ++i) {
      const uint32_t c = i < activeStep_ ? p.sage : (i == activeStep_ ? p.accent : p.grid);
      draw::fillCircle(s, xs[i], 380, 4, col(c));
    }
  });
  heading_.create(root_, 233, 83, font::bold(32), p.text);
  heading_.set("Joining Wi-Fi");
  ssid_.create(root_, 233, 127, font::medium(18), p.muted);
  body_.create(root_, 233, 330, font::medium(18), p.text);
  const char* names[3] = {"PHONE", "WI-FI", "ONLINE"};
  const int xs[3] = {151, 233, 315};
  for (int i = 0; i < 3; ++i) {
    steps_[i].create(root_, xs[i], 396, font::mono(10), p.quiet);
    steps_[i].set(names[i]);
  }
}

void ConnectingScreen::refresh(const Model& m) {
  const Palette& p = palette();
  const bool setup = m.phase == Phase::SetupJoining || m.phase == Phase::SetupOnline;
  title_.set(setup ? "WI-FI SETUP" : "WI-FI");
  steps_[0].set(setup ? "PHONE" : "SAVED");
  const int step = m.phase == Phase::SetupOnline ? 2 : 1;
  heading_.set(step == 2 ? "Going online" : "Joining Wi-Fi");
  ssid_.set(m.wifiSsid);
  body_.set(step == 2 ? "Looking for aircraft\xE2\x80\xA6" : "Checking the connection\xE2\x80\xA6");
  if (step != activeStep_) {
    activeStep_ = step;
    art_.invalidate(140, 370, 326, 390);
  }
  for (int i = 0; i < 3; ++i) steps_[i].setColor(i < step ? p.sage : (i == step ? p.accent : p.quiet));
  if (visible_ && m.nowMs - lastSpinMs_ >= 40) {
    lastSpinMs_ = m.nowMs;
    spin_ = std::fmod(m.nowMs * 0.24f, 360.0f);
    art_.invalidate(162, 169, 304, 216);
    art_.invalidate(162, 216, 304, 311);
  }
}

// ------------------------------------------------------------------ result

void ResultScreen::build(lv_obj_t* parent) {
  root_ = screenRoot(parent);
  const Palette& p = palette();
  topHeader(root_, title_, "WI-FI SETUP");
  art_.create(root_, [this](draw::Surface& s) {
    const Palette& p = palette();
    if (kind_ == Kind::Connected) {
      draw::fillCircle(s, 233, 224, 60, col(p.night ? 0x100B07 : 0x0B1610));
      draw::ring(s, 233, 224, 59.5f, 1, col(p.night ? 0x472D1A : 0x294635));
      const draw::Point check[3] = {{233 - 21, 225}, {233 - 6.3f, 225 + 14.7f}, {233 + 23.8f, 225 - 18.2f}};
      draw::polyline(s, check, 3, 4, col(p.sage));
    } else {
      draw::fillCircle(s, 233, 201, 52, col(p.night ? 0x100B07 : 0x1B0D08));
      draw::ring(s, 233, 201, 51.5f, 1, col(p.night ? 0x472D1A : 0x593024));
      draw::segment(s, 233, 177, 233, 211, 4, col(p.coral));
      draw::fillCircle(s, 233, 226, 3, col(p.coral));
    }
  });
  heading_.create(root_, 233, 83, font::bold(32), p.text);
  line1_.create(root_, 233, 309, font::medium(20), p.sage);
  line2_.create(root_, 233, 340, font::regular18(), p.muted);
  hint_.create(root_, 233, 409, font::medium(16), p.muted);
  hint_.set("Hold 5s to reset Wi-Fi");
  button_.create(root_, 372, "OPEN RADAR", p.sage, 214);
  kind_ = Kind::Failed;
  setKind(Kind::Connected);
}

void ResultScreen::setKind(Kind k) {
  if (k == kind_) return;
  kind_ = k;
  const Palette& p = palette();
  const bool ok = k == Kind::Connected;
  line1_.setFont(ok ? font::medium(20) : font::regular18());
  line1_.setColor(ok ? p.sage : p.muted);
  line1_.moveTo(233, ok ? 309 : 278);
  line2_.moveTo(233, ok ? 340 : 304);
  hint_.show(!ok);
  const int width = ok ? 214 : 218;
  const int y = ok ? 372 : 345;
  button_.x0 = 233 - width / 2;
  button_.x1 = 233 + width / 2;
  button_.y0 = y;
  button_.y1 = y + 48;
  lv_obj_set_pos(button_.box, static_cast<lv_coord_t>(button_.x0), static_cast<lv_coord_t>(y));
  lv_obj_set_size(button_.box, static_cast<lv_coord_t>(width + 1), 49);
  button_.label.moveTo(233, y + 14);
  button_.setColor(ok ? p.sage : p.coral);
  button_.label.set(ok ? "OPEN RADAR" : (k == Kind::Offline ? "SET UP WI-FI" : "TRY AGAIN"));
  art_.invalidate();
}

void ResultScreen::refresh(const Model& m) {
  if (m.phase == Phase::SetupDone) {
    setKind(Kind::Connected);
    heading_.set("You\xE2\x80\x99re connected");
    line1_.set(m.wifiSsid);
    line2_.set("Your sky is ready.");
    return;
  }
  if (m.phase == Phase::Offline) {
    setKind(Kind::Offline);
    heading_.set("Wi-Fi not found");
    line1_.set("Can\xE2\x80\x99t reach " + (m.wifiSsid.size() > 18 ? m.wifiSsid.substr(0, 17) + "\xE2\x80\xA6" : m.wifiSsid) + ".");
    line2_.set("Still trying in the background.");
    return;
  }
  setKind(Kind::Failed);
  switch (m.setupFailure) {
    case SetupFailure::NotFound:
      heading_.set("Network not found");
      line1_.set("Pick a 2.4 GHz network");
      line2_.set("close to the radar.");
      break;
    case SetupFailure::Refused:
      heading_.set("Couldn\xE2\x80\x99t connect");
      line1_.set("The router refused the radar.");
      line2_.set("Wait a moment and try again.");
      break;
    case SetupFailure::NoInternet:
      heading_.set("No internet");
      line1_.set("Wi-Fi works, but flight data");
      line2_.set("can\xE2\x80\x99t be reached right now.");
      break;
    case SetupFailure::WrongPassword:
    default:
      heading_.set("Couldn\xE2\x80\x99t connect");
      line1_.set("Check the Wi-Fi name");
      line2_.set("or password on your phone.");
      break;
  }
}

void ResultScreen::press(int x, int y, bool down) { button_.setPressed(down && button_.contains(x, y)); }

void ResultScreen::tap(int x, int y) {
  if (!button_.contains(x, y) || !actions_) return;
  if (kind_ == Kind::Connected)
    actions_->openRadar();
  else
    actions_->retrySetup();
}

}  // namespace ui
