# Quantum GammaWave (QG-1) — Project Status & Audit Report

**Date:** 2026-11-02
**Target Hardware:** M-VAVE FM-1 Multitrack FM Synthesizer & Groovebox (Dual-Core JieLi AC791N SoC)
**Branch:** `feat/v1.5-hardening` (cut from `main` @ `4682fef`)
**Current Milestone Version:** `v1.6-QG`

### v1.6-QG — what was verified (2026-11-03)

| Check | Result |
|---|---|
| `tests/preset_library_test.py` | **VERIFIED** — 200 browsable sounds (ANALOG 56, PHASE 25, LOFI 25, FORMANT 20, WHEEL 18, DRUM 10, NOISE 17, FM6 29); DRUM 0..9 preserved and legacy kits still at 10/11 (byte-identical hashes); names unique per engine |
| `tests/regress.c` | **VERIFIED (host)** — 220 golden renders (200 library + 2 legacy DRUM + modes/mixes), 0 changed / 0 gone after the intended update, 0 health failures, 0 voice/routing failures |
| `tests/ui_render.c` | **VERIFIED (host)** — 171 screens x 17 palettes, 0 lint findings, 0 alignment errors (2,655,786 measurements), splash reads `v1.6-QG`; 197 of 425 timeline steps change the screen |
| Installer / Web Studio / backup / package bounds | **VERIFIED (host, simulated device)** — all four suites pass |
| ASan/UBSan: fuzz_ed 20,000 frames, editor_test | **VERIFIED (host)** |
| Browser emulator | **VERIFIED (wasm)** — rebuilt with Emscripten 6.0.13 (998,738 B) after fixing `web/emu/felucca_web.c`'s stale `lcd_init()` call; boots, renders the new splash, and matches the native build |
| 8 / 10 / 12 voices | **SIMULATED (host)** — 4,224 / 5,280 / 6,336 B for six tracks, 367 / 352 / 388 ns/sample; 12 stays off. **NO VERIFICADO on hardware** (`docs/POLYPHONY_1.6.md`) |
| Package signature / power-cut recovery | **NOT IMPLEMENTED / NO VERIFICADO** — design only, no bootloader or flash-critical change (`docs/SECURITY.md`) |



---

| Component | Status | Verification Method |
| :--- | :--- | :--- |
| **Firmware Core (C/Asm)** | Stable | 25+ hostsim tests passed (audio, scales, project, engines, voice management) |
| **DSP & Polyphony (v1.4-QG)** | Active | 10-voice psychoacoustic stealing (`quiet_tail`), Direct Form II Transposed Biquad EQ |
| **Code Generation Pipeline** | Stable | Verified with Pillow + fonttools; tables & UI headers generated in `build/gen/` |
| **Web Studio (`web/editor.html`)** | Stable | Verified via `node web/test_web.mjs` (all protocol tests pass, synchronized v1.6-QG) |
| **Web Installer (`web/index_pkg.html`)** | Stable | Verified via `node web/test_pkg_bounds.mjs` & `install_test.py` |
| **Backup System (`web/fm1backup.js`)** | Stable | Verified via `node web/test_backup.mjs` (43 object structures verified) |
| **Browser Emulator (`web/emu`)** | Active | Updated with resilient MIDI fallback & AudioWorklet diagnostics |
| **Windows Launchers (`*.bat`)** | Fixed | Updated to multi-Python detection (`python`, `py`, `python3`) |
| **Documentation Suite** | Complete | Audited, purged of obsolete commercial references, aligned with QG-1 identity |
| **Harmonic Distortions** | Verified | SOFT, TAPE, HEARTH, FLUX synced in firmware, emulator, Web Studio |
| **Themes (SOLARIS/CYBER/NORDIC)** | Verified | Strict WCAG compliance pass, thermal ramps, UI layouts integrated |
| **Preset Library (+42 Presets)** | Verified | 8 musical categories across engines, verified in web and firmware |
| **Transient PRESET Popup (1.5s)** | Verified | Non-blocking timer popup reusing SAVE browser view |
| **Dynamic ARP Visualization** | Verified | Real-time 16-step trajectory and playhead animation in GR_ARP |
| **GLO 2 Monitor Screen** | Simulated | Rendered and asserted by `ui_render` (S_MONITOR); the values it reads are host-simulated, **no FM-1 was connected** |
| **Full-Screen HOME Visualizer** | Simulated | Harmonic scope + Lissajous orbit toggle on HOME -> HOME, rendered by `ui_render` |

## 1. Executive Summary & Audit Conclusions

Following independent review of the repository after PR #6 fusion, all discrepancies between reported items and repository state were systematically diagnosed, verified with reproducible tests, and resolved:

