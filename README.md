# Kona Radar

A glance-then-look-up flight tracker for the **Waveshare
ESP32-S3-Touch-AMOLED-1.43**. It hangs on a balcony with a view of Kona
International Airport (KOA) and shows nearby aircraft, using free public ADS-B
data over Wi-Fi. The scope is centered on **you**, and the top of the screen
is the direction you face, so an aircraft drawn up and to the right is up and
to the right when you look out. When an arrival is about a minute from
landing, the screen says which way to look.

| Radar (view up, 10 NM) | 5 NM, departure selected | Aircraft detail | Look-up alert |
| --- | --- | --- | --- |
| ![radar](docs/img/06_radar_view_aligned.png) | ![zoom](docs/img/07_radar_zoom_5nm.png) | ![detail](docs/img/08_aircraft_detail.png) | ![alert](docs/img/09_look_up_alert.png) |
| **Phone setup** | **Night** | **Quiet idle** | **Balcony alignment** |
| ![setup](docs/img/02_wifi_setup_qr.png) | ![night](docs/img/10_night_mode.png) | ![idle](docs/img/11_idle_dim.png) | ![alignment](docs/img/12_balcony_alignment.png) |

*These are screenshots from the host simulator, which runs the real UI code
with the design's example traffic around a made-up observer. The gray
corners are outside the round panel. The UI follows the design in
[`docs/design/`](docs/design/DESIGN.md).*

