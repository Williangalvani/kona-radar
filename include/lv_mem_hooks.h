// Allocator behind LVGL's heap (LV_MEM_CUSTOM in lv_conf.h).
// Device (src/hal/lv_mem_hooks.cpp): PSRAM first, internal RAM as fallback.
// Simulator (src/sim): the C allocator, counting live bytes for leak checks.
#ifndef LV_MEM_HOOKS_H
#define LV_MEM_HOOKS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* radar_lv_malloc(size_t size);
void radar_lv_free(void* p);
void* radar_lv_realloc(void* p, size_t size);

#ifdef __cplusplus
}
#endif

#endif
