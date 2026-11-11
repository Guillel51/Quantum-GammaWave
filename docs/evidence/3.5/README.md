# Evidence — pass 3.5 (2026-11-11): HOME's worlds, their answers, the visualizer's depth

Three classes of picture live here and they must not be read as each other.

**Host x86 renders** (`before/`, `after/`): `tests/ui_render.c` drawing the screen into a 240×240
framebuffer and writing a PPM, scaled 2× (nearest neighbour) for reading. This is the same drawing
code the firmware links (`ui_being.c`, `ui_graph.c`) — **not** an FM-1 and **not** the emulator.

**Reaction strips** (`after/poke0..4.png`): the HOME panel cut out of five frames of
`tests/capture_home_pokes.c` (240×66 → 480×132), one per world, captured six frames after that
world was tapped. Same class as above: host x86, deterministic build (`-DUI_NO_DRIFT=0` so the
reactions run, a fixed clock for everything else).

**Never taken:** emulator frames and device frames. See *What was not run* at the end.

## The images

| file | what it shows |
|---|---|
| `before/home.png` | HOME as it was: five bodies drawn as flat discs with one tone each, one ring, five pinpoints |
| `after/home.png` | HOME now: the ringed giant carries bands, a bright limb and the ring's shadow on the globe; the ocean world two atmospheres and a mare; the storm world three belts and its eye; the rocky world a lit horizon and a crescent; the frozen world caps and a crevasse. A bounce glint sits on each lit edge |
| `before/viz.png` | the visualizer at its old background level, no distant field |
| `after/viz.png` | the visualizer darkened (`bg_darken`), with the deep field: a giant limb upper-left, a ringed moon in the gap under the scope, a small world right, an arc along the bottom. The trace, the bars and the header keep their exact greys |
| `after/poke0.png` | **SPIN** — the ringed giant drags a comet tail and its orbit jumps |
| `after/poke1.png` | **SWELL** — the ocean world swells and settles |
| `after/poke2.png` | **RIPPLE** — a wave front leaves the storm world, with its dotted echo |
| `after/poke3.png` | **SLING** — the rocky world is pushed out and its ring plane opens |
| `after/poke4.png` | **FROST** — the frozen world gains a frost ring and a fan of particles |

I looked at `poke0`, `poke2` and `poke4` by eye; `poke1` and `poke3` are here as the harness wrote
them and were judged by its measurement (113 px and 88 px of change), not visually.

## The reactions, as measured

`tests/capture_home_pokes.c` (host x86, `-DUI_NO_DRIFT=0`) taps each world and compares the frame
with a control frame at the same age, then taps again mid-reaction and checks it restarts.

```
poke: body 0 at 41,158: 77 px answer
poke: body 1 at 34,157: 113 px answer
poke: body 2 at 29,152: 10 px answer
poke: body 3 at 29,147: 88 px answer
poke: body 4 at 35,148: 28 px answer
poke: ok: five distinct, restartable answers (77 113 10 88 28 px)
```

The tap points are **screen** rows: the panel is drawn on its own strip and blitted at `graph_y()`,
and `gw_planet_at()` now converts a tap's row by that offset (it used to compare canvas rows against
screen rows, which would have missed every world in a real front end). Before that fix the harness
was agreeing with itself in canvas space; after it, the harness finds the worlds through the same
path a pointer uses.

The reactions are a pure function of `ui.frame - pl_poke_t[i]` — no accumulators — so a second tap
re-bases the clock and a reaction can always be interrupted by another. `PREF_ANIM_OFF` and the
deterministic builds (`UI_NO_DRIFT`) freeze them.

## Cost — host x86, `cc -O1`, `clock()` around the draw

Three runs of `tests/ui_render.c` **before** (a `git archive HEAD` tree, same machine, same session)
and three **after**, same command. Medians, with the spread shown because the spread matters here:

| measurement | before (3 runs) | after (3 runs) | verdict |
|---|---|---|---|
| HOME, full frame | 159.5 µs (171.6 / 159.5 / 152.0) | 154.3 µs (154.3 / 155.8 / 154.2) | −3 %, inside the spread |
| HOME, frame without force | 48.1 µs (48.4 / 48.1 / 45.5) | 46.4 µs (46.5 / 46.3 / 89.2) | −3 %, one run noise |
| HOME's visualizer, HIGH | 81.4 µs (82.5 / 78.3 / 81.4) | 79.7 µs (94.3 / 79.7 / 79.6) | −2 %, inside the spread |
| HOME's visualizer, LOW | 81.8 µs (81.8 / 81.3 / 117.2) | 74.9–82.4 µs (82.4 / 115.0 / 74.9) | no change distinguishable |
| a second screen (7 638 columns) | 119–123 µs | 120–123 µs | unchanged |

**What this does and does not say.** This VM is shared and single screens vary by 10–20 % between
consecutive runs of the same binary (the 94.3 µs and 115.0 µs rows above are that, not the code).
The medians differ by less than that spread, in both directions across sessions, so the honest claim
is **no measurable regression and no provable speedup** — not a speedup. What is bounded by
construction: the ember field has a hard cap (`GW_EMBER_MAX 32`, three depths, no allocation), the
deep field is `O(r)` with `r ≤ 30` and is off below the HIGH tier, planets are capped at `r = 9`, and
nothing in the new code allocates, calls `sinf`/`cosf` per pixel, or touches the audio path.

## The sweep

`tests/ui_render.c` (exit 1, expected):

```
68 lint findings, 0 inks on their own colour, 396 ellipsised free texts, 0 GREY / MONO pixels off
gray / neutral; 159806 column values swept; 208 FM6 charts linted; 856 cards linted with MOTION's icon
```

68 findings is the baseline of this tree before the pass; the alignment pass reports 0. `ui_test`
prints `ui: all passed` (exit 0). `tests/particle_bounds_test.py` prints
`particles: 0 failed checks`.

## What was not run

- **The emulator was not rebuilt and was not exercised.** `emcc` is not installed here and
  `web/emu/build.sh` pins Emscripten 6.0.11, so `build/emu/felucca.wasm` is the old binary. The tap
  hook added for it (`web_tap` in `felucca_web.c`, `m.type === "tap"` in `worklet.js`, a
  `pointerdown` listener in `index.html`) is therefore **compiled nowhere and untested**; it is
  written to no-op when `ex.web_tap` is missing.
- **No FM-1 was touched.** No device frame, no device FPS, no physical validation. The FM-1 has no
  touchscreen: `ui_tap(x, y)` exists for front ends that can point (the emulator, the host harness),
  and HOME's own buttons/knobs are unchanged.
- Device frame rate is **not** measured here. The figures above are host draw times, not FPS on the
  AC791N.
