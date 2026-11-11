# 3.1 — the owner's visual pass: host captures

Class of evidence: **host render** (`cc -O1`, `tests/ui_render.c`, the same `draw()` the panel gets,
`QG_DUMP=<screen>,<palette>`). Nothing here was captured on the FM-1 or the AC791N: no toolchain and no
unit were available in this environment.

- `before/` — `build/base`, a clean copy of the commit this work started from (`git` HEAD at the time).
- `after/` — this tree, after the changes.
- Palettes: 13 QUANTUM, 15 CYBERPUNK, 16 NORDIC, 8 GAMMA. Screens: 0 HOME, 36 FX, 41 SCL, 97 FX held
  (the punch-in map), 104 REVERB, 156 REVERB 2, 158 MASTER, 182 HOME visualizer.

What the pairs show: HOME's solar system no longer leaves its bay; the visualizer's spectrum tops stay in
their box; SCL is a grey keyboard with the scale's notes marked, the same in every theme; the punch-in map
says M1–M6 and its footer has the `OCT+ LATCH` switch; MASTER has no `UN / CPU / PK` band and its VU does
not touch the captions.

Not shown by a still: the reverb rings follow the bus's return (`fx_vu[5]`) and decay to rest; that is
`tests/ui_test.c` ("REVERB graph: the rings follow the bus's return and fall with the tail").
