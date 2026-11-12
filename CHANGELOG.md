## 2026-11-12 — FASE 1/5: optimización total (medida)

- **Mixer**: a part with no sends (CHOR / DLY / REV at 0) no longer clamps its signal for the buses nor divides for the send ceiling — the clamp and the division only happen when there is something to send. Bit for bit the same sound; 0.4 % cheaper per audio block on a dry six-part scene in the host bench, and neutral with every bus fed.
- **FX buses**: the chorus line's index is masked once per sample instead of twice (the write and the phaser tap).
- **Instrument**: `tests/fx_bench` now prints process CPU time next to the wall clock and measures two scenes (every bus fed / dry parts). That is what made a 1 % difference readable on a shared VM — and what got a candidate rejected instead of merged: hoisting the master's per-block settings cost 1.4 % more CPU and was reverted (measured, not assumed).
- **TIMER5 tick**: a key column that reads exactly as it did last frame and has no debounce in flight no longer walks its four `fm1__key` calls (each only stored a 0 over a 0): 35–36 → 27–28 ns of CPU per tick idle (−22 %) and −15 % with a column held, on a new host bench (`tests/isr_bench`, built by the suite). `main.c`'s tick also computes its `sub % 5u` phase once instead of twice. The 16-bit 595 shift is untouched: 48 of the tick's 60 GPIO register accesses are that shift, whose pace the board needs, so the hardware-bound part of the tick is unchanged. Cost: +24 B of `.bss` (`sizeof(fm1_in)` 280 → 304 B, measured).
- **Record**: `docs/QG_PHASE_1_OPTIMIZATION.md` has the recovered state (rebased onto v2.8.4), the host profile, the measurements, what was refused and why, the blocked target/WASM/hardware checks, and the extension points for phases 2–5. `run_tests.log` is a real run again: the tracked one was a stale, failing log from a tree that no longer exists.
- **Tests**: `ALL HOST TESTS PASSED` (119 sections), 220 golden renders unchanged, 0 lint findings over 191 screens × 17 palettes, ASan/UBSan and both fuzzers clean. No target build, no WASM, no unit: nothing here is a hardware measurement.

## 2026-11-12 — v2.8.4: the HOME screens breathe

- **Atmosphere**: the four new HOME screens share a background layer drawn before their text — sixteen embers that drift upward a pixel or three a frame (hashed positions, so they never clump) and a floor of six bars, each the level its track is making now. All of it is 1 px rects in the panel's own steps (`T_RAISE`/`T_LINE`), so no palette can make it unreadable and it costs no allocation and no per-pixel path.
- **Hot meters**: MIXER's level cells turn to the accent above 84 %, and MODULATION's depth bars above 96 %, so a track that is pushing reads at a glance.
- **Knob labels** on PERFORMANCE no longer carry a numeric suffix.

## 2026-11-12 — v2.8.3: HOME is a full set of screens

