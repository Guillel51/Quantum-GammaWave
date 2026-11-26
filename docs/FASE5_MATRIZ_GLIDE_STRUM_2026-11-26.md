# FASE 5/6 — dos destinos expresivos nuevos: GLIDE y STRUM

Fecha: 2026-11-26. Base: `8eb5b37b` (FASE 4). Alcance: `firmware/src/mod.c`, `firmware/src/params.c`,
`firmware/src/core.h`, `tests/mod_test.c`, `tests/etapa7_audit_test.c`.

## 1. Qué se revisó antes de tocar nada

La investigación de las tareas 1 a 6 dio un resultado incómodo y útil: **el instrumento ya tiene casi todo lo que
se pedía**, y agregar más parámetros habría sido lo que las condiciones prohíben (redundancia). Lo que hay, medido
en las tablas:

| área | qué existe ya |
|---|---|
| formas de onda | ANALOG: 11 ondas (SAW SQR TRI SIN PWM SYNC RING SAW3 TUBE FOLD PM), osciladores con `blep()` (sin alias por construcción, `dsp.c`); WAVETABLE: MORPH + WARP + PHASE con cuatro modos (SINE, KNEE, HOLD, X2); LOFI: BYTE/FLOAT, reducción y decimado |
| tablas wavetable | WARP (profundidad), PHASE (modo), EVOLVE: la tabla deriva sola (`~1.4 s a un ciclo`), todo por página |
| armónicos y parciales | ADDITIVE: ocho niveles por parcial (DRAW / DRAW 2), SPECTRUM entre perfiles, TILT por armónico, RULE (perfil generado), DECAY espectral por parcial, SUB |
| modulación | matriz de **8 ranuras por pista** (4 almacenadas + 4 en `t->mx`), **17 fuentes** (OFF LFO ENV VEL KEY RAND MODW AT EXPR M1..M4 STEP NOISE PAT BAR), **32 destinos** antes de esta fase; LFO con forma, curva, fase, re-sync; envolventes con ATK/DEC/REL modulables |
| fase, movimiento, morph | PHASE (WAVETABLE), MORPH (ANALOG/PHASE/LOFI), MORPH A↔B de escenas, TILT, DETUNE/UNISON, ALLOC, START scatter |
| edición por motor | páginas propias: OPERATOR / OP ENV / OPERATOR 2 (FM6), KIT 3 / X-MOD / SOUND 1..4 / LEVELS / STEREO (DRUM), DRAW (ADDITIVE), PHASE (WAVETABLE), CHAR por motor, FILTER por familia |
| expresividad | GLIDE + GLMODE + PRIO, VEL/AT/PAT/EXPR como fuentes, MIDI CC1/CC11/aftertouch |

Lo que **no** existía: ningún destino de la matriz alcanzaba **GLIDE** ni **STRUM**. Eran los dos únicos parámetros
que el DSP lee en el ataque de la nota y que ninguna fuente podía mover nota a nota: el glide era un número fijo por
parte, y el strum del acorde también.

## 2. Lo implementado

Dos destinos, **añadidos al final** de la lista (los números guardados no cambian):

| número | nombre | qué mueve | rango del parámetro |
|---|---|---|---|
| 32 | `GLIDE` | el tiempo de portamento con que arranca la nota siguiente (`P_GLIDE`, 0..127) | 0..127, sumado con el AMT de la ranura |
| 33 | `STRUM` | la separación entre las notas del acorde, de grave a agudo (`P_CSTR`, ~0..63 ms) | 0..127 (usa la misma ranura que STRUM) |

`MD_N` pasa de 32 a 34 y `QG_DST_N` con él (`core.h`), con `_Static_assert` en `mod.c` para que la lista de nombres y
la enumeración no puedan divergir.

**Por qué estos dos y no otros.** Los dos se leen **una vez, cuando la nota ataca**: un offset por bloque los
convierte en algo que depende de la velocidad, la presión o el LFO **en ese instante**, que es exactamente lo que
hace musical a un glide (las notas suaves se deslizan, las fuertes no) y a un strum (el acorde se abre con la
presión). No agregan estado por voz, no agregan trabajo por muestra y no tocan el DSP: escriben el mismo byte que la
página ya escribe, así que la pantalla y la matriz no pueden contradecirse.

Recetas (documentación de la tarea 10):

- `VEL -> GLIDE +32`: legato con expresión; las notas suaves entran deslizando.
- `AT -> STRUM +40`: el acorde se abre bajo la presión del aftertouch.
- `EXPR -> STRUM -20`: la rueda de expresión cierra el acorde a bloque.
- `LFO (lenta) -> GLIDE ±16`: un portamento que respira sin cambiar el patch.

## 3. Verificación

- `mod_test` (compilado como lo compila la suite, `-DMOD_TSRC_COUNT=1`): **todas las comprobaciones pasan**,
  incluidas cuatro nuevas: `LFO -> GLIDE` sube `p[P_GLIDE]` en el bloque y el valor guardado vuelve tras `mod_end`;
  `LFO -> STRUM` lo baja y también vuelve; y los dos nombres existen para la interfaz (`mod_dst_name`).
- `etapa7_audit_test`: la auditoría que fija la longitud de `N_MDST` como parte del formato de preset pasa con la
  lista de **34** (antes 32); los números 0..31 conservan su significado, que es la condición del formato.
- `phase5_test`, `web/test_web.mjs`: **pasan**.
- `regress`: **316 renders dorados idénticos, 0 cambiados**, 0 fallos de salud, 0 de voces/ruteo, 1 fallo de
  presupuesto de CPU — el conocido de este host (reloj de 10 ms; el mismo que ya documenta la FASE 3), 0 notas por
  encima del +40 %. 67,0 s.
- [AC791N]: imagen **580 172 B** de 583 612 (3 440 libres), alcance de llamadas desde RAM 10 388 B (mínimo 8 192),
  RAM 75 664 / 98 304, POOL y LOWBUF iguales.

**No se escuchó**: este entorno no tiene salida de audio ni unidad. La prueba es numérica (el parámetro que lee el
ataque de la nota se mueve) y de regresión (los 316 renders no cambian porque ningún preset usa los destinos
nuevos). Decirlo así es la única forma honesta de entregarlo.

## 4. Pendientes registrados

- **El Web Studio no muestra los dos nombres todavía.** `web/test_web.mjs` valida la lista del editor contra el
  **emulador compilado** (`web/emu`), que en este entorno no se puede reconstruir: no hay `emcc`. Tocar sólo
  `editor.html` rompe esa prueba (lo comprobé: 2 fallos), así que el editor queda como está hasta que se reconstruya
  el WASM; el firmware y la interfaz del instrumento sí los muestran.
- **Presets de demostración**: no toqué las tablas de fábrica. Cualquier cambio de preset mueve `tests/golden.txt` y
  `tests/cpu_baseline.txt` (316 renders y los presupuestos por preset) y no quise regenerar esa red en el mismo
  commit que un cambio de formato. Las cuatro recetas de arriba están listas para cuando se haga a propósito.
- Siguen abiertos, con su medida, los de las fases 1 a 4: CLOUD 0,1 dB, `Gl` 0,258 px, los dos presets a 0,044, y
  las ocho pantallas con hueco muerto de la FASE 4.
