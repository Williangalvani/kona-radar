// Host-side simulator: runs the real UI (src/ui) against LVGL on a PC with a
// virtual clock and the example traffic from docs/design, writes PNG
// screenshots of every screen to sim_out/ and checks the touch interactions
// through the same gesture code the device uses. Build and run with:
//   pio run -e simulator && .pio/build/simulator/program
//
// The display path matches the device: 466x32 partial buffers, the same
// rounder, RGB565 with byte swap.
#include <lvgl.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "adsb_parser.h"
#include "app_clock.h"
#include "config.h"
#include "geo.h"
#include "tracker.h"
#include "traffic.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace {

constexpr int W = 466;
constexpr int H = 466;
constexpr int kBufRows = 32;

uint32_t g_now = 100000;
uint16_t g_fb[W * H];
lv_color_t g_buf1[W * kBufRows], g_buf2[W * kBufRows];
lv_disp_draw_buf_t g_drawBuf;
lv_disp_drv_t g_dispDrv;
lv_indev_drv_t g_indevDrv;
bool g_pressed = false;
lv_point_t g_touch{0, 0};
uint64_t g_flushedPixels = 0;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
  const int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; ++y) {
    for (int x = a->x1; x <= a->x2; ++x) {
      g_fb[y * W + x] = px[(y - a->y1) * w + (x - a->x1)].full;
    }
  }
  g_flushedPixels += static_cast<uint64_t>(w) * (a->y2 - a->y1 + 1);
  lv_disp_flush_ready(drv);
}

void rounderCb(lv_disp_drv_t*, lv_area_t* a) {
  a->x1 &= ~1;
  a->y1 &= ~1;
  a->x2 |= 1;
  a->y2 |= 1;
}

void touchCb(lv_indev_drv_t*, lv_indev_data_t* d) {
  d->point = g_touch;
  d->state = g_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ---- minimal PNG writer (stored deflate blocks) ----
uint32_t crcTable[256];
void initCrc() {
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; ++i) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c;
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x);
}
void chunk(std::ofstream& f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> c;
  put32(c, static_cast<uint32_t>(data.size()));
  c.insert(c.end(), type, type + 4);
  c.insert(c.end(), data.begin(), data.end());
  const uint32_t k = crc(c.data() + 4, c.size() - 4) ^ 0xFFFFFFFFu;
  put32(c, k);
  f.write(reinterpret_cast<const char*>(c.data()), static_cast<std::streamsize>(c.size()));
}

void writePng(const std::string& path) {
  std::vector<uint8_t> raw;
  raw.reserve((W * 3 + 1) * H);
  for (int y = 0; y < H; ++y) {
    raw.push_back(0);
    for (int x = 0; x < W; ++x) {
      uint16_t v = g_fb[y * W + x];
#if LV_COLOR_16_SWAP
      v = static_cast<uint16_t>((v >> 8) | (v << 8));
#endif
      uint8_t r = static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31);
      uint8_t g = static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63);
      uint8_t b = static_cast<uint8_t>((v & 31) * 255 / 31);
      // Show the parts the round panel cannot display as dark gray.
      const float dx = x + 0.5f - W / 2.0f, dy = y + 0.5f - H / 2.0f;
      if (dx * dx + dy * dy > (W / 2.0f) * (W / 2.0f)) r = g = b = 40;
      raw.push_back(r);
      raw.push_back(g);
      raw.push_back(b);
    }
  }
  std::vector<uint8_t> z{0x78, 0x01};
  uint32_t a = 1, bsum = 0;
  for (uint8_t c : raw) {
    a = (a + c) % 65521;
    bsum = (bsum + a) % 65521;
  }
  size_t pos = 0;
  while (pos < raw.size()) {
    const size_t n = std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(n & 0xFF);
    z.push_back(n >> 8);
    z.push_back(~n & 0xFF);
    z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + n));
    pos += n;
  }
  put32(z, (bsum << 16) | a);
  std::ofstream f(path, std::ios::binary);
  const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  f.write(reinterpret_cast<const char*>(sig), 8);
  std::vector<uint8_t> ihdr;
  put32(ihdr, W);
  put32(ihdr, H);
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
  chunk(f, "IHDR", ihdr);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  std::printf("  wrote %s\n", path.c_str());
}

