# JIANT-inspired pages (v1.13.0 / corrected in v2.0.1): ENV's stages and the FX pages that had no picture

> **Correction (v2.0.1).** The section below said `DLY`, `DLY 2`, `DLY 3`, `REVERB`, `CHORUS` and `MASTER` drew an
> empty panel. They did not: `ui_tech.c` already draws all six from the page id (`tech_dly`, `tech_rev`,
> `tech_chorus`, `tech_master`), with animation driven by the real bus peaks, and the `GR_NONE` in the table is what
> hands the page to them. The four static graphs this document describes **were removed in v2.0.1** and the pages are
> `GR_NONE` again; the test that caught it is `tests/ui_test`'s *"REVERB graph: the rings follow the bus's return"*.
> Only the ENV part of this document (the stage markers) is still in the firmware. See `docs/SCL_REWORK_2_0.md` §3b.

The reference was the ENV/STAGE and FX sheets of JIANT FM. What was taken is the *organisation* — a page that
shows what its knobs do — and not its palette, its layout or its wording: every graph here is drawn in Quantum
GammaWave's own colours, on QG's own panel rectangle, from QG's own numbers.

Every measurement below names its class. **Nothing was tested on an FM-1**: there is no unit in this environment,
`INSTALL BLOCKED` stands, and no claim about CPU, latency or how the panel looks on the real hardware is made.

## 1. What the audit found

- The ENV page already drew the real curve: attack as a line, decay and release walked with the DSP's own
  `ENV_K(n)` recurrence, and (since v1.12) a playhead that walks the *same* recurrence, so the dot sits on the ink.
  What it did not do was **name the stages**: the four segments were unmarked.
- `DLY`, `DLY 2`, `DLY 3`, `REVERB`, `CHORUS` and `MASTER` were declared `GR_NONE` in `params.c`: four knob cards
  above an **empty panel**.
- `FX > BUSES` (`ui_fxb.c`, 6.1) already drew all six of those blocks, 34 px tall, with real peaks. So the maths
  existed; what was missing was the page that owns the effect at full panel size.
- SCL already has its three pages (circle, keyboard with degrees, chord with voices and strum) from 6.1, and
  `SEQ > TOOLS` from 6.2: nothing of that was rebuilt here.

## 2. What was implemented

| Page | Graph | Read from |
|---|---|---|
| ENV, OP1..OP4 ENV | the four stages marked: a quiet guide at each boundary, `ATK` `DEC` `SUS` `REL` under their own segment, the live stage's letter in the accent, and the existing playhead dot | `P_ATK/P_DEC/P_SUS/P_REL`, `voice_t.stage`, `voice_t.env`, the same `ENV_K` the voice runs |
| DLY, DLY 2, DLY 3 | the repeats: one tap per echo, `TIME` sets their spacing, `FEEDBK` their decay (the count stops at −26 dB), `TONE` their colour, PING-PONG puts the odd taps under the axis | `G_DTIME`, `G_DFDBK`, `G_DCOLOR`, `G_DPING` |
| REVERB | the tail as bars: `SIZE` its length, `DAMP` the bend of its decay, `PRE` the gap before the room answers | `G_RSIZE`, `G_RDAMP`, `G_RPRE`, `G_RTYPE` |
| CHORUS, PHASER, FLANGER | the LFO the model runs, two to four taps, `RATE` the period, `DEPTH` the sweep, `STEREO` the phase between taps | `G_CRATE`, `G_CDEPTH`, `G_CSTER`, `G_CTYPE` |
| MASTER | the limiter's transfer: unity diagonal, `CLIP`'s knee, `DUCK`'s shelf, and the real stereo peak as a dot | `G_CLIP`, `G_DUCK`, `audio_out_peak(0/1)` |

No DSP was added and no stage was invented: a page cannot show a bus the engine does not run. The graphs read the
same parameters the cards edit, so moving a knob moves the drawing in the same frame.

**No new state.** The graphs are pure functions of `song.g[]`, `t->p[]`, `voice_t` and `fx_vu[]`: no RAM, no
buffer, no per-frame cache. They are redrawn by the page's existing signature (the FX pages fold in the peaks), so
a silent instrument leaves them still.

## 3. What it cost

- **AC791N build** (`tools/build.py`, toolchain JieLi `20260730.1`): image **583 524 B of 583 612** — 88 B of
  headroom left, the tightest this tree has been. `.data+.bss` **92 688 B of 98 304** (unchanged: no new state),
  pool spare 8 384 B, LOWBUF 31 648/32 768.
- **Making it fit**: the first build overflowed XIP by 584 B. The four captions that repeated the cards' own
  headings (`SPRING`, `CHORUS`, `"4 ECHOES"`, `CLIP`/`IN`) were removed, the ENV guides became one quiet line
  instead of a dotted one, and the chorus wave gained linear interpolation instead of a fifth of its own table.
  What is left is the drawing; the words are on the cards above it.
- **Host render sweep** (`tests/ui_render`): 196 screens × 22 palettes, **0 lint findings**, **0 alignment
  misses**, 0 inks on their own colour.

## 4. What is not verified

- **Nothing was heard and nothing was seen on an FM-1.** The claims are the render sweep, the host tests and the
  AC791N link.
- The chorus wave was checked in a 2× render of the QUANTUM palette: it is smooth there; the panel itself was not
  looked at.
- The emulator runs the same C, but its WebAssembly build was rebuilt only for the release; parity is checked by
  `web/emu/emu_test.mjs` as part of `tests/run_tests.sh`.

## 5. What is still open

- The `DIST`, `AUTO PAN`, `REVERB 2`, `MASTER EQ` and `STEREO M/S` pages remain `GR_NONE`. Their blocks exist in
  the engine, but there is no headroom left in the app slot: they need code freed elsewhere first.
- Every animation here is bounded by the page's signature, not by a timer; a per-frame moving dot on the delay's
  taps (like JIANT's travelling echo) would cost a signature change and was not added.
