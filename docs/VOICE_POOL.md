# The voice pool (3.5): what it is, what it costs, what it does not pass yet

Branch `exp/voice-pool`. **Not merged into `main` and not enabled in any shipped profile.** This page is
the record of the experiment: the architecture, the AC791N measurements, and the tests that failed.

## 1. Why a pool

A part used to own `voice_t v[NVOICE]` of its own (core.h). Six parts therefore reserved
`NPART * NVOICE = 6 * 10 = 60` slots of 88 B while the shared budget (`voice_cap()`) let only `NVOICE = 10`
voices sound at once. Measured at `5513692` with `tools/ram_report.py`, one more voice of budget cost
1134 B of `.bss`, and 1002 B of that was pure duplication:

| symbol | 10 voices | 12 voices | per added voice |
|---|---|---|---|
| `drw_v` (`eng_wheel.c`, 92 B/slot) | 5520 | 6624 | +552 (6 x 92) |
| `trk[].v` (`core.h`, 88 B/slot) | 15600 | 16656 | +528 (6 x 88) |
| `vz_nv` (`ui_being.c`) | 480 | 576 | +48 |
| `pl_seen` (`ui_being.c`) | 60 | 72 | +6 |

Six parts paid for capacity that only one budget of voices can ever use.

## 2. Architecture as implemented

- `voice_t vpool[VPOOLN]` in `voice.c`: one storage for every voice. `VPOOLN = NPART * NVOICE + 1` by
  default, i.e. the old shape plus one slot.
- `track_t.vidx[NVOICE]` and `track_t.vn`: a part's **window** into the pool. `V(t, i)` in `core.h` is the
  lvalue every caller already used (`vpool[(t)->vidx[(i)]]`), so `voice.c`, the engines, the sequencer and
  the UI read a voice exactly as before. 100 call sites were converted mechanically and verified by the
  golden renders, not by eye.
- `VDEAD = VPOOLN - 1`: the pool's inert slot. A window's unused tail points at it, so every scan that
  still runs to `NVOICE` reads a silent voice that is never active, never free and never stolen. This is
  what keeps `for (i = 0; i < NVOICE; i++)` correct without rewriting ~50 loops.
- `vown[VPOOLN]`: who owns each slot (0 = nobody, p+1 = part p).
- Per-voice state of the engines follows the voice, not the part: `drw_v[VPOOLN]` is keyed by
  `VSLOT(v)` (the pool slot), so a voice that changes hands keeps its rotor phases and gains.
  `vindex(t, v)` gives the window index for the per-part arrays that really are per part (`fm6_note`,
  drum lanes).
- `voice_window_grow(t)` (called from `voice_alloc` when every slot the part holds is sounding): it takes
  a slot nobody owns, else one that another part's window holds but whose voice is **not sounding**
  (`!active && !stage`), from the part with the widest window and never below one slot. No voice is moved:
  ownership of an idle slot changes hands. Deterministic, no allocation, no blocking, nothing that can
  click, because nothing that sounds is touched.

Shipped default (`VPOOLN = 61`) never grows (`vn == NVOICE`), so `main`'s behaviour is untouched by the
infrastructure: 220 golden renders, 0 changed.

## 3. Measured on AC791N (compiled here, not on a unit)

`python3 tools/build.py` with `FELUCCA_VOICES=n FELUCCA_VPOOL=m`, `tools/ram_report.py` for the regions.

| profile | image | XIP free | `.bss` used | `.bss` free | POOL free | NOINIT free |
|---|---|---|---|---|---|---|
| 10 voices, shipped (no pool) | 538356 | 63312 | 94924 | 3380 | 9248 | 124 |
| 12 voices, shipped (no pool) | 538820 | 62848 | 97196 | 1108 | 9248 | 124 |
| 10 voices, pool identity (61) | 539760 | 61824 | 95236 | 3068 | 9248 | 124 |
| 10 voices, pool 13 | 539532 | 62048 | 86548 | 11756 | 9248 | 124 |
| 10 voices, pool 15 | 539600 | 61984 | 86900 | 11404 | 9248 | 124 |
| 12 voices, pool 13 | 539708 | 61872 | 86660 | 11644 | 9248 | 124 |
| 12 voices, pool 15 | 539472 | 62112 | 86996 | 11308 | 9248 | 124 |
| 12 voices, pool 17 | 539472 | 62112 | 87364 | 10940 | 9248 | 124 |

