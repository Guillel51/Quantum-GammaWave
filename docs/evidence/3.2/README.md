# Evidence — pass 3.2 (2026-11-09)

Two classes of image live here and they must not be read as each other.

**Host x86 renders** (`host_*.png`): `tests/ui_render.c` drawing each screen into a 240×240 framebuffer and writing a
PPM, scaled 2× (nearest neighbour) for reading. This is the same drawing code the firmware links, measured by the
sweep's lint and alignment pass — not a device and not the emulator.

| file | screen |
|---|---|
| `host_splash.png` | the boot splash, final frame (BOOT 100 %, name, ringed planet, 16-block rail) |
| `host_home.png` | HOME: four planets in the solar system bay, the graph, the FX slots |
| `host_scl.png` | SCL: white keys with the note name only (no degree digit) |
| `host_fx.png` | FX: the four sends and their meters at 100 % / 12 % without touching the card edge |
| `host_dly.png` | DELAY: the seven bars, always visible at their minimum height |
| `host_reverb_reso.png` | REVERB, type RESO: the resolver — four combs, energy dots running along each string |
| `host_reverb_cloud.png` | REVERB, type CLOUD: the swarm at three depths (a new sweep scene, so both are rendered) |
| `host_layer_scl_active.png` | the SCL layer card held over HOME |

**Emulator** (`emu_*.png`): the real WebAssembly build (`build/emu/felucca.wasm`, 1 086 293 B, sha256 `36e1dce6…`) in
the Cloud browser, the canvas read back at 240×240.

| file | what it shows |
|---|---|
| `emu_splash_strip.png` | nine frames sampled from the emulator's own canvas across one boot: 17 % → 36 % → 64 % → 92 % → 100 % → hold → hand-over |
| `emu_splash_early.png` | the first second of the same animation (the name being written, 13 % / 20 %) |
| `emu_boot1.png` | the emulator after the splash: HOME, with the preset it loaded |

No hardware was involved in any of these.
