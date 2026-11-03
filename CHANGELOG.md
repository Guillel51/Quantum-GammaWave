# Changelog — QUANTUM GAMMAWAVE (QG-1)

All notable changes to the firmware, the Web Studio, the emulator and the installer pages.
The device is an M-VAVE FM-1 (AC791N); no entry here was tested by flashing hardware.

## v1.7-QG — Splash identity

- The splash monogram was a square `Q` (a box with a diagonal) that read as a placeholder. It is now a round
  Q ring with its tail, drawn with the same orbit/knob language as the rest of QG, in the same palette and
  mono rule. `ui_render` still reports 0 lint findings and 0 alignment errors over 171 screens x 17 palettes.

## v1.7-QG — CI budget fix (target compiler checks)

- `mod_begin` and `mod_voice` are back inside their target budgets, measured with the JieLi compiler:
  `mod_begin` 274/166 (+65 %) -> **35/166**, `mod_voice` 36/30 (+20 %) -> **19/30**. The per-slot work is
  out of the loop (`mod_block_slot`, `mod_scan`), `mod_tsrc` / `mod_voice_add` are no longer inlined there,
  and the per-voice sources are resolved once. No budget line was changed; `mod_test` passes and all 220
  golden renders are byte-identical.

- `mod_voice` resolves the per-voice sources (ENV, VEL, KEY, RAND, PAT) once, before the slot loop, and
  indexes them there. 1.6's PAT added a source test to that loop, whose weighted cost had a 30 budget;
  the loop now does one load per slot instead of re-testing the source, with identical renders (220 goldens,
  0 changed).
- `tests/mod_test.c` still asserted `MS_N == 15`; PAT was appended in 1.6 (15, `MS_N` 16). The test now
  checks both NOISE 14 and PAT 15, and the matrix suite passes again on `main`.

## v1.7-QG — Splash dust

- The boot splash gains deterministic particles behind the three written waves, in the saved palette and mono
  rule, with no extra frames of drawing (197 changing frames of 425). `ui_render` now derives the timeline
  length from `SPL_WRITE + SPL_HOLD` instead of a hardcoded 425.

## v1.7-QG — UI Evolution & Hardware Validation (2026-11-03)

- UI audit kept at 0 lint findings and 0 alignment errors across 171 screens x 17 palettes.
- `tools/budget_report.py`: static RAM for 8/10/12 voices, library composition and host CPU baselines, with
  the host/target distinction built into the output.
- `docs/HARDWARE_SIMULATION.md`: what the host stubs and the wasm shim model, and what they do not.
- `docs/FEATURES_1.7.md`: every requested feature classified as already present, deferred with a reason, or
  blocked by the frozen project format. No persistent parameter was added.
- **MIDI Learn (session only):** CC 119 arms the parameter id of the track's engine, the next CC binds to it,
  CC 118 clears every binding. RAM only, so projects, user presets and settings keep their layout; the MIDI
  control test covers the range, the clear and an out-of-range arm.
- `tests/stress_run.sh`: project, persistence, robustness and MIDI paths under ASan/UBSan for N runs, then
  two full golden passes, all against the simulated flash.
- `tools/hw_model.py`: block deadlines, half-buffer latency, DMA and LCD bandwidth, SRAM share per voice
  count and the cycle ceiling at 160/240 MHz — arithmetic from documented constants, never a measurement.
- **UBSan findings fixed:** ANALOG shifted negative samples left twice (`y << 1`, `voice_amp(...) << 1`);
  both are now `* 2`, the same DSP with defined behaviour (goldens unchanged). The FUN5 persistence test
  wrote 27+ globals into a 27-entry header; it now stops at the old format's size.
- Version 1.7-QG across firmware, Web Studio, installer strings and documentation.

## v1.6-QG — EVOLVE / MOTION, Touch & 200 Sounds (2026-11-03)

### Library — exactly 200 factory sounds
- Retired from browsing the two least distinct DRUM kits, **HEARTH KIT** and **CYBER DRUM**. They stay in
  the table as legacy entries at their original indices 10 and 11, so a project, favorite or user preset
  made before 1.6 loads the exact kit it saved (their golden hashes are byte-identical to v1.5). DRUM's
  ten browsable sounds keep indices 0..9 and the library is exactly 200 (`core.h engine_library`).
