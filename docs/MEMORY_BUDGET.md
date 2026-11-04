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

### 2.1 Measured image — v1.9.0 (the integrated tree), `./build.sh`, identity `FM-1_982`

| Region | Measured | Budget | Headroom |
|--------|----------|--------|----------|
| App image (`build/felucca.bin`) | 559 536 B | 568 KiB (581 632 B) | **22 096 B** |
| `.data + .bss` | 96 452 B | 98 304 B | **1 852 B** |
| RAM code (`.ram_text` + `.c1_text` + `.hot_text`) | 22 500 B | 24 576 B | **2 076 B** |
| Pool spare (`POOL_END - pool_top`) | 9 248 B | ≥ 8 192 B | **1 056 B** |
| XIP code / constants | 397 056 B / 139 232 B | constants only use slot | calls reach 63 456 B past the code |
| Loader image / `ota.bin` | 9 524 B at `0x01C0A800` / 8 007 B | `0x14000` B | large |
| Package (`build/felucca.fwsc`) | 611 163 B | — | — |

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
