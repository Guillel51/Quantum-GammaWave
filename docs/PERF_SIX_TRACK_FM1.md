# Six-track load, memory budget and installer trust — FM-1 (QG-1)

Scope: `perf/six-track-hardening`, based on `fa94ec5`. Nothing here was flashed
into a physical FM-1, and nothing in the boot chain, flash map, partitions or
loader was modified.

## 1. What was measured, and on what

| Layer | Artefact | Hash / id | Notes |
|---|---|---|---|
| Host | `tests/six_track_test.c` | `build/uifix/six_track_test` | the firmware's own DSP through `tests/hostsim.c` |
| Target | `build/felucca.fwsc` | `7f7a9ee8…`, 610 897 B | built by `tools/build.py` with JieLi 20260730.1 + AC79 SDK |
| Target | `build/felucca.elf` | 627 552 B | `tests/memory_map_test.py` reads its sections |
| Target | `build/felucca.dis` | 8 202 854 B | `tests/target_budget.py` reads the render loops |

The AC791N clock is not fixed by this repository (160–240 MHz reported, 320/360
MHz also mentioned). No cycle count below is presented as a device measurement.

## 2. Six simultaneous tracks

Scene (`tests/six_track_test.c`): six tracks, **ten sustained voices** — the
Quantum limit — distributed 3+2+2+1+1+1 over the six tracks (the scene asks for
exactly ten notes, and the test fails if it does not), each track with
DIST 20 / CHORUS 30 / DELAY 25 / REVERB 20, master EQ active (PEAK, 1 kHz,
+12 dB, Q 64), master at 4096, one minute of continuous blocks of 32 frames
(82 687 blocks). All six tracks are melodic: a drum voice is one-shot and would
leave the ten by design, so the DRUM render path is covered separately by
`target_budget.py`'s "DRUM path" line.

Result on the host (wall clock, `-O2`):

```
six: 6 tracks, 10 notes, 10 voices, 82687 blocks of 32 frames (60 s)
six: p50 8.6 us, p99 16.1 us, p99.9 32.4 us, worst 2954.5 us, mean 9.0 us, deadline 725.0 us
six: blocks over the deadline 1 of 82687 (0.001 %), peak 19903, rail 0, zeros 4069 (0.077 %)
six: silent blocks 0 of 82687, fewest non-zero samples in a block 60 of 64
six: all passed
```

* **Silence is checked per block, not as an average.** Every block counts its
  non-zero samples; a block with none fails the run. Here none did, and the
  leanest block still carried 60 of 64 samples. A global ratio such as
  `pct_quiet < 50 %` would have allowed almost half the samples to be zero and
  is not used as the criterion.
* `voices_busy()` is sampled in every block and must equal 10; `voice_kills`
  must not move. This proves **voice-state continuity** — no voice was stolen,
  dropped or muted — and it does **not** prove that each of the six tracks
  produces independently audible output. Verifying that needs a per-track bus
  capture, which this harness does not do, so no such claim is made.
* Percentiles are computed with `qsort` over all 82 687 blocks, not a sample.
* The silent-sample ratio is a floating-point quotient, printed as 0.077 %.
* The worst block is printed as it is (2 954.5 µs here) together with the exact
  number of blocks over the deadline (1 of 82 687). That outlier is a scheduler
  stall of this shared VM — the same scene measured 100 µs on another run — but
  the document reports it rather than dismissing it.

**This is HOST TESTED.** It proves the scene, the mixing path and the absence of
clipping or silence at ten voices. It does **not** prove the AC791N's timing:
that would need the target and a scope, and remains NOT TESTED.

## 3. Memory and flash budget (target build)

`tests/memory_map_test.py` on the ELF produced by `tools/build.py`:

```
memory map: RAM spare 2572 B; pool spare 9248 B; RAMTEXT spare 2112 B; NOINIT spare 124 B
memory map: passed
```

`tests/target_budget.py` on the disassembly (static instruction cost of the
render loops, not a measurement): every checked loop is within budget, e.g.
`rev_spring 131 (budget 127)`, `perf_block 372 (budget 522)`, `perf_pre 31 (31)`,
`DRUM path 1189 (1191)`. Note `rev_spring` is over its nominal figure and the
script still accepts it, so that budget is a tolerance band, not a hard ceiling.
Package: 610 897 B total, 581 564 B application, in a 1 MiB flash with the app
window `[0x4000, 0x93000)`.

