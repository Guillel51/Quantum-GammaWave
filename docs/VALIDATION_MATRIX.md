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
## 16. Update — 2026-11-08, the AC791N image built at `-Oz` (no unit touched)

The render budget is measured from the target's own disassembly (`tests/target_budget.py`), not from a
guess, so the memory question came first: `tools/ram_report.py` (new) reads `build/felucca.elf` and
prints every linker region and the largest symbols, taking the section from `objdump -t` because the
linker's placement wins over what a `__attribute__((section))` in the source suggests. With that in hand
the whole image was rebuilt at `-Oz` instead of `-Os`.

`-Oz` alone shrank everything and cost exactly two lines: the audio helpers that the source had marked
`inline` stopped being inlined into `dx_run` and `dx_run_sn`, so the two heaviest drum loops grew
(+196 % and +56 %) and took the `DRUM_PATH` sum with them. Marking those helpers `always_inline`
(`mulq16`, `sine_i`, `softclip`, `noise32`, `tsvf_coef_k` in `dsp.c`, and the generated `pitch_inc`)
recovered the loops; `dv_kb` deliberately stays a call, because inlining it moved three divisions into
the hit loop and cost more than the call it removed.

| Region / metric | `-Os` (`ed34528`) | `-Oz` (`1b72598`) | Change |
|---|---|---|---|
| image | 565 160 B | **531 172 B** | −33 988 B |
| XIP code | 403 228 B | **371 564 B** | −31 664 B |
| XIP constants | 139 416 B | 139 416 B | 0 |
| RAM `.data+.bss` | 96 056 B | 96 088 B | **+32 B** |
| RAM region (`ram_report`) | 96 036 used | 96 068 used (2 236 free) | +32 B |
| pool used / spare | 318 432 / 9 248 B | 318 432 / 9 248 B | 0 |
| RAM code | 21 768 B of 24 576 | **19 444 B** (headroom **5 132 B**) | −2 324 B |
| NOINIT | 15 572 B of 15 696 | 15 572 B | 0 |
| package | 611 163 B | **610 806 B** | −357 B |
| summed loop cost | 44 045 | **34 002** | −22.8 % |
| `analog_render` / `analog_morph` / `analog_ext` | 577 / 764 / 686 | 148 / 89 / 157 | −74 % / −88 % / −77 % |
| `fm1_timer5_irq` (key/LED scan) | 34 988 | 27 338 | −22 % |
| `dx_run` / `DRUM_PATH` | 409 / 1 189 | **426 / 1 153** | +4 % / −3 % |

Class of measurement: **AC791N build** (pi32v2 clang, pinned SDK, static instruction count from the
disassembly) and **host x86** (`regress`, the golden renders and the CPU baseline). None of it is a
cycle count on an FM-1, and the +32 B of RAM is the honest direction of the trade: 32 bytes of `.data`
for 34 KB of flash and 2.3 KB of RAM-code headroom.

What this does **not** say: nothing about a physical unit, and the loop cost is a static count of a
function's own loops, not a CPU margin.

**The release chain is stale after this commit, on purpose.** The firmware bytes changed, so the signed
package of §13 (`611 163 B`, sha `2b6c592c…`) no longer describes this tree, and the published site still
names that build. Re-signing needs the release private key, which by design lives outside the checkout
(`~/.fm1-keys/`, `docs/SIGNING_CUSTODY.md`) and is **not** present in this environment: nothing here
could sign, so nothing here was published. The emulator was rebuilt from this tree (WASM 1 065 573 B,
build `1b72598`) with **Emscripten 7.0.0** because that is what installs here, not the pinned `6.0.11`:
`web/emu/emu_test.mjs` passed (all checks, the heavy song at 2.3 % of real time), but bit-parity with a
CI build of the pinned version is not claimed.

A repository defect surfaced here and was fixed rather than worked around: `build_manifest.py` counted
the toolchain as an uncommitted change whenever `git status` printed the untracked `.tools/` directory
instead of its contents, so a clean local build could fail `--require-clean` and `make_site` refused the
package for a dirty record it did not have; it now treats a path that *contains* an external input as
that input. `tests/key_custody_test.py` had the same shape of problem for the pinned key (§14 records
the fix); with both in place the host suite is green again: 4 999 `ok` lines, `ALL HOST TESTS PASSED`,
and `web/test_provenance.mjs` passes with an emulator built from this tree.

## 17. Update — 2026-11-08, the `-Oz` tree rebuilt and re-signed for the release

`main` gained the `-Oz` profile (`933c7d8`) while the release was being assembled, so the package was rebuilt
and re-signed on that tree rather than shipping bytes that no longer described it:

| Item | Value | Class |
|---|---|---|
| RAM code | **19 444 B of 24 576** (headroom **5 132 B**), calls reach 88 948 B | `[TARGET]` |
| package | **610 806 B**, `FM-1_982`, sha256 `5c80a092…` (rebuilt in §17 as `80ad995c…` at `7eb9636`) | `[TARGET]` |
| build record | commit `26d7a6b`, `dirty: []` (updated in §17 to `7eb9636`) | `[TARGET]` |
| signature | `verified: FM-1 version 1.9.0, key qg1-release-1` | `[HOST]` |
| emulator | 1 066 409 B wasm, Emscripten **6.0.11**, same commit as the firmware | `[WASM]` |

