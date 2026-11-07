# VALIDATION MATRIX — what was actually run (v1.9.0, the integrated tree)

Honesty first: this table records the checks executed **in this session**, on this machine, against
this tree. Anything not listed here was not run. Labels: `[HOST]` executed natively, `[TARGET]`
executed against a real AC791N build, `[WASM]` executed in Node against the Emscripten build,
`[STATIC]` inspected without execution, `[HW]` needs an FM-1.

Environment: Linux x86-64, gcc 13 (`cc`), Python 3 (Pillow/fontTools/numpy), Node 20,
**JieLi pi32v2 toolchain `jieli-linux-toolchains-20260730.1`**, **AC79 SDK
`AC79NN_SDK_V1.2.1_2023-12-13`** (three files match `SDK_SHA256`), **Emscripten 6.0.11** (the emulator workflow installs the same version).
No FM-1, no MIDI hardware, no browser automation.

## 1. Integration

| Check | Label | Result |
|-------|-------|--------|
| `42c35cc` / `integ/v1.8.9` and `b6d53bf` share the base `fa94ec5` (no shared history past it) | `[STATIC]` | confirmed with `git merge-base` |
| Merge on `integ/v1.9-security` | `[STATIC]` | **pass**, commit `61a3198` |
| Conflicts (5 files) resolved by hand, not by picking a side | `[STATIC]` | `project.c` (one guard, both comments), `felucca_web.c` (one status-register stub, guarded), `ldr_test.c` (fault injection **and** derived cut points), `CHANGELOG.md`, `felucca.c` (version) |
| Neither line's feature dropped | `[HOST]` | EQ tests, raw restore, signing, preflight, six-track load, flash identification, OTA bounds, core-1 self-test, verified backup all pass below |
| `main` untouched, no force-push, no branch deleted | `[STATIC]` | `origin/main` = `fa94ec5`, unchanged |

## 2. Target build (AC791N)

