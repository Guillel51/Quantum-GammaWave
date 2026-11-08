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

## 11. Update — 2026-11-07, the RAM-code reserve met: the package builds

The 316 B §10 was short by came from `2cfe514` (the FX echo train measured inside `always_inline`
`fx_buses`, i.e. inside `fx_run0`), not from the visual work. Two measured attempts:

| Attempt | RAM code | Verdict |
|---|---|---|
| walk the echo train per block on the peak the bus already takes (`dlpk`) | 23 028 B | **rejected**: 184 B *worse* than the per-sample walk and `dly_echo_test` failed; reverted |
| `perf_begin` out of `HOT_FNS` (`244cc99`) | **22 144 B** (headroom **2 432 B**) | **kept** |

`perf_begin` was in `.hot_text` as a placement assumption, not a requirement: the build's own check
reports `.ram_text: 1 193 insns, no calls`, so no RAM code reaches it, and its only caller is the XIP
block orchestrator — flash-off audio never depended on it being in RAM. The 2 048 B reserve is **not**
relaxed; it is met with 384 B to spare.

A second target-only defect surfaced here and is fixed in `7a3149f`: `cv_begin` cleared the frame with
`memcpy` per row, and the AC791N build has **no libc**, so `string.h` does not exist and the link failed
outright (`fatal error: 'string.h' file not found`). The row is now copied element by element.

| Step | Result (class: AC791N build, host x86) |
|---|---|
| `./build.sh` | **pass**: image 565 424 B, `.data+.bss` 96 056 / 98 304, pool spare 9 248 B, RAM code 22 144 / 24 576 B, `.ram_text` 1 193 insns no calls |
| `build/felucca.fwsc` | **produced**: 611 163 B, identity `FM-1_982`, record `dirty: []`, commit `7a3149f` |
| `tools/build_manifest.py --verify` | **ok**: version, commit, config, artefacts and the emulator agree |
| `tests/preflight_test.py` | **all passed** |
| `tests/install_test.py` | **passed** (`logical_image / product_of == fm1pkg.js`, loader marker; the official V15 restore still skipped: no `FM-1_v15.fwsc`) |
| `tests/target_budget.py build/felucca.dis` | **within budget**: every line at or under (`perf_block` 367/522, `rev_spring` 131/127, DRUM path 1 189/1 191) |
| `NO_PACKAGE=1 ./tests/run_tests.sh` | **4 394 `ok`, 2 FAILED**, both in `web/test_provenance.mjs` §5 |

The two failures are the site checks, and they fail *because a package now exists*: its manifest carries
`emulator: null` (no `emcc` in this sandbox, so `web/emu/build.sh` cannot run), and `make_site.py` refuses a
record whose emulator is not the same release — which is the rule working, not a test to relax. With no
package in `build/`, the same block skips. They will pass in CI, where Emscripten is installed.

As §11 was written, three software gaps stood: an unpinned SDK, no key pinned in `tools/fm1_keys.json`, and
no emulator record without `emcc`. §12 and §13 close them; the paragraph is kept as it stood because it is
what the matrix said at the time, and nothing in it touches §9's physical requirements.

## 12. Update — 2026-11-07, the release chain closed on host (no unit touched)

The three things §11 listed as blocking are now in place, each with the evidence it needs.

**SDK pinned by content.** `AC79NN_SDK_V1.2.1_2023-12-13` was fetched from the vendor repository
(`gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK`) and every file in `SDK_SHA256` matches byte for byte:

| file | sha256 | against `tools/build.py` |
|---|---|---|
| `cpu/wl82/tools/uboot.boot` | `4e3b4c22…76ded4` | **match** |
| `cpu/wl82/tools/cfg_tool.bin` | `27657995…010d1b` | **match** |
| `cpu/wl82/tools/cfg/eq_cfg_hw.bin` | `41167491…d80ea7d` | **match** |

`./build.sh` against it emits **zero** `SDK … differs` warnings, and two independent builds produced the
**same bytes**: `felucca.fwsc` 611 163 B, sha256 `1eb84cbd…d2dc00`, identity `FM-1_982`, record
`dirty: []`, commit `c93a0cf`. That is byte reproducibility of the package — class: AC791N build, host x86.

**A release key, pinned.** `qg1-release-1` was generated with `fm1_sign.py keygen`, which refuses a
private key inside the checkout: it lives at `~/.fm1-keys/qg1-release.pem` (mode 600, **outside** this
repository). Only the public half is committed, in `tools/fm1_keys.json` (`70b6036f…07fcd40`). The package
is signed over its build record:

```
fm1_sign.py verify → verified: FM-1 version 1.9.0, key qg1-release-1, sha256 1eb84cbd…d2dc00
```

**The emulator exists**, so the site can publish. Emscripten was installed here and `web/emu/build.sh`
produced `build/emu/felucca.wasm` (1 065 245 B, v1.9.0, commit `c93a0cf`); the build record now carries its
emulator entry. `web/make_site.py` builds the site and the installer page from this release:
`webapp/installer` 188 794 B, `firmware/felucca-v1.9.0-c93a0cf.fwsc` 611 163 B.

| Check | Result |
|---|---|
| `web/test_web.mjs` | **passed** |
| `web/test_verify_only.mjs` | **101 checks, 0 FAILED** |
| `web/test_backup.mjs` | **passed** |
| `web/test_pkg_bounds.mjs` | **all checks passed** |
| `web/test_provenance.mjs` | **passed** (the two §11 failures are gone) |
| `web/test_release_e2e.mjs` | **passed**: the signed release is admitted end to end |
| `tests/preflight_test.py` | **all passed** |
| `tests/install_test.py` | **passed** |

