# QUANTUM GAMMAWAVE 3.0 — AUDITORÍA Y ESTADO DE LAS FASES

Rama: `freebuff/222b35789f0959dc0a20e352`  
Base: `v2.9.7` (`e5a8db9`), árbol de release: `v3.0.0`

Fecha: 2026-11-13

Estado de 3.0: **VALIDADA EN HOST, TARGET AC791N Y WASM; RELEASE v3.0.0 EN CURSO**

Este documento registra qué fases cubre el árbol real, qué se midió en host, target y WebAssembly,
qué se corrigió en esta pasada y qué sigue fuera de alcance por hardware físico o por la compuerta
de publicación del repositorio.

---

## 0. Resultado ejecutivo

El árbol **no necesita una reescritura para 3.0**: ya tiene nueve motores, 273 presets, 84 páginas,
secuenciador completo, morphing, macros, matriz de modulación, escenas, undo/redo, herramientas
euclidianas y un ecosistema de emulador/editor/instalador. La mayor parte de las funciones del plan
3.0 **ya existe** y está probada en host.

Lo que faltaba y sí era demostrable en esta pasada era un riesgo de release: `tag-and-publish.yml`
publicaba automáticamente cualquier versión que apareciera en `main`, incluida una 3.0. Se añadió
una compuerta explícita para 3.x y se limitó `release.yml` a tags. La versión `v3.0.0` se asigna en
este árbol después de cerrar CPU, memoria, host, target y WASM; el paquete y el tag se crean desde
el commit que contiene estos artefactos, nunca desde un árbol sucio.

| Clase | Estado |
|---|---|
| IMPLEMENTED | compuerta 3.x, `fmhub.json` actualizado, familia WTABLE morph, este informe |
| HOST TESTED | compuerta, YAML, Hub, coherencia, regresión, UI, presets, motores |
| AC791N TARGET COMPILED | **YES**: build real con toolchain y SDK fijados; presupuestos medidos abajo |
| WASM TESTED | **YES**: Emscripten 6.0.11, paridad bit a bit con el build nativo |
| BROWSER TESTED | **NOT VERIFIED** (sin navegador en el runner; el motor WASM sí se probó en Node) |
| HARDWARE VERIFIED | **NOT VERIFIED** (no hay FM-1 físico) |
| 3.0 publicada | **EN CURSO**: paquete, tag y release desde el árbol limpio |

---

## 1. Fase 1 — Optimización global

### Lo que ya estaba medido y sigue vigente

- El mix no divide ni recorta los envíos de una pista seca; sólo lo hace cuando hay algo que enviar.
- El índice de la línea de chorus se enmascara una vez por muestra.
- El tick de TIMER5 no recorre columnas sin cambio.
- `size_fns.py` mantiene el código del bucle principal fuera del despachador único.
- Las tablas de const están medidas por símbolo en el ELF, no estimadas por el header.

### Medición de esta pasada

| Prueba | Resultado |
|---|---|
| `build/host/regress tests/golden.txt tests/cpu_baseline.txt` | **PASS**: 291 goldens (6 nuevos), 0 cambiados de los 285 previos, 0 fallos de salud/voces/routing, 0 over budget |
| Coste de UI | `ui_render`: 189 pantallas × 22 paletas, 0 lint, 3 390 046 alineaciones, 0 desviaciones |
| Memoria host | `budget_report.py`: `voice_t` 88 B; seis pistas 6 336 B con 12 voces, 5 280 B con 10 |
| Flash/RAM target | **PASS**: imagen 574 588 B, RAM 87 956/98 304 B, pool 305 096 B, `.ram_text` 3 792 B |
| CPU estática target | **PASS**: `phase_render` 84/84, `phase_render_noise` 103/103, `phase_harm` 23/23, `wheel_block` 32/86, `noise_render` 219/201 (+9 %, dentro de +10 %), `fm6_render` 292/322 |

El build AC791N destapó además un error que el host no veía: `params.c` copiaba dos
`param_desc_t` en un inicializador estático y clang 4.0.1 lo rechaza; ahora usa punteros a los
descriptores. No se cambió DSP ni el sonido de los presets existentes. Un cambio de sonido sin escucha ni target violaría las reglas del
propio plan. Las filas de CPU ns que el `regress` marca por encima de +40 % son ruido de host compartido y se
informan como nota, no como regresión.

Los cuatro overruns de CPU estática se cerraron **sin tocar las líneas históricas** de
`tests/target_budget.txt`: `wheel_block` desenrolla sus dos rotores y hace la fuga en una pasada con
recíproco exacto de 1270; `fm6_render` simplifica la condición de etapa de la envolvente; `noise_render`
cambia `isqrt32` por una tabla exacta de 128 entradas; y `phase_render` separa el camino NOISE
(`phase_render_noise`, 103) y HARM (`phase_harm`, 23) para que el camino normal vuelva a 84. Las
entradas nuevas están medidas y presupuestadas; el camino NOISE+HARM (126) también baja frente a los
145 anteriores.

