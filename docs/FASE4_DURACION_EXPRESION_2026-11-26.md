# FASE 4/5 — Duración real, expresividad y auditoría de motores y presets

Fecha: 2026-11-26 · versión v2.5.4 · base v2.5.3 (`656ade2c`)

Las medidas van etiquetadas: `[AC791N]` compilación real con el toolchain de JieLi (no es una medición en
hardware), `[HOST x86-64]` el host de pruebas, `[EMU]` el emulador WebAssembly.

## 1. Grabación y reproducción de la duración real

### Lo que ya estaba (y se verificó, no se reimplementó)

La duración real de cada nota ya estaba grabada en el árbol de partida: `seq.c rec_note/rec_hold/rec_release`
miden la pulsación desde la primera tecla del grupo, la escriben en el paso cuando la nota cabe en él y en el
último TIE cuando cruza, y `seq_step()` la usa como `seq_off`. `tests/seq_dur_test.c` lo cubre (un cuarto de
paso → 4/16, acorde con su nota más larga, nota que cruza → 12/16 en el TIE, ida y vuelta por proyecto). REC Q
(`P_HRQ`), microtiming (`ST_MICRO`), swing, TIE, REST, overdub y la compatibilidad de proyectos ya estaban.

El problema real era el **coste**: `step_t` llevaba un `uint8_t dur`, es decir, un byte por paso (384 B en seis
pistas), que es la forma que el presupuesto de LOWBUF había rechazado antes.

### La representación nueva: cuatro bits dentro del paso

`dur` desaparece del struct y pasa a **cuatro bits libres de `flags`**: 5, 6, 7 (los tres bajos) y 2 (el alto).
`flags` sólo usaba 0 (acento), 1 (slide) y 3–4 (RATCH), y ahora tiene dos accesores en `core.h`:

```c
static uint32_t step_dur(const step_t *s);        /* 0 = GATE, 1..15 dieciseisavos del paso */
static void step_set_dur(step_t *s, uint32_t d);  /* escribe sólo sus cuatro bits */
```

Por qué así y no una tabla dispersa (la otra opción del enunciado):

- **Exacta siempre.** Una tabla de excepciones con tope pierde duraciones en cuanto una interpretación tiene más
  pasos con longitud que entradas; aquí no hay tope ni caso degenerado.
- **Sin segunda estructura.** Al vivir en el paso, cualquier copia de un paso (mover un patrón, un kit, cargar un
  proyecto, el editor) lleva la duración sin sincronizar nada. Se auditaron y corrigieron los sitios que
  escribían `flags` en bloque (`gen.c`, `gen_trap.c`, `song_dice.c`, `ui.c`, `editor.c`) para que conserven
  `SF_DUR`.
- **Cero coste de búsqueda.** Leerla es una máscara y un desplazamiento, no un recorrido de tabla.

Medido `[AC791N]`: `sizeof(step_t)` 18 → **17 B**, RAM `.data+.bss` **76 048 → 75 600 B** (−448 B, −384 de los
pasos), **LOWBUF 29 256 → 28 488 B** (el par de imágenes de deshacer del proyecto baja 384 B cada una) e imagen
**+380 B** (el acceso a los bits y la columna HOLD). El formato del proyecto **no cambia**: `D` `U` sigue escribiendo un
byte sólo donde la duración no es el GATE, así que los proyectos viejos cargan y los nuevos no gastan un byte por
paso. Los patrones anteriores a la función tienen `dur = 0` (el GATE de siempre).

### Control nuevo: SEQ > TIMING, perilla 4, HOLD

La página TIMING tenía su cuarta perilla vacía. Ahora muestra y edita el valor grabado: `GATE`, o `n/16` para
1..15 dieciseisavos del paso, con el mismo dibujo y el mismo lint que el resto (`ui_input.c timing_edit`,
`ui_draw.c`). Es el único control que faltaba para que la duración grabada sea también editable sin volver a
tocar las teclas.

## 2. MIDI y control expresivo — auditoría

| Función | Estado en el árbol | Acción |
| --- | --- | --- |
| CC2 Breath | Fuente `MS_EXPR` de la matriz; `mod.c` toma el menor de EXPR y breath; CC2 escribe su propio byte (`br_off`) para convivir con CC11 | ninguna (ya soportado) |
| CC11 Expression | Igual, `ex_off` | ninguna |
| Aftertouch de canal / polifónico | `at` en la matriz; `pat` por nota (0xA0) | ninguna |
| Pitch bend | `midi_bend_q8` con suavizado de ~6 ms y aterrizaje exacto (`voice.c`), rango por RPN | ninguna |
| CC5 Portamento Time | `{5, P_GLIDE}` en el mapa CC estándar: escribe el tiempo de glide de la pista | ninguna |
| CC74 / CC71 | CUT / RES del motor por su tabla `MIDI_CC_ENG` | ninguna |