Headroom is therefore real but thin: 2 572 B of RAM and 9 248 B of pool. That is
the constraint every optimisation has to respect, and the reason no speculative
change was applied.

## 4. Optimisations: what was done and what was not

Applied in this mission:

* `fx_div64_32` replaces libgcc's 64-bit divide in the EQ coefficient
  normalisation (the target links no `__divdi3`/`__udivdi3`). It runs only on a
  parameter change, never per sample; the host sweep and the DSP budgets are
  unchanged, and the package builds again.

Not applied, and why:

* **No buffer, voice, effect or sample-rate reduction.** They would "win" the
  benchmark by changing the product, which the brief forbids.
* **No code moved to SRAM.** RAMTEXT spare is 2 112 B; moving a loop without a
  target profile would trade a measured budget for an unmeasured one.
* **No UI redraw change.** `ui_render` already reports 225 identical rewrites
  skipped per splash timeline and 0 lint; without a target profile there is no
  evidence the remaining draws are on the audio deadline.

A `callgrind` baseline exists (`tests/cpu_vg.py`, `tests/cpu_baseline_vg.txt`)
and is the right instrument for a per-sample profile; it measures host
instructions, so it can rank work but cannot set a target deadline.

## 5. Installer trust: what is and is not verified

The WebMIDI installer (`web/fm1ota.js`, `web/index_pkg.html`) performs a
handshake that returns a **34-byte identity**: `model_version` (e.g. `FM-1_982`).
It validates that identity, the container header, the size and the SHA-256
before writing, and it has a verify-only path.

What it does **not** do: read the SPI NOR **JEDEC id** from the device. The
identity packet has no field for it, and the firmware reports `model_version`
only. Therefore:

* The 1 MiB allowlist (`0x856014`, `0xB36014`) is enforced *in the firmware*
  (`project.c`) when it mounts flash, not by the installer before writing.
* The installer cannot distinguish a unit whose flash id it has never read.
* Claiming "physical flash identity verified before writing" would be false.

Status: **BLOCKED**. Closing this needs a firmware-side report of the JEDEC id
(a HAL read at boot, exposed in the identity reply) plus a hardware unit to
confirm the two documented ids. Adding a UI warning would not be a fix, so none
was added.

## 6. Boot and installation simulation: the real limits of `fm1-emu` 0.4.0

* `FM1_FLASH_CUT` does not exist in this build (not in `--help`, not in
  `strings`), so power cuts during a flash write cannot be injected.
* `--flash` rejects every image buildable from the package
  (`does not contain build/felucca.fwsc at 0x4120`).
* `--raw-flash` boots (2 500 ms, 180 LCD frames) but writes nothing:
  `changed bytes: 0`, storage sectors stay `0xFF`.

Consequences, stated plainly:

| Failure point | How it is covered | Status |
|---|---|---|
| Staging OTA erase/page | `tests/ota_cut_test.c` (23 cuts) | HOST TESTED |
| OTA cleanup / recovery | `tests/ota_cut_test.c` (3 cuts) | HOST TESTED |
| Persistent objects (40) | `tests/storage_cut_test.c` (160 cuts) | HOST TESTED |
| Application-area write | not modelable here | BLOCKED |
| Loader/bootloader write | outside this firmware | BLOCKED |
| Cold boot after a cut | `--raw-flash` is read-only | BLOCKED |
| Physical brownout / stuck bits | needs hardware | NOT TESTED |

The 186 host cuts do **not** prove the application is recoverable if power is
lost while the app area is written. Nothing in this document should be read as
an anti-brick guarantee.

## 7. References and what was actually re-checked here

| Project / source | Technique | Cost | Fit for Quantum | Risk |
|---|---|---|---|---|
| `fm1-emu` | full-system run, LCD/key/audio, flash image loading | none at runtime | already the CI smoke test | does not model power cuts |
| `m-bootloader` | loader/app contract, image validation | — | reference only (boot chain is out of scope) | copying it would change boot |
| `m-flasher` | full flash backup, sector map, selective write, readback | host-side | installer flow | needs the JEDEC id to be safe |
| `mc-1` | dual-core partition per voice/effect, deadline tests | — | matches CPU0/CPU1 split | its timing figures do not transfer |
| `FM-Stud1o` | eight tracks, load distribution | — | shows 6 tracks is conservative | different DSP |
| Felucca / fm1-base | storage, loader, audio, limits | — | the actual base | — |