---

## 2. Fase 2 — Formas de crear y editar sonidos

Ya implementado y probado:

| Función | Evidencia |
|---|---|
| Morphing | `P_MORPH` en siete motores; `morph_audible_test` 21 casos, 0 fallos; `morph_six_test` |
| Macros | `LAYER_MACRO`, `macro_v[]`, `P_M1SRC…P_M4AMT` |
| Matriz de modulación | 4 slots SRC/DST/AMT, `mod_test` |
| Escenas | `scene_capture`, `song_idx.scene` |
| Undo/Redo | `undo_mark` / `undo_do`, SAVE sostenido y GLO G4 |
| Randomización | dados de sonido, secuencia, LFO y FX con CANCEL |
| Edición contextual | páginas EDIT por motor, FM6 OP/ENV, WTABLE, DRUM |
| Copiar/pegar y herramientas de SEQ | `ui_seqtools.c`, `ui_tools.c`, TOOLS |
| Envolventes/LFO | ADSR, LOOP, ENV VEL, LFO SYNC, SLEW, INVERT, PHASE, FADE |

**No implementado en esta pasada:** un A/B de sonido separado de las escenas. El árbol ya tiene
escenas y slots; añadir un tercer mecanismo de snapshot exigiría RAM y un diseño de interacción
que no se puede validar sin unidad. Queda como B, no como función terminada.

---

## 3. Fase 3 — Los nueve motores

Inventario real: ANALOG, FM6, PHASE, WHEEL, VOICE/FORMANT, LOFI, DRUM-X, NOISE y WTABLE.

- `engine_param_test`: **216 knobs × 3 presets**, 0 controles sin efecto audible.
- `phase2_test`: banco CHARACTER por motor, LFO SHAPE, ENV VEL, páginas y round trip.
- `morph_audible_test`: 21 casos, todos audibles.
- `preset_audit_test`: 273 presets, 0 silencios, 0 duplicados, 0 fuera de rango.

No se añadieron motores nuevos: GRANULAR, SAMPLE y VOCODER no caben en la RAM/flash que el árbol
presupuesta, y no hay target para medir un motor nuevo. WTABLE existe con 20 presets; la familia
nueva mueve MORPH por LFO o POSMOD en lugar de dejarlo fijo.

---

## 4. Fase 4 — ARP, SEQ, SCALE y CHORD

Ya implementado:

- ARP: modos, octavas, gate, swing, hold, probabilidad, orden, repeat, chord, transposición.
- SEQ: 64 pasos, piano roll, drum grid, ratchets, condiciones, locks, automatización, humanización,
  microtiming, swing, longitudes, C.LEN, MUTATE, TOOLS, grabación.
- SCALE/CHORD: 16 escalas, 12 raíces, 11 modos, 4 voicings, progresiones, teclado, grados.
- `chord_scale_test`, `ratchet_test`, `motion_test`, `project_test`: PASS.

El defecto real de Fase 3 anterior —etiquetas GATE/SWING/PROB solapadas con la barra— está
corregido en `4d3b0ad` y verificado con UI render.

---

## 5. Fase 5 — FX y procesamiento

- PHASER ya está medido: peine de **23,6 dB** dentro de 200 Hz–6 kHz; sin click al mover DEPTH;
  cadena acotada. No es un efecto débil.
- FLANGER: peine de 12,0 dB, realimentación estable.
- Reverb, delay, chorus, slicer, EQ, saturación, M-WIDE y compresor/limitador existen y tienen
  pruebas (`fx_probe`, `dly_echo_test`, `reverb_test`, `fx_meter_test`).
- No se añadieron efectos que pidan buffers nuevos: frequency shifter, reverse delay y granuladores
  no caben sin una medición de RAM target.

---

## 6. Fase 6 — Renovación de UI

- 189 pantallas × 22 paletas: **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY/MONO fuera de gama.
- 3 390 046 medidas de alineación: **0 desviaciones**.
- El suelo Solar Heat ya está en las páginas de sección desde v2.9.5.
- HOME tiene cinco pantallas y el pie cuenta `HOME n/5`.
- Todas las pantallas multipágina llevan `PAGE n/m`.

No se hizo un rediseño visual total 3.0: no hay navegador ni unidad para validar el resultado, y
el propio plan prohíbe declarar terminada una fase sin esa validación. El rediseño es B/D, no A.

---

## 7. Fase 7 — Presets

