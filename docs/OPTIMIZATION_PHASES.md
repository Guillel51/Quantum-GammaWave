# Optimization phases 2–9: what was measured, and what was refused

Companion to `docs/OPTIMIZATION_BASELINE.md` (FASE 1). Every number below was produced in this tree at
`1abfffa`; the class is named each time. Nothing here is a measurement on an FM-1: there is no unit in
this environment, so `INSTALL BLOCKED` stands and no CPU or latency claim about the instrument is made.

The method was the same for every phase: find the work, measure it, change it, and keep it only if the
target's own budgets, the 220 golden renders and the suite all pass. Two candidates were built, measured
and **refused**; that is recorded here so the next attempt does not repeat them.

## FASE 2 — memory

The baseline map (AC791N build) has three regions near their limit: `.data+.bss` 94 920/98 304 (3 384 B),
POOL 311 240 with 9 248 B spare (floor 8 KiB), LOWBUF 26 532/28 672.

The only multi-KB blocks whose lifetimes could plausibly be disjoint are the three LOWBUF citizens:

| buffer | size | what it holds | lifetime |
|---|---:|---|---|
| `proj_scratch` | 9 452 B | a decoded project, staged for pack/import | one project operation |
| `sec_stage` | 9 398 B | the song chain's *next* section | refilled all through playback |
| `undo_st[2]` | 7 680 B | the two undo slots | persistent by design |

Overlapping `proj_scratch` with `sec_stage` would free 9 398 B, and moving a `.bss` citizen into the
freed tail (e.g. `up_bank`, 6 160 B) would raise the `.bss` margin to 9 544 B — enough to make 12 voices
possible. **It was refused**: the pair is not provably disjoint. `sec_stage` is refilled by the main loop
(`song_main.c:64`) and consumed by the ISR's swap for as long as the song plays, while a project save
(`project.c:1709`, `project_capture` → `proj_pack` into `proj_scratch`) also runs in the main loop. A
save during playback would have to stop the chain for its whole duration to make the overlap safe, and a
1/16 that does not arrive is an audible behaviour change, not an optimization.

Verdict: **no provable overlap without changing behaviour.** `.bss` margin stays 3 384 B; LOWBUF was never
the region blocking anything.

## FASE 3 — voices and voice stealing

Measured from scratch with `FELUCCA_VOICES=12` (see the baseline document §5): **+2 272 B** of
`.data+.bss` (the historical figure was 1 056 B), leaving 1 112 B of margin, +240 B of image, 2 of the
220 golden renders changed (they allocate differently with twelve voices), and +9.3 % on the host
`6parts_full` row (+33 % on `3parts_full_drums`, same run, noisy host).

The stealing path itself was not changed: it is reached only when the 10 voices are gone, and its cost is
inside the per-block render that the target budgets already cover. Asserting an improvement there needs a
new stress scenario (a twelve-note chord over a held release) and a unit to hear it.

Verdict: **12 voices stay an experiment.** RAM margin is the binding constraint, and no CPU claim can be
made for the instrument itself.

## FASE 4 — DSP and FX: the one code change that was built, measured and refused

`dx_run` (DRUM-X, half rate) and `dx_run_sn` (full rate) compute four per-block ramp steps each by dividing
by the block's length: `(end - start) / n`. That is eight divisions per lane and block, and n is 16 or 32
in every full block. The change replaced them with an exact shift (`x + ((x >> 31) & (n - 1)) >> log2 n`,
which turns the shift's floor into C's truncation toward zero) and a fallback to `/` for a tail block.

What the change proved, and what killed it:

| check | result |
|---|---|
| exactness, host | **8 717 934 592 pairs, 0 differences** (all int32 for n = 16 and 32; corners and 2 M random for n = 1..64) |
| audio, host | **220 golden renders, 0 changed** |
| divides in the target's disassembly | `dx_run` 10 → 6, `dx_run_sn` 12 → 8 (exact, per build) |
| `tests/target_budget.py` | **DRUM path 1 153 → 2 197 (+91 %)**, `dx_run` cost 426 → 1 472: the loop instructions went 394 → 563 and the loop divides 1 → 3 |
| same with `always_inline` | **DRUM path 2 440 (+112 %)**, loop instructions 669, loop divides 4 |

