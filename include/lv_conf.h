// LVGL 8.4 configuration. Anything not set here takes LVGL's default
// (see lvgl/src/lv_conf_internal.h).
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// RGB565, byte-swapped: the panel takes big-endian pixels over QSPI.
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_COLOR_SCREEN_TRANSP 0

// LVGL's heap (objects, styles, label text, draw scratch). On the device it
// goes to PSRAM; plain malloc would keep every block under 4 KB in internal
// RAM, which Wi-Fi, TLS and DMA need (see lv_mem_hooks.h).
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "lv_mem_hooks.h"
#define LV_MEM_CUSTOM_ALLOC radar_lv_malloc
#define LV_MEM_CUSTOM_FREE radar_lv_free
#define LV_MEM_CUSTOM_REALLOC radar_lv_realloc

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 20
#define LV_DPI_DEF 200
// The radar invalidates many small blip areas per frame; past this count
// LVGL falls back to redrawing the whole screen.
#define LV_INV_BUF_SIZE 64

// Time base shared with the app (millis() on the device, a virtual clock in
// the simulator).
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "app_clock.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (app_millis())

#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 8
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_LAYER_SIMPLE_BUF_SIZE (24 * 1024)

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_TXT_ENC LV_TXT_ENC_UTF8

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0

#define LV_USE_QRCODE 1

#define LV_BUILD_EXAMPLES 0

#endif  // LV_CONF_H
