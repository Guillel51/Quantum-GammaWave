# Fase 4/8 — el lazo de ROOM por muestra (AC791N + HOST)

Clase de cada número: **AC791N** = bytes e instrucciones del enlazador y del desensamblado de
`build/felucca.elf` / `build/felucca.dis`; **HOST x86-64** = `cc -O2` (y `cc -O2 -pg` para `gprof`) en este
VM. No hay unidad física en esta sesión.

## Lo que costaba, leído del desensamblado

El lazo de `rev_room_core` (4 combs + 2 allpasses por muestra) recorría los combs con un `k` de 0..3 y
accedía al estado a través de `fx`, que es RAM normal. El compilador no puede probar que los `int16_t` que
escribe en las líneas de los combs no tocan `fx`, así que en cada iteración y por cada comb **cargaba
`fx.comb_i[k]` dos veces** (`200c168` y `200c16c`), escribía la posición de vuelta, y volvía a leer
`REV_COMB[k]` para el límite. Con 4 combs por muestra eso es tráfico de memoria de sobra.

## El cambio

`rev_room_core` (firmware/src/fx.c): las seis posiciones (`comb_i[4]`, `ap_i[2]`) y los cuatro estados del
pasa-bajos (`comb_lp[4]`) viven en locales durante todo el bloque y se escriben de vuelta una vez al final;
los cuatro combs y los dos allpasses van desenrollados con las direcciones base y los límites como
constantes. **La aritmética es la misma expresión, en el mismo orden, muestra a muestra** — por eso los
renders son idénticos bit a bit.

## Antes / después, AC791N

| medida (AC791N) | antes | después |
|---|---|---|
| `rev_room` coste de `target_budget.py` | **245** | **152** (−38 %) |
| `rev_room_soft` (la misma red, por el knee de CLOUD) | **225** | **135** (−40 %) |
| instrucciones de `rev_room` | 94 | 196 |
| **instrucciones ejecutadas por muestra** (contadas en el bucle exterior del desensamblado) | **195** | **152** (−22 %) |
| `BLOCK_PATH` (ahora incluye las cuatro redes de ROOM) | **35 315** | **35 132** (−183, −0,5 %) |
| imagen | 581 172 B | 581 700 B (+528) |
| código XIP | 456 140 B | 456 660 B (+520) |
| alcance de llamadas | 8 980 B | 8 460 B (≥ 8 192, sigue ok) |

El coste de `target_budget.py` pondera las instrucciones de bucle por profundidad de anidamiento
(`NEST ** (d-1)`), así que al desaparecer el bucle interno de 4 vueltas su caída (−38 %) exagera la mejora
real; el número honesto es el de instrucciones por muestra medido en el desensamblado: **195 → 152, −22 %**.
Los dos apuntan en la misma dirección.

`BLOCK_PATH` no se movía antes de este cambio porque sumaba **solo** `mix_block` y `events_block`: la
reverb corre en `fx_run`, tres niveles por debajo (`mix_block → fx_run → rev_room`), fuera de esa suma. Se
añadieron las cuatro redes a la lista y a `FUNCS` para que el bloque las cuente.

## Antes / después, HOST x86-64 (`gprof`, `cc -O2 -pg`, `tests/regress.c`)

| | antes | después |
|---|---|---|
| `rev_room_core` (self) | **1,68 s = 7,30 %** | **1,20 s = 5,36 %** (−29 %) |
| `fx_buses` (self) | 7,11 s = 10,56 % | 9,54 s = 10,23 % |
| total del run | 43,5 s | 42,4 s |

`fx_buses` es el lazo de picos y el chorus/delay, que este cambio no toca: se queda igual (la variación es
ruido de la máquina). La caída está donde se trabajó. El gate de CPU del host falla en **las dos** corridas
(el binario `-pg` es más lento): no es una regresión de este cambio.

## Renders dorados y estrés (HOST)

- `tests/regress.c`: **316 renders dorados, 0 cambiados**, 0 fallos de salud, 0 de voz/routing, **0
  fallos de presupuesto de CPU**, 0 notas por encima de +40 %, 0 crashes. Sin `-pg`: 36,6 s.
- `tests/audio_stress.c 8`: todas las comprobaciones pasan; p99 **182,2 µs** y máx **381,1 µs** de 725 µs
  (antes del cambio: 192,6 / 405,0 µs, misma clase y misma máquina ruidosa: se da como indicio, no como
  prueba).
- `six_track_live_test` 12 ok, `poly_six_test` 0 FAIL.

## Lo que no se tocó

`rev_room_mod_core` (ROOM modulado, con lectura interpolada de dos muestras) conserva su lazo: sólo corre
con RMOD/WIDE activos y su acceso es distinto (dos lecturas por comb), así que no comparte la mejora y no se
presenta como si la compartiera. `rev_spring`, `rev_cloud` y `rev_shimmer` quedan como estaban.
