## v3.0.0 — 2026-11-13 — CPU target dentro de presupuesto y biblioteca de 273 sonidos

## v3.1.1 — 2026-11-14 — instalado en las dos memorias flash (informado por el dueño)

- El dueño informa que el paquete v3.1.1 se instaló en **dos FM-1, una con Puya P25Q80H (`0x856014`) y
  otra con UCUN UC25HQ80 (`0xB36014`)**. Se registra en `docs/VALIDATION_MATRIX.md` §11 como evidencia
  **informada por el dueño**: no la ejecutó ni la observó este entorno, no hay lectura de JEDEC, ni
  geometría, ni dump, ni recuperación ensayada, ni prueba de salir a otro firmware.
- `release.yml` vuelve al pin por SHA de `fm1-build`: ese SHA **es** el tag revisado `v1` y la
  attestation nombra el commit, así que fijar un tag mutable era la opción débil.

## v3.1.1 — 2026-11-14 — Hub, attestation y versión publicable

### Changed
- `FELUCCA_VERSION` -> **v3.1.1**. v3.1 no podía publicarse: `tools/release_gate.py` exige `vX.Y.Z`
  y rechazaba la forma de dos componentes, así que el tag y la release nunca se crearon.
- **`release.yml` usa `zvenson/fm1-build/.github/workflows/build.yml@v1`**, el tag revisado que el
  Hub reconoce, en lugar del commit fijado: con el SHA el paquete podía attestarse pero la tarjeta
  no habría obtenido «Built from source on GitHub».
- **`fmhub.json`**: 275 sonidos (no 273) y color `#FF9424`, el acento Quantum; `check_fmhub.py`
  valida campos, longitud, enlaces y contraste (9.5:1 sobre negro).
- **`docs/card.png`** regenerado desde el render real de HOME en la paleta Solar Heat, 480 × 480,
  16 336 B.
- `docs/IDENTITY.md` y `docs/FM1_HUB_READINESS.md` con las cifras de este build (574 992 B de
  583 612 B) y la biblioteca de 275 sonidos.

### Pending
- La release la crea el workflow **sólo** con `QG_RELEASE_APPROVED=true` en las variables del
  repositorio; esa autorización no se toca desde aquí.
- El Hub exige una instalación real desde V15 y desde otro firmware: **PENDING HARDWARE**.

## v3.1 — 2026-11-14 — validación, sincronización y release

### Changed
- `FELUCCA_VERSION` -> **v3.1**. El firmware es el auditado en las fases 3–5: 275 presets
  (0 silenciosos, 0 calientes, 0 nombres repetidos), 574 988 B de 583 612 B, RAM code 19 584 B.
- **Emulador recompilado** con Emscripten **6.0.11** (la versión fijada por `web/emu/build.sh`):
  WASM de 1 207 502 B, SHA-256 `ff4f0ce1…`, build `487735744fb5`. Antes el sitio sólo podía servir
  un emulador de un commit anterior.
- **Artefactos de release**: `release/felucca-v3.1.fwsc` (610 959 B, SHA-256 `b6b0088a…`),
  su manifiesto (integridad SHA-256 + tamaño + registro de build, sin firma: `keyid` vacío),
  `release/build-manifest-v3.1.json`, `release/emu/` y `release/SHA256SUMS` (103 entradas).
- **Portal reconstruido desde esos bytes**: `versions.json` 3.1, registro del emulador con su WASM
  y su commit, instalador y editor regenerados.

### Verified
- Suite host completa (`NO_PACKAGE=1 ./tests/run_tests.sh`), golden sin tocar, ASan/UBSan y fuzzing
  con semillas fijas, `target_budget.py` exit 0, `install_test`, `test_web`, `test_backup`,
  `test_pkg_bounds`, `emu_test`, `coherence_check` (ok), `site_check --url` (todo ok), `test_site`.
- Navegador real: `/` en 3.1 con el hash del paquete; `/emulator/` carga el WASM de 1 207 502 B y
  dibuja el canvas de 240 × 240.
- El paquete y el WASM son idénticos byte a byte entre el build sucio y el build desde el commit.
- **Dos rojos heredados de la Fase 5, corregidos**: los goldens WTABLE (10 hashes cambiados y 2
  nombres desaparecidos por la normalización de niveles y los renombrados de la Fase 5) se
  re-registran y vuelven a dar 293 renders, 0 cambiados, 0 ausentes; y `particle_bounds_test`
  comprobaba el texto literal de un guard que la Fase 2 hizo crecer — ahora comprueba su forma y el
  hit test del campo profundo, más estricto que antes.
- `docs/QG_3.1_RELEASE_2026-11-14.md`.

### NOT verified
- Nada se flasheó ni se leyó de una FM-1 física: **hardware no verificado**.
- No se escuchó audio del emulador ni se ejercitó un dispositivo MIDI real.

## v3.0.5 — 2026-11-14 — FASE 6: nuevo banner, portal reescrito, capturas reales

### Changed
- **Banner nuevo desde cero** (`tools/gen_banner.py`, regenerable): campo índigo con un sol
  térmico cuantizado por matriz de Bayer, anillos de la rampa de calor del firmware, el hexágono
  del tema QUANTUM y un espectro de 72 barras; el nombre en Michroma a 96 px con relleno
  degradado. **Sin número de versión** y sin tipografía por debajo de 24 px.
- **Portal**: la introducción responde qué es, para qué dispositivo y qué se puede tocar antes de
  cualquier cifra; sección "Qué es" nueva con tres tarjetas (tocar, programar, sonar); cifras
  reales (275 sonidos, 22 paletas); el banner es la `og:image`.
- **Capturas del firmware regeneradas** con `tools/fw_shots.py --render` desde `tests/ui_render.c`
  (el código auditado en las fases 3–5), no desde mockups.

### Verified
- `web/make_site.py` construye el portal completo desde esas fuentes (instalador 247 399 B,
  paquete `felucca-3.0.0.fwsc` 610 959 B, FM-1_965); servido en local: `/` 200, banner 200, título
  y textos nuevos presentes. Vista previa abierta para el dueño.
- `docs/QG_FASE6_PRESENTATION_2026-11-14.md`.

### Blocked (declared, not worked around)
- Publicar 3.0.0 exige recompilar el emulador WASM (`web/emu/build.sh`) y **aquí no hay `emcc`**,
  así que `site/` no se sobrescribió: sigue siendo el despliegue 2.9.7 coherente y
  `tools/site_check.py` no tendría nada que reconciliar. `release/` también sigue en 2.9.7.

## v3.0.4 — 2026-11-14 — FASE 5: 20 presets reachable again, two sounds added, the bank counted

### Fixed
- **WTABLE's 20 factory sounds were invisible.** `tools/gen_preset_categories.py` never listed the
  engine, so no browser page, filter or category showed them, and `tests/preset_categories_test.py`
  asserted a fixed 251 and stayed green. The engine is listed now (20 + the 2 new sounds) and the
  test asserts the measured count: **273/273 presets with one category each**.

### Added
- `ELECTRIC TINE` (KEYS, electric piano) and `GLASS CHIME` (BELLS) in WTABLE: two families the bank
  lacked by name, built from the engine's existing MORPH/HARM/CUT/RES and filter envelope — no new
  DSP. Both renamed off `TINE EP` / `CRYSTAL BELL`, which FM6 already uses.

