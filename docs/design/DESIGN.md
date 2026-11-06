# Kona Radar — the balcony instrument

Visual concept, not firmware. Twelve native circular screens plus twelve 600×600 bezel renders. All flights, statuses, SSIDs and the QR credentials are examples; no live feed was queried.

> **Published copy.** The original package was drawn around a real balcony. In this public copy the observer is a made-up placeholder about 3 NM east of KOA (facing it at 260° true), the four free-flying example aircraft were moved with it so the composition is unchanged, and every derived direction/distance in the screens and this text was recomputed by `render.py`. The firmware gets the real location from the phone setup page or a gitignored `include/config.local.h`.

## Intent

Glance at the instrument, then find the real aircraft. Both land and ocean remain truly black; the coastline alone provides context, so zooming does not light a large static land polygon. True black, restrained vector geography, upright labels, short trails and one selected target; no theatrical radar sweep. Large arrival/direction text is the distance-reading layer. The map labels and telemetry are for arm’s-length inspection: a 1.43-inch screen cannot make every field readable from two meters. Test sunlight legibility on the actual panel; shade the enclosure rather than promise outdoor readability from a PNG.

## Balcony orientation and geography

- Preview observer: a placeholder, **19.7470, -155.9940** (not a real balcony). Both coordinates and facing bearing must remain editable; this is not a fixed product location.
- KOA reference center: 19.738765, -156.045631. It is **2.96 NM / 5.48 km at 260.4° true** from that observer. Preview view-up is **260° true**, so the airport sits above the observer. North is near the right edge, not the top.
- The scope is **observer-centered**, not airport-centered. Ring distances are from the balcony. Range options: 5 / 10 / 20 NM. North-up is an explicit alternative; changing orientation rotates geography, aircraft headings and trails together, never label text.
- Local projection: `E=(lon-observer_lon)*60*cos(observer_lat)`; `N=(lat-observer_lat)*60` in NM. For view bearing `h`, `x=cx+s*(E*cos(h)-N*sin(h))`, `y=cy+s*(-E*sin(h)-N*cos(h))`. Convert degrees to radians before trigonometry; `s=166/range_nm`.
- Draw runway 17/35 using the real threshold coordinates: 17 at 19.75385525, -156.04428864; 35 at 19.72367472, -156.04697358. Its true axis is **185°/005°**, close to horizontal in this balcony view, not 170°/350° true.
- Coastline is an intentionally simplified geographic schematic. It is not a navigation map.
- “RIGHT 77°” for the overview example is observer-to-aircraft bearing 337.0°T minus view 260°T. The later alert example is 307.0°T, **47° right / NW**. Direction means relative to the configured balcony view, not the board’s arbitrary mounting angle. No magnetometer is assumed.

**Privacy:** never publish a real observer location in this package — not as text, and not implicitly through where the airport sits relative to “YOU” in the radar images. Firmware starts with the observer unset (centered on the airport), not a compiled-in location.

## Palette

| Token | Hex | Use |
|---|---|---|
| Black | `#000000` | Real display background / unlit pixels |
| Ivory | `#F2F4EB` | Main text, runway, observer |
| Cyan | `#80E1E8` | Traffic, active control, view heading |
| Amber | `#FFC271` | Selection and probable arrival |
| Sage | `#B8E3AD` | Climb / connected success |
| Coral | `#FF896D` | Connection failure, not an aviation warning |
| Muted | `#97A7A9` | Secondary text |
| Quiet | `#64797C` | Micro labels / demonstration badge |
| Grid | `#23383C` | 1px rings and ticks |
| Panel / outline | `#0C171B` / `#294148` | Information dock / buttons |
| Land + ocean / coast | `#000000` / `#294045` | Outline-only geographic context |
| Night text / accent | `#A66C45` / `#D1945D` | Warm, luminance-limited night mode |
| Outside mask | `#20262B` | Mockup only; never paint this on the panel |

Selection is amber; departure state remains sage in its dock. Text, climb arrows, aircraft shape and status words supplement color. Night mode uses warm replacements, including panel and grid colors, not a blue-tinted overlay.

## Typography and LVGL

Supplied static TTFs are IBM Plex Sans Condensed Regular / Medium / SemiBold and IBM Plex Mono Medium, with SIL OFL license. Convert them to LVGL bitmap fonts with 4bpp antialiasing; draw aircraft, arrows, Wi-Fi, check and observer as vectors or small transparent assets. No emoji or webfont runtime is needed.

Native mockup sizes: 10–14px Mono for metadata, 16–21px Condensed for readable secondary text, 26–36px SemiBold for selected data and screen titles, 43–53px for alerts / identity / bearing, and 66px digits for dim idle. Exact font size and bounds for every text string are in `text-bounds.json`, and `render.py` is the pixel reference. Large display faces only need uppercase, numbers and the few displayed punctuation glyphs; subset glyphs per size and keep converted fonts in flash rather than allocating whole TTFs at runtime. There is no need to instantiate every screen’s font at once. One extra unused Plex Sans asset is included for future phone setup styling.

## Pixel layout

Origin is upper-left. All dimensions below are native pixels, not CSS-scaled coordinates.

