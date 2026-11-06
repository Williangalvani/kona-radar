# Rendering check on the simulated 466×466 panel

Every UI screen and state was rendered by the host simulator and inspected at
native size and as 4× nearest-neighbour crops, then compared against
`docs/design/screens/`. To regenerate everything here:

```bash
pio run -e simulator && python3 tools/sim_check.py --before <SIM_OUT dir of an older build>
```

| Folder / file | What it holds |
| --- | --- |
| `screens/` | 56 native PNGs. Gray marks the area outside the round panel; `x_*` are the extra states. |
| `zoom/` | 53 crops at 4×: range rings and ticks, the focus ring, glyphs, the helicopter, small text, the dock, the QR codes, spinner and hold-ring frames, arc ends, bezel edges, footers, idle text. |
| `before-after/` | The same crop before (top) and after (bottom) the fixes. |
| `checks.txt` | The simulator's interaction checks, its text audit, QR decoding, and an edge-band scan of the raw (unmasked) render. |

## Is the simulator faithful to the device?

The simulator and the device build compile against the **same
`include/lv_conf.h`**: `LV_COLOR_DEPTH 16` with `LV_COLOR_16_SWAP`,
`LV_DRAW_COMPLEX`, default `LV_ANTIALIAS`, and the same 4 bpp IBM Plex fonts.
The simulator also copies the device's display path:
- the same 466×32 double partial buffers;
- the same 2×2 rounder;
- the framebuffer kept as swapped RGB565, expanded to 8 bits only when the PNG is written.

**I changed nothing in the LVGL config**, because nothing differed. The
device's flush-time rotation is a lossless pixel permutation, so it is not
modeled.

I added these to the simulator (`src/sim/sim_main.cpp`):

| Addition | What it does |
| --- | --- |
| `SIM_OUT` | Picks the output directory. |
| `SIM_RAW=1` | Keeps the corners outside the panel unmasked, so drawing that leaks off the panel can be measured. |
| `SIM_EXTRA=1` | Captures extra states: four spinner frames and two hold-ring frames mid-animation; 20 and 5 NM with the observer set; radar at all four ±2 px burn-in shift corners; night radar, detail, alert and alignment; the longest night footers; all seven idle resting spots, each with the stage shift pushing outward; emergency and stale targets; a worst-case text aircraft (wide 8-character callsign, long owner and type, squawk 7700, high and close, behind the view); and a quiet sky. |
| Text audit on every screenshot | Every visible LVGL label must keep its ink box within the design's safe area (DESIGN.md: radius 228 with 2 px kept for the shift, so 226 unshifted). It must not overlap another label, and must sit inside any panel it touches. |

There is no rotating radar sweep in this UI. The animated "sweeps" are the
Wi-Fi connecting spinner, the hold-to-reset progress ring and the splash
progress bar, and the mid-animation frames cover these.

## Issues found and fixed

| # | Issue | Where | Before → after | Fix |
| --- | --- | --- | --- | --- |
| 1 | **Night footer clipped by the round edge (device-visible).** `NIGHT · WI-FI LOST · RECONNECTING` reached r = 243 and `NIGHT · SET YOUR LOCATION ON PHONE` r = 245, so both lose their first and last letters on the panel. | radar footer | `before-after/night_footer_wifi_lost.png`, `night_footer_location.png` | The footer now has a maximum width of 194 px, the width of the safe area at its baseline. `Text` drops its letter spacing when the text would overflow, and the radar drops the `NIGHT ·` prefix when the line is full (the warm palette already says it is night). |
| 2 | **Day footers inside the shift reserve.** `SET YOUR LOCATION ON PHONE` reached r = 229.0 and `TAP HERE TO EDIT ON PHONE` r = 227.2. Both were over 226 at rest; with the ±2 px shift the first goes past 228. | radar, alignment | `before-after/alignment_footer.png` | Same footer width limit; the letter spacing goes from 1 to 0 for these lines. |
| 3 | **Detail footer clipped.** `EXAMPLE · SQUAWK 3142 · 3° UP` reached r = 234.7, past the panel edge (this also showed in the README screenshot). | detail | `before-after/detail_footer.png` | The `EXAMPLE ·` prefix (simulator only) is added only when it fits; squawk and elevation always show. |
| 4 | **Hard, stair-stepped arc ends.** `draw::arc` antialiased the radial edges but cut the ends with a binary angle test. On the 6 px hold ring and 4 px alert arc the ends were visibly jagged, and on the moving spinner and hold ring the stair-steps crawl from frame to frame. | `draw::arc` (spinner, hold ring, alert arcs, splash arc, Wi-Fi icon) | `before-after/hold_arc_end.png`, `alert_arc_start.png`, `spinner_1.png` | End coverage now comes from the pixel's arc-length distance to the nearer end (square, antialiased ends). Zero-span arcs return early. |
| 5 | **Aircraft icons drawn over airport and runway labels.** At 20 NM (and at 10 NM whenever traffic crosses the field), SWA250 sat on runway ident `17`, and on the observer-not-set screen it covered `KOA`. Neither was readable. | radar | `before-after/radar_20nm_crowd.png`, `docs/img/15_radar_20nm_observer_not_set.png` | A static label (`KOA`, runway idents, `YOU`) is skipped while an aircraft icon covers at least 15 % of it, and its area is invalidated when that changes. A wingtip grazing a label leaves it alone. |
| 6 | **Arrival line ran through the focus rings into the icon.** It started 14 px from the aircraft, inside the 19/23 px rings. In the design it starts outside them. | radar | `before-after/radar_focus_ring_hal208.png`, `radar_runway_swa250.png` | Starts at 25 px for the focused aircraft (14 px otherwise). |
| 7 | **A wide callsign ran into the back chevron.** `WMW8888W` at 46 px started at x ≈ 105, on top of the `‹` at x 104–113. The audit cannot see this, because the chevron is drawn art. | detail | `before-after/detail_long_text.png` | Above 222 px wide the callsign drops to the 36 px weight. |
| 8 | **Subtitle cut in bytes.** The cut at 33 bytes produced `Incorporated  ·…` and could split a UTF-8 character (`·` is two bytes), which LVGL would render as garbage. | detail | `before-after/detail_long_text.png` | When operator and type do not fit 300 px, the type is shown alone. Otherwise the cut is width-based on whole characters, trims a dangling `·` or space, and adds `…`. |

