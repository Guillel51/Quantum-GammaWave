# QG — Plan de expansión de EDIT

**Fase:** 1/6 (auditoría) · **Fecha:** 12 de noviembre de 2026
**Regla de esta fase:** ninguna de estas propuestas se implementa acá. Cada una trae utilidad, referencia,
estado actual, archivos afectados, dificultad, coste estimado, riesgos y pruebas.

Aviso sobre los costes: el único presupuesto medido disponible es el de `docs/MEMORY_BUDGET.md` §2.1
(imagen 531 204 B / holgura 50 428 B; `.data+.bss` 96 088 B / holgura **2 216 B**; RAM code 19 444 B / holgura
5 132 B; pool spare 9 248 B). Las cifras por función son **estimaciones**, y así están marcadas. Se confirman o
se corrigen en la fase de implementación con `./build.sh` + `tests/memory_map_test.py` + `tests/target_budget.py`.

Escala de dificultad: **B** (bajo, ≤1 archivo y <50 líneas) · **M** (medio, 1–2 archivos, 50–200 líneas) ·
**A** (alto, toca formato de proyecto o más de 2 archivos) · **MA** (muy alto).

---

## E1 — Página `VOICE 2` con ALLOC y DETUNE

| Campo | Detalle |
|---|---|
| **Utilidad** | **A.** Hoy dos parámetros que cambian el sonido (modo de asignación de voces y detune de UNISON) son inalcanzables desde el instrumento |
| **Referencia** | JIANT `{"VOICE 2", FAM_EDIT, SC_TRACK, GR_NONE, {P_ALLOC, P_DETUNE, 0xFF, 0xFF}}` (`params.c`). ALLOC ROT/REUSE decide si una voz liberada se reutiliza; DETUNE es el desvío de UNISON (hasta ~±40 cents, `voice.c:770`) |
| **Estado en QG** | parámetros definidos (`core.h:150`), con descriptor (`params.c:106-107`), **leídos por el DSP** (`voice.c:622`, `voice.c:770`), sin página. Sólo editables por el editor web (`web/editor.html:2231`) |
| **Archivos** | `firmware/src/params.c` (1 línea en `PAGES[]`), `firmware/src/ui.c` (`page_visible`, si se quiere condicionar a UNISON), `docs/MANUAL.md` + `MANUAL.es.md` (tabla EDIT) |
| **Dificultad** | **B** (una línea de tabla; el resto es documentación) |
| **Coste** | Flash: ~40 B (una entrada de 16 B en `PAGES[]` + el código de `page_visible`). RAM: 0. CPU: 0 |
| **Riesgos** | (1) la página ya existe con otro contenido en QG (`VOICE 2` = M.STRT/N.RATE): hay que decidir si se **amplía** a 3 parámetros o se crea `VOICE 3`. Recomiendo **ampliar la página existente**: `{P_MSTART, P_NRATE, P_ALLOC, P_DETUNE}` — 4 valores, 0 páginas nuevas, y M.STRT/N.RATE pasan a KNOB 3/4. (2) `page_visible` no debe ocultarla (hoy no lo hace). (3) Cambiar el orden de knobs de una página existente es un cambio de UX: documentarlo |
| **Pruebas** | `tests/ui_render.c` (la página en 17 paletas, lint/alineación), `tests/ui_test.c` (que un giro de KNOB 3 escriba `P_ALLOC` y no `P_MSTART`), `tests/regress.c` (nada debe cambiar: no se toca DSP) |

---

## E2 — Página `FX · PUNCH`: ver y editar la lane de punch-in

