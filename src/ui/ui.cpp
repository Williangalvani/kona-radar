#include "ui.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "format.h"
#include "theme.h"

namespace ui {

const Theme& theme() {
  static const Theme t;
  return t;
}

namespace {

constexpr int W = 466;
constexpr int kQrSize = 170;
constexpr int kQrPad = 8;
// Radar overlay layout
constexpr int kHeaderY = 50;       // top of the clock
constexpr int kFooterBottom = 46;  // range buttons' distance from the bottom edge
constexpr int kRangeBtnDx = 82;    // range buttons' offset from center

enum ButtonId : intptr_t {
  kBtnRotate = 1,
  kBtnView,
  kBtnPhone,
  kBtnWifi,
  kBtnClose,
  kBtnConfirmYes,
  kBtnConfirmNo,
};

void setText(lv_obj_t* label, const char* text) {
  const char* cur = lv_label_get_text(label);
  if (cur && std::strcmp(cur, text) == 0) return;
  lv_label_set_text(label, text);
}

void setText(lv_obj_t* label, const std::string& text) { setText(label, text.c_str()); }

void setVisible(lv_obj_t* obj, bool on) {
  if (on == !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
  if (on) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color, int y,
                    lv_text_align_t align = LV_TEXT_ALIGN_CENTER, int width = 360) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, col(color), 0);
  lv_obj_set_style_text_align(l, align, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, width);
  lv_label_set_text(l, "");
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
  return l;
}

lv_obj_t* makePanel(lv_obj_t* parent, bool round) {
  const Theme& t = theme();
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_style_all(p);
  lv_obj_set_size(p, W, W);
  lv_obj_set_pos(p, 0, 0);
  lv_obj_set_style_bg_color(p, col(t.panelBg), 0);
  lv_obj_set_style_bg_opa(p, round ? t.panelOpa : LV_OPA_COVER, 0);
  lv_obj_set_style_radius(p, round ? LV_RADIUS_CIRCLE : 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int w, int h, lv_event_cb_t cb, void* ud,
                     intptr_t id, lv_obj_t** labelOut = nullptr) {
  const Theme& t = theme();
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, col(t.buttonBg), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, col(t.buttonBorder), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, col(t.buttonBorder), 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_user_data(b, reinterpret_cast<void*>(id));
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  lv_obj_t* l = lv_label_create(b);
  lv_obj_set_style_text_font(l, t.fontBody, 0);
  lv_obj_set_style_text_color(l, col(t.text), 0);
  lv_label_set_text(l, text);
  lv_obj_center(l);
  if (labelOut) *labelOut = l;
  return b;
}

lv_obj_t* makeQr(lv_obj_t* parent, int y, lv_obj_t** qrOut) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, kQrSize + 2 * kQrPad, kQrSize + 2 * kQrPad);
  lv_obj_set_style_bg_color(box, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 6, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(box, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_t* qr = lv_qrcode_create(box, kQrSize, lv_color_black(), lv_color_white());
  lv_obj_center(qr);
  *qrOut = qr;
  return box;
}

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}

std::string ageText(uint32_t s) {
  if (s < 90) return fmt("%u s", static_cast<unsigned>(s));
  if (s < 5400) return fmt("%u min", static_cast<unsigned>((s + 30) / 60));
  return fmt("%u h", static_cast<unsigned>((s + 1800) / 3600));
}

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
  scr_ = lv_scr_act();
  lv_obj_set_style_bg_color(scr_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr_, LV_OBJ_FLAG_SCROLLABLE);
  if (!radar_.create(scr_)) return false;
  radar_.setModel(&m_);
  lv_obj_add_event_cb(radar_.obj(), onRadarEvent, LV_EVENT_SHORT_CLICKED, this);
  lv_obj_add_event_cb(radar_.obj(), onRadarEvent, LV_EVENT_LONG_PRESSED, this);
  buildRadarScreen();
  buildDetails();
  buildSettings();
  buildPhoneQr();
  buildConfirm();
  buildSetup();
  setVisible(setup_, true);
  setVisible(radar_.obj(), false);
  setVisible(hud_, false);
  return true;
}