The size of a profile is `VPOOLN * 88` (voices) `+ NPART * NVOICE + 6` (windows) `+ VPOOLN` (ownership).
Pool and NOINIT do not move: no buffer was traded for this, delay 131072 B, canvas 59520 B, cloud/slicer
32768 B and comb 12288 B are untouched.

**The number that matters:** 12 voices with a 17-slot pool uses 87364 B of `.bss` and leaves 10940 B free,
against 94924 B used and 3380 B free for the 10 voices of today. Two more voices *and* 7560 B more free.

## 4. The tests it fails

Golden renders change at any pool smaller than the identity, and that on its own is not a defect: which
physical slot a note lands in carries residual filter state, so the first samples differ. What matters is
that the correctness checks also fail, and they do:

| run | golden | health | voice / routing | CPU |
|---|---|---|---|---|
| `NVOICE=10 VPOOLN=61` (shipped shape) | 220, **0 changed** | 0 | **0 failed** | 0 over |
| `NVOICE=10 VPOOLN=15` | 220, 54 changed | 0 | **5 failed** | 0 over |
| `NVOICE=10 VPOOLN=17` | 220, 52 changed | 0 | **3 failed** | 0 over |

The failures are all capacity and mode-change cases, and they share one cause: a window that starts at
`VPOOLN / NPART` slots cannot always be grown back to the engine's cap while other parts keep one slot
each (five parts keep five slots; a part can reach at most `VPOOLN - 5`). Named failures at pool 15:

- `voices: budget after a live mode change` — POLY / UNISON chord -> MONO / LEGATO / UNISON: the extra
  voices of the wider mode are not all reclaimed.
- `voices: budget after voice mode changes` — POLY -> MONO / LEGATO and a lower UNISON cap: not all four
  leads kept.
- `voices: note-offs after live voice mode changes` — 76 cases: the lead / fallback priority is not
  preserved.
- `voices: stealing takes a quiet voice, not a loud one` — 10 held / 4 more notes.
- `voices: VOICE engine cap` — 4 voices in POLY, 2 in UNISON.

That is a real behavioural regression, not a golden-number question, so the pool does not go into `main`.

## 5. What the next attempt has to do

1. **Windows that can reach the cap.** Either reserve `NPART - 1` slots above the budget
   (`VPOOLN = NVOICE + NPART - 1`: 15 for ten voices, 17 for twelve) *and* prove the mode-change
   reclamation, or let a donor fall to zero slots and handle `vn == 0` in the MONO / LEGATO paths
   (`t->v[0]` is read directly there today).
2. **Growth on demand, not only when full.** A part that asks for a voice it cannot hold should trigger
   the lending before voice stealing, including in the mode-change paths that re-allocate.
3. **A test that says what equivalence means here.** Sample equality is the wrong instrument once the
   slot is allowed to differ: what has to match is the set of sounding notes, per-part voice counts,
   envelope stages, and a spectral / RMS tolerance over the render. `tests/test_polyphony.c` is a
   stand-alone model of the old fixed layout and has to become the model of the windowed one.
4. **Proof on a unit.** Every number above is AC791N code size and static RAM here. No `.bss` figure says
   anything about the audio stack, the ISR timing or the two cores on the FM-1. That validation is still
   missing, exactly as it was before this experiment.

## 6. Condition to enable 12 voices

The pool is not that condition. Even without it, 12 voices compile and keep the budget (RAM 97196,
1108 B free). What is missing for 12 voices is not storage: it is a measured audio-stack figure from a
unit, and a budget (`tests/target_budget.py`) taken at 12 voices rather than at 10.
