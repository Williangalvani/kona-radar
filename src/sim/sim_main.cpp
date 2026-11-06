// Host-side simulator: runs the real UI (src/ui) against LVGL on a PC with a
// virtual clock and canned aircraft, and writes PNG screenshots of every
// screen to sim_out/. Build and run with:  pio run -e simulator -t exec
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

// ---- canned traffic ----
radar::Aircraft makeAc(const char* hex, const char* cs, const char* reg, const char* type,
                       const char* desc, const char* owner, double brgFromKoa, double distNm,
                       int alt, float gs, float trk, int vr) {
  radar::Aircraft a;
  std::snprintf(a.hex, sizeof(a.hex), "%s", hex);
  std::snprintf(a.callsign, sizeof(a.callsign), "%s", cs);
  std::snprintf(a.reg, sizeof(a.reg), "%s", reg);
  std::snprintf(a.type, sizeof(a.type), "%s", type);
  std::snprintf(a.desc, sizeof(a.desc), "%s", desc);
  std::snprintf(a.owner, sizeof(a.owner), "%s", owner);
  std::snprintf(a.squawk, sizeof(a.squawk), "%s", "1200");
  const radar::LatLon p = radar::destination({RADAR_CENTER_LAT, RADAR_CENTER_LON}, brgFromKoa, distNm);
  a.lat = p.lat;
  a.lon = p.lon;
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

std::vector<radar::Aircraft> cannedTraffic() {
  return {
      makeAc("a8c1f2", "HAL112", "N480HA", "A21N", "AIRBUS A-321neo", "HAWAIIAN AIRLINES INC", 335, 11, 4500, 205, 152, -900),
      makeAc("a1b2c3", "ASA843", "N527AS", "B739", "BOEING 737-900", "ALASKA AIRLINES INC", 62, 17, 11800, 290, 244, -1400),
      makeAc("ab7f10", "N8017Y", "N8017Y", "C172", "CESSNA 172 SKYHAWK", "PARADISE FLYING CLUB", 118, 4.2, 2500, 96, 301, 0),
      makeAc("a2c4e6", "UAL1172", "N37298", "B38M", "BOEING 737 MAX 8", "UNITED AIRLINES INC", 285, 21, 23800, 410, 62, 2100),
      makeAc("a9e3d1", "SWA2341", "N8710M", "B38M", "BOEING 737 MAX 8", "SOUTHWEST AIRLINES CO", 196, 8.5, 6900, 240, 12, -700),
      makeAc("a3f0aa", "BLH12", "N212BH", "AS50", "", "BLUE HAWAIIAN HELICOPTERS", 28, 2.6, 1100, 102, 95, 0),
      makeAc("a40b6e", "HAL41", "N492HA", "A332", "AIRBUS A-330-200", "HAWAIIAN AIRLINES INC", 0, 0.4, 0, 0, 170, 0),
      makeAc("a77c01", "N44KR", "N44KR", "PC12", "PILATUS PC-12", "KAMAKA AIR", 160, 14, 9500, 250, 330, 500),
  };
}

void seedTracker(radar::Tracker& tracker) {
  std::vector<radar::Aircraft> base = cannedTraffic();
  std::ifstream f("test/fixtures/adsbfi_v3_koa_150nm.json");
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string json = ss.str();
  std::vector<radar::Aircraft> fixture;
  const radar::ParseResult r = radar::parseAdsbJson(json.data(), json.size(), fixture);
  std::printf("fixture: ok=%d listed=%zu accepted=%zu\n", r.ok, r.listed, r.accepted);
  for (radar::Aircraft& a : fixture) {
    a.seenPosS = 0;
    base.push_back(a);
  }
  // Three reports 10 s apart so the tracks have history trails.
  for (int k = 2; k >= 0; --k) {
    std::vector<radar::Aircraft> rep = base;
    for (radar::Aircraft& a : rep) {
      if (!a.hasGs || !a.hasTrack || a.onGround) continue;
      const radar::LatLon p = radar::destination({a.lat, a.lon}, a.trackDeg + 180.0, a.gsKt * 10.0 * k / 3600.0);
      a.lat = p.lat;
      a.lon = p.lon;
    }
    tracker.update(rep, g_now - 10000u * k);
  }
}

// ---- app stand-in ----
struct SimApp : ui::Actions {
  ui::Model model;
  void setRange(uint16_t nm) override { model.settings.rangeNm = nm; }
  void setBrightness(uint8_t p) override { model.settings.brightness = p; }
  void rotateScreen() override { model.settings.screenRotation = (model.settings.screenRotation + 1) & 3; }
  void setViewBearing(int16_t deg) override { model.settings.viewBearingDeg = deg; }
  void startWifiSetup() override { model.phase = ui::Phase::SetupJoin; }
  void cancelWifiSetup() override { model.phase = ui::Phase::Running; }
};

SimApp g_app;
ui::Ui g_ui;

void run(uint32_t ms) {
  const uint32_t end = g_now + ms;
  while (g_now < end) {
    g_app.model.nowMs = g_now;
    g_ui.update(g_app.model);
    lv_timer_handler();
    g_now += 20;
  }
}

void tap(lv_coord_t x, lv_coord_t y, uint32_t holdMs = 80) {
  g_touch = {x, y};
  g_pressed = true;
  run(holdMs);
  g_pressed = false;
  run(100);
}

int failures = 0;
void expect(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? " ok " : "FAIL", what);
  if (!cond) ++failures;
}

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
  g_indevDrv.long_press_time = 1200;
  lv_indev_drv_register(&g_indevDrv);

  radar::Tracker tracker;
  ui::Model& m = g_app.model;
  m.settings = radar::Settings::defaults();
  m.tracker = &tracker;
  m.apSsid = "KonaRadar-1A2B";
  m.apPassword = "kona4821";
  m.portalUrl = "http://192.168.4.1/";
  m.wifiSsid = "Ohana-Home";
  m.lanIp = "192.168.1.42";
  m.lanHost = std::string(RADAR_HOSTNAME) + ".local";
  m.clock = "14:32";
  m.feedSource = "adsb.fi";

  if (!g_ui.begin(&g_app)) {
    std::printf("ui begin failed\n");
    return 1;
  }

  std::printf("setup screens\n");
  m.phase = ui::Phase::Boot;
  run(300);
  writePng("sim_out/01_boot.png");
  m.phase = ui::Phase::SetupJoin;
  m.setupCancellable = false;
  run(300);
  writePng("sim_out/02_setup_join.png");
  m.phase = ui::Phase::SetupPortal;
  run(300);
  writePng("sim_out/03_setup_portal.png");
  m.phase = ui::Phase::SetupJoin;
  m.setupError = "Wrong password for Ohana-Home. Try again.";
  run(300);
  writePng("sim_out/04_setup_wrong_password.png");
  m.setupError.clear();
  m.phase = ui::Phase::SetupTrying;
  run(300);
  writePng("sim_out/05_setup_trying.png");
  m.phase = ui::Phase::SetupDone;
  run(300);
  writePng("sim_out/06_setup_done.png");
  m.phase = ui::Phase::Offline;
  run(300);
  writePng("sim_out/07_offline.png");

  std::printf("radar\n");
  seedTracker(tracker);
  m.phase = ui::Phase::Running;
  m.online = true;
  m.feedEverOk = m.feedOk = true;
  run(2500);
  writePng("sim_out/10_radar_25nm_view281.png");
  g_flushedPixels = 0;
  run(1000);
  std::printf("  steady-state flushed pixels/s: %llu (%.1f full frames)\n",
              static_cast<unsigned long long>(g_flushedPixels), g_flushedPixels / double(W * H));

  // Tap the HAL112 blip (find it through the view's own projection).
  {
    const radar::Track* t = tracker.find("a8c1f2");
    float x, y;
    g_ui.closeOverlays();
    // Projection is owned by the view; replicate it from settings.
    radar::ScopeProjection p;
    p.center = m.settings.center();
    p.rangeNm = m.settings.rangeNm;
    p.upBearingDeg = m.settings.effectiveUpBearing();
    p.radiusPx = static_cast<float>(ui::theme().scopeRadius);
    p.project(radar::Tracker::predict(*t, g_now), x, y);
    tap(static_cast<lv_coord_t>(x + 6), static_cast<lv_coord_t>(y - 4));
    run(1100);
    const char* sel = g_ui.selectedAircraft();
    expect(sel && std::strcmp(sel, "a8c1f2") == 0, "tap on HAL112 opens its details");
    writePng("sim_out/11_details_tap.png");
    tap(233, 420);
    run(200);
  }

  std::printf("range buttons\n");
  tap(233 + 82, 466 - 46 - 21);
  run(300);
  expect(m.settings.rangeNm == 50, "+ button goes 25 -> 50 NM");
  writePng("sim_out/12_radar_50nm.png");
  tap(233 - 82, 466 - 46 - 21);
  tap(233 - 82, 466 - 46 - 21);
  tap(233 - 82, 466 - 46 - 21);
  run(300);
  expect(m.settings.rangeNm == 5, "- button x3 goes 50 -> 5 NM");
  writePng("sim_out/13_radar_5nm.png");
  m.settings.rangeNm = 200;
  run(800);
  writePng("sim_out/14_radar_200nm.png");
  m.settings.rangeNm = 25;
  m.settings.viewBearingDeg = 0;
  run(800);
  writePng("sim_out/15_radar_25nm_north_up.png");
  m.settings.viewBearingDeg = RADAR_VIEW_BEARING_DEG;
  g_ui.setPixelShift(RADAR_PIXEL_SHIFT_MAX, -RADAR_PIXEL_SHIFT_MAX);
  run(800);
  writePng("sim_out/16_radar_pixel_shifted.png");
  g_ui.setPixelShift(0, 0);
  run(300);

  std::printf("long press -> settings\n");
  tap(233, 300, 1500);
  run(300);
  writePng("sim_out/20_settings.png");
  tap(233, 142 + 21);  // view toggle
  run(200);
  expect(m.settings.viewBearingDeg == 0, "view toggle -> north up");
  tap(233, 142 + 21);
  run(200);
  expect(m.settings.viewBearingDeg == RADAR_VIEW_BEARING_DEG, "view toggle -> back to 281");
  tap(233, 242 + 21);  // phone settings
  run(300);
  writePng("sim_out/21_phone_settings_qr.png");
  tap(233, 233);
  run(100);
  g_ui.openSettings();
  run(100);
  tap(233, 292 + 21);  // change Wi-Fi
  run(300);
  writePng("sim_out/22_confirm_wifi.png");
  tap(233 + 78, 290 + 23);  // cancel
  run(200);
  expect(m.phase == ui::Phase::Running, "cancel keeps radar running");
  tap(233, 350 + 21);  // close
  run(200);

  std::printf("details for every visible aircraft type\n");
  g_ui.openDetails("a2c4e6");
  run(1200);
  writePng("sim_out/23_details_ual.png");
  g_ui.closeOverlays();

  m.feedOk = false;
  m.feedAgeS = 140;
  run(400);
  writePng("sim_out/24_radar_stale_feed.png");

  std::printf("%s (%d failures)\n", failures ? "FAILED" : "done", failures);
  return failures ? 1 : 0;
}