| Campo | Detalle |
|---|---|
| **Utilidad** | **A.** La lane de 64 pasos × 4 compases existe, se graba en vivo, se guarda con la sección y se reproduce (incluso con los efectos de la 2ª banca). El usuario **no puede verla ni corregirla**: sólo borrarla entera (FX + G5) o dejarla pasar |
| **Referencia** | JIANT `{"PUNCH", FAM_FX, SC_GLOBAL, GR_PUNCH, {G_PFXLEN, G_PFXRES, 0xFF, 0xFF}}` + `graph_punch` (`ui_lzfx.c`) + `ui_input.c` (poner/borrar un paso) |
| **Estado en QG** | lane implementada (`perform.c:225` `pfx_lane[32]` como nibbles, `pfx.c:134-137` `pfx_lane_at`/`pfx_lane_put`), guardada (`project.c:964`, `song_chain.c:70`, `song_main.c:89`), borrable (`ui_input.c:1289`, `2227`). **Sin página**: `GR_PUNCH` no existe en QG (`params.c` enum `GR_*`) |
| **Archivos** | `firmware/src/params.c` (nuevo `GR_PUNCH` + entrada `PAGES[]`), `firmware/src/ui_graph.c` (dibujo: 64 celdas de 3×2 px no caben; ver notas), `firmware/src/ui.c` (`page_visible`, ayuda), `firmware/src/ui_input.c` (editar el paso con KNOB 1/2, OCT+), `firmware/src/pfx.c` (una función de lectura ya existe; quizá `pfx_lane_put` pública), `docs/MANUAL.md` |
| **Dificultad** | **M** |
| **Coste** | Flash: **1,6–2,4 KB** (dibujo de 4 compases × 16 pasos + el cursor + las etiquetas de efecto; comparar con `graph_steps`, ~2 KB). RAM: **0** (la lane ya está en `.bss`; el cursor puede ser un `static uint8_t`). CPU: sólo redibujo. El ISR no cambia |
| **Diseño para 240×122** | 4 compases × 16 pasos: 64 celdas. En 232 px: **3,6 px por celda** — demasiado fino para un icono, suficiente para un color. Propuesta: 64 barras verticales de 3 px (offset 8, paso 3,52 con separador de compás de 2 px), **color por efecto** (una paleta de 6 colores: REPEAT, OCT, TEMPO, DEC, STUT, ARP/RND, más las de la 2ª banca); el paso bajo el playhead con un marco de 1 px; el paso elegido con `T_TINT`; el nombre del efecto elegido **en texto** arriba (`STUTTER 1/8`) y la posición (`BAR 2 · STEP 7 · TGT SYN`) debajo. Es legible sin glifos, y el lint de UI no tiene nada que medir en un rect |
| **Riesgos** | (1) **La lane es de la sección**, no del track: la página debe ser `SC_GLOBAL` como en JIANT. (2) Los valores de la 2ª banca (`14 GLITCH`, `15 TAPE`) tienen significado distinto al de JIANT: hay que usar la tabla de QG (`pfx.c` `pfx_of`), no la de JIANT. (3) No permitir editar mientras el playhead pasa por la misma celda sin criterio: bloquear la escritura en el paso actual mientras `song.playing`, o aceptar que se sobrescriba y decirlo en el manual |
| **Pruebas** | Nuevo `tests/pfx_lane_test.c`: escribir un paso con la API y leerlo con `pfx_lane_at`; que el nibble 13/14/15 hagan lo que dice `pfx_of`; que el borrado de un paso no toque los vecinos (los nibbles comparten byte: es el bug fácil de esta entrega). `ui_render` con la página nueva. `project_test.c`: ida y vuelta por sección con la lane llena |

---

## E3 — Hacer visible la segunda banca del layer FX

| Campo | Detalle |
|---|---|
| **Utilidad** | **A.** REVERSE, FREEZE, TAPE STOP, GLITCH y RETRIGGER están implementados (`perform.c:34`, `305`), con 5 efectos de buffer compartiendo el loop del slicer. El único camino es **pulsación larga de A#4**, y no está en el manual |
| **Referencia** | JIANT los pone en la **primera** banca (F3..C4 = REPEAT ×3, GLITCH, TAPE STOP) y añade SHUFFLE y FREEZE con knobs propios |
| **Estado en QG** | implementado y funcional; `ui_layer.c` ya dibuja la banca (`FX_BANK2`, `fx_white_name`) pero **sólo cambia con el gesto oculto** |
| **Archivos** | `docs/MANUAL.md` §3 y §11, `MANUAL.es.md`, y opcionalmente `firmware/src/ui_layer.c` (una card que diga `BANK 2 · A#4 HOLD` permanente, como la de LATCH) |
| **Dificultad** | **B** (documentación; el código ya está) |
| **Coste** | Flash 0–60 B (una card), RAM 0, CPU 0 |
| **Riesgos** | Que la documentación prometa un gesto que cambie en la próxima iteración. Escribirlo con el nombre exacto del código (`pfx_bank`) |
| **Pruebas** | `tests/perform_test.c` ya cubre los efectos de buffer; añadir un caso que verifique que `pfx_bank` alterna con la pulsación larga y que la banca 1 se restaura al soltar |

