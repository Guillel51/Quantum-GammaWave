# Fase 4/8 — polifonía, seis pistas, multicore y eficiencia (AC791N)

Medido sobre `build/felucca.elf` y `build/felucca.dis` (clase **TARGET AC791N**, bytes e instrucciones
del enlazador) y, donde se dice, sobre **HOST x86-64** con el mismo código. No hay unidad física en esta
sesión: no hay ninguna cifra de ciclos en el FM-1.

## Qué se cambió, y por qué la medición lo justifica

| cambio | archivo | antes | después |
|---|---|---|---|
| `mc_split` cuenta las voces de todas las pistas en **un solo pase por el pool** (18 ranuras, `vown[]`), no con `mc_cost()` por pista (6 × 12 con la indirección `vpool[t->vidx[k]]`) | `firmware/src/fx.c` | coste estático **116** (96 instrucciones, 71 en bucles) | **97** (102 instrucciones, 70 en bucles), **−16,4 %** |
| `mc_job` no limpia `c1_mix` cuando el reparto no le dio ninguna pista (los buffers no se suman: `mc_join` sólo corre con máscara) | `firmware/src/fx.c` | 7 × 32 `int32` = **896 B de stores por bloque** en el núcleo 1, siempre | 0 cuando la máscara de pistas es 0 |
| `voice_alloc` / `voice_room` se ahorran el barrido de `NPART × NVOICE` = **72 ranuras** cuando `voice_live` (contador mantenido) prueba que el pool está por debajo del tope | `firmware/src/voice.c` | 2 barridos por nota nueva, con indirección | el barrido corre sólo si `voice_live >= voice_cap()` |

Las tres son exactas por construcción, no heurísticas:

- `vown[s]` dice quién posee cada ranura del pool y se mantiene en cada concesión y préstamo de ventana
  (`voice_window_grow`), así que el recuento por pista es el mismo que el de `V(t, k).active` sobre la
  ventana; las ranuras de una ventana que se encogió siguen teniendo dueño y se siguen contando.
- `voice_live` cuenta **todas** las voces activas (incluidas las que se están desvaneciendo), de modo que
  `voice_live >= voices_busy()` siempre: por debajo del tope el barrido exacto no puede cambiar la rama.
  Si el contador se quedara corto por un camino no instrumentado, el efecto sería el contrario al peligroso
  (se haría el barrido de más), y `voice_stats()` lo re-cuenta una vez por frame.

## Qué se midió antes de tocar nada (línea base, todo en verde)

- `tests/target_budget.py`: `BLOCK_PATH` **34 845** (presupuesto 33 437, +4 %), `mix_block` 268
  instrucciones (116 en bucles), `events_block` 1 798 (1 732 en bucles).
- `tests/alloc_bench.c` (HOST): nota repetida ~43-76 ns, nota nueva ~140-184 ns (la máquina de este
  host oscila ~1,3× entre tandas; por eso el antes/después de abajo se da en instrucciones del destino,
  no en ns del host).
- Polifonía y seis pistas, 158 comprobaciones en verde: `poly_stress_test` 12, `poly_budget_test` 21,
  `poly_voices_test` 18, `poly_parts_test` 26, `poly_phase2_test` 32, `six_track_live_test` 12,
  `morph_six_test` 6, `morph_depth` 31.
- `tests/regress.c`: 316 renders dorados, 0 cambiados.
- `tests/audio_stress.c`: p50 0,7 µs, p99 192,6 µs, máx 405,0 µs del presupuesto de 725 µs (HOST), 0
  muestras no finitas, 0 saltos por encima de 0,5.

## Después

- `mc_split` **116 → 97** (−16,4 %): además del coste estático, el bucle pasó de 72 iteraciones con dos
  cargas dependientes cada una a 18 sin indirección — del orden de **290 instrucciones menos por bloque**
  en el núcleo 0, dentro del ISR (contando los cuerpos del bucle en el desensamblado: ~6 instrucciones ×
  72 frente a ~8 × 18).
- `BLOCK_PATH` **34 845** sin cambio: el reparto no estaba en la lista de objetivos de
  `tests/target_budget.py`; ahora sí (`mc_split 97`) para que la próxima fase vea su crecimiento.
- Imagen AC791N: **581 172 B** (igual), XIP 456 140 + 120 252 constantes, RAM 86 072/98 304, pool 303 836 B
  con 16 672 B libres, LOWBUF 28 408/32 768, código en RAM 3 940 B, alcance de llamadas 8 980 B.
- Las 158 comprobaciones siguen en verde, 316 dorados idénticos, `audio_stress` igual.

## Lo que la medición descartó

- **Aumentar el pool o NVOICE**: el pool tiene 18 ranuras y 16 672 B libres en `.pool`, pero el coste por
  voz es el que ya mide `BLOCK_PATH`; subir la polifonía sin bajar el coste por voz choca con el
  presupuesto de 725 µs. No se toca.
- **Reparto por coste medido (`mc_tk`) frente a coste por voces**: la mezcla actual (ticks medidos +
  voces por motor, `MC_VW`) ya usa las dos cosas; el desequilibrio que queda viene del suelo de 1 voz por
  pista en los motores ligeros, no de la fórmula.
- **Quitar los barridos de `quiet_tail`/`voice_victim`**: sólo corren con el pool lleno (es donde se
  decide a quién se le quita la voz); ahorrarlos cambiaría la política, y la política es lo que las 158
  comprobaciones fijan.
