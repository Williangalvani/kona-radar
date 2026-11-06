// Kona Radar: app controller. Wires the hardware drivers, Wi-Fi, the ADS-B
// downloader and the web pages to the UI (src/ui) through ui::Model and
// ui::Actions, and owns the display policy: night colors, look-up alerts,
// the quiet idle screen, screen-off and pixel shift.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>

#include <algorithm>
#include <string>
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
#include "traffic.h"
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

std::vector<radar::Target> g_targets;
radar::TrafficSummary g_summary;
uint32_t g_lastAnalyzeMs = 0;

// Display policy state
uint32_t g_lastActivityMs = 0;
bool g_forcedIdle = false;
bool g_idle = false;
bool g_blackout = false;
bool g_night = false;
int8_t g_nightOverride = -1;  // manual day (0) / night (1) until the schedule changes
bool g_overrideScheduled = false;
std::string g_alertHex;
uint32_t g_alertSinceMs = 0;
std::vector<std::string> g_alerted;  // most recent last
float g_shownBrightness = 0;
int g_appliedBrightness = -1;
uint32_t g_lastBrightnessMs = 0;
uint32_t g_lastShiftMs = 0;
uint8_t g_shiftIdx = 0;

uint32_t g_bootDownMs = 0;
bool g_bootHandled = false;
bool g_ntpStarted = false;
bool g_radarOpened = false;
uint32_t g_lastExpireMs = 0;
uint32_t g_lastStatsMs = 0;

constexpr uint32_t kSplashMs = 2500;
constexpr uint32_t kAlertMaxMs = 120000;

void configureFeed() {
  // Wide enough for the "+N outside" count and for arrivals that are still
  // well out on final.
  const double radius = std::max<double>(25.0, g_settings.rangeNm * 2.0);
  g_adsb.configure(g_settings.observer(), static_cast<uint16_t>(std::min(250.0, std::ceil(radius))), g_settings.refreshS,
                   g_settings.source);
}

void applySettings(const radar::Settings& s, uint32_t saveDelayMs, bool persist = true) {
  const uint8_t oldRotation = g_settings.screenRotation;
  g_settings = s;
  g_settings.sanitize();
  if (g_settings.screenRotation != oldRotation) hal::lvglPortSetRotation(g_settings.screenRotation);
  if (persist) g_store.save(g_settings, saveDelayMs);
  configureFeed();
  g_lastActivityMs = millis();
}

bool localTime(struct tm& t) {
  if (!g_ntpStarted) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  localtime_r(&now, &t);
  return true;
}

bool scheduledNight() {
  if (g_settings.nightMode == 1) return false;
  if (g_settings.nightMode == 2) return true;
  struct tm t;
  if (!localTime(t)) return false;
  const int h = t.tm_hour, a = g_settings.nightStartH, b = g_settings.nightEndH;
  return a == b ? false : (a < b ? (h >= a && h < b) : (h >= a || h < b));
}

void wakeUp(uint32_t now) {
  g_lastActivityMs = now;
  g_forcedIdle = false;
  g_idle = g_blackout = false;
}

class AppActions : public ui::Actions {
 public:
  void setRange(uint16_t nm) override {
    radar::Settings s = g_settings;
    s.rangeNm = nm;
    applySettings(s, 5000);
  }
  void setNorthUp(bool northUp) override {
    radar::Settings s = g_settings;
    s.northUp = northUp ? 1 : 0;
    applySettings(s, 3000);
  }
  void toggleNight() override {
    g_overrideScheduled = scheduledNight();
    g_nightOverride = g_night ? 0 : 1;
  }
  void goIdle() override { g_forcedIdle = true; }
  void wake() override { wakeUp(millis()); }
  void openRadar() override { g_radarOpened = true; }
  void retrySetup() override {
    if (g_wifi.state() == net::WifiState::Offline) {
      g_wifi.startPortal();
    } else {
      g_wifi.retry();
    }
  }
  void resetWifi() override {
    log_i("touch hold: resetting Wi-Fi");
    g_wifi.resetAndStartPortal();
  }
  void dismissAlert(const char* hex) override {
    if (g_alertHex == hex) g_alertHex.clear();
  }
};
AppActions g_actions;