### A. Emulator Version & GitHub Pages Deployment Audit
**Measured 2026-11-02, ~19:00 UTC, over HTTP — re-checkable with the URLs below.**

- **Published Pages URL:** `https://guillel51.github.io/Quantum-GammaWave/`
- **Version embedded:** `v1.5-QG (preview)` — the footer of the deployed `index.html` (`<small class="version">`). The page contains no `1.4-QG` string at all.
- **WASM binary deployed:** `felucca.wasm`, 1,004,467 bytes, SHA-256 `74c6819beaa663bb6b66b69a9c4ffa159faf158835aa272c55b626ec585bc71a`. A string scan of the binary returns exactly one version: `v1.5-QG` (no `v1.4-QG`, no `v1.3-QG`).
- **Root `index.html`:** 29,951 bytes, stable across four no-cache requests.
- **The deploy it came from:** workflow `emulator`, run [#115](https://github.com/Guillel51/Quantum-GammaWave/actions/runs/41503671469) on `main` at `4682fef`. `build` (emulator, smoke test, package, installer, editor, target compiler checks) succeeded; job `deploy` succeeded and `actions/deploy-pages@v4` finished at `2026-11-02T18:48:20Z`.
- **Installer's version list:** `firmware/versions.json` served by the site, newest first:
  1. `v1.5-QG-4682fef` — `felucca-v1.5-QG-4682fef.fwsc`, 610,113 bytes
  2. `v1.4-QG-9fb0549` — kept so a user can roll back
  3. `v1.3-QG-f5ea2fa` — kept so a user can roll back

**Why `1.4-QG` can still appear on screen, and why that is not a stale build:**

1. The installer's version dropdown deliberately lists the three builds above, so `v1.4-QG` and `v1.3-QG` are *selectable options*, not the current one. The default and newest entry is `v1.5-QG-4682fef`.
2. GitHub Pages edge caching: `Cache-Control: max-age=600` keeps a cached copy for 10 minutes after a deploy. A hard refresh clears it. (The first request after this deploy still served a 29,890-byte copy; four subsequent no-cache requests all returned the same 29,951-byte, `v1.5-QG` page.)
3. Browser service workers and local disk caches may hold the previous page.

**To re-verify:** `curl -s https://guillel51.github.io/Quantum-GammaWave/ | grep -o 'v1\.[0-9]-QG'` and `curl -s https://guillel51.github.io/Quantum-GammaWave/firmware/versions.json`.

### B. Master EQ Implementation & Signal Tests
- **Filter Architecture:** Parametric Biquad Filter in Direct Form II Transposed in fixed-point Q14 arithmetic (`qg_biquad_t`).
- **Modes Supported:** `OFF` (transparent bypass), `PEAK` (parametric bell with variable gain and Q), `NOTCH` (rejection notch with variable Q).
- **Firmware Parameters:** Mapped into global parameters `G_EQMODE` (70), `G_EQFREQ` (71), `G_EQGAIN` (72), `G_EQQ` (73).
- **UI Integration:** Accessible via new `MASTER EQ` page in `firmware/src/params.c` and hardware LCD graph in `firmware/src/ui_tech.c` (`tech_master_eq`).
- **Signal Test Verification:** Verified via standalone C test harness `tests/test_dsp_eq.c`:
  - Notch attenuation > 40 dB at center frequency.
  - Peak boost +5.9 dB with nominal +6 dB gain.
  - Bit-exact bypass transparency.
  - No numerical overflow or divergence under extreme impulses.

### C. Polyphony & Voice Stealing
- **Voice Capacity:** 10 real-time voices (`NVOICE 10`) across 6 tracks.
- **Smart Voice Stealing:** Implemented in `firmware/src/voice.c`. When voice capacity is saturated, the engine prioritizes dying release tails ($<-40\text{ dB}$, $\text{env\_out} < 120$ weighted by track volume) before stealing sustained notes, preventing clicks.
- **Verification:** Tested via `tests/test_polyphony.c` verifying 10 simultaneous sustained voices and protected release tails.

### D. Web Studio, Installer & Branding Cleanup
- **Distortions:** Pruned to 4 warm musical models (`TAPE`, `HEARTH`, `PLASMA`, `FLUX`) across `firmware/src/fx.c`, `firmware/src/params.c`, `web/editor.html`, and `web/index_pkg.html`. Obsolete types (`HARD`, `FOLD`, `CRUSH`, `RECT`) completely removed.
- **Branding & Credits:** Removed legacy commercial VST promotion banners while preserving all obligatory open-source credits (GPL-3.0, Felucca, JIANT, SLOOP, Melodee, Dexed).

---

## 2. Test Execution Log

| Test Suite | Command | Result | Scope |
| :--- | :--- | :--- | :--- |
| **Firmware C Syntax** | `gcc -fsyntax-only -I firmware/hal -I build/gen -I firmware/src firmware/src/felucca.c` | **PASSED** (0 errors) | Core firmware code |
| **DSP Master EQ** | `gcc -O2 tests/test_dsp_eq.c -lm -o tests/test_dsp_eq && ./tests/test_dsp_eq` | **PASSED** (100%) | Biquad Peak/Notch signal response |
| **Polyphony & Stealing** | `gcc -O2 tests/test_polyphony.c -o tests/test_polyphony && ./tests/test_polyphony` | **PASSED** (100%) | 10-voice budget & stealing |
| **Web Studio Suite** | `node web/test_web.mjs` | **PASSED** (100%) | Protocol, MIDI, editor, parameters |
| **Backup & Flash Restore** | `node web/test_backup.mjs` | **PASSED** (100%) | Flash backup/restore integrity |
| **USB Web Installer** | `python3 tests/install_test.py` | **PASSED** (100%) | OTA flasher & package security |
| **WASM Emulator Smoke** | `BENCH_MAX=2.0 node web/emu/emu_test.mjs /tmp/live_felucca.wasm` | **PASSED** (100%) | Live WASM boots, plays, renders |


---

## 3. Release v1.5-QG — Validation Results and Real Limits

**Branch:** `feat/v1.5-hardening` · **Suite:** `NO_PACKAGE=1 ./tests/run_tests.sh` · **Date:** 2026-11-02

### What actually ran

The runner here stops at 5 minutes, so the suite was executed end to end in
tramos cut from `tests/run_tests.sh` itself (same commands, same order), plus the
sections the first pass never reached.

| Check | Result |
| :--- | :--- |
| Whole host suite `NO_PACKAGE=1 ./tests/run_tests.sh` | **78 `==` sections, `ALL HOST TESTS PASSED`** — the only non-green line is `ui`'s own count |
| `tests/ui_test.c` | **8 FAILED** — was **12** at `main` (`4682fef`): four `#15 LARGE` layout checks fixed, **no new failure** (diffed name by name against a clean `main` build) |
| `tests/ui_render.c` | exit 0 — 171 screens x 17 palettes, **0 lint findings**, 0 inks off their colour, 0 alignment errors over 2,655,362 measurements, splash reads `v1.5-QG` |
| `tests/ui_render.c` splash timeline | 425 steps of 4 ms, **160 change the screen, 265 identical rewrites skipped (62 %)**; the last frame is byte-identical to a whole draw |
| `tests/regress.c` | exit 0 — **215 golden renders (204 + the 11 DRUM presets recorded here), 0 changed, 0 gone**, 0 health failures, 0 voice / routing failures, 0 CPU entries over budget |
| `tests/editor_test.c` | `editor test passed` — **110 checks, exit 0** (was 4 FAIL at `main`) |
| `tests/lcd_st7789_test.c` | 14/14 — the four black lines reproduced on the old path, closed by `lcd_gdram_clear()` |
| ASan + UBSan (5 stored-data suites + 2 fuzz runs) | 9 sections, no finding — `fuzz_ed` 20000 packets, `fuzz_proj` 5000 stores |
| `firmware/src/felucca.c` syntax + dead code | `cc -fsyntax-only` **rc 0**; with `-Wall -Wunused-function` **0 unused functions or variables** (23 warnings, all the expected 32-bit pointer casts) |
| `tests/fm6bank_test.c` | `all ok` |
| `node web/test_web.mjs` | `web tests passed` |
| `node web/test_pkg_bounds.mjs` | all checks passed |
| `python3 tests/install_test.py` | install tests passed |
| ASan + UBSan runs (part of the suite) | ran, no findings |

### Not run — environment, not "passed"

| Skipped | Why |
| :--- | :--- |
| Package / loader tests, `build/felucca.fwsc` | no JieLi toolchain (`NO_PACKAGE=1`) |
| `tests/target_budget.py` static budget | needs the `.fwsc` / disassembly it produces |
| Browser emulator (`web/emu`) | no `emcc` |
| CPU under `valgrind` / `callgrind` | not installed |
| Anything on a physical FM-1 | **no device was connected or flashed** |

### Fixed in this branch

- **GLO / CPU LOAD (point 1)**: `open_global()` did not clear `ui.home_viz`, so opening GLO left `home_viz=1` with `home=0` and `ui_draw()` rendered only `graph_home_viz()` — `draw_columns()`/`draw_graph()` never ran, MONITOR's CPU LOAD stayed frozen and the section buttons did nothing. `ui_draw()` now guards that state; `open_global()` clears it. Measured: `ui_test` 12 → **8 FAILED**.
- **Section buttons with the visualizer open (point 3)**: same root cause — GLO/FX/EDIT/SCL/LFO/ENV/SAVE now change section without HOME; 11 checks added (`qg_band`, MONITOR, LATE/SHED, header, SYSTEM card, the 7 buttons).
- **Startup (point 2)**: `lcd_init(T_BG)` + `lcd_gdram_clear()` write all 76800 pixels before `DISPON` / backlight, so the four black lines cannot survive a partial clear; the old windowed path is reproduced and asserted by `tests/lcd_st7789_test.c`. The splash was redesigned from scratch (octagonal frame, QUANTUM wordmark, spectrum rail, playhead, `v1.5-QG` + `QUANTUM GAMMAWAVE // QG-1`).
- **EXTRA unreachable from the editor**: `ED_MENU[]` stopped at `{MI_SELHOME, 21}`, so `MENU_DESC` answered `id=127` for indices 22..24, `MENU_SET` answered `rc 1`, and `INFO` advertised 25 settings with 22 real. CPU MODE / VIS QUALITY / SPLASH could not be read or written from the Web Studio. Appended as ids 22 / 23 / 24; the 4 failing `editor_test` checks are green.
- **Emulator build**: `web/emu/felucca_web.c` declared `lcd_init(void)` while `main.c` now passes the theme colour — `startup_test` did not compile.
- **Boot splash traffic**: `splash_sig()` skipped 265 of 425 timeline steps whose artwork was unchanged (~7.9 M fewer pixels per boot), with the last frame asserted byte-identical to a whole draw.
- **Regression coverage**: the 11 DRUM presets were never in `tests/golden.txt`; recorded and verified (0 changed).
- **EXTRA in firmware**: CPU MODE / VIS QUALITY / SPLASH in `menu_items.c`, `settings_persist.c` and Web Studio (page 4), ids 22 / 23 / 24, defaults AUTO / HIGH / FULL, no id or name repeated.

### Still failing (pre-existing on `main`, not introduced here)

**8 `ui_test` checks**, down from 12: COLOR KNOB palette stepping, HOME leaving display preferences, `#3 SELECT ON HOME`, `#39 FX/GLO/SCL/EDIT` layer taps, FX section taps, `MENU > STYLE`, `0.6.69 SELECT` section scrolling, and `#15 LARGE ON` cards. They fail identically on `main` (`4682fef`). Diffed name by name against a clean `main` build: **four `#15 LARGE` layout checks** are fixed here and **nothing new fails**.

### Polyphony: 10 vs 12 voices

Measured on the **host** build (x86-64, 5 runs of 40000 blocks, best of):

| | 10 voices | 12 voices |
| :--- | :--- | :--- |
| CPU / frame | 326.5 +- 7.8 ns | 364.3 +- 6.1 ns (**+11.6 %**) |
| SRAM (`track_t` x 6) | 2556 B | 2724 B (**+1008 B**) |
| Golden renders changed | - | **2** (`mode/ANALOG/UNISON`, `mode/VOICE/UNISON`) |
| Peak voices reached | 9 / 10 | 11 / 12 |
| Clipping / DC | 0 samples >= 30000, DC 3.9 | 0 samples >= 30000, DC 4.0 |

**12 voices is not adopted.** It costs +11.6 % CPU and +1008 B SRAM on a 96 KB target, changes two golden renders (i.e. the sound), and there is no JieLi toolchain or device here to prove the result on the target. 10 voices stays, which is the most stable configuration measured.

### Categories

- **VERIFICADO EN HARDWARE** — nothing in this release. No FM-1 was connected, powered, flashed or recovered.
- **SIMULADO** — everything in "What actually ran": the firmware's own sources built for the host, the menu/protocol, the golden renders, the splash timeline on the framebuffer, the ST7789's frame memory modelled byte for byte, the installer's package bounds and protocol, ASan/UBSan + fuzz, the 10 vs 12 measurement, and the FM-1 flash/recovery *analysis* in `docs/SECURITY.md`.
- **NO VERIFICADO** — anything on a physical FM-1 (no device was connected, powered or flashed); dual-core timing and per-core load on the real AC791N; the flash map, bootloader and recovery window of a physical unit; `m-bootloader` installability; the CPU render budget (`tests/target_budget.py` skipped: no `.fwsc`, no JieLi toolchain — and `regress` reports *no instruction counter on this host*, so the budget is not checked here either); power-cut behaviour on real flash; the browser emulator (no `emcc`); CPU under valgrind (not installed); the `.fwsc` package (no toolchain).
