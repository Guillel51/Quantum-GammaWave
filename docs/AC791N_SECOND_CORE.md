# AC791N SECOND CORE — stability, fallback and limits

The FM-1's SoC is a dual-core `pi32v2`. Core 0 runs the audio ISR, the UI and the display; core 1
renders some synth parts and the FX buses. This page describes the mechanism, the protections added
in v1.8.7 and — explicitly — what has **not** been measured.

Reference: <https://sbegueria.github.io/fm1-dev-wiki/Second-Core.html>. Startup sequence and the
memory-read guard come from Melodee (GPL-3.0) and fm1-x0x (GPL-3.0); this is JIANT's own version of
it, kept in `firmware/hal/fm1_multicore.h`.

---

## 1. Mechanism

* `fm1_mc_start()` at boot: core 1's entry address is written to `0x01C7FFF8` (protected top of RAM,
  before `fm1_guard_lock_top`), its interrupt bank is zeroed, `0x10008` bit 3 is held while it
  starts, then `C1_CON` bit 3 set / bit 1 cleared. Success = it reports `alive`.
* Per block, core 0 publishes a job (`fn`, `arg`, `job` number) with a check word
  `fn ^ arg ^ job*0x9E3779B1` and waits with an 8 ms bound (a block is ~0.7 ms).
* Core 1 validates the check word, runs the job, stamps its duration from TIMER4 and publishes
  `done`. Anything else increments `bad`; core 0 holds the core at the first bad value.
* The failure that motivates all of this is documented on real units: a 32-bit load can return
  `0x00200000` where the word holds 0, more often under load. A fault of core 1 is caught in
  `fm1_irq.h` (`fm1_core1_fault`).

## 2. Protections, and what each one catches

| Protection | Catches |
|-----------|---------|
| Check word over `fn`, `arg`, job number | a corrupted job descriptor (the wrong function or argument) |
| `job == done + 1` sequence check | a duplicated or skipped job |
| `bad` counter | core 1 refusing a job it read wrong |
| 8 ms wait bound | a job that never comes back (or comes back far too late) |
| FX sanity (`FX_SANE`, 2^20) | the buses' return or side going out of range, including a misread that no check word can catch (a reverb/delay state word) |
| `fm1_mc_bar` in `.noinit` | re-enabling a core that already failed, across soft resets |
| **Boot self-test (v1.8.7)** | a core that reaches its loop but reads or writes the shared mailbox wrong — checked *before* any audio is handed to it |
| `REC` held at power-on | a deliberate single-core session |

## 3. The boot self-test (new)

`fm1_mc_start()` used to trust `alive`. That only proves the core reached its loop. The new
`fm1_mc_selftest()` runs eight fixed jobs whose answers core 0 computes itself
(`firmware/hal/fm1_mc_selftest.h`, pure C):

* core 0 writes a job number plus a known pattern into the mailbox's spare words;
* core 1 reads the job number **through shared RAM** and writes the sequence for it;
* core 0 compares every word.

One wrong word, a changed job number, or a job that does not return in time and the core is held for
good: `fm1_mc_on = 0`, `fm1_mc_lost = 1` (`CORES 1 (BAD READ)` in `MENU > ABOUT`), and every part is
rendered by core 0 from the next block. The test costs a few hundred microseconds at boot and runs
outside the ISR.

## 4. Fallback semantics (unchanged, now reachable earlier)

When a job is lost mid-block, that block's core-1 parts **and** the FX buses are dropped, the
matrix state of the interrupted part is restored (`mc_lost()`), the core is barred until power-off,
and core 0 renders everything afterwards. Dropping the block is deliberate: the voices on core 1
have already advanced their state, so re-rendering would advance envelopes twice and click. The
audible result is one missing block (~0.7 ms) instead of a crash or a stuck core.

The unit keeps working with one core. The 10-voice budget, the effects and the tails are unchanged;
the second core is a rendering split, not extra polyphony.

## 5. What is *not* measured — read this before quoting numbers

* No physical FM-1 was available. There is **no** measured latency, jitter, CPU split or fallback
  rate in this document. `MENU > ABOUT` and the console report `mc_fx_tk`, `fm1_mc_job_us`,
  `fm1_mc_max_ticks` and the lost reason **on the unit**; those are the numbers to collect.
* Clock (320 vs 360 MHz), shared-RAM behaviour under thermal load and per-unit variation are not
  characterised here.
* The self-test is exercised on the host (`tests/mc_selftest_test.c`), which proves the payload and
  the comparison logic, not the silicon.

## 6. Long-running validation plan (not executed)

Inject: corrupted check words, wrong job numbers, duplicated jobs, a stopped core, a core that comes
back late, corrupted mailbox words, `0x00200000` misreads, rapid preset changes under maximum load,
audio + UI simultaneously, thousands of jobs, and a worker restart. Measure p50/p95/p99 job time,
fallback count and audio dropouts per hour. Record the unit's serial and clock with every result.
