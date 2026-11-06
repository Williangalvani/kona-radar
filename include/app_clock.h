// Millisecond time base used by LVGL and the UI. Implemented in main.cpp on
// the device (millis()) and by the simulator (virtual clock).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t app_millis(void);

#ifdef __cplusplus
}
#endif
