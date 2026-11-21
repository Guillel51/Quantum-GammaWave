# Etapa 5/8 — herramientas de edición, modulación y performance

Clases: **`[HOST]`** = x86 (`tests/etapa5_tools_test.c`, `tests/regress`, `groups.sh ui|presets|web`);
**`[AC791N]`** = el build del destino (`./build.sh`, `tests/target_budget.py` sobre `build/felucca.dis`).
Nada `[HW]`: no hay FM-1 física.

## 1. Qué se agregó

| Módulo | Nuevo | Cómo se usa | Coste |
|---|---|---|---|
| **ARP** | **PEDAL** (modo 16) | el bajo de los acordes sostenidos cae en cada tiempo fuerte de cuatro y las tres notas entre ellos suben por el resto. Pedal point clásico. | ~40 B XIP |
| **ARP** | **ALBERTI** (modo 17) | la figura 1-3-2-4, la que sostiene cualquier cadencia. | ~30 B XIP |
| **SCL** | **HIJAZ** (escala 16) | dominante frigia (0 1 4 5 7 8 10): el b2 con la tercera mayor. | tabla |
| **SCL** | **HUNG MINOR** (escala 17) | menor armónica con #4 (0 2 3 6 7 8 11). | tabla |
| **LFO** | **1/2D** y **1/16D** (RATE −13, −14) | las dos divisiones con puntillo que faltaban, junto a las 12 que había. | tabla |

Todos son **valores nuevos al final de una lista existente**: el número guardado en un preset, un proyecto o un
`SYNC`/CC de MIDI conserva su significado (0..15 de ARP, 0..15 de SCALE, −12..127 de RATE siguen sonando igual).
No hay parámetros nuevos, ni campos nuevos en el formato de proyecto o preset, ni RAM nueva.

### Integración

- **Hardware**: los nombres salen de `N_AMODE`, `N_SCALE`, `N_SCALELONG`, `N_SCLFAM` y `N_LSYNC`, que es de
  donde las páginas EDIT/SCL dibujan las listas; los topes que estaban fijos en la UI se ampliaron
  (`ui_scl.c` 15→17, `ui_graph.c` 16→17, `ui_layer.c` 16→18), así que la onda, la escala y el modo nuevos se
  pueden elegir en el aparato y no sólo por MIDI.
- **Web Studio**: `web/editor.html` lleva su propia copia de las listas; se sincronizaron las tres (arp MODE,
  SCALE, LFO RATE en tiempo) más dos que estaban atrasadas desde etapas anteriores (PHASE WAVE y LOFI WAVE con
  su `SYNC`). `python3 tools/sync_editor_mock.py --check` y `node web/test_web.mjs` lo comprueban.
- **MIDI y guardado**: MODE, SCALE y RATE ya viajan por CC/NRPN y por el proyecto; los valores nuevos usan los
  mismos campos int8, así que guardar, recuperar y automatizar no cambian de formato.
- **Compatibilidad**: los valores viejos siguen siendo válidos y suenan idénticos — los **307 golden renders** no
  cambiaron (0 cambiados, 0 desaparecidos).

## 2. Coste `[AC791N]`

| | antes (Etapa 4) | después | Δ |
|---|---|---|---|
| imagen (app slot) | 579 972 B | **580 180 B** | **+208 B** |
| libre en el slot | 3 640 B | **3 432 B** | −208 B |
| `.data+.bss` | 93 612 B | 93 612 B | **0** |
| pool spare / `.lowbuf` / `.noinit` | — | sin cambio | 0 |

`tests/target_budget.py` **exit 0**: ningún lazo del DSP creció (mix_block, los renders de motor y los dos IRQ
quedan en su presupuesto; los dos IRQ incluso anotan −68 % y −19 % respecto del baseline, avisos informativos
que no se escriben). Esto importa porque el primer intento de PEDAL se escribió como llamada recursiva a
`arp_step_index`, y eso hizo que el compilador dejara de aplanar el camino del secuenciador dentro de
`mix_block`: **+37 %** en el presupuesto del bloque de audio. Se reescribió sin recursión (el índice de UP es
`i % len`) y el presupuesto volvió a 0: la medición es la que decidió, no la intuición.

