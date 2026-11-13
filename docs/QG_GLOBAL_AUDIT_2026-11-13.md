# QUANTUM GAMMAWAVE — AUDITORÍA GLOBAL 2026-11-13

Rama: `freebuff/222b35789f0959dc0a20e352`  
HEAD auditado: `a7fc3ac`  
Fecha: 2026-11-13  
Alcance: firmware, DSP, motores, UI, secuenciador, presets, herramientas, instalador y documentación.

---

## 0. Veredicto en una página

El árbol está **sano en host** y no contiene los huecos funcionales que el brief supone. La mayor parte de
las funciones pedidas ya existe y está conectada al DSP: LFO sincronizado al tempo, morphing, cuatro macros,
matriz de modulación de cuatro slots, escenas, undo, herramientas euclidianas, ratchet, humanización,
probabilidad, acordes, inversiones y voicings. Añadir una segunda implementación de cualquiera de ellas
habría duplicado código y habría violado la regla 3 del propio brief.

Lo que sí quedaba por corregir era verificable y ya está corregido en esta rama:

| Corrección | Commit | Estado |
|---|---|---|
| Backups M-Flasher inmutables y chip desconocido sin escrituras | `a7fc3ac` | IMPLEMENTED · HOST TESTED |
| Etiquetas GATE / SWING / PROB del ARP separadas de sus medidores | `4d3b0ad` | IMPLEMENTED · HOST TESTED · UI RENDER |
| Ficha del Hub y hashes de `v2.9.7` coherentes | `e635b86` | IMPLEMENTED · HOST TESTED |

No se añadieron motores, voces, buffers, tablas ni efectos nuevos. No se cambió ningún sonido de preset.
No se tocó bootloader, `USB_KEY`, recovery, zonas protegidas ni procedimientos de escritura.
No se instaló ni flasheó nada.

**Etiquetas usadas:** IMPLEMENTED, HOST TESTED, UI RENDER, AC791N TARGET COMPILED, WASM TESTED,
BROWSER TESTED, HARDWARE VERIFIED, NOT VERIFIED.

---

## 1. Método y límites de esta pasada

1. Se leyó el árbol real antes de modificar: `firmware/src`, `firmware/hal`, `tests`, `tools`, `web`, `docs`.
2. Se ejecutaron los binarios y scripts existentes, no se supuso su resultado.
3. Se midió con las herramientas del propio repositorio (`tools/budget_report.py`, `tools/preset_audit.py`,
   `build/host/regress`, `build/host/ui_test`, `build/host/engine_param_test`).
4. No se declaró ninguna prueba de target, WASM, navegador o hardware.

Límites de este entorno, que condicionan todo lo que sigue:

| Recurso | Estado | Consecuencia |
|---|---|---|
| Toolchain JieLi pi32v2 / SDK AC79 | no disponible en esta sesión | no hay `.elf`, `.fwsc`, RAM/CPU/flash de target |
| `emcc` | no disponible | no hay WASM nuevo |
| Navegador | no disponible | no hay BROWSER TESTED |
| FM-1 física | no disponible | no hay HARDWARE VERIFIED, ni escucha, ni instalación |
| Backup `.bin` de `144531` | no está en el workspace | no se puede verificar tamaño/SHA del dump |

Por eso este documento separa siempre **host** de **target** y **hardware**. Un PASS de host no es un PASS
del AC791N.

---

## 2. Inventario real del árbol

### 2.1 Tamaño y estructura

| Elemento | Valor medido |
|---|---:|
| Líneas de firmware (`firmware/src/*.c` + `*.h`) | 52 580 |
| Líneas de tests C (`tests/*.c`) | 42 232 |
| Páginas declaradas en `PAGES` (`params.c`) | 84 |
| Familias de página | 10 (`EDIT`, `SEQ`, `FX`, `LFO`, `SAVE`, `GLO`, `SCL`, `ENV`, `ARP`, `TRK`) |
| Motores | 9 |
| Presets totales | 267 |
| Presets con movimiento (`x[]`) | 265 / 267 |
| Presets sin ningún send | 0 |
| Duplicados exactos / nombres repetidos / fuera de rango / silencios | 0 / 0 / 0 / 0 |

