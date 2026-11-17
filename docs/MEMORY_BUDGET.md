# MEMORY BUDGET — AC791N

Budgets are contracts: a change that crosses one is a decision, not an accident. This page records
the numbers the tree is held to, what this change costs, and how to measure it. The figures in §2.1
are **measured**, not estimated: `./build.sh` ran with JieLi's pi32v2 toolchain
(`jieli-linux-toolchains-20260730.1`) and the pinned SDK (`AC79NN_SDK_V1.2.1_2023-12-13`, whose three
files match `SDK_SHA256` in `tools/build.py`).

---

## 1. Budgets

| Region | Budget | Source |
|--------|--------|--------|
| `.data + .bss` | 96.452 / 98.304 B | `firmware/app.ld`, checked by `tests/memory_map_test.py` |
| RAM code (`.ram_text`, `.c1_text`, `.hot_text`) | 22.528 / 24.576 B | app.ld + `tests/target_budget.py` |
| Pool spare (`POOL_END - pool_top`) | ≥ 8 KiB | `AGENTS.md` |
| DSP render, 32 samples @ 44.1 kHz | ≤ 522 instructions in the pi32v2 disassembly | `tests/target_budget.txt` |
| Stack (core 0 / core 1) | linker-checked, core 1's own EMU bank limit | `firmware/app.ld`, `fm1_multicore.h` |

The AC791N has 578 KB of SRAM, but the firmware's linker budget is 96 KiB for `.data + .bss`: the
rest of the window is not available to the application. Treating 578 KB as usable is how a build
"fits" and then fails at boot.

## 2. Cost of the v1.8.7 changes

| Change | Flash | RAM | RAM code | Note |
|--------|-------|-----|----------|------|
| Flash identification policy (`fm1_flash_id.h`, `flash_identify()`) | ~90 B of `.text`, no tables | **0** | 0 | pure comparisons; the existing RAM entry points are reused |
| OTA entry range checks + flags check + wider boot cleanup | ~70 B of `.text` | 0 | 0 (the boot cleanup uses 72 B of stack in a 1 KiB frame that already existed) | |
| Second-core boot self-test | ~120 B of `.text` (inlined payload, no tables) | **0** (uses 12 spare words inside the existing 128-byte mailbox) | 0 | runs once at boot, not in the ISR |
| Backup CLI / docs | 0 (host side) | 0 | 0 | not in the firmware image |

