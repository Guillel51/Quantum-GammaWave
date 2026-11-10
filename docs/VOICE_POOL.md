# The voice pool (3.5 / 3.6 / 3.7): the allocator, the numbers, and where the sound leaves

Branch `exp/voice-pool`. **Not merged into `main`.** The shipped build is unchanged: 10 voices, per-part
arrays, 220 golden renders identical, every voice / routing check passing.

## 1. Why a pool

A part used to own `voice_t v[NVOICE]`, so six parts reserved `NPART * NVOICE = 60` slots of 88 B while the
shared budget (`voice_cap()`) let only `NVOICE = 10` voices sound at once. Measured at `5513692` with
`tools/ram_report.py`, one more voice of budget cost 1134 B of `.bss` and 1002 B of that was duplication:
`drw_v` 552 B, `trk[].v` 528 B, `vz_nv` 48 B, `pl_seen` 6 B per voice.

## 2. Architecture

- `voice_t vpool[VPOOLN]`: one storage. `track_t.vidx[NVOICE]` + `vn`: a part's window. `V(t, i)` in
  `core.h` is the lvalue every caller already used, so the 100 call sites read a voice exactly as before.
- `VDEAD = VPOOLN - 1`: inert slot; a window's unused tail points at it, so every scan that runs to
  `NVOICE` reads a silent voice that is never active, never free and never stolen.
- `vown[VPOOLN]`: who owns each slot. `VSLOT(v)`: the physical slot. `vindex(t, v)`: the voice's place in
  its part (what `v - t->v` used to be).
- `trk_vcap(t)` is the cap the engine and LOAD give a part; `trk_nvoice(t) = min(cap, vn)`.
- `voice_window_grow` lends one slot at a time, only one that is **not sounding** (`!active && !stage`),
  from the widest window, never below one. `voice_alloc` asks when all its slots are sounding;
  `mono_play` asks for the whole UNISON spread first.
- `voices_boot` splits `VDEAD / NPART`.

## 3. Memory, measured on AC791N (compiled here, not on a unit)

| profile | image | XIP free | `.bss` used | `.bss` free | POOL | NOINIT |
|---|---|---|---|---|---|---|
| 10 voices, shipped | 538356 | 63312 | 94924 | 3380 | 9248 | 124 |
| 12 voices, shipped | 538820 | 62848 | 97196 | 1108 | 9248 | 124 |
| 10 voices, pool 16 | 539532 | 62048 | **87084** | **11220** | 9248 | 124 |
| 12 voices, pool 18 | 539708 | 61872 | **87564** | **10740** | 9248 | 124 |
| 12 voices, pool 20 | 539708 | 61872 | 87916 | 10388 | 9248 | 124 |

7840 B freed at 10 voices; 9632 B at 12. Delay (131072), canvas (59520), cloud/slicer (32768) and comb
(12288) do not move, and neither do POOL, NOINIT, LOWBUF or RAMTEXT.

## 4. What passes

| run | golden | health | voice / routing | CPU |
|---|---|---|---|---|
| `VPOOLN=61` (shipped shape) | 220, **0 changed** | 0 | **0 failed** | 39/39 |
| `VPOOLN=16` (10 voices) | 220, 52 changed | 0 | **0 failed** | 39/39 |
| `VPOOLN=18` (12 voices) | 220, 52 changed | 0 | **0 failed** | 39/39 |
| `ui_test` at 16 and at identity | — | — | **all passed** | — |

The minimum that passes is `VPOOLN >= NVOICE + NPART` (16 / 18). One slot short and the mode-change and
engine-cap checks fail, because five parts keep one slot each and the sixth can no longer reach its cap.

## 5. Where the sound leaves (measured, one cause at a time)

Every render differs from the old layout starting at **the same sample** (52866, block 413, t = 1.199 s) —
the note where a voice first reuses a slot that already played. That pointed at per-slot state, and it is
three separate things:

**(1) `voice_t` residual state.** A note starting on a voice that is not sounding inherits the oscillator
phases (`ph[3]`) and filter state (`s[8]`) of the last note that used that slot; the old layout had 60
slots, so a part's slot was reused much later, and by the same part. With `FELUCCA_VRESET=1` the state is
cleared when the voice starts from silence: differences 25 -> 19.

**(2) WHEEL seeds its wheels from the slot number.** `wheel_note_on` does
`ph[k] = k*0x9E3779B9 ^ vi*0x7F4A7C15` and `nz = 0x13579BDF + vi`, where `vi` became the *pool slot*
instead of the voice's place in its part. Two parts sharing a slot start their wheels on different phases.
With `FELUCCA_WSEED=1` the seed uses `vindex(t, v)` and a slot with no note reseeds: 19 -> 14, and all
remaining differences are WHEEL.

| build | vs the old layout | vs each other |
|---|---|---|
| pool 16, as shipped | 25 renders outside 2 % peak, 32 including a 2 % RMS bound | — |
| + `VRESET` | 19 | — |
| + `VRESET`, `WSEED` | — | **14 renders, all WHEEL** |
| shipped layout + `VRESET`, `WSEED` | **49 renders change** | — |