Los 267 presets se reparten así: 251 en la tabla clásica de ocho motores, 14 de WTABLE y 2 entradas
legacy de DRUM. La suma por motor del audit es:

`ANALOG 68 · PHASE 32 · LOFI 29 · VOICE/FORMANT 25 · WHEEL 22 · DRUM 18 · NOISE 22 · FM6 37 · WTABLE 14`.

### 2.2 Páginas por familia

| Familia | Páginas | Qué cubre |
|---|---:|---|
| EDIT | 26 | motores, operadores FM6, carácter, filtro, morph, envíos |
| SEQ | 18 | pasos, piano roll, drum grid, automatización, humanizar, mutar, tools |
| FX | 17 | buses, delay, reverb, coro, slicer, compresor, EQ |
| LFO | 6 | LFO, destinos, shape, mod matrix, mseq |
| SAVE | 5 | proyectos, slots, usuario, patrones |
| GLO | 4 | ajustes globales, MIDI, info, load |
| SCL | 3 | escala, acorde, progresión |
| ENV | 2 | envolvente y destinos |
| ARP | 2 | arpegio y su segunda página |
| TRK | 1 | mezcla de pistas |

Cada pantalla multipágina ya tiene indicador: `ui_draw.c:829` escribe `PAGE n/m`. No es un pendiente.

---

## 3. Lo que ya existe (y no hay que duplicar)

El brief pide investigar si las funciones existen antes de añadirlas. Ésta es la correspondencia real:

| Función pedida | Estado | Evidencia en código / prueba |
|---|---|---|
| LFO sincronizado con BPM y divisiones musicales | **ya existe** | `P_LRATE` negativo, `N_LSYNC` 4BAR…1/32, `voice.c:373`; página LFO |
| Morphing entre estados / presets | **ya existe** | `P_MORPH` en ANALOG, PHASE, LOFI, WHEEL, NOISE, VOICE, WTABLE; `tests/morph_audible_test` 21 casos |
| Morph bajo carga, seis pistas y buses | **ya existe** | `tests/morph_six_test`; latencia, slew y click medidos |
| Cuatro macros asignables | **ya existe** | `LAYER_MACRO`, `macro_v[]`, `P_M1SRC…P_M4AMT`, `ui_layer.c` |
| Matriz de modulación | **ya existe** | 4 slots SRC/DST/AMT, `mod.c`, página MOD, `tests/mod_test` |
| Destinos múltiples LFO/ENV | **ya existe** | `P_LD_PIT/FLT/SHP/AMP`, `P_ED_*` |
| Edición por pasos, piano roll y drum grid | **ya existe** | `ui_graph.c`, `ui_tools.c`, `GR_ROLL`, `GR_DRUM` |
| Euclidean / generación de patrones | **ya existe** | `tl_euclid()` en `ui_tools.c:102` |
| Ratchet x1…x4 | **ya existe** | `seq_ratchet()`, `tests/ratchet_test` |
| Probabilidad, condiciones, humanización | **ya existe** | `P_APROB`, `P_HVEL/P_HTIM/P_HGAT`, condiciones de trig; tests |
| Microtiming y swing | **ya existe** | `P_ASWING`, `P_SSWING`, HUMAN TIME |
| Acordes, inversiones y voicings | **ya existe** | `P_CHRD/P_CVOI/P_CSTR`, `chord_build`, `tests/chord_scale_test` |
| Escalas, quantize y progresiones | **ya existe** | `P_SCALE/P_ROOT/P_QUANT`, página PROG |
| Escenas de interpretación | **ya existe** | `scene_capture`, `song_idx.scene`, `ui_input.c:986` |
| Undo / redo | **ya existe** | `undo_mark()` / `undo_do()`, SAVE sostenido, GLO G4 |
| A/B de estados guardados | **parcial**: escenas y slots, no un A/B de sonido | no se añadió para no duplicar escenas/slots |
| Copiar / pegar pasos | **ya existe** en herramientas de SEQ | `ui_seqtools.c`, `ui_tools.c` |
| Indicador de página | **ya existe** | `ui_draw.c:829` |
| Feedback de teclas y controles | **ya existe** | `ui_input.c`, LEDs, animación de teclas |
| Medidores reales de FX / señal | **ya existe** | `ui_tech.c`, `fx_meter_test`, `dly_echo_test` |

