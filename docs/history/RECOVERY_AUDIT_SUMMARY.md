# Recovery Audit — Quantum GammaWave v2.4.1

**Branch**: `recovery/v2.4.1-main`  
**Base**: `45dacd92affa82c41e559c14e4c8e9e2854183de` (main)  
**Status**: Audit complete, changes verified, tests passing

## Recovered from historical branches

- `audit/2.1.2-trazabilidad`: Voice pool audit (VPOOLN=NVOICE+NPART structure)
- `exp/voice-pool`: 3 commits on pool sound differences (voice reset, WHEEL seeding, chord order)
- `freebuff/4549bd389852a9607bba2622`: UI improvements, meter fixes, docs
- `freebuff/6ae847e53332b13a021ccec4`: AC791N build, WASM, cache barriers
- `feat/v1.7-qg`: v1.7 features (budget model, MIDI learn, stress passes)
- PR #85 (363011e): Merge with validation matrix docs

## Key verifications

- `NVOICE=12` maintained (not reduced to 10)
- `P_MSTART`/`P_NRATE` kept (required for test suite)
- All tests pass: `bash tests/run_tests.sh`
- Preset levels verified (27.6 dB spread, no degrading correction)
- 8 engines present and functional: ANALOG, FM6, PHASE, LOFI, FORMANT, WHEEL, NOISE, DRUM

## Still pending

- AC791N compilation (needs JIELI_TOOLCHAIN + AC79_SDK)
- Physical FM-1 hardware verification
- Full installer signature validation

## Not merged (conflict risk)

- `feature/fix-home-viz-and-drum-presets`: Net -21154 lines
- `perf/integral-audit`: Conflicts in multiple files