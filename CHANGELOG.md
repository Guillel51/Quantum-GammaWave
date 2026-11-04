## v1.8.8 — real AC791N build, portable cache barriers, WASM rebuilt (2026-11-04)

No DSP, voice count, effect, parameter, layout or project format changed. This pass produced the
**target build** the previous one could not, and fixed what compiling it (and running the whole host
suite) exposed.

- **The firmware is now built for the AC791N.** JieLi's pi32v2 toolchain
  (`jieli-linux-toolchains-20260730.1`, sha256 of the archive
  `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`) plus the pinned SDK
  `AC79NN_SDK_V1.2.1_2023-12-13` (its three files match `SDK_SHA256` in `tools/build.py`) produce
  `build/felucca.bin` 556 856 B, `build/loader/ota.bin` 7 999 B and the package
  `build/felucca.fwsc` 611 155 B (identity `FM-1_982`). Measurements and headroom:
  [`docs/MEMORY_BUDGET.md`](docs/MEMORY_BUDGET.md).
- **Cache barriers are portable, so the host tests that needed them compile.** `csync` / `ssync`
  were inline asm at every call site (`fm1_usb.h`, `fm1_uart.h`, `fm1_irq.h`, `fm1_sys.h`,
  `fm1_flash.h`, `fm1_multicore.h`): an x86 or wasm build assembled them verbatim and failed. They
  are now `FM1_CSYNC()` / `FM1_SSYNC()` in `fm1_cc.h`, the chip's instruction on pi32v2 (same
  expansion as before) and a compiler barrier with the same ordering on a host. `persistence_test`,
  `backup_test` and `fm6bank_test` compile and pass — they were reported as "pre-existing failures"
  before, and the cause was this.
- **`FELUCCA_FLASH=0` builds again.** `flash_identify()` referenced the SFC entry points outside
  `#if FELUCCA_FLASH`, and the TAKES reel module (`gen_reel_flash.c`) and its two call sites were
  unguarded: a documented option in `BUILDING.md` did not compile. Fixed in `project.c`,
  `gen_reel_flash.c` and `main.c`; the storage-less build now passes the same checks
  (526 688 B image, 84 356 B `.data+.bss`).
- **The loader's power-cut test no longer depends on how much the two apps differ.** `ldr_test`
  cut at fixed byte counts (up to `0x80000`); once the old and the new app shared enough sectors,
  fewer bytes were programmed, the "power cut" never happened and the test failed on its own
  assumption. The cut points are now derived from the bytes the install actually programs, and the
  counter ignores the post-commit record cleanup (cutting there is not a mid-write loss).
- **The browser emulator builds and matches the firmware again.** Its simulated part needed
  `fl_status_ram` once the identification existed; the stub is guarded by `HOSTSIM_FLASH_ID` so a
  test that drives its own part keeps its own answer. WASM rebuilt at `v1.8.8`:
  `build/emu/manifest.json` carries the version, the commit and the wasm sha256.

## v1.8.7 — flash identification, OTA bounds, second-core self-test, verified backup CLI (2026-11-04)

Hardening pass over the write paths and the second core. No DSP, voice count, effect, layout or
parameter changed; no physical FM-1 was used and no target build was produced in this session
(the JieLi toolchain and the AC79 SDK are not installed here). Full evidence, including what could
**not** be run, is in [`docs/VALIDATION_MATRIX.md`](docs/VALIDATION_MATRIX.md).

- **Flash identification is now evidence, not a single reading.** `flash_ok` used to be one `0x9F`
  read against an allowlist. It now requires two independent reads that agree, a documented 1 MiB
  part at its documented capacity, and a status register that is readable, idle and **not
  block-protected** (a protected part ignores erase/program while every call returns success). Any
  other result leaves the firmware RAM-only instead of writing to a part it cannot identify.
  Policy: `firmware/hal/fm1_flash_id.h`; application: `flash_identify()` in `project.c`;
  test: `tests/flash_id_test.c` (11 cases).
- **The update path refuses packages that point outside the part.** `ota_ufw` now requires every
  `flash.bin` / `ota.bin` range to satisfy `off <= 1 MiB && len <= 1 MiB - off` (the only form that
  cannot wrap) and `flash.bin` to cover the app window, before anything is erased. A reply is only
  accepted when it echoes the flags byte the request was sent with.
- **The update loader can no longer hang for ever.** A 20-minute session deadline (`ldr_core.c`)
  turns a host that goes quiet without unplugging into a failed write with the record intact, so the
  next power-on resumes instead of looping.
- **Boot cleanup covers a half-written loader.** `ota_boot_cleanup` samples the head, the first body
  page and the record instead of four bytes, so a staging area whose first word reads `0xFFFFFFFF`
  is still cleared.
- **The second core must prove itself before it is used.** `fm1_mc_start` used to trust `alive`; the
  documented failure of some units is a wrong 32-bit load under load. Eight fixed jobs with answers
  computed on core 0 now run through the shared mailbox at boot
  (`firmware/hal/fm1_mc_selftest.h`); one wrong word and the core is held for good, `CORES 1 (BAD
  READ)` is reported and core 0 renders every part (`tests/mc_selftest_test.c`).
