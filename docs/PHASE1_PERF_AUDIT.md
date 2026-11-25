# Phase 1/3 — memory, CPU and storage: what the tree actually has

Every number is `[HOST]` (this machine, `tools/engine_size.py`, `tools/dsp_bench.py`, `tests/regress`) or an
exact count from the AC791N build. **Nothing here is a measurement of an FM-1.**

## The state before this phase

| | value | source |
|---|---|---|
| version / commit | v2.3.14, `f3ef8c6` | `firmware/src/felucca.c`, `git log` |
| image | 578 000 B of 583 612 (5 612 B free) | `./build.sh` |
| XIP | 447 024 B code + 126 196 B constants = 573 220 B | `tools/engine_size.py` |
| RAM `.data+.bss` | **93 460 B of 98 304 (4 844 B free)** | `tools/engine_size.py` |
| `voice_t` | 92 B; six tracks 4 416 B at 8 voices, 5 520 B at 10 | `tools/budget_report.py` |
| mix, 6 parts full | 325.4 ns/sample (3 parts + drums 284.9, idle 105.0) | `tools/dsp_bench.py` |
| UI frame | 1 700 ns/call (draw 1 550, input 557) | `tools/dsp_bench.py` |

The RAM figure is the one that matters: **4 844 B of headroom**, under the 8 KiB the operating document asks for.
The image has 5 564 B of app slot left.

## Where the counted RAM goes (`nm` on `build/felucca.elf`, 0x01c00000 window)

| symbol | bytes | what it is |
|---|---|---|
| `trk` | 10 584 | six tracks: params, engine state, voices, VU, sends |
| `proj_scratch` | 9 644 | one `project_store_t`, the import/export staging |
| `sec_stage` | 9 446 | the decoded song section the main loop stages for the ISR |
| `undo_st[2]` | 7 680 | UNDO and the snapshot it replaces (redo) |
| `fm6_note[6][6]` | 7 200 | one FM6 note per track and voice (200 B each) |
| `drum_kit` | 5 376 | the DRUM kit's sixteen slots |
| `st_buf`, `upf`, `ua_ring`, `abuf` | 3 840 / 3 728 / 2 048 / 2 048 | flash sector staging, user-preset cache, USB ring, audio buffer |

## What this phase changed

`fm6_note_compute()` computes msfa's AMS factor (`exp(sensamp/262144 * 0.07 + 12.2)`) — which depends only on the
block's `amod` and `lfo_val`, never on a sample — once per block, outside the operator loop, and skips
`fm6_env_get()` for an operator whose envelope is already in stage 4 (which cannot move).

- **Output: bit-identical.** `tests/regress`: 311 golden renders, 0 changed; 0 health failures; emulator output
  bit-for-bit against the native build.
- **`tests/fm6_test`**: 34 checks, 0 failures.
- **Flash: +48 B** (578 000 → 578 048; the hoisted block costs more code than it saves there).
- **CPU: not measurable here.** `dsp_bench` 6 parts full 325.4 → 325.0 ns/sample, 3 parts + drums 284.9 → 282.6,
  i.e. inside this host's ±5 % run-to-run noise. The removed work is real (one exp2 per operator per block, one
  call per finished operator) and it is **not** claimed as a speedup.

## What is left, with sizes, for phases 2 and 3

1. **`proj_scratch` + `undo_st[1]` (≈9.6 KB)** — the largest single block. A union is only safe if the import path
   and an UNDO can never run at the same time; that invariant is not written down anywhere yet, so it needs
   proving before the union is taken.
2. **`sec_stage` (9.4 KB)** — live while a song plays, so it cannot share with the import staging.
3. **`fm6_note` (7.2 KB, 200 B per note)** — 36 notes for 6 tracks × 6 voices. `rel_t` (4 B) fits in 2 B of
   blocks; `base[6]` is a per-note cache that could be recomputed to buy 24 B a note (864 B for the engine) at a
   cost of six scale computations a block. Worth measuring, not guessing.
4. **8 KiB of headroom** — the rule in the operating document is not met (4 844 B). Either the budget or the
   buffers need a decision; both are recorded here rather than papered over.
5. **The per-sample DSP paths are already tight**: `mix_part` pays nothing for a dry part's sends, its gains and
   pans are per-block ramps, there is no divide or `float` in it, and six parts cost 325 ns/sample against 105 for
   idle — the part cost is the voices, not the mixer.