The figures in §13 (21 768 B, `2b6c592c`) describe the `-Os` tree and are superseded by these (and in turn by §17's final build `7eb9636` / `80ad995c…`). Nothing here
was measured on a device.
## 18. Update — 2026-11-08, what is left after `-Oz`: three measurements and no dead weight

Before touching anything else the obvious suspects were measured on the built image, and each came back empty:

| Suspect | Measurement | Result |
|---|---|---|
| dead code | app link with `--gc-sections` (it was not passed) | **32 B** — there is no unreferenced function left, so the flag was not kept |
| duplicated constants | SHA-256 of every symbol in the read-only image, grouped | **68 B** in 3 pairs (identical 24 B function bodies), i.e. LLVM already merged them |
| read-only data sitting in RAM | symbols in `.data` | **734 B total** for 62 symbols, all runtime state: the 96 KiB is `.bss` buffers |

So the RAM margin can only grow by sharing buffers whose lifetimes are proven disjoint, and the 26.5 KB
candidate set (`proj_scratch`, `sec_stage`, `undo_st`) is exactly the one whose lifetimes can overlap: an
autosave can run during PLAY, and a mutation can run with a song loaded. That is not a measurement this
environment can make, and a wrong call there corrupts a project on a unit. Not done, by decision, with the
numbers above showing why nothing cheaper was left.

Two things were added instead of guessed at. `CPU_SPREAD=1` in `tests/regress.c` now reports, per CPU row, the
**p50/p95/p99/p99.9/max of each 32-sample block** and how many blocks went over the 725 us budget — the mean
alone cannot see a dropout. Class: host x86, wall clock, opt-in (it costs a clock read per block and its
outliers are the host's: the same rows showed a single 4-5 ms block that no other run reproduced, which is a
page fault or a preemption, not the DSP). On the heaviest mix (`cpu/mix/6parts_full`, six parts, a different
engine each, 8 notes each, four sends open):

| CPU row | p50 | p95 | p99 | p99.9 | max | blocks over 725 us |
|---|---|---|---|---|---|---|
| `cpu/mix/6parts_full` | 14.1 us | 17.6 us | 24.6 us | 45.6 us | 52.0 us | 0 |
| `cpu/mix/idle` | 2.9 us | 3.0 us | 6.1 us | 30.1 us | 31.3 us | 0 |

And the RAM-code headroom left by `-Oz` (5 132 B) is **not** being filled with more hot functions: the only
metric available here is a static instruction count, and moving code between XIP and SRAM does not change it,
so a claim that it helps the device would be a hunch. `tests/target_budget.py` also shows the dominant line is
the key/LED scan ISR (27 338 of 34 002), whose loops are bit-banged GPIO with settling delays that exist to
make the hardware work: shortening them is not an optimisation, it is a bug.

## 19. Update — 2026-11-08, the 12-voice question answered by the linker (no unit touched)

The experiment was run properly this time: the voice count is now a real build knob (`FELUCCA_VOICES=12` for
`tools/build.py`, `POLY=12 ./tests/run_tests.sh` for the host), and the stale comment in `core.h` that claimed
`POLY=12` already worked (neither script implemented it) is gone. Both scripts refuse anything but 10 or 12, so
a typo cannot produce a build whose voice count nobody wrote down. 10 remains the shipped value; every golden
render, budget and release artefact in this repository is still taken at 10.

| Question | Measurement | Result |
|---|---|---|
| Does it fit? | `FELUCCA_VOICES=12` target link | **No. `section .bss will not fit in region RAM` / `region RAM overflowed by 40 bytes`** |
| How much does it need? | 2 216 B free, over by 40 B, 2 extra voices x 6 parts | **2 256 B = 188 B per voice** (`sizeof(voice_t)`), i.e. the RAM margin would be **−40 B** |
| Does the sound change? | `POLY=12` host suite, 220 golden renders | **2 changed**: `mode/ANALOG/UNISON` and `mode/VOICE/UNISON` |
| Why does UNISON change? | `voice.c trk_nvoice()` | UNISON stacks as many voices as the engine may use, so more voices is a different timbre, not only more notes |
| What does it cost in CPU? | `POLY=12` host x86, CPU time per sample, 3 alternating runs, median | `cpu/mix/6parts_full` 410 -> **465 ns** (+13 %); `cpu/mix/3parts_full_drums` 337 -> **368 ns** (+9 %) |

The earlier estimate of "+1 056 B, 11.6 % CPU" was wrong on memory by more than a factor of two: the real cost is
2 256 B, and it does not fit. `-Oz` (section 16) bought 34 KB of flash and 2.3 KB of RAM-code headroom and
**zero bytes of `.data+.bss`**, so it could not have changed this: the two voices need RAM the build does not
have, and freeing it would mean sharing the 26.5 KB of staging buffers (section 18 says why that is not a
measurement this environment can make) or shipping with no margin at all.

**Decision: rejected for the stable firmware.** It fails the first requirement — it does not link — and even
with 2 256 B found it would change two shipped UNISON sounds and cost 9-13 % CPU on the heaviest mix. It stays
as a reproducible experiment behind the two knobs above, which is what "keep it experimental" has to mean if it
is to mean anything.

Also found in this pass, and worth recording because it weakens a check rather than a feature: on a host whose
kernel does not expose `proc_pid_rusage` instructions (this Linux one does not), `tests/regress.c` prints
`no instruction counter on this host (proc_pid_rusage); the budget is not checked` and **skips the CPU budget
comparison entirely**. The ns per sample are measured and printed, and the `CPU_SPREAD` percentiles work, but
the pass/fail line is not enforced there. That is a gap in the harness, not in the firmware, and it is the next
thing to fix (a per-host ns baseline, created on first run and compared afterwards, since ns are not portable
across machines the way the instruction count is).

One more thing this pass hit, so the next one does not lose an hour to it: a `NO_PACKAGE=1` run on a tree
whose `build/emu` predates `HEAD` ends with `HOST TESTS FAILED` and exactly two red checks, both from
`web/test_provenance.mjs` (`make_site: a clean record and its package build the site`, and the CSP one). The
reason is printed by `make_site.py` itself: `refusing to publish it: the emulator was built from 'ea8a8a5e9aa9',
the firmware from 32ecee5`. Nothing is wrong with the firmware or with the check — the check is doing exactly
what it is for. The emulator is a build artefact and is rebuilt in the same commit by `emulator.yml`, so CI does
not see this. Either rebuild `build/emu` at `HEAD` or read those two red lines as "stale emulator", never as a
regression.

Host suite at 10 voices on this tree: every check passes except those two.

## 20. Update — 2026-11-08, v1.9.0 published: tag, release and site on one package

**v1.9.0 is published.** Tag `v1.9.0` → `7eb9636`, GitHub Release `Quantum GammaWave v1.9.0` (not a draft,
not a pre-release), four assets, and Pages serving the same bytes. Everything below was read from the public
URLs afterwards, with no credential: that is the `[PUBLIC]` class, not a plan.

| Item | State | Evidence |
|---|---|---|
| Tag | **created** | `refs/tags/v1.9.0` = annotated `a7bd0c6c`, peeling to `7eb9636f58f4…` (`[PUBLIC]`) |
| Release | **published** | `releases/tag/v1.9.0` → HTTP 200, name `Quantum GammaWave v1.9.0`, `draft: false`, `prerelease: false` (`[PUBLIC]`) |
| Assets | **four, downloaded and checked** | `felucca-v1.9.0.fwsc` 610 806 B, `felucca-v1.9.0.manifest.json` 492 B, `build-manifest.json` 2 414 B, `SHA256SUMS` 267 B (`[PUBLIC]`) |
| Hashes | **match** | `sha256sum -c SHA256SUMS` on the downloaded files: three `OK` (`[PUBLIC]`) |
| Signature | **verified on the downloaded asset** | `verified: FM-1 version 1.9.0, key qg1-release-1, sha256 80ad995c…, record abcf3914…` (`[PUBLIC]`) |
| Release = tree | **byte-identical** | `cmp` against `release/` for all four files (`[PUBLIC]`) |
| Pages | **same package** | `firmware/versions.json`: `v1.9.0-7eb9636`, sha256 `80ad995c…`, 610 806 B, record `7eb9636`, emulator `7eb9636` (wasm 1 073 325 B) (`[PUBLIC]`) |
| Site package = release | **byte-identical** | downloaded `firmware/felucca-v1.9.0-7eb9636.fwsc`, sha256 `80ad995c…`, `cmp` clean (`[PUBLIC]`) |
| Installer page | **live and parsed** | inline module 175 933 B, `node --check` clean, CSP hash matches the script, `qg1-diagnostic-1` and `#vrep-states` present, no key material (`[PUBLIC]`) |
| Workflow | **green end to end** | run 42482503161: build, deploy and publish all success (`[PUBLIC]`, log in `ci/log/42482503161-success`) |

### Why the tag had never existed

Three real defects, each found in the run log rather than guessed. The Actions API is rate-limited from this
host and there is no `gh` credential here, so `.github/workflows/ci_diag.yml` copies the finished run's log
into `ci/log/<run>-<conclusion>`: the runner has the token, and a branch can be read without one.

1. **`git tag -a` had no committer.** `fatal: empty ident name (for <runner@…>) not allowed`. A fresh runner
   has no git identity, so every earlier publish run passed its checks and died at the tag, exit 128. The tag
   now carries the workflow's own identity (`-c user.name=… -c user.email=…`), verified in a scratch repo
   with the identity removed.
2. **The package described a tree that was no longer `main`.** The log's own words:
   `the source changed after the build: firmware/src/ui_being.c`. The owner's UI pass had landed after the
   package was signed. Rather than relax the check, the package was rebuilt and re-signed at the commit it
   ships from; the package bytes are unchanged (`80ad995c`) because nothing that reaches them changed, only
   the record, which names its commit.
3. **The deploy check asked for something impossible.** It demanded `GITHUB_SHA` of a `versions.json` keyed by
   the *build record's* commit — and the package is always built before the commit that adds it to `release/`.
   The site also recorded the emulator committed beside the release while it copied this run's build. Now
   `make_site.py --emu` records the emulator actually served, and the check holds the package to its record
   commit and signed hash and the emulator to `GITHUB_SHA`, comparing ids as prefixes (a 12-character manifest
   id against a 40-character SHA was the fourth bug, and it failed a site that was correct).

Idempotence: the publish job first asks whether `v1.9.0` already names this build *and* has a release, and
stops there if it does — so a later commit cannot silently republish different bytes, and a finished release
does not turn CI red forever.

### The installer's eight states, audited

| State | True only when | Cannot be shown without |
|---|---|---|
| `DIAGNOSTIC READY` | a read-only run produced a report | the report itself |
| `VERIFY READY` | an FM-1 answered the identity query | `kind != none` |
| `PACKAGE VERIFIED` | the offered package describes itself consistently | gate row `gate.package` |
| `SIGNATURE VERIFIED` | an Ed25519 signature was checked against a pinned key | gate row `gate.signature` |
| `DEVICE COMPATIBLE` | the unit's own answer fits this package | gate row `gate.compat` |
| `RECOVERY NOT VERIFIED` | no recovery was demonstrated on this unit | `gate.recovery != pass` |
| `INSTALL READY` | **every** gate row is a pass **and** `verdict.canInstall` | both, together |
| `INSTALL BLOCKED` | the complement of the previous line | never writable |

`INSTALL READY` is computed from the gate rows and the verdict; there is no path to it that skips a row, and
`RECOVERY NOT VERIFIED` / `INSTALL BLOCKED` are marked apart from the states that represent evidence so they
cannot be read as progress. Verify Only stays read-only: it sends the identity query and never opens a chip
session, so `flash.jedec` is reported as "cannot be established without a write session", never as read. The
diagnostic export carries no key, no token and no package bytes, and a person can save it after Verify Only
without starting an install. 108 checks, 0 failed (`[HOST]`).

### What is still not done, and is nobody's fault but physics

The gates that need the owner's unit are unchanged and open: the JEDEC and geometry of *that* FM-1, a 1 MiB
dump of *that* unit, the verification of that dump, a rehearsed recovery, and the first controlled write.
Nothing in this pass sent a write, erase, bootloader change or recovery command to any unit. See
`docs/FORENSIC_FM1_VIABILITY.md`.

## 21. Update — 2026-11-08, the 3.1 tree rebuilt here, and it reproduces the published package

This pass fetched the pinned toolchain and SDK into another machine and rebuilt the tree the owner's visual
pass landed in, then built the emulator, then ran what only those builds can check. It exists to answer one
question the publication log cannot: **is the published package really this firmware?** Every figure is
`[HOST]` or `[TARGET]`; no unit was involved, and nothing here is a second publication.

| Item | State | Evidence |
|---|---|---|
| Toolchain / SDK | pinned and verified | JieLi `jieli-linux-toolchains-20260730.1`, sha256 `a1545cdd…` checked by `tools/get_toolchain.sh` against `tools/toolchain.sha256`; SDK branch `AC79NN_SDK_V1.2.1_2023-12-13` (`[HOST]`) |
| AC791N build | **built** | image 531 992 B; `.data+.bss` 95 912 B of 98 304 (2 392 B spare); `POOL_END - pool_top` 9 248 B; XIP code 372 508 B; RAM code 19 284 B of 24 576 (`[TARGET]`) |
| Package | **identical to the published one** | 610 806 B, `FM-1_982`, sha256 `80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2` = `release/felucca-v1.9.0.fwsc` = the asset and the site's copy (`[HOST]`) |
| Reproducibility | **GO on a second machine, and now gated** | two clean builds are byte-identical (`cmp` on `.fwsc` and `loader/ota.bin`); `tests/release_repro_test.py` compares a fresh build with `release/*.fwsc` and fails on the first differing byte, in the host suite and in `target.yml` (`[HOST]`) |
| Emulator | **built here** | Emscripten 6.0.11, 1 073 325 B wasm, sha256 `3d039f75…`, named by the record at `HEAD`; `build_manifest --verify --require-clean --expect-commit` ok (`[HOST]`) |
| Budgets and reader | **GO** | `memory_map_test`, `target_budget.py` on `build/felucca.dis`, `web/test_pkg_bounds.mjs` (`[TARGET]`/`[HOST]`) |
| Host suite | **ALL HOST TESTS PASSED** | full run with the package present: `test_release_e2e`, `test_provenance`, `test_verify_only` (108 checks), `key_custody_test` (`[HOST]`) |

The value of this section is the equality, not a new claim: an independent rebuild, in another checkout and
another toolchain directory, produces the same 610 806 bytes that `release/`, the Release asset and Pages
serve, so "the published package is this firmware" stops being an inference. It also corrects a figure: the
record of the package built at `e784b70` carried `.data+.bss` 96 088 B, and this tree spends 95 912 B — the
visual pass did not take RAM from anywhere.

The visual pass itself was re-read at 3–6× on HOME (solar bay and scope), SCL in QUANTUM, CYBERPUNK and
NORDIC, FX 1 and the held punch-in map: the degree labels sit only on in-scale keys (C MIN: C, D, F, G; E, A
and B stay recessed), exactly three accidentals carry a strip (D#, G#, A#), the spectrum caps stay inside rows
153–178, and the map's header (`LATCH:NO`), LATCH card and `KEYS · FX · K1-4 MACRO · OCT+ LATCH` footer agree
with the engine flags. No further defect was found, and none was invented to have something to change.
## 22. Update — 2026-11-08, voice stealing takes the quietest voice, and it shows in a test

The policy in `voice.c` documented itself as "the oldest released, else the oldest held voice of a POLY part that is
not its lowest note". A test written for this pass found what "oldest" costs: a chord held from the start of the
phrase is cut note by note while a fresh *quiet* note next to it keeps its voice, and when the victim is the part's
own the voice is restarted **in place, with no fade at all** — the harshest outcome of the two, and the one the
older comment did not mention.

The scenario is deterministic: two POLY parts hold ten notes (the budget), the five **older** ones at velocity 120
(env 30718 of 32767, around -0.6 dB) and the five **newer** ones at velocity 24 (env 6143, around -14 dB). Four more
notes then arrive one at a time and the engine has to choose. The test (`voices: stealing takes a quiet voice, not
a loud one`, `tests/regress.c`) counts every voice that is lost — fading after a cross-part take, or restarted in
place — and records its envelope the block before.

| Policy | Voices taken | Mean envelope at the take | Worst | Audible (>= 8000, ~-12 dB) |
|---|---|---|---|---|
| oldest first (before) | 6 | 17 408 | **30 718** | **3** (notes 55, 59, 62: the loud sustained ones, two in place) |
| quietest first, oldest on a tie (now) | 4 | 6 143 | 6 143 | **0** |

The new rule keeps the classes exactly as they were (released tails first, then extra UNISON voices, then held
voices of a POLY part that are not its bass, and the lowest held note of every part stays protected); inside a class
the voice with the lowest `env_out x (level + 1)` goes first, and age only breaks a tie. A release tail about to die
is still the cheapest thing to lose, and so is the soft note of a chord beside the loud one. It is the same measure
`quiet_tail` already used for dying releases, now applied to the voices that are still held.

Evidence, by class:

* **Host x86, verified**: `tests/regress.c` as above; 220 golden renders, **0 changed** — the improvement is
  invisible in every existing render, because none of them steals under contention; all voice and routing checks
  pass; the CPU gate reads PASS.
* **Target compiled**: the same tree builds, image 531 172 -> **531 204 B (+32 B of flash)**, `.data+.bss` 96 088 B
  (unchanged, **0 B of RAM**), RAM code 19 444 B (unchanged), pool spare 9 248 B. The extra work is one multiply per
  candidate voice on the note-on path (at most 60, never per sample).
* **Hardware**: none. No unit was touched; this is not a claim about how it sounds on the FM-1.

Retrigger, mono/legato/unison, sustain, the bass protection, the note-off paths and the engine-switch fade are
untouched by this change, and their existing checks pass unchanged.

## 23. Update — 2026-11-08, the installer was refusing its own package

The first real Verify Only report (`qg1-diagnostic-v1.9.0-7eb9636-7eb9636.json`) carried one line that
explained the whole block: **`manifest: The package is for FM-1, not FM-1_982.`** The signed manifest names
the *product* (`FM-1`); the page and the install path name the *identity of the build* (`FM-1_982`); and
`verifyManifest` compared them with `!==`. Every package this project ever published was refused by its own
installer, `releaseOk` never became true, and **INSTALL was disabled on the live page for everyone**, with or
without a unit attached. The unit never got a chance to be the problem.

Three fixes, each with the evidence that produced it:

| Defect | What it did | Fix | Evidence |
|---|---|---|---|
| Product compared to identity | refused every published package before any device was involved | the manifest's product is matched to the *model* of the identity, never across models | live page: `Error: The package is for FM-1, not FM-1_982.` · fixed build: `Ready to flash (610,806 bytes)` (`[PUBLIC]`, browser) |
| Signature reported as "cannot be established without a write session" | the one gate that needs no hardware was marked unknowable, so `SIGNATURE VERIFIED` was unreachable | `verifyPackage()` re-hashes the bytes, reads the package's own identity, and checks Ed25519 against the pinned key — offline | `verifyPackage`: `pass`, `key qg1-release-1`, `610806 B`, sha256 `80ad995c…` (`[HOST]`, Node 26 WebCrypto over the released bytes) |
| A report could not say *why* nothing answered | `query.frames: 0` sat next to "every probed port stayed silent" when the browser had enumerated **zero** ports | `Updater.find()` now records `reason` (`no-ports` / `no-match` / `no-pair` / `send-failed` / `silent` / `answered`), the input and output counts, and every port name WebMIDI exposed | `web/test_verify_only.mjs`: zero-port, silent-port and foreign-port cases (`[HOST]`) |

`INSTALL` now enables (`#go.disabled === false` after the four preflight boxes) on the fixed build with **no
device connected**, while `INSTALL READY` still requires every gate row to pass — which still needs the unit.
Verify Only now reaches `PACKAGE VERIFIED` and `SIGNATURE VERIFIED` and still cannot reach `VERIFY READY`,
`DEVICE COMPATIBLE` or `INSTALL READY` without an answer from the unit. `verify only: 123 checks, 0 FAILED`.

Nothing in this pass weakened a gate: the model rule is narrower than the name-based comparison it replaces
(it can never accept another model), and the signature check is a real Ed25519 verification of the real bytes
rather than the presence of a key in a map. The blocked gates are unchanged and physical: the unit's own
JEDEC and geometry, a 1 MiB dump of that unit, that dump's verification, a rehearsed recovery, the first
controlled write. See `docs/FORENSIC_FM1_VIABILITY.md`.

## 24. Update — 2026-11-08, the public site now serves the release's emulator, not main's

Deploying the installer fix exposed the next coupling, in the live page itself:
`Error: The emulator was built from 55a7be2, the firmware from 7eb9636: they are not the same build.`
`fm1provenance.js` holds the package to the emulator its own build record names — a good rule — but the
Pages workflow copied **this run's** `build/emu` (main, which has advanced past the release) into the site.
As soon as `main` is ahead of the last release, the public site serves an emulator from another tree and its
own installer refuses its own package. The `deploy` job had been saying so in the only place nobody read:
`site: versions.json's emulator record is the emulator's wasm  FAIL`.

The fix is not to relax the rule but to satisfy it: when the site serves the committed release, it now builds
that release's emulator from the release commit and serves **that** one, so package, record, emulator and
installer are one release again. The release emulator is reproducible — built here from `7eb9636` it produced
`3d039f75bbc8…`/1 073 325 B, byte for byte the wasm `release/emu/manifest.json` names (`[HOST]`, emsdk
6.0.11). The preview channel (`/emulator/` under `preview/`) still shows main.

Checked locally against the real artefacts: `site_check.py` with `--expect-commit 7eb9636
--expect-sha256 80ad995c… --expect-emu-commit 7eb9636` → **all checks passed**, and the installer loaded in a
browser reports `Ready to flash (610,806 bytes)` with `#go.disabled === false` and no device attached
(`[HOST]`, HeadlessChrome). `deploy` now expects the release commit of the emulator it serves, and
`GITHUB_SHA` only when there is no committed release to serve.

## 25. Update — 2026-11-08, a visible port can no longer be missed, and one that is absent cannot be conjured

The second real Verify Only report says, in the new rows, exactly what is and is not true: `pkg.verified`
passes, `gate.signature` is `pass`, `SIGNATURE VERIFIED` is reached — and `ports.inputs: 0`,
`ports.outputs: 0`, `query.answer` sourced as *"this browser sees no MIDI device at all: no input and no
output"*. The installer is enabled and the package is trusted; the operating system is not presenting the
unit. That is not something a web page can fix, and this pass does not pretend otherwise. What it can do is
make sure a unit that *is* presented is never missed:

| Change | Why it existed |
|---|---|
| pairing by normalised descriptor, not exact name | Windows exposes one device as `MIDIIN2 (FM-1 Midi)` and `MIDIOUT2 (FM-1 Midi)`; compared literally, every such unit looked like "an input with no output" and nothing was probed |
| Verify Only probes every in/out pair (`probeAll`) | a unit whose Windows descriptor does not say FM-1 was unreachable; the only frame sent is the read-only identity query, and the answer, never the name, decides what it is. The **install** path keeps the name filter and is proven never to probe foreign gear |
| `statechange` re-runs Verify Only | plugging the unit in while the page is open needed a reload; now the page logs `ports changed: N in / N out` and re-reads, read-only |
| an empty list says what it means | `vNoPorts` in EN/ES/ZH: with the unit powered and a data cable, an empty list means this system is not presenting it (charge-only cable, unit off, or another program holding the port — Windows gives one client exclusive access per port) |

Evidence (`[HOST]`, simulated device, not hardware): with a fake WebMIDI stack exposing one unit under those
two Windows names and answering the vendor handshake as a stock V15, the page reports
`Verify: FM-1 detected, it answers official V15 (FM-1_015)`, `ports 1 in / 1 out`, gate rows 1–5 **pass**,
and the states `VERIFY READY`, `PACKAGE VERIFIED`, `SIGNATURE VERIFIED`, `DEVICE COMPATIBLE` reached, with
`INSTALL READY` still not reached. `verify only: 126 checks, 0 FAILED`.

What this does **not** establish: any reading from the owner's unit. With `0 in / 0 out` there is nothing to
read, and no software change on this side can create a port the OS does not publish. The gates that remain
need the unit (identity, JEDEC, backup, recovery) — and on a **stock** unit the JEDEC is refused by design,
because `0x12` is answered only by QG (`docs/JEDEC_READ.md` §1): the first backup of a stock unit is a
hardware SPI read.

## 26. Update — 2026-11-09, PR #73 reviewed before merge: kept, corrected, removed

The branch `jules-16051787961813429323-dfe6ca9b` (17 commits, `9aa1a1b`) was reviewed file by file against
`4e8d6b6`. This section records the decision for each part, because a merge that only says "reviewed" says
nothing.

**Kept.** The multi-chip allowlist, because it fails closed on both installers: `fl_jedec_writable()` still
returns only Puya `0x856014` and UCUN `0xB36014`; `tools/fm1_gate.py` refuses a `READ_ONLY_JEDEC` id with
`chip_read_only` before `PRODUCTION_JEDEC` is consulted; `requireChip()` in `web/fm1ota.js` refuses
`READ_ONLY_JEDEC` before `KNOWN_JEDEC`, and `probeChip()` reports `writable: false` for those five parts.
Reading and backing up more parts is not writing them. `tests/negative_write_test.py` and
`tests/backup_audit_test.py` (both import `tools/fm1_gate.py` and `tools/fm1_image.py`) and
`tests/installer_pipeline_production_test.mjs` (the real `Updater`/`requireProof` against the release
package the build record names) stay. The documentation of the chip matrix, the flash map and the
M-Flasher import likewise stays: it is sourced (`docs/M-FLASHER.md` cites the files in that tool) and it
does not claim a measurement of our own.

**Corrected.** (1) `STOCK_DONOR_VERSIONS` had been widened to `{15, 16}` with no unit read, no document and
no report behind it, while the comment above it says only the stock versions this project has actually read
are donors; it is `{15}` again and `web/test_donor.mjs` encodes that limit again (`stock V16 is not a donor
this project has read`). A donor is a stock unit an install may start from, so this is the one place where
an unverified widening is a write risk, not a convenience. (2) The installer's M-Flasher panel was Spanish
only inside a page that is EN/ES/ZH, and it printed an `m-flasher read --out … --size 1M` command and an
`OCT-`+`OCT+`+`HOME` combination as fact although neither is in the audited sources; it now has `data-t`
keys in the three languages and says what it is: documented from that tool's source, not demonstrated on a
unit here, with the key combination left to `docs/M-FLASHER.md`. (3) `tests/installer_pipeline_production_test.mjs`
had the release path and size typed in; it now resolves the package from `release/build-manifest.json`, so a
new release does not need the test edited and a renamed package cannot slip past it.

**Removed, and why.** `tests/installer_pipeline_gates_test.py`, `tests/nor_flash_fault_injection_test.py` and
`tests/power_cut_sim_test.py` do not test this repository: each defines its own `InstallerEngine`,
`SimulatedSpiNor` or `FakeOtaTransport` inside the test file and asserts on that. They pass whatever the
installer does, they duplicate `SUPPORTED_DONOR_VERSIONS` (so they would have kept asserting V16 after the
correction above), and a green run of them proves nothing about `tools/` or `web/`. The behaviour they name
is covered by real code: `tests/chip_test.py`, `tests/transport_gate_test.py`, `tests/install_test.py`,
`tests/backup_test.c` and `web/test_web.mjs` (the 0x12/0x13 gate, the UPGRADE gate, the loader's own
before-any-write refusals). `tests/ota_fault_injection_test.mjs`
only round-tripped `pack7`/`unpack7` and called it fault injection. `tools/fm1_backup_validator.py` had no
caller and no test and re-implements what `tools/fm1_flash_evidence.py` already does with a test
(`tests/spi_dump_test.py`).

**Enforced.** None of the six new test files ran in CI, and `tests/run_tests.sh` did not name them either: the
PR added coverage that nothing executed. The three that test real modules are now steps in
`tests/run_tests.sh`, which is what the `host tests` workflow runs.

**Class of this evidence.** `[HOST]`: the host suite on `9aa1a1b`, the JS suites, the gate and package tests.
Nothing here was measured on a device, and the merge changes no gate: the owner's unit stays at
`INSTALL BLOCKED`. The published release (`release/`, built at `7eb9636`) is untouched and is still the only
signed package; the commits merged here are not in it, and no package was rebuilt, signed or published for
this review.

**Known hole, reported not fixed.** The `emulator` workflow's step *the package, the installer and the editor*
carries `continue-on-error: true`, so a failure inside it does not fail the run (on `9aa1a1b` the step's own
conclusion was `success`). The steps after it — the target compiler checks and *a package and a site exist
(fail closed)* — are enforced, so a missing package is still caught; a broken assertion inside that step is
not.

## 27. Update — 2026-11-09, the chip gate is now a read, not a write session

Gate 6 ("flash chip, sizes and bounds") used to say *deliberately not done by verify only*, and the row for
it read *cannot be established without a write session*. That was wrong about the protocol: `0x12` is the
identification query and it is read-only, so the only reason a stock unit cannot answer it is that the
vendor firmware does not implement it (`docs/JEDEC_READ.md` §1) — not that answering it would require a
write.

The installer now has a **CHECK CHIP (read only)** action, next to Verify Only. It sends the identity
handshake and then two `0x12` identifications on the same link (`chipCheck()` in `web/fm1ota.js`, built on
`probeChip()`), and never opens the update path: no `F0 22 24 35 7F F7`, no `0x30`, no archive PUT. On a
unit running QG it fills gate 6 with the chip's own JEDEC and geometry and says whether the part is one
this installer writes; on a stock V15 it reports the refusal (`The FM-1 did not answer 0x12: nothing was
sent.`), which is a measured fact about that unit instead of an empty cell. The report gains
`chip.jedec`, `chip.geometry`, `chip.write` — or `chip.refused` — and the JSON carries the same block.

A passing gate 6 does **not** authorise anything: the verdict still requires the recovery, backup and
post-install gates, and on this project's unit those are physical. `INSTALL BLOCKED` is unchanged.

Evidence (`[HOST]`, simulated devices, no unit): `web/test_verify_only.mjs` now has ten more cases,
`136 checks, 0 FAILED` — a stock unit that ignores `0x12` (reported, one query on the wire, nothing that
writes), a unit answering `0x856014` (JEDEC, geometry, fresh session token, still no UPGRADE), a read-only
part `0xC84014` (identified, `writable: false`, refused by `requireChip()`), an unknown JEDEC, no port at
all, and both directions of the gate row in the report. The site still builds with the page
(`web/make_site.py`, 610 806-byte release package). What is **not** established: that any unit answers
`0x12` here, because none was available.

## 28. Update — 2026-11-09, the owner's unit is a known chip with a known backup (install path: superseded by §30.1)

The owner ran M-Flasher 0.9.4 against the unit and kept its record (`fm1-20261108-161613.json`):

| Field | Value | Class |
|---|---|---|
| identity | `FM-1 015 (stock)`, bootloader `stock (M-VAVE)`, app `OK` | `[HW]`, tool record |
| **flash** | **`0x856014`** (Puya P25Q80H), chip key `0x980F` | `[HW]`, M-Flasher's own `ONLINE_DEVICE` read |
| unit id | `4150353835313708` | `[HW]`, tool record |
| backup | 1 MiB dump, sha256 `38c66ae7c36b46f2fad1c0f8a041ba257934b1b46a73d4085216c60e78f2d9cd` | `[HW]`, tool record |
| tool | `m-flasher 0.9.4` (loader 1.3) | `[HW]`, tool record |

Gates 6 and 8 were the two the page could not answer on a stock unit. They are now answered, and not by
optional: **gate 6** is the chip's own identification, read by the tool that can reach the chip's ROM mode;
**gate 8** is a full 1 MiB read whose SHA-256 the page recomputes from the bytes the owner loads.

**What changed in the software.** A stock unit answers the handshake but not `0x12`, so `install()` used to
refuse on the *running* firmware's link before `UPGRADE` — which made a first install impossible by
construction, not by evidence. The install path now accepts a chip fact taken out of band, on one
condition and one only: the device is a **stock donor** (the same rule as before: same model, version in
`STOCK_DONOR_VERSIONS`), the record parses, its part is one this project writes (`0x856014`/`0xB36014`, the
read-only five are refused by name), and **the page has hashed the dump itself** and got the record's hash
(`web/fm1record.js`, `bindRecord()` in `web/fm1ota.js`). A unit already running QG is still identified on
its own link with `0x12`; a record never stands in for that. Without a verified record nothing is sent: the
suite asserts both directions on the wire.

**What this does not prove, said plainly.** The record is a claim until its dump is hashed here; the page
does that when the file is loaded, and refuses the pair otherwise. It does not prove the write works on
this part — `0x856014` is a writable part *in the allowlist*, not a write demonstrated on this unit. It
does not demonstrate recovery: the route is M-Flasher restoring that same dump, which is available and
audited but has not been tried on this unit. And the 1 MiB read was taken by a third-party tool; this
project verified its hash only when the bytes are loaded.

**Security state.** Nothing about the write path was relaxed: the signature, the package identity, the
model, the image hash, the one-use authorisation and the TTL are unchanged, the boot area is still never
written by QG, and `INSTALL BLOCKED` remains the verdict until the gates above are satisfied *for this
unit at that moment*. What is no longer impossible is the first install on a stock unit whose chip and
backup are known — the case this repository was built for.

Evidence (`[HOST]`, simulated unit): `web/test_record.mjs`, 28 checks, 0 FAILED — the owner's record parses
to `0x856014`/P25Q80H; a read-only part, another model, a non-stock bootloader, a bad hash and a 1 MiB − 1
byte dump are each refused; a verified record and the answering unit agree, and the same record against
another firmware version is refused; with the record, the install sends **zero** `0x12` frames and exactly
one `UPGRADE`, and a unit that does not hand over ends in an error, never a silent write; without it, the
install refuses with `chip` and sends no `UPGRADE` at all. `web/test_verify_only.mjs` (136) and
`web/test_web.mjs` stay green.
## 29. Update — 2026-11-09, the 3.2 visual pass: what was fixed, and the class of each proof

The pass was ordered with one priority (the PUNCH-IN LATCH), ten visual defects, and a rule: nothing counts unless it
is visible and checkable in the project's own emulator. This section records each item, its state, and the class of
evidence — never one class presented as another.

| Item | State | Evidence | Class |
|---|---|---|---|
| PUNCH-IN LATCH: root cause | **FIXED** | The engine was already correct: `test_punch_latch_matrix()` (15 keys of bank 1, three at once and the fourth refused, FX released, six mutes, bank change on A#4, LATCH OFF) passes. The defect was reachability: the mode shipped **OFF** and its switch lived only in MENU and in a hidden combo whose footer vanished exactly when LATCH was off. `prefs_update()` now turns `PREF_LATCH` on once per settings revision (`prefs_rev = 3`) and the map card reads `OCT+ TO SWITCH`. | host x86 (`tests/ui_test.c`), layout in `tests/ui_render.c` |
| FX percentage at 100 % | **FIXED** | The value is right-aligned against its meter (`cv_text_r(mx - 2, …)`) in `graph_fx()` and the DEPTH module, so it cannot reach the card edge on any palette. | host render, 240 px |
| SCL white keys | **FIXED** | The degree digit is gone (the key says `C`, not `1 C`); `scl_degree()` removed as dead code; `scl_key()` uses a 6 px round with a mixed corner pixel and a single lit row. | host render `docs/evidence/3.2/host_scl.png` |
| Delay bars | **FIXED** | `dly_tick()`/`dly_bar()`: always visible (minimum 3 px), 7 px wide, rounded tips, instant rise with a 3 px bounce, 1 px per frame fall. No new work in the audio path — the bars read the published level. | host render `host_dly.png` |
| RESO vs CLOUD | **FIXED** | Two different pictures: RESO is a resolver (four combs, energy dots running along the strings, one ring per note); CLOUD is a swarm at three depths that breathes with `rev_act` and puffs on a note. A `reverb_cloud` scene was added to the sweep so both are rendered, not described. | host render `host_reverb_reso.png`, `host_reverb_cloud.png` |
| HOME planets | **FIXED** | Four bodies (`NPL 4`): rock with craters, banded, ocean, ringed; orbits 11/18/26/33 with tilt, note ring and per-note excitation. `particle_bounds_test.py` now takes the planet count from `NPL` and checks every orbit against the bay arithmetic. | host render `host_home.png`; `tests/particle_bounds_test.py` |
| Boot splash | **FIXED** | New 16-bit identity: deep wash, three elliptical nebulae mixed on the sky's own colour, 40 stars in three layers, `QUANTUM`/`GAMMAWAVE` in the project's block alphabet (with G and V, which were missing), ringed planet with two moons, 16-block rail, BOOT/% readout, hand-over to HOME; 3.1 s. Audio, MIDI and init start before it, as before. | `ui_render` timeline (775 steps of 4 ms, 176 change pixels) + emulator capture `docs/evidence/3.2/emu_splash_strip.png` |
| DEPTH on KNOB 2 | **FIXED** | `layer_knob()` maps KNOB 2 to DEPTH and KNOB 4 to CRUSH (engine slots unchanged); tests updated (`EN_K2`/`EN_K4`, `rc_run`, `#102`, `#40`). | host x86 `tests/ui_test.c` |
| Memory | **PASS** | AC791N build (merged tree, commit 585d05d): image 535 204 B, `.data+.bss` 94 936 B of 98 304, pool spare 9 248 B (gate: >= 8 KiB). | target compiled |
| Bootloader, partitions, loader | **UNTOUCHED** | No file under `bootloader/`, `loader/` or the partition tables changed in this tree. | source diff |
| Emulator | **PASS** | wasm 1 086 293 B, sha256 `36e1dce6…`; the splash stages and HOME render in the browser emulator; the dev server answers 200 for `/` and `/felucca.wasm`. | emulator (wasm) |
| Published site, after the push | **PASS (same release as the package)** | The Pages run deployed `688d804`; the portal now carries `Qué cambia en v1.9.0`, the editor page reports `data-version="v1.9.0-7eb9636"` with the brand `WEB STUDIO // QG-1`, the installer page contains no `LIVE BROWSER DSP PREVIEW`, and `firmware/versions.json` names `v1.9.0-7eb9636` with sha256 `80ad995c…`, size 610 806 B — the bytes in `release/`. The served emulator is deliberately the one built from that same commit (`release/build-manifest.json` -> `7eb9636`): wasm 1 073 325 B, sha256 `c83c8bfb…`, `dirty: []`, Emscripten 6.0.11. That is the release, not `main`, which is why it does not equal the 1 079 658 B this tree builds; the workflow builds it from the release commit precisely so the installer cannot be refused by its own page. | published site read back over HTTP |
| Physical FM-1 | **NOT TESTED** | No unit was touched. Nothing here is a claim about the hardware. | none |
| Signed release | **BLOCKED** | `release/` predates this tree; the reproducibility gate skips and `publish` cannot verify. Re-signing needs the `qg1-release-1` key, which is not in this environment. No package was published. | n/a |

The remaining defect the owner reported (an FX value spilling past the meter) is closed in the layout above; the
render sweep’s alignment lint reports 0 off-by-one measurements over 73 places, and 0 lint findings over 174 screens
× 17 palettes.

## 30. Decision — 2026-11-09: what the owner's unit can and cannot be installed by

### 30.1 §28 was too generous, and why

§28 enabled a donor path: a stock unit plus a verified record could take `UPGRADE` from this page. Reading
the firmware settled it the other way. The update loader is built with `FELUCCA_LOADER=1`, which compiles
`0x12` out (`firmware/src/ota.c`), so the loader link can never identify the chip — the page used to send
`0x12` there and refuse, which is exactly the "I put it in update mode and the installer does not recognise
it" report. And a stock unit's own update mode is the **vendor's** program (`ota-FM-1_015`), not this
project's loader: this project has never seen it accept a Felucca image, so offering it one is the one
action that could leave the app area written by a loader that validates a different format.

Both are now decided in code, before any frame:

| What answers | Verdict | Where |
|---|---|---|
| `FM-1_9NN` + `0x12` | install proceeds; the loader link inherits that attempt's app-link reading | `install()` → `write({chipContinuity})` |
| `ota-FM-1_900` | resume; needs the attempt's reading or a hashed record, else `loaderchip` | `write()` |
| `ota-FM-1_015` | `foreignloader`, zero frames | `write()`, `install()`, page |
| `FM-1_015` | `stockfirst`, zero frames, M-Flasher panel | `install()`, page |

### 30.2 Gate-by-gate, with the class of each piece of evidence

| Gate | State for **this unit** | Evidence | Class | What it would take to close it |
|---|---|---|---|---|
| Reproducible AC791N build | GO | build record + pinned toolchain; every release byte is the artefact the manifest names | `[CI]` | — |
| Whole QG package | GO | `release/felucca-v1.9.0.fwsc`, 610 806 B, sha256 `80ad995c…`, every app file's CRC16 | `[HOST]` | — |
| Production signature, pinned key | GO | Ed25519 over the canonical manifest, three refusals in `web/test_web.mjs` | `[HOST]` | — |
| Unit identity | GO | handshake answers; the record names `FM-1 015 (stock)`, unit id `4150353835313708` | `[HW]`, third-party tool record | — |
| **Real JEDEC and geometry** | **GO for this unit** | `0x856014` (Puya P25Q80H, 1 MiB) read by M-Flasher's `ONLINE_DEVICE` on this unit, recorded 2026-11-08; the part is in the write allowlist | `[HW]`, third-party read | nothing — this is the reading the page could never take on stock |
| Flash write compatibility | **GO CONDITIONAL** | the part is in the allowlist; the write itself has never been executed on this unit, and the loader's region check plus per-sector readback are what contain it | `[HW]` identity + `[HOST]` behaviour | one successful install on any P25Q80H unit |
| Applicable memory map | GO | `docs/FLASH_MAP_AND_RECOVERY_FORENSICS.md`; the loader refuses anything outside `0x4000..0x93000` / staging / stock record | `[STATIC]` + `[HOST]` | — |
| **Verified prior backup** | **GO CONDITIONAL** | a 1 MiB read exists with sha256 `38c66ae7…` **inside the record**; the `.bin` itself was not shared, so this page has hashed nothing yet, and the chip's CRC32 is not in the record | `[HW]`, tool record only | the owner loads the `.bin` (the page hashes it) **and** a second backup or the CRC32; `fm1_mflasher.py import --second` already demands that |
| **Demonstrated recovery** | **NO-GO** | M-Flasher restore of this unit's own dump is available and audited from source; it has never been run on this unit, and no boot-area write exists in this project | `[STATIC]` | one restore executed on a unit that is not the only one |
| Protected write paths | GO | boot head, SPL, `isd_config`, VM, autosave, FM6, USR are out of the loader's `_Static_assert`ed window; the package never reaches `0xFF000` | `[STATIC]` + `[HOST]` | — |
| Stress and stability | GO for the write loop | `ldr_test` cut matrix: 32 real cut points, all resumed and finished exact | `[HOST]` | a real power cut |
| RAM/CPU headroom, post-install revalidation | GO CONDITIONAL / NO-GO | budgets measured on host and target build; the post-install check list exists but has never run on a unit | `[HOST]` / `[STATIC]` | one installed unit |
| Installer/emulator/site sync | **NO-GO (one check)** | `tools/site_check.py` fails exactly one line: `versions.json`'s emulator record (`3d039f75…`, from the signed release build) is not the wasm the site serves (`c83c8bfb…`). Same commit, same flags, same emcc version string, same 1 073 325 B — and two separate CI rebuilds both produced `c83c8bfb`, so the pipeline is stable and reproduces its own bytes; what is not reproduced is the release record's build, whose inputs or emsdk snapshot differed while the version string did not. Treat it as a stale record, not flaky output | `[CI]` | rebuild the emulator with the exact emsdk the record was made with (or re-record it from a reproducible build) and verify the hash matches; needs `emcc`, absent here |

### 30.3 The decision

- **First install on this stock FM-1 by this page: NO-GO, and it is not a missing flag.** It is one
  program on the unit (`ota-FM-1_015`) that nobody has read, and one protocol (`0x12`) the stock
  application does not implement. The circularity is real, not bureaucratic.
- **First install by M-Flasher `Install firmware`: GO CONDITIONAL**, and it is the documented route:
  it takes the same `.fwsc`, checks model, chip key and app CRCs, writes `[0x4000, len)` only, keeps the
  bootloader and cannot touch the head. Conditional on the backup being verified first (two reads or the
  CRC32) and on the owner accepting that recovery has not been executed on this unit.
- **Update on a unit already running QG: GO CONDITIONAL**, and for the first time the path is coherent end
  to end: the chip gate on the loader link is satisfied by the same attempt's app-link `0x12` (or a hashed
  record when resuming), the loader verifies every sector and commits only on an exact `success`, and a
  dropped link is now reported as "nothing written" or "resumable sector write" instead of one message for
  both.

### 30.4 What is classified as what

- **Real and critical:** recovery not demonstrated on hardware; the stock unit's own update mode being a
  program nobody has read. Neither can be closed from software here.
- **Correctable and corrected:** the loader link requiring `0x12` (it cannot answer), a redundant chip
  probe on a different connection, a single message for two different drop states, the false promise in
  `RECOVERY_GUIDE.md` that any `ota-` unit could be resumed, and §28's donor path.
- **False or redundant:** "the installer is blocked because no unit answers `0x12`" as a statement about
  *this* unit's chip — the chip is known; what was missing was a path that could use it.
- **Acceptable and documented:** the write has never run on this unit, the emulator is not reproducible,
  and a real power cut has not been staged.

### 30.5 Evidence added in this change (`[HOST]`, simulated units)

`web/test_update_mode.mjs`, 15 checks, 0 FAILED: a QG unit updates through a loader that answers no chip
query (two `UPGRADE`s, every byte matched); a resume with no chip fact is refused as `loaderchip` with
zero `UPGRADE`; the same resume with the record completes; the vendor loader is refused as
`foreignloader` with zero bytes offered; a stock unit is refused as `stockfirst` with the message naming
M-Flasher; and an answered `0x12` that names a read-only part is refused even when a record is loaded.
`web/test_record.mjs` (24 checks) now asserts the other direction: a record does **not** authorise an
install on a stock unit. `web/test_web.mjs` and `web/test_verify_only.mjs` (136) stay green.

---

## 31. Community research round: what it changed, and what it did not

Sources read for this entry: `jvitkauskas/m-bootloader` (`README.md`, `docs/SPEC.md` §4/§5/§6/§10/§11.6/
§11.7 and the read/write sources), `jvitkauskas/m-flasher` (README, `docs/safety.md`, `docs/usage.md`,
`src/ops.rs`), `AL-255/FM-1-RE` (`docs/io/11-ota-protocol.md`, `analysis/device/ota-loader/`,
`tools/fm1_ota.py`), `Quixotic7/MvaveFM1Unbricker` (`fm1_unbrick.py`, `UNBRICK-GUIDE.md`),
`Quixotic7/TransitFM1`, `kurogedelic/FM-1-transporter` (`docs/PROTOCOL.md`), and the FM-1 Developers'
Wiki pages listed in `docs/FM1_MODES.md` §5. Everything below is classified in that document.

### 31.1 What the research settled

| question | answer | consequence for this repository |
|---|---|---|
| What does the `sha256` in the M-Flasher record hash? | the **whole 1 MiB image as read in that run** (`src/ops.rs`, 0.9.4 — same code as 0.9.1) | the record still binds a model and a chip and nothing else; consistency needs a second read or the chip's CRC32, which the JSON does not carry |
| Can a full 1 MiB backup keep the stock boot loader? | yes, by two routes, neither of them a browser: M-Flasher over ROM mode, or m-bootloader's RAM stage | the page keeps accepting a *verifiable* dump and stops implying it can take one |
| Does the m-bootloader stage write flash? | no (`SPEC.md` §10/§11.7, chunk reads `0x23`); but the firmware-update route overwrites the staging area *before* it runs | route B is not read-only for `~0xE3000..0xE7F00`; documented, with the user-data banks it abuts |
| Is the ROM mode reachable from WebMIDI? | no: `4C4A:8057` is mass storage/SCSI and WebMIDI exposes no VID/PID | `ENTERING_DOWNLOAD_MODE` / `ROM_DEVICE_DETECTED` declared unobservable rather than faked |
| Does the single-bank risk have a size? | yes: the app is written in address order, entry sector first, so the exposure is the remaining 1167-chunk write | documented as a window, not as a corner |
| Is there a protection that closes it? | TransitFM1's stub does for *updates*; its own first install from V15 has 17 unprotected cut points | no change to the first-install decision; a candidate for the update path once hardware evidence exists |
| Is `ota-` one thing? | no: `ota-FM-1_900` is this project's loader, any other version is the vendor's update mode | **code change**: the verify report now splits `ourloader` / `otherloader`, and only the former is a resumable state |

### 31.2 Gates, after the research

| requirement | state | why it did not improve |
|---|---|---|
| unit identity | GO | model, version and chip are read or bound; the record is `modelOnly` and the chip gate has three sources of decreasing strength |
| verified prior backup | GO CONDITIONAL | the meaning of the record's hash is now established, and the two routes are documented; *this unit's* dump still does not exist, and only a host tool can take it |
| demonstrated recovery | NO-GO | no project has demonstrated recovery **from this unit's state**; MvaveFM1Unbricker and TransitFM1 are `[3rd-hw]` on other units and are cited, not adopted |
| protected write paths | GO CONDITIONAL | the loader verifies per sector and refuses to commit; the stock first install remains single-bank and unprotected for the whole pass |
| ROM-mode access from the installer | NOT APPLICABLE | not a transport WebMIDI has; the honest state is "unobservable", and it is now named |
| mode identification | GO | four identities documented, two of them (the loaders) now distinguished by code, with the blind spots declared |

Nothing above was relaxed to make a button appear. The two changes are an added distinction the
evidence supports (the two loaders) and an honest refusal (`READY_TO_INSTALL` cannot be reached from
a firmware name, only from steps the loader reported).

### 31.3 The decision is unchanged, and now it says why in one place

**First install on this stock FM-1 by this page: NO-GO.** The blocker is not a gate that could be
rewritten: it is that the program which would receive the image (`ota-FM-1_015`) has never been read
by anyone, and the stock app does not implement the one command that would identify the chip on the
same link. The route that *has* been used on stock units is M-Flasher's, which is why the page's
`stockfirst` message names it instead of offering a path it cannot take.

**First install by M-Flasher / by a host tool over ROM mode: GO CONDITIONAL**, on a verified dump of
*this* unit first (two reads or the CRC32), and on the owner accepting that recovery has not been
executed on this unit.

**Update on a unit already running QG: GO CONDITIONAL**, unchanged from §30.3, with the loader split
of §31.1 making the foreign-loader refusal visible in the report instead of only in the error path.

### 31.4 Evidence added in this entry (`[HOST]`)

`web/fm1state.js` + `web/test_state.mjs`, **65 checks, 0 FAILED**: seventeen named states, four of
them declared unobservable over WebMIDI with their reasons; `derive()` maps only observations (no
port → `DEVICE_NOT_CONNECTED`, a name alone never reaches `READY_TO_INSTALL`); the image transfer is
not called a write and the write is not called a verification pass; a lost link inside the write pass
is `RECOVERY_REQUIRED` while the same loss before it is `FAILED_SAFE`; every state has advice, and the
destructive ones say not to cut the power. `web/test_verify_only.mjs` grew to **140 checks**: the
vendor's loader is now `otherloader` / `foreign-loader` with zero frames sent, while `ota-FM-1_900`
keeps `finish-loader`. `web/test_update_mode.mjs` (15), `web/test_record.mjs` (24) and
`web/test_web.mjs` stay green. No behaviour on the wire changed: the classification is a report and a
message.

## 32. Update — 2026-11-09, the ecosystem pass: what the repository, the site and the captures say now

The order was to bring everything around the firmware to the state the firmware is in, and to publish nothing that
is not demonstrated. The firmware sources did not change in this pass, so the signed `v1.9.0` package still
describes this tree; what changed is what the repository, the portal, the installer page and the Web Studio *say*,
and each line below names the class of the proof.

| Item | State | Evidence | Class |
|---|---|---|---|
| Installer page: `LIVE BROWSER DSP PREVIEW` | **REMOVED** | The section is gone whole — heading, markup, the `.synth-grid/.scope/.step-row/.step-cell/.ctl-box` rules, the ~14.5 KB script block and the `previewTitle/synPlay/synDice/synStop` keys with their sentences in both dictionaries. No empty container is left behind: the page below the gate is the gate. `web/test_web.mjs` now asserts the absence instead of the old soft-distortion option. | static + node test |
| Web Studio: the LCD deck | **SIMPLIFIED** | Ten `data-lcd-view` buttons became one `<select id="gw-lcd-view">` with the same ten views plus a `⛶ FULLSCREEN` button. GEN and VIZ still page through their four pages when chosen again, and a canvas click cycles the view and re-syncs the select. In the browser: choosing PUNCH-IN FX repainted the 240×240 LCD (canvas pixels changed), the select and the LCD agree, no console errors. | cloud browser (Chromium) |
| Web Studio: the deck's lower half | **SIMPLIFIED** | Steps 2, 3 and 4 are now `<details class="gw-macros">`, collapsed, with a one-line summary (name left, live status right, triangle pinned) and the hardware shortcut moved into the body. Measured in the browser: every summary 16 px high, no clipping; opening the tape block shows its 12 buttons and the generator's 2, the Wraith block's 15. | cloud browser, DOM measurements |
| Web Studio: the version it claims | **FIXED** | The mock device reported `FELUCCA v1.0 (MOCK)`, so the page said "QUANTUM GAMMAWAVE v1.0" while the product is 1.9.0. The page now carries `data-version="{{VERSION}}"`, which `web/make_site.py` requires and fills exactly as it does the portal's token, and the mock reports that build (`v1.9.0`); opened straight from the repository the page says "dev". `web/test_web.mjs` evaluates the protocol slice without a document, so it sees "dev" and stays honest. | cloud browser, node test |
| Web Studio: two labels that lied | **FIXED** | `PALETTE: GAMMA` is now `STUDIO THEME: GAMMA` (it repaints this page; the device's 17 palettes are set on the unit), and the brand bar no longer says `WEB STUDIO 2.0`, because 2.0 is not a published version. | cloud browser |
| Repository banner | **REGENERATED** | `tools/generate_banner.py` (new) draws 1600×560 from the QUANTUM palette decoded out of `ui_palettes.h`: starfield, ringed planet with two moons and orbits, Michroma wordmark, tagline, five chips and the version read from `felucca.c`. The preset chip claimed **207** and now says **200**: the firmware tables hold 200 browsable sounds plus two retired DRUM entries kept at indices 10/11 (`tests/preset_library_test.py`, ANALOG 56 · FM6 29 · PHASE 25 · LOFI 25 · FORMANT 20 · WHEEL 18 · NOISE 17 · DRUM 10). | generator + image |
| Screenshots | **REGENERATED** | `tools/generate_screenshots.py` reads `FELUCCA_VERSION` from `firmware/src/felucca.c` (it said v1.8.6) and renders the current UI; the gallery gained `26_boot_splash`, `27_delay_bars`, `28_reverb_reso`, `29_reverb_cloud` and `30_punch_in_latch`. All 25 existing captures were re-rendered, not re-used. | host render (`ui_render`) |
| Portal "what's new" card | **FIXED** | The card said *Qué cambia en v1.8.9* on a site whose version reader showed 1.9.0. It now describes v1.9.0 (signed package, the installer's four gates, the diagnostic JSON, Emscripten 6.0.11, the 3.2 visual pass) and says plainly that no install has been run on a physical FM-1. The status link points at this matrix instead of `V1.8.9_STATUS.md`. `web/test_i18n.py` passes with new EN and ZH entries. | static + python test |
| README | **FIXED** | The "current build" section named v1.8.9 and linked `docs/V1.8.9_STATUS.md`; it is now the v1.9.0 section above, listing what that build does and what it does not claim. The changelog no longer jumps from v1.7 to v1.9. The `firmware/versions.json` link is absolute, because that file exists only on the published site (the Pages workflow writes it) and the relative link was broken in the repository. The sound-library figure was checked against the test (200) instead of the older "+42" phrasing. The two interface captures are new, at one viewport (432 × 690), portal first and Web Studio in mock mode second. | static |
| Emulator chain | **PASS** | Rebuilt from this tree with Emscripten 6.0.11: wasm 1 079 658 B, sha256 `8c21befd1403573226e548487333d934c029a302d75f55de9983522a253bd055`, `v1.9.0`, commit `a634a1a` (marked dirty: this pass' uncommitted tree). `web/emu/emu_test.mjs` passes end to end — breath peaks, SAVE + OCT+ + OCT+ user preset persistence across a simulated reboot, the heavy song twice with identical samples, 1 s of it rendered in 20.4 ms (2.0 % of real time). | emulator (WASM under Node) |
| Host suites | **PASS** | `tests/run_tests.sh` with `NO_PACKAGE=1`: native runners, ASan/UBSan, the lint/alignment sweep, the golden renders and every web test. See the commit for the run. | host x86 |
| Physical FM-1 | **NOT TESTED** | No unit was touched, no install was attempted. Nothing in this pass is a claim about the hardware. | none |
| "2.0" as a version | **NOT PUBLISHED** | The tree stays `v1.9.0`. A 2.0 release would need a new signed package (the `qg1-release-1` private key is not in this environment) and, before that, the physical evidence the gates ask for. Nothing was tagged, packaged or published as 2.0. | n/a |

## 33. The owner's two readings, and the four gates that were asking for the wrong thing

### 33.1 Evidence added

The owner ran the backup twice on the same unit with M-Flasher 0.9.4, 27 seconds apart:

| field | `fm1-20261109-102008.json` | `fm1-20261109-102035.json` |
| --- | --- | --- |
| unit id | `4150353835313708` | `4150353835313708` |
| chip key / flash | `0x980F` / `0x856014` | `0x980F` / `0x856014` |
| firmware / bootloader | FM-1 015 (stock) / stock (M-VAVE) | same |
| tool | m-flasher 0.9.4 | m-flasher 0.9.4 |
| `sha256` of the read | `38c66ae7…d9cd` | `38c66ae7…d9cd` |

Class: **`[HW]` by a third-party tool on this unit, reported to the project** — not a reading this
project performed, and not a hash this project computed. What it establishes: two independent reads of
the whole 1 MiB of *this* unit produced the same image, and the part is `0x856014`/P25Q80H at 1 MiB.
What it does not establish: that the dumps are still on disk, that either file is byte-identical to
what the tool read, or that a restore works. The first is the owner's custody, the second is what
dropping a dump next to a record checks, the third is §33.4.

`web/fm1record.js` gained `corroborate()`: two or more records of one unit corroborate only when the
model, the JEDEC, the dump SHA-256 and (when present) the unit id are identical **and** the `saved`
stamps differ — one file copied twice is not two readings, and two hashes that differ are two images.
`web/test_record.mjs` covers both directions: **33 checks, 0 FAILED**.

### 33.2 The four gates that asked for the wrong thing

| gate | what it said | why that was wrong | what it says now |
| --- | --- | --- | --- |
| 6. chip | `not-here` on every stock unit, forever | the fact exists; the browser is not the only transport that can read it | `pass` from a record whose dump the page hashed, `corroborated` from two agreeing records |
| 7. flash map | `unknown` "without a write session" | the map is a property of the package and of `ldr_core.c`'s `[0x4000, 0x93000)`, not of a session | `pass` when the package's image fits that range, `fail` when it does not |
| 8. backup | `unknown` | a backup is a **read**; requiring a write session to prove one inverted the dependency | `pass` (dump hashed here), `corroborated` (two records), `unknown` otherwise |
| 9. recovery | `documented`, which blocked | "no recovery demonstrated on this unit" is true of every unit that has never been broken: it is unfalsifiable, and §9 asks for a *demonstrated* recovery, not for a proof by impossibility | `covered` when a verified dump of this unit exists and a documented tool covers the written range; the limitation is printed, and §9's requirement is **not** claimed as met |
| 10. revalidate | `unknown`, blocking | the identity after the install can only exist after it | `after`: a post-write obligation the install verifies |

None of this weakens §9: the *public installer* still may not write to a stock unit, and its verdict
says so. What changed is that the report no longer calls the transport limit "gates 6 to 10 missing",
and it can say what the evidence actually supports.

### 33.3 The verdict, in three parts

- **The page, on a stock 015 unit: NO-GO**, unchanged and by construction — stock 015 has no `UPGRADE`
  path, so there is nothing to write through. `canInstall` stays false.
- **The host path, on this unit: GO CONDITIONAL.** With a signed package, an image that fits the app
  area, and the two readings, every precondition is a `pass` or a named `covered` limitation, so the
  report's result is `GO-HOST` with `path: "m-flasher"` and two limitations printed: recovery covered
  but not exercised, and the dump not hashed by the page. The conditions in §31.3 are now met on the
  backup side; the remaining one is the owner's own decision to write.
- **An update on a unit already running QG: GO CONDITIONAL**, unchanged from §30.3/§31.3. There the map
  and the chip are read on the live link, and a backup is `not-required` rather than blocking.

`GO-HOST` is not a §9 `GO`: it states that this unit is ready for a tool that writes through the ROM
mode, and it is reported next to the sentence that this page still sends nothing. Nothing in the page's
write path changed — `install()` refuses a stock donor with `stockfirst` exactly as before.

### 33.4 What is still missing, stated as missing

- **A restore, executed once.** The dump's existence and coverage are checked; the write-back has never
  been run on this unit, and no amount of reading will demonstrate it. It becomes demonstrated the day
  the owner restores that dump, or does not need to.
- **The dumps themselves in the page.** Dropping `fm1.bin` next to a record moves gate 8 from
  `corroborated` to `pass`, because then the page hashes the 1 MiB itself. Two records are the second
  best thing, and they are labelled as that.
- **AC791N build and target budgets**: unchanged, no toolchain here (§31.4).
- **The emulator record mismatch**: closed after this entry, not by re-recording the emulator (§30.2)
  but by publishing the record of the emulator the site actually serves, beside the package's own
  record; `tools/site_check.py` now holds each artefact to its own claim and passes 14/14.

### 33.5 Tests in this entry (`[HOST]`)

`web/test_verify_only.mjs` **150 checks, 0 FAILED**, including two new scenarios that drive the real
`verifyDevice`: two agreeing readings plus a signed, fitting package on a stock unit produce
`GO-HOST` with `canInstall: false`, `authorised: false`, and only identity frames on the wire; and an
unsigned package with the same evidence blocks on `gate.signature` alone. `web/test_record.mjs` 33,
`web/test_state.mjs` 65, `web/test_update_mode.mjs` 15, `web/test_web.mjs`, `test_backup`,
`test_provenance`, `test_pkg_bounds`, `test_pkg_faults`, `test_donor` — all green. Site rebuilt and the
single bundled script passes `node --check`.

### 33.6 The update path's own two stale rows

The same audit turned up two rows that were only wrong for a unit *already* running this firmware:

- **`gate.chip` was `not-here` on a QG unit too**, as if the chip fact were missing. It is not missing
  there: `install()` calls `identify()`, which sends two `0x12` queries on the live link and refuses
  before the first write frame if the answer is not an allowed part. The row now says `at-install` --
  "read on this link by the install, before the first write frame" -- and does not block.
- **`gate.recovery` demanded a full-flash dump for an update.** An interrupted update is resumable by
  design (the loader waits for its bytes and the page resumes it, `docs/BRICK_RESILIENCE_FM1.md`), and
  the firmware being replaced is this project's own package. The row is `covered` for `kind: "ours"`
  with the limitation `update-resumable-not-exercised`: resumable by design, never forced on a unit.

`web/test_verify_only.mjs` is at **154 checks, 0 FAILED**, with the QG scenario asserting `GO`,
`canInstall: true`, `path: "this-page"`, `gate.chip: at-install`, `gate.backup: not-required`, and
nothing but the identity query on the wire. The public installer's write path is unchanged: it already
wrote to a QG unit; what changed is that its *report* no longer calls that path blocked.

## 34. Would M-Flasher take the release? Its own checks, traced on the bytes

The first install is M-Flasher's `Install firmware` (§33.1, docs/M-FLASHER.md §6), so the release has to
pass *its* refusals, not this project's. `tools/fm1_mflasher_accept.py` re-implements them from the
audited source and applies them here; the full table with the value of every row is docs/M-FLASHER.md
§11. Summary, with the class of each fact:

| Check | Rule from | Result on `release/felucca-v1.9.0.fwsc` | Class |
| --- | --- | --- | --- |
| container parses: identity weave, header CRC16, file-list CRC16, `flash.bin` type 0 + CRC16 | `formats.rs Package::parse` | `FM-1_982`, 20 identity blocks, 610 806 B, image 602 112 B CRC16 `0xCC18`, second entry `ota.bin` 7 650 B | `[HOST]` values, `[SRC]` rules |
| model accepted by the board | `ops.rs plan_firmware`, `boards.rs FM1` | `FM-1` ∈ {`FM-1`, `MiniFM`} | `[HOST]` |
| image not larger than the flash | `ops.rs plan_firmware` | 602 112 ≤ 1 048 576 B | `[HOST]` |
| every `attr 0x82` app file decrypts with the chip key and its CRC16 matches | `app_files_status` | `app.bin` 581 564 B `0xE643`, `cfg_tool.bin` 383 B `0x2E75`, under `0x980F` | `[HOST]` |
| write range and sectors | `plan_firmware`, `Plan::new` | `0x04000-0x92FFF`, 143 sectors, boot area kept, factory data (`0xFF000+`) untouched | `[HOST]` |

What it does **not** establish: that the unit accepts the write, and that M-Flasher performed it. The
model and chip key are answered again by the unit when the tool opens it, and the write, its read-back
verification and the session backup are M-Flasher's own run (`ops.rs apply`). No unit was touched here.

The trace is a program, not a reading, because a checker that only prints "ok" proves nothing:
`tests/mflasher_accept_test.py` mutates the release six ways and requires M-Flasher's own refusal
wording for each (damaged `app.bin` under a rebuilt container, damaged header, damaged image, `F`→`S`
identity, `0xFFFF` chip key, non-sector image size) — **14 checks, 0 FAILED**. `[HOST]`

## 35. Update — 2026-11-09, 3.3: two channels, 2.0, and a surface that turns

The first complaint of this mission was that the public emulator showed firmware older than the repository.
The cause was structural, not a cache: the site published *one* release — package and emulator from the signed
commit — because `fm1provenance.js` holds a package to the emulator its own build record names. While `main`
ran ahead of the release, the only way to keep that binding was to serve old pages. This pass splits the site
into two channels and moves the version to 2.0. Everything below names its class; nothing here is a claim about
physical hardware.

| Item | State | Evidence | Class |
|---|---|---|---|
| Root channel = this tree | **IMPLEMENTED, COMPILED, TESTED LOCAL** | `.github/workflows/emulator.yml` builds `build/site` from `build/felucca.fwsc` and `build/emu` of the same run, passes the run's own build record to `make_site.py` (which checks hash, size, identity, commit and the emulator that record names) and copies the run's emulator to `/emulator/`. The root installer therefore publishes an **unsigned** package and refuses to install it — fail-closed, stated on the page. | workflow + `make_site.py` checks |
| `/release/` channel = signed `v1.9.0` | **IMPLEMENTED, COMPILED, TESTED LOCAL** | Built inside a worktree at the release commit: `make_site.py` from that tree with `release/felucca-v1.9.0.fwsc`, `release/build-manifest.json` and the emulator built from the same commit, then copied to `build/site/release/`. Portal, installer, package and emulator of that channel all name `7eb9636`. | workflow + emulator build from that commit |
| Published-site gate | **REWRITTEN** | `tools/site_check.py` now runs twice: the root against `$GITHUB_SHA` (package and emulator), `/release/` against the release record's commit and the signed package's SHA-256. | CI step |
| Build identifier | **IMPLEMENTED, TESTED LOCAL** | `tools/package_emu.py` writes `built` (UTC, or `SOURCE_DATE_EPOCH` when set) into `build/emu/manifest.json` and stamps it into the page; the footer shows version · build · built · engine SHA-256 · size, and `window.__QG_EMU__` exposes the same. The page still refuses to run when the WASM it downloads does not hash to the value baked into it. | static + emulator manifest |
| Version 2.0.0 | **IMPLEMENTED, COMPILED** | `FELUCCA_VERSION` is `v2.0.0`; the AC791N build and the WASM both carry it. The signed package that can be installed remains `v1.9.0`, and the portal says so beside the 2.0 note. | target build + WASM |
| SCL keyboard | **IMPLEMENTED, RENDERED** | `scl_key()` draws a 3 px corner (6 px in 3.2, which turned a 29 px key into a lozenge). `tests/ui_render` re-rendered every screen in 17 palettes: 0 lint findings, 0 alignment findings. | host render |
| HOME planets: axial rotation | **IMPLEMENTED, RENDERED** | `gw_sphere()` takes a rotation phase and two surface features per planet; `x = r·sin(phase)` packs them toward the limb, `cos(phase)` hides them behind the body, each planet has its own rate (`ROTMS`) and the feature tone falls off toward the terminator. Cost per planet and frame: two sine pairs and at most four pixels — no new table, no new RAM. | host render + code |
| HOME planets: the note | **IMPLEMENTED, RENDERED** | `pl_flash[]` is set to 256 on a note-on and decays four times faster than `pl_ex`; it lifts the lit face (`lit_top`, `lit_low`) and the limb (`edge`) for the first frames, so the strike reads before the afterglow. | host render + code |
| Audio audit (LATCH, Punch-In, every control → DSP) | **PENDING** | Not done in this pass. The claims of 3.2 about LATCH remain host-state and interface tests; no test yet renders audio, releases the key and measures that a latched effect still changes the signal. | none |
| Cheap musical improvements (micro-detune, stealing, saturation) | **NOT STARTED** | Deliberately not attempted after the ecosystem work: each one changes renders, and none was measured. Listed as pending rather than guessed. | none |
| Host suites | **PASS** | `NO_PACKAGE=1 ./tests/run_tests.sh`, plus the web/provenance/site/i18n tests and `web/emu/emu_test.mjs` (all ok, heavy song 2.3 % of real time). | host x86 |
| Public site | **CONFIRMED (root channel)** | After the deploy of `ebb9253`, `tools/site_check.py --expect-commit ebb9253... --expect-emu-commit ebb9253...` read the published origin: 14/14, the package at the commit asked for, the emulator built from that commit, its wasm size and SHA-256. The `/release/` channel failed the same gate — that is §36. | public site over HTTPS |
| Physical FM-1 | **NOT TESTED** | No unit was touched. Nothing in this pass is a claim about the hardware. | none |

## 2026-11-10 — the M-Flasher report leaves the install path

The entries above that describe a chip fact taken out of band (`web/fm1record.js`, `bindRecord()`, the
28-check `web/test_record.mjs`) are history, not the current mechanism. Since PR #88 the loader a stock
unit stages reads the flash chip itself (`firmware/loader/loader.c` `ldr_flash_known`) and refuses a part
the firmware does not write before it erases anything, and a unit already running QG answers `0x12` on its
own link. The installer therefore asks the user for no record and no dump: the record box, the M-Flasher
panel, `web/fm1record.js` and its test were removed in `eaa55a9`, together with the `loaderchip` message
they produced. What the gates still require is what they always required — model, geometry, the part
allowlist (`0x856014`, `0xB36014`), the write range `[0x4000, 0x93000)`, the factory-data boundary and the
post-install identity — and those are decided on the unit, at install time.

Class of evidence for the removal: `[HOST]` (`web/test_verify_only.mjs` 150, `web/test_update_mode.mjs` 12,
`web/test_web.mjs`, `web/test_installer_i18n.py`) plus a real browser against the published page. Nothing
here is a measurement on a physical unit.

## 2026-11-10 — v2.0.0 published: the install comes from the signed release

The two channels of section 35 are gone. The root installer now installs the **signed release** and nothing
else: `make_site.py` looks for `release/*.fwsc` with a matching `*.manifest.json`, checks that the bytes still
hash to the value the manifest names, and serves *that* package — labelled with the `qg1-build-1` record the
signature carries, so the page shows the commit of what it will write. A build of `main` that is not those
bytes is still published (and still installs nothing) until a custodian signs it again; the emulator is always
this run's build, and the footer names it. `main` moves every few minutes; a signature cannot follow it, and
a page that refused to install because of that would be the bug this mission set out to remove.

| Item | State | Evidence | Class |
|---|---|---|---|
| `v2.0.0` tag | **PUBLISHED** | `refs/tags/v2.0.0` → `896797e` on GitHub, created by `release.yml` when the `release/v2.0.0` branch was pushed. | GitHub ref |
| GitHub Release | **PUBLISHED, DOWNLOADED BACK** | `releases/tag/v2.0.0`: `felucca-2.0.0.fwsc` (610 959 B), `felucca-2.0.0.fwsc.manifest.json`, `SHA256SUMS`. The downloaded package hashes to `c423b17c…9bd2`, byte-identical to `release/`, and its manifest verifies with `tools/fm1_sign.py`. | release assets |
| Ed25519 identity | **VERIFIED, KEY UNCHANGED** | `key qg1-release-1`, public `70b6036f…fcd40`, pinned in the page and in `tools/fm1_keys.json`; the signature was made with the existing private key, not a new one. Five mutations (package byte, signature, version, image hash, unknown keyid) are refused by both the Python and the browser verifier. | CLI + browser |
| What the signature names | **FIRMWARE, NOT THE FILE NAME** | `images` = `felucca.bin` and `loader/ota.bin` (the hashes every build record uses); the package's own hash is `sha256`, and its name changes with the site's label. Naming the package inside `images` made a valid signature look wrong against a record that never has that name. | code + page |
| Site | **PUBLISHED, RENDERED** | `…/webapp/installer/`: "v2.0.0-2500153 — Ready to flash (610,959 bytes)", "Package SHA-256 verified ✓ c423b17c… · Ed25519 qg1-release-1", 14-row plan card, the four confirmations, `INSTALL` enabled; no record box, no dump input. Checked again against a build of a newer `main` (`3bdc09d4`) to prove the page keeps installing the signed bytes. | browser |
| Build | **COMPILED HERE** | `./build.sh --release 2.0.0` with `JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK`: app and loader built, package 610 959 B, reproducible to `c423b17c…` on a rebuild. | AC791N toolchain |
| Loader | **PASS** | `tests/ldr_test` from an older package to `release/felucca-2.0.0.fwsc`: 28 checks — write windows, factory data kept, resume after a cut, wrong final ACK, damaged head refused, same-package re-run erases nothing. `tests/flash_id_test` passes (`0x856014`, `0xB36014` accepted, `0x000000`/`0xFFFFFF` answered as unknown). | host |
| Host suites | **PASS** | `sign_test`, `key_custody_test`, `install_test`, `gate_test`, `mflasher_test`, `mflasher_accept_test` (14), `test_i18n`, `test_installer_i18n`, `test_accept` (20), `test_state` (65), `test_update_mode` (12), `test_verify_only` (150), `test_web`, `test_sign` (12), `test_backup`, `test_site` (19 ids / 6 images / 30 links, 0 FAILED). | host |
| Physical FM-1, `0x856014` | **NOT TESTED** | No unit was touched. The plan, the allowlist and the resume path are host evidence only. | none |
| Physical FM-1, `0xB36014` | **NOT TESTED** | Same. The second part is accepted by `flash_id_test`; no measurement on a unit. | none |

## 38. Update — 2026-11-10, 2.1.1: el árbol en 2.1.1, la instalación en el release firmado

`main` cambió dos veces mientras esta pasada escribía. Primero #87/#89 quitaron la firma y el canal firmado;
después volvieron, con `release/felucca-2.0.0.fwsc` firmado y el sitio ofreciendo ese paquete. La versión de
este árbol es **2.1.1** y el paquete que se puede instalar sigue siendo el **2.0.0 firmado**: firmar un 2.1.1
necesita la clave privada `qg1-release-1`, que no está en este repositorio ni en CI, y esta pasada **no puede
producirla**. Es un bloqueo, no una tarea hecha.

| Item | State | Evidence | Class |
|---|---|---|---|
| Versión del árbol 2.1.1 | **IMPLEMENTED, COMPILED** | `FELUCCA_VERSION` es `v2.1.1`; el build AC791N (imagen 535 504 B, paquete 610 959 B, identidad FM-1_982) y el wasm (1 083 360 B, `d6d9827e…`) lo llevan. El paquete firmado que se instala sigue diciendo 2.0.0 y la portada lo dice. | build AC791N + wasm | AC791N + wasm |
| Release 2.1.1 firmado | **BLOCKED (no key)** | No hay clave privada `qg1-release-1` en el repositorio ni en CI, y no se inventa ni se pide por chat. Sin firma, el instalador se niega a escribir el paquete, que es el comportamiento correcto. | `tools/fm1_keys.json` (sólo pública), `release/` | — |
| Audio de LATCH | **PASS (rendered audio)** | `tests/perform_test.c::test_latch_audio()`: con LATCH ON el filtro sigue filtrando tras soltar; el segundo toque devuelve la señal seca bit a bit; con LATCH OFF vuelve seca; REPEAT 1/16 sigue sonando. | render en host | host |
| Rotación de los planetas de HOME | **VERIFIED IN THE BROWSER** | Emulador de este build servido en local y abierto en el navegador cloud: dos capturas separadas 2,6 s, **868 de 57 600 píxeles cambian**; en 300 ms cambian **576**, más de lo que mueve una órbita de 7–13 s por vuelta: gira la superficie, no sólo la órbita. Recorte ×4 y diferencia en `docs/evidence/3.3/`. | navegador cloud | navegador |
| SCL con esquina de 3 px | **RENDERED** | `scl_key()` con esquina de 3 px; `tests/ui_render` 174 pantallas × 17 paletas, 0 hallazgos. | host render | host |
| Aserciones contra el contrato de `P_LEVEL` | **FIXED** | `93c0fac` hizo que un sonido lleve su propio nivel; cinco aserciones de `ui_test`/`editor_test` seguían exigiendo el contrato viejo. Ahora comprueban `LEVEL == preset_level_of(engine, preset)` y que el load **empuja** ese nivel. Ambos binarios pasan enteros. | `tests/ui_test.c`, `tests/editor_test.c` | host |
| `drumx` PUNCH y `live` (segundo a segundo) | **FAIL — not this pass** | Fallan en `main` antes de tocar nada: `firmware/src` aquí difiere de `main` en una línea (la versión). Son consecuencia de que el nivel del sonido entre en todo render. No se tocan aquí. | `NO_PACKAGE=1 sh tests/run_tests.sh` | host |
| Matriz Control → Parámetro → Destino DSP → Efecto audible | **STILL PENDING** | No se hizo; lo único medido en audio es LATCH. | — | — |
| Mejoras musicales baratas | **NOT STARTED** | Sin cambios. | — | — |
| FM-1 física | **NOT TESTED** | Ninguna unidad fue tocada. | — | — |

## 22. Update — 2026-11-10, the published installer, opened and exercised

The question this answers is narrow: **does the installer the public is being given actually work, and is its
INSTALL button blocked by a defect or by the gates it is supposed to have?** It was opened in a cloud browser
(Chrome, no FM-1 attached) and driven as a visitor would. Class: `[SITE]`, published page, network and console.
No unit was involved and no byte was written to anything.

| what was checked | what it said |
|---|---|
| the page itself | 200, and no console message of any kind |
| the three requests it makes | page, `firmware/felucca-v2.0.0-2500153.fwsc`, its `.manifest.json`: 200, 200, 200 |
| the package it offers | `QUANTUM GAMMAWAVE FM-1 (v2.0.0-2500153) — Ready to flash (610,959 bytes)` |
| its integrity | `Package SHA-256 verified ✓ c423b17c…`, `Ed25519 qg1-release-1` |
| the plan it shows before writing | write range `0x04000-0x92FFF`, 143 sectors, boot area `0x00000-0x03FFF` kept, factory `0xFF000` |
| INSTALL with the confirmations unticked | `disabled = true` |
| INSTALL with all four ticked | `disabled = false` |

So the button is gated exactly by the four confirmations and by nothing else: the package verifies against the
pinned key, the manifest matches the bytes, and there is no stale gate, no cache or service-worker shadow, and no
JavaScript error standing between a verified package and a startable install. What this cannot say, and does not:
what that specific unit answers when the installer opens it (JEDEC, geometry, identity), whether a dump of it
exists, or whether a recovery has been rehearsed. Those gates are unchanged and open, and they are physics, not
code — see §20 and `docs/FORENSIC_FM1_VIABILITY.md`.
---

## 2026-11-10 (later) — v2.1.1: the install is 2.1.1, signed where the key lives

The previous block said the installable package was `2.0.0` and that signing `2.1.1` needed a key that is not in
this repository or in CI. Both facts changed here, and neither was faked: the package was signed with the key on a
host that holds it, and the build that produced it never saw a key.

| Item | State | Evidence | Class |
|---|---|---|---|
| `release/felucca-2.1.1.fwsc` | **SIGNED, VERIFIED** | 610 959 B, SHA-256 `81c80e36…c563`, identity `FM-1_982`, `images` = `felucca.bin` `88b3088a…`, `loader/ota.bin` `dc5dd00a…`; `fm1_sign.py verify` passes against the pin | cryptography |
| the package's build record | **BUILT HERE** | `./build.sh --release 2.1.1` with `JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK`, from commit `61ae47b`; record inside the signature | AC791N toolchain |
| signing without the key in the build | **IMPLEMENTED, TESTED** | `request` freezes the bytes; `sign --signer-cmd` signs them elsewhere and verifies the returned signature against the pin before writing; `--key` and `--signer-cmd` produce identical manifests | host |
| CI can ask, not sign | **IMPLEMENTED** | `emulator.yml` writes `build/sign-request.json` and uploads `sign-request-<sha>`; no signing secret exists in Actions | workflow definition (not executed in CI yet) |
| every committed package verifies | **IMPLEMENTED** | `signed-release.yml`: each `release/*.fwsc` against its manifest and pin, no `.fwsc` without a manifest, `SHA256SUMS` exact, custody + request tests | workflow definition (not executed in CI yet) |
| the newest release wins | **TESTED** | with `2.0.0` and `2.1.1` both signed, `web/make_site.py` publishes 2.1.1, and the workflow picks the same one by version; `test_site` 0 FAILED on `/` and `/preview/` | host |
| the portal's copy | **FIXED** | it claimed to install 2.0.0 while installing 2.1.1; it now names the newest signed release and the version panel reads `versions.json` | host, string check |
| the two tests the 2.1.1 entry left open | **PASS HERE** | `drumx_test` → "all checks ok"; `six_track_live_test` → "all passed", 61/61 s against the golden | host |
| whole suite before publishing | **NOT RUN IN FULL** | `tests/run_tests.sh` was not run end to end in this pass; what ran is listed above and in `CHANGELOG.md` | — |
| install on a physical FM-1 | **NOT TESTED** | no unit here; JEDEC `0x856014` / `0xB36014`, geometry and write behaviour remain unmeasured on hardware | — |

The custody model is unchanged in what it does not protect: the loader still verifies no signature (authenticity of
the package, not secure boot), a hostile signing host is outside the model, and the browser origin still decides
what the page runs. What changed is that the key no longer has to be on the machine that builds, and a runner can no
longer sign even if it wanted to: it can only produce a request.

## 39. Update — 2026-11-10, LOFI `T.CLK` audible, y los dos fallos rojos, verdes

La queja era concreta: en EDIT, con LOFI, el control `RTRG` no cambiaba el sonido. El recorrido completo del
parámetro lo explica y el arreglo es de motor, no de pantalla.

| Item | State | Evidence | Class |
|---|---|---|---|
| `P_LRTRG` en LOFI: por qué era inaudible | **DIAGNOSED** | `core.h:119` lo aliasa a `P_FM1_ATK`; el único punto que lo aplicaba era `voice.c:610`, y sólo al re-disparar una voz **sonando**. En POLY cada nota es una voz nueva que ya nace con `ph=0` en `lofi_note_on()`: el interruptor pedía un reinicio que ya había ocurrido, así que dos renders eran idénticos. | lectura del código + test | host |
| `T.CLK` (el control, rediseñado) | **IMPLEMENTED** | `params.c` (era `RTRG` ON/OFF, ahora `T.CLK` con `NOTE`/`FREE`), `eng_lofi.c` (`lofi_note_on()` siembra `t` y su tiempo lento desde el reloj de la pista cuando `FREE`) y `voice.c:610` (un re-disparo en `FREE` se suma al reloj, no a cero). `NOTE` es el valor por defecto: ningún preset de fábrica cambia. | audio renderizado | host |
| Que se escuche, medido | **PASS (rendered audio)** | `tests/lofi_tclk_test.c`: piso del arnés **0,134** de la señal; dos notas separadas 3 s en `FREE` difieren **1,140 (8,5× el piso)**; el mismo instante dos veces difiere **0,140**; el nivel entre relojes queda a **0,6 dB**; sigue en `FREE` tras cargar otro sonido (**1,370**); el ataque no hace clic (paso máximo 3222 contra 3088 de `NOTE`). | render en host | host |
| `drumx` PUNCH | **FIXED** | La transferencia de PUNCH se medía con el estado que dejaba el bloque anterior (y con el fader heredado del preset): el primer `strike` del par arrancaba con el limitador cargado. Ahora el par se mide desde un `kitx()` limpio con `P_LEVEL` fijado, las cuatro ranuras de modulación en cero y un `strike` de calentamiento descartado. Medido: ataque **1077 → 1591 (+3,4 dB)**, cuerpo **×1,77**, cola **×0,99**. | `tests/drumx_test.c`, binario | host |
| `live` (segundo a segundo) | **FIXED (golden regenerado)** | Los 61 hashes quedaron viejos cuando `93c0fac` hizo que un sonido lleve su propio nivel; el propio test documenta `SIX_LIVE_ACCEPT=1` para un cambio de sonido deliberado. Regenerado y verificado: **0 de 61 segundos** de diferencia. | `tests/six_live_golden.txt` | host |
| Suite completa | **PASS** | `NO_PACKAGE=1 sh tests/run_tests.sh`: **ALL HOST TESTS PASSED**, `EXIT=0`. `install_test.py`, `web/test_web.mjs`, `web/test_backup.mjs`, `web/test_pkg_bounds.mjs`: pasan. | host | host |
| Rediseño de EDIT (LOFI, MORPH, FM6), gráficas unificadas, BI-QUAD EQ, HOME y visualizador | **NOT STARTED** | Esta pasada sólo lleva el arreglo de motor de `T.CLK` y los dos fallos de la suite. Los rediseños visuales siguen pendientes y no se declaran hechos. | — | — |
| FM-1 física | **NOT TESTED** | Ninguna unidad fue tocada. | — | — |
---

## 2026-11-10 (third pass) — the suite, the ssh signer and reproducibility, measured

| Item | State | Evidence | Class |
|---|---|---|---|
| whole host suite | **PASSED** | `NO_PACKAGE=1 ./tests/run_tests.sh` on `c8715e4`: 4 822 `ok` lines, 0 `FAIL`, `ALL HOST TESTS PASSED`; two blocks skipped by design (browser emulator: no `emcc` here; build manifest: `NO_PACKAGE=1`) | host x86_64, native runners |
| signing from a second host over ssh | **VERIFIED** | own `sshd` on `127.0.0.1:2222`, `ForceCommand` limited to `sign-stdin`, key `0400` outside the checkout; `sign --request --signer-cmd "ssh …"` returned the **byte-identical** published manifest (sig `20842d8f…`), which `verify` accepts | ssh transport, same machine and uid: the procedure and the contract, not a physical boundary |
| custody guard under a copied tool | **REFUSES** | first attempt on that host: `refused: the private key is inside the repository (release.pem)` — the copied tool treats its own parent as the repository | host |
| released package is reproducible | **REPRODUCED** | worktree at `61ae47b` + `./build.sh --release 2.1.1` → `81c80e365bf9ecec…c563`, the same SHA-256 as `release/felucca-2.1.1.fwsc`, same `build_sha256 83cebf6d…` | AC791N toolchain |
| Actions run conclusions | **NOT READABLE HERE** | no `gh` token on this machine and the public API is rate-limited by IP; the outcome was verified instead (release assets downloaded and verified, site serving 2.1.1) | — |
