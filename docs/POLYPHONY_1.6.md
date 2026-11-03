# Polyphony 8 / 10 / 12 — QG-1 v1.6

## What was measured

Host build (x86-64, `gcc -O2`, `tests/regress.c`, six parts, the full mix and FX), on 2026-11-03.
`NVOICE` is compile-time, so each column is a separate binary (`-DNVOICE=8|10|12`).

| voices | `sizeof(voice_t)` | voices of 6 tracks | `6parts_full` ns/sample | health failures |
|---:|---:|---:|---:|---:|
| 8  | 88 B | 4,224 B | 367.2 | 0 |
| 10 | 88 B | 5,280 B | 351.9 | 0 |
| 12 | 88 B | 6,336 B | 388.0 | 0 |

- **RAM:** each extra voice costs 88 B per track, i.e. **+1,056 B** of static SRAM for 10 → 12 across the
  six tracks. The 8 KiB `POOL_END - pool_top` headroom rule is unaffected by the preset work; the voice
  arrays are not in the audio pool.
- **CPU:** the three runs are single host samples and the differences (352–388 ns/sample) are inside
  run-to-run noise; this host has no instruction counter (`proc_pid_rusage` absent), so the exact
  instructions/sample figure the budget uses was **not available** and no speedup is claimed.
- **Behaviour:** the golden suite reports 4 changed renders at 8 voices and 2 at 12, all in patches whose
  UNISON stack is wider than the table; that is the expected effect of changing `NVOICE`, not a regression.
- Health (clipping, DC, voices free after release, silence) passes at all three sizes.

## Decision

- **Default stays 10 voices.** It is the largest size with the current CPU budget, FX and six tracks that
  the project has shipped and measured on the host.
- **12 voices is not enabled by default.** It costs +1,056 B SRAM and its CPU margin on the AC791N is
  **NO VERIFICADO**: no board, no cycle counter and no trustworthy target measurement were available.
  It remains a compile-time experiment only: `-DNVOICE=12`.
- **Fallback:** the existing voice allocator, per-engine caps and fade-on-steal path are unchanged; a
  failed 12-voice experiment cannot leave the audio path without its budget, and returning to `NVOICE=10`
  is a rebuild with no data migration.

## What would verify 12 voices on hardware

1. Build with the JieLi toolchain and `-DNVOICE=12`; record `.text`/`.bss` and the pool headroom.
2. On an FM-1, run the six-part stress mix at 44.1 kHz with all FX; log underruns and the DSP load
   counter (`G_INFO`, `song.cpu_q8`) over at least 30 minutes.
3. Repeat at 10 voices as the control, at the same temperature and battery state.
4. Only then consider changing the default; any result below 10 % margin keeps 10.