## 3. Prueba nueva

`tests/etapa5_tools_test.c` (registrada en `tests/run_tests.sh`), 3 grupos:

- **ARP**: los 10 modos cuyo orden decide `arp_step_index` dan **10 órdenes distintas** de 16 pasos con 7 notas
  (los otros siete — RND, ORD, REPEAT, UP+8, WALK, CHORD, TRNS — los decide `arp_pick`). PEDAL sostiene la nota
  más grave en cada tiempo fuerte y sube por las demás; ALBERTI es exactamente 1-3-2-4; los dos se degradan
  bien con dos notas.
- **SCL**: HIJAZ y HUNG MINOR tienen **exactamente** los grados que sus nombres prometen, ninguna de las 18
  escalas repite el conjunto de otra, y el #4 de HUNG MINOR se cuenta desde la fundamental de la pista.
- **LFO**: 1/2D es **12×** el período de 1/32 y 1/16D **3×**, con el orden de velocidades correcto, y `RATE`
  acepta −14.

Lo que no prueba: cómo suenan ni qué hace un músico con ellos. Eso es del oído y del instrumento.

## 4. Verificación ejecutada (`[HOST]` y `[AC791N]`)

- `regress`: **307 golden renders, 0 cambiados, 0 desaparecidos**, 0 fallos de salud / voz / routing, 0
  entradas de CPU fuera de presupuesto, 0 crashes.
- `groups.sh ui`: 0 — barrido de pantallas y lint de alineación (3 787 506 medidas de 74 sitios, 0 fuera).
- `groups.sh presets`: 0 — 287 sonidos, 287/287 con una categoría primaria.
- `groups.sh web`: 0 — incluye editor con las tablas del firmware (52 listas), export MIDI (64 checks), sitio.
- `python3 tools/sync_editor_mock.py --check`: limpio. `tests/resources_test.py`: 13/13.
- `tests/target_budget.py`: exit 0. Build de destino completo (`.fwsc`, identidad FM-1_965).

## 5. Lo que ya existía (investigado antes de tocar nada) y lo que no entra

El instrumento ya trae, y no se duplicó: 10 formas de LFO (SIN TRi SAW SQR S&H RAMP EVO HSIN PULSE PLUK), RATE
en tiempo (12 divisiones), PHASE con opción FREE, FADE, SLEW/INVERT, cuatro destinos con cantidad; matriz MOD con
4 slots, 16 fuentes (LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, M1–M4, STEP, NOISE, PAT) y 27 destinos; macros y
MSEQ; ARP con 16 modos, OCT 1–4, GATE, SWING, PROB, HOLD, orden NOTE/PLAY y TRNS; SEQ con LEN/DIV/SWING/GATE,
HUMAN, probabilidad, condiciones (WHEN), PLAYHEADS, MUTATE, PHRASES; SLICER; meta-MSEQ. Es difícil señalar un
hueco grande: por eso lo agregado es preciso en vez de amplio.

Un **parámetro nuevo** (offset/unipolar de LFO, un quinto slot de MOD, microtiming por paso) exige campo nuevo en
`track_t` (RAM), en el formato de proyecto (compatibilidad) y en la UI, y **no entra** en los 3 432 B de XIP con
4 692 B de RAM: la etapa 5 se hizo con valores nuevos en listas existentes, que es lo que el presupuesto permite.

## 6. Pendientes para la Etapa 6

1. **Liberar XIP** sigue siendo el requisito de todo lo demás (3 432 B libres). Candidatos medidos: `UAC_FIR`
   (7 680 B), el código de UI (`draw_graph` 15 700 B, `draw_columns` 13 636 B), las tablas de iconos.
2. Con ~500 B libres más, la siguiente tanda natural es: SEQ con **condición de paso por probabilidad
   acumulada** y un LFO **unipolar/offset** — ambos necesitan un campo nuevo, así que primero hay que decidir
   dónde vive (un byte de `track_t` cuesta 6 B de RAM por pista y obliga a versión de formato de proyecto).
3. **`[HW]`**: nada de esta etapa se probó en la FM-1 física; los modos, escalas y divisiones nuevos se
   verificaron en host y en el build de destino.
