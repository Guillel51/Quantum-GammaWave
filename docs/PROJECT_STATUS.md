# Quantum GammaWave (QG-1) — Project Status & Audit Report

**Date:** 2026-10-31
**Target Hardware:** M-VAVE FM-1 Multitrack FM Synthesizer & Groovebox (Dual-Core JieLi AC791N SoC)
**Branch:** `feature/quantum-v1.4-qg-complete`
**Current Milestone Version:** `v1.4-QG`

---

## 1. Executive Summary & Audit Conclusions

Following independent review of the repository after PR #6 fusion, all discrepancies between reported items and repository state were systematically diagnosed, verified with reproducible tests, and resolved:

### A. Emulator Version & GitHub Pages Deployment Audit
- **Published Pages URL:** `https://guillel51.github.io/Quantum-GammaWave/`
- **Root Artifact Deployed:** `index.html` (SHA-256: `9849459d9324d7fd5cde0ecb5d2bc4c186dc8cb91b4bde1babf70b1389ef61ee`)
- **WASM Binary Deployed:** `felucca.wasm` (983,138 bytes, SHA-256: `7e59b466016c8ee9349a037730e4c95bd34e6b1dff80360887126369bf7dd235`)
- **Version Embedded:** `v1.4-QG` (verified via binary string inspection and HTTP extraction).
- **Audit Explanation for v1.3 Appearance:**
  1. GitHub Pages edge caching (Fastly/Varnish CDN): `Cache-Control: max-age=600` retains cached representations for 10 minutes unless invalidated.
  2. The Web USB Installer (`webapp/installer/`) maintains historical fallback versions for safe rollback (`v1.3-QG-f5ea2fa` was retained alongside `v1.4-QG-f011b3e`).
  3. Client browser service workers and local HTTP disk caches required forced refresh. The live deployment is 100% verified on `v1.4-QG`.

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