What happens: at `-Oz` this clang does not keep the "is n a power of two" branch out of the ramps — it
duplicates it into the sample loops, so the instruction stream that actually runs per sample grows even
though the divides vanish. The gate is right and the change is wrong for this target.

Verdict: **refused and reverted; no firmware byte changed.** The remaining divides in DRUM-X are a real
cost (~8 per lane block, 8 lanes, ~1 378 blocks/s), but removing them needs a formulation without a
runtime branch — a reciprocal fixed at the call site, where the block size is known — not a helper that
decides per call.

## FASE 5 — UI

Not measured, and not guessed. The repository's exact counter for this (`tests/cpu_vg.c`, callgrind
sections for every engine, every reverb, the full mix, the sequencer and a song) needs valgrind, which
this environment does not have (no root, no package candidate). Host millisecond timings of a screen are
not a substitute for a frame budget on a 320 MHz core that also renders tracks 1–3.

Verdict: **blocked on the tool.** The UI shares core 0 with the audio path, so a real frame cost is worth
having: installing valgrind (or running `tests/cpu_vg.py` in CI) is the prerequisite, not a rewrite.

## FASE 6 — multitasking and the 10 kHz ISR

The TIMER5 tick is the largest single consumer in the static map: 1 564 instructions, 1 257 of them in
loops, 74 call edges, cost 28 138 against a budget of 27 338, 10 000 times a second — about 15.6 M
instructions/s on core 0 before any audio.

The cheap arithmetic win is not there: the two divides the model counts are not in the C source's shape
(`sub % 5u` compiles to a multiply-shift on pi32v2 — checked in the disassembly, no `div` in the function),
and the rest is the LED row's 16-bit shift and the key debounce, already written as tightly as the
comments claim. Lowering the tick rate would be exactly the kind of "optimization" the brief forbids.

Verdict: **no change**. This is where the remaining core-0 load is, and it needs the unit to be attacked
honestly (a measured tick profile, not an instruction count).

## FASE 7 — polyphony

Covered by FASE 3. 10 voices stays the shipped profile; 12 is refused on RAM margin (1 112 B), not on
taste.

## FASE 8 — validation

Every candidate above went through the same gate: AC791N build with the real toolchain (sections and the
four budgets), `tests/target_budget.py`, the 220 golden renders, and the full host suite (117 groups,
ASan/UBSan, emulator parity, package and release e2e). The refused DRUM-X change passed exactness and the
goldens and still failed the target budget — which is the point of having the budget.

## FASE 9 — publication

`docs/OPTIMIZATION_BASELINE.md` (FASE 1) and this document are the published record. No firmware change
accompanies them, because none of the candidates survived its own measurement.

## What would actually move the needle, in order of expected value

1. **The 10 kHz tick** (FASE 6): the largest verified load in the system, ~15.6 M instructions/s. A win
   here is worth more than every audio-path edit combined, but it needs a measured profile on the unit
   (or the callgrind counter in CI) and it must not reduce the scan or LED refresh rates.
2. **The DRUM-X ramps** (FASE 4), with a call-site reciprocal instead of a helper branch: ~8 divides per
   lane block, bit-exact, and the exactness proof and test already exist in this document's history.
3. **The UI frame cost** (FASE 5): unmeasured, therefore untouchable — first make the counter run.
4. **Memory** (FASE 2): only worth work as part of a decision that needs the bytes (12 voices), and that
   decision is refused on evidence.

## FASE 1/2 (v1.11.2) — the idle headroom of POOL and LOWBUF

Class: AC791N build (`tools/build.py`, sections and the four budgets) plus `tools/mem_budget.py`. Nothing
here is a measurement on an FM-1; `INSTALL BLOCKED` stands.

