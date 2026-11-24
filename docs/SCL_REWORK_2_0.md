# SCL rework and the 2.0 keyboard (v2.0.1)

This is the SCL pass of the 2.0 line: the keybed was the smallest thing on the panel and the least informative, and the
three Scale/Chord pages had a row of air over it. Everything below is measured on the host renderer and the AC791N
build. **No FM-1 was used**: there is no unit here, nothing was flashed, and no claim about the physical panel, its
touch behaviour or its audio is made.

## 1. What the audit found

- **The keybed was 26 px tall** (`8,92,224,26` on SCALE, `12,90,216,26` on CHORD) with 4 px of plate under it, and the
  naturals' names were centred in the whole key instead of in the white part below the accidentals: at 26 px the name's
  last row landed on the key's own bottom edge.
- **A third text row (IVL) sat alone under DEG**, mostly empty, while the keyboard went without the space.
- **The live marker was a 1–2 px bar** at the foot of a key: a sounding note and a picked note were told apart by a few
  pixels in the same corner.
- **The CHORD page's ruler was display-only**: the chips on the axis are the chord's notes, and a tap there did nothing.
- The SCALE layer (`graph_scale`, the keyboard the SCL button shows) carried its **own key renderer**, a second copy of
  the same bevel in the same translation unit.

## 2. What was implemented

| Where | Change |
|---|---|
| SCALE | keybed `8,84,224,38` (**+12 px**), the three text rows are two: the chord and its notes at y 56, `DEG`/`STEPS` and `IVL` sharing y 68 (the second column starts where the first one's text ends, so twelve semitone steps fit when nothing is picked) |
| CHORD | keybed `12,86,216,36`, the interval ruler moved up 4 px (`ay` 72 → 68) so its names clear the keys |
| Names | naturals are centred in the **white band under the accidentals** (and fall back to the key's centre only when that band is too short); accidentals keep their name inside their own face. Nothing is drawn outside a key |
| Live notes | a sounding key is **lit by its face** (theme blend) plus a 2 px accent bar at its foot — picked notes keep their accent **border** and top strip, so the two states never read as one |
| Hit animation | the live mask is compared with the previous frame: a note that **starts** takes 24 % more accent for ten frames. Driven by `voice_t` (the voices the engine sounds), so it covers a held key, a MIDI note, a sequencer step and the arpeggiator, and nothing pulses when nothing plays |
| Interaction | on CHORD, the ruler's chips are a keybed too: a tap picks or drops that semitone, from `scl_ruler_geo()` — the same geometry the chips are drawn with |
| Renderer | `scl_key()` (the SCALE layer's private bevel) is gone: `graph_scale` draws with `scl_pkey()`, the one the SCL pages use |

No new DSP, no new parameter, no new persisted state: the picture is a function of `scale_mask()`, `scl_selm[]`,
`voice_t` and `ui.frame`.

## 3. The mini optimisation, measured

The 2.0 keyboard and the two new interactions had to be paid for inside a slot that had **88 B free**, so this pass is
also a small, measurable size optimisation:

- `scl_key()` (29 lines: three corner loops, nine rects, a contour) was a **second implementation** of the bevel that
  `scl_pkey()` already draws. Removing it and calling `scl_pkey()` from `graph_scale` also unifies the SCL layer's look
  with the SCL pages.
- The SCALE page's third row became a column, not a row.

Result: **AC791N image 583 524 B → 583 444 B (−80 B)** while adding the taller keybed, the hit animation and the ruler
taps; `.data+.bss` unchanged at **92 688 / 98 304**, pool spare 8 384 B, LOWBUF 31 648/32 768. This is a code-size
measurement, not a speed claim: `tests/target_budget.py` and the render sweep are what say the audio paths and the
frames are where they were.

## 4. Tests run

- `tests/ui_render`: 196 screens × 22 palettes, **0 lint findings**, **0 alignment misses**, 0 inks on their own colour.
  The alignment hook covers the natural's name inside its key.
- `tests/scl_test`: keybed geometry (the bed the pages draw, from `scl_bed_rect()`), the ruler at `14..226 / ay 68 /
  13 px`, four taps on the drawn keys, the bass, the chord read back, a tap outside the panel.
- `tests/ui_pixels.py` regenerated: the frames that changed are the SCL pages, the SCL layer and the version string.
- Full suite: see `CHANGELOG.md` v2.0.1 for the run.
- Emulator: rebuilt for the release and compared bit for bit with the native build by `web/emu/emu_test.mjs`.

## 5. What is not verified

- **No FM-1**: the keybed was not touched, the panel was not looked at, and no MIDI controller was plugged in. The
  "live notes" and "hit" behaviour is the renderer reading `voice_t`, exercised by the host sweep, not by a finger.
- The hit animation is a 10-frame decay at the UI's own frame rate; its feel on the real panel is unknown.