// ---- example traffic (docs/design/manifest.json) ----
radar::Aircraft makeAc(const char* hex, const char* cs, const char* reg, const char* type, const char* desc,
                       const char* squawk, double lat, double lon, int alt, float gs, float trk, int vr) {
  radar::Aircraft a;
  std::snprintf(a.hex, sizeof(a.hex), "%s", hex);
  std::snprintf(a.callsign, sizeof(a.callsign), "%s", cs);
  std::snprintf(a.reg, sizeof(a.reg), "%s", reg);
  std::snprintf(a.type, sizeof(a.type), "%s", type);
  std::snprintf(a.desc, sizeof(a.desc), "%s", desc);
  std::snprintf(a.squawk, sizeof(a.squawk), "%s", squawk);
  a.lat = lat;
  a.lon = lon;
  a.altFt = alt;
  a.hasAlt = true;
  a.onGround = alt <= 0;
  a.gsKt = gs;
  a.hasGs = gs > 0;
  a.trackDeg = trk;
  a.hasTrack = true;
  a.vertRateFpm = vr;
  a.hasVertRate = true;
  return a;
}

std::vector<radar::Aircraft> designTraffic() {
  return {
      makeAc("a1d208", "HAL208", "N475HA", "B712", "BOEING 717-200", "3142", 19.840, -156.036, 1900, 132, 185, -600),
      makeAc("a8a250", "SWA250", "N8810L", "B38M", "BOEING 737 MAX 8", "4271", 19.786, -156.043, 2300, 155, 5, 1400),
      makeAc("a5a875", "ASA875", "N563AS", "B738", "BOEING 737-800", "2143", 19.6748, -156.1098, 4400, 210, 34, -700),
      makeAc("a1d123", "HAL123", "N481HA", "B712", "BOEING 717-200", "3254", 19.6714, -156.0302, 3600, 178, 344, -500),
      makeAc("a6c507", "MHO507", "N507MH", "C208", "CESSNA 208 CARAVAN", "1200", 19.7886, -155.9698, 2800, 110, 189, 0),
      makeAc("a0b123", "N123BH", "N123BH", "EC30", "", "1200", 19.7151, -156.0191, 900, 85, 319, 0),
  };
}

// Reports 10 s apart so the tracks have a minute of history, as on the device.
void seed(radar::Tracker& tracker, std::vector<radar::Aircraft> base, int reports = 7) {
  tracker.clear();
  for (int k = reports - 1; k >= 0; --k) {
    std::vector<radar::Aircraft> rep = base;
    for (radar::Aircraft& a : rep) {
      a.seenPosS = 0;
      if (!a.hasGs || !a.hasTrack || a.onGround) continue;
      const radar::LatLon p = radar::destination({a.lat, a.lon}, a.trackDeg + 180.0, a.gsKt * 10.0 * k / 3600.0);
      a.lat = p.lat;
      a.lon = p.lon;
    }
    tracker.update(rep, g_now - 10000u * k);
  }
}

std::vector<radar::Aircraft> fixtureTraffic() {
  std::ifstream f("test/fixtures/adsbfi_v3_koa_150nm.json");
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string json = ss.str();
  std::vector<radar::Aircraft> list;
  const radar::ParseResult r = radar::parseAdsbJson(json.data(), json.size(), list);
  std::printf("fixture: ok=%d listed=%zu accepted=%zu\n", r.ok, r.listed, r.accepted);
  return list;
}

// ---- app stand-in ----
struct SimApp : ui::Actions {
  ui::Model model;
  radar::Tracker tracker;
  std::vector<radar::Target> targets;
  int wifiResets = 0, retries = 0, opened = 0;
  std::string dismissed;
  void setRange(uint16_t nm) override { model.settings.rangeNm = nm; }
  void setNorthUp(bool on) override { model.settings.northUp = on; }
  void toggleNight() override { model.night = !model.night; }
  void goIdle() override { model.idle = true; }
  void wake() override { model.idle = false; }
  void openRadar() override {
    ++opened;
    model.phase = ui::Phase::Running;
  }
  void retrySetup() override {
    ++retries;
    model.phase = ui::Phase::SetupJoin;
  }
  void resetWifi() override { ++wifiResets; }
  void dismissAlert(const char* hex) override {
    dismissed = hex;
    model.alertHex.clear();
  }
  void analyze() {
    radar::analyzeTraffic(tracker, model.settings, g_now, targets, model.summary);
    model.targets = &targets;
    model.tracker = &tracker;
  }
};

SimApp g_app;
ui::Ui g_ui;

void run(uint32_t ms) {
  const uint32_t end = g_now + ms;
  while (g_now < end) {
    g_app.model.nowMs = g_now;
    if (g_now % 200 == 0) g_app.analyze();
    g_ui.update(g_app.model);
    lv_timer_handler();
    g_now += 20;
  }
}

void press(lv_coord_t x, lv_coord_t y, uint32_t ms) {
  g_touch = {x, y};
  g_pressed = true;
  run(ms);
}

void release() {
  g_pressed = false;
  run(120);
}

void tap(lv_coord_t x, lv_coord_t y) {
  press(x, y, 80);
  release();
  run(300);
}

