# FM6: a released note has to give its voice back

**Class of every number here**: `[HOST]`, `tests/regress` and `tests/fm6_test` on this machine. No FM-1 was
involved.

## The defect, as the suite saw it

```
HEALTH FAIL preset/FM6/00_SILT_KEYS   voices not free 12 s after the note-offs; DC 19 left at the end; peak 582
HEALTH FAIL preset/FM6/31_FERROUS_PAD voices not free 12 s after the note-offs; DC -172 left at the end; peak 2288
```

and `tests/fm6_test` failed *"voices: all free some seconds after the release"*. The engine has six voices: two
presets that never come back are two voices the player does not have again for the rest of the song.

## Why it happened

Stage 3 of an operator envelope is the release: it falls from wherever the key left it to `L4` and then advances
to stage 4, which msfa means as "the envelope has nothing more to say". `fm6_env_get()` kept returning the level
at stage 4, so the operator's gain stayed at `L4`, and `fm6_note_done()` — which frees the voice when every
carrier has `gain_out < FM6_DONE` (8960) — could never be satisfied by a patch whose `L4` is audible. Read off the
carrier 12 s after the note-offs: five operators at `ix 4, gain 1069` and one at `ix 3, gain 32680904`.

The tail is also allowed to be very long: the released stage's rate is the patch's `R4` minus the panel's
`RELEASE` knob, floored at 1 (`eng_fm6.c:349`), and rate 1 is a fall of ~284 M level units at ~16 per sample —
around 100 s. While a stage's level does not move, the static table holds it for up to `FM6_STATICS[0]` =
1 764 000 samples (40 s). SILT KEYS' `P_REL` 58 on an operator whose `R4` is below 41 lands right there.

## What was tried and rejected

- **Making stage 4 silent** (`fm6_env_get()` returns 0 once `ix >= 4`) fixed both health failures *and* left
  `tests/fm4_test` red: the FM4 → FM6 conversions are measured against the retired four-operator engine at
  0.05, 0.3 and 1.0 s, their tails live on `L4`, and cutting them turned *centroid 18/18 ok* into
  *7/18 (worst 3.78 octaves)* and the level check into *16/18*. A fix that breaks a conversion's fidelity is not
  a fix.
- **A floor on the release rate** (`FM6_REL_MIN`, ~4 s) made every release faster than its patch asked for and
  changed 28 golden renders. It also was not needed once the voice has a time budget (below), so it is not in
  the code.

## The fix that stayed: the tail is the patch's, the voice has a budget

- The envelope is untouched: `L4` still holds, the release still reaches it at its own rate, and the FM4
  conversions measure the same tail they did before (`18/18`, `18/18`).
- `fm6_note_t` gained `rel_t`, samples since the key went up (`fm6_core.c`). `fm6_render()` counts it and, after
  `FM6_REL_MAX` = 5 s, fades the voice out over `FM6_REL_FADE` = 60 ms and hands it back (`n->live = 0`). The
  fade is applied to the block's output in the same place the amplitude ramp is, so a cut can never click. A held
  note never counts (`rel_t` only runs while the key is up).
- 5 s is longer than anything `tests/fm4_test` measures (1 s), longer than any tail a player hears as part of the
  note, and shorter than the 12 s the health checks allow.

## Result `[HOST]`

| | before | after |
|---|---|---|
| `tests/regress` health failures | 2 | **0** |
| `tests/regress` voices free, SILT KEYS | never (DC 19, peak 582 at 12 s) | 5.06 s, DC 0, tail 0 |
| voices free, FERROUS PAD | never (DC −172, peak 2288) | 5.06 s, DC 0, tail 0 |
| `tests/fm4_test` centroid / level | 18/18, 18/18 | 18/18, 18/18 |
| `tests/fm6_test` | 5 failures | **0** (34 checks) |
| FM6 golden renders changed | — | 29, re-baselined (their tail now ends) |

## The four fm6_test failures that were the test's, not the engine's

*"attack -> L1, decay -> L2 -> L3, held, then the release"*, *"the voice ends after the release"*, *"a decayed
note ends while its key is held (L3 0)"* and *"the 8 factory patches never trimmed"* all came from the fixture:
`setup()` left the track's envelope knobs at the values `host_preset` gives (SILT KEYS'), and `eng_fm6.c`
applies those knobs to the patch — `ATK`/`DEC`/`REL` are subtracted from the operators' rates (`:343-349`) and
`SUSTAIN` **rewrites** `L3` of every operator (`:347`, `50 + SUS*49/126`). With `DEC` 74 the test patch's `R2` 50
became 1 and its decay took ~40 s; with `SUS` 116 its `L3` 0 became 95, so a percussive note never decayed.

The fixture now neutralises the three rate knobs, and the sustain knob in the percussive case. No assertion was
changed: they now measure the patch, which is what they say they measure.
