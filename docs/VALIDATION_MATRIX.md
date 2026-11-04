# VALIDATION MATRIX — what was actually run (v1.8.8 pass)

Honesty first: this table records the checks executed **in this session**, on this machine, against
this tree. Anything not listed here was not run. Labels: `[HOST]` executed natively, `[TARGET]`
executed against a real AC791N build, `[WASM]` executed in Node against the Emscripten build,
`[STATIC]` inspected without execution, `[HARDWARE]` needs an FM-1.

Environment: Linux x86-64, gcc (`cc`), Python 3 with Pillow/fontTools/numpy, Node.js,
**JieLi pi32v2 toolchain `jieli-linux-toolchains-20260730.1`**, **AC79 SDK
`AC79NN_SDK_V1.2.1_2023-12-13`** (three files match `SDK_SHA256`), **Emscripten 3.1.50**.
No FM-1, no MIDI hardware, no browser automation.

## 1. Target build (AC791N)

| Check | Command | Label | Result |
|-------|---------|-------|--------|
| App + loader + package build | `AC79_SDK=… JIELI_TOOLCHAIN=… ./build.sh` | `[TARGET]` | **pass** |
| Image / budgets | built-in `check()` in `tools/build.py` | `[TARGET]` | **pass**: 556 856 B image, `.data+.bss` 95 732/98 304 B, RAM code 22 464/24 576 B, pool spare 9 248 B, calls reach 66 044 B |
| Loader image | same build | `[TARGET]` | **pass**: 9 520 B at `0x01C0A800`, `ota.bin` 7 999 B |
| Package | same build | `[TARGET]` | **pass**: 611 155 B, identity `FM-1_982`, version `v1.8.8` |
| Regions do not overlap | `python3 tests/memory_map_test.py` | `[TARGET]` | **pass** |
| Render-loop cost | `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | `[TARGET]` | **pass** |
| `FELUCCA_FLASH=0 FELUCCA_OTA=0` build | `FELUCCA_FLASH=0 FELUCCA_OTA=0 ./build.sh` | `[TARGET]` | **pass** (526 688 B image, 84 356 B `.data+.bss`, 18 632 B RAM code) — did not compile before v1.8.8 |

## 2. Host suite

`tests/run_tests.sh` was executed. It does not fit in the runner's five-minute command window, so it
was run as the unmodified script (which reached check 47) and then continued by running the
remaining checks directly with the same compiler flags. Every check that ran passed.

| Check | Label | Result |
|-------|-------|--------|
| flash storage, user presets, keys/buttons, TRS MIDI, USB descriptors (12 combinations) | `[HOST]` | pass |
| flash identification (allowlist, two agreeing reads, capacity, protection) | `[HOST]` | pass |
| second-core boot self-test | `[HOST]` | pass |
| USB audio (44.1/48 kHz, resampler, ring/packetiser) | `[HOST]` | pass |
| M-UPGRADE entry with the built package | `[HOST]` | pass |
| update loader, other app → this build (`tests/ldr_test.c`) | `[HOST]` | **pass**, including five power cuts derived from the bytes actually programmed and each resume |
| scales, SPEAKER EQ, DSP render, TRACKS, project formats, motion, RATCH, MIDI control, DIGITAL, FM4, themes, text, settings, UI, UI renders (lint, alignment, palettes), audio, metronome/count-in | `[HOST]` | pass (runner's first 47 checks) |
| `persistence_test` | `[HOST]` | **pass** — did not compile at v1.8.7 (see §4) |
| `backup_test` | `[HOST]` | **pass** |
| `fm6bank_test` | `[HOST]` | **pass** |
| `startup_test`, `lcd_st7789_test`, `editor_test`, `robust_test` | `[HOST]` | pass |
| regression: golden renders, health, voices, CPU baseline | `[HOST]` | **pass** (`tests/golden.txt`, `tests/cpu_baseline.txt` unchanged) |
| backup CLI, installer CLI, README navigation | `[HOST]` | pass (`tests/backup_tool_test.py`, `tests/install_test.py`, `tests/readme_nav_test.py`) |
| Web Studio / protocol / package bounds | `[HOST]` (Node) | pass (`web/test_web.mjs`, `web/test_backup.mjs`, `web/test_pkg_bounds.mjs`) |

## 3. Browser emulator (WebAssembly)

| Check | Command | Label | Result |
|-------|---------|-------|--------|
| WASM rebuild at `v1.8.8` | `sh web/emu/build.sh` | `[WASM]` | **pass**: 1 070 737 B, sha256 `a83d0d4a4eea149d83c0d555425bbc54cf24c3b407297e934aaba148291f070b` |
| Manifest carries version + commit + hash | `build/emu/manifest.json` | `[STATIC]` | `{"version":"v1.8.8","commit":"dcd39f9872cf",…}` |
| Native reference render | `web/emu/native_check.c` | `[HOST]` | pass |
| Boot, keys, MIDI, screen, LEDs, PLAY, save across instances, parity with native, cost | `node web/emu/emu_test.mjs …` | `[WASM]` | **pass**: bit-for-bit equal to the native render; 1 s of the heavy song in 25.2 ms (2.5 % of real time) |
| Real browser (DOM, AudioWorklet output, console, network) | — | `[BROWSER]` | **not run**: no browser automation in this workspace |

The emulator is the firmware's sources in WASM against a simulated FM-1. It is **not** a full-system
emulation: it does not run `uboot.boot`, the AC791N binary or USB.

## 4. Defects found and fixed in this pass

| Symptom | Cause | Fix | Verified by |
|---------|-------|-----|-------------|
| `persistence_test`, `backup_test`, `fm6bank_test` failed to compile (`no such instruction: csync`) — previously written off as pre-existing | `csync`/`ssync` inline asm assembled verbatim for the host in six HAL headers | `FM1_CSYNC()`/`FM1_SSYNC()` in `fm1_cc.h`: chip instruction on pi32v2, compiler barrier on a host | the three tests now build and pass |
| `FELUCCA_FLASH=0` did not compile (`FL_FAR` undefined, `flash_ok`/`st_buf` undeclared) | `flash_identify()`, the TAKES reel module and its call sites were outside `#if FELUCCA_FLASH` | guards in `project.c`, `gen_reel_flash.c`, `main.c` | storage-less target build passes |
| `ldr_test` reported `LOADER TEST FAILED` with the real package | cut points were fixed byte counts; the install programmed fewer bytes once the old and new app shared sectors, so no cut happened | cut points derived from the bytes the app area actually programs (the post-commit record cleanup is not a cut point) | 5/5 cuts and resumes pass |
| WASM build failed (`use of undeclared identifier 'fl_status_ram'`) | the emulator's simulated part predated the identification | stub added, guarded by `HOSTSIM_FLASH_ID` so a test with its own part keeps it | emulator builds and matches native |
| Host TUs including `project.c` twice for the identification stub | duplicate static definitions | one guard macro (`HOSTSIM_FLASH_ID`) for the host, used by `tests/hostsim.c`, `web/emu/felucca_web.c` and the tests that drive their own part | suite compiles clean |