| Element | Geometry |
|---|---|
| Physical circle | 466×466; center (233,233), usable radius 232 |
| Critical text safe area | All text-box corners within radius 228; reserve 2px for pixel shift |
| Main title | Center (233,41), 26px SemiBold |
| Mode/range line | Center (233,73), 13px Mono; `VIEW 260°T · 10 NM` |
| Radar rings | Center (233,252); radii 55.3 / 110.7 / 166; 10 NM at outer ring |
| Observer | Center (233,252); 16px outlined marker + 6px dot; view cone above |
| Aircraft / selection | ~20px silhouette; selection rings 38px and 46px diameter |
| Information dock | x=91, y=337, w=284, h=72, corner radius 15 |
| Example / status footer | Center y=429; ~8px visible cap height from 11px Mono |
| Setup QR | x=151, y=145, 164×164; 4px modules, 16px white quiet zone on every side |
| Buttons | 214–228px wide, 48px high, radius 12; whole surface touchable |
| Aircraft detail columns | x=150 and x=318; divider x=233; 36px main values |
| Look-up arrow | Center (233,248); 120px circle; arrow angle relative to view-up |
| Device render | 600×600, screen origin (67,67), 466px screen, graphite bezel |

Map lines may run under the information dock and be circularly clipped. Critical labels cannot. The 5 NM screen shows four in-range aircraft and `+2 OUTSIDE`; it does not pretend distant targets are still inside the range ring.

## Interactions

- Tap a blip or its label: select it; tap selected blip or the information dock: aircraft detail. Use invisible **44×44px minimum hit areas** and choose the nearest target when hit areas overlap. A second tap cycles an unresolved cluster instead of selecting an arbitrary flight.
- Tap range text, or swipe left/right on empty radar: 5 / 10 / 20 NM. Tap `VIEW 260°T`: alignment screen. Tap North Up / View Up: switch mode. Heading or coordinates open a phone configuration QR; the phone editor handles precise numbers, not a tiny circular keyboard.
- Detail: back chevron or right-swipe returns to radar. Alert: one tap dismisses, opening the selected aircraft’s detail; do not immediately re-alert the same arrival. Idle: any tap wakes. Swipe up on empty radar toggles day/night; swipe down requests dim idle.
- Setup failure: tap Try Again to re-open phone setup. Holding anywhere **5 seconds** resets only Wi-Fi credentials after a visible hold-progress indicator; releasing cancels. Preserve observer and display preferences. Do not make a normal radar long-press erase credentials.

## First-time connection

Primary path is **QR → temporary setup Wi-Fi → captive portal**, no account or native app. The QR is a standard Wi-Fi join payload, not a URL masquerading as a one-step connection. It joins `Kona-Radar-7A2C` with example password `look-up-7412`; the OS should then offer the sign-in page. If captive detection does not appear, the screen explicitly supplies **192.168.4.1**. Show network name and password for manual join. Use a unique suffix and generated per-device setup password in production; the displayed examples are not production credentials.

Phone flow: select home network → enter password → enter/confirm observer coordinates and true facing bearing → save. Connecting distinguishes joining Wi-Fi from internet/feed readiness; Connected means Wi-Fi + usable internet succeeded, then auto-opens radar. If internet is unavailable, report that separately rather than claiming the password is wrong. The failure mockup is the bad-credentials variant; network timeout and unavailable-feed variants reuse this layout with accurate copy. BLE can be an optional secondary transport using the same settings model, but it should not force an app download or imply iPhone web-Bluetooth support. The native QR decodes successfully; it is only a demonstration network, not a running device.

## Flight cues and operating behavior

Use callsign, position, barometric altitude, ground speed, true track, vertical rate, type and squawk when supplied. Never invent an origin, destination, aircraft model or exact landing clearance from callsign alone. Unknown fields show an em dash. `Likely arrival`, runway and approximate ETA are heuristics: sustained inbound track aligned with a runway, descending, near its extended centerline, and fresh data; do not label every nearby aircraft as landing. ETA is approximate distance-to-threshold / ground speed, not an airline schedule. Show `~` and suppress it for stale / missing data. The 1-minute alert is a later example snapshot of HAL208, not another simultaneous position.

Proposed feed policy: refresh 5–10s; at >30s age show `STALE 35s` and mute target color, at >60s remove arrival ETAs / cancel alerts, and at >180s remove the target. Location suggests where to look, not guaranteed line of sight through terrain or weather. This gadget is for spotting, **not navigation or collision avoidance**.

Day: manual high-contrast brightness with an enclosure/shade; no unverified automatic light sensor. Night: warm palette and low panel brightness, with an explicit manual override. After 90s without touch and without an imminent arrival, enter dim idle; a valid arrival may wake at the current day/night brightness. Shift the complete scene within ±2px once a minute (not just the text), vary idle placement, and optionally black out after 15 minutes. These mitigate, not eliminate, OLED aging. Avoid white burn-in-heavy backgrounds except the temporary QR screen.

## Files, regeneration and evidence

`contact-sheet.png` is the labeled 4×3 overview. `manifest.json` maps all screen and bezel filenames. `render.py` draws at 3× and downsamples; `verify.py` checks sizes, circular mask, text safe area, QR decoding, six/four range counts and direction geometry. All native PNGs were visually inspected and the zoom labels were moved to avoid collisions. This is not a hardware, touch-driver or sunlight test.

Regenerate: `python3 -m venv --system-site-packages .venv && .venv/bin/pip install Pillow qrcode && .venv/bin/python render.py`.
Verify: `.venv/bin/pip install opencv-python-headless && .venv/bin/python verify.py` (OpenCV is already available on the authoring box).

Runway source: [FAA airport data for KOA](https://nfdc.faa.gov/nfdcApps/services/ajv5/airportDisplay.jsp?airportId=KOA), opened November 6, 2026 HST; effective October 29–November 26, 2026. FAA reference is used for geometry only. All six aircraft are simulated design examples.
