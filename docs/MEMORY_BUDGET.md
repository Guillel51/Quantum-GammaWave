# MEMORY BUDGET — AC791N

Budgets are contracts: a change that crosses one is a decision, not an accident. This page records
the numbers the tree is held to, what this change costs, and how to measure it. **No target build was
produced in this session** (JieLi's pi32v2 toolchain and the AC79 SDK are not installed here), so the
figures marked `[STATIC]` are the historical budgets and `[NOT-VERIFIED]` marks what needs a build.

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

`[NOT-VERIFIED]` — the exact byte counts need a target build:
`./build.sh` then `python3 tools/budget_report.py` and `python3 tests/target_budget.py
build/felucca.dis tests/target_budget.txt`.

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
