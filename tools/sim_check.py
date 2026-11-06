#!/usr/bin/env python3
"""Rendering check of every UI screen on the simulated 466x466 panel.

Runs the host simulator (same lv_conf.h as the device: RGB565, 4 bpp fonts,
466x32 partial buffers) with the extra states, then writes to docs/sim-check/:

  screens/  every screen at native size (gray = outside the round panel)
  zoom/     4x nearest-neighbour crops of the regions most likely to show
            aliasing, clipping or overlap
  checks.txt  edge-band scan, QR decode, the simulator's text audit

  pio run -e simulator && python3 tools/sim_check.py

Needs Pillow and numpy; QR decoding also needs opencv-python-headless.
`--before DIR` (a SIM_OUT directory from an older build) adds before/after
pairs of the crops listed in BEFORE_AFTER.
"""
import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SAFE_R = 226  # DESIGN.md: text within r=228, 2 px kept for the pixel shift

# name: (screen, x, y, w, h)
CROPS = {
    "radar_rings_ticks_nw": ("06_radar_view_aligned", 76, 95, 80, 80),
    "radar_view_marker_top": ("06_radar_view_aligned", 193, 66, 80, 60),
    "radar_focus_ring_hal208": ("06_radar_view_aligned", 289, 189, 80, 80),
    "radar_runway_swa250": ("06_radar_view_aligned", 190, 168, 110, 64),
    "radar_glyph_mho507": ("06_radar_view_aligned", 239, 256, 80, 60),
    "radar_helicopter_n123bh": ("06_radar_view_aligned", 176, 200, 72, 52),
    "radar_observer_you": ("06_radar_view_aligned", 203, 225, 60, 60),
    "radar_north_label_ticks": ("06_radar_view_aligned", 386, 255, 60, 56),
    "radar_count_tag": ("06_radar_view_aligned", 55, 108, 75, 40),
    "radar_dock_corner": ("06_radar_view_aligned", 80, 328, 110, 90),
    "radar_coast_bezel_left": ("06_radar_view_aligned", 0, 215, 80, 60),
    "radar_footer": ("06_radar_view_aligned", 120, 418, 226, 28),
    "radar_5nm_focus": ("07_radar_zoom_5nm", 290, 136, 120, 80),
    "radar_20nm_crowd": ("x_radar_20nm", 170, 150, 150, 160),
    "radar_emergency_stale": ("x_radar_emergency_stale", 150, 110, 160, 200),
    "radar_quiet_dock": ("x_radar_quiet_sky", 88, 334, 290, 80),
    "radar_long_dock": ("x_radar_long_dock", 88, 334, 290, 80),
    "radar_shift_bezel_br": ("x_radar_shift_+2_+2", 300, 330, 140, 120),
    "night_glyphs": ("10_night_mode", 289, 189, 80, 80),
    "night_dock_text": ("10_night_mode", 95, 340, 280, 70),
    "night_footer_wifi_lost": ("x_night_footer_wifi_lost", 100, 418, 266, 28),
    "night_footer_location": ("x_night_footer_location_not_set", 100, 418, 266, 28),
    "splash_art": ("01_splash", 160, 78, 150, 150),
    "splash_bar_status": ("01_splash", 180, 390, 106, 45),
    "qr_setup": ("02_wifi_setup_qr", 151, 145, 164, 164),
    "qr_setup_finder": ("02_wifi_setup_qr", 151, 145, 60, 60),
    "qr_link": ("13_phone_settings_link", 151, 145, 164, 164),
    "setup_small_text": ("02_wifi_setup_qr", 150, 362, 166, 70),
    "header_rule_top": ("03_wifi_connecting", 190, 18, 86, 46),
    "spinner_0": ("x_spinner_0", 162, 169, 142, 142),
    "spinner_1": ("x_spinner_1", 162, 169, 142, 142),
    "spinner_2": ("x_spinner_2", 162, 169, 142, 142),
    "spinner_3": ("x_spinner_3", 162, 169, 142, 142),
    "steps_dots": ("03_wifi_connecting", 140, 370, 186, 36),
    "hold_ring_34": ("x_hold_34", 162, 129, 142, 142),
    "hold_ring_68": ("x_hold_68", 162, 129, 142, 142),
    "connected_check": ("04_wifi_connected", 168, 159, 130, 130),
    "failed_button": ("05_wifi_failed", 118, 340, 230, 90),
    "alert_arrow": ("09_look_up_alert", 165, 178, 136, 136),
    "alert_bezel_arc": ("09_look_up_alert", 400, 100, 66, 150),
    "alert_arc_start": ("09_look_up_alert", 402, 100, 30, 30),
    "hold_arc_end": ("x_hold_34", 272, 218, 32, 32),
    "alert_long_text": ("x_alert_long_text", 60, 40, 346, 130),
    "detail_footer": ("08_aircraft_detail", 100, 418, 266, 28),
    "detail_chevron_title": ("08_aircraft_detail", 95, 40, 240, 60),
    "detail_tag_divider": ("08_aircraft_detail", 85, 165, 296, 70),
    "detail_long_text": ("x_detail_long_text", 60, 75, 346, 130),
    "alignment_cone_runway": ("12_balcony_alignment", 188, 196, 90, 100),
    "alignment_segmented": ("12_balcony_alignment", 104, 358, 258, 58),
    "alignment_footer": ("12_balcony_alignment", 110, 420, 246, 26),
    "idle_dim_text": ("11_idle_dim", 140, 310, 186, 140),
    "idle_spot5_bottom": ("x_idle_spot5", 140, 330, 186, 116),
    "idle_spot3_right": ("x_idle_spot3", 230, 120, 140, 230),
}