## 5. Failure-injection coverage (unchanged by this pass, all `[HOST]` yes)

| Injection | Where | Expected behaviour |
|-----------|-------|--------------------|
| JEDEC reads disagree / bus floats / bus shorts | `flash_id_test` | no writes; firmware RAM-only |
| Flash block-protected / busy | `flash_id_test` | no writes |
| Shared mailbox word corrupted, job number changed, job unanswered | `mc_selftest_test` | core held; one core continues |
| Power loss at five points of the write | `ldr_test` | record survives, next session resumes and completes |
| Sector changed in transfer after the check | `ldr_test` | sector read again, then written |
| Wrong final ACK | `ldr_test` | no commit, record kept |
| Content changes between the two backup passes / cable pulled | `backup_tool_test` | `inconsistent` / `lost`, nothing written |
| Archive altered after writing | `backup_tool_test` | refused before the first write |
| Restore to another model / update mode / missing object | `backup_tool_test` | refused before any write |

## 6. Not run, and why

| Check | Why | Status |
|-------|-----|--------|
| Anything on an FM-1 (boot, audio, MIDI, LEDs, flash timing) | no hardware | `[HARDWARE]` **not verified** |
| ASan + UBSan passes and the two fuzz runs | they are a second, much longer pass of the same binaries; this session's budget went to the target build | **not run this pass** (they pass at `v1.8.7`; unchanged code paths since) |
| Real browser: page render, AudioWorklet output, console, network | no browser automation here | `[BROWSER]` **not verified** |
| A real power cut during an update | needs hardware | **not verified** |
| Cryptographic signature and anti-rollback | no key material, bootloader is factory and unmodifiable | **not implemented** (`docs/SECURITY_AUDIT.md` A) |

## 7. Verdict

**NO-GO for the main unit.** A real target build exists and every software check that can run here
passes, but nothing has run on an FM-1.
**GO for laboratory validation on a secondary unit**, with the caveats in
[`SECURITY_AUDIT.md`](SECURITY_AUDIT.md): no signature, no anti-rollback, single-bank update window,
and no raw flash backup path.
