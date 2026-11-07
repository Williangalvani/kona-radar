// Build-time configuration for Kona Radar.
//
// Values marked (runtime) are factory defaults that can be changed from a
// phone - in the setup portal or on the settings page at
// http://kona-radar.local/ - without reflashing. Only values that were
// changed are stored in NVS, so untouched settings follow these defaults after
// a firmware update. "Restore factory settings" clears the stored changes.
//
// Personal values (such as where the radar is mounted) do not belong in this
// public file. Put them in include/config.local.h, which is gitignored and
// included first; see include/config.local.example.h. Any value can also be
// overridden with -D<NAME>=<value> in platformio.ini build_flags.
#pragma once

#if defined(__has_include)
#if __has_include("config.local.h")
#include "config.local.h"
#endif
#endif

// ---------------------------------------------------------------------------
// Reference airport (runtime) - Kona International Airport, KOA / PHKO
// ---------------------------------------------------------------------------
// The airport whose arrivals and departures are tracked and whose runway is
// drawn. Runway geometry is built in for KOA (see places.cpp).
#ifndef RADAR_CENTER_LAT
#define RADAR_CENTER_LAT 19.738765
#endif
#ifndef RADAR_CENTER_LON
#define RADAR_CENTER_LON (-156.045631)
#endif
#ifndef RADAR_CENTER_NAME
#define RADAR_CENTER_NAME "KOA"
#endif

// ---------------------------------------------------------------------------
// Observer (runtime) - where the radar is mounted
// ---------------------------------------------------------------------------
// The scope is centered here; ring distances, "look" directions and elevation
// angles are measured from this point. The neutral default is the airport
// itself, which means "not set yet": the screen then centers on the airport,
// shows north up and asks for the real location from the phone.
#ifndef RADAR_OBSERVER_LAT
#define RADAR_OBSERVER_LAT RADAR_CENTER_LAT
#endif
#ifndef RADAR_OBSERVER_LON
#define RADAR_OBSERVER_LON RADAR_CENTER_LON
#endif
// Eye height above mean sea level, feet (only affects elevation angles).
#ifndef RADAR_OBSERVER_ALT_FT
#define RADAR_OBSERVER_ALT_FT 50
#endif

// ---------------------------------------------------------------------------
// Orientation (runtime)
// ---------------------------------------------------------------------------
// Facing bearing: the compass direction (degrees true) you look in from the
// observer, e.g. toward the airport. "LOOK LEFT/RIGHT" cues are relative to
// it, and in view-up mode it is drawn at the top of the screen.
//   -1 = auto: the bearing from the observer to the airport.
//   0..359 = that bearing.
#ifndef RADAR_VIEW_BEARING_DEG
#define RADAR_VIEW_BEARING_DEG (-1)
#endif
// 0 = view-up (facing bearing at the top), 1 = north-up.
#ifndef RADAR_NORTH_UP
#define RADAR_NORTH_UP 0
#endif
// Physical mounting of the board, clockwise quarter turns 0..3 (runtime).
#ifndef RADAR_SCREEN_ROTATION
#define RADAR_SCREEN_ROTATION 0
#endif

// ---------------------------------------------------------------------------
// Range
// ---------------------------------------------------------------------------
// Outer-ring radius in nautical miles (runtime; tapping the range text or
// swiping left/right changes it and the last choice is remembered).
#ifndef RADAR_DEFAULT_RANGE_NM
#define RADAR_DEFAULT_RANGE_NM 10
#endif
// Ranges to step through (nautical miles, ascending, <= 125).
#ifndef RADAR_RANGE_STEPS_NM
#define RADAR_RANGE_STEPS_NM {5, 10, 20}
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
// Position age policy: mute the target and show "STALE" after this long...
#ifndef RADAR_STALE_MUTE_S
#define RADAR_STALE_MUTE_S 30
#endif
// ...drop arrival estimates and cancel alerts after this long...
#ifndef RADAR_STALE_NO_ETA_S
#define RADAR_STALE_NO_ETA_S 60
#endif
// ...and remove the target after this long.
#ifndef RADAR_STALE_AFTER_S
#define RADAR_STALE_AFTER_S 180
#endif

