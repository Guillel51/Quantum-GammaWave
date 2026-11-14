# FASE 5 — The preset bank: real inventory, one hole found, two sounds added

Date: 2026-11-14. Every number here comes from the tables in `firmware/src/eng_*.c` rendered
through the real engine, not from a document.

## 1. Inventory (counted, not quoted)

`engine_param_test --table` renders every factory preset once and prints its RMS and peak; the
category generator reads the same tables. The two disagreed, and that was the finding.

| Engine | In the firmware | In the browser | Mean RMS (own level) | Peak range |
| --- | ---: | ---: | ---: | --- |
| ANALOG | 68 | 68 | -17.0 dBFS | -30.0 .. -9.3 |
| FM6 | 37 | 37 | -18.7 | -28.8 .. -13.4 |
| PHASE | 32 | 32 | -17.5 | -26.3 .. -10.6 |
| LOFI | 29 | 29 | -16.8 | -22.7 .. -12.1 |
| VOICE (FORMANT) | 25 | 25 | -16.7 | -21.6 .. -10.1 |
| NOISE | 22 | 22 | -23.6 | -40.8 .. -17.4 |
| WHEEL | 22 | 22 | -16.1 | -22.3 .. -12.6 |
| WTABLE | **22** | **0 → 20 (+2)** | -15.5 before, -16.3 after | -19.8 .. -11.9 |
| DRUM | 18 | 16 | -15.2 | -21.9 .. -9.0 |
| **Total** | **275** | **251 → 271** | median -18.5 | -40.8 .. -9.0 |

Before this phase `tools/gen_preset_categories.py` did not list WTABLE at all: **20 factory
sounds shipped in the firmware and no browser page, filter or category ever showed them**, and
`tests/preset_categories_test.py` did not notice because it asserted a fixed 251. That is the
kind of thing an inventory found by counting catches and an inventory copied from a document does
not.

## 2. Remaster: measured, and what was left alone

- **0 presets clip** at their loaded level: no peak above -0.5 dBFS in the whole bank (275 of 275
  measured), and 0 presets silent, 0 out-of-range, 0 repeated names, 0 exact duplicates
  (`tests/preset_audit_test.c`).
- The level each sound loads with is generated from a measurement
  (`tools/gen_preset_levels.py` → `preset_level.c`): median -18.5 dBFS, 248 of 275 presets placed
  by the fader.
- **27 presets are still out of reach of the fader** — the generator lists them with the residual
  gap after its maximum: NOISE `STATIC HAZE` 10.5 dB, FM6 `AURORA FM` 10.7 dB, NOISE `GEIGER CNT`
  10.3 dB, and 24 more of 1.5–6.2 dB. Raising their `P_LEVEL` is not possible (127 already) and
  normalising them would erase what makes them what they are, so they are **reported, not
  flattened**. A remaster of those means changing the sound, not the number, and that is open.
- NOISE sits 7 dB under the bank on average. That is partly the nature of filtered noise (low RMS
  at high peak) and partly real: it is listed as open, not normalised away.

## 3. New sounds: two, and why only two

The audit shows the bank is not short of presets (275 for 22 visible families), so adding was
justified only where a family was thin. DRUM is the least represented (16 visible) but a new kit
is eight voices of parameters — out of proportion for this pass. WTABLE had 22 sounds and no
reachable page at all, so it got the two families the bank lacked by name:

| Preset | Family | Why | Measured |
| --- | --- | --- | --- |
| `ELECTRIC TINE` | KEYS / electric piano | the bank had no tine EP: `SINE KEY` is a sine, FM6's `TINE EP` is FM | -16.8 dBFS RMS, peak -11.1 |
| `GLASS CHIME` | BELLS / crystal | a chime with a long tail, distinct from `METAL BELL`'s short one | -16.3 dBFS RMS, peak -7.4 |

Both are built from what the engine already does — MORPH picks the frame, HARM the tilt, CUT/RES
the body, the filter envelope the attack — so neither adds DSP cost. Neither name duplicates an
existing one: the first drafts were `TINE EP` and `CRYSTAL BELL`, which FM6 already uses, and they
were renamed for that reason.

## 4. Counts, as asked

- **Kept:** 273 presets, none deleted (the brief allows up to 5 per engine; no preset was redundant
  enough to justify it — the audit reports 0 exact duplicates and 0 repeated names per engine).
- **Modified:** 0 presets' parameters. The generated level table was extended for the two new
  entries only (4 lines), not re-derived: a full regeneration moved 16 existing levels by ±1,
  which is measurement noise, not a remaster.
- **Added:** 2. **Newly reachable:** 20 (WTABLE).
- **Bank now:** 275 in the firmware, 271 listed in the browser (DRUM's 2 legacy kits at fixed ids
  10 and 11 are filtered on purpose, by id, in the generator).

## 5. Verification (host x86, this build)

- `tests/preset_audit_test.c`: 275 presets — 0 silent, 0 hot, 0 repeated names, 0 exact
  duplicates, 0 out of range, all envelope stages in range.
- `tests/preset_categories_test.py`: **273/273**, one primary category each (was 251/251 with 22
  sounds missing).
- `tests/regress.c` + `tests/golden.txt`: **293 golden renders, 0 changed, 0 gone**; the two new
  sounds are two added lines, reviewed in the diff. Re-run without update: clean.
- `build/ui_test`: 888 ok. `./build.sh`: ok (image +92 B, package 610 959 B).
  `tests/target_budget.py`: exit 0.

## 6. Not done

- **Nobody listened.** Every claim above is arithmetic on rendered samples; whether `GLASS CHIME`
  is beautiful is not in this document.
- The 27 out-of-reach presets and NOISE's 7 dB gap (§2).
- DRUM is still the thinnest engine (16 visible) and no kit was added.
- The Fase 4 backlog stands: musical ranges, MIXER's LARGE footer, SEQ parameter locks, and the
  systematic parameter-step pop instrument.
