# FASE 6 — Presentation: a new banner, a reworked portal, and what could not be published

Date: 2026-11-14.

## 1. The banner, drawn from scratch

`tools/gen_banner.py` was rewritten; the artwork is generated, so it can be reviewed in the diff
and regenerated with `python3 tools/gen_banner.py`.

- **Field:** an indigo-to-black vertical gradient with a warm sun rising behind the emblem,
  quantised through a 4×4 Bayer matrix. The dithering is what makes the bands read as a 16-bit
  screen instead of a CSS gradient; scanlines and a pixel grid sit over it.
- **Sun:** eighteen rings of the firmware's own thermal ramp (violet → ember → orange → amber →
  gold), each ring a step hotter than the last, plus a blurred core.
- **Emblem:** the hexagon the QUANTUM theme already uses, in gold and orange over the sun's core.
- **Music:** a 72-bar spectrum along the foot, coloured by the same ramp the meters use.
- **Name:** `QUANTUM` / `GAMMAWAVE` in Michroma at 96 px, filled with a gold-to-ember gradient
  through a blurred shadow pass so it holds over the brightest part of the sun. The type is
  measured and shrunk to fit its column, so a longer word cannot overflow the layout.
- **No version number anywhere**, and no type below 24 px.
- Output: 1600 × 520, `assets/branding/quantum_banner.png` (221 253 B), copied to
  `web/site/assets/banner.png` and used as the portal's `og:image`.

The first attempt put the hexagon under the wordmark and the spectrum through the subtitle; the
layout is now a grid (name block 96..820 px, emblem centred at 1330,190, spectrum band at the
foot) and the captures below were checked at that size.

## 2. The portal

`web/site/index.html` (the source; `site/` is its built output):

- **Hero copy rewritten** to answer, in order: what it is, for which device, what you can play,
  which engine families, which effects, and that the emulator and Studio need no install.
- **A new "Qué es" section** between the hero and the first technical block: three cards — to
  play (six tracks, twelve voices, ARP, 64-step sequencer), to program (nine engines, the
  modulation matrix, the LFO in Hz or in tempo), to sound (per-track effects, buses, master).
- **Real numbers**, from the tables and the tests: 275 factory sounds, 22 palettes. The CPU,
  memory and audit figures stay in their own sections, after the introduction, as asked.
- **`og:image`** now points at the banner.
- The gallery keeps the eight firmware screens; `tools/fw_shots.py --render` regenerated them
  from `tests/ui_render.c`, i.e. from the sources that were just audited in phases 3–5, not from
  mockups: `fw-home`, `fw-visualizer`, `fw-mixer`, `fw-fx`, `fw-master`, `fw-presets`,
  `fw-routing`, `fw-matrix`, `fw-browser`.

## 3. What was verified, and what is blocked

- `python3 web/make_site.py build/felucca.fwsc 3.0.0 build/site_preview --no-release` builds the
  whole portal from those sources: portal + assets, installer (247 399 B), editor, and the package
  `firmware/felucca-3.0.0.fwsc` (610 959 B, FM-1_965, build v3.0.0 `db20e71`). The tool reports
  what is true of that build: **no emulator record, no manifest, INSTALL will refuse it**.
- Served on 127.0.0.1:3000: `/` → 200, 51 596 B; `<title>QUANTUM GAMMAWAVE — Firmware, emulador y
  Web Studio para FM-1</title>`; `/assets/banner.png` → 200; the new copy is in the served page.
  The preview was opened in the chat's Browser panel for the owner.

**Blocked, and not papered over:** publishing a 3.0.0 portal needs the emulator rebuilt from the
same commit (`sh web/emu/build.sh`), and **this environment has no `emcc`**, so there is no WASM
for 3.0.0 to serve. `tools/site_check.py` holds every artefact to the package's commit — an
emulator from the previous build next to a 3.0.0 `versions.json` is exactly the mismatch it
exists to catch. `release/` also still holds only the 2.9.7 artefacts. So `site/` (the published
root) was **not** overwritten: it remains the last coherent 2.9.7 deployment, while the new
presentation lives in `web/site/` and is ready for the release pipeline to build.

## 4. Not verified

- No browser pixel inspection of the portal and no mobile viewport test: only HTTP status, byte
  counts, the served title and the presence of the new copy were checked.
- The installer and Web Studio images (`installer.png`, `webstudio.png`) were **not** regenerated:
  they are screenshots of pages that were not rebuilt in this environment.
- The new banner was inspected by eye at 1600 × 520 (shown in this phase's report); nobody has
  seen it on a printed sheet or at banner-ad sizes.