| Check | Command | Label | Result |
|-------|---------|-------|--------|
| App + loader + package | `./build.sh` | `[TARGET]` | **pass**: `felucca.bin` 559 536 B, `ota.bin` 8 007 B, `felucca.fwsc` 611 163 B, identity `FM-1_982` |
| Budgets printed by the build | built-in `check()` | `[TARGET]` | **pass**: `.data+.bss` 96 452/98 304, RAM code 22 500/24 576, pool spare 9 248, calls reach 63 456 B |
| Regions do not overlap | `python3 tests/memory_map_test.py` | `[TARGET]` | **pass** |
| Render-loop cost | `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | `[TARGET]` | **pass** |
| Storage-less configuration | `FELUCCA_FLASH=0 FELUCCA_OTA=0 ./build.sh` | `[TARGET]` | **pass** (built and measured; the manifest and CI job cover it) |

## 3. Build identity and provenance

| Check | Command | Label | Result |
|-------|---------|-------|--------|
| Manifest written after the artefacts, with commit/version/config/toolchain/SDK/hashes | `./build.sh` → `build/manifest.json` | `[TARGET]` | **pass** |
| It names the code that is here | `python3 tools/build_manifest.py --verify` | `[HOST]` | **pass** (`v1.9.0`, `61a3198c065c`, 3 artefacts hashed, emulator agrees) |
| The verifier refuses a changed artefact, another commit, a dirty tree, a wrong schema, a wrong version, a missing manifest, a size mismatch | `python3 tests/manifest_test.py` | `[HOST]` | **pass** (12 checks) |
| The stale-emulator defect is gone | `build/emu/manifest.json` | `[STATIC]` | `v1.9.0` / `61a3198c065c` / wasm sha256 — it used to say `dcd39f9` while the tree was elsewhere, which `--verify` now fails |

## 4. Host suite

`tests/run_tests.sh` was executed. It does not fit in the runner's five-minute window, so it ran as
the unmodified script up to the sanitizer section and the remaining groups were executed directly with
the same compiler flags (`cc -O1 -Wall -Wno-unused-function`, `-Ibuild/gen -Ifirmware/src`, `-lm`).
Every check that ran passed.

| Group | Label | Result |
|-------|-------|--------|
| flash storage, user presets, keys/buttons, TRS MIDI, USB descriptors (12 combinations) | `[HOST]` | pass |
| flash identification (allowlist, two agreeing reads, capacity, protection) | `[HOST]` | pass |
| second-core self-test | `[HOST]` | pass |
| USB audio (44.1/48 kHz, ring, resampler) | `[HOST]` | pass |
| M-UPGRADE entry and the update loader (`ota_test`, `ldr_test`), five derived power cuts + resumes | `[HOST]` | **pass** |
| scales, SPEAKER EQ, DSP render, TRACKS, project formats, motion, RATCH, MIDI control, DIGITAL/FM4, themes, text, settings, UI, UI renders, audio, metronome | `[HOST]` | pass |
| **EQ (newly wired)**: `eq_dc_test`, `eq_response_test`, `eq_state_test`, `eq_sweep_test` | `[HOST]` | **pass** |
| **six-track load (newly wired)**: `six_track_test` | `[HOST]` | **pass** |
| **host security tools (newly wired)**: `chip_test.py`, `restore_test.py`, `sign_test.py`, `preflight_test.py` | `[HOST]` | **pass** |
| `persistence_test`, `backup_test`, `fm6bank_test`, `startup_test`, `lcd_st7789_test`, `editor_test`, `robust_test` | `[HOST]` | pass |
| regression: golden renders, health, voices, CPU baseline | `[HOST]` | **pass**, baselines unchanged by the merge |
| installer CLI, backup CLI, README navigation | `[HOST]` | pass |
| Web Studio / protocol / package bounds | `[HOST]` (Node) | pass |

## 5. Sanitizers and fuzzing (the previous pass's pending work)

| Check | Label | Result |
|-------|-------|--------|
| ASan + UBSan: `ldr_test` (other app → this build), `ota_test` (M-UPGRADE entry) | `[HOST]` | **pass** |
| ASan + UBSan: `editor_test`, `project_test`, `backup_test`, `robust_test`, `fm6bank_test` | `[HOST]` | **pass** |
| ASan + UBSan fuzz: editor SysEx + raw USB-MIDI packets, 20 000 frames, seed 7 | `[HOST]` | **pass** (18 777 frames, 1 223 raw packets) |
| ASan + UBSan fuzz: project stores → import → restore → render, 5 000 iterations, seed 13 | `[HOST]` | **pass** (486 imported) |

## 6. Browser emulator (WebAssembly)

| Check | Label | Result |
|-------|-------|--------|
| WASM rebuild at `v1.9.0` | `[WASM]` | **pass**: 1 067 931 B, sha256 in `build/emu/manifest.json` |
| Manifest carries version, commit, config, `emcc` version and the dirty list | `[STATIC]` | pass |
| Native reference render, parity bit for bit, cost | `[WASM]` | **pass** (from the v1.8.8 pass, re-built here; `emu_test.mjs` runs in CI) |
| Real browser (DOM, AudioWorklet output, console, network) | `[BROWSER]` | **not run**: no browser automation in this workspace |

## 7. Not run, and why

| Check | Why | Status |
|-------|-----|--------|
| Anything on an FM-1 (boot, audio, MIDI, LEDs, flash timing) | no hardware | `[HW]` **not verified** |
| A real power cut during an update | needs hardware | **not verified** |
| Physical raw restore (`fm1_restore.py --write` with `flashrom`) | needs hardware; the tool refuses without a programmer and a verified identity | **not verified** |
| A real browser session | no browser automation here | **not verified** |
| Signature custody | `tools/fm1_keys.json` has no production key, so every package is refused (the correct state) | **by design** |
| Anti-rollback, secure boot | no bootloader change possible, no key material | **not implemented** |

## 8. Verdict

**NO-GO for the main unit.** A real target build exists, it is tied to its commit by a verifier that
CI runs, and every software check that can run here passes — but nothing has run on an FM-1.
**GO for laboratory validation on a secondary unit**, with the caveats in
[`SECURITY_AUDIT.md`](SECURITY_AUDIT.md): no signature custody yet, no anti-rollback, single-bank
update window, and no raw flash backup path verified on hardware.

## 9. Update — 2026-11-07, host-only pass at `66b8743`

This section records only what was executed in that session, on this machine. Where it contradicts a
claim above, this section is later and the claim is to be treated as historical.

| Check | Label | Result |
|-------|-------|--------|
| Host suite, whole script | `[HOST]` | **pass**: `NO_PACKAGE=1 tests/run_tests.sh` → `ALL HOST TESTS PASSED`, exit 0, 4 984 `ok` lines |
| install, backup CLI, web, provenance, M-Flasher, editor, project format, robustness | `[HOST]` | pass inside that run |
| Verify Only | `[HOST]` | **101 checks, 0 FAILED** |
| Signing / key custody / gate / chip / flash evidence | `[HOST]` | `sign_test.py` (39), `key_custody_test.py` (55), `gate_test.py` (14), `chip_test.py` (38), `flash_evidence_test.py` (16) all pass |
| Render sweep | `[HOST]` | 173 screens × 17 palettes, **0 lint findings**, 2 643 176 alignment measurements, **0 off by 1 px** |
| Render determinism | `[HOST]` | two runs of the same binary: **0 differing files** (the earlier "226 frames differ" was `git describe` changing `splash`/`about`/`about_rec`, not a layout change) |
| Regression found and fixed | `[HOST]` | `card_edge` drew the card's side lines, the rolling value strip cleared them and did not repaint them: **5 `ui_test` checks failed**, the 150 000-frame soak counted **84 stale cards**. Fixed in `66b8743`; the whole suite passes again |
| AC791N target build | `[TARGET]` | **not run today**: `JIELI_TOOLCHAIN` and `AC79_SDK` are unset in this sandbox and `build/` holds no `.fwsc`; `tools/build_manifest.py --verify` refuses (`no manifest`) |
| Emulator rebuild | `[WASM]` | **not run today**: no `emcc`; `web/emu` has no built manifest in this tree |
| Preflight | `[HOST]` | **not run**: refuses without `build/felucca.fwsc` (correct behaviour, not a failure) |
| Production signature | `[STATIC]` | `tools/fm1_keys.json` still has `keys: {}` — no pinned public key, so every package is refused. Unchanged, and not faked |
| Installer gate | `[STATIC]` + `[HOST]` | `canInstall` is `false` in every answer of `web/fm1verify.js` and the tests assert it (`test_verify_only.mjs`) |

Rows 1–3, 7 (map for a specific unit), 8, 9, 12 (target RAM/CPU) and 13–14 of the 14-requirement matrix
stay **NO-GO**: each needs a target build or a reading from the owner's unit, and neither exists in this
environment. Row 11 (stress and stability) is **GO** on the host evidence above. Row 10 (protected write
paths) stays **GO CONDITIONAL** on the same evidence as before.

## 10. Update — 2026-11-07, target build attempted here (toolchain installed)

The JieLi toolchain was installed in this sandbox from the pinned archive and verified:

| Step | Result |
|---|---|
| `tools/get_toolchain.sh` | **pass**: sha256 `a1545cdd…7647bd` matches `tools/toolchain.sha256`, `jieli-linux-toolchains-20260730.1`, `pi32v2/bin/clang` reports **clang 4.0.1** |
| AC79 SDK | present at `~/fw-AC79_AIoT_SDK` (`cpu/wl82/tools/uboot.boot` found) but **not** the pinned `AC79NN_SDK_V1.2.1`: the build warns `SDK uboot.boot differs from AC79NN_SDK_V1.2.1` and `SDK cfg/eq_cfg_hw.bin differs …; the package will not match the reference` |
| `./build.sh` (`ulimit -n 9000`) | **compiles and links**: image 565 436 B, `.data+.bss` 96 056 / 98 304 B, pool spare 9 248 B, XIP code 402 420 B, RAM code 22 844 / 24 576 B, `.ram_text` 1 193 insns with no calls |
| the build's own gate | **FAIL `RAM code headroom 1732 B < 2048 B`** → `build: checks failed`, **no `.fwsc`, no manifest** |
| same build at `7871c14` (before any of this session's UI work) | **identical**: `RAM code 22844 B`, same `FAIL` — the headroom failure is pre-existing on `main`, not a regression from the visual work |

What that means, plainly: the target toolchain works here and the firmware links, but the repository refuses to
package it because 316 B of RAM code would have to be freed before the 2 048 B headroom rule is met. That is a
bounded code change (a hot function moved out of `.hot_text`, or made smaller), and it must be measured against
`tests/target_budget.py` and the audio golden renders — it is **not** a budget to relax, because `.ram_text` is
what runs with the flash off.

Even after that, a package built with the SDK present here would **not** be byte-reproducible against the
reference: the SDK is not `AC79NN_SDK_V1.2.1`. Signing and releasing a package built against an unpinned SDK
would make the signature cover an artefact that the project cannot rebuild, which is exactly what §5 forbids.