// ---------------------------------------------------------------------------
// Alerts
// ---------------------------------------------------------------------------
// Show the full-screen "LOOK RIGHT" alert when a likely arrival is this many
// seconds from the runway threshold (runtime switch: alerts).
#ifndef RADAR_ALERT_ETA_S
#define RADAR_ALERT_ETA_S 75
#endif
// Wake the screen (from idle, screen-off, or a swipe-down) when an aircraft
// is predicted to land within this many minutes, and keep it awake until it
// lands (runtime landingWakeMin, 0 = off).
#ifndef RADAR_LANDING_WAKE_MIN
#define RADAR_LANDING_WAKE_MIN 10
#endif

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
// Day brightness in percent (runtime).
#ifndef RADAR_DEFAULT_BRIGHTNESS
#define RADAR_DEFAULT_BRIGHTNESS 80
#endif
// POSIX TZ string for the clock and night hours. Hawaii has no DST.
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
// Night mode and AMOLED burn-in protection
// ---------------------------------------------------------------------------
// Night palette (runtime nightMode): 0 = by the hours below, 1 = always day,
// 2 = always night. Swiping up on the radar overrides it until the next
// scheduled change.
#ifndef RADAR_NIGHT_MODE
#define RADAR_NIGHT_MODE 0
#endif
// Night window in local hours [start, end) (runtime; equal = never).
#ifndef RADAR_NIGHT_START_H
#define RADAR_NIGHT_START_H 19
#endif
#ifndef RADAR_NIGHT_END_H
#define RADAR_NIGHT_END_H 6
#endif
// Panel brightness at night, percent (runtime).
#ifndef RADAR_NIGHT_BRIGHTNESS
#define RADAR_NIGHT_BRIGHTNESS 12
#endif
// Enter the dim idle screen after this many seconds without touch and
// without an aircraft about to land (runtime, 0 disables).
#ifndef RADAR_IDLE_DIM_S
#define RADAR_IDLE_DIM_S 90
#endif
// Panel brightness cap while idle, percent (runtime).
#ifndef RADAR_IDLE_BRIGHTNESS
#define RADAR_IDLE_BRIGHTNESS 40
#endif
// Turn the panel off after this many seconds of idle (runtime, 0 = never).
#ifndef RADAR_BLACKOUT_S
#define RADAR_BLACKOUT_S 900
#endif
// Also wake from idle while an aircraft is within this many NM of the
// observer (runtime, 0 = only for arrivals).
#ifndef RADAR_WAKE_RANGE_NM
#define RADAR_WAKE_RANGE_NM 0
#endif
// Shift the whole scene by up to RADAR_PIXEL_SHIFT_MAX pixels every
// RADAR_PIXEL_SHIFT_S seconds.
#ifndef RADAR_PIXEL_SHIFT_S
#define RADAR_PIXEL_SHIFT_S 60
#endif
#ifndef RADAR_PIXEL_SHIFT_MAX
#define RADAR_PIXEL_SHIFT_MAX 2
#endif

// ---------------------------------------------------------------------------
// Wi-Fi setup
// ---------------------------------------------------------------------------
// Name prefix of the temporary setup network; 4 hex digits of the MAC
// address are appended (e.g. "Kona-Radar-7A2C"). Its password is
// "look-up-" plus 4 random digits, generated once per device.
#ifndef RADAR_AP_PREFIX
#define RADAR_AP_PREFIX "Kona-Radar"
#endif
#ifndef RADAR_AP_PASS_PREFIX
#define RADAR_AP_PASS_PREFIX "look-up-"
#endif
// mDNS host name on the home network: http://<name>.local/
#ifndef RADAR_HOSTNAME
#define RADAR_HOSTNAME "kona-radar"
#endif
// Seconds to wait for the saved network at boot before reporting it.
#ifndef RADAR_WIFI_CONNECT_TIMEOUT_S
#define RADAR_WIFI_CONNECT_TIMEOUT_S 25
#endif
// Seconds to wait for the first flight data after joining Wi-Fi in setup
// before reporting "no internet".
#ifndef RADAR_ONLINE_CHECK_S
#define RADAR_ONLINE_CHECK_S 30
#endif
// Hold anywhere on the screen (or the BOOT button) this long to forget the
// Wi-Fi credentials and start setup again. Other settings are kept.
#ifndef RADAR_RESET_HOLD_S
#define RADAR_RESET_HOLD_S 5
#endif

#define RADAR_FW_VERSION "2.1.0"
