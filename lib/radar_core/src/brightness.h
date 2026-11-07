// Brightness percent (settings, 0-100) to the AMOLED brightness register
// (0x51, 0-255). The panel's light output is close to linear in the
// register, so a straight mapping leaves the bottom third of the range
// near-invisible; this one starts at a visible floor and rises faster at
// the low end. 0 stays fully dark (screen off).
#pragma once

#include <cstdint>

namespace radar {

constexpr uint8_t kPanelLevelFloor = 20;

uint8_t panelLevel(uint8_t percent);

}  // namespace radar