Conclusión: varias de las “nuevas” funciones del brief ya están implementadas y probadas. Volver a
implementarlas sería deuda, no evolución.

---

## 4. Problemas y oportunidades detectados

### 4.1 DSP y audio

No se encontró un defecto DSP reproducible en esta pasada. La auditoría de parámetros de motor
(`engine_param_test`) recorrió **216 knobs × 3 presets** a ambos extremos de su rango, a través del render
real, y encontró **0 controles sin efecto audible**.

Los golden renders siguen siendo la red de seguridad: **285 renders, 0 cambiados, 0 desaparecidos, 0 fallos
de salud, 0 fallos de voces/routing, 0 crashes**.

Oportunidades que **no** se implementaron por no ser demostrables sin escucha o sin target:

- Reducir el coste de reverb/delay en LOAD ECO: requiere medir en AC791N y escuchar el resultado.
- Nuevos algoritmos FM6 o WTABLE: coste de CPU/RAM no medible aquí.
- Cambios de saturación o filtros: riesgo de alterar el carácter y los 285 goldens; necesitan aprobación
  sonora, no una decisión automática.

### 4.2 CPU y memoria

Medición de host disponible, **no de target**:

- `regress` pasó el presupuesto agregado de CPU con **207 filas**, mediana de host 1,05 y límite agregado
  +20 %.
- 7 filas quedaron por encima de +40 % de su baseline de nanosegundos y se informaron como nota, no como
  fallo: LOFI `08_BYTE_TONE`, WHEEL `02_GOSPEL`, DRUM `07_INDUSTRIAL`, FM6 `20_TIBET_BOWL` y tres más.
  Son ns de host compartido, no instrucciones del AC791N; no se corrigen a ciegas.
- `budget_report.py` (host) da `voice_t` 88 B, seis pistas 6 336 B con 12 voces, 5 280 B con 10.
  La RAM exacta del target **no se midió** en esta sesión.
- Flash, `.data+.bss`, `.ram_text`, pool y NOINIT: **NOT VERIFIED** hoy; requieren `build/felucca.elf` y el
  linker del AC791N. No se reutilizan cifras históricas como si fueran actuales.

### 4.3 Interfaz

- `ui_test`: **all passed**.
- `ui_render`: en la pasada anterior de esta rama, 189 × 22, 0 hallazgos de lint, 3 390 046 alineaciones y 0
  descentradas; esta pasada no se regeneraron las capturas.
- El defecto real de la Fase 3 (etiquetas ARP solapadas 2 px con la barra) está corregido en `4d3b0ad`.
- No se encontraron controles nuevos que dibujen una cosa y modifiquen otra en el barrido estático.

### 4.4 Presets

`tools/preset_audit.py` sobre el dump real:

| Motor | n | Con movimiento | Sin FX | DIST | CHOR | DLY | REV |
|---|---:|---:|---:|---:|---:|---:|---:|
| ANALOG | 68 | 68 | 0 | 15 | 33 | 32 | 46 |
| PHASE | 32 | 32 | 0 | 11 | 32 | 30 | 52 |
| LOFI | 29 | 29 | 0 | 9 | 14 | 34 | 32 |
| VOICE | 25 | 25 | 0 | 9 | 47 | 30 | 70 |
| WHEEL | 22 | 22 | 0 | 12 | 27 | 19 | 56 |
| DRUM | 18 | 16 | 0 | 15 | 6 | 17 | 24 |
| NOISE | 22 | 22 | 0 | 9 | 30 | 30 | 69 |
| FM6 | 37 | 37 | 0 | 3 | 32 | 30 | 57 |
| WTABLE | 14 | 14 | 0 | 9 | 31 | 28 | 48 |

- 0 problemas estructurales, 0 duplicados, 0 silencios.
- Distribución por categoría: PADS 57, KEYS 56, TEXTURES 49, DRUMS 24, BASS 18, ORGANS 18, BELLS 15,
  LEADS 12, PLUCKS 11, ARPS 7.
- Oportunidad B: WTABLE tiene sólo 14 presets y FM6 usa DIST en 3 de 37. Ampliar esas familias es defendible,
  pero **no se hizo**: exige escucha y criterio musical, no sólo contar huecos.

