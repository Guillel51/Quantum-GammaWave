# TRACKS 4 / 5 / 6, and what the 12-voice build costs (3.4)

Measured on this tree (`bfb59c0` + the 3.4 work), class `[TARGET]` for every memory and size figure:
the AC791N build with the pinned pi32v2 toolchain, `tools/build.py` and `tools/ram_report.py`. Nothing
here was measured on a physical FM-1, and no unit was touched.

## 1. The three track modes

`MENU > TRACKS` and `GLO > SYSTEM`'s KNOB 1 now step **4 ECO / 5 BAL / 6 FULL** instead of 4 / 6.

| mode | `trk_live` | what sleeps | what still exists |
|---|---|---|---|
| `4 ECO` | 4 | T5, T6 | their steps, presets, automation, FX, mixer rows and project slots |
| `5 BAL` | 5 | T6 | the same for T6 |
| `6 FULL` | 6 | nothing | everything |

- **Storage.** `favorites.factory[15][18]` (one of the bytes of an engine that does not exist, the block
  the other device settings already use): 0 = 4, 1 = 5, 2 = 6, and `0` in every older setting means the
  four tracks it had. The old `PREF_SIX` bit is still written when the mode is 6, so a unit that goes back
  to an older build keeps six tracks instead of silently losing two; reading an older setting that has the
  bit set and no byte gives 6.
- **Sleeping tracks are real.** `fx.c mix_part()` already faded a sleeping part's voices out and let its
  mute go when `trk_live` dropped (0.6.18/0.6.19); that path is what the 5-track mode uses, unchanged. No
  voice is left hanging: a note sent to a sleeping track says so on screen (`T5-T6 ASLEEP` at four tracks,
  `T6 ASLEEP` at five) and points at the setting that wakes it.
- **CPU.** The per-track render, FX and mixer loops iterate `i < trk_live` (`fx.c:2210`, `mix_part`), so a
  sleeping track costs no DSP. That is a code-path argument, not a measurement: the target has no cycle
  counter exported to a host harness, and the host `ns` counter is declared ±40 % by its own test.
- **Compatibility.** The setting lives with the settings, not in a song: projects, presets, patterns and
  automation load unchanged. Older firmware reads 4 or 6 as before. MIDI, SysEx, the sequencer, the mixer
  and the editor protocol keep their six objects.
- **Tests.** `tests/ui_test.c`: the MENU row steps 4 → 5 → 6 and stops; `trk_live` follows; the older bit
  is clear at 5 and set at 6; `GLO > SYSTEM` steps the same three and shows `5 TRACKS: T6 ASLEEP` /
  `6 TRACKS: FULL`; ALGORITHM stops at T5 in the 5-track mode and at T6 in the 6-track one; a note sent
  to a sleeping T5 is refused with the message.

## 2. Twelve voices: measured, not enabled

`firmware/src/core.h` has always allowed `NVOICE` to be overridden for exactly this experiment
(`FELUCCA_VOICES=12` for the target, `POLY=12` for the host tests). Both profiles were built here.

| | 10 voices (shipped) | 12 voices (`FELUCCA_VOICES=12`) |
|---|---|---|
| image (`felucca.bin`) | **537 532 B** | **537 980 B** (+448) |
| XIP used / free | 517 428 / 64 136 B | 517 876 / 63 688 B |
| `.data+.bss` (RAM) | **94 924 / 98 304 B** | **97 196 / 98 304 B** |
| RAM free | **3 380 B** | **1 108 B** |
| POOL used / free | 318 432 / 9 248 B | 318 432 / 9 248 B |
| LOWBUF / RAMTEXT / NOINIT | 26 532 / 19 356 / 15 572 B | unchanged |
| `target_budget` | 39 ok, 0 out | not run on this profile |

The twelve extra slots (two per part, six parts) cost **2 272 B** of static RAM, so a voice slot is about
**189 B** on this ABI — `voice_t` plus the per-voice engine state that grows with `NVOICE` (`fm6_note`,
`drw_v`, the DIGITAL tables).