- **The installer can no longer write to an unrelated port.** `pair_output` returned the only output
  on the bus whatever its name; it now requires the names to match (ignoring Windows' numeric
  suffix) and otherwise reports "FM-1 not found".
- **New `tools/fm1_backup.py`.** Two independent reads compared byte for byte, SHA-256 per read, per
  object and for the file, a manifest (device identity, date, method, capacity, passes, result) and
  an explicit classification (complete / partial / inconsistent / unverifiable). Restore validates
  manifest, file hash, every object hash and CRC, the device model and the object geometry before
  the first write, reads every object back and compares it, and reports a selective restore as
  partial. Archives are written `0600` outside the repository; a destination inside a git work tree
  is refused before the device is opened. It moves **musical objects**, not raw flash, and the
  manifest says so.
- **README navigation** is the required two-row table: `WEB SITE` first and double width, `WEB
  EMULATOR`, `WEB STUDIO`, `WEBAPP / EDITOR`, `USB INSTALLER`, `FLASH FM-1`; second row `LICENSE`,
  `GPL-3.0-ONLY`. `tests/readme_nav_test.py` enforces the names, the order, the widths and that every
  destination resolves.
- **Documentation:** `SECURITY_AUDIT.md`, `INSTALLATION_SAFETY.md`, `BACKUP_RESTORE.md`,
  `RECOVERY_GUIDE.md`, `EMULATOR_FIDELITY.md`, `AC791N_SECOND_CORE.md`, `MEMORY_BUDGET.md`,
  `RELEASE_PROCESS.md`, `VALIDATION_MATRIX.md`.
- **Still open, and the reason the verdict does not move:** no cryptographic signature on packages
  (no key material in this tree), no anti-rollback (vendor bootloader, not modifiable), the
  single-bank write window, and no raw-flash backup path. Installation remains **NO-GO for the main
  unit**; the target build, the WASM rebuild and every physical check are unverified.

## v1.8.6 — FX particles, splash, note visualizer, Punch-In keyboard and Master EQ (2026-11-04)

- **FX particles:** the first FX section was redrawn at `ui.frame / 16` (about 3.75 fps); it now advances on
  every UI frame while drift is on, like the other sections, without extra allocations or reduced particle counts.
- **Splash:** the boot sequence now has stars, parallax, sparks, logo reveal and a sweep, with the same bounded
  timeline contract and no blank/black frames.
- **Note visualizer:** particles are driven by the voices actually playing (`active`, `gate`, `note`, `vel`), with
  fixed storage and bounded work for chords, polyphony and note release.
- **Punch-In keyboard:** white keys are 12 px with a 2 px gap and black keys 8 px wide, 58 px high and 6 px
  apart, preserving MIDI/F3–G5 mapping and vertical labels. `BKPOS` now follows the engine's own order, so
  A#3 and A#4 sit on the boundary of the white key they follow instead of the next one (the old table drew
  three black keys in a row and left a gap).
- **Master Biquad EQ:** real 20 Hz–20 kHz frequency mapping (`EQ_HZ`), logarithmic graphics axis, exact
  `10^(dB/40)` gain tables, Q24 internal coefficients and 64-bit response evaluation. The +24/−24 dB extremes,
  shelves, peak/notch and 480 parameter corners are tested; the graph no longer folds or drops at 20 Hz.
  The sweep also proves the poles stay inside the unit circle at 20 Hz and 20 kHz (+24 dB, both Q ends) and
  that one impulse decays back to silence.
- **Target link:** the EQ's coefficient normalisation divided 64-bit values, and pi32v2 links no
  `__divdi3`/`__udivdi3`; `qg_biquad_set_*` now normalise through a local 64/32 restoring division, so
  `tools/build.py` links again. The rebuilt package is 610.897 B (581.564 B app), SHA-256 `7f7a9ee8…`.
- **Verification:** `ui` (0 lint), `mod`, `presets`, `security`, `target`, `emu`, `sync`, web and package tests;
  target app/package rebuilt from this tree. No physical FM-1 was flashed.

## 2026-11-04 — Integration, target package and public deployment (`0f52f48`)

