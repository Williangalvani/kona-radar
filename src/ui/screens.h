// The screens of docs/design/DESIGN.md. Each one builds its LVGL objects at
// the design's pixel positions, refreshes them from the Model and handles the
// taps / swipes the Ui forwards to it (in design coordinates).
#pragma once

#include <lvgl.h>

#include <string>

#include "radar_view.h"
#include "ui_model.h"
#include "widgets.h"

namespace ui {

enum class Swipe : uint8_t { Left, Right, Up, Down };

// Navigation requests from screens to the Ui.
class Nav {
 public:
  virtual ~Nav() = default;
  virtual void openDetail(const std::string& hex) = 0;
  virtual void openAlignment() = 0;
  virtual void openPhoneLink() = 0;
  virtual void back() = 0;
};

class Screen {
 public:
  virtual ~Screen() = default;
  virtual void build(lv_obj_t* parent) = 0;
  virtual void refresh(const Model& m) = 0;
  virtual void tap(int, int) {}
  virtual void press(int, int, bool) {}
  virtual void swipe(Swipe) {}
  void show(bool on);
  bool visible() const { return visible_; }
  void bind(Actions* a, Nav* n) {
    actions_ = a;
    nav_ = n;
  }

 protected:
  lv_obj_t* root_ = nullptr;
  Actions* actions_ = nullptr;
  Nav* nav_ = nullptr;
  bool visible_ = true;
};

// 01: power-on ident with a progress bar.
class SplashScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;

 private:
  Sketch art_;
  Sketch bar_;
  Text kona_, radar_, tagline_, status_;
  float progress_ = 0;
  uint32_t startMs_ = 0;
};

// 02: QR code for the setup network.
class SetupQrScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  // Also used for the "edit on phone" link to the settings page.
  void showLink(const std::string& url);

 private:
  void setQr(const std::string& data);
  Text title_, heading_, step1_, step2_, ssid_, pass_, hint_;
  lv_obj_t* qrBox_ = nullptr;
  lv_obj_t* qr_ = nullptr;
  std::string qrData_;
  bool linkMode_ = false;
  std::string linkUrl_;

  friend class Ui;
};

// 03: joining Wi-Fi, then checking the internet connection.
class ConnectingScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;

 private:
  Sketch art_;
  Text title_, heading_, ssid_, body_, steps_[3];
  int activeStep_ = 1;
  float spin_ = 0;
  uint32_t lastSpinMs_ = 0;
};

// 04 / 05: connected, failed, or saved network unreachable.
class ResultScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;
  void press(int x, int y, bool down) override;

 private:
  enum class Kind : uint8_t { Connected, Failed, Offline };
  void setKind(Kind k);
  Kind kind_ = Kind::Connected;
  Sketch art_;
  Text title_, heading_, line1_, line2_, hint_, footer_;
  Button button_;
};

// 06 / 07 / 10: the radar.
class RadarScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;
  void swipe(Swipe s) override;
  const std::string& focus() const { return focus_; }
  void select(const std::string& hex) { userFocus_ = hex; }

 private:
  void refreshDock(const Model& m, const radar::Target* t);
  void cycleRange(int dir);

  RadarView view_;
  lv_obj_t* headerBox_ = nullptr;
  Text title_, mode_, outsideCount_, outsideLabel_, footer_;
  Tag count_;
  lv_obj_t* dock_ = nullptr;
  Text dockKind_, dockMain_, dockTopRight_, dockBottomRight_, dockHint_;
  std::string userFocus_;
  std::string focus_;
  const Model* m_ = nullptr;
};

// 08: one aircraft.
class DetailScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;
  void swipe(Swipe s) override;
  void setTarget(const std::string& hex) { hex_ = hex; }
  const std::string& target() const { return hex_; }

 private:
  Sketch art_;
  Text title_, callsign_, sub_, labels_[2], values_[2], units_[2], vrate_, track_, look_, footer_;
  Tag tag_;
  lv_obj_t* lookBox_ = nullptr;
  std::string hex_;
};

// 09: full-screen "look up" alert for an imminent arrival.
class AlertScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;

 private:
  Sketch art_;
  Text kind_, look_, sub_, eta_, who_, data_, footer_;
  float rel_ = 0;
  std::string hex_;
};

// 11: dim idle.
class IdleScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;

 private:
  void layout(int dx, int dy);
  lv_obj_t* dot_ = nullptr;
  lv_obj_t* rule_ = nullptr;
  Text title_, count_, label_, next_, wake_, footer_;
  int dx_ = 0, dy_ = 0;
  bool hasNext_ = false;
};

// 12: observer orientation (view-up vs north-up).
class AlignmentScreen : public Screen {
 public:
  void build(lv_obj_t* parent) override;
  void refresh(const Model& m) override;
  void tap(int x, int y) override;
  void swipe(Swipe s) override;

 private:
  Sketch art_;
  Text title_, heading_, bearing_, mode_, koa_, distance_, coords_, north_, view_, footer_;
  lv_obj_t* segment_ = nullptr;
  bool northUp_ = false;
  float runwayRel_ = -96;
  bool known_ = true;
};

// Shared helpers for screens.
std::string rangeText(uint16_t nm);
const radar::Target* findTarget(const Model& m, const std::string& hex);

}  // namespace ui