After the fixes, the text audit reports **0 warnings** across all 56 screens
(13 before). On every non-radar screen, ink stays within r ≤ 222.3. Only the
radar coastline reaches the edge or goes past it; that is intended, since the
glass cuts it. The README screenshots in `docs/img/` were refreshed from the
fixed build.

## Checked and fine

- **Text antialiasing.** All text uses 4 bpp Plex bitmaps. Mono 10–12 labels,
  runway idents, `N`, `YOU`, tags and footers have clean edges at 4×. Every
  special glyph the UI uses (° · — ’ … ↑ ↓ ← →) is present in every font
  except the digits-only 66 px idle count, which only shows `%02d`.
- **Aircraft glyphs.** These are polygons with 4×4 supersampling (17 coverage
  levels), prerendered in 5° steps. The helicopter is an ellipse ring plus
  antialiased segments. Rings, range rings, ticks, trail dots, the runway with
  its dash pattern, the observer mark, the check mark and the alert arrow are
  all distance-field antialiased. There are no hard edges except the
  intentional QR modules.
- **QR codes.** These are integer 4 px modules with a 24 px (6-module) white
  quiet zone, and all three decode at native size (`checks.txt`).
- **Banding and dithering.** The UI uses no gradients or shadows, only flat
  fills and antialiasing ramps, so RGB565 banding cannot occur and LVGL
  dithering is irrelevant. The dim idle colors (`0x2C3638` and similar) leave
  RGB565 only about 5–6 steps in the red and blue antialiasing ramps. At 4×
  this still reads as smooth text (`zoom/idle_dim_text.png`).
- **Bezel and shift.** The radar at all four ±2 px shift corners and the
  idle screen at all seven spots stay inside the safe area. Panels such as
  the dock and tags clear the edge by 13 px or more.

## Remaining (not changed)

- When the alert arrow points behind, the outer arc runs under
  `ESTIMATE ONLY` with about 7 px of clearance (`screens/x_alert_long_text.png`).
  It is crowded but does not overlap.
- At idle spot (0, +14), `Tap to wake` sits about 10 px above the fixed
  `DIM IDLE` footer.
- The hold overlay is 92 % black, so the screen behind shows faintly through.
  That is intended context, but at 4× you can read ghost text behind the
  Wi-Fi icon.
- The 0.7 px dashed crosshair in the grid color is nearly invisible. This
  matches the design's intent, but at low brightness it may vanish entirely.
- At 20 NM in crowded skies, some aircraft labels are culled by the
  placement rules (SWA250 and N123BH in `x_radar_20nm`). This is by design;
  the dock and taps still reach them.
- In the simulator, the detail screen drops `EXAMPLE ·` because it does not
  fit alongside squawk and elevation. The device never sets example data.

## What the simulation cannot show

- **Subpixel layout.** The PNGs assume square RGB-stripe pixels. The panel's
  real subpixel arrangement (not documented by Waveshare) may add color
  fringes to 1 px lines and to the edges of 4 bpp text. The thin grid, ring
  and coast lines are the most exposed.
- **AMOLED response and gamma.** Near-black values (grid `0x23383C`, idle
  colors, the 8 % hold overlay) depend on the panel's low-gray behavior and
  the brightness setting. They may crush to black, tint green or magenta, or
  show mura at low brightness. The sim shows linear RGB565 values only.
- **Brightness and night dimming** run through the controller's brightness
  register and are not modeled. Night and idle screens are judged by color
  only.
- **Round edge.** The sim's gray mask is a hard circle at r = 233. The real
  active-area edge and the glass mask may cut a pixel or two differently.
  This is why the 226 px safe radius matters.
- **Timing.** The simulator renders deterministic frames. Tearing during
  partial flushes (no TE sync), the effective spinner frame rate over QSPI at
  40 MHz, and the flush-time rotation path are only testable on hardware.
- **Sunlight and viewing angle.**
