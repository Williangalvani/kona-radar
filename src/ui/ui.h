// Screen manager: owns every screen, shows the one that matches the Model,
// rebuilds them when the day/night palette changes, and turns raw touch
// into taps, swipes and the 5-second "reset Wi-Fi" hold. Also shows the
// first-run gesture guide and short notices ("Night colors on").
#pragma once

#include <lvgl.h>

#include <memory>
#include <string>

#include "screens.h"
#include "ui_model.h"

namespace ui {

class Ui : public Nav {
 public:
  bool begin(Actions* actions);
  // Call every loop iteration (before lv_timer_handler).
  void update(const Model& m);
  // Burn-in protection: moves everything by a few pixels.
  void setPixelShift(int dx, int dy);

  // Nav (also used by the simulator to reach screens directly).
  void openDetail(const std::string& hex) override;
  void openAlignment() override;
  void openPhoneLink() override;
  void openMenu() override;
  void openGuide() override;
  void back() override;
  void notify(const std::string& title, const std::string& sub) override;
  void selectAircraft(const std::string& hex) { radar_.select(hex); }
  const std::string& focusedAircraft() const { return radar_.focus(); }
  bool onMenu() const { return active_ == &menu_; }
  bool onRadar() const { return active_ == &radar_; }

 private:
  enum class View : uint8_t { Radar, Detail, Alignment, PhoneLink, Menu, Guide };

  void build();
  void buildToast();
  void updateToast();
  void closeGuide();
  Screen* route(const Model& m);
  void pollTouch(uint32_t now);
  void showHold(float progress);
  void hideHold();

  Actions* actions_ = nullptr;
  Model m_;
  bool haveModel_ = false;
  bool night_ = false;
  View view_ = View::Radar;
  View from_ = View::Radar;  // where back() returns to from a sub-page
  Screen* active_ = nullptr;
  lv_obj_t* stage_ = nullptr;
  int shiftX_ = 0, shiftY_ = 0;

  SplashScreen splash_;
  SetupQrScreen setupQr_;
  SetupQrScreen link_;
  ConnectingScreen connecting_;
  ResultScreen result_;
  RadarScreen radar_;
  DetailScreen detail_;
  AlertScreen alert_;
  IdleScreen idle_;
  AlignmentScreen alignment_;
  MenuScreen menu_;
  GuideScreen guide_;
  UpdateScreen update_;
  bool guideOffered_ = false;
  bool radarSeen_ = false;

  // Touch gesture state
  bool down_ = false;
  bool holdFired_ = false;
  bool moved_ = false;
  lv_point_t start_{}, last_{};
  uint32_t downMs_ = 0;

  lv_obj_t* hold_ = nullptr;
  Sketch holdArt_;
  Text holdTitle_, holdSub_, holdHint_;
  float holdProgress_ = -1;

  lv_obj_t* toast_ = nullptr;
  Text toastTitle_, toastSub_;
  std::string toastTitleText_, toastSubText_;
  uint32_t toastUntilMs_ = 0;
  bool toastShown_ = false;
};

// "WIFI:T:WPA;S:<ssid>;P:<pass>;;" with the escaping phone cameras expect.
std::string wifiQrPayload(const std::string& ssid, const std::string& pass);

}  // namespace ui
