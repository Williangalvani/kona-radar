# Kona Radar

A stand-alone, glance-then-look-up flight tracker for the **Waveshare
ESP32-S3-Touch-AMOLED-1.43**. It shows live aircraft around Kona International
Airport (KOA) on a round radar scope, using free public ADS-B data over Wi-Fi.
It is designed to sit on a balcony with a view of the airport: by default the
top of the screen points the way you face when you look toward KOA.

| View-aligned (default, top = 281°) | North-up | Details card | Settings |
| --- | --- | --- | --- |
| ![radar](docs/img/radar_view_aligned.png) | ![north up](docs/img/radar_north_up.png) | ![details](docs/img/details.png) | ![settings](docs/img/settings.png) |

*(Screenshots come from the host simulator, which runs the real UI code. The
gray corners are outside the round panel.)*

- [Hardware](#hardware)
- [Flashing](#flashing)
- [First-time Wi-Fi setup](#first-time-wi-fi-setup)
- [Using the radar](#using-the-radar)
- [Data source](#data-source)
- [Configuration](#configuration)
- [AMOLED burn-in protection](#amoled-burn-in-protection)
- [Code layout and restyling the UI](#code-layout-and-restyling-the-ui)
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
| BOOT button (GPIO 0) | Hold it for 5 s while running to open Wi-Fi setup. |

Graphics use **LVGL 8.4** with two 466×32-line DMA buffers in internal RAM.
The controller has no rotation hardware, so rotation is done in software in the
flush callback. The radar background is pre-rendered into an 868 KB PSRAM layer.

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

1. **Plug the radar in.** If no network has been set up yet, it shows
   **"Wi-Fi setup"** and a QR code.

   ![setup](docs/img/setup_join.png)

2. **Open your phone's camera and point it at the code.** Tap the notification
   that says *Join "KonaRadar-XXXX"*. On Android, if the camera ignores it, go
   to Settings › Wi-Fi and tap the QR icon. The screen also shows the network
   name and password in case you prefer to type them.
3. **A "Kona Radar" page opens on your phone by itself.** If it doesn't, the
   radar now shows a second QR code: scan it, or open `http://192.168.4.1/`.

   ![portal](docs/img/setup_portal.png)

4. **Tap your home network, type its password, and tap Connect.**
5. **Wait about 10–20 seconds.** The radar tests the password before saving
   it. If the password is wrong, both the phone and the radar say so and you
   can try again. If the network isn't found, it must be a 2.4 GHz network
   (the ESP32 has no 5 GHz radio).

   ![wrong password](docs/img/setup_wrong_password.png)

6. When the radar shows **Connected ✓**, you're done. Your phone returns to
   its usual Wi-Fi by itself. The radar remembers the network and reconnects
   automatically after power cuts or router restarts.

**Changing the network later.** Long-press the radar (about 1 second) to open
Settings, then tap **Change Wi-Fi** › **Set up**. Or hold the **BOOT** button
for 5 seconds. The old network stays saved until a new one works, and
**Cancel** on the setup screen goes back to it. If the saved network can't be
reached after boot, the radar shows **No Wi-Fi**. It keeps retrying in the
background and also offers a **Set up Wi-Fi** button.

### Why a captive portal with a QR code

I compared the options for "as easy as possible for a non-technical person":

| Option | What the person has to do | Problem |
| --- | --- | --- |
| **SoftAP captive portal + Wi-Fi QR code (chosen)** | Point the camera, tap join, pick the network, type the password | None that matters. Works on any phone with no app, using the camera and captive-portal page every phone already has. |
| Espressif BLE provisioning (ESP BLE Prov app + QR) | Install "ESP BLE Provisioning" from the app store, scan, pick the network | Needs a niche developer app; adds the BLE stack (~300 KB flash, RAM) |
| Improv Wi-Fi over BLE | Open a web page in Chrome, connect over Web Bluetooth | **Web Bluetooth doesn't work on iPhone or iPad** (Safari has none, and every iOS browser uses Safari's engine) |
| Improv Wi-Fi over serial | Plug into a computer and use a web flasher page | Needs a computer and a Chromium browser; no good for re-setup on a balcony |

The chosen flow is built to avoid the usual captive-portal problems:
- The setup network has a password, shown on screen and in the QR code, so
  neighbors can't join it.
- The radar answers every OS connectivity check (Android, iOS/macOS, Windows,
  Firefox) and advertises the portal through DHCP, so the page pops up by
  itself.
- A second QR code covers phones that don't open the page.
- Credentials are tested before being saved, and errors are shown in plain
  language (wrong password, network not found or 5 GHz).
- The access point stays up 10 s after success so the phone sees the result.

## Using the radar

- **The scope.** It is centered on KOA (the **+** marked "KOA"). Your balcony
  is the **◎ YOU** marker. Range rings are labeled in nautical miles, and
  coastlines of the islands are drawn in green. Other airports are small
  squares (ITO, MUE, OGG, HNL, …).
- **Aircraft.**
  - Each aircraft is a dot that flares as the sweep passes over it.
  - The line ahead of the dot shows where it will be in one minute. The dotted
    trail shows where it has been.
  - Labels show the callsign and altitude, with ▲/▼ when it is climbing or
    descending.
  - Hollow squares are aircraft on the ground. Red means an emergency squawk,
    and amber is the aircraft you selected.
  - Positions are dead-reckoned between downloads, so movement stays smooth.
- **Header.** The clock (Hawaii time), the number of aircraft, and warnings
  such as "Data 2 min old" or "Wi-Fi lost, reconnecting".
- **Tap an aircraft** for details:
  - **Look W 287° ▲ 9° up**: which way to turn and how high above the horizon
    to look from the balcony. It accounts for Earth curvature and atmospheric
    refraction.
  - Distance from you (NM and km) and from KOA.
  - Altitude with vertical rate, speed in knots and mph, and track.
  - Squawk, registration, airline, and aircraft type.

  Tap again to close.
- **− / +** change the range: 5, 10, 25, 50, 100, or 200 NM. The radar
  remembers your choice.
- **Long-press the scope** for Settings:
  - brightness
  - view: switch between facing KOA and north-up
  - rotate screen (for mounting)
  - **Phone settings**, which shows a QR code for the settings page
  - **Change Wi-Fi**

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
- It requests a circle 1.3× the displayed range (capped at 250 NM) every 5 s
  by default, never faster than every 2 s.
- It runs in a background task on core 0 over HTTPS (with the built-in CA
  bundle) and reuses the connection between requests.
- On HTTP 429 it waits 60 s. On other errors it backs off exponentially, up
  to 60 s.
- After 3 failures in a row it switches to the other source, and drifts back
  to the preferred one later.
- Aircraft not reported for 60 s are removed.

**Coverage near KOA is the main limitation, not the firmware.** The
aggregators depend on volunteer receivers, and the Big Island's west side has
few. In the snapshot used for the test fixtures:
- adsb.fi had 37 aircraft within 150 NM of KOA, but only **2 within 25 NM**.
- adsb.lol had **none within 50 NM**.

Most aircraft were around Maui and Oahu. Jets at altitude near Kona show up;
low aircraft on approach, helicopters, and ground traffic often don't. A cheap
local feeder (an RTL-SDR dongle on a Raspberry Pi, feeding adsb.fi or
adsb.lol) anywhere around Kona would improve coverage for everyone.

## Configuration

### From a phone, no reflashing

When the radar is online, open **`http://kona-radar.local/`** on any phone or
computer on the same Wi-Fi. If `.local` names don't work on your device, use
the QR code under Settings › **Phone settings**, which shows the IP address.

| Setting | Default |
| --- | --- |
| Balcony location (paste "lat, lon", e.g. from Google Maps) | 19.700000, -156.000000 |
| Height above sea level | 100 ft |
| Radar center and its label | 19.7388, -156.0456, "KOA" |
| Range | 25 NM |
| Top of the screen points | toward the center from your balcony, a fixed compass bearing, or north (default: fixed 281°) |
| Screen mounting rotation | 0 |
| Brightness, idle dim delay and level, wake distance, night hours and level | see below |
| Preferred data source, refresh interval | adsb.fi, 5 s |

Settings are stored in NVS flash and survive power cuts and firmware updates.
Only values you change are stored, so a new firmware with different defaults
still updates everything you never touched. **Restore factory settings** on
the page clears your changes. The page has no password: anyone on your home
Wi-Fi can change settings, which is normal for this kind of gadget.

### Observer and view-aligned rotation

The scope stays **centered on KOA**. The observer (balcony) is configured
separately: the default 19.700000, -156.000000 is a few km from KOA. From there
KOA lies at a bearing of 281°, so the default **view bearing is 281°**.

The top of the screen is the direction you face when you look at the
airport, and the **YOU** marker sits straight below the center. An aircraft
drawn up and to the left of KOA is up and to the left of the airport in real
life too. Bearings and elevations in the details card are always computed
from the observer.

The *view* setting has three modes: `281` (or any bearing), `-1` = auto
(always face from the balcony toward the center, recomputed if you move
either point), or `0` = classic north-up. The Settings menu on the device
toggles between north-up and your aligned bearing.

### Build-time defaults

All factory defaults live in [`include/config.h`](include/config.h) and can
be overridden in `platformio.ini` without editing the file, e.g.
`build_flags = ... -DRADAR_DEFAULT_RANGE_NM=50`.

| Macro | Default | Meaning |
| --- | --- | --- |
| `RADAR_CENTER_LAT` / `_LON` / `_NAME` | 19.7388 / -156.0456 / "KOA" | Scope center |
| `RADAR_OBSERVER_LAT` / `_LON` / `_ALT_FT` | 19.700000 / -156.000000 / 100 | Balcony |
| `RADAR_VIEW_BEARING_DEG` | 281 | Bearing at the top (−1 auto, 0 north-up) |
| `RADAR_SCREEN_ROTATION` | 0 | Mounting, quarter turns clockwise |
| `RADAR_DEFAULT_RANGE_NM` | 25 | Starting range |
| `RADAR_RANGE_STEPS_NM` | {5,10,25,50,100,200} | Steps for − / + |
| `RADAR_REFRESH_INTERVAL_S` | 5 | Seconds between downloads (min 2) |
| `RADAR_PRIMARY_SOURCE` | 0 | 0 adsb.fi, 1 adsb.lol |
| `RADAR_STALE_AFTER_S` | 60 | Drop silent aircraft after |
| `RADAR_DEFAULT_BRIGHTNESS` | 80 | Percent |
| `RADAR_IDLE_DIM_S` / `RADAR_IDLE_BRIGHTNESS` | 300 / 20 | Idle dimming |
| `RADAR_WAKE_RANGE_NM` | 6 | Brighten when an aircraft is this close to you |
| `RADAR_NIGHT_START_H` / `_END_H` / `_BRIGHTNESS` | 22 / 6 / 8 | Night cap (local time) |
| `RADAR_PIXEL_SHIFT_S` / `_MAX` | 120 / 3 | Burn-in pixel shift |
| `RADAR_SWEEP_PERIOD_S` / `RADAR_SWEEP_FPS` | 6 / 25 | Sweep speed and frame rate |
| `RADAR_TIMEZONE` | "HST10" | POSIX TZ for the clock |
| `RADAR_PANEL_FORCE` | 0 | 0 auto-detect, 1 SH8601, 2 CO5300 |
| `RADAR_AP_PREFIX` / `RADAR_HOSTNAME` | "KonaRadar" / "kona-radar" | Setup network / mDNS name |
| `RADAR_LOG_FPS` | 0 | 1 = log display throughput and memory every 5 s |

## AMOLED burn-in protection

OLED pixels age with use, and static bright elements age fastest. The
firmware takes these measures:
- **Black background everywhere.** The moving sweep means no area stays
  equally lit.
- **Idle dimming.** After 5 minutes without a touch the screen fades to 20%.
  The first touch on a dimmed screen only wakes it, so you can't change
  anything by accident.
- **Wake on nearby traffic.** An airborne aircraft within 6 NM of the balcony
  brings full brightness back. That is the "glance, then look up" moment.
- **Night cap.** From 22:00 to 06:00 local time brightness is capped at 8%.
- **Pixel shift.** Every 2 minutes the whole scope and header move by 3 px in
  a 9-position pattern, so rings, labels, and ticks never sit on the same
  pixels for long.
- Menus and details close by themselves when the screen dims.

All of these can be changed on the settings page or in `config.h`.

## Code layout and restyling the UI

```
include/config.h          build-time defaults (see above)
include/lv_conf.h         LVGL configuration
lib/radar_core/           portable C++ with no Arduino dependency (tested on the host)
  adsb_parser.*           readsb / ADSBx-v2 JSON (adsb.fi, adsb.lol) -> Aircraft
  geo.*                   haversine, azimuthal-equidistant scope projection with
                          view rotation, look angles (curvature + refraction)
  tracker.*               merge downloads, dead reckoning, trails, expiry
  settings.*              Settings struct, validation, JSON (web page + NVS)
  format.*                altitude/speed text, airline and aircraft-type names
  places.*, coastline_data.cpp   airports and simplified island coastlines
src/hal/                  board pins, AMOLED panel (SH8601/CO5300), FT3168, LVGL port
src/net/                  Wi-Fi manager + setup portal, ADS-B downloader, web pages
src/app/settings_store.*  NVS persistence
src/ui/                   everything you see: no Wi-Fi/HTTP/NVS/hardware code
  ui_model.h              the only interface: Model (state in) + Actions (intent out)
  theme.h                 every color, font and size
  radar_view.*            the scope (pre-rendered background + sweep + aircraft)
  ui.*                    screens: radar overlay, details, settings, setup
src/sim/sim_main.cpp      host simulator rendering the UI to PNG
src/main.cpp              app controller: wires it all together, brightness policy
tools/gen_coastline.py    regenerates coastline_data.cpp from US Census shapefiles
```

To restyle for the upcoming design spec:
- Colors, fonts, and sizes live in `src/ui/theme.h`.
- Screen layout lives in `src/ui/ui.cpp`, with the layout constants at the
  top.
- The scope drawing lives in `src/ui/radar_view.cpp`.
- The UI only reads `ui::Model` and calls `ui::Actions`, so a full redesign
  doesn't touch drivers, networking, or storage.
- The simulator shows every screen in seconds without flashing.
- Custom fonts can be generated with LVGL's `lv_font_conv` and assigned in
  `theme.h`.

## Tests and simulator

```bash
pio test -e native                                  # 19 unit tests, no hardware
pio run -e simulator && .pio/build/simulator/program  # PNG screenshots in sim_out/
```

**The unit tests cover:**
- Parsing real captured responses from both adsb.fi and adsb.lol near KOA,
  plus edge cases (`"alt_baro": "ground"`, missing fields, emergencies, bad
  JSON, size limits).
- Checking the projection against the distance and bearing the servers
  themselves compute for each aircraft.
- Balcony → KOA = a few km at 281°.
- View-aligned projection, look angles, and screen-rotation math.
- Tracker dead reckoning, trails, and expiry.
- Text formatting.
- Settings JSON with clamping and the NVS diff.
- Coastline sanity.

**The simulator:**
- Runs the real `src/ui` code on LVGL with the same 32-line partial buffers
  as the device.
- Feeds it canned traffic plus the captured adsb.fi response.
- Taps aircraft and buttons and long-presses for settings, checking the
  results.
- Writes a screenshot of every screen.

## What to check first on real hardware

These could not be verified without the board. Everything else (build,
parsing, geo math, UI rendering, and interaction logic) was checked on the
host.

1. **Display comes up.** The serial log prints `panel SH8601 (id 0x86)` or
   `panel CO5300 (id 0x..)`.
   - If the picture is shifted or has a stripe at one edge, force the other
     controller with `-DRADAR_PANEL_FORCE=1` (SH8601) or `=2` (CO5300).
   - If the picture flickers or tears, lower the pixel clock with
     `-DRADAR_PANEL_PCLK_HZ=20000000`.
2. **Touch lines up.** Tap a blip and check that its details open, and that
   − and + change the range. If the picture is sideways for your mounting,
   use Settings › **Rotate screen**. Touch rotates with the picture.
3. **Wi-Fi setup on your phone.** Check that the camera QR joins the network
   and the page pops up by itself. On an iPhone, confirm that the "Kona
   Radar" sheet appears.
4. **Frame rate and memory.** Build with `-DRADAR_LOG_FPS=1` and watch the
   log. Expect about 2 million pixels/s with free internal heap staying above
   ~40 KB. If the sweep stutters, lower `RADAR_SWEEP_FPS` or the
   `sweepTrailDeg` value in `theme.h`.
5. **Coverage.** Compare against an app like Flightradar24 from the balcony.
   Gaps are almost always the aggregator's receiver coverage (see
   [Data source](#data-source)).

## Licenses and credits

- Aircraft data: [adsb.fi](https://adsb.fi) (personal non-commercial use,
  attribution) and [adsb.lol](https://adsb.lol) (ODbL 1.0).
- `src/hal/esp_lcd_sh8601.*`: Espressif Systems, Apache-2.0 (from Waveshare's
  demo package).
- Coastlines: US Census Bureau cartographic boundary files (public domain),
  simplified by `tools/gen_coastline.py`.
- [LVGL](https://lvgl.io) (MIT) and [ArduinoJson](https://arduinojson.org)
  (MIT), including LVGL's bundled Montserrat font (OFL).