**Recommendation: keep 10 as the shipped value.** At 12 the static RAM is at 98.9 % with 1 108 B free,
which is below the headroom the 96 KiB rule was written to keep, and nothing here can measure the stack
of the audio path on the target. Twelve voices is buildable, documented and measurable — not safe as a
default on evidence available in this environment.

## 3. The global voice pool: what the numbers say

The per-part reserve is `NVOICE` slots in every part: at 12 voices that is 6 × 12 = **72 slots**, about
**13.6 KiB** of static RAM at 189 B a slot, of which a typical performance uses a handful.

A single pool of, say, 16 slots would need 16 × 189 ≈ **3.0 KiB**, i.e. **up to ~10.6 KiB** of static RAM
back — enough to make 12 (or more) voices comfortable, or to give the FM6/DRUM per-voice state room.

Why it is **not implemented here**: it is not a byte-saving change, it is a change of ownership. Today a
voice belongs to `trk[k].v[i]`, and that identity is assumed by note-on/off per track, UNISON's voice
spread, LEGATO's voice reuse, POLY's per-track cap, `voice_kill()` on a sleeping or muted track, FM6's
per-note operator state (`fm6_note` indexed by part and voice), DRUM-X's per-slot state, the MIXER's
per-track peak, and the editor protocol's per-track view. A pool needs an owner field, per-track free
lists, cross-track stealing that respects UNISON groups, and a rule for what a released voice does when
its track goes to sleep — and every one of those is a behaviour change that the 220 golden renders and
the CPU budget would have to accept or reject.

Doing that properly is a branch of its own with a before/after harness (identical goldens at 10 voices,
two-account-style stress on stealing, sleep/wake of tracks, MIDI and sustain), which is more than this
pass could verify. The measurement above is what a follow-up needs to justify it: the saving is real and
large, and it is the only route to more voices on this part.

## 4. What this pass did not touch

- No DSP change: no division replaced, no loop rewritten, no buffer resized.
- `TIMER5`, `dx_run`, the effect buses and the FM6 core are as they were.
- No new engine.

## 5. The obvious shortcut, measured and rejected

The cheapest way to make twelve voices fit looked like rebalancing the two RAM regions: `.bss` is what runs
out, and `.pool` (the audio buffers' region) had 9 248 B spare. Two objects are plainly main-loop state, never
touched by an audio block: `storage.c`'s `st_buf` (3 840 B, the flash driver's staging buffer) and `gen.c`'s
`gen_keep` (2 176 B, the two patterns the generator holds for its preview and its UNDO). Both were moved to
`.pool` by section attribute and both profiles rebuilt:

| | `.bss` free | `.pool` free | twelve voices: `.bss` free |
|---|---|---|---|
| shipped | 3 380 B | **9 248 B** | 1 108 B |
| both moved to `.pool` | **7 220 B** | 3 232 B | **7 124 B** |