---

## E4 — Página `EDIT · FOCUS` (parámetro + ranura de modulación)

| Campo | Detalle |
|---|---|
| **Utilidad** | **A.** Es la función que convierte la matriz de modulación de "4 ranuras a ciegas" en "apunto este parámetro del motor a esta fuente". Hoy, para modular `CUT` hay que ir a LFO > MOD, recordar que CUT es el destino 2, y que E1..E8 son los knobs del motor |
| **Referencia** | JIANT `{"FOCUS", FAM_EDIT, SC_TRACK, GR_FOCUS, {…}}`, `ui_input.c focus_knob`: KNOB 1 PARAM (E1..E8 con la etiqueta real del motor), KNOB 2 VALUE, KNOB 3 SRC, KNOB 4 AMT; escribe en la ranura MOD elegida |
| **Estado en QG** | no existe. La información está: `N_MDST` ya rotula `E1..E8` como destinos, y `param_desc` del motor da las etiquetas |
| **Archivos** | `firmware/src/params.c` (`GR_FOCUS` + página), `firmware/src/ui_graph.c` (gráfico: el parámetro grande + su nombre + la fuente y la cantidad), `firmware/src/ui_input.c` (la lógica de los 4 knobs y en qué ranura escribe), `firmware/src/mod.c` (sin cambios: sólo escribe `P_M*SRC/DST/AMT`) |
| **Dificultad** | **M/A** (la lógica de los knobs es la parte delicada: hay que decidir cuándo crear ranura nueva y cuándo reutilizar) |
| **Coste** | Flash **1,2–1,8 KB**, RAM **0** (usa `mod_ui_slot`, ya existente, o un `static`), CPU 0 |
| **Riesgos** | (1) Si el usuario apunta la misma ranura dos veces, se pisa: hay que mostrar `SLOT 1..4 · USADO` y no sobrescribir en silencio. (2) `P_E0..P_E7` no son universales: en FM6 E8 es SLOT (banco de patch), no un parámetro modulable de verdad — `FOCUS` debe leer el descriptor del motor y deshabilitar los que no sean continuos. (3) La página debe ocultarse con LOAD ECO si el coste de dibujo importa |
| **Pruebas** | `tests/mod_test.c` (que la ranura escrita produzca audio distinto y que las ranuras inactivas sigan bit-idénticas), `ui_test` (que KNOB 3 no escriba en `P_E*`), `ui_render` |

---

## E5 — Grupos y pestañas dentro de EDIT

