# AI Agent Handoff Document — Quantum GammaWave (QG-1)

## Overview & Architecture
Quantum GammaWave (QG-1) is open-source custom firmware and web workstation software for the **M-VAVE FM-1 multitrack groovebox**.
The physical device uses a dual-core **JieLi AC791N** processor (Pi32 DSP core architecture) running at 44.1 kHz 16-bit audio.

### Dual-Core Partitioning
- **Core 0:** Sequencing, MIDI IO, track synthesis (tracks 1..3), master FX bus, clipping, leveler, parametric EQ, and 240x240 LCD rendering.
- **Core 1:** Tracks 4..6 synthesis (DRUM-X engine, additional poly voices) rendered in parallel and mixed in `c1_mix`.

## Key Changes Introduced in Milestone v1.4-QG
1. **10-Voice Polyphony (`core.h`, `voice.c`):**
   - Expanded `NVOICE` to 10.
   - Smart psychoacoustic voice stealing evaluates energy metric $E = \text{env\_out} \times (\text{level} + 1)$ targeting inaudible release tails ($<-40\text{ dB}$, $\text{env\_out} < 120$).
2. **Master Parametric EQ (`fx.c`, `params.c`, `core.h`, `ui_tech.c`):**
   - Direct Form II Transposed Q14 Biquad Filter.
   - Global parameters `G_EQMODE` (70), `G_EQFREQ` (71), `G_EQGAIN` (72), `G_EQQ` (73).
   - Dedicated hardware LCD graph `tech_master_eq()` displaying real-time frequency curves.
3. **Harmonic Distortion Rationalization (`fx.c`, `params.c`, `web/index_pkg.html`, `web/editor.html`):**
   - 4 warm musical models: `DT_TAPE` (cubic soft saturation), `DT_HEARTH` (tube warmth + sub weight), `DT_PLASMA` (AM saturation), `DT_FLUX` (analog tape drift).
4. **Branding & Web Studio Cleanup:**
   - Removed legacy third-party VST commercial links.
   - All tests (`tests/test_dsp_eq.c`, `tests/test_polyphony.c`, `test_web.mjs`, `test_backup.mjs`, `install_test.py`) verified 100% passing.

## Verification Instructions for Next Agent
- To run host web tests: `node web/test_web.mjs && node web/test_backup.mjs`
- To run Python installer tests: `python3 tests/install_test.py`
- To run DSP signal checks: `gcc -O2 tests/test_dsp_eq.c -lm -o tests/test_dsp_eq && ./tests/test_dsp_eq`
- To run polyphony checks: `gcc -O2 tests/test_polyphony.c -o tests/test_polyphony && ./tests/test_polyphony`
- To check C syntax: `gcc -fsyntax-only -I firmware/hal -I build/gen -I firmware/src firmware/src/felucca.c`