- Added 16 original sounds (not variations): QUANTUM BLOOM, ABYSSAL DRONE, PRISM KEYS, GLASS BELL,
  AURORA PLUCK, OCEANIC PAD, EMBER LEAD (ANALOG); EVOLVING GLASS (PHASE); TAPE DRONE (LOFI);
  CATHEDRAL REED, TIDAL ORGAN (WHEEL); BREATH CHOIR (FORMANT); SOLAR WIND (NOISE);
  EVOLVE BELL, VELVET EP, AURORA FM (FM6).
- Per engine: ANALOG 56, PHASE 25, LOFI 25, FORMANT 20, WHEEL 18, DRUM 10, NOISE 17, FM6 29 = 200.
- `tests/preset_library_test.py` (run by `tests/run_tests.sh`) checks the total, each engine's share,
  DRUM's ten preserved indices, the retired names and every entry's ranges.

### EVOLVE / MOTION
- New LFO wave **EVO** (`P_LWAVE = 6`, appended to `N_LWAVE`): a smooth random walk with one new point per
  cycle, eased with an integer smoothstep. Reuses RATE, PHASE, FADE, the LFO destinations and the matrix.
- Negative RATE is already the musical division on the transport's grid (4BAR..1/32); positive RATE is the
  free rate. Both are used by the demo sounds. No new parameter, no format change.
- Demo sounds: QUANTUM BLOOM, ABYSSAL DRONE, OCEANIC PAD, EVOLVING GLASS, TAPE DRONE, SOLAR WIND,
  CATHEDRAL REED, TIDAL ORGAN, EVOLVE BELL, AURORA FM.

### Touch — velocity and aftertouch, honestly
- Channel aftertouch (MIDI `0xD0`) continues to feed `MS_AT`, per track.
- New per-voice source **PAT** (`MS_PAT`, appended): polyphonic aftertouch (MIDI `0xA0`) from an external
  controller, routed to the matching held key. The FM-1's 27 key switches have no pressure sensor, so the
  device's own keys never fake per-key pressure.
- Sensitivity and curve: the matrix AMT per destination (CUT, SHP, AMP, PITCH, VIB, E1..E8, CHO, DLY, REV,
  RATE); engines keep their own velocity response. Adding stored curve/sensitivity parameters would have
  grown `P_COUNT` and broken the frozen project layout, so it was deliberately not done.
- Presets demonstrating the routes: PRISM KEYS (VEL→CUT, AT→CUT), GLASS BELL (VEL→CUT, PAT→VIB),
  AURORA PLUCK (VEL→CUT, AT→REV), BREATH CHOIR (AT→E1 vowel), VELVET EP (VEL→CUT, AT→VIB),
  EMBER LEAD (VEL→CUT, AT→VIB).

### UI
- HOME and EDIT WAVE 1 rotary knobs redrawn as real round knobs: filled body, 270-degree ring, value arc,
  needle and centre dot; the offset orbit ring and the bakelite artefact are gone.
- Boot splash redrawn as one SURF card in the UI's language: theme rule, QG monogram with a dotted orbit,
  three waves written across it, the 16-segment rail and its playhead. The old octagonal frame's dark bands
  (read as four black lines) are gone. It is still shapes only, in the saved palette, and still written by
  the same 4 ms timeline.
- No change to the device bootloader, flash layout, or recovery code.

### Verification (see `docs/PROJECT_STATUS.md`, `docs/POLYPHONY_1.6.md`, `docs/TOUCH_1.6.md`)
- **VERIFIED on the host:** 200-sound library test plus the two legacy DRUM indices; UI lint/alignment
  (0 findings) and PNG renders; 220 golden renders and the health suite; installer, Web Studio and backup tests; sanitizer/fuzz runs when
  the local compiler supports them.
- **SIMULATED:** the emulator page and its installer strings, from the same firmware sources.
- **NOT VERIFIED:** anything on a physical FM-1; a 12-voice build on hardware; a rebuilt `felucca.wasm`
  when Emscripten is unavailable in the build environment; package signing (design only, not enabled).