# Crops worth showing next to the build before the fixes.
BEFORE_AFTER = ["detail_footer", "alignment_footer", "night_footer_wifi_lost", "night_footer_location",
                "hold_arc_end", "alert_arc_start", "spinner_1", "radar_focus_ring_hal208", "radar_20nm_crowd",
                "detail_long_text", "radar_runway_swa250"]


def run_sim(program, out, raw):
    env = dict(os.environ, SIM_OUT=str(out), SIM_EXTRA="1")
    if raw:
        env["SIM_RAW"] = "1"
    else:
        env.pop("SIM_RAW", None)
    p = subprocess.run([str(program)], cwd=ROOT, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.stdout.write(p.stdout + p.stderr)
        raise SystemExit("simulator failed")
    return p.stdout


def zoom(img, box, scale=4):
    x, y, w, h = box
    return img.crop((x, y, x + w, y + h)).resize((w * scale, h * scale), Image.NEAREST)


def edge_scan(raw_dir):
    """Lit pixels in the outer visible band and drawn outside the panel."""
    yy, xx = np.mgrid[0:466, 0:466]
    r = np.hypot(xx + 0.5 - 233, yy + 0.5 - 233)
    lines = []
    for f in sorted(raw_dir.glob("*.png")):
        a = np.asarray(Image.open(f).convert("RGB")).astype(int)
        lit = a.max(axis=2) > 12
        band = lit & (r > SAFE_R) & (r <= 233)
        out = lit & (r > 233)
        lines.append(f"{f.stem:36s} max r {r[lit].max() if lit.any() else 0:6.1f}  "
                     f"lit in band {SAFE_R}-233: {band.sum():4d}  lit outside panel: {out.sum():4d}")
    return lines


def qr_check(screens):
    try:
        import cv2
    except ImportError:
        return ["QR decode skipped (no opencv-python-headless)"]
    out = []
    for name in ("02_wifi_setup_qr", "02b_wifi_setup_phone_joined", "13_phone_settings_link"):
        img = cv2.imread(str(screens / f"{name}.png"))
        value, _, _ = cv2.QRCodeDetector().detectAndDecode(img)
        out.append(f"{name}: {'decodes' if value else 'DOES NOT DECODE'} at native size: {value!r}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--program", default=ROOT / ".pio/build/simulator/program", type=Path)
    ap.add_argument("--out", default=ROOT / "docs/sim-check", type=Path)
    ap.add_argument("--before", type=Path, help="SIM_OUT dir rendered by an older build")
    args = ap.parse_args()

    screens, zooms = args.out / "screens", args.out / "zoom"
    for d in (screens, zooms):
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)

    with tempfile.TemporaryDirectory() as tmp:
        masked, raw = Path(tmp) / "masked", Path(tmp) / "raw"
        log = run_sim(args.program, masked, raw=False)
        run_sim(args.program, raw, raw=True)
        for f in sorted(masked.glob("*.png")):
            Image.open(f).convert("RGB").save(screens / f.name, optimize=True)
        edges = edge_scan(raw)

    for name, (screen, *box) in CROPS.items():
        zoom(Image.open(screens / f"{screen}.png"), box).save(zooms / f"{name}.png", optimize=True)

    if args.before:
        pairs = args.out / "before-after"
        shutil.rmtree(pairs, ignore_errors=True)
        pairs.mkdir()
        for name in BEFORE_AFTER:
            screen, *box = CROPS[name]
            b = zoom(Image.open(args.before / f"{screen}.png").convert("RGB"), box)
            a = zoom(Image.open(screens / f"{screen}.png"), box)
            sheet = Image.new("RGB", (b.width, b.height * 2 + 8), (255, 0, 255))
            sheet.paste(b, (0, 0))
            sheet.paste(a, (0, b.height + 8))
            sheet.save(pairs / f"{name}.png", optimize=True)

    audit = [l for l in log.splitlines() if "[warn]" in l or "text audit" in l or "FAIL" in l]
    report = ["# Generated by tools/sim_check.py", "", "## Simulator checks and text audit", ""]
    report += [l.strip() for l in log.splitlines() if "[ ok ]" in l or "FAIL" in l]
    report += audit + ["", "## QR codes", ""] + qr_check(screens)
    report += ["", f"## Edge band (raw render, lit = any channel > 12; text safe radius {SAFE_R})", ""] + edges
    (args.out / "checks.txt").write_text("\n".join(report) + "\n")
    print("\n".join(audit))
    print(f"wrote {len(list(screens.glob('*.png')))} screens, {len(CROPS)} zooms to {args.out}")


if __name__ == "__main__":
    main()