- **Both histories are integrated.** `origin/main` (`36d3eac`, including PR #60–#62) was merged into
  `work/v18-integration` in `9ff38e3` without force-push and without dropping either side. Conflicts in
  `ui_layer.c`, `web/editor.html`, `web/site/index.html`, `web/test_site.py` and the two captures were
  resolved against the Quantum/two-band tree; the duplicated `layer_fx`/`fx_card` calls and the duplicated
  `pf`/`PF_LOW` blocks were removed.
- **The target package is current.** JieLi `20260730.1` and the AC79 SDK files produced
  `v1.7-QG-0f52f48`, **610.897 B**, `FM-1_982`, SHA-256 `eb7d14a6…308e7b2c`. It includes the two-band
  Punch-in FX, LATCH, the key assignments and the three-head buffer engine; the old `1fae6cc` package is
  no longer offered as current.
- **GitHub Pages published the integrated tree.** The run from `0f52f48` replaced the manifest commit
  `8c101d13…` with `0f52f48011ce`; `/emulator/`, `/emulator/manifest.json`, `/webapp/editor/`,
  `/webapp/installer/` and `/firmware/versions.json` all return 200.
- **Public Chromium passed:** 1440 px and 390 px without overflow, Web Site first, seven Web Studio tabs
  visible, emulator at **60,1 FPS** with WASM `c3258d73…` (1.030.002 B), installer listing
  `v1.7-QG-0f52f48` with the downloaded package's SHA-256 verified, and no console errors.
- **Documentation is aligned with the public build** in `docs/V1.8_STATUS.md`; the temporary `ci-diag`
  diagnostics were removed from the Pages workflow. This remains `v1.7-QG`, not a v1.8.5 release, and no
  physical FM-1 was flashed.

## 2026-11-04 — Target build, real package and a deploy that survives a missing toolchain

- **The firmware is compiled for the AC791N again.** JieLi's linux toolchain (`20260730.1`) and the three AC79 SDK
  files were fetched here; `tools/build.py` produces `felucca-v1.7-QG-7768f59.fwsc`, **610.897 B**, `FM-1_982`,
  SHA-256 `eb7d14a6…308e7b2c`, from a tree that includes the two-band Punch-in FX, the LATCH fixes and the three
  buffer heads. It replaces the `1fae6cc` package, which predated those changes.
- **The site build no longer needs a package committed to the tree.** The target build produces the
  package; if the toolchain did not run, the workflow falls back to one already built for the target.
- **The Pages deploy no longer depends on the target compiler.** `build/site` used to be produced only inside the
  toolchain-gated step, so a failed download left nothing to upload and Pages never moved (that is why `/emulator/`
  answered 404 while the portal served a build from hours earlier). The site falls back to a target-built
  package when the compiler did not run, and a check fails loudly if there is still no `build/site/index.html`.
- **`rev_room` baseline 218 → 243** after measuring with the compiler JieLi ships today: `fx.c` is byte-identical to
  `ab3da86`, so the delta is the toolchain, not a render loop that grew.

# Changelog — QUANTUM GAMMAWAVE (QG-1)

All notable changes to the firmware, the Web Studio, the emulator and the installer pages.
The device is an M-VAVE FM-1 (AC791N); no entry here was tested by flashing hardware.

## 2026-11-04 — Portal access hierarchy, Quantum identity and a synced emulator

- **The portal names the browser build `Web Emulator`** (never `WASM`) and drops the leftover `#emu` hash
  redirect that replaced the portal with the emulator.
- **New `#accesos` section:** `Web Site` first and clearly the largest, with its URL beside the name, then
  `Web Emulator`, `Web Studio` and the USB installer. `web/test_site.py` now enforces the order, rejects the
  old naming and the Solar Heat wording, and fails on any internal link that has no file in the build.
- **Quantum identity on the portal and in the Web Studio:** both use the firmware's `QUANTUM` palette
  (`0x0022 0x10a5 0xef9f 0x3eff 0xfd44`). Every id, tab, handler and protocol frame of the Web Studio is
  unchanged; `web/test_web.mjs` still passes.
- **Web Studio simulator fixed:** `?mock=1` now connects at boot (it only set a flag before), the SIMULATE
  button keeps a coherent label and is idempotent, and `Link.close()` is idempotent, so a deliberate
  disconnect no longer logs `Error: closed`. Chromium 1440/390: seven tabs, no console errors, no 404.
- **Emulator manifest names the current commit** after a rebuild, and `build/site/emulator` ships that same
  `index.html` + `felucca.wasm` + `manifest.json`.
- **Installer:** the published package matches `firmware/versions.json` (`v1.7-QG-0f52f48`, `FM-1_982`,
  610.897 B, SHA-256 `eb7d14a6…308e7b2c`). It was compiled for the AC791N from the integrated tree and
  includes the Punch-in FX / LATCH UI fixes; the older `1fae6cc` package is retained only as history.

## 2026-11-04 — Web portal, Web Studio 2.0 and consonant intervals

- **Official portal at `/`.** `web/site/index.html` is a real Solar Heat / Quantum landing page: current
  build screenshots, firmware features, M-VAVE FM-1 compatibility, installation warnings, dynamic
  `firmware/versions.json` metadata and direct links to the emulator, installer and Web Studio. The Pages
  workflow now uploads `build/site`, keeps `/webapp/installer/` and `/webapp/editor/`, and serves the
  emulator at `/emulator/`. `/?emu=1` and `/#emu` redirect to it. `web/make_site.py` no longer emits a
  redirect page, and `web/test_site.py` checks ids, assets, routes and the published package hash.
- **Web Studio 2.0 surface.** The editor keeps every id, tab, protocol frame and DSP mapping; a new CSS
  layer unifies the charcoal/amber/cyan identity, the brand bar, panels, controls, focus and mobile
  overflow. Chromium 131 local: 1440 px and 390 px, seven tabs, no horizontal overflow, no console or
  failed-request errors. `web/test_web.mjs` still passes. Portal sections are visible without the scroll
  observer; its entrance animation is now progressive, and `web/test_site.py` rejects the old hidden state.
- **Consonant random intervals (JIANT V10.38, adapted).** `param_semi_pick()` and `param_semi_snap()` live
  in `params.c`; `gen.c` keeps or snaps `F_SEMI` parameters instead of adding a random detune, the sound
  dice picks a musical interval, and `ui_layer.c` excludes interval destinations from random macros.
  `tests/semi_test.c` checks 4.096 picks and the snap floors (including the 5/7 tie).
- **WASM rebuilt after the interval change:** **1.030.693 B**, SHA-256
  `5b8475aaeda9e090f022fe8879d6cb3861845838367b454718ef0d482c2fd76a`; native comparison bit for bit
  (88.200 frames), 23,5 ms/s (2,4 % of real time). No target build, physical FM-1 or online deployment
  is claimed.
- **Version decision:** **v1.8.5 is not published.** `FELUCCA_VERSION` stays `v1.7-QG`; the build is
  published and verified in software, but physical hardware, signature, rollback and independent recovery
  remain open. See `docs/V1.8_STATUS.md`.

## 2026-11-04 — PUNCH-IN FX keyboard and EQ safety pass

- **PUNCH-IN FX redrawn as a keyboard.** The ten effect cards, the separate mute row and the four
  black-effect cards are gone. The sixteen white keys and eleven black keys use the full panel height;
  each key carries its abbreviated function vertically. Bank (`B1/B2`), latch, capture state, target
  (`ALL/SYN/DRM`) and the selected track share two compact header lines. No note names are drawn.
  Every label is read from `perf_key()`; bank 2 is `REV FRZ TAPE GLCH RTRG`. Black keys 1..6 remain
  latched mutes T1..T6; G#4/A#4/C#5/D#5/F#5 remain VIB/TGT/GATE/RTRG/DIVE.
  Host UI render: **172 screens × 17 palettes, 0 lint findings, 2.647.728 alignment measurements, 0
  misalignments**. Capture: `artifacts/punch-in-fx-new.png` (240×240, rendered from the firmware code).
- **Master EQ hardening.** The setters clamp FREQ/GAIN/Q, reject coefficients outside the stable
  triangle or a sane feed-forward range, clear the delay line when coefficients change, force a
  poisoned state to zero after 32 silent blocks, and remove DC before the EQ (a +24 dB low shelf
  otherwise amplifies an offset into the leveler). `tests/eq_sweep_test.c` sweeps 480 mode/frequency/
  gain/Q corners: worst tail after silence **0**, nonzero tails **0**, bad coefficients **0**; a
  poisoned state clears; extreme low shelf → bypass → peak passes audio; 4.000 rapid changes remain
  bounded.
- **WASM rebuilt** after these changes: **1.030.207 B**, SHA-256
  `8da41752486e9f8aceade1d221e33198b415a0ae8a3944cf4a5af6f47c8600c5`; native comparison **bit for bit**
  (88.200 frames), 23,4 ms/s (2,3 % of real time). No target, browser or hardware validation is claimed.

## v1.8-QG — correcciones de audio, UI y EQ (NO PUBLICADA)

- **Low Shelf: ruido de fondo corregido y demostrado.** La causa era doble: los productos Q14 se
  desbordaban a 32 bits cuando el shelf +24 dB a 125 Hz llevaba el estado a ~16× la entrada, y la
  cuantización Q14 podía dejar `|a1|` fuera del triángulo estable. Ahora el biquad multiplica en 64 bits,
  limita estados, mete los coeficientes en el triángulo estable y vacía el residuo de ciclo límite sólo en
  bloques mudos por debajo de −24 dB. Prueba directa (cuadrada fuerte + 4.096 ceros): cola **0**; el
  filtro anterior dejaba **21.844**.
- **Phaser: intensidad y pops.** Seis etapas all-pass con realimentación 0,366 y mezcla húmeda completa
  (antes cuatro etapas y retorno fijo). DEPTH y la ganancia rampan por muestra; al apagar DEPTH se
  desvanece a seco y se reinician los estados. Medido: notch de 20 a 97.807 (más de 18 dB), primer
  sample al activar −5.526 contra seco −5.525 (1 LSB), DEPTH 0 bit-exacto. UBSan detectó que el sexto
  estado escribía fuera de `ph_s[4]`; el array se amplió a `ph_s[6]`.
- **Vúmetros y partículas.** El VU de FX es ahora una barra de 7 px con peak cap; se quitó la barra de
  envío duplicada. Las 18 partículas de fondo aceleraron de 21–45 a 55–109 unidades/frame sin añadir
  primitivas ni allocaciones. La gráfica del MASTER EQ añade guías logarítmicas, etiquetas 125/2k/20k y
  mantiene la línea 0 dB y la curva de los coeficientes reales.
- **UBSan/ASan limpios en las rutas ejercitadas.** Se corrigieron desplazamientos a la izquierda de
  valores negativos en `perform.c`, `fx.c`, `slicer.c`, `eng_analog.c`, `eng_lofi.c`, `eng_phase.c`, y
  desbordes en `fm6_core.c` (suma de fase y producto de la onda). `mod_test` y `perform_test` con
  ASan+UBSan terminan sin hallazgos.
- **12 voces: experimento medido, no adoptado.** Sobre las mismas fuentes, la canción pesada costó
  22,9 / 24,1 / 24,3 ms por segundo con 10/11/12 voces (12 = **+6,1 %**), y el estado estático pasó de
  294.792 a 295.864 / 296.936 B (+2.144 B con 12). No hay medición AC791N; se mantiene `NVOICE 10`.

- **Phaser y M-WIDE reparados y medidos.** El Phaser usa cuatro etapas all-pass estables, Q15 con `a`
  entre 0,20 y 0,95, feedback bajo y bypass con DEPTH 0. M-WIDE conserva el centro a 64 y abre el estéreo
  hacia 127 sin inversión de fase ni pérdida anormal; los tests host pasan.
- **HPF/LPF dejan de actuar como mute.** `PF_LOW` subió a `28 << 8` (~480 Hz); las pruebas conservan
  1 kHz en la banda de paso y siguen atenuando los extremos. No se quitó el filtrado.
- **EQ real y recuperado en la UI.** `fx_master_eq_db()` usa los mismos biquads del audio para PEAK,
  NOTCH y shelves; la respuesta se calcula sin división de 64 bits para el target. La gráfica vuelve a
  dibujar una curva, no una imagen decorativa.
- **Vúmetros de señal real.** Los sends y retornos de chorus/delay/reverb publican peaks reales; la prueba
  comprueba señal y caída a cero. No se usa el fader como medición.
- **Punch-in FX:** máximo de tres efectos simultáneos. Las seis capturas (REPEAT, REV, FRZ, TAP, GLC, RTR)
  ahora usan **tres cabezas de lectura independientes sobre una sola captura de 32 KB**: las 15 parejas y
  las 20 ternas se probaron con audio distinto, además de tres FX sobre las seis pistas. El cuarto efecto
  se rechaza sin apagar los activos; los mutes T1–T6 son independientes. La pantalla muestra `CAP %`,
  `n/3` y una barra de captura/lectura por tarjeta. Un único estado `LATCH:NO / LATCH:YES`.
- **Visualizador y startup:** 18 partículas y 24 partículas de arranque, sin allocaciones dinámicas ni
  esperas añadidas.
- **Emulador sincronizado por hash.** `tools/package_emu.py` genera `build/emu/manifest.json` y fija en la
  página el SHA-256 y el tamaño del WASM; la carga usa `?v=<sha256>`, `cache: "no-store"` y verificación
  con `crypto.subtle`. `tests/emu_sync.py` falla si página, manifiesto y binario no coinciden.
- **Build identificable en el emulador:** la página y `manifest.json` muestran versión, commit y SHA-256 del
  WASM; la carga es `?v=<sha256>` con `no-store` y verificación `crypto.subtle`. El workflow publica desde
  `main` o la rama autorizada `perf/security-hardening`.
- **UBSan real corregido:** `eng_analog.c` desplazaba a la izquierda un valor negativo
  (`(pitch16 - 60*16) << 2`); ahora usa `* 4`. `fuzz_proj` pasa 5.000 iteraciones sin ASan/UBSan.
- **Etiquetas del emulador visibles y validadas en Chromium:** el `fill` SVG con `var()` se sustituyó por la
  clase `.panel-label`. La página carga el WASM `b9a52ca…`, corre a 60,6 FPS y Web Studio/instalador cargan
  sin errores de página.
- **Estado:** cambios locales en `perf/security-hardening`, HEAD `c020b7e`; **sin commit ni push**.
  Target app 552660 B, paquete 610897 B; `perf_block` 372/522. `FELUCCA_VERSION` sigue en `v1.7-QG`.
  Browser real, FM-1 físico, ALSA y FPS en navegador siguen **NO VERIFICADOS**. No es una release.

## v1.7-QG — PUNCH-IN FX emulator sync

- **The emulator now shows the PUNCH-IN FX redesign.** The page requests `felucca.wasm?v=@VERSION@` so a
  rebuilt WASM is not hidden by a cached copy; `build/emu` was regenerated at `v1.7-QG` (1019205 B).
- **WASM label fix.** The 18 px effect pads draw their names directly with `cv_text_on`; the measured
  `cv_text_r` path rendered the pads but not their labels in the WebAssembly build. The LED and the
  top highlight stay; no DSP, voice count or parameter changed.
- **Effect audit recorded.** `docs/PUNCH_IN_FX_AUDIT.md` separates the 15 existing pads and the four
  black-key effects from Reverse/Glitch/Freeze/Tape Stop, which remain unimplemented. `DIVE` is a
  two-octave pitch drop, not a tape stop; `RND` and `ST8`/`ST16` are not relabelled as Glitch.
- Final target at this HEAD: app **549220 B**, package **610897 B**, identity `FM-1_982`; app SHA-256
  `616db4d6ed191e171721c7cdc1f7ea9fde80705ef1911f0dad51c1918c206f8d`, package SHA-256
  `106b5f41bd8f93fa22142e9c9669375dc784f7e65a19de2b8183d7e19acedab4`. `perf_block` 569; no new DSP.
  UI 172 × 17 with 0 lint and 0 alignment findings; installation remains **NO VERIFICADO EN HARDWARE —
  NO APTO PARA INSTALACIÓN REAL**.

## v1.7-QG — HOME encoders, mini-knobs, splash and installation safety audit

- **HOME encoder mapping fixed.** `bg_knob()` now reads `home_param()` while HOME is open instead of
  the last page's columns. KNOB 1–4 each edit their own parameter and their own mini-knob; engine
  pages keep `page_desc()`. A new host test covers every available engine and all four encoders.
- **Mini-knobs redesigned.** Concentric rim, recessed body and a 270-degree radial needle, in the
  Solar Heat/Quantum palette; the four controls are evenly spaced in the lower-right bay.
- **Splash polished.** A second electron, twelve solar ticks with longer flares and a progress-driven
  wave phase join the existing wordmark, waves and spectral horizon. No bootloader, flash or audio
  path changed; it remains a function of boot progress with no stored bitmap.
- **Hardware specification correction.** The repository now records 578 KB SDK SRAM versus the 96 KiB
  firmware budget, 14 buttons, 7 encoders plus MASTER, the ST7789-class panel and the unresolved
  320/360 MHz clock reports. `0x856014` and `0xB36014` are the two documented 1 MiB IDs in an explicit
  allowlist; unknown IDs are rejected. Neither variant has been validated on hardware.
- **Loader completion handshake hardened.** The loader now requires exactly `success\0` for the final
  `0xF0000000` ACK and returns `-14` without clearing the update record on a missing or wrong reply.
  Host tests cover a lost final ACK; neither installer reports `done` without the reboot/identity
  confirmation.
- Final target at this HEAD: app **549028 B**, package **610897 B**, `FM-1_982`, package SHA-256
  `932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`. The app slot is 581564 B,
  not the app size; `548708` and `549012` were earlier baselines. UI 172 × 17 with 0 lint;
  `perf_block 569/522`. Installation verdict: **NO VERIFICADO EN HARDWARE — NO APTO PARA
  INSTALACIÓN REAL** until physical identity, backup and USB_KEY recovery are demonstrated; see
  `FM1_INSTALLATION_SAFETY_AUDIT.md`.

## v1.7-QG — final audit and PUNCH-IN FX visual pass

- PUNCH-IN FX: rounded pads, a tighter 5-column grid and a quiet `PUNCH-IN FX · A#4 TGT · G5 CLR`
  section line, in the existing Solar Heat/Quantum palette. Drawing only; no parameter or DSP change.
- The buffered Reverse/Glitch/Freeze/Tape Stop bank was measured at `perf_block` 662/522 (+27 %) and
  below the RAMTEXT margin, so it was **discarded**. No budget was relaxed; F#5 keeps the existing
  DIVE and is not claimed as a verified tape stop.
- Installer: a failed USB `success` ACK now reports `lost` instead of marking the step finished.
- Previous target (before the HOME/splash pass): app 548708 B, package 610876 B, `FM-1_982`, SHA-256
- Final target: app 548708 B, package 610876 B, `FM-1_982`, SHA-256
  `38eb7655e903a5244663c7625db87e46317591d905e862d0ccb4795534d7351d`. Final status is
  **NO VERIFICADO EN HARDWARE**; see `docs/AUDIT_FINAL_FM1.md`.

## v1.7-QG — visual system: scope and splash

- The full-screen visualizer was recomposed instead of recoloured: a single header with the six live track
  levels, a 144 px zero-crossing waveform on the left, its real X/Y phase trajectory in a framed inset on
  the right, and the 16-band spectrum with peak caps on its own floor. The old overlapping reticle, the
  full-width trace through the bars and the crowded cloud of motes are gone. The phase plot and the
  music-reactive meters remain; the six ambient motes are deliberate and slow, not a random field.
- The boot splash is now a real title screen: the seven block letters of QUANTUM in the same glyphs as the
  QG logo, an accent Q, a solar core with a corona, two orbits and a travelling electron, a progress
  hairline under the wordmark, three written waves and a spectral horizon with its playhead. It stays
  inside the existing 240x124 canvas, the palette and the mono rule; no stored bitmap was added.
- UI passes 172 screens x 17 palettes with 0 lint and 0 alignment findings. The splash timeline still has
  425 steps of 4 ms; 198 change the screen and 227 identical rewrites are skipped. No DSP, persistent
  layout, bootloader or flash path changed. Target app grew 748 B to 548852 B; package size is unchanged
  at 610876 B (`FM-1_982`, SHA-256 `b0d4d9c56690f6224dadcefbf06690b3e33ef0d03c819e3778eb7d85dd9c6a8b`).

## v1.7-QG — performance / security hardening

- `graph_home_viz()` measured the 16 spectrum bands twice per frame because the screen is drawn in two
  88-pixel bands, so peak caps fell two rows per frame. It now measures and updates the decay once and
  draws the same bars in both passes. A native HOME->HOME render was added to `ui_render` (172 screens,
  0 lint findings, 0 alignment errors).
- Seven left shifts of negative fixed-point samples became equivalent multiplications in the DC blocker,
  DRUM-X output and DRUM output/IMAGE paths. This removes undefined behaviour without changing sound:
  all 220 golden renders are unchanged. The six-part 10-voice stress, `audio_test` and `reverb_test`
  pass under the repository's ASan/UBSan policy.
- `tests/groups.sh` gains `security` and `stress`; `docs/PERF_SECURITY_AUDIT.md` records the host and
  target measurements, the remaining single-bank/no-signature risks, and what was not tested on metal.

## v1.7-QG — flash I/O, loader bounds and FX/MASTER UI

- Flash: SPI timeout errors now propagate to erase/program/read callers instead of being discarded;
  cache and SFC waits are bounded, and a failed I/O operation cannot report success. The loader's app
  window is asserted at `[0x4000,0x93000)`; VM, autosave, FM6 bank and USR are rejected at compile time.
- Loader: the stock record at `0xE8F00` is invalidated in place (80 zero bytes), preserving the rest of
  the VM-backup sector; `fvb_boot()` no longer reads an uninitialised sequence when no home bank exists.
- UI: the first FX page gains real per-send peak meters; MASTER gains a live CLIP transfer curve, PUNCH
  envelope, DUCK gain, REL and output VU. UI lint/alignment pass 172 screens x 17 palettes; FX host
  draw cost 125.3 -> 123.9 units. The full gallery was regenerated from the firmware renderer.
- Tests: target build, loader (normal and ASan/UBSan), web, security, presets and UI renderer pass.
  No physical FM-1 verification; the single-bank/no-signature risks remain.

## v1.7-QG — Splash identity

- The splash monogram was a square `Q` (a box with a diagonal) that read as a placeholder. It is now a round
  Q ring with its tail, drawn with the same orbit/knob language as the rest of QG, in the same palette and
  mono rule. `ui_render` still reports 0 lint findings and 0 alignment errors over 172 screens x 17 palettes.

## v1.7-QG — CI budget fix (target compiler checks)

- `mod_begin` and `mod_voice` are back inside their target budgets, measured with the JieLi compiler:
  `mod_begin` 274/166 (+65 %) -> **35/166**, `mod_voice` 36/30 (+20 %) -> **19/30**. The per-slot work is
  out of the loop (`mod_block_slot`, `mod_scan`), `mod_tsrc` / `mod_voice_add` are no longer inlined there,
  and the per-voice sources are resolved once. No budget line was changed; `mod_test` passes and all 220
  golden renders are byte-identical.

- `mod_voice` resolves the per-voice sources (ENV, VEL, KEY, RAND, PAT) once, before the slot loop, and
  indexes them there. 1.6's PAT added a source test to that loop, whose weighted cost had a 30 budget;
  the loop now does one load per slot instead of re-testing the source, with identical renders (220 goldens,
  0 changed).
- `tests/mod_test.c` still asserted `MS_N == 15`; PAT was appended in 1.6 (15, `MS_N` 16). The test now
  checks both NOISE 14 and PAT 15, and the matrix suite passes again on `main`.

## v1.7-QG — Splash dust

- The boot splash gains deterministic particles behind the three written waves, in the saved palette and mono
  rule, with no extra frames of drawing (197 changing frames of 425). `ui_render` now derives the timeline
  length from `SPL_WRITE + SPL_HOLD` instead of a hardcoded 425.

## v1.7-QG — UI Evolution & Hardware Validation (2026-11-03)

- UI audit kept at 0 lint findings and 0 alignment errors across 171 screens x 17 palettes.
- `tools/budget_report.py`: static RAM for 8/10/12 voices, library composition and host CPU baselines, with
  the host/target distinction built into the output.
- `docs/HARDWARE_SIMULATION.md`: what the host stubs and the wasm shim model, and what they do not.
- `docs/FEATURES_1.7.md`: every requested feature classified as already present, deferred with a reason, or
  blocked by the frozen project format. No persistent parameter was added.
- **MIDI Learn (session only):** CC 119 arms the parameter id of the track's engine, the next CC binds to it,
  CC 118 clears every binding. RAM only, so projects, user presets and settings keep their layout; the MIDI
  control test covers the range, the clear and an out-of-range arm.
- `tests/stress_run.sh`: project, persistence, robustness and MIDI paths under ASan/UBSan for N runs, then
  two full golden passes, all against the simulated flash.
- `tools/hw_model.py`: block deadlines, half-buffer latency, DMA and LCD bandwidth, SRAM share per voice
  count and the cycle ceiling at 160/240 MHz — arithmetic from documented constants, never a measurement.
- **UBSan findings fixed:** ANALOG shifted negative samples left twice (`y << 1`, `voice_amp(...) << 1`);
  both are now `* 2`, the same DSP with defined behaviour (goldens unchanged). The FUN5 persistence test
  wrote 27+ globals into a 27-entry header; it now stops at the old format's size.
- Version 1.7-QG across firmware, Web Studio, installer strings and documentation.

## v1.6-QG — EVOLVE / MOTION, Touch & 200 Sounds (2026-11-03)

### Library — exactly 200 factory sounds
- Retired from browsing the two least distinct DRUM kits, **HEARTH KIT** and **CYBER DRUM**. They stay in
  the table as legacy entries at their original indices 10 and 11, so a project, favorite or user preset
  made before 1.6 loads the exact kit it saved (their golden hashes are byte-identical to v1.5). DRUM's
  ten browsable sounds keep indices 0..9 and the library is exactly 200 (`core.h engine_library`).
- Added 16 original sounds (not variations): QUANTUM BLOOM, ABYSSAL DRONE, PRISM KEYS, GLASS BELL,
  AURORA PLUCK, OCEANIC PAD, EMBER LEAD (ANALOG); EVOLVING GLASS (PHASE); TAPE DRONE (LOFI);
  CATHEDRAL REED, TIDAL ORGAN (WHEEL); BREATH CHOIR (FORMANT); SOLAR WIND (NOISE);
  EVOLVE BELL, VELVET EP, AURORA FM (FM6).
- Per engine: ANALOG 56, PHASE 25, LOFI 25, FORMANT 20, WHEEL 18, DRUM 10, NOISE 17, FM6 29 = 200.
- `tests/preset_library_test.py` (run by `tests/run_tests.sh`) checks the total, each engine's share,
  DRUM's ten preserved indices, the retired names and every entry's ranges.

### EVOLVE / MOTION
- New LFO wave **EVO** (`P_LWAVE = 6`, appended to `N_LWAVE`): a smooth random walk with one new point per
  cycle, eased with an integer smoothstep. Reuses RATE, PHASE, FADE, the LFO destinations and the matrix.
- Negative RATE is already the musical division on the transport's grid (4BAR..1/32); positive RATE is the
  free rate. Both are used by the demo sounds. No new parameter, no format change.
- Demo sounds: QUANTUM BLOOM, ABYSSAL DRONE, OCEANIC PAD, EVOLVING GLASS, TAPE DRONE, SOLAR WIND,
  CATHEDRAL REED, TIDAL ORGAN, EVOLVE BELL, AURORA FM.

### Touch — velocity and aftertouch, honestly
- Channel aftertouch (MIDI `0xD0`) continues to feed `MS_AT`, per track.
- New per-voice source **PAT** (`MS_PAT`, appended): polyphonic aftertouch (MIDI `0xA0`) from an external
  controller, routed to the matching held key. The FM-1's 27 key switches have no pressure sensor, so the
  device's own keys never fake per-key pressure.
- Sensitivity and curve: the matrix AMT per destination (CUT, SHP, AMP, PITCH, VIB, E1..E8, CHO, DLY, REV,
  RATE); engines keep their own velocity response. Adding stored curve/sensitivity parameters would have
  grown `P_COUNT` and broken the frozen project layout, so it was deliberately not done.
- Presets demonstrating the routes: PRISM KEYS (VEL→CUT, AT→CUT), GLASS BELL (VEL→CUT, PAT→VIB),
  AURORA PLUCK (VEL→CUT, AT→REV), BREATH CHOIR (AT→E1 vowel), VELVET EP (VEL→CUT, AT→VIB),
  EMBER LEAD (VEL→CUT, AT→VIB).

### UI
- HOME and EDIT WAVE 1 rotary knobs redrawn as real round knobs: filled body, 270-degree ring, value arc,
  needle and centre dot; the offset orbit ring and the bakelite artefact are gone.
- Boot splash redrawn as one SURF card in the UI's language: theme rule, QG monogram with a dotted orbit,
  three waves written across it, the 16-segment rail and its playhead. The old octagonal frame's dark bands
  (read as four black lines) are gone. It is still shapes only, in the saved palette, and still written by
  the same 4 ms timeline.
- No change to the device bootloader, flash layout, or recovery code.

### Verification (see `docs/PROJECT_STATUS.md`, `docs/POLYPHONY_1.6.md`, `docs/TOUCH_1.6.md`)
- **VERIFIED on the host:** 200-sound library test plus the two legacy DRUM indices; UI lint/alignment
  (0 findings) and PNG renders; 220 golden renders and the health suite; installer, Web Studio and backup tests; sanitizer/fuzz runs when
  the local compiler supports them.
- **SIMULATED:** the emulator page and its installer strings, from the same firmware sources.
- **NOT VERIFIED:** anything on a physical FM-1; a 12-voice build on hardware; a rebuilt `felucca.wasm`
  when Emscripten is unavailable in the build environment; package signing (design only, not enabled).