| Campo | Detalle |
|---|---|
| **Utilidad** | **A.** Con 25 páginas visibles en el peor caso (DRUM + FM6 condicionales), un ciclo plano obliga a pasar por KIT 2, KIT 3, X-MOD, SOUND 1..4 para llegar a FILTER |
| **Referencia** | JIANT `CE_SYN` / `CE_FM6` / `CE_DRM` en `ui_lens.c` + pestañas dibujadas por `draw_tabs` (`ui_draw.c`), breadcrumb `EDIT · TONE · WORDS 6/7` |
| **Estado en QG** | ciclo plano con memoria por familia (`ui.fam_last`) y salto de sección sólo en FX (`ui.c:714-748`). No hay grupos |
| **Archivos** | `firmware/src/params.c` (tabla de grupos: 3 valores por página, `{grupo, orden}` — 25–28 B de const), `firmware/src/ui.c` (`page_visible`, `open_family`, una `page_next_group`), `firmware/src/ui_draw.c` (tira de pestañas de 8 px bajo el header, o el breadcrumb del pie), `firmware/src/ui_input.c` (doble toque = grupo siguiente) |
| **Dificultad** | **M** |
| **Coste** | Flash **1,5–2,5 KB** (tabla + dibujo de pestañas + lógica), RAM **0** si los grupos son constantes, CPU 0. **Ojo:** una tira de pestañas come alto del panel (o del header). Con 240 px de alto y el header en 28, una tira de 8 px baja el panel de 122 a 114 |
| **Riesgos** | (1) Es un cambio de **navegación global**: afecta a todas las familias si se generaliza. Hacerlo **primero sólo en EDIT** y medir. (2) `ui_render` tiene 172 pantallas; la tira nueva entra en todas ⇒ hay que actualizar las capturas de referencia y esperar hallazgos de alineación. (3) Todo el estado de `ui` es byte a byte delicado (`ui.fam_last`, `ui.nmem` no existe en QG): no agregar campos si se puede codificar en los existentes |
| **Pruebas** | `ui_test`: recorrido completo de EDIT con un track de cada motor y comprobación de que se visitan exactamente las páginas visibles, sin repetir y sin salir; `ui_render` completo; prueba manual del orden con FM6 y DRUM |

---

## E6 — Retrato reducido del motor dentro de EDIT

| Campo | Detalle |
|---|---|
| **Utilidad** | **B.** Ayuda a entender qué hace un parámetro sin saber el nombre del motor |
| **Referencia** | JIANT `ui_engvis.c` (129 KB): tracto vocal animado en VOICE, colonia de operadores en FM6, célula polar en ANALOG |
| **Estado en QG** | `graph_fm6` (chart de algoritmo), `graph_phase`, `graph_lofi`, `graph_wheel`, `graph_colony` (DRUM). ANALOG, VOICE, NOISE y WHEEL (EDIT 1/2) caen a `graph_being()` o al osciloscopio (`ui_graph.c:3266-3280`) |
| **Archivos** | `firmware/src/ui_graph.c` (o un `ui_engvis.c` nuevo incluido desde ahí), `firmware/src/ui_being.c` (reutilizar `bg_col`, `gw_*`) |
| **Dificultad** | **A** (es arte procedural: mucho código y mucho ajuste visual) |
| **Coste** | Flash: **4–8 KB por motor** si se hace con el detalle de JIANT (su `ui_engvis.c` es ~129 KB de fuente, aunque el binario sólo toma lo usado). RAM: 0–64 B. CPU: sólo al redibujar |
| **Riesgos** | (1) El `ui_render` de QG mide alineación de **texto**; el arte procedural sin texto no lo rompe, pero sí puede tapar valores. (2) **Identidad**: QG tiene su propio lenguaje visual (planetas, forja, ribbons). Portar el tracto vocal de JIANT metería un idioma ajeno en HOME/EDIT. Recomendación: **no portar** `ui_engvis`, y en cambio hacer que HOME > BEING (ver `QG_HOME_NAVIGATION_PLAN.md`) muestre el `graph_being` del motor grande, que ya existe |
| **Pruebas** | `ui_render` (lint/alineación), `particle_bounds_test.py` (si se animan partículas), captura de referencia por motor |

---

## E7 — Página `STAGE` (envolvente por etapa)

| Campo | Detalle |
|---|---|
| **Utilidad** | **B.** Editar ATK/DEC/SUS/REL viendo la etapa elegida, en vez de los 4 knobs sueltos |
| **Referencia** | JIANT `{"STAGE", FAM_ENV, SC_TRACK, GR_STAGE, {0xFF, 0xFF, P_ELOOP, 0xFF}}` |
| **Estado en QG** | `P_ELOOP` existe y está en `ENV DEST` KNOB 4. No hay página de etapa |
| **Archivos** | `firmware/src/params.c`, `firmware/src/ui_graph.c` (dibujar el ADSR con la etapa marcada y un cursor) |
| **Dificultad** | **M** |
| **Coste** | Flash ~1 KB, RAM 0 (etapa en un `static`) |
| **Riesgos** | Duplicar información con `ENV`; el valor de la página es la enseñanza, no el control. P2/P3 |
| **Pruebas** | `ui_test` (que el cursor no escriba), `ui_render` |

