#include "lv_mem_hooks.h"

#include <esp_heap_caps.h>

namespace {
constexpr uint32_t kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
}

extern "C" void* radar_lv_malloc(size_t size) {
  void* p = heap_caps_malloc(size, kPsram);
  return p ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

extern "C" void radar_lv_free(void* p) { heap_caps_free(p); }

extern "C" void* radar_lv_realloc(void* p, size_t size) {
  void* q = heap_caps_realloc(p, size, kPsram);
  return q || size == 0 ? q : heap_caps_realloc(p, size, MALLOC_CAP_8BIT);
}
