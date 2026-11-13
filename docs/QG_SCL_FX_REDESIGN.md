# SCL and FX > BUSES, redesigned (6.1)

What changed on the SCL and FX sides of the instrument, why, what was measured and what could not be. The class of
every measurement is named: **host x86** (the firmware's own C compiled for the host, the same drawing code the panel
runs), **AC791N build** (the target compiler), **hardware** (nothing here — the owner's FM-1 was not touched).

## 1. What the two screens were

- **SCL** was six pages: `SCL` (a keyboard), `SCALE` (a two-column table of notes, intervals and each degree's triad),
  `CHORD` (a table of every degree's chord), `VOICING` (the four dispositions with a compass bar), `INTERVAL` (up and
  down from the root) and `PROG` (eight progressions). `SCL 2` (the song-wide scale transpose) lived at the end of the
  family, though it is a global.
- **FX** was nine sections (TRACK, DIST, AUTO PAN, DLY, REVERB, CHORUS, SLICER, MASTER, MASTER EQ / STEREO M.S), all
  of them knob pages. There was no page that showed the chain — sends → buses → returns → slicer → master — as a
  whole, and the per-bus meters only existed on the track's `FX` page.

## 2. SCL, three pages

| Page | What it shows | What its cards set |
|---|---|---|
| **SCALE** 1/3 | the twelve notes on a circle (tonic at the top, in-scale degrees joined in the track's colour, out-of-scale notes as dim stubs, a spoke from the centre to every degree), the keyboard under it with the scale lit and every sounding note marked, and the key / family / note count / QUANT state / the chord the CHORD page plays in the right column | ROOT, SCALE, QUANT, TRANS |
| **CHORD** 2/3 | the chord a key plays: its symbol, its notes, the voicing's name, the strum in ms, the degrees of the key as pills (the one the chord stands on in the accent, the last one the track played marked), and the voices as four lanes over the chord's own compass — the closer the dots the tighter the voicing, and the lanes step right with STRUM | CHORD, VOICING, STRUM, ROOT |
| **PROG** 3/3 | the eight progressions as pills, and each step's chord in this key (degree, root, notes, quality) | SCALE, ROOT, SCALE, CHORD (KNOB 1 walks the progression) |

Everything drawn is computed from the same `scale_mask()`, `scale_offsets()`, `chord_from()` and `chord_build()` the
sequencer plays, so a page cannot show a chord the key does not play. The circle and the keyboard read the same mask,
so they cannot disagree either.

**What left, and where its function is now:**

- `SCL` (the keyboard page): the keyboard is under the circle on SCALE, with the same information and the note names.
  The page also existed as the hold-layer map of the SCL button (`ui_layer.c` `LY_SCL`), which keeps drawing it.
- `VOICING`: it is CHORD's page 2 (the reference layout puts the voicing selector there), and the voices are drawn
  as lanes instead of four rows of text.
- `INTERVAL` and `KEYS`: the twelve keys with their degree are the circle; the interval from the root is the spoke
  and the ring segment. The up/down list was a second table of the same data.
- `PROG` stayed, restyled.

`SCL 2` moved to the global family (`GLO > SCL 2`), so the three SCL pages count 1/3, 2/3, 3/3 as the reference does.

## 3. FX > BUSES: the routing console

New page, `FX` family, right after the track's `FX` page. The footer says **BUSES 1/1** and the page title inside the
family follows the same rule as every other page.

- **KNOB 1** walks the links: TRACK, BUSES, DLY, REV, CHO, SLI, PUNCH, MASTER. The four cards above the panel become
  that link's parameters through `page_desc()` (`firmware/src/params.c`), so:
  - TRACK → the selected track's DLY / REV / CHORUS sends (`P_DLY`, `P_REV`, `P_CHOR`);
  - BUSES → the three buses' MIX / SIZE / DEPTH (`G_DMIX`, `G_RSIZE`, `G_CDEPTH`);
  - DLY / REV / CHO / SLI / PUNCH / MASTER → the three parameters of those pages (`G_DTIME`, `G_DFDBK`, `G_DMIX`,
    `G_RSIZE`, `G_RDAMP`, `G_RPRE`, `G_CRATE`, `G_CDEPTH`, `G_CSTER`, `G_SLMODE`, `G_SLPAT`, `G_SLDEP`, `G_PUNCH`,
    `G_DUCK`, `G_DREL`, `G_CLIP`, `G_MWIDTH`, `G_EQGAIN`).
- Six blocks, each with a drawing of its own kind, all from the DSP's own numbers:
  - **DELAY**: the repeats, spaced by TIME and each FEEDBK softer, their height from the delay bus's wet peak;
  - **REVERB**: the tail's length from SIZE and its height from the wet peak;
  - **CHORUS**: three taps as waves, their phase running with the sound, their sweep from DEPTH;
  - **SLICER**: the eight gates of PAT, the step's own lit while the sequencer plays;
  - **PUNCH**: the drum bus's transient (PUNCH's amount and the master peak) and the dip the others get (DUCK);
  - **MASTER**: the gain stage against its own scale, the output peak and the clip lamp.
- A block that is off, or a bus with no signal, draws nothing; the animation runs only on `fx_vu[]`, which `fx.c`
  measures per block. The page's redraw signature folds the peaks in, so a silent instrument leaves it still.

**No new DSP.** The console has no bus model of its own: it shows and edits the three global buses, the slicer on its
bus, the drum bus's punch/duck and the master exactly as `fx.c` runs them. Independent per-track buses were not
invented, because the engine has no such thing and pretending would be a lie on a screen.

## 4. Tests

| Test | Class | Result |
|---|---|---|
| `tests/ui_render` (every screen, 22 palettes) | host x86 | 189 screens, **0 lint findings**, 0 inks on their own colour, 0 GREY/MONO regressions, alignment 0 off by 1 px |
| `tests/ui_render.py` PNGs | host x86 (the firmware's drawing code) | `.shots/scl.png`, `.shots/chord.png`, `.shots/prog.png`, `.shots/buses.png` |
| `tests/ui_test` | host x86 | green, including the new FX navigation (FX → BUSES → DIST, sections TRACK/BUSES/DELAY/…) and GLO → SCL 2 |
| `chord_scale_test`, `mod_test`, `noise_test`, `regress`, `six_live_golden` | host x86 | green (see the suite log) |
| full suite `NO_PACKAGE=1 ./tests/run_tests.sh` | host x86 | see §6 |
| `tools/build.py` (AC791N) | AC791N build | see §6; RAM budget below |

## 5. What is not verified

- **Nothing was heard.** The measurements are the render sweep and the host tests: the pages are drawn, the cards
  move the parameters the engine reads (checked by `ui_test` through `page_desc`), and the meters read `fx_vu`. No
  listening test, no device.
- **The FM-1 was not touched**: no install, no flash write, no bootloader change.
- The **emulator** (WebAssembly) runs the same C, so the pages exist there too, but the WASM build was not rebuilt in
  this change; `sh web/emu/build.sh` produces it.
- The circle is drawn with a twelve-point ring and integer trigonometry; it was checked by the render sweep and the
  alignment lint, not by eye on a panel.

## 6. Results of the suite and the target build

Filled by the run recorded in `CHANGELOG.md` (6.1):

- `NO_PACKAGE=1 ./tests/run_tests.sh` (host x86): **ALL HOST TESTS PASSED**, including `ui_render`'s 189 screens × 22
  palettes with **0 lint findings** and **0 alignment misses**, `ui_test` with the new FX navigation, and the golden
  renders (285 renders, 0 changed, 0 health failures, 0 crashes).
- `tools/build.py` (AC791N, host toolchain in this VM): builds, image 580 096 B, **`.data+.bss` 87 760 B of 98 304**
  (10 544 B of headroom, unchanged: the pages add no RAM), XIP code 401 508 B (+2 440 B for the two new screens),
  pool spare 15 392 B (≥ 8 KiB), package 610 959 B. The build prints the SDK's `eq_cfg_hw.bin` difference warning it
  always printed for this SDK copy; it does not touch the firmware's own bytes.

## 7. Controls, in one table

| Where | Control | Does |
|---|---|---|
| SCL page | KNOB 1..4 | the four cards (ROOT / SCALE / QUANT / TRANS, or CHORD / VOICING / STRUM / ROOT, or the progression on PROG's KNOB 1) |
| SCL layer (hold SCL) | the layer's own map | the keyboard page with TRACK / CHORD |
| FX > BUSES | KNOB 1 | the link the console edits |
| FX > BUSES | KNOB 2..4 | that link's three parameters |
| FX > BUSES | FX button | next page of the family (DIST after BUSES); two quick taps, the next section |
