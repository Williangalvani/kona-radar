#include "lvgl_port.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "board_pins.h"
#include "rotation.h"

namespace hal {
namespace {

// Rows per partial render buffer. Both buffers live in internal DMA-capable
// RAM (~30 KB each). Unrotated, LVGL renders into one while the panel
// receives the other. Rotated, LVGL renders into the first and the flush
// rotates it into the second, which the panel then receives: still one
// block rendering while the previous one is sent, without a third buffer.
constexpr int kBufRows = 32;
constexpr size_t kBufPixels = LCD_WIDTH * kBufRows;
constexpr uint32_t kPanelWaitMs = 100;

AmoledPanel* g_panel = nullptr;
TouchFt3168* g_touch = nullptr;
lv_disp_drv_t g_dispDrv;
lv_disp_draw_buf_t g_drawBuf;
lv_indev_drv_t g_indevDrv;
lv_color_t* g_buf1 = nullptr;
lv_color_t* g_buf2 = nullptr;
// Changed only while nothing is being sent (lvglPortSetRotation).
volatile uint8_t g_rotation = 0;
volatile bool g_panelBusy = false;
TouchStartHook g_touchHook = nullptr;
uint32_t g_lastTouchMs = 0;
bool g_wasPressed = false;
bool g_swallowing = false;
uint32_t g_statFlushes = 0, g_statPixels = 0;
bool g_waitWarned = false;

void onPanelDone(void*) {
  g_panelBusy = false;
  // Rotated blocks were copied out of LVGL's buffer before sending, so LVGL
  // was told it is free right away.
  if (g_rotation == 0) lv_disp_flush_ready(&g_dispDrv);
}

void waitPanelIdle() {
  const uint32_t t0 = millis();
  while (g_panelBusy) {
    if (millis() - t0 > kPanelWaitMs) {
      // A lost completion must not freeze the UI.
      if (!g_waitWarned) log_w("panel transfer did not complete; continuing");
      g_waitWarned = true;
      g_panelBusy = false;
      break;
    }
  }
}

void flushCb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
  ++g_statFlushes;
  g_statPixels += static_cast<uint32_t>((a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1));
  const uint8_t rot = g_rotation;
  if (rot == 0) {
    g_panelBusy = true;
    // Without a transfer there is no completion interrupt; LVGL would wait
    // for it forever.
    if (!g_panel->drawBitmap(a->x1, a->y1, a->x2, a->y2, px)) {
      g_panelBusy = false;
      lv_disp_flush_ready(drv);
    }
    return;
  }
  // The second buffer may still be on its way to the panel.
  waitPanelIdle();
  const radar::PanelBlock b = radar::toPanel(rot, LCD_WIDTH, LCD_HEIGHT, a->x1, a->y1, a->x2, a->y2,
                                             reinterpret_cast<const uint16_t*>(px),
                                             reinterpret_cast<uint16_t*>(g_buf2));
  g_panelBusy = true;
  if (!g_panel->drawBitmap(b.x1, b.y1, b.x2, b.y2, b.pixels)) g_panelBusy = false;
  lv_disp_flush_ready(drv);
}

// The controllers update in 2x2 pixel units: start on even, end on odd.
// With the even panel size this survives rotation (see rotation.h).
void rounderCb(lv_disp_drv_t*, lv_area_t* a) {
  a->x1 &= ~1;
  a->y1 &= ~1;
  a->x2 |= 1;
  a->y2 |= 1;
}

void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t tx = 0, ty = 0;
  const bool pressed = g_touch && g_touch->read(tx, ty);
  if (pressed) {
    g_lastTouchMs = millis();
    if (!g_wasPressed) g_swallowing = g_touchHook && g_touchHook();
  } else {
    g_swallowing = false;
  }
  g_wasPressed = pressed;

  if (!pressed || g_swallowing) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  int x, y;
  radar::touchToLogical(g_rotation, LCD_WIDTH, LCD_HEIGHT, tx, ty, x, y);
  data->point.x = static_cast<lv_coord_t>(x);
  data->point.y = static_cast<lv_coord_t>(y);
  data->state = LV_INDEV_STATE_PRESSED;
}

void* dmaAlloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
}

}  // namespace

bool lvglPortBegin(AmoledPanel& panel, TouchFt3168& touch) {
  g_panel = &panel;
  g_touch = &touch;
  const bool panelOk = panel.begin(kBufPixels * sizeof(lv_color_t), onPanelDone, nullptr);

  // LVGL is set up even without a panel: the UI and the rest of the app keep
  // running (Wi-Fi, web pages, OTA), and flushes complete immediately.
  lv_init();
  g_buf1 = static_cast<lv_color_t*>(dmaAlloc(kBufPixels * sizeof(lv_color_t)));
  g_buf2 = static_cast<lv_color_t*>(dmaAlloc(kBufPixels * sizeof(lv_color_t)));
  if (!g_buf1) {
    log_e("out of DMA memory for LVGL buffers");
    g_buf1 = static_cast<lv_color_t*>(heap_caps_malloc(kBufPixels * sizeof(lv_color_t), MALLOC_CAP_8BIT));
    if (!g_buf1) return false;
  }
  if (!g_buf2) log_w("single LVGL buffer; screen rotation disabled");
  lv_disp_draw_buf_init(&g_drawBuf, g_buf1, g_buf2, kBufPixels);

  lv_disp_drv_init(&g_dispDrv);
  g_dispDrv.hor_res = LCD_WIDTH;
  g_dispDrv.ver_res = LCD_HEIGHT;
  g_dispDrv.flush_cb = flushCb;
  g_dispDrv.rounder_cb = rounderCb;
  g_dispDrv.draw_buf = &g_drawBuf;
  lv_disp_t* disp = lv_disp_drv_register(&g_dispDrv);

  lv_indev_drv_init(&g_indevDrv);
  g_indevDrv.type = LV_INDEV_TYPE_POINTER;
  g_indevDrv.read_cb = touchCb;
  g_indevDrv.disp = disp;
  g_indevDrv.long_press_time = 1200;
  g_indevDrv.scroll_limit = 12;
  lv_indev_drv_register(&g_indevDrv);
  return panelOk;
}

void lvglPortSetRotation(uint8_t quarterTurns) {
  const uint8_t rot = g_buf2 ? (quarterTurns & 3) : 0;
  if (rot == g_rotation || !g_buf1) return;
  // The buffers change roles: let the last block reach the panel first.
  const uint32_t t0 = millis();
  while ((g_drawBuf.flushing || g_panelBusy) && millis() - t0 < kPanelWaitMs) delay(1);
  g_panelBusy = false;
  g_rotation = rot;
  lv_disp_draw_buf_init(&g_drawBuf, g_buf1, rot ? nullptr : g_buf2, kBufPixels);
  if (lv_disp_get_default()) lv_obj_invalidate(lv_scr_act());
  log_i("screen rotation %d degrees", radar::rotationDegrees(rot));
}

void lvglPortSetTouchStartHook(TouchStartHook hook) { g_touchHook = hook; }

uint32_t lvglPortLastTouchMs() { return g_lastTouchMs; }

void lvglPortTakeStats(uint32_t& flushes, uint32_t& pixels) {
  flushes = g_statFlushes;
  pixels = g_statPixels;
  g_statFlushes = g_statPixels = 0;
}

}  // namespace hal