The self-test deliberately uses `fm1_mc_box.pad0` (spare words on core 0's cache line) instead of a
new buffer, so shared RAM does not grow and the mailbox stays 128 bytes / two cache lines.

### 2.1 Measured image — the tree at v1.9.0 after the `-Oz` campaign, `./build.sh`, identity `FM-1_982`

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 531 204 B | 568 KiB (581 632 B) | **50 428 B** |
| `.data + .bss` | 96 088 B | 98 304 B | **2 216 B** |
| RAM code (`.ram_text` + `.c1_text` + `.hot_text`) | 19 444 B | 24 576 B | **5 132 B** |
| Pool spare (`POOL_END - pool_top`) | 9 248 B (pool 311 240 B) | ≥ 8 192 B | **1 056 B** |
| XIP code / constants | 371 564 B / rest | constants only use slot | calls reach 88 920 B past the code |
| Loader image / `ota.bin` | 9 096 B at `0x01C0A800` / 7 650 B | `0x14000` B | large |
| Package (`build/felucca.fwsc`) | 610 806 B, sha256 `80ad995c…` (rebuilt & signed at `7eb9636`) | — | — |

These are the numbers the `-Oz` build (documented in `docs/VALIDATION_MATRIX.md` §16/§17) leaves, measured on
this tree: 531 204 B of image (the voice-stealing change of VALIDATION_MATRIX §22 added 32 B), 19 444 B of
RAM code and 2 216 B of `.data+.bss` headroom. The RAM headroom
did not move: `-Oz` buys flash and RAM code, never `.bss`, which is why the 12-voice question (VALI-
DATION_MATRIX §19) is a linker question and not a compiler one — 12 voices need 2 256 B of `.bss` and the
build has 2 216 B, so it overflows by exactly 40 B.

### 2.2 Where the RAM goes (measured, `objdump -t` on `build/felucca.elf`)

The 96 452 B are state, not slack. The largest objects:

| Bytes | Object | What it is |
|-------|--------|------------|
| 15 576 | `trk` | the six tracks: parameters, patterns, motion (the product's core state) |
| 7 104 | `fm6_note` | FM6 per-voice operator state, six voices |
| 6 160 | `up_bank` | the user preset bank's RAM mirror (flash is the copy of record) |
| 5 520 | `drw_v` | the UI's per-frame value cache |
| 5 376 | `drum_kit` | the DRUM kit's 16 pads |
| 3 840 | `st_buf` | the storage staging buffer (flash writes run with the part in SPI mode) |
| 3 728 | `upf` | the user-preset parser/editor state |
| 2 048 ×3 | `abuf`, `ua_ring`, `cdc_out` | audio output, USB audio ring, USB serial ring |

What was checked and **not** changed: there are no dead buffers (every one of these is read by the
build), the CDC rings are already inside `#if FELUCCA_CDC` (`usb.c`), and the storage-less build
(`FELUCCA_FLASH=0`) drops 12 096 B because `storage.c`, `upreset.c` and the TAKES reel are guarded.
Shrinking any of the objects above means removing a feature (a track, a voice, a pad, the preset
bank), which this project does not trade for a number; the honest headroom is the 1 852 B above.

The build's own checks (`tools/build.py check`) print these and pass, as do
`python3 tests/memory_map_test.py` and `python3 tests/target_budget.py build/felucca.dis
tests/target_budget.txt` (the render loops stay inside `tests/target_budget.txt`).

A storage-less build (`FELUCCA_FLASH=0 FELUCCA_OTA=0 ./build.sh`) measures 526 688 B image, 84 356 B
`.data+.bss`, 18 632 B RAM code, 9 344 B pool spare, and passes the same checks. It did not compile
before v1.8.8 (`flash_identify()` and the TAKES reel module were outside their `#if`).

## 3. How to measure (target toolchain present)

```bash
./build.sh
python3 tests/memory_map_test.py                 # regions do not overlap, sizes inside budget
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
python3 tools/budget_report.py                   # static RAM per voice count, library composition
nm --size-sort -S build/felucca.elf | tail -40   # the largest symbols
```

`tests/regress.c` keeps the CPU baseline for every preset and mix; `tests/cpu_baseline.txt` is the
reference and a +25 % drift fails. Sound or performance changes must be reviewed with those numbers,
never with a smaller test set.

## 4. Rules this change keeps

* No `malloc` in any realtime path (nothing here allocates at all).
* No new persistent parameter, no change to `P_COUNT`, project sizes or SysEx layouts.
* No voice-count change: 10 stays 10.
* Nothing moved from flash to RAM: the self-test and the identification policy are read-only code.

## 5. The host CPU gate (what it is, and what it is not)

`tests/regress.c` measures every preset and mix as 1 s of rendering after a 0.5 s settle, in its own child
process, and keeps two baselines:

* **Instructions per sample** (`tests/cpu_baseline.txt`), where the kernel exports a counter
  (`proc_pid_rusage`, macOS). This is deterministic for a given source, ISA and compiler: a row +25 % over
  its baseline is a regression, and that is a hard failure.
* **A per-host index** (`tests/cpu_ns_baseline.txt`), everywhere else. The index is the process CPU ns per
  sample divided by the ns per step of a fixed integer workload timed in the same child, and the job is
  measured `CPU_NS_REPS` (3) times with the smallest window counted. Absolute ns are not portable between
  machines and are not stable on a shared one, so they are never compared across hosts: the file has one
  `[host] architecture | processor | compiler` block per machine, and a host with no block reads
  **NOT MEASURED** (never `PASS`).

What this VM taught (and why the gate is shaped that way): with 1 s windows the same binary, run twice back
to back, swings 40-100 % on individual rows — SMT siblings stall FP-heavy and integer-heavy engines
differently — while the median of all 207 rows stayed at 1.00 thanks to the calibration workload. So the
gate is the **median** (+20 %), and a single row +40 % over is printed as a note to confirm, not as a
failure. That still catches what matters: a lost `-O2`/`-Oz`, a shared loop, a hot path that doubled. It
does not catch a 20 % regression in one engine on this host, and does not pretend to: the instruction
baseline is the precise one, and CI runs it where the kernel offers the counter.

Creating or replacing a baseline is deliberate (`BUDGET_UPDATE=1`), never the first run of a new host: it
rewrites *this host's* block only and keeps every other one, so a commit moves one machine's numbers.

```bash
BUDGET_UPDATE=1 ./build/host/regress tests/golden.txt tests/cpu_baseline.txt   # this host's block only
CPU_NS_BASE=/tmp/other.txt ./build/host/regress tests/golden.txt tests/cpu_baseline.txt
```

Proof the ns gate bites: with this host's own baseline scaled by 0.5 (a synthetic 100 % regression) the
run prints `CPU ns: FAIL (every job on this host is 101 % slower than its baseline …)` and exits 1; on the
unmodified baseline it prints `CPU ns: PASS (207 rows …, host median 1.00 …)` and exits 0.

## 6. Measured image — 2026-11-16 (FASE 1: XIP and RAM)

The tree at `21baf7d` measured with the pinned toolchain and SDK (`docs/QG_FASE1_OPTIMIZACION_2026-11-16.md`
has the method and the evidence): the XIP slot was the binding limit, **88 B** free.

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 582 388 B | 583 612 B (`APP_SLOT` 0x8E7BC) | **1 224 B** |
| `.data + .bss` | 90 676 B | 98 304 B | **7 628 B** |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 684 B | 24 576 B | 4 892 B |
| Pool spare (`POOL_END - pool_top`) | 9 248 B | ≥ 8 192 B | 1 056 B |
| XIP code / constants | 418 768 B / 143 140 B | — | — |

What moved: the glyph table stores each glyph's advance once instead of once per phase (`aag_t` 8 → 6 B,
−840 B of constants), `LFO_HZ_X100` is a `uint16_t` (−256 B), and the user-preset bank mirror (`up_bank`,
6 160 B, main loop only) moved from `.bss` to `.pool`, which took `.data+.bss` from 1 468 B of headroom to
7 628 B and left the pool at 9 248 B spare (above the 8 KiB floor). The move is a re-layout inside the same
SRAM window, not new memory. Code size is +36 B (pool addresses) and the package is unchanged (it is padded
to the slot). Verified with 293 golden renders (0 changed), the text reference (0 of 6 732 strings, 0 pixels),
the UI render sweep (0 lint findings, 0 alignment misses), `tests/target_budget.py` 39/39 at 0 % and
`tests/memory_map_test.py`.

## 7. Measured image — 2026-11-16 (FASE 2: the categories, and the room they left)

The same tree measured again after the preset categories went into the firmware
(`docs/QG_FASE2_CATEGORIAS_2026-11-16.md`): the feature costs **808 B of image** in total, because the three
savings below paid for it, and the XIP margin goes from 1 224 B to **416 B**: the categories are in, and the margin is thin.

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 583 196 B | 583 612 B (`APP_SLOT` 0x8E7BC) | **416 B** |
| `.data + .bss` | 90 676 B | 98 304 B | **7 628 B** |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 684 B | 24 576 B | 4 892 B |
| Pool spare (`POOL_END - pool_top`) | 9 248 B | ≥ 8 192 B | 1 056 B |
| XIP code / constants | 419 352 B / 143 364 B | — | — |

How it adds up, each step measured on its own:

| Step | Image | XIP code | Constants |
|------|-------|----------|-----------|
| FASE 1 (`3f6cb02`) | 582 388 B | 418 768 B | 143 140 B |
| + categories (table, `PCAT_*`, SELECT/PRESETS, LIST card) | 583 292 B | 419 332 B | 143 480 B |
| + the legacy aliases counted in `engine_t` (`legacy_n`, one loop less) and three unused 12 px icon cells | 583 164 B | 419 352 B | 143 332 B |
| (the S face's glyph boxes in four bits a field, 688 B, was tried and reverted: see below) | — | — | — |

The category table itself is 276 B of constants: `PCAT_NAME[12][9]` (108 B, no pointers, sized from the
longest name), `PCAT_BASE[NENGINES]` (30 B, `PCAT_GONE` where an engine has no sounds) and `PCAT_NIB` (138 B:
two sounds a byte for the 275 entries of the nine tables). Nothing is duplicated: a sound's name stays in its engine's table, its category is four
bits. The code is the two order helpers (which now answer in one compare when an engine has no legacy entry)
and the browser's composed filter, never a table walk per row.

## 8. Measured image — 2026-11-16 (FASE 6: the third wavetable band and the UI blitters)

The same tree after `docs/QG_FASE6_OPTIMIZACION_2026-11-16.md`: the WAVETABLE's last stored band limit (4 096 B
of flash) is now built at boot like the other two, and the glyph blitters skip what the canvas does not show.
The binding region was XIP and it is the one that changed.

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 579 148 B | 583 612 B (`APP_SLOT` 0x8E7BC) | **4 464 B** |
| `.data + .bss` | 94 772 B | 98 304 B | 3 532 B |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 684 B | 24 576 B | 4 892 B |
| Pool spare (`POOL_END - pool_top`) | 9 248 B | ≥ 8 192 B | 1 056 B |
| XIP code / constants | 419 408 B / 139 260 B | — | — |

`WT_GEN` holds the three band limits (12 288 B of `.bss`, +4 096 B) and `WT_L5` is gone from
`tools/gen_tables.py` (−4 096 B of constants). The generated band differs from the stored one by ±1 on 32 000
in 972 of its 2 048 points and the 293 golden renders are unchanged (measured). The blitters cost +56 B of XIP
and their draw cost on the host (`cc -O1`, median of 5 alternating runs) falls 3,2 % across 12 screens,
16,3 % on the credits. Both are in the FASE 6 page.

## 9. Measured image — 2026-11-17 (v1.3.2: the numbers and the tool that reports them)

Same toolchain and SDK as every section above. Every row was read twice: once from `tools/build.py` (the
gate that fails the build) and once from `tools/ram_report.py`, which now derives the six regions from the
same linker symbols. They agree to the byte.

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 582 864 B | 583 612 B (`APP_SLOT` 0x8E7BC) | **748 B** |
| `.data + .bss` | 95 012 B | 98 304 B | 3 292 B |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 676 B | 24 576 B | 4 900 B |
| Pool spare (`POOL_END - pool_top`) | 9 248 B (pool 318 432 B used) | ≥ 8 192 B | 1 056 B |
| LOWBUF | 26 724 B | 28 672 B | 1 948 B |
| NOINIT (`proj_slot[4]` + `felucca_dbg`) | 15 572 B | 15 696 B | 124 B |
| XIP code / constants | 422 308 B / 140 084 B | inside the image | — |

### 9.1 The report disagreed with the build, and why

`tools/ram_report.py` summed section sizes per region. Two of the six came out wrong:

* **XIP** listed only `.text` and so reported **21 252 B free**. The XIP region is filled by the whole image:
  `.text` plus the load copies of `.ram_text`, `.c1_text`, `.hot_text` and `.data` (`AT > XIP` in `app.ld`,
  19 676 + 796 B). The real figure is the one the packer enforces, **748 B**. A report that says a region is
  97 % empty while any new function fails the link is worse than no report.
* **RAM** summed `.data` (796 B) and `.bss` (94 180 B) and lost the 36 B the linker leaves between them
  (`.data` ends at `0x01C0831C`, `.bss` starts at `0x01C08340`), i.e. 3 328 B free instead of 3 292.

Both rows now come from the landmarks `tools/build.py` checks: `_bss_end - _data_start` for RAM,
`max(_pool_end, _c1_stack_hi) - POOL_ORIGIN` for the pool, `_low_end - _low_start`, `_hot_end - _rt_start`,
and the size of `build/felucca.bin` against `APP_SLOT` for XIP. `tests/memory_map_test.py` keeps the region
bounds, so the two tools can no longer drift apart silently.

### 9.2 What this release changed in the image

The HOME fix that 1.3.2 carries (one early return in `draw_columns()` instead of four cards drawn and then
covered, and `graph_home_view()` opening at `Y_LABEL`) removed **96 B** of code: image 582 928 → **582 832 B**.
The other half of the same version — SELECT no longer pages from HOME, and a tap needs a real release — costs
**32 B**, so the release builds at **582 864 B** with a slot margin of **748 B** (684 → 748 B). Nothing else in
the firmware changed: the three candidates of the audit (a memoised FM6 carrier mask, a last-use cache in `hc_lut()`, and `-fdata-sections`) were built,
measured and refused — +48 B, +16 B and −4 B of image respectively, none of them in the region that binds.

### 9.3 The FX scenes land, and the slot is down to 292 B

This version also carries the per-track FX scene (four sends plus the DIST type, four slots per track, 120 B of
`.bss`). Measured on the release build of `d4b5842` with the same toolchain and SDK:

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 583 320 B | 583 612 B (`APP_SLOT` 0x8E7BC) | **292 B** |
| `.data + .bss` | 95 124 B | 98 304 B | 3 180 B |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 632 B | 24 576 B | 4 944 B |
| Pool spare | 9 248 B (pool 318 432 B used) | ≥ 8 192 B | 1 056 B |
| LOWBUF | 26 724 B | 28 672 B | 1 948 B |
| NOINIT | 15 572 B | 15 696 B | 124 B |
| XIP code / constants | 422 780 B / 140 112 B | inside the image | — |

`tools/ram_report.py` and `tools/build.py` agree to the byte on all six regions. The scene costs **456 B** of
image (582 864 → 583 320 B), so the margin is now **292 B** — less than a third of what it was, and the reason the
next feature (the SLICER's per-step MODE/DEPTH, §8.4 of `BUILD_AC791N.md`) does not fit: it needs ~1 100 B and the
space behind the slot before the next partition is 725 B in total.