void Ui::buildRadarScreen() {
  const Theme& t = theme();
  hud_ = lv_obj_create(scr_);
  lv_obj_remove_style_all(hud_);
  lv_obj_set_size(hud_, W, W);
  lv_obj_clear_flag(hud_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(hud_, LV_OBJ_FLAG_CLICKABLE);

  clock_ = makeLabel(hud_, t.fontLarge, t.text, kHeaderY, LV_TEXT_ALIGN_CENTER, 200);
  info_ = makeLabel(hud_, t.fontSmall, t.textDim, kHeaderY + 26, LV_TEXT_ALIGN_CENTER, 260);
  status_ = makeLabel(hud_, t.fontSmall, t.warn, kHeaderY + 46, LV_TEXT_ALIGN_CENTER, 300);

  minusBtn_ = makeButton(hud_, LV_SYMBOL_MINUS, 56, 42, onRangeButton, this, -1);
  plusBtn_ = makeButton(hud_, LV_SYMBOL_PLUS, 56, 42, onRangeButton, this, +1);
  lv_obj_align(minusBtn_, LV_ALIGN_BOTTOM_MID, -kRangeBtnDx, -kFooterBottom);
  lv_obj_align(plusBtn_, LV_ALIGN_BOTTOM_MID, kRangeBtnDx, -kFooterBottom);
  lv_obj_set_style_bg_opa(minusBtn_, LV_OPA_70, 0);
  lv_obj_set_style_bg_opa(plusBtn_, LV_OPA_70, 0);
  rangeLabel_ = lv_label_create(hud_);
  lv_obj_set_style_text_font(rangeLabel_, t.fontLarge, 0);
  lv_obj_set_style_text_color(rangeLabel_, col(t.text), 0);
  lv_label_set_text(rangeLabel_, "");
  lv_obj_align(rangeLabel_, LV_ALIGN_BOTTOM_MID, 0, -kFooterBottom - 10);

  const lv_coord_t footerTop = static_cast<lv_coord_t>(W - kFooterBottom - 42 - 4);
  radar_.setKeepOut({
      {143, static_cast<lv_coord_t>(kHeaderY - 2), 323, static_cast<lv_coord_t>(kHeaderY + 66)},
      {static_cast<lv_coord_t>(W / 2 - kRangeBtnDx - 32), footerTop,
       static_cast<lv_coord_t>(W / 2 + kRangeBtnDx + 32), static_cast<lv_coord_t>(W - kFooterBottom + 4)},
  });
}

void Ui::buildDetails() {
  const Theme& t = theme();
  details_ = makePanel(scr_, true);
  lv_obj_add_event_cb(details_, onOverlayTap, LV_EVENT_SHORT_CLICKED, this);
  dTitle_ = makeLabel(details_, t.fontHuge, t.selected, 52, LV_TEXT_ALIGN_CENTER, 320);
  dOperator_ = makeLabel(details_, t.fontBody, t.text, 96, LV_TEXT_ALIGN_CENTER, 340);
  dType_ = makeLabel(details_, t.fontBody, t.textDim, 118, LV_TEXT_ALIGN_CENTER, 360);
  dLook_ = makeLabel(details_, t.fontLarge, t.accent, 150, LV_TEXT_ALIGN_CENTER, 400);
  dKeys_ = lv_label_create(details_);
  lv_obj_set_style_text_font(dKeys_, t.fontBody, 0);
  lv_obj_set_style_text_color(dKeys_, col(t.textDim), 0);
  lv_obj_set_style_text_align(dKeys_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(dKeys_, 150);
  lv_obj_set_pos(dKeys_, 70, 190);
  dValues_ = lv_label_create(details_);
  lv_obj_set_style_text_font(dValues_, t.fontBody, 0);
  lv_obj_set_style_text_color(dValues_, col(t.text), 0);
  lv_obj_set_width(dValues_, 200);
  lv_obj_set_pos(dValues_, 234, 190);
  lv_label_set_text(dKeys_, "");
  lv_label_set_text(dValues_, "");
  lv_obj_t* hint = makeLabel(details_, t.fontSmall, t.textDim, 396);
  lv_label_set_text(hint, "Tap to close");
}

void Ui::buildSettings() {
  const Theme& t = theme();
  settings_ = makePanel(scr_, true);
  lv_obj_set_style_bg_opa(settings_, LV_OPA_COVER, 0);
  lv_obj_t* title = makeLabel(settings_, t.fontTitle, t.accent, 40);
  lv_label_set_text(title, "Settings");
  lv_obj_t* bl = makeLabel(settings_, t.fontSmall, t.textDim, 80);
  lv_label_set_text(bl, LV_SYMBOL_IMAGE "  Brightness");
  slider_ = lv_slider_create(settings_);
  lv_obj_set_width(slider_, 250);
  lv_obj_align(slider_, LV_ALIGN_TOP_MID, 0, 108);
  lv_slider_set_range(slider_, 5, 100);
  lv_obj_set_style_bg_color(slider_, col(t.buttonBg), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider_, col(t.accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider_, col(t.text), LV_PART_KNOB);
  lv_obj_add_event_cb(slider_, onBrightness, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t* b;
  b = makeButton(settings_, "", 270, 42, onSettingsButton, this, kBtnView, &viewBtnLabel_);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 142);
  b = makeButton(settings_, "", 270, 42, onSettingsButton, this, kBtnRotate, &rotateBtnLabel_);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 192);
  b = makeButton(settings_, LV_SYMBOL_SETTINGS "  Phone settings", 270, 42, onSettingsButton, this, kBtnPhone);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 242);
  b = makeButton(settings_, LV_SYMBOL_WIFI "  Change Wi-Fi", 270, 42, onSettingsButton, this, kBtnWifi);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 292);
  b = makeButton(settings_, "Close", 150, 42, onSettingsButton, this, kBtnClose);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 350);
  settingsInfo_ = makeLabel(settings_, t.fontSmall, t.textDim, 404, LV_TEXT_ALIGN_CENTER, 260);
}