// While the panel is off the first touch only turns it back on.
bool onTouchStart() {
  const bool wasOff = g_blackout;
  wakeUp(millis());
  return wasOff;
}

ui::Phase phaseFor(net::WifiState s, uint32_t now) {
  switch (s) {
    case net::WifiState::Off:
      return ui::Phase::Boot;
    case net::WifiState::Connecting:
      return now < kSplashMs ? ui::Phase::Boot : ui::Phase::Connecting;
    case net::WifiState::Offline:
      return ui::Phase::Offline;
    case net::WifiState::Portal:
      return g_wifi.apClientCount() > 0 ? ui::Phase::SetupPortal : ui::Phase::SetupJoin;
    case net::WifiState::PortalTrying:
      return ui::Phase::SetupJoining;
    case net::WifiState::PortalOnline:
      return ui::Phase::SetupOnline;
    case net::WifiState::PortalDone:
      return g_radarOpened ? ui::Phase::Running : ui::Phase::SetupDone;
    case net::WifiState::PortalFailed:
      return ui::Phase::SetupFailed;
    case net::WifiState::Connected:
    default:
      return now < kSplashMs ? ui::Phase::Boot : ui::Phase::Running;
  }
}

ui::SetupFailure failureFor(net::SetupError e) {
  switch (e) {
    case net::SetupError::WrongPassword:
      return ui::SetupFailure::WrongPassword;
    case net::SetupError::NotFound:
      return ui::SetupFailure::NotFound;
    case net::SetupError::Refused:
      return ui::SetupFailure::Refused;
    case net::SetupError::NoInternet:
      return ui::SetupFailure::NoInternet;
    default:
      return ui::SetupFailure::None;
  }
}

// Setup is only "done" once flight data actually downloads.
void checkSetupInternet(uint32_t now) {
  const net::WifiState ws = g_wifi.state();
  const bool checking = ws == net::WifiState::PortalOnline;
  const bool failedNoNet = ws == net::WifiState::PortalFailed && g_wifi.setupError() == net::SetupError::NoInternet;
  if (!checking && !failedNoNet) return;
  const net::FeedStatus st = g_adsb.status();
  if (st.everOk && st.lastOk && static_cast<int32_t>(st.lastOkMs - g_wifi.stateSinceMs()) >= 0) {
    g_radarOpened = false;
    g_wifi.confirmInternet();
  } else if (checking && now - g_wifi.stateSinceMs() > RADAR_ONLINE_CHECK_S * 1000UL) {
    g_wifi.failInternet();
  }
}

bool aircraftNearObserver() {
  if (g_settings.wakeRangeNm == 0) return false;
  for (const radar::Target& t : g_targets) {
    if (!t.stale && !t.track->ac.onGround && t.distNm <= g_settings.wakeRangeNm) return true;
  }
  return false;
}

bool imminentArrival() {
  return g_summary.nextArrival >= 0 && g_summary.nextArrivalEtaS <= RADAR_IMMINENT_ETA_S;
}

void updateAlert(uint32_t now) {
  if (!g_alertHex.empty()) {
    const radar::Target* t = nullptr;
    for (const radar::Target& x : g_targets)
      if (g_alertHex == x.track->ac.hex) t = &x;
    if (!t || t->cue.phase != radar::FlightPhase::Arrival || now - g_alertSinceMs > kAlertMaxMs) g_alertHex.clear();
    return;
  }
  if (!g_settings.alerts || g_summary.nextArrival < 0) return;
  const radar::Target& t = g_targets[g_summary.nextArrival];
  if (!t.inRange || t.stale || t.cue.etaS < 0 || t.cue.etaS > RADAR_ALERT_ETA_S) return;
  const std::string hex = t.track->ac.hex;
  if (std::find(g_alerted.begin(), g_alerted.end(), hex) != g_alerted.end()) return;
  g_alerted.push_back(hex);
  if (g_alerted.size() > 16) g_alerted.erase(g_alerted.begin());
  g_alertHex = hex;
  g_alertSinceMs = now;
  wakeUp(now);
  log_i("alert: %s landing in ~%.0f s", t.track->ac.callsign, t.cue.etaS);
}