Las fuentes de CC se leen **una vez por bloque** (32 muestras, 2,9 ms): el salto máximo de un mensaje MIDI es
1/127 por bloque, y el bend, que es el único que puede saltar de −8192 a +8191, ya está suavizado. No se añadió
otro sistema de suavizado ni otra matriz: sería duplicar el SLEW que ya existe en la matriz y en el LFO.

## 3. Motores — auditoría de lo que se puede exponer

| Motor | EDIT | CHARACTER | Hueco real |
| --- | --- | --- | --- |
| ANALOG | 8/8 | KTR+, TONE | ninguno; KTR+ ya es el seguimiento de filtro |
| FM6 | 8/8 | TONE, FOLD | ninguno; el patch de operadores vive en su propia página |
| WAVETABLE | 8/8 | WARP, POSMOD, FOLD | PHASE (fase 3) ya integrada |
| ADDITIVE | 8/8 | EVOLVE, RULE | DRAW y RULE (fase 3) ya integrados |
| PHASE | 8/8 | HARM, NOISE | ninguno |
| LOFI | 8/8 | BITS+, RATE+ | ninguno |
| VOICE (FORMANT) | 8/8 | F3+, ARTIC, NASAL | ninguno; el seguimiento de formantes es fijo por diseño |
| NOISE | 8/8 | BAND, EVOL, HPF | ninguno; TRK ya expone el key tracking |
| WHEEL | 8/8 | LEAK, ROTOR+, P.DCY | ninguno |
| DRUM | 8/8 | (los niveles de lane) | ninguno |

No hay slots de EDIT libres en ninguno de los diez motores, y CHARACTER está lleno o reservado (la tercera
perilla es `P_XCH`/`P_MORPH` del motor). Añadir un control exigiría retirar otro; no se hizo, y no se agregó
ningún control decorativo.

## 4. Presets

`preset_audit_test` sobre los 298 presets de fábrica: **0 silenciosos, 0 por encima del nivel declarado, 0
duplicados exactos, 0 nombres repetidos, 0 valores fuera de rango**. Cinco presets (PHASE `PD 808`, `ETHNIC
PLUCK`, `SYNC CATHEDRAL`; LOFI `BYTEBEAT`, `BYTE VAR`) quedan **reportados** con DC entre 2 y 3,6 % del fondo de
escala: es una consecuencia de sus formas asimétricas, el test lo informa como dato y no como fallo, y no se
cambió su identidad ni se añadieron kits DRUM-X. `preset_categories`, índices y compatibilidad no se tocaron.

## 5. Pruebas ejecutadas

- `regress`: **316 goldens, 0 cambiados**, 0 fallas de health/voces. El chequeo de CPU por host falla en esta VM
  también con el binario de HEAD (host ~2,8× más lento que la baseline): no es el cambio.
- `seq_dur_test`: todo pasa, más las comprobaciones nuevas de la representación (17 B, bits compartidos con
  acento/slide/RATCH, copia del paso, y que borrar la duración no toca los flags).
- `six_track_live` 61/61 s; `ui_test`, `project_test`, `editor_test`, `seq_tools_test`.
- UI: 203 pantallas × 22 paletas + barrido de páginas/valores, **0 hallazgos** de lint, 0 tintas sobre su color,
  0 medidas de alineación fuera de 1 px (la columna HOLD incluida).
- `target_budget.py` dentro de presupuesto; emulador WebAssembly bit a bit contra el build nativo.

## 6. Límites

- No hay medición en un FM-1 real: `[AC791N]` es compilación y `[HOST]` es esta VM.
- La duración se mide en dieciseisavos del paso: una nota más corta que 1/16 o más larga que 15/16 se redondea
  (la segunda sigue usando TIE, como antes). No se añadió una resolución mayor porque no cabe en cuatro bits y
  porque el modelo de pasos no la reproduce sin cambiar el secuenciador.
- Los cinco presets con DC quedan documentados, no corregidos: corregirlos exige un bloqueador de DC en PHASE y
  LOFI, que cambia el render de todos sus presets y no forma parte de esta fase.
