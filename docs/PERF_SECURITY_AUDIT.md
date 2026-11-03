# Performance and security audit — branch perf/security-hardening

Scope: the firmware in this branch, the host harnesses, the target build and the installer/package
code. The branch starts after the 1.7 UI/preset work; no bootloader, flash layout or update loader was
modified. **No operation was run on a physical FM-1.**

## Status labels

- **IMPLEMENTED**: source change reviewed and built.
- **EXECUTED ON HOST**: native test or benchmark ran on this machine.
- **EXECUTED ON EMULATOR**: WASM harness; not repeated after these DSP edits.
- **TARGET COMPILED**: JieLi toolchain build and static budget passed.
- **VALIDATED ON HARDWARE**: none.
- **UNVERIFIED / BLOCKED**: listed explicitly.

## Changes made

1. **Visualizer peak decay (UI, not DSP).** `graph_home_viz()` measured the 16 spectrum bands twice
   because the screen is drawn in two 88-pixel bands; the peak markers therefore fell two rows per
   frame. It now measures/updates once and draws the same bars in both passes. A native HOME->HOME
   render was added to `ui_render`.
2. **Undefined behaviour in fixed-point DSP.** Seven left shifts of negative values were replaced by
   equivalent multiplications: `fx.c` DC blocker (`x * 64`, `d * 4096`), `drumx_voice.c` output
   (`* 2`) and `eng_drum.c` output/IMAGE (`* 2`, `* 2`, `* 2`, `* 4096`). The audio is bit-identical:
   the 220 golden renders are unchanged.
3. **Reproducible groups.** `tests/groups.sh` now has `security` (installer, backup, package bounds,
   sanitized audio test) and `stress` (`tests/stress_run.sh N`), in addition to `ui`, `presets`, `web`,
   `emu` and `target`.

## Measurements (host, not AC791N)

- `hostsim` six-part stress: 10 shared voices, all six parts sounding, no hard-cut sample step; worst
  single part 602.9 ns/sample (ASan/UBSan build), six-part mix 569.5 ns/sample.
- `regress`: 220 golden renders, **0 changed**, 0 health failures, 0 voice/routing failures, 0 CPU
  entries over budget; `6parts_full` 680.3 ns/sample on this host.
- Target build after the loader fix: app **545676 B**, package **610116 B**, `FM-1_982`,
  SHA-256 `1a3ca1353d7aa3697b85e8e2651b90a76e797a0d2d0241ac96a3053ffee696d9`. (The PR A
  package, without this loader change, remains 610113 B.)
- Target static budget: pass; `mod_begin` 35/166, `mod_voice` 19/30, DRUM path 1189/1191,
  `perf_block` 569/522 (`+9 %`, inside the existing +10 % tolerance).