`tools/preset_audit.py` sobre el dump real:

- 273 presets en 9 motores.
- 271/273 con movimiento.
- 0 sin envíos, 0 silencios, 0 duplicados, 0 problemas.
- Distribución: PADS 62, KEYS 56, TEXTURES 49, DRUMS 24, BASS 19, ORGANS 18, BELLS 15, LEADS 12,
  PLUCKS 11, ARPS 7.
- WTABLE pasa de 14 a 20 con seis sonidos que recorren la tabla: MAREJADA, PRISMA, CORAL GLASS,
  VOCAL TIDE, ARCO SOLAR y MORPH SUB. Sus RMS medidos están entre −16,7 y −17,2 dBFS, dentro de
  ±3 dB de la mediana; no hay nombres ni parámetros repetidos.
- FM6 con DIST en 3/37 sigue siendo el otro hueco de familia; ampliarlo exige escucha.

---

## 8. Fase 8 — Emulador, WASM, Web Studio e instalador

El emulador se reconstruyó en esta pasada con **Emscripten 6.0.11**: `web/emu/build.sh` produce
`build/emu/felucca.wasm` desde las mismas fuentes que el paquete y `emu_test.mjs` comprueba arranque,
teclas, MIDI, pantalla, LEDs, PLAY, guardado entre instancias y **paridad bit a bit** con el build
nativo. El registro del WASM nombra versión, commit, SHA-256 y tamaño; se regenera después del commit
de fuentes para que el commit del registro sea el del árbol limpio que produjo el binario.

`python3 tools/coherence_check.py` se ejecuta al final, cuando el paquete `v3.0.0`, su manifiesto, el
registro de build, `site/firmware/versions.json`, `release/emu` y `SHA256SUMS` existen y coinciden.
No se afirma prueba de navegador: el runner no tiene navegador; sí se probó el motor WebAssembly en
Node y su paridad con el build nativo.

---

## 9. Fase 9 — Seguridad de instalación y chips

- Chips aceptados: P25Q80H `0x856014` y UCUN UC25HQ80 `0xB36014`, en app y loader.
- Chip desconocido: el loader devuelve `-15` sin borrar, programar ni tocar registros.
- Backups M-Flasher: `O_EXCL`, `fsync`, `sync_dir`, sin sobrescritura.
- M-Flasher más reciente consultado: **0.11.0** (GitHub API, 2026-11-10). No hay 0.11.1+.
- `mflasher_test`, `flash_read_test` (16), `gate_test`, `flash_evidence_test` (20): PASS.
- `ldr_test`: PASS.
- `ldr_test_asan`: no terminó en la ventana de 180 s; **no se declara PASS**.

Sigue **NOT VERIFIED**: lectura JEDEC real, escritura/borrado físico, recuperación, interrupción
real, backup del propietario. No se escribió ninguna unidad.

---

## 10. Fase 10 — FM-1 Firmware Hub

Estado comprobado hoy:

| Requisito | Estado | Evidencia |
|---|---|---|
| Música, seis pistas, nueve motores | GO | código y presets |
| Fuente pública GPL-3.0 | GO | `LICENSE`, `LICENSING.md` |
| `.fwsc` en un release | GO | `v2.9.7`, `release/felucca-v2.9.7.fwsc` |
| Identidad `FM-1_965` | GO | libre en catálogo, 202 listings, 2026-11-13 18:01 UTC |
| Loader `ota-FM-1_965` | GO | bytes del paquete |
| `fmhub.json` válido | GO | `tools/check_fmhub.py`, 0 fallos |
| Imagen cuadrada ≤300 KB | GO | `docs/card.png`, 512×512, 20 860 B |
| Built from source | GO CONDITIONAL | el badge necesita el próximo tag |
| Pruebas físicas de instalación/salida/USB/interrupción | **PENDING** | no hay FM-1 |

### Sobre el campo `sounds`

El Hub **no acepta** `sounds` en `fmhub.json`: un campo desconocido descarta el archivo entero.
El esquema publicado acepta `what`, `author`, `kinds`, `play`, `editor`, `cheat`, `image` y `color`
(validados por el repo contra la KB y ejemplos vivos). El conteo de sonidos va dentro de `what`;
por eso el texto ahora dice **“nine engines and 273 sounds”** y no se agregó un campo inválido.

No se solicitó inclusión ni se declaró listado.

---

## 11. Fase 11 — 3.0

`FELUCCA_VERSION` pasa a `v3.0.0` **después** de cerrar los cuatro overruns, compilar el target y
pasar host + WASM. El paquete se construye desde un árbol limpio; su manifiesto, el registro de build,
el sitio, el emulador y `SHA256SUMS` se derivan de ese mismo commit. La release sigue sujeta a la
compuerta de repositorio: sin `QG_RELEASE_APPROVED=true` el workflow no crea tag ni release. El
tag y el release se crean una sola vez y nunca se mueven.

