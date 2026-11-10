# Optimization baseline (measured, FASE 1)

Suite: `opt/performance-baseline`. Tree measured: `main` at `6ceee7c` (2026-11-09), the same sources
everything below refers to. Every number says what produced it; host timings are this CI-class VM and
they move with the machine, so only the *ratios measured in one run* are meaningful there.

    class            what proves it
    AC791N build     tools/build.py with the JieLi pi32v2 toolchain (image, sections, budgets)
    static target    tests/target_budget.py over build/felucca.dis (pi32v2 instruction counts)
    host             cc -O2 test runners on this x86-64 VM (clock(), not cycles)
    emulator         build/emu WebAssembly + native reference (bit-for-bit)

Nothing here was measured on an M-VAVE FM-1. There is no unit in this environment: no latency, no real
CPU time, no JEDEC, no write. Installation stays `INSTALL BLOCKED`.

## 1. Memory map (AC791N build, `-Oz`)

| region | used | capacity | spare |
|---|---:|---:|---:|
| `.data` + `.bss` | 94 920 B | 98 304 B | **3 384 B** |
| POOL (`.pool` + core-1 stacks) | 311 240 B | 327 680 B | **9 248 B** (the floor is 8 KiB) |
| LOWBUF | 26 532 B | 28 672 B | 2 140 B |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 284 B | 24 576 B | 5 292 B |
| XIP code + constants | 511 960 B | 581 052 B | 69 092 B |
| image / package | 532 088 B / 610 806 B | 1 MiB NOR | — |

`.data` is only 748 B; the RAM budget is nearly all `.bss` (94 152 B). The largest residents:

    RAM (.bss)                              POOL (.pool)
    15 600  trk (4 parts)                   131 072  dly_buf (delay line: the delay TIME)
    15 360  proj_slot[2]                     59 520  cv_buf (240x124 canvas)
     9 452  proj_scratch   (LOWBUF)          32 768  sl_buf (slicer)
     9 398  sec_stage      (LOWBUF)          32 768  cloud_buf (granular)
     7 680  undo_st        (LOWBUF)          12 288  comb_buf
     7 104  fm6_note (4 x 2 x 244)            9 874  rev_comb
     6 160  up_bank                           4 992  reel
     5 520  drw_v (WHEEL voice state)         4 096  shim_buf / rpre_buf / cho_buf

The LOWBUF region is exactly those three scratch buffers. The 2 140 B it has spare do not cover any of
the `.bss` citizens, and moving ~2 KiB into POOL would drop its spare below the 8 KiB floor: there is no
free byte to relocate, which is why FASE 2 has to remove a buffer, not shuffle one.

## 2. Hot-path cost on the target (static, exact)

`tests/target_budget.py` counts the instructions in each function's loops, weights them by nesting
(x4 per level) and adds 9 per hardware divide. It is a static count, not a cycle count; it changes only
when the compiled code changes. The ten heaviest, per run (a render runs once per 32-sample block, the
timer ISR 10 000 times a second):

    fm1_timer5_irq   1564 insn, 1257 in loops, 2 divides, 74 calls   cost 28 138  (budget 27 338)  <- 10 kHz
    slicer_bus        261 insn,  251 in loops                        cost    806
    analog_ext        346 insn,  157 in loops, 16 calls              cost    157
    analog_morph      202 insn,   89 in loops                        cost     89
    lofi_byte_render  226 insn,   87 in loops                        cost     87
    analog_dirty      194 insn,   84 in loops                        cost     84
    phase_render      274 insn,   84 in loops                        cost     84
    noise_render      494 insn,  201 in loops                        cost    201
    formant_render    535 insn,  229 in loops                        cost    229
    fm6_render        510 insn,  253 in loops, 13 calls              cost    322

Two facts that change where the work is worth doing:

- **The key/LED ISR is the largest single consumer**, and it runs 10 000 times a second: ~1 564
  instructions per tick is ~15.6 M instructions/s on core 0, ~5 % of a 320 MHz core before any audio.
  Its loop is the 16-bit shift of the LED row plus the key debounce, not DSP.
- **The render loops call per block, not per sample.** `fm6_render`'s 13 calls are 6 operators x 2 buses
  per 32-sample block (~0.4 calls/sample); `analog_render` has none (everything inlined). Inlining the
  operators would save no meaningful work — the block structure is already the optimization.