---

## E8 — Página `GLO · SENDS` y `FX · BUSES`

| Campo | Detalle |
|---|---|
| **Utilidad** | **B.** Ver los 6 tracks como barras y los buses como valores principales, en vez de entrar track por track |
| **Referencia** | JIANT `SENDS` (`GR_TRK`, KNOB 1..4 = DIST CHOR DLY REV, gráfico `track_col6`) y `BUSES` (`GR_BUSES`, DMIX RSIZE CDEPTH SLDEP) |
| **Estado en QG** | no existen; los mismos parámetros están en `FX` (por track) y en `DLY`/`REVERB`/`CHORUS`/`SLICER` |
| **Archivos** | `firmware/src/params.c`, `firmware/src/ui_graph.c` (`graph_col6`-like) |
| **Dificultad** | **M** |
| **Coste** | Flash ~1,2 KB cada una, RAM 0 |
| **Riesgos** | Ninguno funcional (son vistas). Riesgo de confusión si los knobs de una vista escriben parámetros de otro track: en `SENDS` la convención de JIANT es que KNOB 1..4 = los 4 envíos **del track seleccionado** y el gráfico muestra los 6 |
| **Pruebas** | `ui_test` (que los knobs escriban el track correcto), `ui_render` |

---

## E9 — ROTOR con profundidad y DUST en WHEEL

| Campo | Detalle |
|---|---|
| **Utilidad** | **C.** El rotor de WHEEL en QG tiene modo (E8) pero no profundidad ni "polvo" |
| **Referencia** | JIANT `{"ROTOR", FAM_EDIT, SC_TRACK, GR_NONE, {P_WRDEP, P_WDUST, 0xFF, 0xFF}}` |
| **Estado en QG** | **no existe**: `P_WRDEP`/`P_WDUST` no están en `core.h` de QG |
| **Archivos** | `core.h` (2 ids nuevos **antes de `P_E0`**), `params.c`, `eng_wheel.c`, `project.c` (**nueva versión de formato**), `editor.html`, `fm4_convert`/`upreset` (mapa por count) |
| **Dificultad** | **A** |
| **Coste** | Flash ~300 B de DSP + 4 B de descriptor; RAM 0; **coste real: una versión de formato de proyecto** y su migración |
| **Riesgos** | Alto: `core.h:154-156` dice explícitamente que los parámetros comunes nuevos van **justo antes de `P_E0`** y que presets/proyectos mapean **por cantidad**. Insertar ids obliga a subir la versión de formato, migrar, y probar ida y vuelta con archivos viejos. **No lo haría por un parámetro C.** |
| **Pruebas** | `project_test.c` (todos los formatos, ida y vuelta), `upreset_test`, `regress`, `editor_test` |

---

## E10 — VOICE con palabras (port de `sing.c` de JIANT)

| Campo | Detalle |
|---|---|
| **Utilidad** | **A** (es un motor distinto en la práctica: la FM-1 canta texto) |
| **Referencia** | JIANT `sing.c` (~60 KB de fuente) + página `WORDS` (LANG LYRIC POS + OCT+/TYPE) + `ui_name.c` para escribir la letra + `ui_engvis.c` (tracto vocal) |
| **Estado en QG** | inexistente: no hay `sing.c`, ni `P_VLANG/P_VLYR`, ni página |
| **Archivos** | enorme: `sing.c` nuevo, 2 parámetros comunes nuevos (⇒ formato), `eng_formant.c` (usar los fonemas), `ui_name.c`, `params.c`, `ui_graph.c`, `project.c`, editor |
| **Dificultad** | **MA** |
| **Coste** | Flash: el de JIANT es uno de sus mayores aportes de código; estimación **8–15 KB** de imagen + tablas de fonemas. RAM: ~200–600 B (estado por voz). Formato: 2 params nuevos |
| **Riesgos** | Muy alto en RAM (2 216 B de holgura) y en CPU (síntesis de formantes por voz). QG tiene **12 voces** frente a las 8 de JIANT: el mismo código costaría ~50 % más CPU en el peor caso. Requiere medición en target antes de comprometerse |
| **Pruebas** | imposible de cerrar sin unidad física; en host, `regress` + un test de inteligibilidad no es medible automáticamente |

