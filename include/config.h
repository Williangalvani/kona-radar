// Build-time configuration for Kona Radar.
//
// Values marked (runtime) are factory defaults that can be changed from a
// phone on the device's settings page (http://kona-radar.local/ or the IP
// shown on the device under Settings) without reflashing. Only values that
// were changed are stored in NVS, so untouched settings follow these
// defaults after a firmware update. "Restore factory settings" on that page
// clears the stored changes.
//
// Every value can be overridden without editing this file by adding
// -D<NAME>=<value> to build_flags in platformio.ini.
#pragma once

// ---------------------------------------------------------------------------
// Scope center (runtime) - Kona International Airport, KOA / PHKO
// ---------------------------------------------------------------------------
#ifndef RADAR_CENTER_LAT
#define RADAR_CENTER_LAT 19.7388
#endif
#ifndef RADAR_CENTER_LON
#define RADAR_CENTER_LON (-156.0456)
#endif
#ifndef RADAR_CENTER_NAME
#define RADAR_CENTER_NAME "KOA"
#endif

// ---------------------------------------------------------------------------
// Observer (runtime) - where the display is mounted (the balcony)
// ---------------------------------------------------------------------------
// Used for the "look here" hint (compass bearing + elevation angle from the
// observer), the observer marker on the scope and the auto view bearing.
// Default: the balcony, a few km from KOA (KOA bears ~281 deg from it).
#ifndef RADAR_OBSERVER_LAT
#define RADAR_OBSERVER_LAT 19.700000
#endif
#ifndef RADAR_OBSERVER_LON
#define RADAR_OBSERVER_LON (-156.000000)
#endif
// Eye height above mean sea level, feet (only affects elevation angles).
#ifndef RADAR_OBSERVER_ALT_FT
#define RADAR_OBSERVER_ALT_FT 100
#endif

// ---------------------------------------------------------------------------
// View orientation (runtime)
// ---------------------------------------------------------------------------
// Compass bearing (degrees true) drawn at the top of the screen.
//   281 = default: facing from the balcony toward KOA, so the screen lines
//         up with the view when you look at the airport.
//    -1 = auto: computed as the bearing from the observer to the center.
//     0 = classic north-up. Any other 0..359 value is used as-is.
#ifndef RADAR_VIEW_BEARING_DEG
#define RADAR_VIEW_BEARING_DEG 281
#endif
// Physical mounting of the board, clockwise quarter turns 0..3 (runtime).
#ifndef RADAR_SCREEN_ROTATION
#define RADAR_SCREEN_ROTATION 0
#endif

// ---------------------------------------------------------------------------
// Range
// ---------------------------------------------------------------------------
// Outer-ring radius in nautical miles on first boot (runtime; the -/+
// buttons change it and the last choice is remembered).
#ifndef RADAR_DEFAULT_RANGE_NM
#define RADAR_DEFAULT_RANGE_NM 25
#endif
// Ranges the -/+ buttons step through (nautical miles, ascending, <= 250).
#ifndef RADAR_RANGE_STEPS_NM
#define RADAR_RANGE_STEPS_NM {5, 10, 25, 50, 100, 200}
#endif

