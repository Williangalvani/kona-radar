#include "lvgl_port.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "board_pins.h"
#include "rotation.h"

namespace hal {
namespace {

// Rows per partial render buffer. Two buffers plus one rotation buffer of
// this size live in internal DMA-capable RAM (~30 KB each).
constexpr int kBufRows = 32;
constexpr size_t kBufPixels = LCD_WIDTH * kBufRows;

AmoledPanel* g_panel = nullptr;
TouchFt3168* g_touch = nullptr;
lv_disp_drv_t g_dispDrv;
lv_disp_draw_buf_t g_drawBuf;
lv_indev_drv_t g_indevDrv;
uint16_t* g_rotBuf = nullptr;
volatile uint8_t g_rotation = 0;
TouchStartHook g_touchHook = nullptr;
uint32_t g_lastTouchMs = 0;
bool g_wasPressed = false;
bool g_swallowing = false;
uint32_t g_statFlushes = 0, g_statPixels = 0;

void onPanelDone(void*) { lv_disp_flush_ready(&g_dispDrv); }

void flushCb(lv_disp_drv_t*, const lv_area_t* a, lv_color_t* px) {
  const int lw = a->x2 - a->x1 + 1;
  const int lh = a->y2 - a->y1 + 1;
  ++g_statFlushes;
  g_statPixels += static_cast<uint32_t>(lw * lh);
  const uint8_t rot = g_rotation;
  if (rot == 0) {
    g_panel->drawBitmap(a->x1, a->y1, a->x2, a->y2, px);
    return;
  }
  int x1, y1, x2, y2;
  radar::rotateArea(rot, LCD_WIDTH, LCD_HEIGHT, a->x1, a->y1, a->x2, a->y2, x1, y1, x2, y2);
  radar::rotateBlock16(rot, reinterpret_cast<const uint16_t*>(px), g_rotBuf, lw, lh);
  g_panel->drawBitmap(x1, y1, x2, y2, g_rotBuf);
}

// The controllers update in 2x2 pixel units: start on even, end on odd.
void rounderCb(lv_disp_drv_t*, lv_area_t* a) {
  a->x1 &= ~1;
  a->y1 &= ~1;
  a->x2 |= 1;
  a->y2 |= 1;
}

void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
  uint16_t tx, ty;
  const bool pressed = g_touch->read(tx, ty);
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
  radar::unrotatePoint(g_rotation, LCD_WIDTH, LCD_HEIGHT, tx, ty, x, y);
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
  if (!panel.begin(kBufPixels * sizeof(lv_color_t), onPanelDone, nullptr)) return false;

  lv_init();
  auto* buf1 = static_cast<lv_color_t*>(dmaAlloc(kBufPixels * sizeof(lv_color_t)));
  auto* buf2 = static_cast<lv_color_t*>(dmaAlloc(kBufPixels * sizeof(lv_color_t)));
  g_rotBuf = static_cast<uint16_t*>(dmaAlloc(kBufPixels * sizeof(uint16_t)));
  if (!buf1 || !buf2 || !g_rotBuf) {
    log_e("out of DMA memory for LVGL buffers");
    return false;
  }
  lv_disp_draw_buf_init(&g_drawBuf, buf1, buf2, kBufPixels);

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
  return true;
}

void lvglPortSetRotation(uint8_t quarterTurns) {
  g_rotation = quarterTurns & 3;
  lv_obj_invalidate(lv_scr_act());
}

void lvglPortSetTouchStartHook(TouchStartHook hook) { g_touchHook = hook; }

uint32_t lvglPortLastTouchMs() { return g_lastTouchMs; }

void lvglPortTakeStats(uint32_t& flushes, uint32_t& pixels) {
  flushes = g_statFlushes;
  pixels = g_statPixels;
  g_statFlushes = g_statPixels = 0;
}

}  // namespace hal
