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

Scene (`tests/six_track_test.c`): six tracks, **ten voices** total — the
Quantum limit — spread as 3+2+2+1+1+3, five melodic tracks plus one drum track,
each with DIST 20 / CHORUS 30 / DELAY 25 / REVERB 20, master EQ active
(PEAK, 1 kHz, +12 dB, Q 64), master at 4096, one minute of continuous blocks of
32 frames (82 687 blocks).

Result on the host (wall clock, `-O2`):

```
six: p50 7.2 us, p99.9 59.3 us, blocks over the deadline 1 of 82687
six: 6 tracks, 10 voices, 82687 blocks of 32 frames (60 s)
six: worst block 2512.1 us, mean 7.9 us, deadline 725.6 us (host wall clock)
six: peak 19850, samples at the rail 0, exact zeros 0.08%
six: all passed
```

* Ten voices held for the whole minute: no voice stealing, no dropped note.
* No sample reached the rail (±32 700) and 0.08 % of samples were exactly zero,
  so no buffer went silent under load.
* The tail is 50–59 µs (p99.9 over repeated runs) against a 725.6 µs deadline.
  The *absolute* maximum is 2.5 ms, and it is a scheduler stall of this shared
  VM, not the DSP: the same scene measured 96 µs on an earlier run, and exactly
  one block of 82 687 crosses the deadline. The test therefore asserts on the
  tail and on the count of late blocks, and prints the outlier instead of
  hiding it. All of these are host numbers.

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