// ---------------------------------------------------------------------------
// Data source
// ---------------------------------------------------------------------------
// Seconds between downloads (runtime). The public APIs allow 1 request/s;
// the firmware never goes below 2 s.
#ifndef RADAR_REFRESH_INTERVAL_S
#define RADAR_REFRESH_INTERVAL_S 5
#endif
// Preferred feed (runtime): 0 = adsb.fi, 1 = adsb.lol. After repeated errors
// the firmware fails over to the other one automatically.
#ifndef RADAR_PRIMARY_SOURCE
#define RADAR_PRIMARY_SOURCE 0
#endif
// Aircraft not reported for this long are removed.
#ifndef RADAR_STALE_AFTER_S
#define RADAR_STALE_AFTER_S 60
#endif

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
// Seconds per sweep revolution (cosmetic).
#ifndef RADAR_SWEEP_PERIOD_S
#define RADAR_SWEEP_PERIOD_S 6
#endif
// Sweep animation frame rate. Each sweep frame repaints the beam's afterglow
// sector (~45k pixels), so this is the main knob for display bandwidth.
#ifndef RADAR_SWEEP_FPS
#define RADAR_SWEEP_FPS 25
#endif
// Normal brightness in percent (runtime).
#ifndef RADAR_DEFAULT_BRIGHTNESS
#define RADAR_DEFAULT_BRIGHTNESS 80
#endif
// POSIX TZ string for the clock. Hawaii has no daylight saving time.
#ifndef RADAR_TIMEZONE
#define RADAR_TIMEZONE "HST10"
#endif
// Panel controller: 0 = auto-detect (read ID register 0xDA), 1 = SH8601,
// 2 = CO5300. Waveshare ships this board with either controller.
#ifndef RADAR_PANEL_FORCE
#define RADAR_PANEL_FORCE 0
#endif
// QSPI pixel clock. 40 MHz is what Waveshare's own demos use.
#ifndef RADAR_PANEL_PCLK_HZ
#define RADAR_PANEL_PCLK_HZ (40 * 1000 * 1000)
#endif
// Print display throughput and memory statistics to the serial log every 5 s.
#ifndef RADAR_LOG_FPS
#define RADAR_LOG_FPS 0
#endif

// ---------------------------------------------------------------------------
// AMOLED burn-in protection
// ---------------------------------------------------------------------------
// Dim to the idle brightness after this many seconds without touch
// (runtime, 0 disables). A touch restores full brightness.
#ifndef RADAR_IDLE_DIM_S
#define RADAR_IDLE_DIM_S 300
#endif
#ifndef RADAR_IDLE_BRIGHTNESS
#define RADAR_IDLE_BRIGHTNESS 20
#endif
// Wake from idle dim while an aircraft is within this many NM of the
// observer (runtime, 0 disables): the "glance, then look up" trigger.
#ifndef RADAR_WAKE_RANGE_NM
#define RADAR_WAKE_RANGE_NM 6
#endif
// Night window in local hours [start, end) with a lower brightness cap
// (runtime; equal start and end disables it).
#ifndef RADAR_NIGHT_START_H
#define RADAR_NIGHT_START_H 22
#endif
#ifndef RADAR_NIGHT_END_H
#define RADAR_NIGHT_END_H 6
#endif
#ifndef RADAR_NIGHT_BRIGHTNESS
#define RADAR_NIGHT_BRIGHTNESS 8
#endif
// Move the whole picture by up to RADAR_PIXEL_SHIFT_MAX pixels every
// RADAR_PIXEL_SHIFT_S seconds so static lines never sit on the same pixels.
#ifndef RADAR_PIXEL_SHIFT_S
#define RADAR_PIXEL_SHIFT_S 120
#endif
#ifndef RADAR_PIXEL_SHIFT_MAX
#define RADAR_PIXEL_SHIFT_MAX 3
#endif

// ---------------------------------------------------------------------------
// Wi-Fi setup
// ---------------------------------------------------------------------------
// Name prefix of the temporary setup network; 4 hex digits of the MAC
// address are appended (e.g. "KonaRadar-1A2B").
#ifndef RADAR_AP_PREFIX
#define RADAR_AP_PREFIX "KonaRadar"
#endif
// mDNS host name on the home network: http://<name>.local/
#ifndef RADAR_HOSTNAME
#define RADAR_HOSTNAME "kona-radar"
#endif
// Seconds to wait for the saved network at boot before offering setup.
#ifndef RADAR_WIFI_CONNECT_TIMEOUT_S
#define RADAR_WIFI_CONNECT_TIMEOUT_S 25
#endif
// Seconds to hold the BOOT button to open Wi-Fi setup while running.
#ifndef RADAR_BOOT_HOLD_S
#define RADAR_BOOT_HOLD_S 5
#endif

#define RADAR_FW_VERSION "1.0.0"
