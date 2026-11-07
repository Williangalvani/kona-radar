#include "brightness.h"

#include <cmath>

namespace radar {

uint8_t panelLevel(uint8_t percent) {
  if (percent == 0) return 0;
  if (percent >= 100) return 255;
  const double curve = std::pow(percent / 100.0, 0.75);
  return static_cast<uint8_t>(std::lround(kPanelLevelFloor + (255 - kPanelLevelFloor) * curve));
}

}  // namespace radar
