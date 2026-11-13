# QG UI and sound design refinement — HOME five screens

Status of the brief *"CORRECCIÓN DE BOOTSPLASH, HOME, SCL, EDIT, LFO Y CONSISTENCIA VISUAL GLOBAL"*.
This document states, area by area, what was investigated, what was changed, what was measured, and
what is **not** done. Nothing in it is claimed as verified beyond the check named next to it.

Classification used everywhere below:

| tag | meaning |
| --- | --- |
| **[HOST]** | built and run on the x86 host harness (`tests/ui_render.c`, `tests/ui_test.c`, `tests/regress.c`) |
| **[AC791N]** | compiled by the cross toolchain (`build.sh`), not installed on a device |
| **[EMU]** | run in the WASM emulator build |
| **[HW]** | needs the FM-1 itself: not available to this workspace |

---

## 1. Bootsplash

**Investigated.** The splash is painted by `draw_splash()` (`firmware/src/ui_draw.c`) and then advanced
by `splash_write(ms)`. Its clock is `fm1_micros()`, i.e. **TIMER4**, which is free-running from the 24 MHz
crystal from `fm1_time_init()` and does not depend on TIMER5 (`firmware/hal/fm1_time.h`). The earlier
"frozen splash" cause — a millisecond counter that only started half way through `fm1_main()` — is
already gone (the `(FASE 2/5)` note in `main.c`).

**Root cause of what remains.** The picture advances only when `splash_step()` is called, and it is called
at *init stage boundaries* (`panel_init`, `audio_init`, `usb_start`, `autosave_boot`, `reel_boot`). Between
two stages the panel holds one frame: `.bss`/state set-up, the audio engine's boot, the flash read of the
autosave and the TAKES reel. That is the still image the player sees before the animation "starts"; the
animation is not late, the repaints are sparse. `ui_render` measures the timeline as **1125 steps of 4 ms,
176 of which change the screen** — i.e. the art itself is progressive, not static.

