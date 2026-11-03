# v1.7 feature audit — what exists, what is deferred, and why

The project format (`FUN1`–`FUNC`) and the parameter count (`P_COUNT`) are frozen. A feature that needs a
new stored parameter would break old projects and SysEx backups, so those are deferred rather than shipped
half-safe. "Present" means the capability already exists in 1.6 and was verified by the host suites.

| Requested feature | Status | Where / why |
|---|---|---|
| Smart Randomizer | **Present** | `SEQ > MUTATE`, `SHIFT > CHANCE`, the DICE functions (`song_dice.c`), `ui_input.c` preset dice; deterministic per project |
| Macro Controls | **Present** | M1–M4 (`macro_v`) and the latched MACRO layer; matrix sources M1..M4 |
| Preset Morphing | **Present** | `P_MORPH` on ANALOG / PHASE / LOFI (wave morph), matrix and MOTION can drive it |
| Motion Recorder | **Partial** | Automation/MOTION locks and the modulation sequencer (`MS_STEP`) are present; a real-time knob-recorder timeline is deferred (needs UI + storage, not just a parameter) |
| LFO retrigger | **Present** | LFO `PHASE` restarts on a fresh phrase; LOFI has `RTRG`. A global per-note retrigger switch would need a new parameter — deferred |
| Arpeggiator improvements | **Present (8 controls)** | `ARP` / `ARP 2`: mode, rate, octave, gate, swing, probability, hold, order. Further modes deferred until a musical gap is demonstrated |
| Chord Memory | **Present** | `SCL > CHORD`: fixed chord types, voicing, strum |
| Preset Compare | **Deferred** | Needs a held "compare" state and a second sound buffer; no free persistent surface without changing the UI/format. Alternative: save the sound to a user slot and use the browser A/B by ear |
| MIDI Learn | **Deferred** | Would need a mapping table persisted with the settings; the current CC map is fixed and safe. Alternative: a session-only learn (lost on power-off) is the next low-risk step |

## Why not force them in

- Every new persistent parameter changes `P_COUNT`, which is part of the project record
  (`project.c`'s frozen sizes) and of user presets (`upreset.c` maps by count). The audit of 1.6 showed how
  a seemingly harmless table change can move a stored sound; the same rule applies to parameters.
- UI additions must pass the 171-screen lint/alignment sweep in 17 palettes; a half-designed page would fail
  it or, worse, ship cut text.
- MIDI Learn and Preset Compare both need state that survives a power cycle to be useful; storing it
  safely means a versioned settings/format change with a migration, which is a project of its own.

## Next low-risk candidates

1. Session-only MIDI Learn (RAM table, no flash), with a visible "LEARN" state and a reset.
2. A compare mode that works on the *current* sound and its last loaded preset, using the parameters
   already in RAM (no new storage), behind a held button.
3. Motion recorder onto the existing automation lanes (no new parameter): record knob moves as automation
   events, which the project format already stores.
