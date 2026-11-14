# FASE 3 — Panel depth and the Solar Heat ground

Date: 2026-11-14. Scope: the shared drawing primitives, not the screens one by one.

## Why the interface read flatter than FX > BUSES

`cv_rrect` (`firmware/src/gfx.c`) already gave a big panel a frame, a lit top edge, a shaded
foot and the theme's corner mark — but only in the **LINE/chamfer style**
(`ux.chamfer && c == T_SURF && w >= 36 && h >= 36`). The rounded style, which is what almost
every screen uses, filled a 234 × 100 panel with one flat colour. `graph_heat`
(`firmware/src/ui_graph.c`) added a ground inside the graph canvas, but it was gated on
`ux.style` (LINE only) and dithered every pixel of the area.

So the depth FX > BUSES had was a property of one screen's code path, not of the interface.
Fixing it per screen would have produced eighteen slightly different answers; fixing it in
the primitives gives all 189 screens the same one.

## What was added

1. **`cv_plate_ground` in `gfx.c`** — every large surface plate (the same test the LINE style
   already used) now gets: a 1 px frame on all four sides, a 1 px lit edge under the top one,
   five rows of falloff on a card (nine on a panel), and the palette's accent as the short
   mark on the top border. Small plates — keycaps, chips, meter bars, 3 px gauges — are
   untouched.
2. **`graph_heat` rewritten, and no longer LINE-only** — three bands of shade under the bezel
   and six bands of glow along the foot, with the middle of the panel left exactly as the
   screen drew it. Skip in MONO (a black-and-white page has no heat and its contrast belongs
   to the data); on a light page (PAPER/SOLARIS/MINIMAL) the glow is halved and taken from
   the ink instead of the accent, because there an accent is darker than the paper.

Every colour is `ux_mix()` between two tokens of the palette in use, which saturates per
channel, so GREY stays a gray bevel and MONO stays neutral (verified below).

## What was tried and rejected (all measured, not guessed)

| Attempt | Result |
| --- | --- |
| Eight full-height gradient bands per panel | Host redraw of HOME went from ~155 µs to 748 µs — the panel's whole area refilled on every redraw. Rejected. |
| A tint over the whole graph area | Every list turned sepia; the dim rows (`OFF → OFF 0 %`, the preset names) lost their contrast against the data tone. Rejected. |
| `heat_col()` as the tint source | In the rounded styles `heat_col()` is not the thermal ramp at all, it is `T_ACCENT` — QUANTUM's accent is cyan, so the first build painted a cold teal floor that read as the old screen with a border. The ground is built from `T_BG` (violet shade) and `T_ACCENT` warmed toward `T_TEXT` (solar gold) instead. |

## Deliberate limits

- **Cards get frame + top fade only, no foot glow.** A card's value strip is repainted while a
  number rolls (`ui.roll`), and a ground under those rows flickered back on the next full
  redraw — the roll tests caught exactly that (a 2-row band at y 61–62). The foot glow
  belongs to panels tall enough that nothing repaints over it.
- **The bevel is not free of geometry.** The `#15 LARGE` assertion that the panel's first row
  inside its top edge is the flat surface is now `!= T_BG` (the panel still starts there; its
  first row is the lit bezel). One assertion changed; the rest of the suite is unchanged and
  green, including every "a forced redraw changes nothing" invariant.

## Evidence

Host, `cc -O1`, this VM, 2026-11-14:

- `build/ui_render`: 189 screens × 22 palettes + the page/value sweep — **0 lint findings,
  0 inks on their own colour, 0 GREY/MONO pixels off neutral, 3 390 046 alignment
  measurements with 0 off by 1 px**. Captures in `build/ui_new2/ppm/`.
- `build/ui_test`: **888 ok, all passed** (was 887 before; the LARGE assertion above).
- `./build.sh` with the JieLi/AC79 SDK: **ok**. Image 574 772 B (was 574 388 B, **+384 B**),
  RAM code 19 584 B of 24 576, package 610 959 B.
- `tests/target_budget.py`: **exit 0**, every target inside its budget (the audio targets are
  untouched by this phase: no DSP source changed).
- Draw cost: a pristine `HEAD` build and this one were interleaved in the same VM on the same
  test binary — `home` 671/681/677 µs pristine against 646/790/615 µs new. The difference is
  inside the run-to-run spread of this machine (the same binary varies by ±30 %), so **no
  claim is made about the time cost**; what is known exactly is the pixel cost: five or nine
  1 px rows per plate and nine band fills per graph panel.

## Not verified

- **No hardware.** Nothing here was run on the FM-1: no unit was available, so the picture on
  the panel and the frame time on the AC791N are unknown. Everything above is host-side.
- **The browser emulator was not rebuilt** (no `emcc` in this environment): it still shows the
  previous firmware, so a WASM screenshot would not prove anything about this change.
