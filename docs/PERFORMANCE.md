# Perfilado del bloque de audio (Fase 1/4, v1.8.0)

Clase de cada número, dicha como manda `AGENTS.md` §4:

| número | de dónde sale | qué prueba |
| --- | --- | --- |
| % por función | `gprof` sobre `tests/fx_bench` compilado con `-pg -O2`, host x86_64 | cuánto del bloque se lleva cada función **en el host** |
| ns/muestra | `tests/fx_bench` (reloj de pared, mejor de N) | coste del bloque **en el host** |
| costo estático | `tests/target_budget.py` sobre `build/felucca.dis` (pi32v2) | trabajo por muestra del código **que emite el compilador del target** |
| RAM / flash | `tests/resources_test.py` sobre el mapa del linker | presupuesto de memoria del target |

Ninguno de estos números es una medición en un FM-1, y ninguno es un margen de CPU del aparato: para eso hace
falta la unidad, y esta fase no la tuvo (regla §3: no se escribe en la unidad del dueño).

## Reproducir

```bash
cc -O2 -pg -w -Ibuild/gen -Ifirmware/src -o build/fx_prof tests/fx_bench.c -lm
(cd /tmp && /ruta/al/build/fx_prof 6)
gprof -b -p build/fx_prof /tmp/gmon.out | head -25
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/fx_bench tests/fx_bench.c -lm && ./build/fx_bench 7
```

## El perfil (v1.8.0, 6 repeticiones, 127 152 bloques)

| % | self | llamadas | µs/llamada | función |
| --- | --- | --- | --- | --- |
| 18,6 | 0,08 s | 127 152 | 0,63 | `fx_buses` (coro + delay + reverb + gate) |
| 16,3 | 0,07 s | 127 152 | 0,55 | `rev_room` (4 combs + 2 allpass) |
| 11,6 | 0,05 s | 4 068 864 | 0,01 | `master_out` (32 veces por bloque) |
| 11,6 | 0,05 s | 762 912 | 0,07 | `mix_part` (6 por bloque) |
| 9,3 | 0,04 s | 98 460 | 0,41 | `fm6_render` |
| 7,0 | 0,03 s | 228 456 | 0,13 | `fm6_op_run` |
| 7,0 | 0,03 s | 127 152 | 0,24 | `mix_block` (propio) |
| 4,7 | 0,02 s | 127 152 | 0,16 | `events_block` (secuenciador) |
| 4,7 | 0,02 s | 29 412 | 0,68 | `analog_dispatch` |

Lectura: **los buses de FX y la reverb son el 35 % del bloque**; los seis motores juntos, alrededor del 25 %;
el secuenciador, menos del 5 %. Eso es lo que hay que atacar primero si alguna vez hace falta margen, y no es
donde se había estado mirando.

## Intentos medidos y descartados (para no repetirlos)

1. **Fundir el vúmetro del master con el loop de salida** (un pase menos sobre el bloque). A/B: 81,3 vs 79,1
   ns/muestra (peor) y +46 en el costo estático del target: el compilador ya encadenaba bien los dos pases y
   el registro extra por muestra costaba más que el pase ahorrado.
2. **`always_inline` sobre `master_out`** (32 llamadas por bloque). A/B: 78,5 / 80,5 / 89,7 contra
   78,6 / 79,1 / 79,5. Sin ganancia y con código más grande dentro del ISR.
3. **Sacar la ganancia de damping de la reverb fuera del loop** (era `32767 - damp`, cuatro veces por
   muestra en los combs). El compilador del target ya la sacaba: `rev_room` 243 → 243. Se descartó para no
   ensuciar el árbol con un cambio que no se puede medir.

## El cambio que sí quedó

`events_block()` fuera de `mix_block` (`noinline`). El gate de coste estaba en rojo por esto: el secuenciador
entero estaba dentro de la función del bloque y el baseline de 1.4.5 ya no describía el código. Ahora
`mix_block` mide 115, `events_block` 45 589 y `BLOCK_PATH` —la suma, línea nueva— 45 704, así que mover
trabajo entre las dos no puede esconder un total crecido. Host: neutro (79,4 / 79,3 / 78,4 vs 78,8 / 79,5 /
79,6 ns por muestra). Audio: 307 renders golden idénticos. UI: 3 056 frames idénticos.
