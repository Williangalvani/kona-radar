// Kona Radar: app controller. Wires the hardware drivers, Wi-Fi, the ADS-B
// downloader and the web pages to the UI (src/ui) through ui::Model and
// ui::Actions, and owns the brightness / burn-in policy.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>

#include <vector>

#include "app/settings_store.h"
#include "config.h"
#include "hal/amoled_panel.h"
#include "hal/board_pins.h"
#include "hal/lvgl_port.h"
#include "hal/touch_ft3168.h"
#include "net/adsb_client.h"
#include "net/web_ui.h"
#include "net/wifi_manager.h"
#include "tracker.h"
#include "ui/ui.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

extern "C" uint32_t app_millis(void) { return millis(); }

namespace {

hal::AmoledPanel g_panel;
hal::TouchFt3168 g_touch;
app::SettingsStore g_store;
net::WifiManager g_wifi;
net::AdsbClient g_adsb;
net::WebUi g_web;
radar::Tracker g_tracker;
radar::Settings g_settings;
ui::Ui g_ui;
ui::Model g_model;
std::vector<radar::Aircraft> g_reports;

// Brightness / burn-in state
uint32_t g_lastActivityMs = 0;
bool g_dimmed = false;
float g_shownBrightness = 0;
int g_appliedBrightness = -1;
uint32_t g_lastBrightnessMs = 0;
uint32_t g_lastShiftMs = 0;
uint8_t g_shiftIdx = 0;

uint32_t g_bootDownMs = 0;
bool g_bootHandled = false;
bool g_ntpStarted = false;
uint32_t g_lastExpireMs = 0;
uint32_t g_lastStatsMs = 0;

void configureFeed() {
  const uint16_t radius = static_cast<uint16_t>(std::min(250.0, std::ceil(g_settings.rangeNm * 1.3)));
  g_adsb.configure(g_settings.center(), radius, g_settings.refreshS, g_settings.source);
}

void applySettings(const radar::Settings& s, uint32_t saveDelayMs) {
  const uint8_t oldRotation = g_settings.screenRotation;
  g_settings = s;
  g_settings.sanitize();
  if (g_settings.screenRotation != oldRotation) hal::lvglPortSetRotation(g_settings.screenRotation);
  g_store.save(g_settings, saveDelayMs);
  configureFeed();
  g_lastActivityMs = millis();
}

class AppActions : public ui::Actions {
 public:
  void setRange(uint16_t nm) override {
    radar::Settings s = g_settings;
    s.rangeNm = nm;
    applySettings(s, 5000);
  }
  void setBrightness(uint8_t percent) override {
    radar::Settings s = g_settings;
    s.brightness = percent;
    applySettings(s, 3000);
  }
  void rotateScreen() override {
    radar::Settings s = g_settings;
    s.screenRotation = static_cast<uint8_t>((s.screenRotation + 1) & 3);
    applySettings(s, 3000);
  }
  void setViewBearing(int16_t deg) override {
    radar::Settings s = g_settings;
    s.viewBearingDeg = deg;
    applySettings(s, 3000);
  }
  void startWifiSetup() override { g_wifi.startPortal(); }
  void cancelWifiSetup() override { g_wifi.cancelPortal(); }
};
AppActions g_actions;

// First touch on a dimmed screen only wakes it.
bool onTouchStart() {
  const bool wasDimmed = g_dimmed;
  g_lastActivityMs = millis();
  if (wasDimmed) {
    g_dimmed = false;
    log_i("wake by touch");
  }
  return wasDimmed;
}

ui::Phase phaseFor(net::WifiState s) {
  switch (s) {
    case net::WifiState::Off:
      return ui::Phase::Boot;
    case net::WifiState::Connecting:
      return ui::Phase::Connecting;
    case net::WifiState::Offline:
      return ui::Phase::Offline;
    case net::WifiState::Portal:
      return g_wifi.apClientCount() > 0 ? ui::Phase::SetupPortal : ui::Phase::SetupJoin;
    case net::WifiState::PortalTrying:
      return ui::Phase::SetupTrying;
    case net::WifiState::PortalDone:
      return ui::Phase::SetupDone;
    case net::WifiState::Connected:
    default:
      return ui::Phase::Running;
  }
}

bool localTime(struct tm& t) {
  if (!g_ntpStarted) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  localtime_r(&now, &t);
  return true;
}

bool aircraftNearObserver(uint32_t now) {
  if (g_settings.wakeRangeNm == 0) return false;
  for (const radar::Track& t : g_tracker.tracks()) {
    if (t.ac.onGround) continue;
    const radar::LatLon p = radar::Tracker::predict(t, now);
    if (radar::distanceNm(g_settings.observer(), p) <= g_settings.wakeRangeNm) return true;
  }
  return false;
}

void updateBrightness(uint32_t now, ui::Phase phase) {
  if (phase != ui::Phase::Running) g_lastActivityMs = now;
  if (aircraftNearObserver(now)) g_lastActivityMs = now;
  const uint32_t touch = hal::lvglPortLastTouchMs();
  if (static_cast<int32_t>(touch - g_lastActivityMs) > 0) g_lastActivityMs = touch;

  g_dimmed = g_settings.idleDimS > 0 && now - g_lastActivityMs > g_settings.idleDimS * 1000UL;
  int target = g_settings.brightness;
  struct tm t;
  if (localTime(t)) {
    const int h = t.tm_hour, a = g_settings.nightStartH, b = g_settings.nightEndH;
    const bool night = a == b ? false : (a < b ? (h >= a && h < b) : (h >= a || h < b));
    if (night) target = std::min<int>(target, g_settings.nightBrightness);
  }
  if (g_dimmed) target = std::min<int>(target, g_settings.idleBrightness);

  // Fade at ~100 %/s so changes are gentle.
  const float dt = (now - g_lastBrightnessMs) / 1000.0f;
  g_lastBrightnessMs = now;
  const float step = 100.0f * std::min(dt, 0.2f);
  if (g_shownBrightness < target) g_shownBrightness = std::min<float>(target, g_shownBrightness + step);
  if (g_shownBrightness > target) g_shownBrightness = std::max<float>(target, g_shownBrightness - step);
  const int v = static_cast<int>(g_shownBrightness + 0.5f);
  if (v != g_appliedBrightness) {
    g_appliedBrightness = v;
    g_panel.setBrightness(static_cast<uint8_t>(v));
  }
}

void updatePixelShift(uint32_t now) {
  if (RADAR_PIXEL_SHIFT_S <= 0 || RADAR_PIXEL_SHIFT_MAX <= 0) return;
  if (now - g_lastShiftMs < RADAR_PIXEL_SHIFT_S * 1000UL) return;
  g_lastShiftMs = now;
  static const int8_t kPattern[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1},
                                       {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
  g_shiftIdx = static_cast<uint8_t>((g_shiftIdx + 1) % 9);
  g_ui.setPixelShift(kPattern[g_shiftIdx][0] * RADAR_PIXEL_SHIFT_MAX,
                     kPattern[g_shiftIdx][1] * RADAR_PIXEL_SHIFT_MAX);
}

void pollBootButton(uint32_t now) {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    if (g_bootDownMs == 0) g_bootDownMs = now;
    if (!g_bootHandled && now - g_bootDownMs > RADAR_BOOT_HOLD_S * 1000UL) {
      g_bootHandled = true;
      log_i("BOOT held: starting Wi-Fi setup");
      g_wifi.startPortal();
    }
  } else {
    g_bootDownMs = 0;
    g_bootHandled = false;
  }
}

String statusJson() {
  const net::FeedStatus st = g_adsb.status();
  String s = "{\"version\":\"" RADAR_FW_VERSION "\",\"aircraft\":";
  s += g_tracker.tracks().size();
  s += ",\"feedOk\":";
  s += st.lastOk ? "true" : "false";
  s += ",\"source\":\"";
  s += net::AdsbClient::sourceName(st.source);
  s += "\",\"feedError\":\"";
  for (const char* p = st.error; *p; ++p) {
    if (*p != '"' && *p != '\\') s += *p;
  }
  s += "\",\"ssid\":\"";
  for (size_t i = 0; i < g_wifi.savedSsid().length(); ++i) {
    const char c = g_wifi.savedSsid()[i];
    if (c != '"' && c != '\\') s += c;
  }
  s += "\",\"rssi\":";
  s += WiFi.RSSI();
  s += ",\"panel\":\"";
  s += g_panel.name();
  s += "\",\"uptimeS\":";
  s += millis() / 1000;
  s += ",\"freeHeap\":";
  s += ESP.getFreeHeap();
  s += ",\"freePsram\":";
  s += ESP.getFreePsram();
  s += "}";
  return s;
}

void fillModel(uint32_t now) {
  ui::Model& m = g_model;
  m.nowMs = now;
  m.phase = phaseFor(g_wifi.state());
  const bool trying = m.phase == ui::Phase::SetupTrying || m.phase == ui::Phase::SetupDone;
  m.wifiSsid = (trying ? g_wifi.tryingSsid() : g_wifi.savedSsid()).c_str();
  m.apSsid = g_wifi.apSsid().c_str();
  m.apPassword = g_wifi.apPassword().c_str();
  m.portalUrl = std::string("http://") + net::WifiManager::kPortalIp + "/";
  m.setupError = g_wifi.lastError().c_str();
  m.setupCancellable = g_wifi.hasSavedNetwork();
  m.online = g_wifi.online();
  m.lanIp = g_wifi.localIp().c_str();
  m.lanHost = std::string(RADAR_HOSTNAME) + ".local";

  const net::FeedStatus st = g_adsb.status();
  m.feedEverOk = st.everOk;
  m.feedAgeS = st.everOk ? (now - st.lastOkMs) / 1000 : 0;
  m.feedOk = st.lastOk && m.feedAgeS < g_settings.refreshS * 3u + 15u;
  m.feedSource = net::AdsbClient::sourceName(st.source);
  m.feedError = st.error;

  struct tm t;
  if (localTime(t)) {
    char buf[12];
    const int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
    m.clock = buf;
  } else {
    m.clock.clear();
  }
  m.dimmed = g_dimmed;
  m.settings = g_settings;
  m.tracker = &g_tracker;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  log_i("Kona Radar " RADAR_FW_VERSION);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  g_settings = g_store.load();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  if (!g_touch.begin(Wire)) log_w("touch controller not found");

  if (!hal::lvglPortBegin(g_panel, g_touch)) {
    log_e("display init failed");
  }
  log_i("panel %s (id 0x%02X)", g_panel.name(), g_panel.rawId());
  hal::lvglPortSetRotation(g_settings.screenRotation);
  hal::lvglPortSetTouchStartHook(onTouchStart);

  if (!g_ui.begin(&g_actions)) log_e("UI init failed (out of PSRAM?)");
  fillModel(millis());
  g_ui.update(g_model);
  lv_timer_handler();

  g_wifi.begin(&g_store);
  net::WebUi::Hooks hooks;
  hooks.getSettings = [] { return g_settings; };
  hooks.applySettings = [](const radar::Settings& s) { applySettings(s, 0); };
  hooks.resetSettings = [] {
    g_store.resetToDefaults();
    applySettings(radar::Settings::defaults(), 0);
  };
  hooks.statusJson = statusJson;
  g_web.begin(&g_wifi, hooks);

  configureFeed();
  g_adsb.begin();
  g_lastActivityMs = g_lastBrightnessMs = g_lastShiftMs = millis();
  log_i("heap %u, psram %u", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
  const uint32_t now = millis();
  g_wifi.loop();
  g_web.loop();
  g_store.loop();
  pollBootButton(now);

  if (!g_ntpStarted && g_wifi.online()) {
    configTzTime(RADAR_TIMEZONE, "pool.ntp.org", "time.google.com");
    g_ntpStarted = true;
  }

  uint32_t receivedMs;
  if (g_adsb.takeLatest(g_reports, receivedMs)) g_tracker.update(g_reports, receivedMs);
  if (now - g_lastExpireMs > 1000) {
    g_lastExpireMs = now;
    g_tracker.expire(now, RADAR_STALE_AFTER_S * 1000UL);
  }

  fillModel(now);
  updateBrightness(now, g_model.phase);
  g_model.dimmed = g_dimmed;
  updatePixelShift(now);
  g_ui.update(g_model);
  lv_timer_handler();

  if (RADAR_LOG_FPS && now - g_lastStatsMs > 5000) {
    uint32_t flushes, pixels;
    hal::lvglPortTakeStats(flushes, pixels);
    log_i("flush %.1f/s, %.2f Mpx/s, heap %u, psram %u, aircraft %u", flushes / 5.0f,
          pixels / 5e6f, ESP.getFreeHeap(), ESP.getFreePsram(),
          static_cast<unsigned>(g_tracker.tracks().size()));
    g_lastStatsMs = now;
  }
  delay(1);
}
