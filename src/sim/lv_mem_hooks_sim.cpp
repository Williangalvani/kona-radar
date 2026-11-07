// LVGL's heap in the simulator: the C allocator with a size header, so the
// simulator can check that screens and navigation do not leak.
#include <cstdlib>
#include <cstring>

#include "lv_mem_hooks.h"

namespace {
constexpr size_t kHeader = 16;  // keeps the payload 16-byte aligned
size_t g_liveBytes = 0;
size_t g_liveBlocks = 0;
}  // namespace

size_t simLvLiveBytes() { return g_liveBytes; }
size_t simLvLiveBlocks() { return g_liveBlocks; }

extern "C" void* radar_lv_malloc(size_t size) {
  auto* p = static_cast<unsigned char*>(std::malloc(size + kHeader));
  if (!p) return nullptr;
  std::memcpy(p, &size, sizeof(size));
  g_liveBytes += size;
  ++g_liveBlocks;
  return p + kHeader;
}

extern "C" void radar_lv_free(void* q) {
  if (!q) return;
  auto* p = static_cast<unsigned char*>(q) - kHeader;
  size_t size;
  std::memcpy(&size, p, sizeof(size));
  g_liveBytes -= size;
  --g_liveBlocks;
  std::free(p);
}

extern "C" void* radar_lv_realloc(void* q, size_t size) {
  if (!q) return radar_lv_malloc(size);
  if (size == 0) {
    radar_lv_free(q);
    return nullptr;
  }
  auto* p = static_cast<unsigned char*>(q) - kHeader;
  size_t old;
  std::memcpy(&old, p, sizeof(old));
  auto* n = static_cast<unsigned char*>(std::realloc(p, size + kHeader));
  if (!n) return nullptr;
  std::memcpy(n, &size, sizeof(size));
  g_liveBytes = g_liveBytes - old + size;
  return n + kHeader;
}
