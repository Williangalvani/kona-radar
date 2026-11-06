// Screen manager: owns every screen, shows the one that matches the Model,
// rebuilds them when the day/night palette changes, and turns raw touch
// into taps, swipes and the 5-second "reset Wi-Fi" hold.
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
  void back() override;
  void selectAircraft(const std::string& hex) { radar_.select(hex); }
  const std::string& focusedAircraft() const { return radar_.focus(); }

 private:
  enum class View : uint8_t { Radar, Detail, Alignment, PhoneLink };

  void build();
  Screen* route(const Model& m);
  void pollTouch(uint32_t now);
  void showHold(float progress);
  void hideHold();

  Actions* actions_ = nullptr;
  Model m_;
  bool haveModel_ = false;
  bool night_ = false;
  View view_ = View::Radar;
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
};

// "WIFI:T:WPA;S:<ssid>;P:<pass>;;" with the escaping phone cameras expect.
std::string wifiQrPayload(const std::string& ssid, const std::string& pass);

}  // namespace ui