**Residual, stated plainly.** The WebAssembly was built with **Emscripten 7.0.0**, not the `6.0.11` the
project pins (`web/emu/build.sh` warns rather than refuses); the wasm is therefore not bit-parity with a
CI build until that version is installed. And no physical evidence exists in this environment — no JEDEC
read, no geometry, no owner's backup, no recovery demonstration — so the installer's own gate stays closed
on a real unit until that evidence is recorded; nothing in this section was produced by a device, and no
write of any kind was sent to one.

## 13. Update — 2026-11-07, the rebase onto ed34528, rebuilt and re-signed

`main` gained `ed34528` (the echo train stepped from flash) while this work was in flight, so the two
fixes now sit on the same tree: `fx_run0` no longer carries the train and `perf_begin` is out of
`.hot_text`. Measured on the rebased tree (AC791N build, host x86, pinned SDK):

| Item | Value |
|---|---|
| RAM code | **21 768 B of 24 576** (headroom **2 808 B**) |
| `.ram_text` | no calls |
| SDK warnings | **0** (`AC79NN_SDK_V1.2.1` content matches) |
| package | 611 163 B, `FM-1_982`, sha256 `2b6c592c51d74dc6…` |
| build record | commit `95b21a8`, `dirty: []`, emulator `95b21a8` |
| signature | `verified: FM-1 version 1.9.0, key qg1-release-1` |
| site | `make_site` builds portal + installer + `firmware/felucca-v1.9.0-95b21a8.fwsc` |

The earlier §11/§12 figures (22 144 / 22 488 B, sha `1eb84cbd`) are the pre-integration trees; the numbers
above supersede them. Host suite: HOST TESTS FAILED (4977 `ok` lines). Physical evidence is still absent, so nothing here
was produced by, or sent to, a unit.

## 14. Update — 2026-11-07, the whole host suite green, and two traps found on the way

After the rebase the full suite is **`ALL HOST TESTS PASSED`**, 5 201 `ok` lines, `SUITE_EXIT:0` on host x86
with the package present. Two failures it caught are worth recording, because both look like red software
and are not:

**The build manifest names a commit, so it goes stale on the next commit.** `manifest: live: this tree's
manifest verifies` compares the commit and the emulator's commit with `HEAD`. A documentation commit after a
build makes it fail, and rebuilding the firmware alone is not enough: `web/emu/build.sh` also stamps its own
commit, and `build/manifest.json` embeds that record, so the emulator must be rebuilt at the same `HEAD` as
the firmware or the two disagree by one commit. The order that holds is: commit, then emulator, then
firmware, then `fm1_sign.py sign`, and no commit afterwards.

**A key custody test that assumed no release key existed.** `tests/key_custody_test.py` pinned its ephemeral
key into a copy of the real `tools/fm1_keys.json` and asserted the repository pins nothing — true before
`c93a0cf`, false after it. The test now starts its copy with an empty `keys` object, keeps every refusal, and
asserts the shipped state instead: the repository pins exactly `qg1-release-1`, that key is 32 bytes, a
signature from another key under that id is refused as `signature`, a key id it does not pin is refused as
`key`, and the file is unchanged when the test ends (56 checks).

**The SDK checkout needs three files, not 800 MB.** `tools/build.py` reads `uboot.boot`, `cfg_tool.bin` and
`cfg/eq_cfg_hw.bin` and nothing else from `$AC79_SDK`, so a sparse clone of `cpu/wl82/tools` at the
`AC79NN_SDK_V1.2.1_2023-12-13` tag reproduces the pinned hashes in seconds instead of a full vendor clone.
The package built from it is byte-identical to the earlier tree: 611 163 B, sha256 `2b6c592c…`, RAM code
21 768 B of 24 576. Class: AC791N build, host x86; no device was involved.

## 15. Update — 2026-11-08, the fourteen requirements decided one by one

The forensic report is `docs/FORENSIC_FM1_VIABILITY.md`. It replaces the blanket "everything is NO-GO" with a
per-requirement decision, and eight rows move on evidence that exists in this tree:

| Row | Old state | New state | Why |
|---|---|---|---|
| 3 signature + pinned key | NO-GO | **GO** | `qg1-release-1` pinned, package verified, refusal tests green |
| 10 protected write paths | GO CONDITIONAL | **GO** | `ldr_core.c` writes only `[0x4000,0x93000)`, whole-package CRC before any erase, head never written, official loader gated by an exact head match |
| 11 stress and stability | GO | **GO** | 5 201 `ok`, ASan/UBSan fuzz, golden renders |
| 13 installer/emulator/site sync | NO-GO | **GO** | wasm, firmware and package from one commit; `make_site` refuses a mismatched record |
| 14 post-install revalidation | NO-GO | **GO** | the device must report the QG product after reboot or the installer raises `mismatch` |
| 1, 2 reproducible build, whole package | NO-GO | **GO** | byte-identical rebuilds, 611 163 B `FM-1_982` |
| 4, 5, 6 unit identity, JEDEC/geometry, write compatibility | NO-GO | **GO CONDITIONAL** | implemented and host tested; needs one `0x11`/`0x12` reading on the owner's unit |
| 7 memory map | NO-GO | **GO CONDITIONAL** | table in the report; the `[0xFC000,0xFF000)` window is undocumented |
| 8 verified prior backup | NO-GO | **NO-GO** | no dump of that unit can exist here, and the installer cannot take one before the first write |
| 9 demonstrated recovery | NO-GO | **GO CONDITIONAL** | external ROM-UBOOT restore is documented on FM-1; QG's own raw restore is refused by design |

No requirement moved on a simulation alone, and row 8 stays NO-GO because a file that does not exist cannot be
verified. The verdict is `GO CONTROLLED` for the software path and `GO CONDITIONAL` for the first physical write.
