# Phase 1 audit — conservative cleanup of Quantum GammaWave

Branch: `optimize/phase1-audit` (based on `main` @ ae2e62e). No bootloader/loader touched. No release published.

## 1. Method

- Source tree: 79 C files, ~47.7 kLOC in `firmware/src`.
- Toolchain: JieLi AC791N pi32v2 clang (clang 4.0.1), `-Oz -ffunction-sections`, linker `--gc-sections`.
- Dead-code detection: temporarily dropped `-Wno-unused-function` from `CFLAGS` in `tools/build.py`, rebuilt, collected 44 `unused function` warnings. Restored the original flag afterwards (production build unchanged).
- Each candidate cross-checked with `grep -rn` over `firmware/`, `web/`, `tools/`, `tests/`, `docs/` for direct calls, macro use, generated-code references, emulator/Web Studio/installer use, and test hooks.
- No function was removed unless it had **zero references anywhere** (including tests). HAL helpers (`fm1_*`, `fl_*`) were retained even when unused, because they are recovery/validation entry points and the policy forbids weakening those paths.

## 2. Inventory of candidates

### A. Unused within the firmware build (44 functions)

`arp_tick`, `click_beat_now`, `dice_all`, `dice_print`, `dice_sound`, `dice_texture`,
`engine_columns`, `fl_erase4k`, `fl_plain_window_init`, `fl_verify_xip`, `fl_write`,
`fm1_input_scan`, `fm1_irq_mask`, `fm1_led_key`, `fm1_mem_readable`, `fm1_reboot`,
`fm1_reset_reason`, `fm1_wdt_stop`, `fmt_int`, `gen_page`, `gen_power_on`,
`graph_drum`, `gw_dither`, `midi_out_event`, `motion_replace_track`, `mut_progress`,
`og_dash`, `og_ill`, `og_label`, `og_plate`, `og_quad`, `og_shape`, `og_stipple`,
`og_tip`, `ota_boot_cleanup`, `ota_session`, `panel_alert`, `perf_buf_state`,
`rng`, `slicer_reset`, `str_cpy`, `str_eq`, `str_len`, `vz_flares`.

### B. Test-referenced (NOT removed)

`arp_tick`, `click_beat_now`, `gen_power_on`, `slicer_reset` and several others are
static firmware helpers called directly by `tests/*` (e.g. `tests/motion_test.c`,
`tests/click_test.c`, `tests/ui_test.c`, `tests/slicer_test.c`). Removing them would
delete test coverage, so they stay. They are "unused in the firmware build" only
because the tests compile the same sources with extra entry points.

### C. HAL helpers retained (NOT removed)

`fl_*` and `fm1_*` functions are hardware-abstraction entry points. Even with zero
callers today they are the recovery/validation surface and may be needed by the
loader, the emulator shim or a future unit test. Removing them would violate the
"do not remove recovery/validation functions" rule.

### D. Common helpers retained

`str_cpy`, `str_eq`, `str_len`, `fmt_int`, `rng` are used hundreds of times
(`str_cpy` 371 refs, `rng` 136). Not candidates.

### E. Zero-reference UI/render helpers identified

`dice_sound`, `dice_texture`, `engine_columns`, `gen_page`, `graph_drum`,
`gw_dither`, `og_plate`, `og_shape`, `og_tip`, `panel_alert`, `perf_buf_state`,
`vz_flares` have zero references anywhere. They are **already eliminated from the
linked binary** by `--gc-sections` + `-ffunction-sections`, so removing them from
source changes **no firmware metric** (size, RAM, CPU). Removal would be pure
source hygiene and risks an accidental bad deletion (a trial run corrupted
`ui_graph.c` by over-running braces in a string literal). Decision: **leave them
in source**; document as candidates for a future, tool-assisted removal pass with
a brace-aware remover. This keeps the diff minimal and avoids regressions.

### F. Duplicates / residues / includes