Only the repository itself and the package were re-verified in this mission;
the other projects were reviewed earlier and are cited as design references,
not re-cloned here. No code was copied from them.

## 8. Verdict

* Six tracks with ten voices, per-track FX and the master EQ run without
  clipping, silence or voice loss **on the host**, and fit the static DSP
  budgets of the target build.
* The target package builds again after the 64-bit divide fix and passes the
  memory map, the DSP budgets and the package bounds.
* The installer validates identity, size, hash and container, but **cannot**
  verify the physical flash id; that is BLOCKED on a firmware-side JEDEC report
  and a physical unit.
* Recovery during an application-area write, cold boot after a cut, and all
  hardware behaviour remain unproven. Do not install on a physical FM-1 on the
  strength of this document.

## Target build of this branch (AC791N, JieLi toolchain)

Command: `JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK ./build.sh`
(toolchain `jieli-linux-toolchains-20260730.1`, host x86_64 Linux).

| Item | Value |
|---|---|
| Package | `build/felucca.fwsc`, 610 897 B, identity `FM-1_982` |
| Package SHA-256 | `a56cdcb9b32490cd4d3ec560e055d60beb7262ba0217e15eb2e1a9a414fa17c1` |
| App image | 556 152 B, SHA-256 `374222d760c052d51ea9d7cf1a9a7a7be66109806ce39ae6b204845c3168c35c` |
| Loader | `ota.bin` 7 741 B, SHA-256 `a1ad9343f2991267f88d98a59de63cbb9b309e88e494f7e80074c3004d878be0` |
| RAM `.data+.bss` | 95 732 B of 98 304 (spare 2 572 B) |
| Pool | spare 9 248 B |
| RAM code | 22 464 B of 24 576 (spare 2 112 B) |
| DSP budgets | `tests/target_budget.py` RC 0 (every render loop within budget) |
| Bounds | `web/test_pkg_bounds.mjs` RC 0 |

The build is reproducible: two independent runs from the same tree gave the same
package SHA-256. The splash change (`SPL_WRITE` 1100 / `SPL_HOLD` 1200) changes
the binary, so the `fa94ec5` hash `7f7a9ee8…` does not describe this commit; the
numbers above do.

**This is TARGET COMPILED and TARGET BUDGET VERIFIED. Timing on the AC791N is
still NOT TESTED.**

## CI 218: what failed and why

Run 41891015576 (`5c2ef79`) failed at `the emulator boots, plays, saves` one
second after it started, which aborted the job: the JieLi toolchain, the target
package and the target checks were **skipped**, and the fail-closed step then
reported `no target package was built`.

Root cause, reproduced here: `web/emu/emu_test.mjs` rendered 2 200 ms after
boot (200 + 800 + 1 200) and expected HOME by then. The slower splash lasts
2 300 ms (1 100 + 1 200), so the emulator was still inside it, HOME had not been
drawn and a key produced silence — `boots: the splash, then HOME (85 colours)`
and `a key sounds (peak 0.000)` failed on the WASM built from that exact commit.
The test now waits 2 500 ms; the same WASM reports HOME (246 colours), a key
(peak 0.114) and MIDI (0.113), with the heavy song at 2.3 % of real time.

The fail-closed control was not the cause: it correctly refused to publish when
no target package existed. It must stay as it is.

## Baseline of the integrated tree (v1.9.0, `d32a600`)

The numbers above belong to `perf/six-track-hardening` (`fa94ec5`). This is the same
harness run again on the tree that now carries the Quantum identity and the
transport hardening, so the optimisation work has a measured starting point. Host
only; nothing here is a cycle count of an FM-1.

```
build            : ./build.sh, clean tree, d32a600
image            : 559 844 B; RAM .data+.bss 96 484 B of 98 304 (1 820 B left);
                   pool 311 240 B, spare 9 248 B; RAM code 22 520 B of 24 576
package          : build/felucca.fwsc 611 163 B, identity FM-1_982, app 581 564 B
                   sha256 f157c893708e…
emulator         : build/emu/felucca.wasm, built from the same commit
```

Six tracks, ten voices, DIST/CHORUS/DELAY/REVERB per track and the master EQ,
one minute of 32-frame blocks (82 687), three runs of the same binary
(`cc -O1`, as `tests/run_tests.sh` builds it):