Se añadió la protección para que un despiste no lo haga solo:

- `tools/release_gate.py`: bloquea cualquier versión `>= 3.0.0` sin `QG_RELEASE_APPROVED=true`.
- `tests/release_gate_test.py`: 8 casos.
- `tag-and-publish.yml`: llama la compuerta antes de crear tag o release.
- `release.yml`: sólo tags `v*`; job `gate` antes del build compartido.

La variable es de repositorio, no una constante del árbol. Sin ella, 3.0 no se publica.

---

## 12. Matriz final de esta pasada

| Elemento | Implemented | Host tested | Target | WASM | Browser | Hardware |
|---|---|---|---|---|---|---|
| Compuerta 3.x | sí | sí | n/a | n/a | n/a | n/a |
| `fmhub.json` 273 sounds | sí | sí (validator) | n/a | n/a | n/a | n/a |
| Regresión DSP | sí, optimizada | sí, 291 goldens, 0 cambios | compilada; CPU estática dentro de presupuesto | paridad nativa | NOT VERIFIED | NOT VERIFIED |
| UI | no cambiada | sí, 189 pantallas | n/a | n/a | NOT VERIFIED | NOT VERIFIED |
| Presets | familia WTABLE nueva | sí, 273, 0 problemas | tabla incluida en el paquete | sí, emulador | NOT VERIFIED | NOT VERIFIED |
| Seguridad de flash | ya corregida | sí | NOT VERIFIED | n/a | n/a | NOT VERIFIED |
| Release 3.0 | sí | sí | sí | sí | NOT VERIFIED | **NOT VERIFIED** |

---

## 13. Pruebas ejecutadas en esta pasada

| Comando | Resultado |
|---|---|
| `python3 tests/release_gate_test.py` | PASS, 8 casos |
| `python3 tools/check_fmhub.py --offline` | PASS, todos los campos |
| `python3 tools/coherence_check.py` | PASS |
| `python3 tools/check_identity.py` | `FM-1_965` FREE, 202 listings |
| `python3 tests/mflasher_test.py` | PASS (dump real omitido) |
| `python3 tests/flash_read_test.py` | PASS, 16 checks |
| `python3 tests/gate_test.py` | PASS |
| `python3 tests/flash_evidence_test.py` | PASS, 20 checks |
| `python3 tests/install_test.py` | PASS, 2 skips por paquetes ausentes |
| `build/host/ldr_test …` | PASS |
| `build/host/regress …` | PASS, 291 goldens (6 nuevos), 0 cambios en los 285 previos |
| `build/host/ui_test` | PASS |
| `build/host/ui_render …` | PASS, 0 lint, 0 desalineaciones |
| `build/host/preset_audit_test` | PASS, 273, 0 problemas |
| `./build.sh` con `JIELI_TOOLCHAIN` y `AC79_SDK` | PASS, `felucca.fwsc` 610 959 B |
| `tools/ram_report.py` + `tests/memory_map_test.py` | PASS, RAM/pool/RAMTEXT/NOINIT con holgura |
| `tests/target_budget.py build/felucca.dis tests/target_budget.txt` | **PASS**, 0 funciones sobre +10 % |
| `build/host/engine_param_test` | PASS, 216 knobs |
| `build/host/morph_audible_test` | PASS, 21 casos |
| `build/host/phase2_test`, `project_test`, `ratchet_test`, `chord_scale_test` | PASS |

La suite host completa se ejecutó en una sola corrida con `emcc` y terminó en
`ALL HOST TESTS PASSED`, incluyendo ASan/UBSan, fuzzing, WASM y paridad con el build nativo.
`ldr_test` corre en la suite; su variante ASan aislada no se repitió. No se ejecutó navegador ni
hardware: esas dos clases siguen **NOT VERIFIED**.

---

## 14. Bloqueos y siguiente paso

1. **Hardware**: sin FM-1 no se pueden hacer las pruebas físicas que el Hub exige; la release no las
   declara. No se escribió ninguna unidad.
2. **Navegador**: el motor WASM se probó en Node; falta la pasada en un navegador real con AudioWorklet.
3. **Backup `144531`**: falta el `.bin` para verificar tamaño y SHA.
4. **FM6/DIST**: sigue necesitando escucha, no sólo conteo.

La compuerta `QG_RELEASE_APPROVED` es una variable de repositorio, no una constante del árbol. Si no
está puesta, el workflow rechaza la 3.0 y no se crea tag: es el comportamiento correcto, no un fallo
del build.
