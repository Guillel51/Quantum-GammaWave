# The voice pool (3.5 / 3.6): the allocator, the numbers, and the one thing it cannot prove

Branch `exp/voice-pool`. **Not merged into `main`.** The shipped build is unchanged: 10 voices, per-part
arrays, 220 golden renders identical.

## 1. Why a pool

A part used to own `voice_t v[NVOICE]` of its own, so six parts reserved `NPART * NVOICE = 60` slots of
88 B while the shared budget (`voice_cap()`) let only `NVOICE = 10` voices sound at once. Measured at
`5513692` with `tools/ram_report.py`, one more voice of budget cost 1134 B of `.bss`, and 1002 B of that
was duplication:

| symbol | 10 voices | 12 voices | per added voice |
|---|---|---|---|
| `drw_v` (`eng_wheel.c`, 92 B/slot) | 5520 | 6624 | +552 (6 x 92) |
| `trk[].v` (`core.h`, 88 B/slot) | 15600 | 16656 | +528 (6 x 88) |
| `vz_nv` (`ui_being.c`) | 480 | 576 | +48 |
| `pl_seen` (`ui_being.c`) | 60 | 72 | +6 |

## 2. Architecture

- `voice_t vpool[VPOOLN]` in `voice.c`: one storage for every voice.
- `track_t.vidx[NVOICE]` + `vn`: a part's **window** into the pool. `V(t, i)` in `core.h` is the lvalue
  every caller already used (`vpool[(t)->vidx[(i)]]`), so `voice.c`, the engines, the sequencer and the UI
  read a voice exactly as before. 100 call sites were converted mechanically and checked by the golden
  renders, not by eye.
- `VDEAD = VPOOLN - 1`: the pool's inert slot. A window's unused tail points at it, so every scan that
  still runs to `NVOICE` reads a silent voice that is never active, never free and never stolen. This is
  what keeps `for (i = 0; i < NVOICE; i++)` correct without rewriting ~50 loops.
- `vown[VPOOLN]`: who owns each slot (0 = nobody, p+1 = part p).
- Per-voice engine state follows the voice, not the part: `drw_v[VPOOLN]` is keyed by `VSLOT(v)`, so a
  voice that changes hands keeps its rotor phases and gains. `vindex(t, v)` gives the window index for the
  arrays that really are per part (`fm6_note`, drum lanes).
- `trk_vcap(t)` is the cap the engine and the LOAD setting give a part (what `trk_nvoice` used to return);
  `trk_nvoice(t)` is `min(cap, vn)`, the voices it can use right now.

### The allocator (3.6)

`voice_window_grow(t)` gives a part one more slot, and only ever a slot that is **not sounding**
(`!active && !stage`): a slot nobody owns, else one another part's window holds but does not use, from the
part with the widest window, never below one slot. No voice is moved, so nothing that sounds can click;
the answer is the same for the same state.

Two callers ask for capacity before they need it:

- `voice_alloc` (POLY): when every slot the part holds is sounding and the engine could take one more.
- `mono_play` (UNISON): the whole spread is claimed first (`voice_grow_to(t, trk_vcap(t))`), because a
  UNISON note starts its cap voices in one go and must not be left with the slots of the last phrase.

`voices_boot` splits the grantable slots (`VDEAD / NPART`, at least one each, never `VDEAD` itself).

## 3. Measured on AC791N (compiled here, not on a unit)

`python3 tools/build.py` with `FELUCCA_VOICES=n FELUCCA_VPOOL=m`, `tools/ram_report.py` for the regions.

| profile | image | XIP free | `.bss` used | `.bss` free | POOL free | NOINIT free |
|---|---|---|---|---|---|---|
| 10 voices, shipped (no pool) | 538356 | 63312 | 94924 | 3380 | 9248 | 124 |
| 12 voices, shipped (no pool) | 538820 | 62848 | 97196 | 1108 | 9248 | 124 |
| 10 voices, pool identity (61) | 539760 | 61824 | 95236 | 3068 | 9248 | 124 |
| **10 voices, pool 16** | 539532 | 62048 | **87084** | **11220** | 9248 | 124 |
| **12 voices, pool 18** | 539708 | 61872 | **87564** | **10740** | 9248 | 124 |
| 12 voices, pool 20 | 539708 | 61872 | 87916 | 10388 | 9248 | 124 |

- 10 voices: **7840 B freed** (94924 -> 87084).
- 12 voices: **9632 B freed** against today's 12-voice build, and 7360 B *less* than today's 10-voice
  build while sounding two more voices.
- POOL, NOINIT, LOWBUF, delay (131072 B), canvas (59520 B), cloud/slicer (32768 B) and comb (12288 B) do
  not move. The 8 KiB floor in POOL is untouched.

