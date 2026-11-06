// Binds LVGL to the AMOLED panel and FT3168 touch: partial double-buffered
// rendering with DMA flushes, software rotation, and touch input.
#pragma once

#include <stdint.h>

#include "amoled_panel.h"
#include "touch_ft3168.h"

namespace hal {

// Called when a new touch starts. Return true to swallow the whole touch
// (used for "first touch only wakes the screen").
using TouchStartHook = bool (*)();

bool lvglPortBegin(AmoledPanel& panel, TouchFt3168& touch);
void lvglPortSetRotation(uint8_t quarterTurns);
void lvglPortSetTouchStartHook(TouchStartHook hook);
uint32_t lvglPortLastTouchMs();
// Average flush statistics since the last call (for RADAR_LOG_FPS).
void lvglPortTakeStats(uint32_t& flushes, uint32_t& pixels);

}  // namespace hal
