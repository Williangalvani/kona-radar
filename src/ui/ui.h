// Screen manager: owns every LVGL object, shows the screen that matches
// Model::phase, and turns touches into Actions. Restyle via theme.h; the
// layout constants live in ui.cpp.
#pragma once

#include <lvgl.h>

#include <string>

#include "radar_view.h"
#include "ui_model.h"

namespace ui {

class Ui {
 public:
  bool begin(Actions* actions);
  // Call every loop iteration (before lv_timer_handler).
  void update(const Model& m);
  // Burn-in protection: moves everything by a few pixels.
  void setPixelShift(int dx, int dy);

  // Used by the simulator to reach screens without touch input.
  void openDetails(const char* hex);
  void openSettings();
  void closeOverlays();
  const char* selectedAircraft() const { return radar_.selected(); }

 private:
  enum class Overlay : uint8_t { None, Details, Settings, PhoneQr, ConfirmWifi };

  static void onRadarEvent(lv_event_t* e);
  static void onRangeButton(lv_event_t* e);
  static void onOverlayTap(lv_event_t* e);
  static void onSettingsButton(lv_event_t* e);
  static void onBrightness(lv_event_t* e);
  static void onSetupButton(lv_event_t* e);

  void buildRadarScreen();
  void buildDetails();
  void buildSettings();
  void buildPhoneQr();
  void buildConfirm();
  void buildSetup();
  void showOverlay(Overlay o);
  void updateHeader();
  void updateDetails();
  void updateSettingsLabels();
  void updateSetup();
  void setQr(lv_obj_t* qr, std::string& cache, const std::string& data);

  Actions* actions_ = nullptr;
  Model m_;
  bool haveModel_ = false;
  Phase shownPhase_ = Phase::Boot;
  Overlay overlay_ = Overlay::None;
  int shiftX_ = 0, shiftY_ = 0;
  int16_t lastAlignedBearing_ = 0;
  uint32_t lastDetailsMs_ = 0;
  uint32_t lastHeaderMs_ = 0;

  lv_obj_t* scr_ = nullptr;
  RadarView radar_;

  // Radar overlay
  lv_obj_t* hud_ = nullptr;
  lv_obj_t* clock_ = nullptr;
  lv_obj_t* info_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* rangeLabel_ = nullptr;
  lv_obj_t* minusBtn_ = nullptr;
  lv_obj_t* plusBtn_ = nullptr;

  // Details card
  lv_obj_t* details_ = nullptr;
  lv_obj_t* dTitle_ = nullptr;
  lv_obj_t* dOperator_ = nullptr;
  lv_obj_t* dType_ = nullptr;
  lv_obj_t* dLook_ = nullptr;
  lv_obj_t* dKeys_ = nullptr;
  lv_obj_t* dValues_ = nullptr;

  // Settings
  lv_obj_t* settings_ = nullptr;
  lv_obj_t* slider_ = nullptr;
  lv_obj_t* viewBtnLabel_ = nullptr;
  lv_obj_t* rotateBtnLabel_ = nullptr;
  lv_obj_t* settingsInfo_ = nullptr;
  lv_obj_t* phoneQrPanel_ = nullptr;
  lv_obj_t* phoneQr_ = nullptr;
  lv_obj_t* phoneQrText_ = nullptr;
  std::string phoneQrData_;
  lv_obj_t* confirm_ = nullptr;

  // Setup / status screens
  lv_obj_t* setup_ = nullptr;
  lv_obj_t* sTitle_ = nullptr;
  lv_obj_t* sStep_ = nullptr;
  lv_obj_t* sError_ = nullptr;
  lv_obj_t* sQr_ = nullptr;
  lv_obj_t* sText_ = nullptr;
  lv_obj_t* sSpinner_ = nullptr;
  lv_obj_t* sIcon_ = nullptr;
  lv_obj_t* sButton_ = nullptr;
  lv_obj_t* sButtonLabel_ = nullptr;
  std::string sQrData_;
};

// "WIFI:T:WPA;S:<ssid>;P:<pass>;;" with the escaping phone cameras expect.
std::string wifiQrPayload(const std::string& ssid, const std::string& pass);

}  // namespace ui
