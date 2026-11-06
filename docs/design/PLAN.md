# Kona Radar mockup production plan

Goal: twelve polished 466×466 instrument screens and matching bezel renders, plus an implementation-ready UI specification. This is visual design only; no firmware or shared repository is changed.

- [x] Establish constraints: round AMOLED, touch, configurable balcony observer (placeholder in this copy), view-up 260° true, six simulated aircraft, no implied live information.
- [x] Compare directions: phosphor radar is atmospheric but cluttered; dashboard cards waste the round display; restrained vector instrument gives the best glance-to-sky experience. Choose the instrument.
- [x] Render consistent tokens, typography, icons, geography, QR and twelve screen states with Pillow at 3× resolution.
- [x] Inspect native PNGs and a labeled contact sheet; iterate collisions and circular safe areas.
- [x] Verify image dimensions, mask, QR decoding, aircraft count, layout geometry and all output paths.
- [x] Deliver DESIGN.md, reusable rendering source, exact PNG files and a local ZIP.

Files: `render.py` (renderer), `assets/*.ttf` (static fonts), `screens/*.png` (466×466), `device/*.png` (600×600), `contact-sheet.png`, `DESIGN.md`, `verify.py`, `verification.txt`.

Review outcome: range-label collisions removed; all critical text fits a 228px radius; native QR decodes. Outline-only land/ocean keeps the zoom state genuinely black. Coordinate-accurate example directions are 77° right in the overview and 47° right in the later alert (placeholder observer). No external publication, repo change or firmware work occurred.