## 3. Compiler level: measured, and `-Oz` stays

A tempting idea (the tree has 69 KB of spare flash) is to compile at `-Os` or `-O2` for speed. It is
wrong on this target, and now it is measured rather than assumed:

| build | image | XIP code | `analog_render` cost | divides in it |
|---|---:|---:|---:|---:|
| `-Oz` (shipped) | 532 088 B | 372 504 B | **148** | 0 |
| `-Os` | 567 180 B | 405 116 B | **674 (+355 %)** | **20** |
| `-O2` | does not link | over XIP by 116 068 B | — | — |

`-Oz` is the level at which this clang rewrites the constant divisions as shifts and multiplies; `-Os`
keeps them hardware divides, which on pi32v2 is a software routine. The same regression hits
`analog_ext` (+376 %), `analog_morph` (+846 %) and the timer ISR (+28 %). A per-function `-O2` would need
an attribute this clang does not have (it only has `minsize`, which `-Oz` already applies). The level is
not a size/speed trade here; it is a correctness-of-codegen choice, and `tools/build.py` now records why.

## 4. Host CPU (this VM, `cc -O2`, no instruction counter)

One part, eight notes held, heaviest preset per engine; the mix rows are the musical stress cases:

| row | ns/sample | note |
|---|---:|---|
| idle_drums | 94.9 | the floor: FX buses, sequencer, click |
| 3parts_full_drums | 274.2 | three parts + drums |
| 6parts_full | 352.2 | six parts, the maximum load |
| 220 golden renders | 0 changed | the sound is byte-identical to the baselines |

`0 instr` in each row: this kernel gives no instruction counter (`proc_pid_rusage`), so these are wall
clock. The same run reported a host median of 1.01 against the recorded baselines, so a row's *ratio*
inside one run is usable; across runs it is not.

## 5. Twelve voices, measured from scratch (FASE 7 input)

Built with `FELUCCA_VOICES=12` at `-Oz`, same toolchain:

| | 10 voices (shipped) | 12 voices | delta |
|---|---:|---:|---:|
| `.data` + `.bss` | 94 920 B | **97 192 B** | **+2 272 B** |
| margin to 96 KiB | 3 384 B | **1 112 B** | −2 272 B |
| image | 532 088 B | 532 328 B | +240 B |
| POOL / RAM code | 311 240 / 19 284 B | 311 240 / 19 284 B | 0 |
| host 6parts_full | 352.2 ns | 385.1 ns | +9.3 % |
| host 3parts_drums | 274.2 ns | 364.7 ns | +33 % (same run, noisy) |
| 220 golden renders | 0 changed | **2 changed** | 2 renders allocate differently |

The historical figure was +1 056 B of SRAM; the real build needs **+2 272 B**, which leaves 1 112 B of
`.bss` margin — under a third of today's. It compiles and the host tests pass, but 1 112 B does not
absorb the peaks the instrument actually has (a project load stages 9 452 B in LOWBUF, not `.bss`, so
that one is fine; a preset bank + undo + a song section are not). **12 voices is not viable on RAM
headroom as the tree stands**, and no CPU claim about it can be made without a unit.

## 6. What this baseline rules out, and what it points at

- Ruled out by measurement: global `-Os`/`-O2`; inlining the FM6 operator loops (per-block calls);
  relocating `.bss` into LOWBUF or POOL (no room, and POOL has a 8 KiB floor); `minsize` on more files
  (byte-identical image, measured in PR #85).
- Points at: the 10 kHz TIMER5 ISR (the largest single consumer), the UI draw path on core 0 (which
  shares the core with tracks 1-3), and the three LOWBUF scratch buffers (the only multi-KB blocks whose
  lifetimes could be proven disjoint). Each needs its own measurement and its own commit.

## 7. Reproduce

    export JIELI_TOOLCHAIN=… AC79_SDK=…
    python3 tools/build.py                       # image, sections and the four budgets
    python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/regress tests/regress.c -lm
    build/host/regress tests/golden.txt tests/cpu_baseline.txt
    FELUCCA_VOICES=12 python3 tools/build.py     # the 12-voice row of §5
