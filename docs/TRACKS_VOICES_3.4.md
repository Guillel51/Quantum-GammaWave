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