void swipe(lv_coord_t x0, lv_coord_t y0, lv_coord_t x1, lv_coord_t y1) {
  g_touch = {x0, y0};
  g_pressed = true;
  run(60);
  for (int i = 1; i <= 5; ++i) {
    g_touch = {static_cast<lv_coord_t>(x0 + (x1 - x0) * i / 5), static_cast<lv_coord_t>(y0 + (y1 - y0) * i / 5)};
    run(40);
  }
  release();
  run(300);
}

// Where the radar draws an aircraft (same projection as the view).
lv_point_t blipAt(const char* hex) {
  const radar::Track* t = g_app.tracker.find(hex);
  radar::ScopeProjection p;
  p.center = g_app.model.settings.observer();
  p.rangeNm = g_app.model.settings.rangeNm;
  p.upBearingDeg = g_app.model.settings.effectiveUpBearing();
  p.radiusPx = ui::RadarView::kRadius;
  p.cx = ui::RadarView::kCx;
  p.cy = ui::RadarView::kCy;
  float x = 0, y = 0;
  if (t) p.project(radar::Tracker::predict(*t, g_now), x, y);
  return {static_cast<lv_coord_t>(std::lround(x)), static_cast<lv_coord_t>(std::lround(y))};
}

int failures = 0;
void expect(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? " ok " : "FAIL", what);
  if (!cond) ++failures;
}

void shot(const char* name) { writePng(std::string("sim_out/") + name + ".png"); }

}  // namespace

extern "C" uint32_t app_millis(void) { return g_now; }