### Audited (host x86, this build)
- Inventory counted from the tables: 275 presets in the firmware, 251 in the browser before this
  change (WTABLE absent, DRUM's 2 legacy kits filtered by id on purpose).
- `tests/preset_audit_test.c`: 275 presets, 0 silent, 0 hot, 0 duplicates, 0 out of range.
- Levels: median -18.5 dBFS, no preset above -0.5 dBFS peak; 27 presets remain out of the fader's
  reach (listed in the doc), NOISE sits 7 dB under the bank — reported, not flattened.
- Goldens: 293 renders, 0 changed, 2 added. `docs/QG_FASE5_PRESET_BANK_2026-11-14.md`.

## v3.0.3 — 2026-11-14 — FASE 4: editing audit, and the LARGE footer stops eating names

### Fixed
- **MENU > LARGE cut the sound's name down to three glyphs.** SEQ's tools read `SAW…`, `FUL…`,
  `NO NA…` because the page's title and its `n/m` index took the footer line. The title now gives up
  the index first, and only when the name would really be truncated (the test is the name's own
  width), so a page whose name fits keeps its counter. Render sweep: 516 ellipsised free texts
  (17 distinct) down to 278 (7 distinct), 0 lint, 0 alignment deviations.

### Audited (host x86, this build)
- Every EDIT knob of every engine, at both ends of its range through the real render:
  **216 knobs x 3 presets, 0 with no effect, 0 faint** (`tests/engine_param_test.c`).
- Modulation (`tests/mod_test.c` 71 checks), ratchet (63), swing (11), scale/chord — all ok.
- FX: phaser, M-WIDTH, EQ shelves (`tests/fx_probe.c`), delay train (18), reverb (32), cloud (6),
  EQ sweep (9) — all ok. No effect was added: nothing measured justified one.
- `docs/QG_FASE4_EDIT_AUDIT_2026-11-14.md`: the numbers and the open backlog. **The phase stays
  open** — musical ranges, MIXER's LARGE footer, SEQ parameter locks and a systematic
  parameter-step pop instrument are not done.

## v3.0.2 — 2026-11-14 — FASE 3: panel depth and the Solar Heat ground

### Changed
- **A panel is a surface, not a flat plate.** `cv_rrect` already beveled large plates in the LINE
  style — frame, lit top edge, shaded foot, the theme's corner mark — while the rounded style that
  almost every screen uses filled them with one flat colour, which is why only FX > BUSES looked
  deep. The same treatment now comes from the primitive, so all 189 screens gain it at once: a 1 px
  frame, a lit edge under the top, five rows of falloff on a card and nine on a panel, and the
  palette's accent as the short mark on the top border. Keycaps, chips and meter bars are untouched.
- **The Solar Heat ground runs on every style.** `graph_heat` used to be LINE-only, inset from the
  top, and dithered every pixel of the canvas. It is now three bands of shade under the bezel and
  six of glow along the foot, with the middle of the panel left exactly as the screen drew it, in
  every style: violet shadow above, solar gold below. MONO keeps its neutral page; on a light
  palette the glow is halved and taken from the ink, where an accent is darker than the paper.
- Cards keep the frame and the top fade but no foot glow: their value strip is repainted while a
  number rolls, and a ground under those rows flickered back on the next full redraw.
- `docs/QG_FASE3_PANEL_DEPTH_2026-11-14.md`: the measurements, the three attempts that were
  rejected and why, and what is still open.

### Verified (host)
- `build/ui_render`: 189 screens x 22 palettes, 0 lint findings, 0 inks on their own colour,
  0 GREY / MONO pixels off neutral, 3 390 046 alignment measurements, 0 off by 1 px.
- `build/ui_test`: 888 ok, all passed. `./build.sh`: ok, image +384 B, package 610 959 B.
  `tests/target_budget.py`: exit 0.

## v3.0.1 — 2026-11-13 — Home visualizer knob 1 focus and release

### Fixed
- **HOME's spectrogram no longer scrolls on its own.** Each of the 28 bands now measures its OWN slice of
  the newest window (a peak over four samples) where it used to read one sample each at a fixed lag from the
  newest one — so a waveform travelled across the bars from right to left for as long as the music played.
  KNOB 1 is the only thing that moves the picture sideways: the bands read `i + (KNOB 1 − centre) × 14 / 128`,
  a pure function of the control, so the same value always draws the same place, turning back restores the
  picture exactly, and nothing moves when the hand is still. KNOB 2‑4 keep their parameters, their readouts
  and their effect on the bay, and never take the pan.
- **Every world of HOME answers a touch.** Only the ocean's touch drew anything: the rocky, banded and ringed
  worlds registered the gesture and answered with a 4 px line or nothing at all. Each now has its own answer
  (a turning arc with a grain thrown off the terminator, a breathing halo with glints along the belts, a wake
  widened by the slingshot with embers off the ring's edge), and HOME's own button tap pulses the whole system
  instead of writing a reaction type into `pl_react[]`, an array no screen ever read.
- **CPU optimizations validated.** All four static CPU overruns closed without touching the baseline: `phase_render` 84/84, `wheel_block` 32/86, `noise_render` 219/201 (+9%, within +10%) and `fm6_render` 292/322.

### Changed
- HOME's upper bay: five thermal stops per band (violet, red, orange, gold, solar yellow), a 1 px ember either
  side of the loud bands, a dotted logarithmic grid, a brighter crest, and a drawn cutoff that follows a turn a
  third of the way a frame and lands exactly on the parameter.
- The full-screen visualizer's two background worlds are HOME's own bodies, larger (11 and 8 px) and mixed
  darker, with belts, craters and a rim; each answers a tap with a ring and a mote.
- `docs/QG_FASE2_HOME_VISUALIZER_2026-11-14.md`: the measurements, the tests and what is still open.

### Release
- **Not published.** The tree builds a v3.0.0 package (`build/felucca.fwsc`, 610 959 B) but
  `release/felucca-v3.0.0.fwsc` and its manifest are not in the repository and the browser emulator still
  reports v2.9.7: publishing needs a version decision, `./build.sh --release`, a WASM rebuild (`emcc`) and a
  `site/` sync, none of which this change does. `tools/coherence_check.py` lists the three problems.

### Added
- **Ninguna versión 3.0 se crea, etiqueta ni publica sin aprobación.** `tools/release_gate.py`
  bloquea `v3.0.0` y posteriores salvo que la variable de repositorio `QG_RELEASE_APPROVED` valga
  `true` (también `1` o `yes`). `tests/release_gate_test.py` cubre 2.x, 3.0, rc, versiones
  malformadas y la aprobación por variable. `tag-and-publish.yml` y `release.yml` llaman la misma
  compuerta; `release.yml` deja de correr en `main` y sólo acepta tags `v*`.
- **La tarjeta del Hub dice el conteo real.** `fmhub.json` pasa a “nine engines and 273 sounds”.
  No se agrega `sounds`: el esquema del Hub descarta el archivo entero ante un campo desconocido.
- **Familia WTABLE de morphing.** MAREJADA, PRISMA, CORAL GLASS, VOCAL TIDE, ARCO SOLAR y MORPH SUB
  mueven MORPH con el LFO o con POSMOD en lugar de dejar la tabla fija. La biblioteca pasa a 273
  presets; la auditoría da 0 silencios, 0 duplicados y 0 fuera de rango, y los 285 goldens previos
  no cambian (sólo se añaden las seis líneas nuevas).

### Fixed
- **El build AC791N vuelve a compilar.** `params.c` copiaba dos `param_desc_t` en un inicializador
  estático; el clang 4.0.1 del target lo rechaza. Ahora usa punteros a los descriptores, sin cambiar
  valores ni comportamiento. El paquete resultante mide 610 959 B; RAM, pool y `.ram_text` entran en
  presupuesto.
- **Los cuatro overruns de CPU estática quedan cerrados sin tocar el baseline.** `phase_render` vuelve
  a 84/84 al separar NOISE (`phase_render_noise`, 103/103) y HARM (`phase_harm`, 23/23) del camino
  normal; `wheel_block` baja a 32/86 desenrollando sus dos rotores y unificando la fuga con un
  recíproco exacto de 1270; `noise_render` baja a 219/201 al cambiar `isqrt32` por una tabla exacta de
  128 entradas; `fm6_render` baja a 292/322 al simplificar la condición de etapa de la envolvente.
  `regress` sigue en 291 renders, 0 cambiados y 0 over budget.

### Changed
- `docs/QG_3.0_PLAN_AUDIT_2026-11-13.md` registra el estado de las doce fases, lo que ya existía y
  lo que sigue bloqueado por target, WASM, navegador o hardware. `docs/QG_RELEASE_BLOCKERS.md`
  documenta la compuerta.

### Release
- **Publicación condicionada a la evidencia.** `v3.0.0` se etiqueta y publica desde el paquete y el
  registro de build de `release/`, después de la suite host, el build AC791N, la compuerta de CPU,
  `memory_map_test` y el registro del WASM. La matriz física sigue separada: no hay FM-1 en esta
  sesión, así que la release declara validación software/target/WASM, nunca una prueba de hardware.
  El detalle por clase está en `docs/QG_3.0_RELEASE_REPORT_2026-11-13.md`.

## v2.9.7 — 2026-11-13 — El arranque corre bajo el splash, una etapa por cuadro

### Fixed
- **El cuadro congelado del arranque.** `main.c` ya pintaba el splash a medida que terminaban las
  etapas de inicialización, pero las tres que tocan flash —la lectura del autosave, el reel de TAKES y
  los dados de la sesión— se ejecutaban **antes** de que la animación diera su primer paso: la imagen se
  quedaba en su primer cuadro durante toda la lectura SPI y luego corría el resto de golpe.
- Ahora son el trabajo del propio bucle del splash: se escribe un cuadro, corre **una** etapa, y sigue el
  siguiente. El orden de las etapas no cambia (autosave, reel, dados), el reloj sigue siendo el que
  arranca con el primer píxel y el splash conserva su duración: el trabajo se mete en los cuadros que ya
  se estaban esperando, no se añade tiempo.
- Un reloj atascado no deja la música sin leer: al salir del bucle, toda etapa que no haya corrido corre
  una vez. `STARTUP SPLASH OFF` sigue sin splash y sin bucle, y las tres etapas se ejecutan igual.

### Changed
- Nada del audio, del secuenciador ni del mapa de flash: sólo el orden en que el arranque cede el turno
  al cuadro. Sin RAM nueva.

## v2.9.6 — 2026-11-13 — SCL dice las relaciones, no sólo las notas

### Added
- **SCL > SCALE: los grados por intervalo.** Bajo la columna de la derecha, la escala se escribe como lo que
  es sobre la tónica —`P1 m3 P4 P5 m7` en una pentatónica menor, `P1 M2 M3 P4 P5 M6 M7` en una mayor—, leído
  de `scale_offsets`, los mismos semitonos que el motor cuantiza. Una escala cromática no se enumera (doce
  nombres en 124 px se elidían): dice **ALL 12 NOTES**.
- **SCL > CHORD: el acorde deletreado.** La línea de notas pasa de `C E G` a `P1 C  M3 E  P5 G`, con el
  intervalo de cada voz sobre la fundamental, calculado del mismo `chord_from` que toca la tecla: la tercera
  es una tercera y no una letra suelta.
- **El teclado dice su octava.** La celda de C del teclado lleva **C4** (la tira está afinada en esa octava),
  así que las doce celdas dejan de ser pitch classes anónimas.

### Changed
- Nada del sonido: las tres páginas siguen leyendo la máscara de escala y `chord_from` del secuenciador.

### Performance
- La imagen pasa de 568 616 B a **569 144 B** (+528 B): las tres cadenas de intervalo, el caso cromático y
  la octava del teclado. Quedan **14 468 B** libres. Sin RAM nueva (87 748 B de 98 304, pool spare 15 392).

## v2.9.5 — 2026-11-13 — El suelo Solar Heat en todas las pantallas

### Added
- **El degradado de HOME, en las páginas de sección.** `graph_heat()` (`ui_graph.c`) pinta doce bandas
  tramadas dentro del panel redondeado de cada página — EDIT, ENV, LFO, MOD, SCL, CHORD, FX, FX-BUS,
  SEQUENCER, PRESETS, SAVE > USER, PROJECT, ABOUT, MIDI — oscurecido arriba y cálido hacia abajo, con el
  mismo lenguaje que las cinco pantallas de HOME. Va dentro de los radios (7 px a los lados, 2 arriba y
  abajo), así que ninguna banda dibuja un borde recto sobre la esquina.
- **Coste medido**: doce rectángulos tramados en un redibujado completo y **cero** en un cuadro cacheado;
  no usa reloj de animación, no añade partículas y deja planas las pantallas MONO y la paleta BW (un
  tramado ahí es sólo ruido). La imagen pasa de 568 488 B a **568 616 B** (+128 B); quedan **14 996 B**
  libres en el slot.

### Changed
- Las capturas se regeneraron: el suelo cambia el fondo de casi todas las páginas de sección.

### Compatibility
- Sin cambios de protocolo ni de parámetros: es dibujo, no DSP. Los 285 dorados de audio siguen en 0
  cambiados porque no tocan píxeles.

## v2.9.4 — 2026-11-13 — Sincronización total del ecosistema

### Changed
- **HOME tiene cinco pantallas**: MAIN, VISUALIZER, OVERVIEW, MIX y PERFORM. `HV_COUNT` es 5 y con él
  desaparecen `HV_ENG`, `HV_MOD`, `HV_DYN`, `HV_SYSTEM` y `HV_ROUT`: no queda índice viejo que caminar,
  dibujar ni guardar. La tira del título son tres celdas — las tres páginas que la dibujan — y el pie
  escribe `HOME n/HV_COUNT`, que se lee `/5`. Ninguna función se pierde: esos controles son las páginas
  de EDIT, MOD, EDIT > ENV, SYSTEM y FX > BUSES.
- **README reescrito** en lo que respecta a HOME, a las cifras del build y al estado de publicación; el
  README decía «diez pantallas de HOME» y daba `v2.9.3` por candidato cuando ya estaba publicada.
- **Sitio y capturas regenerados**: `tools/generate_screenshots.py` vuelve a dibujar las 186 pantallas y
  `tools/fw_shots.py` recompone las imágenes del portal. La figura que el sitio titulaba «HOME 9/9 —
  RUTEO» es ahora la página real **FX > BUSES**, y la matriz del portal es la sección **MOD**. Se
  eliminaron `quantum_32_home_engine.png`, `quantum_33_home_mod_matrix.png` y
  `quantum_36_home_routing.png`: mostraban pantallas que ya no existen.
- **Emulador y paquete reconstruidos** desde este árbol: WASM **1 191 701 B** (sha256 `067f9a07…`),
  paquete `release/felucca-v2.9.4.fwsc` **610 959 B** con su manifiesto, y el sitio regenerado con ese
  paquete y su registro de emulador.

### Fixed
- `tools/fw_shots.py` apuntaba a las capturas de las vistas retiradas de HOME; ahora renderiza
  `fx_buses` y `lfo_dest`, que son las páginas que tienen esos controles.
- `firmware/src/felucca.c.bak`, una copia muerta del fuente versionada por accidente, eliminado.

### Performance
- La imagen baja de 573 664 B a **568 488 B** (−5 176 B) porque el dibujo de las cinco vistas retiradas
  queda inalcanzable y `--gc-sections` no lo mete: el espacio libre del slot pasa de 9 948 B a
  **15 124 B**. RAM sin cambios: 87 748 B de 98 304, pool spare 15 392 B.

### Compatibility
- Sin cambios de protocolo: el Web Studio y el instalador siguen hablando los mismos ids SysEx; la
  versión sube a `v2.9.4` porque los bytes cambian y `v2.9.3` ya está publicada con su hash.

### Known limitations
- El instalador sigue **cerrado**: la reproducibilidad del `uboot.boot` no está demostrada (dos
  compilaciones distintas, 14 444 B y 14 536 B) y faltan lectura real de JEDEC, backup físico y
  recuperación demostrada.
- El congelamiento del bootsplash está diagnosticado y **no** corregido: el reloj es correcto (TIMER4
  desde el primer píxel, 176 de 1125 pasos de 4 ms cambian la pantalla) y lo que falla es la cadencia de
  repintado, atada a los límites de las etapas de inicialización.
- SCL, EDIT/LFO por motor y el barrido Solar Heat de las pantallas que no son HOME **no** están
  implementados; están documentados con su alcance en `docs/QG_UI_AND_SOUND_DESIGN_REFINEMENT.md`.
- Nada de este build se ha ejecutado en un FM-1 ni en un navegador: ver
  `docs/QG_ECOSYSTEM_FULL_SYNC_REPORT.md` para la tabla de estados por componente.

## 2026-11-13 — HOME, cinco pantallas

- **HOME queda en cinco pantallas**: MAIN, VISUALIZER, OVERVIEW, MIX y PERFORM. `HV_COUNT` es 5 en
  `firmware/src/ui.c`, así que `HV_ENG`, `HV_MOD`, `HV_DYN`, `HV_SYSTEM` y `HV_ROUT` dejan de existir: no
  hay índice viejo que caminar, dibujar ni guardar. La tira de la esquina del título pasa de ocho celdas
  (124..227) a **tres** (190..227), las tres páginas que la dibujan, y el pie sigue escribiendo
  `HOME n/HV_COUNT`, que ahora se lee `/5`. **Ninguna función se pierde**: los ocho parámetros del motor
  son la primera página de EDIT, la matriz es MOD, la envolvente es la página ENV de EDIT, el recorrido de
  la señal es FX > BUSES y el sistema es la sección SYSTEM. El dibujo de esas páginas sigue en
  `ui_homeviews.c`, ya inalcanzable, y `--gc-sections` no lo mete en la imagen.
- **Medido [HOST]**: barrido de **186 pantallas × 22 paletas** (191 − 5), 0 hallazgos, 3 152 530 medidas
  de alineación con **0 desalineaciones**, 0 tintas sobre su propio color; **285 dorados, 0 cambiados**;
  `ui_test` completo en verde, con el recorrido MAIN → VISUALIZER → OVERVIEW → MIX → PERFORM → MAIN en
  ambos sentidos y la comprobación de que ninguna de las tres páginas dibujadas toca un parámetro.
- **Investigado y no cambiado, con la causa escrita** en `docs/QG_UI_AND_SOUND_DESIGN_REFINEMENT.md`: el
  bootsplash no está congelado por el reloj (TIMER4 corre desde el primer píxel y el arte avanza: 176 de
  1125 pasos de 4 ms cambian la pantalla), sino porque sólo se repinta en los límites de las etapas de
  inicialización; intercalarlas exige reordenar el arranque y validarlo en el FM-1 **[HW]**. Reverb de
  FX-BUS, la mini pantalla de SAVE-PRESETS, las tres páginas de SCL, EDIT/LFO por motor y el barrido Solar
  Heat quedan **sin implementar** en esta pasada, también documentados: el espacio de flash es de 9 948 B,
  suficiente para la UI de SCL y la página de LFO, no para un juego nuevo de parámetros DSP en nueve
  motores sin podar en otro sitio.

## 2026-11-13 — Consolidación: el bloqueo del SPL, medido; el árbol, puesto en su sitio

- **El bloqueo del SDK, resuelto hasta donde se puede desde aquí.** `tools/spl_diff.py` lee el `uboot.boot`
  de los paquetes y lo compara: el de v2.9.0 son **14 444 B** (`64ccc63e…`) y el de v2.9.2 **14 536 B**
  (`4e3b4c22…`), con payloads de 14 428 y 14 520 B. No es el mismo blob en otro contenedor: es **otra
  compilación del SPL**, 92 B más grande. Lo que falta para cerrarlo —una copia verificable del SDK
  `0617f4cc…`, o un archivo del SDK fijado por SHA-256 como ya se hace con el compilador— está escrito en
  `docs/QG_RELEASE_BLOCKERS.md`. Los pines de `tools/build.py` se quedan como están: son la medida.
- **`tests/test_polyphony.c` eliminado**: era una copia muerta del asignador de voces con su propio
  `track_t` falso y ni siquiera compilaba (`lvalue required as unary '&' operand`); no enlazaba el firmware,
  así que no podía fallar cuando el asignador real se rompía. La cobertura existe en `tests/regress.c`
  contra el motor real (presupuesto compartido, robo con fundido, MONO/LEGATO/UNISON, tope de VOICE).
- **README**: los destacados seguían siendo de `v2.7.4.7` (imagen 541 484 B, WASM 1 104 650 B) mientras la
  cabecera decía v2.9.2; ahora salen de este build (imagen 573 664 B con **9 948 B** libres, tablas
  41 947 B, WASM 1 223 359 B, 285 dorados, 267 sonidos con mediana de banco −18,5 dBFS).
- **Capturas regeneradas** (`tools/generate_screenshots.py`): 9 de las 40 cambiaron porque la tira de HOME
  tiene ocho celdas y HOME > ROUTING se movió con ella; ninguna es de una versión anterior.
- **Medido, no copiado**: 267 presets en 9 motores con 0 problemas (`preset_audit`), mediana de banco
  −18,5 dBFS y **NOISE −18,9 contra ANALOG −18,5** (la brecha de 12 dB que describía el informe anterior ya
  no existe: los niveles se aplican al cargar), 191 pantallas × 22 paletas sin un hallazgo, y los 285
  dorados sin cambios. Todo en `docs/QG_CONSOLIDATION_REPORT.md`.
- **La release no se toca**: `refs/tags/v2.9.2` público apunta a `bdc1a0d` y su paquete (`e7727491…`) lleva
  el SPL fijado; el tag local (movido a este árbol) **no se empuja**. Este árbol es candidato a v2.9.3.

## 2026-11-13 — v2.9.3 (candidato local, sin tag): iconos propios, FX BUS, HOME y DYNAMICS

Este árbol es **v2.9.3**: la v2.9.2 se publicó desde `bdc1a0d` sin DYNAMICS ni la poda de tablas, así que
describir este contenido como "v2.9.2" habría dejado a `release/` sirviendo un paquete con el nombre de otra
cosa. Paquete candidato `felucca-v2.9.3.fwsc`, 610 959 B, sha `5fb621e1…`, build `df8210e`; sin tag y sin
release, por lo que dice `docs/QG_RELEASE_BLOCKERS.md`.

- **HOME > DYNAMICS** (décima pantalla de HOME, entre MOD y SYSTEM): la envolvente de la pista seleccionada
  dibujada de sus cuatro tiempos reales (ATK, DEC, SUS, REL) — ataque, caída, meseta y liberación con sus tres
  articulaciones marcadas —, con una cabeza lectora que la recorre mientras `hv_voices()` dice que la pista
  suena, y debajo una columna por pista con el pico que acaba de alcanzar (`trk[].peak`, el mismo número que
  dibujan MIXER y OVERVIEW) y una línea en el nivel al que está puesta. Su glifo propio (`qg_dyn`, U+E002, orden alfabético en la fuente) la
  añade a la iconografía y la tira de HOME pasa a **ocho celdas** (x0 137 → 124, paso 13).
- **Iconografía propia**: ocho glifos dibujados como SVG (`assets/icons-svg/`, rejilla de 24, sólo trazo) y
  convertidos a contornos de fuente por `tools/gen_icon_font.py`, que los cuelga de `web/fukiai_qg.ttf` en
  U+E000..E007 junto a la Fukiai original (que no se toca). La tabla de iconos pasa de 8 177 a **8 608 B**
  (+431) y la imagen de 580 404 a **581 448 B** (+1 044, código de UI incluido).
- **FX > BUSES**: la cadena usa el icono de cadena y el de bus (antes MIX/MOD, prestados), y los bloques PUNCH
  y MASTER sus propios glifos en vez de ACCENT/LEVEL; el suelo de cada bloque es un degradado de cuatro bandas,
  una columna de luz lo cruza mientras ese bus lleva señal, la fila de la cadena tiene su propio degradado y el
  punto que la recorre deja estela.
- **HOME**: una tira de siete iconos en la esquina del título dice en qué pantalla estás (el chip encendido
  respira con `ui.frame`), y las siete vistas se apoyan en un degradado de doce bandas tramadas. Los rótulos de
  esquina que sólo repetían lo mismo con palabras (LIVE, MATRIX, IN > OUT) se fueron; lo que decía algo (la
  pista, el contador de presets, la versión) sigue ahí, una fila más abajo.
- **Tablas de constantes**: medidas por símbolo en el ELF enlazado (`tools/table_sizes.py`), no estimadas
  por el tamaño del header. La cara M (monoespaciada de 15 px, avance fijo de 9 px) llevaba **cuatro fases
  horizontales**: 1 680 B de tabla y 6 380 B de datos para colocar sus glifos dentro de un cuarto de píxel. A
  dos fases su error de hueco medido es **0,044 px** (`text_spacing_test`), menos de lo que la cara S ya
  toleraba con cuatro. Y once celdas de icono nombraban cosas que la capa PERFORM se llevó por delante
  (`x_usb`, `x_hpf`, `x_tstop`, `x_freeze`): **760 B** que ninguna pantalla podía alcanzar.
  `font S 14 669 · font M 8 420 (antes 16 480) · font L 5 049 · icons 7 885 (antes 8 608) · keycaps 5 414 ·
  shapes 158 · palettes 352` — **41 947 B** de tablas en total (8 771 B menos que en v2.9.1), e imagen
  **573 664 B** (antes 581 448, **−7 784 B**), DYNAMICS incluida. El `.fwsc` sigue en 610 959 B: el resto es
  relleno y loader de tamaño fijo.
- **Pruebas**: `ui_render` completo — 191 pantallas x 22 paletas, **0 hallazgos de lint, 0 desalineaciones,
  0 píxeles GREY/MONO fuera de su gama** en 3 156 050 medidas, y los **285 renderizados dorados** de
  `regress` sin un solo cambio (0 sanidad, 0 voces/enrutado, 0 CPU fuera de presupuesto). El volcado de una
  pantalla descubrió tres desreferencias nulas en el propio informe del arnés (una caja sin rótulo dos veces
  y el `FILE*` del informe, que en el volcado de una sola pantalla aún no está abierto), corregidas ahí.

## 2026-11-13 — v2.9.1: auditoría de motores, seguridad y cierre

- **Corregido**: un proyecto que se importa con la ventana de voces de una pista a 0 hacía `% 0` en
  `voice_alloc` (y devolvía un puntero que el llamador desreferenciaba). Lo encontró `tests/fuzz_proj.c`
  bajo ASan/UBSan, con el seed 7; esa semilla queda en la suite como regresión. Cuesta 16 B de flash:
  imagen 580 404 B, 1 160 B de margen.
- **Añadido**: `tests/engine_param_test.c` — 216 mandos EDIT barridos de mínimo a máximo en tres presets
  por motor, comparados muestra a muestra: ninguno sin efecto. Su modo `--levels` mide el RMS de los 267
  presets; la familia NOISE queda 12 dB por debajo de ANALOG y ese es el desequilibrio que hay que arreglar.
- **Documentación**: fases 5 y 6 (`docs/QG_EVOLUTION_PHASE_5_ENGINES.md`, `docs/QG_PRESET_REMASTER.md`,
  `docs/QG_EVOLUTION_PHASE_6_SECURITY.md`), re-verificación del Hub y este cierre final.
- **Sin motores nuevos**: con el margen que hay (1 160 B tras recuperar 956 del código de pantallas), GRANULAR (buffer de 22 051 B frente a 15 392 libres), SAMPLE
  y VOCODER no caben. SID, ACID y POLY se resuelven con presets sobre ANALOG/PHASE/WTABLE.
- **README**: decía `v2.7.4.7` y 212 sonidos de fábrica; ahora dice `v2.9.1` y los 267 que cuenta la
  auditoría.
- **Niveles de la biblioteca**: 52 presets llevan ahora su `P_LEVEL` en el `x[]` que ya tenían (0 B de
  flash). La familia NOISE pasa de **−29,7 a −21,8 dBFS** de mediana y sus 22 presets dejan de caer 12 dB
  por debajo del resto; los 30 que estaban a ≥6 dB de su propio motor quedan en su familia. Techo: pico
  ≤ −2 dBFS y nivel ≤ 127, así que los sonidos de eventos espaciados (GEIGER CNT, RISER, SOLAR WIND) siguen
  bajos por diseño, no por nivel.
- **Slot de la aplicación**: `APP_SLOT` pasa de `0x8DFBC` a `0x8E7BC` (+2 048 B) usando el hueco que la
  imagen ya tenía y nadie direccionaba (2 774 B reales, no los 3 493 que decía la documentación). La
  imagen de flash y las particiones BTIF/USR no cambian. Margen de código: **3 208 B**.
- **Margen de XIP**: `tools/size_fns.py` pide ahora `noinline` además de `minsize` a las funciones del
  bucle principal, que hasta aquí se copiaban dentro de un único despachador (`draw_graph` medía 39 812 B).
  Imagen 581 360 → **580 404 B**: quedan **1 160 B** en vez de 204. El audio no cambia —la pasada excluye
  las 141 funciones que alcanza una interrupción— y `target_budget` da los mismos costes.

## 2026-11-13 — Banda galáctica en el visualizador

- `gw_viz_deep()` dibuja una banda galáctica de 24 columnas antes de los cuerpos: núcleo con bulbo, halo y polvo,
  en el tono de la página para que los datos medidos sigan liderando. Sin trigonometría por píxel, sin memoria.
  No se dibuja en calidad LOW.

## 2026-11-13 — FX > BUSES: distribución rehecha

- La pantalla tenía 318 hallazgos de lint por textos fuera de su celda: los títulos de los seis bloques se
  dibujaban por encima de su rectángulo interior y los nombres completos de la cadena se pisaban en ocho celdas
  de 28 px. Ahora la cadena es una fila de tres letras, cada bloque es un panel con título y dibujo dentro de su
  propio suelo, y el master queda siempre enmarcado como salida común. Cada efecto conserva su dibujo propio,
  animado sólo con señal real (`fx_vu`).
- Se revirtió el gráfico por motor de EDIT 1 (`19b5414`): el barrido exige 32 gráficos FM6 y esa página dejó de
  dibujarlos, así que la comprobación bajó a 0. `SEQ > TOOLS` de ese commit se conserva.

## 2026-11-13 — SEQ > TOOLS y un gráfico por motor

- **SEQ > TOOLS** (nueva página de SEQ, tras MUTATE): DUP, HALF, ROT <, ROT >, REV y CLEAR sobre el patrón de la
  pista elegida, con TIMES 1..8, punto de UNDO antes de cada edición y el patrón dibujado en la propia página.
  Sólo se reescriben las notas: tiempo, compuerta y probabilidad de cada paso se conservan.
- **EDIT 1** tiene ya gráfico: uno por motor, con los parámetros de ese motor (osciladores y filtro, operadores,
  pistas de batería, granos, cortes, onda del sample, morph de la tabla, modos de la cuerda y escalones de LOFI).

## 2026-11-12 — SCL en tres páginas y FX > BUSES

SCL se rehace como herramienta de tres páginas y FX gana la consola de ruteo. El detalle medido está en
`docs/QG_SCL_FX_REDESIGN.md`.

- **SCL 1/3 SCALE**: las doce notas en un círculo (tónica arriba, grados de la escala unidos en el color de la
  pista, notas de afuera tenues, un radio por grado) y debajo el teclado con la escala encendida y cada nota que
  suena marcada. Columna derecha: tonalidad, familia, cuántas notas, QUANT y el acorde que tocaría CHORD.
- **SCL 2/3 CHORD**: el acorde de la tecla con su símbolo, sus notas, el voicing y el strum en ms, los grados como
  pastillas (el que pisa el acorde en el acento) y las voces en cuatro carriles sobre la extensión del acorde, que
  se corren con STRUM. Con CHORD en OFF lo dice y no dibuja voces.
- **SCL 3/3 PROG**: las ocho progresiones como pastillas y el acorde de cada paso en esta tonalidad. Las viejas
  páginas SCL, VOICING, INTERVAL y KEYS desaparecen porque su contenido es ahora el círculo, el teclado y la página
  CHORD; no se perdió ningún parámetro. `SCL 2` (transposición global de las secuencias) pasa a GLO.
- **FX > BUSES**: consola de ruteo con KNOB 1 seleccionando TRACK/BUSES/DLY/REV/CHO/SLI/PUNCH/MASTER y KNOB 2..4
  editando los parámetros reales de ese eslabón (los mismos ids de las otras páginas). Seis bloques con dibujo
  propio y datos reales: repeticiones del delay, cola de la reverb, ondas del chorus, compuertas del slicer,
  transiente y duck del punch, ganancia/pico/clip del master. Sin DSP nuevo: son los tres buses globales, el
  slicer y el master que ya corren.
- **Pruebas**: `ui_render` 189 pantallas × 22 paletas con **0 hallazgos de lint** y 0 de alineación; `ui_test`
  verde con la navegación nueva; capturas actualizadas en `.shots/{scl,chord,prog,buses}.png`.

## 2026-11-12 — Remasterización de la biblioteca: 212 → 267 presets

Los sonidos de fábrica, rehechos uno a uno para que usen lo que el firmware ya tiene. El detalle medido
está en `docs/QG_PRESET_REMASTER.md`; aquí, lo que cambia al tocar el FM-1.

- **Movimiento en 210 de los 212 sonidos existentes** (antes, 40): LFO y matriz de modulación al filtro,
  a la amplitud, al tono y al envío de reverb, con `VEL → CUT` o `AT → VIB` según la familia. Los dos kits
  `legacy` de DRUM (ids 10/11) siguen byte a byte, para que un proyecto viejo cargue el mismo sonido.
- **Los envíos dejan de ser un wash común**: la reverb baja en bajos, percusión y leads y sube en pads,
  coros y campanas; el chorus se queda donde el sonido lo pide, el delay donde hay ataque que repetir y la
  saturación donde el motor tiene drive (`SOFT`, `TAPE`, `HEARTH`, `FLUX`). Nueve sonidos van sin envíos.
- **55 sonidos nuevos**, originales y repartidos por familia (leads, pads y drones, campanas y teclas,
  experimentales, bajos, percusión y una familia de rock electrónico / electrónica atmosférica de nombres
  propios, sin copiar presets ni muestras). Todos los motores reciben: ANALOG 56→68, FM6 29→37, PHASE
  25→32, LOFI 25→29, VOICE 20→25, WHEEL 18→22, NOISE 17→22, DRUM 12→18, WTABLE 10→14.
- **Niveles recalculados**: la dispersión de RMS entre presets pasa de 31,5 dB a 11,7 dB con el pico más
  alto en −2,8 dBFS (ningún preset recorta). 26 sonidos siguen por debajo de lo que el fader alcanza y se
  listan como pendiente.
- **Herramientas**: `tools/preset_audit.py` audita la biblioteca como datos (movimiento, envíos,
  categorías, parecidos) y `tests/preset_audit_test --dump` publica las tablas del firmware en JSON, así
  que la auditoría no puede desviarse del código. El navegador de presets gana el orden de LOFI para los
  cuatro sonidos nuevos y el generador de categorías filtra los kits `legacy` por id, no por posición.
- **Verificación**: host x86 con el motor real — 267 presets, 0 silenciosos, 0 duplicados, 0 fuera de
  rango; `preset_library_test` y `preset_categories_test` en verde con los conteos nuevos. Nadie ha
  escuchado estos sonidos y no hubo ninguna prueba en la unidad física.

## 2026-11-12 — v2.9.0: las cinco fases, en un solo paquete

La versión que este repositorio, su web, su emulador, su instalador y su release sirven ahora. Reagrupa
el trabajo de las cinco fases; cada una tiene su informe con la clase de cada medición.

- **Fase 1 — optimización medida** (`docs/QG_PHASE_1_OPTIMIZATION.md`): memoria, `.ram_text` y el coste
  de los caminos calientes, con las cifras de host y las que quedan fuera de alcance.
- **Fase 2 — la interfaz** (`docs/QG_PHASE_2_UI.md`): el rediseño de HOME y sus vistas, medido en el
  render de host y en el emulador compilado desde este árbol.
- **Fase 3 — motores** (`docs/QG_PHASE_3_ENGINES.md`): WAVETABLE entra como motor propio y la auditoría
  de presets deja los nueve vivos.
- **Fase 4 — herramientas musicales** (`docs/QG_PHASE_4_MUSICAL_TOOLS.md`): FLANGER como tercer
  `CHORUS > TYPE`, **HOME 9/9** con la pantalla ROUTING y una sola balística de medidores.
- **Fase 5 — The Firmware Hub** (`docs/FM1_HUB_READINESS.md`): la tarjeta `fmhub.json` validada por
  `tools/check_fmhub.py`, la identidad leída del paquete publicado con
  `tools/check_package_identity.py`, el job `verify` de atestación en cada tag y la procedencia de cada
  audio y binario en `docs/FM1_HUB_AUDIO_PROVENANCE.md`.
- **La sincronización de esta entrega**: el portal deja de nombrar v2.7.2 y toma la versión del
  despliegue (v2.9.0), dice **nueve** motores con WAVETABLE en la lista, **22** paletas y ya no
  promete un conteo de presets que no puede verificar; la advertencia de instalación explica que los
  manifiestos van sin firma Ed25519 desde 2.8.0 (integridad y trazabilidad, no autenticidad); y las
  capturas —incluidas ROUTING, la matriz de modulación y el navegador de presets— se regeneraron desde
  el código de esta versión con `tools/generate_screenshots.py` y `tools/fw_shots.py`.

## 2026-11-12 — FASE 5/5: preparación para The Firmware Hub

- **La tarjeta del Hub, verificable**. `fmhub.json` en la raíz con un `what` de **274 caracteres** que
  describe lo que el firmware **hace** (nueve motores, secuenciador de 64 pasos con probabilidad y
  ratchets, song mode, escalas y acordes, matriz de mod y tres buses de FX, emulador y editor) en lugar del
  texto anterior que prometía «a sampler with mic input»: SAMPLE, GRAIN y SLICE están **retirados**
  (`ENG_GONE`) y no hay entrada de micrófono. Color **`#35CFFF`** —único entre los 52 listados con color del
  catálogo— con **11,5:1** sobre negro y tinta oscura `#101014` a **10,4:1**; imagen 240 × 240 PNG de
  14 630 B. `tools/check_fmhub.py` valida el archivo contra las reglas publicadas (campos aceptados,
  longitudes, HTML, kinds, enlaces por HTTP, cabecera y tamaño de la imagen, contraste) y ahora corre en CI.
- **La identidad, leída del paquete publicado**. `tools/check_package_identity.py` saca `FM-1_965` de los
  bytes del asset de `v2.8.4` (610 959 B, sha256 `37f7276e…`), encuentra el loader `ota-FM-1_965` y
  **rechaza `FM-1_015`**; `FM-1_965` sigue **libre** en el catálogo del Hub (198 listados, 2026-11-12).
- **Atestación verificada, no supuesta**. `.github/workflows/release.yml` gana un job `verify` que, en cada
  tag `v*`, descarga el asset del release y corre `gh attestation verify` contra el workflow compartido
  antes de dar el tag por bueno; nunca reconstruye ni reemplaza un paquete ya atestado. El pin se documentó:
  `@85a003ab…` **es** el tag `v1` revisado (`git ls-remote`, 2026-11-12). El job no pudo ejecutarse acá
  (§ limitaciones de `docs/FM1_HUB_READINESS.md`).
- **Procedencia completa, con una prueba de reproducibilidad**: `web/banks/gw1-keys.syx` (4 104 B, 32 voces
  con las 11 claves FM6 propias) **se regenera byte por byte** con `tools/gen_fm6_patches.py --syx`
  (`7c39bf4b…`), y ahora CI lo comprueba con `cmp`. No hay ningún `.wav`, `.mp3`, `.ogg` ni `.flac`
  distribuido: todo el sonido es síntesis en tiempo de ejecución. `docs/FM1_HUB_AUDIO_PROVENANCE.md` lista
  cada binario publicado con autor, licencia y uso.
- **Lo que queda pendiente, dicho como pendiente**: las cinco pruebas que exigen una unidad física
  (instalación desde V15, desde otro firmware, salida hacia otro, identidad USB e interrupción/reanudación)
  **no se hicieron** —el FM-1 del dueño no se escribe— y `gh attestation verify` no pudo correr en este
  entorno. `docs/FM1_HUB_READINESS.md` da el estado de cada requisito con su clase de evidencia y el mensaje
  en inglés para Sven, incluida la aclaración de que *Pocket GammaWave* (`FM-1_981`, otro autor) es el
  proyecto upstream y no éste.

## 2026-11-12 — FASE 4/5: FX, ruteo, secuenciador y herramientas musicales

- **FLANGER, el tercer `CHORUS > TYPE`**. Usa la línea del chorus con un retardo corto barrido (48..302
  muestras, 1,1..6,8 ms según DEPTH) y **realimentación 0,6 dentro de la línea**: los notches del peine en
  múltiplos impares de `1/(2d)` son el «jet». **0 B de RAM nueva** y **0 B de flash**: el tap y el LFO ya
  estaban, y cuesta **dos productos por muestra**, menos que los tres taps del ensemble. Medido en host x86:
  peine de **12,0 dB**, pico **7 033** (Q15) —estable—, los notches se mueven con el barrido. En el proyecto
  el `TYPE` pasa a **dos bits** (bits 1..2 del bloque `DT`): un proyecto viejo se lee igual y un firmware
  anterior lee `FLANGER` como `PHASER`, nunca como silencio.
- **Una sola balística de medidores**. La página FX traía su propia caída (1 px por frame) mientras
  `fx_meter_step()` —ataque inmediato, un cuarto por frame, 30 frames de retención— sólo existía para su
  prueba. Ahora la usa también la página FX: **una** ley para todas las barras y **un** origen por medidor
  (`fx_vu[0..5]` para los buses, `fx_vu[6..7]` y `trk[].peak` como en HOME). No quedó medidor duplicado.
- **ROUTING, la novena pantalla de HOME** (`HV_ROUT`, el pie cuenta `/9` en vez de llevar `/8` escrito a mano).
  Dibuja el recorrido real —PARTE → INSERT (`DIST`) → los tres envíos → CHORUS / DELAY / REVERB → MASTER (EQ,
  W, LIM) → OUT— con las cifras que el DSP lee: cada bus muestra **lo que se le envió** (`fx_vu[0..2]`) y **lo
  que devolvió** (`fx_vu[3..5]`), y el pie, los dos picos del master. Una barra oscura es que no pasó audio.
- **El ruteo, medido**: `tests/fx_probe.c` alimenta sólo el bus de chorus y llama a `fx_buses()` 40 bloques:
  `in 11999 / 0 / 0 → out 9060 / 0 / 0`. El envío alimentado levanta su medidor y ningún otro, su bus devuelve
  audio, y un bus sin envíos no devuelve nada.
- **Evaluación de los otros quince efectos** (`docs/QG_PHASE_4_MUSICAL_TOOLS.md` §2.3): once ya existían con
  otro nombre (tremolo y vibrato son destinos del LFO; compresor y limitador, el master; shimmer, `G_RTYPE`;
  tape y overdrive, `DIST > TYPE`), uno se agregó, y los que piden memoria que acá no se puede medir
  (frequency shifter, reverse delay) quedaron **descartados con su razón**, no a medias.
- **Auditoría del secuenciador** (§4): probabilidad, condiciones `WHEN A/B`, ratchets, `MUTATE`, automatización
  y `AUTO LIST`, escenas por fila de canción, macros y punch-in ya están, cada uno con la prueba que lo cubre.
  Las tres carencias reales —fills a demanda, recall de escena desde SEQ, punch-in a mano desde las teclas
  negras— quedan planteadas con su costo: las tres cambian el mapeo de teclas o el formato del proyecto.
- **Herramientas musicales** (§5): escalas, `CHORD`, `VOICING`, `INTERVAL`, `PROG` y `KEYS` ya cubren lo que
  el pedido enumera; la encuesta de JIANT, X0X, Optimist, Melodee y Jangada (todas GPL-3.0) deja dicho qué se
  tomó, qué ya estaba y qué se descartó (los 70 scales, el círculo de quintas, el sampler).
- **Pruebas**: `tests/fx_probe.c` suma el peine del flanger y el ruteo; `tests/fx_bench.c` suma una tercera
  escena con el chorus en FLANGER para medirlo contra el ensemble en la misma corrida; `tests/ui_render.c`
  renderiza la pantalla nueva (**192 pantallas × 22 paletas, 0 hallazgos de lint, 0 desviaciones en 3 164 009
  medidas de alineación**); `tests/ui_test.c` recorre las nueve pantallas de HOME y comprueba que ROUTING no
  toca página, parámetro ni sonido. Sin placa ni toolchain AC791N: nada de esto es `[TARGET]`.

## 2026-11-12 — FASE 3/5: motores, auditoría y un motor nuevo (WTABLE)

- **WTABLE, el motor 14**: un oscilador de tabla de una sola ciclo con morphing. Ocho fotogramas (SINE,
  TRIANGLE, SAW, SQUARE, PULSE 25 %, HOLLOW, METAL, VOCAL) de 256 puntos, normalizados al mismo pico y con
  fase 0 para que el morph no salte ni meta continua, guardados en tres límites de banda (64 / 16 / 5
  armónicos) que el motor elige por la nota: nunca lee por encima de ~11 kHz, así que el teclado entero queda
  sin alias. **MORPH** es el primer destino de motor de la matriz (`MD_E1`), **HARM** inclina el espectro de
  forma continua (un polo a ~1 kHz, ±0,75), y hay **SUB** (octava abajo), **UNI 2 VOICES** con **DETUNE**,
  filtro SVF de 2 polos con seguimiento de tecla y **DRIVE**.
- **Un defecto corregido antes de adoptarlo**: el saturador con SUB dejaba hasta **41,5 % de la escala de una
  voz en continua** (10 voces sumaban). Ahora la voz termina en un bloqueador de continua a ~5 Hz con el
  producto redondeado (con el truncado el bucle se sesgaba ±1 LSB acumulado): **0,00 %** en los diez presets.
- **Presupuesto**: tabla de **12 288 B de flash** `const` (XIP) y **0 B de RAM nueva** — el estado de cada voz
  cabe en `voice_t` (`ph[0..2]`, `s[0..4]`, `keep = 0x1f`). Polifonía `NVOICE` (12), dos lecturas por muestra
  con UNI. En host: **11–12 ns** por muestra y voz aislado, **62,7–116,6 ns** con la ruta de voz completa, y
  un peor caso de nivel de **1,94 × `VOICE_FS`** por voz (ANALOG mide 2,07, PHASE 1,47 en la misma prueba).
- **10 presets**: GLASS PAD, MORPH LEAD, SINE KEY, VOCAL AAH, METAL BELL, HOLLOW WOOD, BRASS SWELL, SUB BASS,
  PULSE ORGAN, QUANTUM MASS. Los knobs de HOME del motor son MORPH, HARM, CUT y UNI.
- **Auditoría de los motores que ya estaban** (`docs/QG_PHASE_3_ENGINES.md` §1): los 8 vivos con sus
  parámetros reales, su polifonía y sus límites. **No se cambió ninguno**: tocar sus parámetros cambia el
  sonido de 200 presets y sus renders dorados, y eso necesita su propia pasada medida. STRING, ADD y MATH
  **no existen** en el árbol (lo aditivo es WHEEL, lo vocal/cuerda es VOICE, lo «math» es FM6); el pedido
  pedía además exactamente un motor nuevo, y ése es WTABLE.
- **Pruebas**: `tests/wavetable_test.c` (fotogramas y morph medidos por espectro, HARM, SUB, UNI batido,
  aliasing en cada octava de C1 a C8, continua, clipping, release, determinismo, coste) y **230 renders
  dorados: 0 cambiados y 10 nuevos**, o sea que ningún sonido anterior se movió. Auditoría de presets: 212,
  0 silenciosos, 0 duplicados. El motor se llamó `WAVETABLE` hasta que el lint de maquetación demostró que el
  campo lo cortaba: es **`WTABLE`**, seis caracteres, y el barrido vuelve a 0 hallazgos.
- **Sin medida de placa**: no hay toolchain AC791N en este entorno (sin `.bss` enlazado, sin flash real, sin
  desensamblado del bucle) ni unidad física (sin escucha). Lo que hay es host x86 y está dicho así en el
  documento de la fase.

## 2026-11-12 — FASE 2/5: la interfaz, viva (medida)

- **Arranque**: el splash no era lento, estaba congelado. Su reloj era `fm1_ms`, que sólo empieza con TIMER5, así que toda la inicialización (panel, motores, audio, USB, LEDs, segundo núcleo, autosave, reel) corría con el contador en 0 y el splash se quedaba en su primer fotograma: de ahí el segundo o dos de pantalla quieta antes de la animación. Ahora se temporiza con TIMER4 desde el primer píxel y repinta al terminar cada etapa (`splash_step`), y lo que la inicialización gasta ya no se vuelve a pagar en el bucle. Además `SPL_HOLD` 2800 → 2000: otro 0,8 s menos, con la firma y el fundido final intactos (4,5 s desde la primera luz).
- **HOME, la estrella**: cuarto disco de aliento, corona de ocho rayos que gira y cambia de longitud con la energía, llamaradas cada ~7 s, y la estrella como blanco: al tocarla, un pulso expansivo enciende y agranda los cinco mundos.
- **HOME, los mundos**: cinco tonos distintos en vez de cinco ámbar (rocoso cálido, gaseoso dorado, anillado rosa, oceánico violeta, congelado azul hielo), un clima propio por cuerpo (polvo, ojo de tormenta, destello, brillo del anillo, escarcha) a su propio período, y una hairline del fondo en `r + 2` para que dos órbitas que se cruzan no fundan dos cuerpos en una silueta.
- **HOME, profundidad**: dos mundos que no orbitan — giran donde están, con terminador propio y mezclados lejos del fondo — y las órbitas pasan por delante de uno.
- **Coste**: HOME 180,0 → 174,9 µs por fotograma en `ui_render` (host) y su visualizador 99,7 → 93,5 µs, con más dibujo que antes.
- **Pieles**: NEBULA, PLASMA, ORGANIC, MINIMAL y RETRO se suman a THERMAL, cada una con su propia rampa térmica además de la paleta (curvas, medidores, estrella y planetas incluidos), elegibles en MENU > COLOR y guardadas como el resto. Las seis pasan los controles de contraste del generador; ninguna toca el audio.
- **Pie de página**: HOME tiene ocho páginas y el pie las llamaba todas «HOME»; ahora dice «HOME 3/8», como el resto de las familias.
- **Dos defectos corregidos de paso**: `firmware/src/ui_palettes.h` (versionado) tapaba a la cabecera generada, así que una paleta nueva no llegaba al build; y la reacción del cuerpo oceánico nacía dentro del cuerpo, donde la nueva hairline la borraba (5 px de respuesta medidos → 62 px).
- **Verificación**: 191 pantallas × 22 paletas en el barrido de render, 0 hallazgos de lint, 0 desalineaciones en 2 863 797 medidas; las cinco reacciones al toque siguen siendo cinco y reiniciables; emulador web compilado desde el árbol (build `fb4e0f867ba6`) con arranque, splash, toque en la estrella y «HOME 2/8» comprobados a mano. Nada de esto es una medida de placa: sin toolchain AC791N ni unidad no hay `[TARGET]`.
- **Pendiente declarado** (`docs/QG_PHASE_2_UI.md` §7): FX ROUTING y SEQUENCER TOOLS no se implementaron; MOD MATRIX, PERFORMANCE y PRESET BROWSER ya existían y se documentan en su sitio.

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