**Changed.** Nothing in the boot sequence. Interleaving the init stages with the paint loop (a
`boot_stage` state machine driven from the splash's 4 ms loop) is the correct fix, but it rewrites the
power-on order of input, ADC, panel, engines, audio, USB, the LED sweep, the second core and the autosave,
and a mistake there is a device that does not boot. It has to be validated on an FM-1 **[HW]**, so it is
left as the next step with the root cause identified instead of being shipped blind.

**Not done:** the interleave, and the timing measurements (first frame, first animated frame, stage
durations, frames lost) — they need the device.

---

## 2. HOME — planets, star, Milky Way

**Audited.** `firmware/src/ui_being.c` already draws five planets (`NPL 5`) on their own ellipses with a
per-planet reaction type (`pl_react`: grow, shrink, orbital pulse, particles, pulse), a lit crescent and a
terminator turned toward the star, per-planet excitation from MIDI note-on (`gw_planet_excite`), and a
hit test (`gw_planet_at`) with a poke path (`gw_planet_poke`).

**Changed.** Nothing in this pass: the interactive layer is already per planet, and the remaining
"some planets do not answer" report could not be reproduced in the host harness. It needs the emulator or
the device with a finger on each world **[EMU]/[HW]**, and the star's corona, the galaxy band rework and
the extra nebulae are a separate visual pass.

**Not done:** star corona/flares, galaxy rework, per-planet hitbox verification.

---

## 3. SAVE-PRESETS — the redundant mini screen

**Audited.** `SAVE` enters the sound-save page directly (`ui.c`, `fam == FAM_SAVE`, `ui.page` set from the
family), and a preset change goes through `apply_preset_to()`. The mini list is drawn by the preset
picker layer, not by the save page.

**Changed.** Nothing. The trigger was not reproduced on the host harness, and guessing at a re-entrancy
guard without reproducing the path is exactly the kind of change that hides a bug instead of fixing it.

**Not done.** Needs a scripted run in the emulator **[EMU]**.

---

## 4. HOME reduced to five screens — **done [HOST]**

**Changed.**

* `firmware/src/ui.c` — `HV_COUNT` is **5**: `HV_HOME` (MAIN), `HV_VIZ` (VISUALIZER), `HV_OVERVIEW`,
  `HV_MIX`, `HV_PERF`. `HV_ENG`, `HV_MOD`, `HV_DYN`, `HV_SYSTEM` and `HV_ROUT` no longer exist, so no
  stale index can be walked, drawn or persisted.
* `firmware/src/ui_homeviews.c` — `graph_home_view()` draws only OVERVIEW, MIX and PERFORM; the strip in
  the title corner is **three cells** (the three pages that draw it) at x 190..227 instead of eight at
  124..227, so it cannot point past `HV_COUNT`.
* `firmware/src/ui_draw.c` — the footer prints `HOME n/HV_COUNT` from the enum, so it reads `/5` without a
  literal to forget.
* `tests/ui_test.c` — the walk is MAIN → VISUALIZER → OVERVIEW → MIX → PERFORM → MAIN, in that order, in
  both the "one screen per tap" check and the "read-only" check.
* `tests/ui_render.c`, `tools/generate_screenshots.py` — the five scenes of the retired pages are gone.

**Nothing was lost.** The engine page's eight parameters are EDIT's first page, the matrix is the MOD
section, the envelope is EDIT's ENV page, the routing is FX > BUSES and the system page is the SYSTEM
section. The home views' drawing code is still in `ui_homeviews.c` but unreachable; `--gc-sections` drops
it, so it costs no flash. Deleting it is the clean follow-up.

**Measured [HOST].**

* `ui_render`: **186 screens × 22 palettes** (191 − 5), **0 lint findings**, 3 152 530 alignment
  measurements, **0 off by 1 px**, 0 inks on their own colour, 0 non-neutral GREY/MONO pixels.
* `regress`: **285 golden renders, 0 changed**, 0 health failures, 0 CPU entries over budget.
* `ui_test`: **all passed**, including the HOME walk and the read-only check.

**Top bar (BPM, battery) after a full cycle.** The shared-state path that could corrupt it — a strip index
computed against the old count, and a rule drawn between cards that the overview/mix/perform pages do not
draw — is gone with the eight-cell strip (`on = home_view - HV_OVERVIEW` can no longer be negative or
past 2). It was not reproduced as a frame on the host, so it is fixed *by construction* here and still
needs a cycle on the device **[HW]**.

---

## 5. FX-BUS — Reverb

**Audited.** `firmware/src/ui_fxb.c` draws the six blocks from `fx_vu[]` peaks: the send above the block's
line, the return below it (`fxb_meters`), the tail from SIZE and the wet peak (`case FXB_REV`), and a
moving column of light (`fxb_sheen`) while the bus carries signal. All amplitudes pass `fxb_px` and are
clamped to the block's height.

**Changed.** Nothing. The reported "line that rises to the top of the screen while it is touched" was not
reproduced in the host sweep, and the block's own drawing does not use the touch position: the only
full-width marks on that page are the chain row and the block frames. Without a reproduction, changing the
clamp would be a guess.

**Not done.** The per-effect icons are already distinct (`FXB_BLOCKI`: delay, reverb, chorus, slicer,
punch, master) but are not enlarged in this pass.

---

## 6–9. SCL, EDIT, LFO, Solar Heat everywhere

**Not implemented in this pass.** Each of these is a design-and-DSP task on its own:

* SCL's three pages (scale degrees and out-of-scale notes, chord tones and inversions, a usable keyboard
  with octave labels and MIDI feedback).
* EDIT per engine: which of shape/morph, unison, sub, sync, FM ratio, feedback, drive, phase modulation,
  noise colour actually exists in each of the nine engines (`eng_*.c`), and a page per engine.
* LFO: waveforms, rate, tempo sync, divisions, phase, retrigger, fade, delay, slew, polarity, depth,
  destination, inversion, and its own screen per engine, wired to MOD.
* The Solar Heat pass on every non-HOME screen, with a particle budget.

They are deliberately not started rather than half-done: each one changes what the instrument sounds like
or how a page is read, and a partial version would be worse than the current one. The flash headroom is
**9 948 B** on the image, which is enough for the UI parts of SCL and the LFO page and **not** enough for
a new DSP parameter set on all nine engines without pruning elsewhere.

---

## Files touched in this pass

```
firmware/src/ui.c             HV_COUNT 5, comment
firmware/src/ui_homeviews.c   graph_home_view dispatch, three-cell strip
firmware/src/ui_draw.c        footer comment (the count is HV_COUNT)
tests/ui_test.c               the HOME walk and the read-only check
tests/ui_render.c             five scenes removed
tools/generate_screenshots.py three captions removed
```

## Pending, in the order they should be done

1. Boot interleave (section 1) — device-validated, then measure the five boot times.
2. SAVE-PRESETS reproduction in the emulator (section 3), FX-BUS reproduction (section 5).
3. SCL's three pages, then per-engine LFO, then per-engine EDIT (sections 6–8), each with its own
   `ui_render` scenes so the sweep covers them.
4. Solar Heat sweep over the remaining screens (section 9) at 1–2 particles per screen, measured against
   the 3 152 530-measurement alignment budget.
