# Fase 4 — el bloque: trabajo repetido por pista, medido y recortado

Fecha: 2026-11-27. Clase de las cifras: **estáticas del AC791N** (`tests/target_budget.py` sobre
`build/felucca.dis`) y **reloj de host** (`cc -O2 -pg`, gprof). No hay FM-1 físico: no hay ciclos de unidad ni
reparto real entre núcleos, y el host de esta VM es ruidoso.

## 1. Qué se buscó

La pregunta era qué parte del bloque de audio se va en trabajo **repetido por voz o por pista** y no en coste
musical. Se midió, en este orden:

1. **Reparto por función** (gprof, `tests/regress.c`): el bloque se reparte entre `fx_buses` (11,0 %),
   `rev_room_core` (5,0 %), `mix_part` (6,3 %) y `track_render` (4,8 %); el resto son los motores por muestra
   (`wt_render`, `noise_render`, `fm6_op_run`, `drum_render`, `pd_wave` …). `mix_part` y `track_render` se
   llaman **17 536 428** veces en 2 922 738 bloques: exactamente **6 por bloque**, uno por parte (NPART = 6).
2. **Prólogo por parte.** El prólogo de `track_render` convertía TUNE a las unidades de los motores y el BEND a
   semitonos/fracaso con divisiones enteras, por parte y por bloque.
3. **Barrido por voz.** El bucle recorre NVOICE = 12 ranuras por parte y bloque (72 visitas), de las que en la
   escena de seis pistas hay ~10–12 con voz: el resto es un `if (!v->active) continue` de ~5 instrucciones.
4. **El índice de pista.** `(t - trk) % NTRK` aparecía en 16 sitios del camino caliente.

## 2. Qué se midió, y qué no dio

**TUNE no era redundancia de verdad.** La conversión (dos divisiones por parte) ya estaba hoisteada por el
compilador: se movió a una función por bloque (`voice_tune_block`) y `events_block` siguió con **12 divides**,
`+96 B` de XIP y **0** de ahorro. Se revirtió. El caché dentro de `track_render` era peor: la función se
inlinea dos veces (`mix_part_m`, `mix_part_m0`) y el árbol **no linkeó** (XIP overflow por 1 272 B).

**El barrido por voz es pequeño.** 72 visitas por bloque, ~60 inertes × ~5 instrucciones ≈ 360 instrucciones
≈ **1 %** del bloque. Acotarlo por `trk_nvoice(t)` no es exactamente equivalente (si el tope baja mientras
suena una voz, esa voz quedaría sin renderizar ni matar), así que **no se tocó**.

## 3. Qué se recortó

`NTRK == NPART == 6` (`core.h`) y **todos** los llamadores del camino de mezcla tienen la dirección de un
elemento de `trk[]`: `mix_part` corre desde `&trk[i < NPART]`, `track_render` desde `mix_part`, los motores
desde `track_render`, `seq_tick` desde `&trk[i < NTRK]`. El índice **ya es** la ranura del array, y el
`% NTRK` que se pagaba en cada uno de esos sitios era una **división entera por pista y por bloque** para
mapear un valor que ya estaba en rango sobre sí mismo.

`trk_index()` existía en `seq.c` con exactamente esa cuenta; se movió a `core.h` (junto a `trk[]`, para que la
mezcla la vea) y se usó en los 16 sitios del camino caliente (fx.c 6, voice.c 8, eng_drum.c 1, fugue.c 1).
Misma aritmética, misma entrada, mismo resultado: no es una aproximación.

## 4. Resultado (AC791N, estático)

| | antes | después |
|---|---|---|
| `events_block` | 34 729 | **33 977** (−752, −2,2 %) |
| `BLOCK_PATH` (mezcla + eventos + 4 redes de reverb) | 35 132 | **34 380** (−752, −2,1 %) |
| divides dentro de `events_block` | 12 | 11 |
| imagen | 581 700 B | **581 572 B** (−128) |
| XIP código | 456 660 B | **456 540 B** (−120) |

## 5. Verificación

- `tests/regress`: **316 renders, 0 cambiados**, 0 fallos de salud, 0 de voz/ruteo, 0 de presupuesto de CPU.
- `tests/target_budget.py`: exit 0, sin FAIL ni notas; `BLOCK_PATH` re-basado a 34 380 con el porqué escrito.
- `tools/dsp_bench.py --compare`: exit 0.
- Build AC791N: `ok`, RAM 86 072/98 304, pool spare 16 672 B, LOWBUF 28 408/32 768.

Lo que **no** está medido: ciclos en la unidad (no hay FM-1). El −2,2 % es un recuento estático ponderado del
destino, no una promesa de tiempo.

## 6. Lo que queda para la fase 5

El barrido de NVOICE por parte y bloque (~1 %) pide un contador de ranuras vivas por pista mantenido en
`voice_alloc`/`voice_kill`: es la única forma de acotarlo sin el riesgo de dejar una voz colgada. Medido, es
lo único repetido que queda con peso; el resto del bloque es coste musical.