It works, and it is **not shippable**: `docs/OPTIMIZATION_BASELINE.md` sets a floor of **8 KiB free in POOL**
(the region also carries the second core's stack), and this leaves 3 232 B. Moving only `st_buf` leaves
5 408 B — still under the floor. The moves were **reverted**; the tree is back at 3 380 / 9 248.

What that leaves: the `.bss` wall cannot be walked around by moving main-loop state into the pool, because the
pool has only 1 056 B of slack above its own floor. A rebalance would need ≥ 3 232 B of `.pool` freed first
without touching an audio buffer — the pool's big citizens are the delay line (131 072 B), the canvas
(59 520 B), the cloud and slicer buffers (32 768 B each) and the comb bank (12 288 B), none of which can
shrink without changing the instrument. That is exactly why the **global voice pool** is the only route that
both frees `.bss` (the 72 slots → 16) and leaves the audio buffers alone.

## 6. Where the per-voice bytes actually are (measured, symbol by symbol)

`tools/ram_report.py --json` on the two profiles (`FELUCCA_VOICES=10` and `=12`, same toolchain, same tree)
gives the growth of every named RAM symbol, so the cost of a voice is no longer an average:

| symbol | 10 voices | 12 voices | growth | per added voice | what it is |
|---|---|---|---|---|---|
| `drw_v` (eng_wheel.c) | 5 520 B | 6 624 B | **+1 104 B** | 552 B (6 × 92) | the wheel engine's per-voice state |
| `trk[k].v[]` (core.h) | 15 600 B | 16 656 B | **+1 056 B** | 528 B (6 × 88) | `voice_t` itself, 88 B per part-slot |
| `vz_nv` | 480 B | 576 B | +96 B | 48 B (6 × 8) | the visualiser's per-voice note cache |
| `pl_seen` | 60 B | 72 B | +12 B | 6 B (6 × 1) | voice-stealing "already counted" marks |
| everything else | — | — | **0** | 0 | **`fm6_note`, `drum_kit`, `dx_core` do not grow with NVOICE** |
| **total** | 94 924 B | 97 196 B | **+2 272 B** | **1 134 B** | (2 × 1 134 plus 4 B of alignment) |

Two things follow that we did not know before this measurement:

- **A voice costs 189 B of storage per part** (88 + 92 + 8 + 1), not 189 B once: the 11 340 B that six parts
  of ten voices hold is `6 × 10 × 189`. The per-engine FM6, drum and wheel *kits* are separate and fixed.
- **The per-slot dimension is the only one that scales**, which is exactly the dimension a global pool
  collapses. Nothing else in RAM has to move for it.

### What a global pool is worth, in bytes

Slots today: 6 parts × 10 = **60 slots, 11 340 B**. A shared pool holds `S` slots and every part draws from it:

| pool | storage | freed from `.bss` | `.bss` free (10-voice base) | notes per part if evenly split |
|---|---|---|---|---|
| 12 slots | 2 268 B | **9 072 B** | **12 452 B** | 2 — needs borrowing, see below |
| 14 slots | 2 646 B | **8 694 B** | 12 074 B | 2 |
| 16 slots | 3 024 B | **8 316 B** | 11 696 B | 2 |
| 18 slots | 3 402 B | **7 938 B** | 11 318 B | 3 |

The point of the last column: **a fixed split does not work**. Twelve slots over six parts is two each, worse
than today's ten. The pool only pays off with *dynamic* windows — a part that is playing four notes holds four
adjacent slots, and a part that is idle holds none, borrowing from its neighbours. That is the whole design
problem, and it is a semantics problem, not a byte-counting one.

### What has to change, exactly

`trk[k].v[i]` is written at **100 sites in 18 files** (`voice.c` 44, `ui_graph.c` 7, `editor.c` 7,
`ui_input.c` 6, `fx.c` 6, `seq.c` 5, `ui_tech.c` 4, `gfx.c` 4, `mod.c` 3, and ten files with 1-2), plus
`drw_v[3]` uses of the wheel's per-slot array. Every one of them assumes the part's voices are a contiguous
array it can index. So the design that keeps those 100 sites untouched is **a contiguous sliding window per
part**: `voice_t *v` points into one global `vpool[VOICE_SLOTS]`, a part owns `[base, base + n)` and grows or
shrinks by trading boundary slots with its neighbours at a block boundary. Indexing stays `trk[k].v[i]`;
only the base moves. The open questions are the ones a prototype must answer with tests, not prose:

- compaction relocates live voices (a 88 B memmove) — only safe at a block start, with the second core's
  engine state quiesced, and it must not desynchronise `drw_v`, `vz_nv` and `pl_seen`, which ride along;
- stealing must stay per-part first, global second, and never take a slot from a part that is under its own
  POLY limit while the thief is at it;
- UNISON groups are 2-4 slots of one note: a window move must not split a group;
- `sizeof trk[k].v` and every `memset(trk[k].v, …)` has to be audited (they would shrink to a pointer).

Until a prototype passes the equivalence suite (six parts, twelve notes, UNISON, LEGATO, sustain, mute, preset
change under load, FM6 and DRUM-X under load), **the pool stays unmerged and the shipped instrument stays at
ten voices**. Branch `exp/voice-pool` was created for it and deleted again without a commit: the measured
accounting above is the part of the work that is finished, and pretending otherwise would put a half-moved
allocator in a tree that thousands of lines of UI and editor code index into.