- RAM: `.data+.bss` 94572 B of 98304 B; pool spare 9248 B (the project's minimum is 8192 B).
  This is margin, not a measured board headroom.
- UI: 172 screens x 17 palettes, 0 lint findings, 0 alignment errors.
- Sanitized host paths: `audio_test`, `reverb_test`, six-part `hostsim`, and the project,
  persistence, robust and MIDI tests plus two full regress passes (the `stress` group) pass under the
  repository policy (ASan + bounds; intentional fixed-point wraps are excluded, as in
  `tests/run_tests.sh`).

## What the security review confirms

The installer already enforces a published SHA-256, package CRC/entry bounds, model identity before
and after the write, disconnect/lost-link handling, resume from update mode, and V15 stock-package
recognition. The browser package parser rejects truncated/oversized/malformed packages. The single-bank
write window, the absence of a signature/anti-rollback and the lack of a demonstrated recovery with a
damaged boot area remain **open**. The flash size is now resolved to 1 MiB (below).

## Second audit (2026-11-03): open risks

Status labels used here: **IMPLEMENTED**, **EXECUTED ON HOST**, **TARGET COMPILED**, **SIMULATED**,
**NO VERIFIED** and **BLOCKED**. No physical FM-1 was available.

### 1. Flash: 1 MiB, not 2 MiB (resolved)

- Puya P25Q80H datasheet: 8 Mbit = 1 MiB; JEDEC `0x856014` encodes 2^20 bytes. Internal evidence:
  `web/emu/felucca_web.c` allocates `nor[0x100000]`, the loader caps `flash.bin` at `LDR_FL_MAX
  0x100000`, and the package image is `0x93000` bytes. `tools/hw_model.py` said 2 MiB: corrected.
- Map: head `[0x0,0x4000)`, app `[0x4000,0x93000)`, VM `[0x93000,0xE9000)`, BTIF
  `[0xE9000,0xEA000)`, USR `[0xEA000,0xFC000)`, factory `[0xFF000,0x100000)`. The app slot starts
  at `0x4120`; `0x4120 + 0x8DFBC = 0x920DC`, so 3.876 B remain before `0x93000`. The package's VM
  descriptor ends exactly at `0xE9000`. No address above `0x100000` is used.
- The 2 MiB statements in `AGENTS.md`, `TECHNICAL_OVERVIEW.md` and `HARDWARE_SIMULATION.md` were
  stale; corrected. **Still NO VERIFIED on hardware**: the JEDEC was not read from a physical unit.
  Conclusion is documentary, not a measurement.

### 2. RAM, stacks, heap, DMA and ISR (EXECUTED ON HOST)

`tests/memory_map_test.py` reads the target ELF and fails on any overlap. Current map:

| Region | Range / size | Headroom |
|---|---|---|
| LOWBUF | `0x1C00000..0x1C067A4` (26.532 B) | 2.140 B to `0x1C07000` |
| `.data` + `.bss` | `0x1C08000..0x1C1F16C` (94.572 B of 98.304) | 3.732 B to `0x1C20000` |
| pool | `0x1C20000..0x1C6BFC8` (311.240 B) | 9.248 B after core-1 stacks |
| core-1 stacks | `0x1C6BFE0..0x1C6DBE0` (7.168 B) | 6 KiB user + 1 KiB exception |
| RAMTEXT | `0x1C70800..0x1C75B10` | 3.312 B to guard `0x1C76800` |
| user stack | `0x1C76900..0x1C7A000` (13.568 B) | 256 B guard each side |
| system stack | `0x1C7A100..0x1C7C000` (7.936 B) | 256 B guard each side |
| NOINIT | `0x1C7C000..0x1C7FCD4` (15.572 B) | **124 B** to reserved `0x1C7FD50` |

- There is **no heap allocator call** in `firmware/` (`malloc`/`calloc`/`pvPortMalloc` absent); the
  big buffers are static `.bss`/`.pool`, so heap fragmentation cannot collide with them. `abuf` is
  `0x1C08380`, 2.048 B in `.bss`; the DMA/audio rings are not on a stack.
- No range overlaps. The risks are **stack depth** (13,75 KiB user, 7,75 KiB system) and **NOINIT's
  124 B**, not a detected collision. The guards are hardware write/EMU limits armed in `main.c`;
  they detect an overflow, they do not make more room. ISR nesting depth and worst-case stack use
  are **not measured** on target.
- The 94.572 B figure includes the 4-byte `.data`/`.bss` alignment; `.bss` alone is 93.804 B.

### 3. Static DSP budgets (TARGET COMPILED)

- `DRUM_PATH 1189/1191`: 2 units under the reference, but the enforced tolerance is +10 %, so the
  hard ceiling is ~1.310; the real margin is ~121 units, not 2. It is a static sum, not a measured
  worst case.
- `perf_block 569/522`: **+9,0 %**, hard ceiling ~574, only **~5 units** left. This is the FX hold
  layer, not the normal path, but it is the thinnest static margin in the build. No target cycle
  measurement exists; do not raise the budget or claim it safe under every effect combination.
- `target_budget.py` passed; no budget was relaxed.

### 4. Installer, backup and restore (INDEPENDENT REVIEW)

Write paths are bounded:

- Browser/CLI update: published SHA-256 before anything, `.fwsc` CRC/entry bounds, model identity
  before and after, lost-link/resume handling; the loader reads the whole `flash.bin` and checks its
  CRC and each app-sector CRC **before the first erase**, then erases/programs only
  `[0x4000,0x93000)`, verifies each sector and retries twice.
- Loader write guards: `ldr_erase`/`ldr_prog` reject the head, the store `[0x97000,0xE0000)` and any
  range past `0x93000`; the head is never written. The host test exercises a damaged package, a
  transfer error after the check, a foreign chip key and five power-cut points.
- Backup: `readBackup` validates format, object ids, sizes, base64 length and CRC32 of every object
  before the first `PUT`; restore sends objects individually with begin/chunk/commit. A disconnect
  can leave a **partial restore** (documented); it cannot write past an object's declared size.
- Residual: SHA-256 is integrity, not provenance (no signature/anti-rollback); the site can replace
  package and hash together. Recovery with a damaged boot area is not demonstrated. No physical
  backup was restored; the “recuperable” claim is limited to the simulated protocol.

### 5. Interruption / disconnection / corruption (SIMULATED)

`tests/ldr_test` now cuts `ldr_prog` after 0, 4 KiB, 64 KiB, 256 KiB and 512 KiB, keeps the update
record, and runs a second session: all five resume, the app matches the package, the head and user
store are untouched, and no write leaves the window. This is **SIMULATED** against the loader in
RAM; it does not exercise the SPL, real erase/program timing or a physical power rail. Disconnection
and corruption paths remain covered by `tests/install_test.py`, `web/test_web.mjs`,
`web/test_pkg_bounds.mjs` and the damaged-package case in `ldr_test`.

### 5b. Update-record clear: fixed data-loss window

The loader's record scan used to cover `[0x93000, 0xFC000)` and skip only Felucca's main store. A
record-looking 80-byte tail at `0x93F00` (VM), `0xE5F00` (autosave), or `0xEBF00` (USR) could make it
erase that 4 KiB sector; the FM6 voice bank at `0xE7000` was in the same risk window. No host-only input
could select an arbitrary address, but existing user data could match the update-record magic/CRC.

The scan is now limited to `[0xE0000, 0xE9000)` and explicitly skips the autosave/FM6 range
`[0xE5000, 0xE8000)`. It still clears Felucca's `0xE4F00` and the stock firmware's `0xE8F00`. The test
places valid record tails at `0x93F00`, `0xE5F00` and `0xEBF00`, checks all three survive, and tightens
its erase guard so any erase outside the app area, `[0xE0000,0xE5000)` or `[0xE8000,0xE9000)` fails.
`ldr_test` and its ASan/UBSan build pass. This is **SIMULATED**, not a hardware write.

### 6. Reducing the single-bank write window (NO CHANGE)

The loader writes `[0x4000,0x93000)` in ascending 4 KiB sectors. Writing the first sector last would
not create a bootable old image by itself: the SPL's update record still forces the loader, and the
old app's remaining sectors would already have been replaced. A real reduction needs bootloader
support (A/B or a recovery slot) or a verified write-order protocol. Neither is added here; changing
it without hardware evidence would risk the boot path.

### 7. Manual recovery (DOCUMENTED, NOT DEMONSTRATED)

Requires the stock `FM-1_v15.fwsc` and one of: OCT− + OCT+ held 5 s at power-on (Felucca update
mode), REC (fm1-base/m-bootloader), the ROM download mode `4C4A:8057` with M-Flasher /
`MvaveFM1Unbricker` / `fm1_rescue`, or the **USB_KEY dongle** for a hard brick. The flash key must
be `0x980F` and JEDEC `856014` (1 MiB); read and save the whole flash before writing; write only
`[0x4000,0x93000)`. A damaged boot area `[0,0x4000)` is **not** recoverable by this procedure. None
of this was run on hardware.

### 8. UBSan exclusions (REVIEWED)

The seven left shifts of negative values fixed on this branch were the actionable UB. The remaining
excluded categories are intentional fixed-point wrap and signed right shifts (implementation-defined,
arithmetic on pi32v2, not UB). A static pass over the touched DSP files found no new left shift of a
negative signed value; `fx.c`'s variable shifts are on unsigned or bounded non-negative values. This
is a code review, not proof: a full UBSan run still reports the intentional wraps, and the policy
continues to exclude them.

### 9. Further DSP optimisation (NO MEASURED WIN)

No additional change is claimed. The branch removes UB but does not reduce operations; the host
`perform_test` has no instruction counter here, and target cycles are unavailable. `perf_block` has
the least static headroom, so any optimisation should start there with a target measurement, not
with a source change made to “look faster”.

### 10. Branch and PR strategy

`origin/main` is `7c3dd24`; this branch is 14 commits ahead: 12 UI/preset/web commits (through
`e23fb14`) plus `200a0dc` (DSP UB) and `e4cede6` (docs). The base is the unmerged
`feat/v1.7-visual-presets`, not `main`.

- **PR A**: the 12 UI/preset/web commits, including the visualizer fix `e23fb14`, into `main`.
- **PR B**: `200a0dc` + these audit changes, rebased onto the merge of PR A; the DSP fixes do not
depend on the UI code, but the report references the branch. Do not open PR B against `main` while
the 12 unrelated commits are still in it.
- Keep this branch as the integration branch; no force push, no history rewrite, no release.
- If PR A is rejected, PR B remains a draft against the UI branch; the DSP patch can be cherry-picked
  alone after a clean build.

## Publication and CI status (2026-11-03)

- Remote `main` is `7c3dd24`; PR #49 (`feat/v1.7-visual-presets`) and PR #51
  (`perf/security-hardening`) are open against `main`. PR #51 is **not** based on PR #49, so its
  current diff against `main` includes the 12 UI commits. This must be retargeted after PR #49 is
  integrated; no remote setting was changed here.
- The emulator workflow passes on both heads (`e23fb14`, `e840624`). The host workflow fails on both,
  and also on `main` (`7c3dd24`): the `fm6bank` preset-count check and the editor mock tables are
  pre-existing failures, not introduced by the DSP/audit commits. Their exact failing steps are
  recorded in the publication report; they are not a reason to merge a red PR.
- No repository ruleset applies to `main`; the classic branch-protection endpoint requires
authentication and could not be read. No review requirement could be confirmed. This is a repository
  governance blocker, not a firmware defect.
- No PR was merged, no branch was deleted, no release or Pages deployment was created, and no physical
  FM-1 was touched.

## Recommendation

**NO APTO PARA VALIDACIÓN FÍSICA CONTROLADA todavía.** The flash size is resolved, the SRAM map is
checked by a test, the loader preflight and backup validation are sound in simulation, and the
record-clear window is fixed. Before a controlled physical run, the host workflow must be green (or
the two pre-existing failures explicitly waived), PR #51 must be based on the integrated PR #49, and
the JEDEC, power-cut, recovery and single-bank window must be tested on hardware with USB_KEY. The
missing signature/anti-rollback remains a security limitation, not a build blocker.

## No claims

- No DSP speedup is claimed: the four UB changes are arithmetic identities and the visualizer change
  is drawing-only.
- No physical CPU, SRAM, cache, DMA, underrun or recovery measurement exists.
- No signature, secure boot, A/B rollback or anti-brick guarantee was added.
- The public site was not deployed.

## Sources

The hardware/bootloader/OTA findings are the ones already collected in `docs/SECURITY.md`:
fm1-dev-wiki (Flash-Storage, Boot-Chain-and-Bootloaders, Second-Core, Update-Protocol-and-Installers,
Recovery), FM-1-RE (architecture and OTA protocol), m-bootloader/M-Flasher, lunar-modulator and
FM-1 Transporter. Those are community reverse-engineering sources, not vendor guarantees.
