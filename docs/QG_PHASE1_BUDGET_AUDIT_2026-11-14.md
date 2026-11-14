# Phase 1, first step — what the target cost gate really compares

Date: 2026-11-14, on the v3.1.1 tree (`75f48e5`). Host measurements only; nothing here was read from an FM-1.

## The question

`tests/target_budget.py` printed two lines that look like breaches and still exited 0:

```
target: formant_render    571 instructions,  238 in loops ... cost   238 (budget 229) ok
target: noise_render      591 instructions,  219 in loops ... cost   219 (budget 201) ok
```

## The answer: there is no invisible breach, and the gate is right

The number in brackets is the **baseline** recorded in `tests/target_budget.txt`, not the limit. The
check is `cost > baseline * (1 + TOL)` with `TOL = 0.10` — a 10 % tolerance, documented in the
module docstring and in `tests/run_tests.sh` line 16. Measured against that:

| function | cost | baseline | delta | limit (baseline × 1.10) | state |
|---|---|---|---|---|---|
| `formant_render` | 238 | 229 | +3.9 % | 251.9 | ok, 14 instructions of margin |
| `noise_render` | 219 | 201 | +9.0 % | 221.1 | ok, **2 instructions of margin** |
| `dx_run` | 426 | 426 | 0 % | 468.6 | ok, unchanged |
| `analog_render` | 138 | 148 | −7 % | 162.8 | ok, under baseline |
| `wheel_block` | 32 | 86 | −63 % | 94.6 | ok, baseline obsolete |

So the tolerance explains the arithmetic: 238 < 251.9 and 219 < 221.1. Nothing was hidden and no
comparison is weakened here.

## What was actually wrong: the report

A line reading `cost 238 (budget 229) ok` invites exactly the misreading this audit started with, and
the only way to resolve it was to open the script. The line now prints the delta and the limit, and
flags a margin thinner than 2 % of it:

```
target: noise_render  591 instructions, 219 in loops, ... cost 219 (budget 201) ok (+9 % of the baseline, limit +10 %, AT THE CEILING)
target: formant_render 571 instructions, 238 in loops, ... cost 238 (budget 229) ok (+4 % of the baseline, limit +10 %)
```

The same `ok (...)` detail is printed for the DRUM path total. No threshold, no baseline and no test
assertion changed; the exit status is computed exactly as before (verified: exit 0 on this build).

## The finding that follows from it

`noise_render` is **2 instructions from failing** its own gate: a 1 % addition inside its loop turns
the suite red. `formant_render` has 14. Those two, and `dx_run`/`dx_run_sn` (which carry the only
divide in a sample loop), are the honest targets for the optimisation phase — in that order, and each
one measured against the golden renders before and after.

`wheel_block` sits 63 % under a baseline that no longer describes the code; it must be re-recorded
with `BUDGET_UPDATE=1` and the change reviewed, not left as a stale number that would hide a later
regression of the same size.

## Class of measurement

Static instruction counts of one function's own loops in the pi32v2 disassembly, weighted 4× per
nesting level and 9× for a divide. It is not a cycle count, not a profile, and not a measurement on
the device: a real load figure needs the hardware.