The size of a profile is `VPOOLN * 88` (voices) `+ NPART * NVOICE + 6` (windows) `+ VPOOLN` (ownership).

## 4. What passes

| run | golden | health | voice / routing | CPU |
|---|---|---|---|---|
| `NVOICE=10 VPOOLN=61` (shipped shape) | 220, **0 changed** | 0 | **0 failed** | 0 over |
| `NVOICE=10 VPOOLN=16` | 220, 53 changed | 0 | **0 failed** | 0 over |
| `NVOICE=12 VPOOLN=18` | 220, changed | 0 | **0 failed** | 0 over |
| `ui_test` at pool 16 and at identity | — | — | **all passed** | — |

The minimum that passes is `VPOOLN >= NVOICE + NPART` (16 for ten voices, 18 for twelve): one slot short
(15 / 17) and the mode-change and engine-cap checks fail, because five parts keep one slot each and the
sixth can no longer reach its cap. Below that the failures are real: extra UNISON voices not reclaimed,
leads lost on a mode change, a loud voice stolen where a quiet one was available.

Two defects were found and fixed on the way:

- `voices_boot` split `VPOOLN / NPART` slots per part, which for a pool that is a multiple of six handed
  the inert slot out as a window. It now splits `VDEAD / NPART`.
- UNISON and the mode-change paths never asked for capacity, so a part was stuck at its first share
  (2 slots) whatever its engine cap. `voice_grow_to` fixed it.

## 5. What does not pass: the sound is not identical

`tests/pool_equiv.sh` renders every golden scenario with two builds and compares peak, tail and voice
count (`GOLDEN_METRICS=1` in `tests/regress.c` prints them):

```
220 renders; 25 outside the tolerance (peak 2 %, tail 1 %, voices exact)
VERDICT: FAIL
  preset/LOFI/05_8BIT_KEYS      peak 18298 -> 21369 (+16.78%)  tail 0 -> 0  voices 5/5
  preset/ANALOG/36_COMB_PLUCK   peak 20053 -> 23136 (+15.37%)  tail 0 -> 0  voices 5/5
  preset/PHASE/22_ETHNIC_PLUCK  peak 20363 -> 22698 (+11.47%)  tail 0 -> 0  voices 5/5
```

Voice counts and release tails match exactly; what moves is the peak of a chord, by up to 1.4 dB. The
cause is structural, not a bug: a voice keeps its oscillator phases and filter state across notes (that is
what makes a retrigger click-free), so *which physical slot* a note lands in changes the phase relation
between the notes of a chord. The old layout gave every part a fixed block of ten slots; a pool of sixteen
cannot reproduce that numbering, so the phases differ.

That is a change of the instrument's sound, small but real, and the golden renders are the project's
contract for it. It is not something to slip in under a memory table.

## 6. Recommendation

1. **`main` stays as it is**: 10 voices, per-part arrays, 220 golden renders identical. Nothing here is
   merged.
2. The pool is worth keeping as the way to the 12-voice profile: at `FELUCCA_VOICES=12 FELUCCA_VPOOL=18`
   it passes every structural check and leaves 10740 B of `.bss` free, against 1108 B today.
3. To enable it, one decision is needed that is not a programmer's: **accept a one-time change of the
   chord phase** (a golden update of 53 renders, peaks within ±1.4 dB, tails and voice counts identical),
   or make the phase independent of the slot first — which changes the shipped sound too, so it is the
   same decision.
4. Still missing, and not obtainable here: the audio stack and the ISR timing on a unit. Every number
   above is AC791N code size and static RAM.

## 7. Reproducing

```sh
# the shipped shape, must be 220 / 0 changed
gcc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/r tests/regress.c -lm && /tmp/r tests/golden.txt tests/cpu_baseline.txt

# the pooled 10-voice window: every voice / routing check must pass
gcc -O2 -w -DVPOOLN=16 -Ibuild/gen -Ifirmware/src -o /tmp/r16 tests/regress.c -lm && /tmp/r16 tests/golden.txt tests/cpu_baseline.txt

# the audio comparison (host)
sh tests/pool_equiv.sh

# the target profiles
FELUCCA_VOICES=10 FELUCCA_VPOOL=16 JIELI_TOOLCHAIN=$PWD/.tools/toolchain AC79_SDK=$PWD/.tools/AC79_SDK python3 tools/build.py
FELUCCA_VOICES=12 FELUCCA_VPOOL=18 JIELI_TOOLCHAIN=$PWD/.tools/toolchain AC79_SDK=$PWD/.tools/AC79_SDK python3 tools/build.py
```