### 4.5 M-Flasher / backups

Ya corregido y probado en `a7fc3ac`:

- el importador no estampa un commit 0.9.1 en backups 0.11.0;
- `fm1_safewrite.py` usa `O_EXCL`, `fsync` y `sync_dir`, y no sobrescribe;
- el loader, ante un chip desconocido, devuelve `-15` sin borrar, programar ni tocar registros.

Pendiente real: verificar el `.bin` de `144531` (1 048 576 B, SHA
`38c66ae7…d9cd`). Sin el archivo no se puede afirmar nada sobre su contenido.

---

## 5. Matriz A–E

| ID | Propuesta | Prioridad | Estado | Coste | Riesgo | Prueba necesaria |
|---|---|---|---|---|---|---|
| A1 | Backups M-Flasher inmutables | **A** | **IMPLEMENTED · HOST TESTED** | bajo | bajo | `mflasher_test`, `flash_read_test` |
| A2 | Chip desconocido sin escrituras | **A** | **IMPLEMENTED · HOST TESTED** | bajo | bajo | `ldr_test`, ASan previo |
| A3 | Etiquetas ARP sin solapamiento | **A** | **IMPLEMENTED · UI RENDER** | bajo | bajo | `ui_render`, `ui_test` |
| A4 | Ficha del Hub / hashes v2.9.7 | **A** | **IMPLEMENTED · HOST TESTED** | bajo | bajo | `check_fmhub`, `SHA256SUMS` |
| A5 | Auditoría global reproducible | **A** | **IMPLEMENTED** (este documento) | bajo | ninguno | comandos de §6 |
| B1 | Ampliar WTABLE y familias FM6/DRUM con escucha | **B** | NOT IMPLEMENTED | medio | musical | escucha A/B, `preset_audit_test`, goldens |
| B2 | Medir y ajustar las 7 filas de CPU ns altas | **B** | NOT VERIFIED | medio | bajo | target + `target_budget.py` |
| B3 | Añadir A/B de sonido si las escenas no bastan | **B** | NOT IMPLEMENTED | medio | medio | diseño de memoria + `ui_test` |
| B4 | Más páginas de edición contextual | **B** | NOT IMPLEMENTED | alto | UI | `ui_render`, lint |
| C1 | Nuevo motor o algoritmo espectral | **C** | NOT IMPLEMENTED | alto | alto | CPU/RAM target + escucha |
| C2 | Envolventes adicionales | **C** | NOT IMPLEMENTED | medio | medio | presupuesto RAM + goldens |
| C3 | Morph con LFO/ENV como eje | **C** | parcial (morph existe) | medio | medio | `morph_audible_test` |
| D1 | Reverb/delay de mayor calidad | **D** | NOT IMPLEMENTED | alto | alto | target, escucha, estabilidad |
| D2 | Secuenciador polirrítmico nuevo | **D** | ya hay base (C.LEN, longitudes) | alto | alto | tests de SEQ |
| E1 | Duplicar LFO sync / macros / mod matrix | **E** | descartado | — | — | ya existen |
| E2 | Añadir controles sin destino DSP | **E** | descartado | — | — | viola regla 4 |
| E3 | Copiar interfaces de escritorio | **E** | descartado | — | — | no caben en 240 × 240 |

---

## 6. Pruebas ejecutadas en esta pasada

Todas se ejecutaron en Linux x86-64, sobre este árbol, con los binarios/scripts del repositorio.