| run | p50 | p99 | p99.9 | mean | worst | late |
|---|---|---|---|---|---|---|
| 1 | 9.2 µs | 16.2 µs | 27.3 µs | 9.6 µs | 3 044.6 µs | 5 / 82 687 |
| 2 | 9.2 µs | 16.3 µs | 22.9 µs | 9.4 µs | 1 505.3 µs | 2 / 82 687 |
| 3 | 9.2 µs | 17.5 µs | 32.6 µs | 10.0 µs | 2 382.8 µs | 4 / 82 687 |

Deadline 725.0 µs. The audio was identical in the three runs (peak 19 918, rail 0,
zero samples 3 964 = 0.075 %), while the worst block moved by a factor of two and
the late count with it: those blocks are this shared VM preempting the process, not
the DSP. p99 is 2.2–2.4 % of the block and p99.9 3.2–4.5 %; that is the margin the
host clock shows, and it says nothing about the AC791N.

Static cost of the same tree (`tests/target_budget.py`, pi32v2 disassembly):
`analog_render` 603 (budget 581), `phase_render` 108 (budget 102) — unchanged by the
integration, both inside the +10 % tolerance.

### Where the host time goes (gprof, `-O1 -pg`, same scene)

| % | function | note |
|---|---|---|
| 44.0 | `analog_dispatch` | the ANALOG per-sample loop (the six tracks play preset 0) |
| 24.2 | `mix_part` | per-block: voices, inserts, pan, sends |
| 9.9 | `fx_buses` | chorus / delay / reverb buses |
| 5.5 | `qg_biquad_process` | master EQ |
| 4.4 | `rev_room` | ROOM reverb |

Line level: `clamp` (`dsp.c:14`, twice per sample per voice inside `tsvf_lp`) 6.0 %,
`tsvf_lp` 3.6 %, the oscillator/mix line of `analog_render` 3.0 %, `softclip` 3.0 %,
`qg_biquad_process` 2.4 %. `analog_render`'s loop is already lean on the target:
no divides inside it, one call (`an_pw`) outside it, 20 divides in the function's
setup only.

That profile is the reason no code change is claimed in this section: the per-sample
loop is ~155 instructions of fixed-point math with no redundant work left to remove
without changing what it computes, and a change that cannot be measured on an FM-1
would be a guess dressed as an optimisation. The candidates, in the order they are
worth trying, are the two `clamp` bounds in `tsvf_lp` (6 % of host time, needs a
bit-exact formulation), the insert chain in `mix_part`, and the reverb's inner loop.

### The three candidates, as they were tried (v1.9.0, after `dc647a4`)

**A. `tsvf_lp`'s two state clamps — applied.** `clamp_sym(v, 150000)` is `clamp(v, -150000,
150000)` in one unsigned compare. On the pi32v2 disassembly the ANALOG sample loop goes from
1250 to 1228 bytes (−22 B per iteration), `analog_render` from 609 to 603 instructions and its
cost from 603 to 577 (−4.3 %), back under its 581 budget; `tests/target_budget.py` is the
measurement, the audio is unchanged (220 golden renders, 0 changed). The identity is locked by
`tests/clamp_sym_test.c`.

**C. `clamp16` for the delay lines, reverb combs / allpasses and slicer buffers — refused.**
Substituting `clamp(v, -32768, 32767)` by the same one-compare form in the 12 stores of `fx.c`,
`perform.c` and `slicer.c` was measured **worse**, not better:

```
rev_room    cost 243 -> 251   (budget 243)
rev_spring  cost 131 -> 134   (budget 127)
RAM code    22520 -> 22548 B  (the headroom check needs >= 2048 B, this left 2028 B: build refused)
```

Those functions are in `.hot_text` (RAM), so the extra bytes cost RAM, and with constant bounds
clang's `-Os` codegen for the two-compare form is cheaper than an add plus an unsigned compare.
Reverted in full; the tree is back to 97 instructions and 243 for `rev_room`, 22520 B of RAM code.

**B. `mix_part`'s insert chain — no change.** At line level the cost is `sc[i] += mulq15(xs, c)`
and its two siblings (fx.c:2014, 2016, 2017): three multiplies and three read-modify-writes per
sample, one per send, which is the feature itself. The level and pan ramps are two multiplies and
two adds; the peak a compare. Nothing in the loop recomputes a value the loop has already got, and
the `clamp(x, ±xmax)` that guards `mulq15` overflow would have to become the same one-compare form
that A shows pays off only inside a two-clamp loop — one clamp is not enough to amortise it.