void updateDisplayPolicy(uint32_t now, ui::Phase phase) {
  const uint32_t touch = hal::lvglPortLastTouchMs();
  if (static_cast<int32_t>(touch - g_lastActivityMs) > 0) g_lastActivityMs = touch;
  if (phase != ui::Phase::Running) wakeUp(now);

  const bool sched = scheduledNight();
  if (g_nightOverride >= 0 && sched != g_overrideScheduled) g_nightOverride = -1;
  g_night = g_nightOverride >= 0 ? g_nightOverride == 1 : sched;

  if (phase == ui::Phase::Running) {
    updateAlert(now);
    const bool keepAwake = !g_alertHex.empty() || (!g_forcedIdle && (imminentArrival() || aircraftNearObserver()));
    if (keepAwake && !g_forcedIdle) g_lastActivityMs = now;
    const uint32_t quietMs = now - g_lastActivityMs;
    const bool autoIdle = g_settings.idleDimS > 0 && quietMs > g_settings.idleDimS * 1000UL;
    g_idle = g_alertHex.empty() && (g_forcedIdle || autoIdle);
    const uint32_t idleForMs = g_forcedIdle ? quietMs : quietMs - std::min<uint32_t>(quietMs, g_settings.idleDimS * 1000UL);
    g_blackout = g_idle && !keepAwake && g_settings.blackoutS > 0 && idleForMs > g_settings.blackoutS * 1000UL;
  }

  int target = g_settings.brightness;
  if (g_night) target = std::min<int>(target, g_settings.nightBrightness);
  if (g_idle) target = std::min<int>(target, g_settings.idleBrightness);
  if (g_blackout) target = 0;

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
    if (!g_bootHandled && now - g_bootDownMs > RADAR_RESET_HOLD_S * 1000UL) {
      g_bootHandled = true;
      log_i("BOOT held: resetting Wi-Fi");
      g_wifi.resetAndStartPortal();
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
  m.phase = phaseFor(g_wifi.state(), now);
  const net::WifiState ws = g_wifi.state();
  const bool trying = ws == net::WifiState::PortalTrying || ws == net::WifiState::PortalFailed;
  m.wifiSsid = (trying && g_wifi.tryingSsid().length() ? g_wifi.tryingSsid() : g_wifi.savedSsid()).c_str();
  m.apSsid = g_wifi.apSsid().c_str();
  m.apPassword = g_wifi.apPassword().c_str();
  m.portalIp = net::WifiManager::kPortalIp;
  m.setupFailure = failureFor(g_wifi.setupError());
  m.online = g_wifi.online();
  m.lanIp = g_wifi.localIp().c_str();
  m.lanHost = std::string(RADAR_HOSTNAME) + ".local";

  const net::FeedStatus st = g_adsb.status();
  m.feedEverOk = st.everOk;
  m.feedAgeS = st.everOk ? (now - st.lastOkMs) / 1000 : 0;
  m.feedOk = st.lastOk && m.feedAgeS < g_settings.refreshS * 3u + 15u;
  m.feedSource = net::AdsbClient::sourceName(st.source);

  m.night = g_night;
  m.idle = g_idle;
  m.blackout = g_blackout;
  m.alertHex = g_alertHex;
  m.settings = g_settings;
  m.tracker = &g_tracker;
  m.targets = &g_targets;
  m.summary = g_summary;
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
    applySettings(radar::Settings::defaults(), 0, false);
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
  if (now - g_lastAnalyzeMs >= 250) {
    g_lastAnalyzeMs = now;
    radar::analyzeTraffic(g_tracker, g_settings, now, g_targets, g_summary);
  }
  checkSetupInternet(now);

  updateDisplayPolicy(now, phaseFor(g_wifi.state(), now));
  fillModel(now);
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