void Ui::buildPhoneQr() {
  const Theme& t = theme();
  phoneQrPanel_ = makePanel(scr_, true);
  lv_obj_set_style_bg_opa(phoneQrPanel_, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(phoneQrPanel_, onOverlayTap, LV_EVENT_SHORT_CLICKED, this);
  lv_obj_t* title = makeLabel(phoneQrPanel_, t.fontTitle, t.accent, 44);
  lv_label_set_text(title, "Phone settings");
  lv_obj_t* step = makeLabel(phoneQrPanel_, t.fontBody, t.text, 80, LV_TEXT_ALIGN_CENTER, 320);
  lv_label_set_text(step, "Scan with a phone on\nthe same Wi-Fi");
  makeQr(phoneQrPanel_, 128, &phoneQr_);
  phoneQrText_ = makeLabel(phoneQrPanel_, t.fontSmall, t.textDim, 326, LV_TEXT_ALIGN_CENTER, 320);
}

void Ui::buildConfirm() {
  const Theme& t = theme();
  confirm_ = makePanel(scr_, true);
  lv_obj_set_style_bg_opa(confirm_, LV_OPA_COVER, 0);
  lv_obj_t* title = makeLabel(confirm_, t.fontTitle, t.warn, 110);
  lv_label_set_text(title, "Change Wi-Fi?");
  lv_obj_t* text = makeLabel(confirm_, t.fontBody, t.text, 156, LV_TEXT_ALIGN_CENTER, 330);
  lv_label_set_text(text,
                    "The radar shows a setup code\nfor your phone. Cancel on that\n"
                    "screen to keep the current\nnetwork.");
  lv_obj_t* yes = makeButton(confirm_, "Set up", 140, 46, onSettingsButton, this, kBtnConfirmYes);
  lv_obj_align(yes, LV_ALIGN_TOP_MID, -78, 290);
  lv_obj_t* no = makeButton(confirm_, "Cancel", 140, 46, onSettingsButton, this, kBtnConfirmNo);
  lv_obj_align(no, LV_ALIGN_TOP_MID, 78, 290);
}

void Ui::buildSetup() {
  const Theme& t = theme();
  setup_ = makePanel(scr_, false);
  lv_obj_clear_flag(setup_, LV_OBJ_FLAG_HIDDEN);
  sTitle_ = makeLabel(setup_, t.fontTitle, t.accent, 40, LV_TEXT_ALIGN_CENTER, 260);
  sStep_ = makeLabel(setup_, t.fontBody, t.text, 76, LV_TEXT_ALIGN_CENTER, 330);
  sError_ = makeLabel(setup_, t.fontSmall, t.warn, 120, LV_TEXT_ALIGN_CENTER, 360);
  lv_obj_t* box = makeQr(setup_, 142, &sQr_);
  lv_obj_set_user_data(sQr_, box);
  sText_ = makeLabel(setup_, t.fontBody, t.text, 334, LV_TEXT_ALIGN_CENTER, 360);
  sSpinner_ = lv_spinner_create(setup_, 1000, 60);
  lv_obj_set_size(sSpinner_, 72, 72);
  lv_obj_align(sSpinner_, LV_ALIGN_TOP_MID, 0, 190);
  lv_obj_set_style_arc_color(sSpinner_, col(t.buttonBg), LV_PART_MAIN);
  lv_obj_set_style_arc_color(sSpinner_, col(t.accent), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(sSpinner_, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sSpinner_, 6, LV_PART_INDICATOR);
  sIcon_ = makeLabel(setup_, t.fontHuge, t.accent, 206, LV_TEXT_ALIGN_CENTER, 120);
  sButton_ = makeButton(setup_, "", 160, 44, onSetupButton, this, 0, &sButtonLabel_);
  lv_obj_align(sButton_, LV_ALIGN_TOP_MID, 0, 392);
}

void Ui::setQr(lv_obj_t* qr, std::string& cache, const std::string& data) {
  if (data == cache) return;
  cache = data;
  lv_qrcode_update(qr, data.data(), static_cast<uint32_t>(data.size()));
}

void Ui::setPixelShift(int dx, int dy) {
  if (dx == shiftX_ && dy == shiftY_) return;
  shiftX_ = dx;
  shiftY_ = dy;
  if (hud_) lv_obj_set_pos(hud_, dx, dy);
}

void Ui::showOverlay(Overlay o) {
  overlay_ = o;
  setVisible(details_, o == Overlay::Details);
  setVisible(settings_, o == Overlay::Settings);
  setVisible(phoneQrPanel_, o == Overlay::PhoneQr);
  setVisible(confirm_, o == Overlay::ConfirmWifi);
  if (o != Overlay::Details) radar_.setSelected(nullptr);
  radar_.setAnimating(o == Overlay::None || o == Overlay::Details);
  if (o == Overlay::Settings) {
    lv_slider_set_value(slider_, m_.settings.brightness, LV_ANIM_OFF);
    updateSettingsLabels();
  }
  if (o == Overlay::PhoneQr) {
    if (m_.online && !m_.lanIp.empty()) {
      setQr(phoneQr_, phoneQrData_, "http://" + m_.lanIp + "/");
      setText(phoneQrText_, "or open\nhttp://" + m_.lanHost + "/\nhttp://" + m_.lanIp + "/");
    } else {
      setQr(phoneQr_, phoneQrData_, "Not connected");
      setText(phoneQrText_, "Connect the radar to Wi-Fi first.");
    }
  }
  if (o == Overlay::Details) {
    lastDetailsMs_ = 0;
    updateDetails();
  }
}

void Ui::openDetails(const char* hex) {
  radar_.setSelected(hex);
  showOverlay(Overlay::Details);
  radar_.setSelected(hex);
}

void Ui::openSettings() { showOverlay(Overlay::Settings); }

void Ui::closeOverlays() { showOverlay(Overlay::None); }

void Ui::update(const Model& m) {
  const bool first = !haveModel_;
  m_ = m;
  haveModel_ = true;
  if (first) {
    lastAlignedBearing_ = m.settings.viewBearingDeg != 0 ? m.settings.viewBearingDeg
                                                         : static_cast<int16_t>(RADAR_VIEW_BEARING_DEG);
    if (lastAlignedBearing_ == 0) lastAlignedBearing_ = -1;
  }
  if (m.settings.viewBearingDeg != 0) lastAlignedBearing_ = m.settings.viewBearingDeg;

  const bool running = m.phase == Phase::Running;
  if (running != (shownPhase_ == Phase::Running) || first) {
    setVisible(setup_, !running);
    setVisible(radar_.obj(), running);
    setVisible(hud_, running);
    if (!running) closeOverlays();
  }
  shownPhase_ = m.phase;

  if (!running) {
    updateSetup();
    return;
  }
  if (m.dimmed && overlay_ != Overlay::None) closeOverlays();

  radar_.configure(m.settings, shiftX_, shiftY_);
  radar_.tick(m.nowMs);

  if (m.nowMs - lastHeaderMs_ >= 250 || first) {
    lastHeaderMs_ = m.nowMs;
    updateHeader();
  }
  if (overlay_ == Overlay::Details && m.nowMs - lastDetailsMs_ >= 1000) updateDetails();
}

void Ui::updateHeader() {
  const Theme& t = theme();
  setText(clock_, m_.clock);
  const size_t n = radar_.visibleCount();
  setText(info_, fmt("%s  " LV_SYMBOL_BULLET "  %u aircraft", m_.settings.centerName, static_cast<unsigned>(n)));
  setText(rangeLabel_, fmt("%u NM", m_.settings.rangeNm));

  std::string status;
  uint32_t color = t.warn;
  if (!m_.online) {
    status = "Wi-Fi lost, reconnecting";
  } else if (!m_.feedEverOk) {
    status = m_.feedError.empty() ? "Loading aircraft" LV_SYMBOL_DOWNLOAD : "No data: " + m_.feedError;
    if (m_.feedError.empty()) color = t.textDim;
  } else if (!m_.feedOk || m_.feedAgeS > 30) {
    status = "Data " + ageText(m_.feedAgeS) + " old";
  } else if (n == 0) {
    status = "No aircraft in range";
    color = t.textDim;
  }
  lv_obj_set_style_text_color(status_, col(color), 0);
  setText(status_, status);

  size_t count;
  const uint16_t* steps = radar::rangeSteps(count);
  const size_t idx = radar::nearestRangeStep(m_.settings.rangeNm);
  if (idx == 0) {
    lv_obj_add_state(minusBtn_, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(minusBtn_, LV_STATE_DISABLED);
  }
  if (idx + 1 >= count) {
    lv_obj_add_state(plusBtn_, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(plusBtn_, LV_STATE_DISABLED);
  }
  (void)steps;
}

void Ui::updateDetails() {
  lastDetailsMs_ = m_.nowMs;
  const char* hex = radar_.selected();
  const radar::Track* tr = (hex && m_.tracker) ? m_.tracker->find(hex) : nullptr;
  if (!tr) {
    if (hex) {
      setText(dLook_, "Signal lost");
      return;
    }
    closeOverlays();
    return;
  }
  const radar::Aircraft& a = tr->ac;
  const radar::LatLon pos = radar::Tracker::predict(*tr, m_.nowMs);
  char buf[64];

  setText(dTitle_, radar::displayName(a, buf, sizeof(buf)));
  setText(dOperator_, radar::operatorName(a, buf, sizeof(buf)));
  setText(dType_, radar::aircraftTypeName(a, buf, sizeof(buf)));

  const double tgtAlt = a.onGround || !a.hasAlt ? 0.0 : a.altFt;
  const radar::LookAngles look =
      radar::lookAngles(m_.settings.observer(), m_.settings.observerAltFt, pos, tgtAlt);
  const long brg = std::lround(look.bearingDeg) % 360;
  std::string lookText = fmt("Look %s %ld\xC2\xB0", radar::compassPoint(look.bearingDeg), brg);
  if (a.hasAlt && !a.onGround) {
    lookText += look.elevationDeg >= 0 ? fmt("  " LV_SYMBOL_UP " %.0f\xC2\xB0 up", look.elevationDeg)
                                       : std::string("  below horizon");
  }
  setText(dLook_, lookText);

  const double fromCenter = radar::distanceNm(m_.settings.center(), pos);
  char alt[24], spd[16], num[16];
  radar::formatAltitude(a, alt, sizeof(alt));
  const int trend = radar::verticalTrend(a);
  std::string altText = alt;
  if (trend != 0 && a.hasVertRate) {
    altText += fmt(" %s %s", trend > 0 ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                   radar::withThousands(std::lround(std::fabs(a.vertRateFpm)), num, sizeof(num)));
  }
  radar::formatSpeed(a, spd, sizeof(spd));
  std::string spdText = spd;
  if (a.hasGs) spdText += fmt("  %.0f mph", a.gsKt * 1.15078);

  std::string keys = "From you\nFrom ";
  keys += m_.settings.centerName;
  keys += "\nAltitude\nSpeed\nTrack";
  std::string vals = fmt("%.1f NM  %.1f km\n%.1f NM\n", look.groundDistNm, look.groundDistNm * 1.852, fromCenter);
  vals += altText + "\n" + spdText + "\n";
  vals += a.hasTrack ? fmt("%.0f\xC2\xB0 %s", a.trackDeg, radar::compassPoint(a.trackDeg)) : std::string("--");
  if (a.squawk[0]) {
    keys += "\nSquawk";
    vals += "\n";
    vals += a.squawk;
    if (a.emergency) vals += "  EMERGENCY";
  }
  if (a.reg[0]) {
    keys += "\nReg";
    vals += "\n";
    vals += a.reg;
  }
  setText(dKeys_, keys);
  setText(dValues_, vals);
}

void Ui::updateSettingsLabels() {
  const int16_t vb = m_.settings.viewBearingDeg;
  if (vb == 0) {
    setText(viewBtnLabel_, LV_SYMBOL_UP "  View: north up");
  } else if (vb < 0) {
    setText(viewBtnLabel_, fmt(LV_SYMBOL_EYE_OPEN "  View: facing %s", m_.settings.centerName));
  } else {
    setText(viewBtnLabel_, fmt(LV_SYMBOL_EYE_OPEN "  View: facing %d\xC2\xB0", vb));
  }
  setText(rotateBtnLabel_, fmt(LV_SYMBOL_REFRESH "  Rotate screen  %d\xC2\xB0", m_.settings.screenRotation * 90));
  std::string info = m_.online ? m_.lanIp : std::string("offline");
  info += "  " LV_SYMBOL_BULLET "  v" RADAR_FW_VERSION;
  setText(settingsInfo_, info);
}

void Ui::updateSetup() {
  const Theme& t = theme();
  bool qr = false, spinner = false, button = false;
  std::string title, step, error, text, btn, qrData;
  switch (m_.phase) {
    case Phase::Boot:
      title = "Kona Radar";
      step = "Starting";
      spinner = true;
      break;
    case Phase::Connecting:
      title = "Connecting";
      step = "Joining Wi-Fi\n" + m_.wifiSsid;
      spinner = true;
      break;
    case Phase::Offline:
      title = "No Wi-Fi";
      step = "Can't reach\n" + m_.wifiSsid;
      text = "Still trying in the background.\nMoved or new router?";
      button = true;
      btn = "Set up Wi-Fi";
      break;
    case Phase::SetupJoin:
      title = "Wi-Fi setup";
      step = "Point your phone camera\nat this code";
      error = m_.setupError;
      qr = true;
      qrData = wifiQrPayload(m_.apSsid, m_.apPassword);
      text = "or join Wi-Fi  " + m_.apSsid + "\npassword  " + m_.apPassword;
      button = m_.setupCancellable;
      btn = "Cancel";
      break;
    case Phase::SetupPortal:
      title = "Almost there";
      step = "Choose your home Wi-Fi\non the page that opened";
      error = m_.setupError;
      qr = true;
      qrData = m_.portalUrl;
      text = "No page? Scan this code\nor open " + m_.portalUrl;
      button = m_.setupCancellable;
      btn = "Cancel";
      break;
    case Phase::SetupTrying:
      title = "Connecting";
      step = "Trying\n" + m_.wifiSsid;
      spinner = true;
      text = "This takes a few seconds.";
      break;
    case Phase::SetupDone:
      title = "Connected";
      step = m_.wifiSsid;
      text = "All set. You can put\nyour phone away.";
      break;
    case Phase::Running:
      break;
  }
  setText(sTitle_, title);
  setText(sStep_, step);
  setText(sError_, error);
  setText(sText_, text);
  lv_obj_set_style_text_color(sTitle_, col(m_.phase == Phase::Offline ? t.warn : t.accent), 0);
  lv_obj_t* box = static_cast<lv_obj_t*>(lv_obj_get_user_data(sQr_));
  setVisible(box, qr);
  if (qr) setQr(sQr_, sQrData_, qrData);
  setVisible(sSpinner_, spinner);
  if (m_.phase == Phase::Offline) {
    setText(sIcon_, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(sIcon_, col(t.warn), 0);
  } else if (m_.phase == Phase::SetupDone) {
    setText(sIcon_, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(sIcon_, col(t.accent), 0);
  } else {
    setText(sIcon_, "");
  }
  setVisible(sButton_, button);
  if (button) setText(sButtonLabel_, btn);
}

void Ui::onRadarEvent(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
    self->showOverlay(Overlay::Settings);
    return;
  }
  lv_point_t p;
  lv_indev_get_point(lv_indev_get_act(), &p);
  const char* hex = self->radar_.hitTest(p.x, p.y);
  if (hex) self->openDetails(hex);
}

void Ui::onRangeButton(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  const intptr_t dir = reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target(e)));
  size_t count;
  const uint16_t* steps = radar::rangeSteps(count);
  const size_t idx = radar::nearestRangeStep(self->m_.settings.rangeNm);
  if (dir < 0 && idx > 0) self->actions_->setRange(steps[idx - 1]);
  if (dir > 0 && idx + 1 < count) self->actions_->setRange(steps[idx + 1]);
}

void Ui::onOverlayTap(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  self->closeOverlays();
}

void Ui::onBrightness(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  self->actions_->setBrightness(static_cast<uint8_t>(lv_slider_get_value(self->slider_)));
}

void Ui::onSettingsButton(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  const intptr_t id = reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target(e)));
  switch (id) {
    case kBtnRotate:
      self->actions_->rotateScreen();
      self->m_.settings.screenRotation = static_cast<uint8_t>((self->m_.settings.screenRotation + 1) & 3);
      self->updateSettingsLabels();
      break;
    case kBtnView: {
      const int16_t next = self->m_.settings.viewBearingDeg == 0 ? self->lastAlignedBearing_ : 0;
      self->actions_->setViewBearing(next);
      self->m_.settings.viewBearingDeg = next;
      self->updateSettingsLabels();
      break;
    }
    case kBtnPhone:
      self->showOverlay(Overlay::PhoneQr);
      break;
    case kBtnWifi:
      self->showOverlay(Overlay::ConfirmWifi);
      break;
    case kBtnConfirmYes:
      self->closeOverlays();
      self->actions_->startWifiSetup();
      break;
    case kBtnConfirmNo:
      self->showOverlay(Overlay::Settings);
      break;
    case kBtnClose:
    default:
      self->closeOverlays();
      break;
  }
}

void Ui::onSetupButton(lv_event_t* e) {
  auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
  if (self->m_.phase == Phase::Offline) {
    self->actions_->startWifiSetup();
  } else {
    self->actions_->cancelWifiSetup();
  }
}

}  // namespace ui