Measured with `tests/pool_equiv.sh`, which bounds peak, **RMS** (a phase shift cancels in RMS; an
amplitude or envelope change does not) and the tail: as shipped, 32 of 220 renders fall outside a 2 %
peak / 2 % RMS / 1 % tail window. The worst is +16.8 % peak (`LOFI/05_8BIT_KEYS`), and one single-voice
render (`ANALOG/38_SCREAM_LEAD`) moves 6 % in RMS with its peak nearly unchanged, which is envelope and
filter state, not phase.

**(3) The allocation order depends on the window.** `voice_alloc` rotates over `np = min(cap, vn)`, so
with a window of 2 grown to 5 the notes of a chord land in a different order of window indices than with a
fixed block of 10 — and the window index is what WHEEL seeds with. That is the 14 that remain. It cannot
be removed without either the old slot count or making the seed independent of the index, and the latter
changes the shipped sound as much as (2).

The three flags are in `core.h` and **off by default**; no shipped profile sets them. `docs/` keeps the
measurement because the price is now exact: 49 of 220 renders change when the three causes are removed,
i.e. roughly a fifth of the instrument's factory sounds would have to be re-blessed. That is a decision
about the sound, not about memory, and it is not one to take while chasing a `.bss` number.

## 6. Alternatives evaluated

| alternative | memory (10 voices) | equivalence | complexity / risk |
|---|---|---|---|
| Old layout (baseline) | 94924 B used | reference | reference |
| Shared pool, 16 slots | **87084 B** (7840 freed) | 25 renders move, up to 1.4 dB | implemented, all checks pass |
| Pool + per-note state reset (`VRESET`, `WSEED`) | 87084 B + ~200 B code | 14 renders move (WHEEL order), shipped sound changes 49 | implemented behind flags |
| Logical identity, compact per-voice state | — | does not help: the state that moves is the *voice's*, not the slot's; compacting it does not make it follow a different slot | analysed, not implemented; it would need a second copy of `ph[3]`+`s[8]` per logical voice, i.e. the same bytes it saves |
| Hybrid: K preferred slots per part + shared extras | `K*6*88 + extras*88 + 6*NVOICE + VPOOLN` — K=5: 2640 + 6 slots ≈ 3.2 KiB used, **~2.1 KiB freed** | only for parts that never exceed K voices *and* whose allocation order matches (the rotation depends on `np`), so not bit-exact | medium; needs its own equivalence run before any claim |
| 12 voices, pool 18 | **87564 B** (9632 freed vs today's 12) | as above | allocator checks pass; stack/CPU unmeasured |

`target_budget.py`: 39/39 in every profile above. Window growth is O(VPOOLN + NPART·NVOICE) and runs only
on note-on, never per sample; no cycles were measured on a unit.

## 7. Recommendation

**Keep the pool out of `main` and keep 10 voices as the shipped profile.** The allocator defect that the
previous pass reported is fixed — no lost notes, caps respected, mode changes reclaimed, 7.8 to 9.6 KiB
freed — but the sound still moves on 25 of 220 renders, and every route that removes the movement changes
the shipped sound by more renders (49) than the pool itself does (25).

The pool pays off for the 12-voice profile (87564 B, 10740 B free, against 1108 B today), so the order that
makes sense is:

1. Decide the sound question first: accept the three causes being removed (49 goldens re-blessed, peaks
   within ±1.4 dB, tails and voice counts identical), or freeze the current voice-start behaviour.
2. If accepted, enable `VRESET` + `WSEED`, re-bless the goldens in one reviewable commit, then run the
   structural suite at pool 16 / 18 and the 12-voice profile becomes a candidate.
3. Only then measure the audio stack and the ISR timing **on a unit**. Every number on this page is AC791N
   code size and static RAM.

## 8. Reproducing

```sh
# shipped shape: must be 220 / 0 changed
gcc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/r tests/regress.c -lm && /tmp/r tests/golden.txt tests/cpu_baseline.txt

# pooled 10-voice window: 0 voice / routing failures, 52 renders change
gcc -O2 -w -DVPOOLN=16 -Ibuild/gen -Ifirmware/src -o /tmp/r16 tests/regress.c -lm && /tmp/r16 tests/golden.txt tests/cpu_baseline.txt

# the audio comparison (peak, tail, voices, RMS)
sh tests/pool_equiv.sh                 # identity vs pool 16
sh tests/pool_equiv.sh -1 "-DVPOOLN=16 -DFELUCCA_VRESET=1 -DFELUCCA_WSEED=1"

# the raw samples of one render, for a per-sample diff
DUMP_DIR=/tmp/a /tmp/r  tests/golden.txt tests/cpu_baseline.txt
DUMP_DIR=/tmp/b /tmp/r16 tests/golden.txt tests/cpu_baseline.txt

# the target profiles
FELUCCA_VOICES=10 FELUCCA_VPOOL=16  JIELI_TOOLCHAIN=$PWD/.tools/toolchain AC79_SDK=$PWD/.tools/AC79_SDK python3 tools/build.py
FELUCCA_VOICES=12 FELUCCA_VPOOL=18  JIELI_TOOLCHAIN=$PWD/.tools/toolchain AC79_SDK=$PWD/.tools/AC79_SDK python3 tools/build.py
```