---

## E11 — Progresiones armónicas como entidad (nuevo; no existe en JIANT)

| Campo | Detalle |
|---|---|
| **Utilidad** | **A** si se quiere componer por grados en vez de por notas |
| **Referencia** | **ninguna en JIANT** (ver `JIANT_SCL_ANALYSIS.md` §2.6). El único precedente son las "familias de armonías" internas de los generadores |
| **Estado en QG** | no existe |
| **Diseño mínimo viable** | una "progresión" = 8 bytes (8 grados de 4 bits, 0 = vacío) + 1 byte de modo; se reproduce como una lista de acordes, uno por compás, llamando a `chord_build`; no toca el secuenciador ni el formato de proyecto si se guarda en los 33 bytes libres de la sección (`PROJ_PFX_OFF`) — **pero esos bytes ya los usa la lane** (32 + 1). No hay lugar: requeriría formato nuevo |
| **Dificultad** | **A** |
| **Coste** | Flash 2–3 KB (página + motor de reproducción), RAM 0–32 B, formato nuevo |
| **Riesgos** | Solapamiento con el generador de canciones (fuera de alcance) y con el secuenciador. Definir bien: ¿la progresión es un *acompañamiento* que suena junto al patrón, o un *atajo de entrada*? Si es lo segundo, casi no hace falta: ya se puede grabar acorde por paso |
| **Pruebas** | test de `chord_build` por grado, test de ida y vuelta de la progresión, `ui_render` |

---

## E12 — Unificar ALL TR en `SCL` KNOB 4 (cosmético)

| Campo | Detalle |
|---|---|
| **Utilidad** | **C.** JIANT eliminó la página `SCL 2` y puso ALL TR en SCL KNOB 4, dejando TRANS sólo en SEQ > SHIFT. QG tiene `SCL 2` como página aparte |
| **Estado en QG** | `SCL` = ROOT SCALE QUANT TRANS; `SCL 2` = ALL TR |
| **Dificultad** | **B** |
| **Coste** | 0 (sólo tabla) |
| **Riesgos** | Rompe la memoria muscular de los usuarios de QG y el manual. **No lo haría**: la página aparte es más clara, y `SCL 2` ya es visible como `SCL 2/3` en el pie. Sólo renombrarla a `SCL · ALL TR` para que se entienda |
| **Pruebas** | `ui_render`, `ui_test` |

---

## Resumen y orden recomendado dentro de EDIT

| # | Entrega | Valor | Dificultad | Coste flash est. | RAM | Formato |
|---|---|---|---|---|---|---|
| E1 | `VOICE 2` + ALLOC/DETUNE | A | B | ~40 B | 0 | no |
| E3 | Documentar 2ª banca FX | A | B | 0–60 B | 0 | no |
| E2 | Página `PUNCH` | A | M | 1,6–2,4 KB | 0 | no |
| E4 | Página `FOCUS` | A | M/A | 1,2–1,8 KB | 0 | no |
| E5 | Grupos/pestañas en EDIT | A | M | 1,5–2,5 KB | 0 | no |
| E8 | `SENDS` / `BUSES` | B | M | ~2,4 KB | 0 | no |
| E7 | `STAGE` | B | M | ~1 KB | 0 | no |
| E6 | Retrato por motor | B | A | 4–8 KB/motor | 0–64 B | no |
| E9 | ROTOR depth/dust | C | A | ~300 B | 0 | **sí** |
| E10 | VOICE con palabras | A | MA | 8–15 KB | 200–600 B | **sí** |
| E11 | Progresiones | A | A | 2–3 KB | 0–32 B | **sí** |
| E12 | ALL TR en SCL K4 | C | B | 0 | 0 | no |