int main() {
  initCrc();
  if (std::system("mkdir -p sim_out") != 0) std::printf("cannot create sim_out/\n");
  lv_init();
  lv_disp_draw_buf_init(&g_drawBuf, g_buf1, g_buf2, W * kBufRows);
  lv_disp_drv_init(&g_dispDrv);
  g_dispDrv.hor_res = W;
  g_dispDrv.ver_res = H;
  g_dispDrv.flush_cb = flushCb;
  g_dispDrv.rounder_cb = rounderCb;
  g_dispDrv.draw_buf = &g_drawBuf;
  lv_disp_drv_register(&g_dispDrv);
  lv_indev_drv_init(&g_indevDrv);
  g_indevDrv.type = LV_INDEV_TYPE_POINTER;
  g_indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&g_indevDrv);

  ui::Model& m = g_app.model;
  m.settings = radar::Settings::defaults();
  // The placeholder observer from docs/design (not anyone's real balcony).
  m.settings.observerLat = 19.7470;
  m.settings.observerLon = -155.9940;
  m.settings.viewBearingDeg = 260;
  m.apSsid = "Kona-Radar-7A2C";
  m.apPassword = "look-up-7412";
  m.portalIp = "192.168.4.1";
  m.wifiSsid = "Balcony Wi-Fi";
  m.lanIp = "192.168.1.42";
  m.lanHost = std::string(RADAR_HOSTNAME) + ".local";
  m.feedSource = "adsb.fi";
  m.exampleData = true;

  if (!g_ui.begin(&g_app)) {
    std::printf("ui begin failed\n");
    return 1;
  }

  std::printf("setup screens\n");
  m.phase = ui::Phase::Boot;
  run(1500);
  shot("01_splash");
  m.phase = ui::Phase::SetupJoin;
  run(300);
  shot("02_wifi_setup_qr");
  m.phase = ui::Phase::SetupPortal;
  run(300);
  shot("02b_wifi_setup_phone_joined");
  m.phase = ui::Phase::SetupJoining;
  run(600);
  shot("03_wifi_connecting");
  m.phase = ui::Phase::SetupOnline;
  run(600);
  shot("03b_wifi_checking_internet");
  m.phase = ui::Phase::SetupDone;
  run(300);
  shot("04_wifi_connected");
  tap(233, 396);
  expect(g_app.opened == 1 && m.phase == ui::Phase::Running, "OPEN RADAR opens the radar");
  m.phase = ui::Phase::SetupFailed;
  m.setupFailure = ui::SetupFailure::WrongPassword;
  run(300);
  shot("05_wifi_failed");
  tap(233, 369);
  expect(g_app.retries == 1 && m.phase == ui::Phase::SetupJoin, "TRY AGAIN re-opens phone setup");
  m.phase = ui::Phase::SetupFailed;
  m.setupFailure = ui::SetupFailure::NoInternet;
  run(300);
  shot("05b_wifi_no_internet");
  m.phase = ui::Phase::Offline;
  run(300);
  shot("05c_wifi_offline");

  std::printf("radar\n");
  seed(g_app.tracker, designTraffic());
  m.phase = ui::Phase::Running;
  m.online = true;
  m.feedEverOk = m.feedOk = true;
  run(1500);
  shot("06_radar_view_aligned");
  expect(g_ui.focusedAircraft() == "a1d208", "next arrival (HAL208) is focused by default");
  expect(m.summary.nextArrival >= 0 && g_app.targets[m.summary.nextArrival].cue.runway &&
             std::strcmp(g_app.targets[m.summary.nextArrival].cue.runway, "17") == 0,
         "HAL208 is a likely arrival on runway 17");
  g_flushedPixels = 0;
  run(2000);
  std::printf("  steady-state flushed pixels/s: %.0f (%.2f full frames)\n", g_flushedPixels / 2.0,
              g_flushedPixels / 2.0 / (W * H));

  tap(290, 80);
  expect(m.settings.rangeNm == 20, "tapping the range text goes 10 -> 20 NM");
  tap(290, 80);
  expect(m.settings.rangeNm == 5, "and wraps 20 -> 5 NM");
  run(400);
  {
    const lv_point_t p = blipAt("a8a250");
    tap(p.x, p.y);
    expect(g_ui.focusedAircraft() == "a8a250", "tapping SWA250 selects it");
  }
  run(1200);
  shot("07_radar_zoom_5nm");
  {
    const lv_point_t p = blipAt("a8a250");
    tap(p.x, p.y);
  }
  run(300);
  shot("08b_detail_departure");
  tap(108, 79);
  m.settings.rangeNm = 5;
  run(300);
  swipe(150, 250, 330, 250);
  expect(m.settings.rangeNm == 5, "swipe right at 5 NM stays at 5 NM");
  swipe(330, 250, 150, 250);
  expect(m.settings.rangeNm == 10, "swipe left zooms out to 10 NM");
  tap(233, 252);
  expect(g_ui.focusedAircraft() == "a1d208", "tapping empty sky returns focus to the next arrival");
  tap(233, 373);
  run(300);
  shot("08_aircraft_detail");
  swipe(150, 250, 330, 250);
  run(300);

  std::printf("alert\n");
  m.alertHex = "a1d208";
  run(600);
  shot("09_look_up_alert");
  tap(233, 233);
  run(300);
  expect(g_app.dismissed == "a1d208" && m.alertHex.empty(), "tap dismisses the alert");
  shot("09b_alert_opens_detail");
  tap(108, 79);

  std::printf("night\n");
  swipe(233, 300, 233, 150);
  run(800);
  expect(m.night, "swipe up switches to night");
  shot("10_night_mode");
  swipe(233, 300, 233, 150);
  run(300);
  expect(!m.night, "and back to day");

  std::printf("idle\n");
  swipe(233, 150, 233, 300);
  run(500);
  expect(m.idle, "swipe down dims to idle");
  shot("11_idle_dim");
  tap(233, 233);
  expect(!m.idle, "tap wakes");

  std::printf("alignment\n");
  tap(190, 60);
  run(300);
  shot("12_balcony_alignment");
  tap(150, 387);
  expect(m.settings.northUp, "NORTH UP segment");
  run(300);
  shot("12b_alignment_north_up");
  tap(300, 387);
  expect(!m.settings.northUp, "VIEW UP segment");
  tap(233, 432);
  run(300);
  shot("13_phone_settings_link");
  tap(233, 233);
  run(300);

  std::printf("hold to reset Wi-Fi\n");
  press(200, 200, 2500);
  shot("14_hold_to_reset_wifi");
  release();
  expect(g_app.wifiResets == 0, "releasing early cancels the reset");
  press(200, 200, 5200);
  release();
  expect(g_app.wifiResets == 1, "holding 5 s resets Wi-Fi");
  expect(m.settings.rangeNm == 10 && m.settings.viewBearingDeg == 260, "other settings untouched by the hold");

  std::printf("north-up, unset observer, live fixture traffic\n");
  {
    radar::Settings s = radar::Settings::defaults();
    s.rangeNm = 20;
    m.settings = s;
    std::vector<radar::Aircraft> live = fixtureTraffic();
    std::vector<radar::Aircraft> d = designTraffic();
    live.insert(live.end(), d.begin(), d.end());
    seed(g_app.tracker, live);
    m.exampleData = false;
    run(1500);
    shot("15_radar_20nm_observer_not_set");
    tap(190, 60);
    run(300);
    shot("15b_alignment_location_not_set");
    tap(108, 79);
    run(300);
    m.feedAgeS = 45;
    run(400);
    shot("16_radar_stale_feed");
    m.feedAgeS = 0;
    g_ui.setPixelShift(RADAR_PIXEL_SHIFT_MAX, -RADAR_PIXEL_SHIFT_MAX);
    run(600);
    shot("17_radar_pixel_shifted");
    g_ui.setPixelShift(0, 0);
  }

  std::printf("%s (%d failures)\n", failures ? "FAILED" : "done", failures);
  return failures ? 1 : 0;
}
