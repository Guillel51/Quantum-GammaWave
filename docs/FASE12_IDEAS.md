# Phase 12: three ideas, what each costs, and what was actually done

Numbers are `[HOST]` (`tools/budget_report.py`, `tests/regress`, `tests/fm4_test`) unless stated. Nothing here
was measured on an FM-1.

## 1. A released FM6 voice gets its tail and then a budget — **done, v2.3.14**

The engine's six voices are the scarcest resource in a song, and a patch whose `L4` is audible held one for ever.
`rel_t` (samples since the key went up) plus a 5 s budget and a 60 ms fade give the voice back without touching
the tail the patch asked for (`docs/FM6_VOICE_RELEASE.md`).

- **Use**: two voices recovered on the two presets that leaked them; the health checks that had been red since
  the leak was found are green.
- **CPU**: `tests/regress` reports 0 CPU budget failures and 0 notes over +40 % against the same baseline; the
  added per-block work is one compare and, on the last 60 ms, one 64-bit multiply per sample.
- **RAM**: 4 bytes per FM6 note (`rel_t`) — 24 bytes for the engine's six.
- **UX**: a pad's tail is unchanged for the 5 s a player hears it; the voice comes back instead of being gone.

## 2. `SUSTAIN` is a rewrite of `L3`, not a modifier — **evaluated, not done**

`eng_fm6.c:347` sets `op[FP_L1 + 2] = 50 + SUS*49/126` for every operator, i.e. the panel's SUSTAIN knob
**replaces** each patch's stage-3 level. Consequences today: a patch's own `L3` can never be heard while the knob
is above 0; `SUS` 0 forces `L3` to silence; and this is why the FM6 test fixture was measuring the panel instead
of the patch (v2.3.14's fix). It is also half the reason a released note could drone: `L3` 95 keeps the envelope
loud right up to the release.

- **Use**: the knob and the patch would coexist; patches keep their designed sustain.
- **Feasibility**: small in code (one line, a scale or an offset instead of a rewrite).
- **Cost**: a sound change across all 37 FM6 presets, their `FX()` defaults and 37 golden renders, plus a
  re-tune of every preset that relies on the present behaviour. That is a sound-design pass with the instrument
  in hand, not a blind edit in a release phase. Left documented, with the line number, so it is one decision away.

## 3. Skip the envelope call for a finished operator — **evaluated, not done**

Once an operator's envelope is past stage 4 its level is constant, so `fm6_env_get()` is called for nothing —
six calls per sample per voice that the renderer already skips.

- **Use**: a few instructions per finished operator per sample.
- **Feasibility**: trivial (an `ix >= 4` early-out in the `k` loop of `fm6_note_compute`).
- **Cost**: none measurable with the tools here — `tests/fm6_test`'s instruction counter is macOS-only and
  `tests/regress` measures ns per preset, where this would hide in the noise. AGENTS.md §8 says never optimise
  the DSP on a hunch: without a measurement that can show the saving, the code change would be a claim, not an
  optimisation. Not done.

## What the phase did do, for the record

- The voice leak (idea 1), with the two rejected attempts documented and their failures quoted.
- `tools/budget_report.py` compiles again (its probe now stubs `pc_arm`), so the RAM and cost tables above can be
  produced at all.
- `tests/fm6_test` from 5 failures to 0 without touching an assertion: the four that were left were the fixture
  measuring the panel's knobs instead of the patch.
