# Hardware simulation, emulation and measurement — what is what

QG-1 has three different things that are easy to confuse. This page keeps them apart.

## 1. Emulation (same C, WebAssembly)

`web/emu/felucca_web.c` compiles the firmware's own translation unit against a shim of the FM-1's
peripherals, and `web/emu/worklet.js` runs it in an AudioWorklet. The screen is the firmware's real
240x240 RGB565 framebuffer, the buttons, encoders, keys, MIDI and USB paths are the firmware's, and the
NOR image lives in wasm memory (`web_nor`). `web/emu/emu_test.mjs` compares its audio with the native build
sample for sample.

- Models: CPU-independent firmware logic, audio DSP, screen, LEDs, keypad/encoders, MIDI in, USB-MIDI
  packets, NOR sectors and persistence across instances.
- Does not model: AC791N instruction timing, cache, DMA bandwidth, interrupt latency, the second core's
  contention, flash erase/program time, or analog behaviour.

## 2. Host simulation (tests/)

`tests/hostsim.c` gives the firmware's peripheral interfaces deterministic stubs and a simulated NOR with
power-cut injection; `tests/regress.c` renders every preset and mix; `tests/project_test.c`,
`tests/backup_test.c`, `tests/fuzz_proj.c` and the sanitizer runs exercise storage and protocol paths.
CPU counters are host counters (`proc_pid_rusage` where available), and the ns figures printed by regress
are host wall-clock, useful for relative comparisons only.

- Models: control flow, data formats, error paths, memory safety, deterministic audio, interleavings that
  the tests inject (power cuts, short writes, corrupt records).
- Does not model: the target's 160–240 MHz core budget, its 96 KB SRAM layout, its 2 MB SPI flash timing,
  or real USB/DMA concurrency.

## 3. Physical measurement (not available here)

An FM-1, the JieLi pi32v2 toolchain and a cycle counter are required for instructions/sample, SRAM use,
flash size, underruns and thermal/battery margin. None of that was available, so:

| Claim | Status |
|---|---|
| Static RAM per voice on this ABI | measured (host); target is smaller with 32-bit pointers |
| 8/10/12 voices health on host | measured |
| 12 voices on AC791N | **NO VERIFICADO** — do not enable by default |
| Flash write, power-cut recovery | simulated only |
| Audio latency/underruns on hardware | **NO VERIFICADO** |

## Validation ladder

1. `python3 tools/budget_report.py` — static sizes and library composition (host).
2. `python3 tests/preset_library_test.py` — 200 sounds, legacy indices.
3. `tests/regress.c` — golden audio, health, voices, routing (host).
4. `tests/ui_render.c` — every screen in every palette (host).
5. `sh web/emu/build.sh` + `node web/emu/emu_test.mjs` — wasm parity (emulation).
6. JieLi build + board measurements — the only source of target CPU/flash/latency numbers.

No result from steps 1–5 is presented as an AC791N measurement.
