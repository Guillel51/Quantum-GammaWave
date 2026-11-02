# Quantum GammaWave (QG-1) — Project Status & Audit Report

**Date:** 2026-11-02
**Target Hardware:** M-VAVE FM-1 Multitrack FM Synthesizer & Groovebox (Dual-Core JieLi AC791N SoC)
**Branch:** `release/v1.5-qg`
**Current Milestone Version:** `v1.5-QG`

---

| Component | Status | Verification Method |
| :--- | :--- | :--- |
| **Firmware Core (C/Asm)** | Stable | 25+ hostsim tests passed (audio, scales, project, engines, voice management) |
| **DSP & Polyphony (v1.4-QG)** | Active | 10-voice psychoacoustic stealing (`quiet_tail`), Direct Form II Transposed Biquad EQ |
| **Code Generation Pipeline** | Stable | Verified with Pillow + fonttools; tables & UI headers generated in `build/gen/` |
| **Web Studio (`web/editor.html`)** | Stable | Verified via `node web/test_web.mjs` (all protocol tests pass, synchronized v1.5-QG) |
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

**Branch:** `release/v1.5-qg` · **Suite:** `NO_PACKAGE=1 ./tests/run_tests.sh` · **Date:** 2026-11-02

### What actually ran

| Check | Result |
| :--- | :--- |
| Full host suite `NO_PACKAGE=1 ./tests/run_tests.sh` | 4204 assertions `ok`, 18 suites `test passed`; the **only** failing suite is `ui` |
| `tests/ui_test.c` | **12 FAILED** — a strict subset of the 13 that fail on `main`. One of them (the GLO cycle) is fixed here. |
| `tests/ui_render.c` | exit 0 — 171 screens x 17 palettes, **0 lint findings**, 0 inks off their colour, 0 alignment errors over 2,655,362 measurements, splash reads `v1.5-QG` |
| `tests/regress.c` | exit 0 — **204 golden renders, 0 changed**, 0 health failures, 0 voice / routing failures, 0 CPU entries over budget |
| `tests/editor_test.c` | `editor test passed` (110 checks) |
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

- **Startup**: `lcd_gdram_clear()` writes all 76800 pixels before `DISPON` / backlight, so the four black lines cannot survive a partial clear.
- **EXTRA**: CPU MODE / VIS QUALITY / SPLASH in firmware (`menu_items.c`, `settings_persist.c`) and Web Studio (page 4), ids 22 / 23 / 24, defaults AUTO / HIGH / FULL, no id or name repeated.
- **`tests/editor_test.c` `stack-buffer-overflow`**: `joined[96]` overflowed on COLOR (17 names = 204 bytes) and aborted the run under `_FORTIFY_SOURCE`, which surfaced as the *previous* check dying. Also fixed the stale EXTRA expectations and exported all 25 rows of `menu.json`.
- **`tests/fm6bank_test.c`**: the preset-count guard said 94; it is 102 (all 102 pass their per-preset checks, none repeats a name).
- **GLO cycle**: `MONITOR` made it four taps, the test expected three.
- **Dead code**: 6 `static` functions with no call anywhere (`cp_arp`, `cv_blit_from`, `eng_list_step`, `fg_on`, `leds_step`, `motion_snapshot_track`), 53 lines.

### Still failing (pre-existing on `main`, not introduced here)

The 12 `ui_test` failures above: display-palette stepping, HOME / SELECT-on-HOME, layer tap handling, FX section taps, `MENU > STYLE`, section scrolling, and the five `#15 LARGE` layout checks. They fail identically on `main` (`c37f511`).

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
- **SIMULADO** — everything in "What actually ran": the firmware's own sources built for the host, the menu/protocol, the golden renders, the CPU budget, the 10 vs 12 measurement, the installer's package bounds and protocol, and the FM-1 flash/recovery *analysis* in `docs/SECURITY.md`.
- **NO VERIFICADO** — dual-core timing and per-core load on the real AC791N; the flash map, bootloader and recovery window of a physical unit; `m-bootloader` installability; 12 voices on target; power-cut behaviour on real flash; the browser emulator (no `emcc`); the `.fwsc` package (no toolchain).