- No untracked build residues (`git status` clean apart from the change).
- No duplicate top-level assets found in `assets/`.
- `tools/build.py` CFLAGS restored to the original `-Wno-unused-function` so CI
  stays quiet; the audit flag is not shipped.
- `web/test_web.mjs`, `tests/install_test.py`, `NO_PACKAGE=1 tests/run_tests.sh`
  all pass on the current tree.

### G. Hot-path / effect-skipping

The audio render path (`voice.c`, `fx.c`) already gates work on state (e.g.
`fx.sp_size`, `fx.primed`, crossfade counters). No obvious "process disabled
effect" waste was found that could be removed without changing sound. Any such
change would require golden-render comparison; none was attempted in this pass
because the render budgets are already at their limits (`perf_block 254/253`,
`target_budget.py` reports ok within +10 %).

### H. Version drift (not fixed here)

`firmware/src/felucca.c` reports `FELUCCA_VERSION "v2.4.1"` while `README.md`
badge reads `v2.4.0`. Resolving this is a release-decision, not an optimization;
left untouched pending release status.

## 3. Changes made in this branch

1. `tests/ui_render.c`: corrected the splash expectation from
   `FELUCCA_VERSION "|QUANTUM GAMMAWAVE // QG-1|BOOT|100%"` to
   `FELUCCA_VERSION "|QUANTUM GAMMAWAVE // QG-1"`. The `BOOT|100%` text was
   intentionally removed from the bootsplash in commit 00bc571 ("remove BOOT
   from bootsplash"); the test still asserted the old string, producing 68 lint
   findings on every palette/size. This is a test correction to match shipped
   behaviour, not a weakening of an assertion.
2. `docs/AUDIT_PHASE1.md`: this document.

No firmware source, HAL, loader, bootloader, backup, licence or build script was
modified. No functionality removed.

## 4. Metrics (before = after; no firmware change)

| Metric | Before | After | Delta |
|---|---|---|---|
| Firmware image (felucca.bin) | 537676 B | 537676 B | 0 |
| Package (felucca.fwsc) | 610959 B | 610959 B | 0 |
| .data + .bss | 87632 / 98304 B | 87632 / 98304 B | 0 |
| Pool spare | 21536 B | 21536 B | 0 |
| RAM code | 19268 / 24576 B | 19268 / 24576 B | 0 |
| XIP code / constants | 377632 / 140028 B | 377632 / 140028 B | 0 |
| .ram_text | 1193 insns | 1193 insns | 0 |
| target_budget.py | ok (perf_block 254/253) | ok (perf_block 254/253) | 0 |
| ui_render findings | 68 (splash) | 0 | -68 |
| install_test.py | pass | pass | 0 |
| web/test_web.mjs | pass | pass | 0 |
| NO_PACKAGE run_tests.sh | pass (after fix) | pass | 0 |

Conclusion: dead-code removal in this codebase cannot produce measurable firmware
wins because the linker already garbage-collects unused static functions. Real
size/CPU/RAM reductions require profiling the live (used) paths and are out of
scope for a safe, single-pass audit. The one concrete defect found and fixed is
the stale splash assertion (68 lint findings).

## 5. Blocked / not done

- No physical FM-1 available: hardware validation unchanged (not simulated).
- emcc not installed: WASM not rebuilt; no WASM size measurement.
- GitHub Release / Pages not touched (per "no publicar todavía").
- Deep audio hot-path optimization and golden-render comparison deferred to a
  follow-up PR with measurements.

## 6. Next steps proposed

1. Tool-assisted removal of the 12 zero-ref UI helpers using a brace/string-aware
   remover, verified by a full build + run_tests.
2. Profile `analog_render` and `fx` render with host timing to find real
   redundant work (target_budget already near limits).
3. Reconcile `FELUCCA_VERSION` vs README badge at release time.
4. Push branch and open PR for review.
PYEOF
echo written