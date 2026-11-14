# FASE 4 — Sound editing and musical tools: audit and first fix

Date: 2026-11-14. This phase is an audit before it is an addition: every claim below names the
instrument that produced it, and everything not measured is listed as open.

## Entry rule

Phases 1–3 are closed. The only open item of phase 1 is publication (no `release/felucca-v3.0.0.fwsc`
in the repository), which is not a code defect and does not block work here. Phase 3 closed with
`8959f88`: 0 lint, 0 alignment, 888 ui tests, build and budgets green.

## 1. The nine engines: does every EDIT knob do something?

`tests/engine_param_test.c` already asks exactly this question: for every engine, for the first,
middle and last preset, each EDIT index is forced to the descriptor's minimum and then to its
maximum, the same note is held and the same block is rendered, and the two buffers are compared
sample by sample. A knob whose note is bit-identical at both ends is a knob that does nothing.

    engine param audit: 216 knobs x 3 presets checked, 0 with no effect at all (none)

Host x86, this build, 2026-11-14. It also reports knobs that move the sound by 2 LSB or less
("FAINT") and printed none. So the answer to "is there a control that produces no change" is no,
measured — not assumed from reading the code.

What this instrument cannot say: whether a range is musical, and whether two knobs do the same
thing by different amounts. That is open, and it needs the unit and ears.

## 2. LFO and modulation

`tests/mod_test.c`: **71 checks, all ok** — four modulation slots, sixteen sources, twenty-six
destinations, per-source depth, RATE modulated within its own kind (free Hz below its zero, tempo
divisions above it), and the LFO's own page (RATE in Hz or in time, WAVE, PHASE, FADE, four
destinations). Nothing was duplicated and no second matrix was added: the existing one is what the
tests exercise.

## 3. ARP and SEQ

- `tests/ratchet_test.c`: **63 checks, all ok** (ratchet and gates).
- `tests/swing_test.c`: **11 checks, all ok** (swing and its relation to the grid).
- The ARP page carries MODE (15 modes), RATE (10 divisions incl. triplets and 2/4 bars), OCT, GATE,
  SWING, PROB, HOLD, ORDER; SEQ carries LEN, DIV, SWING, GATE — all range-checked in `params.c`.

## 4. SCALE and CHORD

- `tests/scale_test.c` and `tests/chord_scale_test.c` pass. The registrations from the earlier
  redesign are untouched.
- **Fixed here (UI):** on MENU > LARGE, SEQ's tools truncated the sound's name to three or four
  glyphs — `SAW LEAD` read `SAW…`, `FULL ORGAN` read `FUL…`, a project called `NO NAME` read
  `NO NA…` — because the page's title and its `n/m` index took the footer line. The title now gives
  up the index first, and only when the name would actually be cut (the test is the name's own
  width, not a fixed margin), so a page whose name fits keeps its counter. Measured by the render
  sweep: **516 ellipsised free texts (17 distinct) → 278 (7 distinct)**, with 0 lint findings and
  0 alignment deviations. What remains is the MIXER screen in LARGE, where the name still meets a
  hard edge — open.

## 5. FX

- `tests/fx_probe.c`: phaser comb depth, M-WIDE correlation (0 mono, 64 untouched, 127 at -0.59
  and level-matched within 0.0 dB), EQ low shelf monotone to -24 dB with no overshoot, peak filter
  reaching +11.7 dB at its centre — **all ok**.
- `tests/dly_echo_test.c` **18 ok** (repeat train, FEEDBK and MIX behaviour), `tests/reverb_test.c`
  **32 ok**, `tests/cloud_hot_test.c` **6 ok**, `tests/eq_sweep_test.c` **9 ok**.
- No effect was added: the DSP budget is the reason, and nothing measured here justified one.

## What is still open (the honest backlog)

1. **Musical ranges, not audibility.** The audit proves every knob moves the sound; it does not
   prove the range is useful. Next instrument: sweep each knob in steps and report where the
   audible change actually happens (a knob whose top half does nothing is a knob with a bad taper).
2. **MIXER's LARGE footer** still truncates (see §4).
3. **Parameter locks / automation depth** for SEQ is untouched: it is the largest real addition in
   this phase's list and it is not implemented here.
4. **Pops and clipping under parameter moves** are measured only where existing tests already look
   (delay train, reverb, cloud, EQ); a systematic "step every FX knob and look at the sample
   discontinuity" instrument does not exist yet.
5. **No hardware.** Nothing in this phase was heard on the FM-1.

This phase is therefore **open**, not complete. What is committed is the audit and one measured UI
fix; the entry rule for phase 5 is not met yet.