The starting point was the v1.11.1 build: `.data+.bss` **94 144 of 98 304 B, 4 160 B free against the
4 096 B floor** — 64 B of real margin in the window that binds every feature. At the same time two other
windows were paying for floors they did not need: LOWBUF kept 1 720 B idle above its 1 024 B floor and
POOL kept 9 248 B above the 8 KiB AGENTS.md requires. Neither can host audio state (they are not zeroed
at boot and the ISR never reads them), but cold main-loop staging fits by construction.

Three editor buffers, all fully written from their first byte before every use and unreachable from any
ISR (`ed_service` / `ed_bk_service` run in the main loop; `ota_wire_send` copies the frame into the USB
ring before it returns) were moved out of the 96 KiB window:

| buffer | size | was | now | why it is safe there |
|---|---:|---|---|---|
| `ed_out` (editor.c) | 600 B | `.bss` | `.lowbuf` | header + body written from byte 0 on every reply |
| `ed_bk_settings` (editor_backup.c) | 612 B | `.bss` | `.pool` | assigned in `ed_bk_capture` (persist_saved or memset) before `ed_bk_object` hands it out |
| `ed_bk_buf` (editor_backup.c) | 256 B | `.bss` | `.pool` | `ed_unpack7` writes the chunk before it is read |

Measured, same toolchain, same sources otherwise:

| window | v1.11.1 | v1.11.2 | floor |
|---|---:|---:|---:|
| `.data+.bss` | 94 144 B used / 4 160 free | **92 688 B used / 5 616 free** | 4 096 |
| LOWBUF | 31 048 / 1 720 free | 31 648 / **1 120 free** | 1 024 |
| POOL | 311 256 / 9 248 spare | 312 124 / **8 384 spare** | 8 KiB (AGENTS.md) |
| XIP code + constants | 576 708 B | 576 708 B | — |
| image | 581 508 of 583 612 | 581 508 of 583 612 | — |

**+1 456 B (35 %) more free RAM in the binding window**, and the margin over that window's floor goes from
64 B to 1 520 B; every floor in `tests/mem_budget.json` and the 8 KiB POOL rule still pass. No render,
instrument, preset, FX, UI, MIDI or sequencer path changed: the moved objects are not read by the audio
ISR and their contents are rebuilt on every use, so the 220 goldens and the whole host suite are expected
to stay bit-identical (and did, see the release record).

### What was audited and refused in this pass

| candidate | measurement | verdict |
|---|---|---|
| alignment padding in `.bss` | 274 B over 777 symbols (largest gap 12 B) | not movable without disturbing struct layout |
| identical constant blobs in XIP | 4 blobs, 128 B total | below noise |
| linker ICF (`--icf=all`) | this toolchain is **GNU ld 2.26**, which has no ICF; the firmware is one TU, so string/constant merging already happened | impossible |
| `step_t` packing | 15 B exactly, no padding, 6 x 64 steps | already minimal |
| `WT_GEN` | 6 192 B: already one half cycle per frame (was 12 288 B) | already done (1.5.22) |
| `SINE` / `fm6_exp2_tab` | 2 050 / 4 100 B, decoded at boot from 1 025 / 2 054 B of deltas | already done; storing them whole in XIP costs 7 199 B more than the app slot has |
| `midi_sel_on` | 1 024 B = 16 ch x 128 notes x 4 bits; track+1 and the pedal bit need exactly 4 | minimal |
| `song_idx`, `msc_p`, `motion_base`, `vpool`, `drw_v` | feature state, not copies | refused |
| `abuf`, `ua_ring`, `sx_frame`, `fm6_note`, `drum_kit`, `gen_keep` | audio DMA / USB ring / ISR / per-voice state | out of bounds by the phase's own rule |
| OTA staging (2 048 B) | main-loop only, but LOWBUF's and POOL's remaining slack after this move is 96 B and 188 B | does not fit the floors |