- **MIXER, redrawn**: each track now reads as the instrument draws everything else — its number and the engine it is running, a nine-cell level meter with the peak held beside it, the pan as a two-pixel tick on a ruled centre line (it used to hide under the ruler's own line), a MUTE pill, and its fader position in cells. Below, the mix's own L and R peaks with a scale, plus the master level. Still nothing stored: `trk[].vu` and `trk[].peak` are what `fx.c` already writes.
- **PERFORMANCE**: octave, transpose, root, scale, tempo, swing, and the selected track's arpeggiator (mode, rate, octave, gate, swing) with the four knobs HOME turns and their values. Every row is a live parameter the DSP reads.
- **SOUND ENGINE**: the engine's own eight parameters with the labels and enum names from its descriptors, the preset it is playing and its number, voices in use against the cap, morph, cutoff and resonance.
- **MODULATION**: the four matrix slots (source → destination, depth as a bar, off rows drawn off) and the LFO's four destinations (`P_LD_*`) with their depths.
- **Tests**: 191 screens (from 188) in 17 palettes, 0 lint findings, 0 of 2 779 972 alignment measurements off; the two overlaps and one out-of-cell spill the sweep found in the first draft of these screens were fixed, and the HOME cycle test now walks all eight screens and asserts each of the four new ones leaves the page, the track and the sound untouched.

## 2026-11-12 — v2.8.2: HOME gets a desk

- **HOME > MIXER**: the fourth HOME screen (after VISUALIZER, OVERVIEW and SYSTEM), reached by the same tap that walks the list. Six rows, one per track: the level the block actually reached (the peaked envelope `fx.c` writes, not a sample), the pan as a tick on a centred ruler (`P_PAN` is −64..63, so 0 lands in the middle), MUTE or the voices the track holds now, and the mix's own two peaks at the bottom. Everything on it is a number the firmware already keeps; nothing new is stored, so a project saved before this screen loads and sounds exactly as it did.
- **Tests**: the render sweep goes from 187 to 188 screens — `home_mix` runs in all 17 palettes with 0 lint findings and 0 of 2 778 340 alignment measurements off. `docs/QG_V2.8_FINAL_AUDIT.md` moves HOME's views from PARCIAL: PERFORMANCE, SOUND ENGINE and MODULATION still do not exist, and this release does not pretend otherwise.

## 2026-11-12 — v2.8.1: the boot is 0.8 s shorter

- **Boot**: the splash's hold drops from 3.6 s to 2.8 s (total 5.3 s instead of 6.1 s). The 2.5 s signature write is untouched, so only the wait after it shrinks — the animation is the same, the instrument answers sooner. `MENU > STARTUP SPLASH` SHORT/OFF work as before.
- **Tests**: `ui_render` splash timeline re-measured; host suite unchanged.

## 2026-11-12 — v2.7.6: the EXTRA tab answers, and the delay shows its train

- **MENU > EXTRA**: under its rows it now says which cores are running (`CORES 1` / `CORES 2`, from fx.c `mc_status()`, the same line ABOUT shows), the audio ISR's own load (`CPU n%`) and, in the corner, when CPU MODE applies (`NEXT POWER-ON`). It did work — main.c reads it before core 1 starts — but a setting that changes nothing until the next power-on reads as a dead row, which is exactly what it looked like.
- **FX > DLY**: the panel above the six repeat slots was empty; it now draws the train's envelope from the peaks the effect actually returns (`fx_dly_rep`), joined and filled to the axis, with a dot on each repeat. Same data the bars had, in the shape of a decay.
- **Tests**: `ui_render` reports 187 screens x 17 palettes, 0 lint findings and 0 of 2 777 796 alignment measurements off by a pixel; 220 golden renders unchanged; `ALL HOST TESTS PASSED`.
- **Tooling**: `QG_DUMP_NAME=<screen>` renders one screen by name (`tests/ui_render.c`), so a visual review names the screen instead of counting an enum.

## 2026-11-12 — v2.7.5: SCL, HOME views, M-WIDE and the security pass

- **SCL**: `chord_build` handles 5, 6 and 8-note scales (ported from JIANT 0.8.3) and six SCL screens show them; `tests/chord_scale_test.c` checks 8 448/8 448 invariants and 360 chords identical to the old traversal.
- **HOME**: a second screen suite (HOME -> VISUALIZER -> OVERVIEW -> SYSTEM) with the engine/time/system summary; `test_home_views` and the 187-screen render sweep stay green.
- **FX**: M-WIDE compensates its own power (a +3.8 dB jump at 127 is now -0.0 dB, correlation still -0.594, mono still mono at -3.9 dB); `tests/fx_probe.c` measures phaser, M-WIDE, LOWCUT and EQ levels.
- **Presets**: `tests/preset_audit_test.c` audits all 202 factory presets (0 silent, 0 out of range, 0 repeated names, 0 duplicates).
- **Audio path**: `fx_buses` walks the chorus, delay and reverb buses once instead of twice (the peaks also answer "anything sent?"), measured with the new `tests/fx_bench`: median 127.2 -> 126.0 ns per sample on host x86, 220 golden renders bit-identical.
- **Security**: `release/SHA256SUMS` had committed merge markers and `coherence_check.py` crashed on them instead of reporting; both fixed, the manifest regenerated from the bytes on disk. `release.yml` pins the third-party build workflow to a commit instead of a mutable tag.
- **Release**: first v2.7.5 package built from this tree with the JieLi toolchain and the AC79 SDK (610 959 B, identity FM-1_965) and an emulator rebuilt with emcc 6.0.11; `coherence_check.py` reports ok.
- **Tests**: `ALL HOST TESTS PASSED`, ASan/UBSan and both fuzzers included; `tests/target_budget.py` reports 39 cost entries, all within budget.

## 2026-11-12 — v2.7.4.8: Reverb visuals and sync pass

- **FX / Reverb**: ROOM, SPRING, SHIMMER and RESO now have distinct geometries and livelier animation driven by the reverb bus return; ROOM adds ghost teeth and an air ring, SPRING a wobbling coil and a second chirp, SHIMMER denser animated sparks, RESO a second travelling pulse and a resonance flash.
- **Tests**: `ui_render` and host tests pass after the change.

## 2026-11-12 — v2.7.4.7: PHASE screen header and sync pass

- **PHASE**: the dedicated `graph_phase` header now reports DCW alongside WAVE, W2, MIX/RING and DT, so the bent-phase screen exposes the parameter that moves its knee.
- **Tests**: `ui_render` (0 lint findings, 0 alignment errors) and `ui_test` pass after the change.

## 2026-11-12 — v2.7.4.6: Reverb model-specific animations and sync

- **Reverb models**: ROOM now shows breathing comb teeth, SPRING a travelling chirp marker, SHIMMER keeps its octave sparks, RESO/CLOUD keep their distinct animations.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.6 firmware + SHA256.

## 2026-11-12 — v2.7.4.5: FX VU meter routing fix and sync

- **FX meters**: the Reverb meter now reflects only the reverb bus's own return; Chorus or Delay activity no longer moves it.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.5 firmware + SHA256.

## 2026-11-12 — v2.7.4.4: Reverb RESO animation fix and release sync

- **Reverb RESO**: removed the out-of-panel glitching orbit ring and added a travelling pulse along each resonant string, with frame-rate scaling tied to the reverb return.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.4 firmware + SHA256.

## 2026-11-11 — v2.7.4.3: FX VU meters rebuilt on HOME's plasma meter

- **FX meters**: replaced the old FX-section VU meters with HOME's segmented plasma meter component, reusing its design, animation and decay ballistics.
- **DSP**: verified send / wet-return / tail paths remain independent and settle to zero when effects are off.
- **Build**: firmware and WASM emulator rebuilt for AC791N, manifests and hashes synchronized.
- **Release**: v2.7.4.3 firmware + SHA256.

## 2026-11-11 — v2.7.4.2: FX visual polish, WASM rebuild and release sync

- **FX**: rounded panels and improved visual consistency.
- **Build**: firmware and WASM emulator rebuilt and manifests synchronized.
- **Release**: v2.7.4.2 firmware + SHA256.

## 2026-11-11 — v2.7.4.1: UI redesign pass, planets, visualizer, FX and splash

- **HOME / solar system**: deeper star field, more dust, nebula and reactive planets (spin, swell, ripple, sling, frost), all five answer taps.
- **Visualizer**: denser reactive particles and orbital elements, improved depth and transitions.
- **FX**: meters track real per-bus peaks; DIST meter idles when drive is off; splash timing extended ~1 s.
- **Splash**: longer writing animation (SPL_WRITE 2500 ms) with hand-over preserved.
- **Release**: firmware v2.7.4.1, package, manifests and hashes.

## 2026-11-11 — v2.7.4: FX meter fix and release

- **FX meter (DIST)**: the DIST bar in the FX screen now stays dark when drive is off (`P_DIST == 0`), instead of following the dry track signal. The other three buses already read `fx.c`'s real measured peaks.
- **Release**: firmware v2.7.4, package `release/felucca-v2.7.4.fwsc`, emulator (WASM) and manifests, same commit.

## 2026-11-11 — créditos con nombre y apellido

- **Créditos del linaje corregidos.** `README.md` y `MENU > ABOUT > CREDITS` atribuían Pocket GammaWave (GW-1) a
  Hügelton Instruments; la línea GW-1 es de **Frederic Valente (@fvalente)**, como ya decían `LICENSING.md` y este
  CHANGELOG. JIANT FM y Felucca quedan a nombre de **Leo Kuroshita (@kurogedelic) / Hügelton Instruments**.
- **Se suman los aportes de este fork**: **Guillermo Lauria** (instalador publicado, CI y registro de build del
  emulador, capa de FX, banco QUANTUM) en README y en el ABOUT del firmware; **David Konsumer (@konsumer)**
  (primera instalación en una unidad de fábrica, chequeo de chip del loader, reanudación, releases por tag) en README.
- **`firmware/src/ui_being.c`**: el encabezado decía `Copyright (C) 2026 ShadowTune / GammaWave // Wraith Custom
  Firmware`, un titular que no aparece en ningún commit, en `LICENSES/` ni entre los colaboradores. El archivo se
  creó en este repositorio y los mundos de HOME se desarrollaron acá: el aviso pasa a Guillel51 / Quantum
  GammaWave, aclarando que "Wraith" es un nombre propio de este firmware (la ribbon y la barra).
- **Marcadores de conflicto, fuera del árbol y con guard.** `firmware/src/felucca.c` llegó a `main` con un marcador de conflicto sin resolver dentro de su
  `#ifndef FELUCCA_VERSION` y `ci_artifact/manifest.json` con el JSON partido: el build
  AC791N no define la versión con `-D`, así que sólo el target fallaba. Los dos resueltos, `ci_artifact/` (registro
  de build commiteado, de un commit que ya no existe) retirado, y `tests/run_tests.sh` ahora nombra cualquier
  marcador commiteado con archivo y línea antes de compilar nada.

## 2026-11-11 — v2.7.3: emulador WASM, manifiestos y publicación completa

- Emulador WASM de calidad estrella (Web Audio Worklet, trabajo en tiempo real 44.1 kHz).
- Página de presentación y fotos del emulador actualizadas.
- Actualización del emulador WASM: 1 104 856 B → 1 112 286 B, SHA-256 `56c78aa354fd04f89e2d97b9a6e592dcbc68bcce643cb50c60aaae28cd8ae244`.
- Versionamiento de toda la web: `site/firmware/versions.json`, manfiestos y capturerros actualizados.
- Página del instalador y del editor actualizadas para v2.7.3.

- **El ABOUT publicado mostraba créditos viejos.** El paquete y el emulador de v2.7.1 se construyeron en
  `91029198`, antes del commit de créditos: el fuente ya decía Frederic Valente (@fvalente) para GW-1 y sumaba a
  Guillermo Lauria, pero el binario servido no. v2.7.2 se construye desde este árbol, así que `MENU > ABOUT >
  CREDITS` del paquete y del emulador publicados dicen lo mismo que `README.md`.
- **Mismo procedimiento, sin atajos:** paquete AC791N con la toolchain pineada, manifiesto de integridad
  (SHA-256 + tamaño, modelo de §9), emulador del mismo commit, espejo del sitio y `release/SHA256SUMS` regenerados
  juntos; `tools/fm1_sign.py verify` sobre el paquete antes de commitear.

## 2026-11-11 — v2.7.1: el conflicto del firmware, resuelto, y el paquete reconstruido

- **El arbol no compilaba.** El bump a v2.7.1 llego con marcadores de merge dentro de
  `firmware/src/felucca.c` (y de `ci_artifact/manifest.json`): `cc` fallaba con "version control conflict marker".
  Se resuelve conservando `#define FELUCCA_VERSION "v2.7.1"` y se elimina el `ci_artifact/` suelto, que era una
  copia de artefacto sin lectores.
- **Todo vuelve a nombrar una sola version.** README, `web/site/`, `web/index_pkg.html` y el banner pasan de
  v2.7.0 a v2.7.1; el arbol queda limpio (`dirty: []`) en el commit del build.
- **Paquete y emulador reconstruidos desde ese commit** (`9102919`): `release/felucca-v2.7.1.fwsc` 610 959 B,
  sha256 `8b1798b93892ffcc89956aeb3b6d5e1e8e14c1c48d3e9c472f08386fd3456de6`, imagen 541 484 B, identidad
  `FM-1_965`; WASM 1 104 650 B sha256 `e3388879…2d03`, Emscripten 6.0.11, mismo commit. Manifiesto
  `qg1-release-1` sin firma (`keyid: ""`), `fm1_sign.py verify` -> integrity only, y `sha256sum -c` limpio
  sobre las 22 entradas de `release/SHA256SUMS`.
- **Imagenes al dia:** banner y las 30 capturas de `assets/screenshots/` re-renderizadas del arbol v2.7.1
  (el splash del banner ya muestra la version real), y el espejo `site/` regenerado desde el paquete nuevo.