- [Hardware](#hardware)
- [Flashing](#flashing)
- [First-time Wi-Fi setup](#first-time-wi-fi-setup)
- [Your location (and keeping it private)](#your-location-and-keeping-it-private)
- [Using the radar](#using-the-radar)
- [Flight cues](#flight-cues)
- [Data source](#data-source)
- [Configuration](#configuration)
- [AMOLED burn-in protection](#amoled-burn-in-protection)
- [Code layout and the UI design](#code-layout-and-the-ui-design)
- [Tests and simulator](#tests-and-simulator)
- [What to check first on real hardware](#what-to-check-first-on-real-hardware)
- [Licenses and credits](#licenses-and-credits)

## Hardware

The board is the ESP32-S3R8 version: 16 MB flash and 8 MB octal PSRAM. The
display and touch details below come from Waveshare's
[wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43) and its
official demo package (`ESP32-S3-AMOLED-1.43-Demo`, `Arduino/examples/07_LVGL_Test`).

| Part | What the firmware does |
| --- | --- |
| 1.43" 466×466 AMOLED, QSPI | Waveshare ships this board with **either an SH8601 or a CO5300** controller. At boot the firmware reads ID register `0xDA` over bit-banged SPI, as Waveshare's demo does. `0x86` means SH8601; anything else means CO5300, which needs a 6-pixel column offset. Pins: CS 9, SCLK 10, D0–D3 11–14, RST 21, AMOLED_EN 42. The pixel clock is 40 MHz. The driver is Espressif's `esp_lcd_sh8601` (vendored), which handles both controllers. |
| FT3168 capacitive touch, I2C `0x38` | Polled over the shared I2C bus on SDA 47 / SCL 48. The board does not wire up the touch interrupt. |
| QMI8658 IMU, PCF85063 RTC, ETA6098 PMIC, microSD | Not used yet. The clock comes from NTP. |
| BOOT button (GPIO 0) | Hold it for 5 s while running to reset Wi-Fi, the same as holding the screen. |

Graphics use **LVGL 8.4** with two 466×32-line DMA buffers in internal RAM.
The controller has no rotation hardware, so rotation is done in software in the
flush callback. The radar background (coastline, rings, runway, view cone) is
pre-rendered into a 434 KB PSRAM layer and redrawn only when the range or
orientation changes. Text uses IBM Plex fonts converted to 4-bit LVGL bitmaps.

## Flashing

1. Install [PlatformIO](https://platformio.org/platformio-ide). Use either the
   VS Code extension or the CLI (`pipx install platformio`).
2. Connect the board with a USB-C **data** cable.
3. From this folder, run:

   ```bash
   pio run -t upload        # build and flash (first build downloads the toolchain)
   pio device monitor       # optional: serial log at 115200 baud
   ```

   If the upload can't find the board or fails to connect, put it in download
   mode: hold **BOOT**, tap **RST**, release **BOOT**, then run the upload again.
   Press **RST** once flashing finishes.

The build uses the [pioarduino](https://github.com/pioarduino/platform-espressif32)
platform 55.03.312 (Arduino-ESP32 3.3.12 on ESP-IDF 5.5) and a board definition
in `boards/`. The serial log goes over the USB port (USB CDC on boot).

## First-time Wi-Fi setup

Nothing to install: it only takes a phone camera.

| 1. Scan | 2. Joining | 3. Checking for flight data | 4. Done |
| --- | --- | --- | --- |
| ![qr](docs/img/02_wifi_setup_qr.png) | ![joining](docs/img/03b_wifi_checking_internet.png) | ![failed](docs/img/05b_wifi_no_internet.png) | ![connected](docs/img/04_wifi_connected.png) |

1. **Plug the radar in.** With no network saved it shows **Set up with your
   phone** and a QR code.
2. **Point your phone's camera at the code** and tap *Join "Kona-Radar-XXXX"*.
   On Android, if the camera ignores it, go to Settings › Wi-Fi and tap the QR
   icon. The network name and password (`look-up-NNNN`) are also printed below
   the code for typing by hand. The screen then shows **Phone joined**.
3. **The sign-in page opens on the phone by itself.** If it doesn't, open
   `http://192.168.4.1/` (shown on the screen as *No page?*).
4. **Pick your home network and type its password.** Optionally paste your
   balcony's location too (see the next section); you can also skip it and
   add it later.
5. **Wait about 10–30 seconds.** The radar first joins the network
   (**Joining Wi-Fi**), then checks that flight data actually downloads
   (**Going online**). Both the phone and the radar report the result:
   - **You're connected**: tap **OPEN RADAR** or wait a few seconds.
   - **Couldn't connect**: wrong password, the network wasn't found (it must
     be 2.4 GHz; the ESP32 has no 5 GHz radio), or the router refused the
     connection. Fix it on the phone, which is still connected, or tap **TRY AGAIN**.
   - **No internet**: the Wi-Fi password was right and is saved, but no
     flight data came through. **TRY AGAIN** checks again.
6. Your phone goes back to its usual Wi-Fi. The radar remembers the network
   and reconnects by itself after power cuts or router restarts.

**Changing networks later: hold anywhere on the screen for 5 seconds**, or
hold the BOOT button. A ring fills up while you hold, and letting go cancels.
This forgets *only* the Wi-Fi; your location, view and display settings stay.
If the saved network can't be reached after power-on, the radar shows **Wi-Fi
not found**, keeps retrying in the background, and offers **SET UP WI-FI**.

![hold to reset](docs/img/14_hold_to_reset_wifi.png)

### How this matches the design

The design (`docs/design/DESIGN.md`, *First-time connection*) chose **QR →
temporary setup Wi-Fi → captive portal**, with no app or account. The
firmware was already built on that flow, so the two are reconciled rather
than replaced. To match the design:

- **Setup network.** It is now called `Kona-Radar-XXXX`, using the last two
  bytes of the MAC. Its password is `look-up-NNNN`, picked at random on first
  boot and kept in NVS, so a printed or remembered QR code stays valid. It
  used to be derived from the MAC address, which anyone could recompute.
- **Separate internet check.** Joining the Wi-Fi and getting flight data are
  separate steps. *Connected* is only shown after the first successful
  download, and "no internet" is reported apart from "wrong password", as the
  design asks.
- **Failure reasons.** Wrong password, network not found and router refused
  each get their own wording, reusing the failure layout.
- **5-second reset.** The **5 s hold anywhere** replaces the previous
  long-press settings menu and its Change Wi-Fi button. It forgets only the
  Wi-Fi.
- **Location step.** The portal asks for the observer location and facing
  bearing, as the design's phone flow does, but the step is optional so that
  joining Wi-Fi never gets stuck on a coordinates question.

The design mentions BLE as a possible optional second transport. It isn't
implemented, for the reasons in the next section.

### Why a captive portal with a QR code

| Option | What the person has to do | Problem |
| --- | --- | --- |
| **SoftAP captive portal + Wi-Fi QR code (chosen, and the design's choice)** | Point the camera, tap join, pick the network, type the password | None that matters. It works on any phone with no app, using the camera and the captive-portal page every phone already has. |
| Espressif BLE provisioning (ESP BLE Prov app + QR) | Install "ESP BLE Provisioning" from the app store, scan, pick the network | Needs a niche developer app; adds the BLE stack (~300 KB flash, RAM) |
| Improv Wi-Fi over BLE | Open a web page in Chrome, connect over Web Bluetooth | **Web Bluetooth doesn't work on iPhone or iPad** (Safari has none, and every iOS browser uses Safari's engine) |
| Improv Wi-Fi over serial | Plug into a computer and use a web flasher page | Needs a computer and a Chromium browser; no good for re-setup on a balcony |

The chosen flow avoids the usual captive-portal problems:
- The setup network has a password, shown on screen and in the QR code, so
  neighbors can't join it.
- The radar answers every OS connectivity check (Android, iOS/macOS, Windows,
  Firefox) and advertises the portal through DHCP, so the page pops up by
  itself. The IP address is on screen as a fallback.
- The password is tested before it is saved, and errors are in plain language.
- The access point stays up for 10 s after success so the phone sees the result.

## Your location (and keeping it private)

The firmware ships with a **neutral default: the observer is KOA itself**.
In that state the radar is centered on the airport, the top of the screen is
north, the footer says **SET YOUR LOCATION ON PHONE**, and the alignment
screen says **Set your location**. Nothing about where the radar actually
hangs is compiled into the public firmware.

| Location not set | Alignment, location not set |
| --- | --- |
| ![unset](docs/img/15_radar_20nm_observer_not_set.png) | ![unset alignment](docs/img/15b_alignment_location_not_set.png) |

There are three ways to give the radar its real location. None of them
involves committing it to the repository:

1. **During Wi-Fi setup**, in the optional *Your balcony* step of the portal.
2. **Any time later** on the settings page at `http://kona-radar.local/`. On
   the radar, tap the title to open the alignment screen, then tap *TAP HERE
   TO EDIT ON PHONE* for a QR code with the page's address.
3. **At build time**, for your own builds only:

   ```bash
   cp include/config.local.example.h include/config.local.h   # gitignored
   # edit RADAR_OBSERVER_LAT / RADAR_OBSERVER_LON / RADAR_VIEW_BEARING_DEG
   pio run -t upload
   ```

   `include/config.h` includes `config.local.h` first when it exists, and any
   macro there overrides the factory default. `.gitignore` also excludes
   `*.local.json`.

Values set from the phone are stored in NVS on the device and survive
firmware updates. Restoring factory settings goes back to the build's
defaults.

To get the coordinates, long-press the spot in Google or Apple Maps and copy
the two numbers. For the facing direction, keep **toward the airport** (the
default) or enter a measured compass bearing in degrees true. The alignment
screen shows the result: **Face the airport · 260°** when the airport is
straight ahead, otherwise how far left or right of your view it is.

**About the design files.** The original mockups were drawn around a real
balcony. The copy in `docs/design/` has been re-rendered around a made-up
placeholder observer (19.7470, −155.9940, about 3 NM east of KOA, facing it at
260°). The coordinates are not the only thing replaced: in a radar picture
the airport's position relative to "YOU" gives the observer away, so the
screens themselves were re-rendered too. The simulator uses the same
placeholder.

## Using the radar

| Where | Gesture | What happens |
| --- | --- | --- |
| Range text (`· 10 NM`, right half of the header) | tap | 5 → 10 → 20 NM, wrapping |
| Empty radar | swipe left / right | zoom out / in |
| Title or `VIEW 260°T` (left half of the header) | tap | Balcony alignment: bearing, distance to the airport, **NORTH UP / VIEW UP** switch, and a QR code for the phone settings |
| Aircraft icon or label | tap | select it; its summary appears in the bottom dock |
| Selected aircraft, or the dock | tap | aircraft detail |
| Overlapping aircraft | tap again | cycles through the cluster instead of guessing |
| Empty sky | tap | back to the automatic focus (the next likely arrival) |
| Detail / alignment | tap ‹ or swipe right | back to the radar |
| Look-up alert | tap | dismiss and open that aircraft's detail |
| Empty radar | swipe up | day ⇄ night colors (until the next scheduled change) |
| Empty radar | swipe down | quiet idle screen; tap to wake |
| Anywhere | hold 5 s | reset Wi-Fi only (see above) |

On the scope:
- Rings are at a third, two thirds and the full range, measured from you.
- Ticks mark every 15°, and **N** marks true north. The cyan triangle at the
  top and the shaded cone are your view.
- The runway is drawn from the FAA threshold coordinates, with its numbers.
- Aircraft are drawn as silhouettes pointing along their track, with three
  trail dots showing where they came from.
  - Cyan is traffic, sage is a departure (↑ after the name), and amber is the
    focused aircraft, which gets a double ring.
  - Helicopters have their own icon.
  - Coral means an emergency squawk. Grey means stale data.
- A dashed amber line joins a likely arrival to its runway threshold.
- The header shows the aircraft count in range, and **+N OUTSIDE** for
  aircraft just beyond the outer ring.
- The footer shows the data source and freshness, for example `ADSB.FI ·
  LIVE`, `STALE 45s` or `WI-FI LOST · RECONNECTING`.

The bottom dock always says what matters most right now:
- `APPROACHING KOA · ~3 MIN · RIGHT 77°` for the next likely arrival,
- `DEPARTING SWA250 ↑ 2,300 FT` for a selected departure,
- otherwise altitude and speed.

The aircraft detail screen adds operator and type, a role tag (`LIKELY
ARRIVAL · RWY 17`, `HELICOPTER`, `STALE`…), altitude, ground speed, vertical
rate, track, squawk, and where to look: `RIGHT 77° · ~3 MIN` and its
elevation above the horizon (`2° UP`).

## Flight cues

The cues are heuristics computed in `lib/radar_core/src/traffic.cpp`. They are
not ATC data:

- **Likely arrival.** All of these must hold:
  - the track is within 25° of a runway's landing direction, before the
    threshold and up to 15 NM out;
  - it is close to the extended centerline, with tolerance growing with
    distance;
  - altitude is on a plausible approach profile, and the aircraft is
    descending or already low;
  - it is getting closer between reports.

  The ETA is the distance to the threshold divided by ground speed, shown
  with `~`.
- **Likely departure.** Climbing at 300 ft/min or more, below 8,000 ft, past
  the far end of the runway, and close to the centerline.
- **Helicopter.** Category A7 or a known helicopter type.
- **Stale data.** Colors are muted after 30 s without a position. ETAs and
  alerts are dropped after 60 s. The aircraft is removed after 180 s.
- **Look-up alert.** Shown once per aircraft when a likely arrival in range
  is 75 s or less from the threshold. It wakes the screen and can be turned
  off on the settings page. The direction is relative to the way you face,
  for example `LOOK RIGHT · 47° from your balcony view`, with the true
  bearing underneath (`NW 307°T`). There is no compass on the board, so this
  relies on the facing bearing being right.

## Data source

There is no local receiver, so positions come from community ADS-B
aggregators. No API key is used.

| Source | Used as | Limits / terms | Notes |
| --- | --- | --- | --- |
| [adsb.fi open data](https://github.com/adsbfi/opendata) `https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{nm}` | **Primary** | 1 request/s on public endpoints, radius up to 250 NM. **Personal, non-commercial use only; attribution required** ("Data: adsb.fi"). | Also includes the aircraft type description and operator |
| [adsb.lol](https://api.adsb.lol/) `https://api.adsb.lol/v2/point/{lat}/{lon}/{nm}` | Automatic fallback | Data under [ODbL 1.0](https://opendatacommons.org/licenses/odbl/summary/); "dynamic" limits, none published; may require a (free) key in the future | Same readsb JSON format |
| airplanes.live | Not used | Returned **HTTP 403** to anonymous requests when tested and asks API users to email first | |
| OpenSky Network | Not used | 400 API credits/day anonymous; OAuth client needed for more; non-commercial research terms | Credit budget too small for a 5 s refresh |

**How the firmware uses them:**
- Every 5 s by default (never faster than every 2 s), it requests a circle
  around the observer of twice the displayed range, at least 25 NM. That
  covers the "+N outside" count and arrivals still well out on final.
- It runs in a background task on core 0 over HTTPS (with the built-in CA
  bundle) and reuses the connection between requests.
- On HTTP 429 it waits 60 s. On other errors it backs off exponentially, up
  to 60 s.
- After 3 failures in a row it switches to the other source, and drifts back
  to the preferred one later.

**Coverage near KOA is the main limitation, not the firmware.** The
aggregators depend on volunteer receivers, and the Big Island's west side has
few. In the snapshot used for the test fixtures:
- adsb.fi had 37 aircraft within 150 NM of KOA, but only **2 within 25 NM**.
- adsb.lol had **none within 50 NM**.

Jets at altitude near Kona show up. Low aircraft on approach, helicopters, and
ground traffic often don't, which also limits the arrival cues. A cheap local
feeder (an RTL-SDR dongle on a Raspberry Pi, feeding adsb.fi or adsb.lol)
anywhere around Kona would improve coverage for everyone.

## Configuration

### From a phone, no reflashing

When the radar is online, open **`http://kona-radar.local/`** on a phone or
computer on the same Wi-Fi. If `.local` names don't work on your device, use
the QR code on the alignment screen, which also shows the IP address.

| Setting | Default |
| --- | --- |
| Balcony location (paste "lat, lon") and height | not set (= the airport), 50 ft |
| Top of the screen | the direction you face (view up); or north |
| You face | toward the airport (auto); or a compass bearing |
| Range | 10 NM (5 / 10 / 20) |
| Screen mounting rotation | 0 |
| Look-up alerts | on |
| Brightness | 80% |
| Quiet screen after / its brightness | 90 s / 40% |
| Screen off after quiet for | 900 s |
| Wake when a plane is within | 0 = only for arrivals |
| Night colors | by the hours 19:00–06:00, night brightness 12% (or never / always) |
| Airport and label, data source, refresh | KOA 19.738765, −156.045631; adsb.fi; 5 s |

Settings are stored in NVS flash and survive power cuts and firmware updates.
Only values you change are stored, so a new firmware with different defaults
still updates everything you never touched. **Restore factory settings**
clears your changes and keeps the Wi-Fi. The page has no password: anyone on
your home Wi-Fi can change settings, which is normal for this kind of gadget.

### Build-time defaults

All factory defaults are in [`include/config.h`](include/config.h). Override
them in the gitignored `include/config.local.h` (see above) or with build
flags in `platformio.ini`, for example `-DRADAR_DEFAULT_RANGE_NM=20`.

| Macro | Default | Meaning |
| --- | --- | --- |
| `RADAR_CENTER_LAT` / `_LON` / `_NAME` | 19.738765 / −156.045631 / "KOA" | Airport |
| `RADAR_OBSERVER_LAT` / `_LON` / `_ALT_FT` | the airport / 50 | Where the radar hangs (the airport = not set) |
| `RADAR_VIEW_BEARING_DEG` | −1 | Facing bearing, degrees true (−1 = toward the airport) |
| `RADAR_NORTH_UP` | 0 | 1 = north at the top instead of the view |
| `RADAR_SCREEN_ROTATION` | 0 | Mounting, quarter turns clockwise |
| `RADAR_DEFAULT_RANGE_NM` / `RADAR_RANGE_STEPS_NM` | 10 / {5, 10, 20} | Range |
| `RADAR_REFRESH_INTERVAL_S` / `RADAR_PRIMARY_SOURCE` | 5 / 0 | Download interval; 0 adsb.fi, 1 adsb.lol |
| `RADAR_STALE_MUTE_S` / `_NO_ETA_S` / `_AFTER_S` | 30 / 60 / 180 | Stale data handling |
| `RADAR_ALERT_ETA_S` / `RADAR_IMMINENT_ETA_S` | 75 / 240 | Look-up alert; arrivals that keep the screen awake |
| `RADAR_DEFAULT_BRIGHTNESS` | 80 | Percent |
| `RADAR_IDLE_DIM_S` / `RADAR_IDLE_BRIGHTNESS` / `RADAR_BLACKOUT_S` | 90 / 40 / 900 | Quiet screen and screen-off |
| `RADAR_WAKE_RANGE_NM` | 0 | Also wake for any aircraft this close (0 = off) |
| `RADAR_NIGHT_MODE` / `_START_H` / `_END_H` / `RADAR_NIGHT_BRIGHTNESS` | 0 / 19 / 6 / 12 | Night colors (0 by hours, 1 never, 2 always) |
| `RADAR_PIXEL_SHIFT_S` / `_MAX` | 60 / 2 | Burn-in pixel shift |
| `RADAR_AP_PREFIX` / `RADAR_AP_PASS_PREFIX` / `RADAR_HOSTNAME` | "Kona-Radar" / "look-up-" / "kona-radar" | Setup network and mDNS name |
| `RADAR_ONLINE_CHECK_S` / `RADAR_RESET_HOLD_S` | 30 / 5 | Setup internet check; hold time for the Wi-Fi reset |
| `RADAR_TIMEZONE` | "HST10" | POSIX TZ for the night schedule |
| `RADAR_PANEL_FORCE` | 0 | 0 auto-detect, 1 SH8601, 2 CO5300 |
| `RADAR_LOG_FPS` | 0 | 1 = log display throughput and memory every 5 s |

## AMOLED burn-in protection

OLED pixels age with use, and static bright elements age fastest. These
measures reduce aging but can't prevent it entirely:
- **True black everywhere.** Land and ocean are unlit, the coastline is a
  thin dim line, and there is no static lit panel apart from the small dock.
- **Quiet idle.** After 90 s without a touch the radar is replaced by a dim
  summary: the aircraft count and the next arrival. Its position changes
  every minute. This doesn't happen while an arrival is within 4 minutes,
  and the look-up alert wakes it.
- **Screen off.** After 15 more minutes the panel turns off. The first touch
  only turns it back on.
- **Night.** From 19:00 to 06:00 colors switch to a warm low-luminance
  palette, and brightness is capped at 12%.
- **Pixel shift.** Every minute the whole scene moves by up to 2 px in a
  9-position pattern. The design keeps 2 px of margin for this.

The QR setup screen is the only white area. It is shown only during setup.

## Code layout and the UI design

```
docs/design/              the UI design: DESIGN.md (spec), screens/, device/, contact sheet,
                          render.py (pixel reference), verify.py, fonts (IBM Plex, OFL)
include/config.h          build-time defaults; include/config.local.h (gitignored) overrides
include/lv_conf.h         LVGL configuration
lib/radar_core/           portable C++ with no Arduino dependency (tested on the host)
  adsb_parser.*           readsb / ADSBx-v2 JSON (adsb.fi, adsb.lol) -> Aircraft
  geo.*                   haversine, observer-centered projection with view rotation, look angles
  tracker.*               merge downloads, dead reckoning, trails, expiry
  traffic.*               per-aircraft distance/direction, arrival/departure/ETA cues
  settings.*              Settings struct, validation, JSON (web page + NVS)
  format.*                text: altitudes, airline/type names, "RIGHT 42°", "~3 MIN", "NW 305°T"
  places.*, coastline_data.cpp   airports with runway thresholds, simplified coastlines
src/hal/                  board pins, AMOLED panel (SH8601/CO5300), FT3168, LVGL port
src/net/                  Wi-Fi manager + setup portal, ADS-B downloader, web pages
src/app/settings_store.*  NVS persistence (settings, Wi-Fi, setup password)
src/ui/                   everything you see; no Wi-Fi/HTTP/NVS/hardware code
  ui_model.h              the only interface: Model (state in) + Actions (intent out)
  theme.*                 day/night palettes and fonts from the design
  fonts/                  IBM Plex as LVGL fonts (regenerate with tools/gen_fonts.sh)
  draw.*, widgets.*       antialiased vector drawing; design-anchored text, panels, buttons
  radar_view.*            the scope: pre-rendered layer, aircraft, trails, label placement
  screen_radar.cpp        radar screen: header, dock, footer, taps and swipes
  screens_setup.cpp       splash, phone QR, connecting, connected/failed
  screens_info.cpp        detail, look-up alert, quiet idle, balcony alignment
  ui.*                    routing between screens, gesture recognizer, 5 s hold overlay
src/sim/sim_main.cpp      host simulator rendering the UI to PNG and checking interactions
src/main.cpp              app controller: Wi-Fi/feed wiring, alerts, idle/night/screen-off policy
tools/                    coastline and font generators
```

Text positions in the UI use the design's convention: a y coordinate is the
top of the capital letters. `designTopOffset()` in `theme.cpp` converts it
from each font's metrics. Coordinates in `src/ui` can therefore be copied
straight from `docs/design/render.py`. To restyle:
- Colors are in `src/ui/theme.cpp`, as day and night palettes.
- Each screen's layout is in its `screens_*.cpp` file.
- The scope drawing is in `radar_view.cpp`.

The UI only reads `ui::Model` and calls `ui::Actions`, so a redesign doesn't
touch drivers, networking or storage. To change fonts or glyph sets, edit
`tools/gen_fonts.sh` and run it with `LV_FONT_CONV` pointing at
[`lv_font_conv`](https://github.com/lvgl/lv_font_conv).

## Tests and simulator

```bash
pio test -e native                                     # 21 unit tests, no hardware
pio run -e simulator && .pio/build/simulator/program   # screenshots in sim_out/ + interaction checks
python3 docs/design/render.py && python3 docs/design/verify.py   # regenerate/verify the design
```

**The unit tests cover:**
- Parsing real captured responses from adsb.fi and adsb.lol near KOA, plus
  edge cases (`"alt_baro": "ground"`, missing fields, emergencies, bad JSON,
  size limits).
- Checking the projection against the distance and bearing the servers
  compute.
- The view-aligned projection, look angles and screen rotation.
- KOA runway data.
- Arrival, departure and helicopter cues and ETA on the design's example
  traffic, including that cues need track history and fresh data.
- The tracker, text formatting, settings JSON with clamping and the NVS diff,
  and the coastline.

**The simulator** runs the real `src/ui` code on LVGL with the device's
32-line partial buffers. It plays the design's example traffic, as a minute
of 10-second reports so trails and cues build up, around the placeholder
observer. It also plays the captured adsb.fi response. It writes a PNG of
every design screen and its variants (setup network joined, checking
internet, no internet, Wi-Fi not found, phone settings QR, hold overlay,
location not set, stale feed, pixel shift). It then checks the touch model
through the same gesture recognizer the device uses:
- the range text and swipes;
- selecting aircraft, and opening detail from the selection and from the dock;
- the back chevron and right swipe;
- night and idle swipes, and tap to wake;
- dismissing the alert;
- the setup buttons;
- the alignment switch;
- a 5 s hold that resets Wi-Fi, and an early release that doesn't.

Compare its output with `docs/design/screens/`.

## What to check first on real hardware

The following haven't been tested on the board. The build, parsing, geo math,
traffic cues, UI rendering and interaction logic were all checked on the host.

1. **Display comes up.** The serial log prints `panel SH8601 (id 0x86)` or
   `panel CO5300 (id 0x..)`.
   - If the picture is shifted or has a stripe at one edge, force the other
     controller with `-DRADAR_PANEL_FORCE=1` (SH8601) or `=2` (CO5300).
   - If it flickers or tears, lower the pixel clock with
     `-DRADAR_PANEL_PCLK_HZ=20000000`.
2. **Touch and gestures.** Check these:
   - tapping a blip selects it, and a second tap opens the detail;
   - tapping the range text changes the range;
   - swipes work;
   - a 5 s hold shows the ring and resets Wi-Fi.

   If the picture is sideways for your mounting, set *Screen mounting* on the
   settings page. Touch rotates with the picture.
3. **Phone setup.** Check that the camera QR joins `Kona-Radar-XXXX` and that
   the page pops up by itself (on an iPhone, the "Kona Radar" sheet). Try a
   wrong password once to see the failure screen. Then enter your location in
   the optional step.
4. **Frame rate and memory.** Build with `-DRADAR_LOG_FPS=1` and watch the
   log. Expect well under one full frame of pixels per second when nothing
   moves, and free internal heap staying above ~40 KB.
5. **Direction cues.** Once the location is set, open the alignment screen
   and check that the bearing and "AHEAD/LEFT/RIGHT" match what you see from
   the balcony. Then compare a real arrival's **LOOK RIGHT/LEFT** with where
   it actually appears.
6. **Coverage.** Compare against an app like Flightradar24 from the balcony.
   Gaps are almost always the aggregators' receiver coverage (see
   [Data source](#data-source)).

## Licenses and credits

- Aircraft data: [adsb.fi](https://adsb.fi) (personal non-commercial use,
  attribution) and [adsb.lol](https://adsb.lol) (ODbL 1.0).
- UI fonts: [IBM Plex](https://github.com/IBM/plex) Sans Condensed and Mono,
  SIL Open Font License 1.1 (`docs/design/assets/OFL.txt`).
- `src/hal/esp_lcd_sh8601.*`: Espressif Systems, Apache-2.0 (from Waveshare's
  demo package).
- Coastlines: US Census Bureau cartographic boundary files (public domain),
  simplified by `tools/gen_coastline.py`.
- Runway thresholds: FAA airport data for KOA.
- [LVGL](https://lvgl.io) (MIT) and [ArduinoJson](https://arduinojson.org) (MIT).
