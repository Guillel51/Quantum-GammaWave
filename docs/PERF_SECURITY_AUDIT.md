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
- Target build: app **545676 B**, package **610113 B**, `FM-1_982`, SHA-256
  `dc2790f0ae20175d9d548110177fc3dce48913188cbbc3238da990dde0eb55a1`.
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
recognition. The browser package parser rejects truncated/oversized/malformed packages. The 1 MiB vs
2 MiB flash discrepancy, the single-bank write window, the absence of a signature/anti-rollback and
the lack of a demonstrated recovery with a damaged boot area remain **open**.

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