| Prueba | Comando | Resultado |
|---|---|---|
| Regresión completa | `build/host/regress tests/golden.txt tests/cpu_baseline.txt` | **PASS**: 285 goldens, 0 cambios, 0 fallos de salud/voces/CPU, 14,6 s |
| UI | `build/host/ui_test` | **PASS**: all passed |
| Auditoría de presets (sonido) | `build/host/preset_audit_test` | **PASS**: 267, 0 silencios/hot/duplicados/fuera de rango |
| Auditoría de knobs de motor | `build/host/engine_param_test` | **PASS**: 216 knobs × 3 presets, 0 sin efecto |
| Morph | `build/host/morph_audible_test` | **PASS**: 21 casos, 0 fallos |
| Fase 2 | `build/host/phase2_test` | **PASS** |
| Formato de proyecto | `build/host/project_test` | **PASS** |
| Ratchet | `build/host/ratchet_test` | **PASS** |
| Acordes / escalas | `build/host/chord_scale_test` | **PASS** |
| M-Flasher | `python3 tests/mflasher_test.py` | **PASS** (dump real omitido: `FM1_REAL_DUMP` no seteado) |
| Lectura de flash | `python3 tests/flash_read_test.py` | **PASS**: 16 checks, 0 FAILED |
| Gate | `python3 tests/gate_test.py` | **PASS** |
| Evidencia de flash | `python3 tests/flash_evidence_test.py` | **PASS**: 20 checks, 0 FAILED |
| Instalador | `python3 tests/install_test.py` | **PASS**, 2 skips por faltar `build/felucca.fwsc` y `FM-1_v15.fwsc` |
| Loader | `build/host/ldr_test tests/fixtures/felucca-v1.9.0.fwsc release/felucca-v2.9.7.fwsc` | **PASS** |
| Presupuesto host | `python3 tools/budget_report.py` | **PASS** (medición host, no target) |
| Auditoría de biblioteca | `python3 tools/preset_audit.py` | **PASS**: 267, 0 problemas |

No ejecutado hoy:

- `ldr_test_asan`: el binario no terminó dentro del límite de 180 s de esta sesión y se canceló. No se
  declara PASS. La cobertura ASan de `ldr_test` consta de una pasada anterior de la rama, no de hoy.
- Suite host completa `tests/run_tests.sh`: no cabe en la ventana de 5 minutos y no se ejecutó entera.
- Target AC791N, WASM, navegador y hardware: no disponibles.

---

## 7. Archivos y commits

Cambios de código de esta rama, ya subidos:

| Commit | Contenido |
|---|---|
| `a7fc3ac` | M-Flasher: backups inmutables, chip desconocido sin escrituras, tests |
| `4d3b0ad` | ARP: etiquetas separadas de los medidores, capturas |
| `e635b86` | Fase 4: ficha del Hub y hashes v2.9.7 |

Documento nuevo de esta pasada:

- `docs/QG_GLOBAL_AUDIT_2026-11-13.md` (este archivo).

No se modificó ningún otro archivo de firmware, presets, web, instalador o release.

---

## 8. Limitaciones pendientes

| Pendiente | Por qué | Cómo cerrarlo |
|---|---|---|
| AC791N TARGET COMPILED | sin toolchain/SDK en esta sesión | ejecutar `./build.sh` con el toolchain pinneado y el SDK correcto |
| RAM/CPU/flash de target | depende del build anterior | `tools/ram_report.py`, `tests/target_budget.py` |
| WASM TESTED | sin `emcc` | reconstruir y correr `emu_test.mjs` |
| BROWSER TESTED | sin navegador | abrir el emulador y revisar consola/audio |
| HARDWARE VERIFIED | sin FM-1 | banco con unidad secundaria, no la principal |
| Backup `144531` | falta el `.bin` | aportar la ruta del dump y verificar tamaño/SHA |
| 7 filas de CPU ns altas | ruido de host compartido | medir instrucciones en target antes de tocar DSP |
| Fase 2 y Fase 3 | requieren aprobación | revisión del propietario; no se fusionan solas |

---

## 9. Recomendación del siguiente conjunto de tareas

1. **Cerrar la validación de target** con el toolchain correcto: tamaño, RAM, `.ram_text` y presupuesto de
   instrucciones. Sin eso, cualquier “optimización” es una hipótesis.
2. **Escuchar y equilibrar la biblioteca**: WTABLE (14) y FM6 (DIST en 3/37) son los huecos más claros.
   Hacerlo con `preset_audit_test` y goldens, no por conteo.
3. **Verificar el backup `144531`** cuando el propietario aporte el `.bin`.
4. **No abrir una Fase 2/3 nueva** hasta que la anterior esté aprobada y validada en target.

La prioridad no es añadir complejidad: es convertir las funciones que ya existen en un instrumento
**medido y escuchado**. Eso es exactamente lo que este árbol necesita ahora.
