# Quantum GammaWave v3.1 — validation, synchronisation and release

Date: 2026-11-14. Version `v3.1` (`FELUCCA_VERSION`), package `release/felucca-v3.1.fwsc`,
identity `FM-1_965`, build commit `487735744fb5`, emulator WASM `ff4f0ce1…` (1 207 502 B).

## What changed

- `FELUCCA_VERSION` **v3.0.0 → v3.1**. Nothing else in the firmware changed in this step: the DSP,
  the UI and the preset bank are the ones audited in phases 3–5 (275 presets, 0 silent, 0 hot).
- **The emulator exists again.** Emscripten **6.0.11** — the exact version `web/emu/build.sh` pins —
  was installed in this environment, so `sh web/emu/build.sh` compiled `web/emu/felucca_web.c` from
  this tree. Until now the site could only serve an emulator from an older commit.
- **Release artefacts**: `release/felucca-v3.1.fwsc` (610 959 B, SHA-256 `b6b0088a…`),
  `release/felucca-v3.1.fwsc.manifest.json` (integrity only: SHA-256 + size + build record,
  `keyid` empty — v3.1 ships unsigned by design), `release/build-manifest-v3.1.json`,
  `release/emu/` refreshed, `release/SHA256SUMS` extended to 103 entries.
- **The portal was rebuilt from those bytes** (`web/make_site.py … 3.1 site`): `site/firmware/versions.json`
  is 3.1 with the package's own hash and size, the emulator record names the WASM and the commit,
  and the installer and the editor are regenerated in the same run.

## Measurements

| what | value | class of measurement |
|---|---|---|
| app image | 574 988 B of a 583 612 B slot (8 624 B free) | AC791N build |
| RAM code | 19 584 B of 24 576 | AC791N linker report |
| package | 610 959 B, FM-1_965 | package built by `./build.sh` |
| `target_budget.py` | every loop within budget (exit 0) | instruction count on the linked ELF |
| WASM | 1 207 502 B, `ff4f0ce1…`, 6.0.11 | Emscripten build |
| emulator speed | 1 s of the heavy song in 26.8 ms — 2.7 % of real time | host Node, not a browser |
| presets | 275 in the engine, 273 with a category | `preset_audit`, `preset_categories_test` |
| CPU/audio time | not re-measured on a device | — |

## Tests run, and what each one proves

- **Full host suite** (`NO_PACKAGE=1 ./tests/run_tests.sh`): native runners, golden regression
  (`regress tests/golden.txt tests/cpu_baseline.txt` — no baseline touched), UI renders for every
  screen and palette with the layout lint, GREY and MONO checks, FM4 sweep, alignment by ink,
  ASan/UBSan builds and the two seeded fuzz runs, the editor/project/backup paths. **Host x86 only.**
- `python3 tests/install_test.py`, `node web/test_web.mjs`, `node web/test_backup.mjs`,
  `node web/test_pkg_bounds.mjs` — all pass (installer, package parsing, backup, JEDEC gates).
- `node web/emu/emu_test.mjs` — the emulator runs the firmware, restores kept sectors, plays the
  heavy song deterministically. **Node, not a browser.**
- `python3 tools/coherence_check.py` — **ok**: v3.1, package and manifest agree, the site serves that
  package, the emulator record matches the build, SHA256SUMS verifies, 958 files carry no merge marker.
- `python3 tools/site_check.py --url http://127.0.0.1:3000` — **all checks passed** against the built site.
- `python3 web/test_site.py site` — 20 ids, 10 images, 31 links, 0 failed.
- **Browser (Cloud Chrome, real page)**: `/` renders the 3.1 hero, `versions.json` reads 3.1 with
  SHA-256 `b6b0088a…` and 610 959 B, `emulator/manifest.json` reads v3.1 / `487735744fb5`;
  `/emulator/` loads a 1 207 502 B WASM, canvas 240 × 240, page prints `build 487735744fb5`.
  The audio output was not heard (no audio device in the VM).

## Reproducibility

The package was built twice: once from a tree with `firmware/src/felucca.c` modified (record says
`dirty: [firmware/src/felucca.c]`) and once from the committed tree. Both are 610 959 B with SHA-256
`b6b0088a7d225c9e06d674f2328195c37b09368f8aa46bd6808236f9b86fb850`; the WASM is byte-identical too
(`ff4f0ce1…`). Only the commit field of the record differs.

## Publication

`tag-and-publish.yml` runs on a push to `main`, verifies `release/felucca-<V>.fwsc` against its
manifest and only then creates the annotated tag **and** the release with the package and
`SHA256SUMS`. The previous `v3.0.1` release was empty because it was created outside that path;
the workflow now leaves existing releases' assets alone, so an empty older release stays empty and
v3.1 has to be created by the workflow, not by hand.

## NOT verified

- Nothing was flashed, installed, read or written on a physical FM-1. **HARDWARE: not verified.**
- No audio was heard from the emulator and no MIDI device was exercised: the MIDI paths are proven
  by the host tests and the emulator harness, not by a playing instrument.
- The Web Studio editor page was loaded through the site check, not driven by hand.
- The installer was not driven against a real USB device.
