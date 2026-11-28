# FASE 1/4 — el margen XIP, medido y recuperado (2026-11-28)

Clase de medición: **build AC791N** (toolchain pi32v2, `./build.sh`, `build/felucca.map`) y **host x86**
(`tests/run_tests.sh`). **Ninguna cifra de este informe sale de la unidad física**, que no se tocó.

## 1. Punto de partida, medido en HEAD y en limpio

```
rm -rf build && ./build.sh
XIP: code 457 816 B + constants 118 948 B = 576 764 B from 0x2000120
RAM: .data+.bss 90 628 B of 98 304 B (7 676 B free)
image: 581 544 B of 583 612 B (2 068 B free in the app slot)
```

El gate del repositorio (`tests/resources_test.py`, `tools/mem_budget.py`) exige **2 048 B** libres en el slot y
4 096 B libres de RAM: el árbol estaba **20 B por encima del mínimo de XIP**, sin margen para nada.

## 2. Dónde no estaba el problema

| comprobación | resultado |
|---|---|
| banderas de tamaño | ya eran `-Oz`, `-ffunction-sections`, `-fdata-sections`, `--gc-sections` |
| alineación del linker | 554 de 589 secciones `.rodata` contiguas; **462 B** de huecos en total |
| secciones muertas | `.syms`/`objdump -t` dan tamaño 0: los tamaños reales están en `build/felucca.map`; el audit da 0 secciones muertas |
| tablas de preset | ya empaquetadas por diferencia (10 404 → 7 680 B) y las dos libres de punteros ya van en LZ4 |
| fuentes | Huffman de 4 bits por glifo, 8 fases (S: 8 920 B de 10 212 B en nibbles) |
| `-fno-jump-tables` | **peor**: +1 520 B |
| `-fno-unroll-loops` | idéntico (a `-Oz` no desenrolla) |
| outliner de máquina | el toolchain lo rechaza |

`part_dice` (6 846 B) y `gen_make` (9 974 B) se comprobaron llamador por llamador: son funciones, no código
muerto, y no se tocaron.

## 3. Dónde sí estaba: las copias del inliner

`-Oz` no impide que el inliner copie un cuerpo entero dentro de cada llamador, y en un árbol de 2 203 funciones
con pantallas de tamaño de página llamadas desde una docena de sitios eso suma kilobytes.

| intento | imagen | libre | presupuestos de audio |
|---|---|---|---|
| HEAD | 581 544 B | 2 068 B | verdes |
| `-fno-inline-functions` (árbol entero) | 578 296 B | 5 316 B | **rojo**: `DRUM_PATH` 1 980 / 1 153 (+72 %) |
| `noinline` en todo lo que no alcanza una interrupción (pase de IR) | 582 748 B | 864 B | verdes |
| `alwaysinline` de vuelta en todo lo alcanzable por interrupción | — | — | el compilador no termina (expansión exponencial) |
| `-mllvm -inline-threshold=1` | 576 568 B | 7 044 B | **rojo**: `dx_run` 1 478 / 426, `events_block` +39 % |
| **`-mllvm -inline-threshold=1` + las dos primitivas del DSP** | **578 024 B** | **5 588 B** | **verdes** |

El último renglón es el que entra.

## 4. Qué se cambió

- `tools/build.py`: `-mllvm -inline-threshold=1` en `CFLAGS`. El inliner sigue copiando lo trivial (las
  primitivas de un par de instrucciones) y deja de copiar los cuerpos grandes de las pantallas.
- `firmware/src/dsp.c`: `clamp` y `mulq15` quedan con **dos** formas — `clamp_i`/`mulq15_i`, `always_inline`,
  y una copia fuera de línea con el nombre de siempre.
- `firmware/src/engines.c` y `firmware/src/felucca.c`: `#define clamp clamp_i` / `#define mulq15 mulq15_i`
  alrededor del bloque de sonido (los motores, `mod.c`, `voice.c`, `fx.c`, `seq.c`, `audio.c`), con su `#undef`
  al terminar. Las pantallas, debajo, usan la copia única: pueden permitirse la llamada.

Ninguna función se elimina, ninguna voz, resolución, tabla ni umbral cambia. Los `#define` son de compilación:
el dispositivo ejecuta exactamente las mismas expresiones.

## 5. Resultado

| ventana | v2.20.4 | v2.21.0 |
|---|---|---|
| código en XIP | 457 816 B | 454 304 B |
| constantes en XIP | 118 948 B | 118 940 B |
| XIP total | 576 764 B | 573 244 B |
| imagen | 581 544 B | 578 024 B |
| **libre en el slot** | **2 068 B** | **5 588 B** |
| `.data+.bss` | 90 628 / 98 304 | 90 608 / 98 304 |
| POOL libre | 16 672 B | 16 672 B |
| LOWBUF libre | 4 360 B | 4 360 B |
| NOINIT libre | 76 B | 76 B |

Margen recuperado: **3 520 B**. Objetivo de la fase (4 KiB libres): cumplido, con 1 492 B de sobra. El mínimo
de 2 048 B no se movió.

## 6. Presupuestos de ciclo (`tests/target_budget.py`, build AC791N)

| camino | v2.20.4 | v2.21.0 | presupuesto |
|---|---|---|---|
| `DRUM_PATH` | 1 155 | **1 018** | 1 153 |
| `BLOCK_PATH` | 34 245 | 34 529 (+0,8 %) | 34 380 (+10 %) |
| `FM6_PATH` | 404 | 404 | 404 |

`DRUM_PATH` **baja**: los lazos del DRUM conservan los inlines que necesitan (`clamp_i`/`mulq15_i`) y además
dejan de arrastrar copias que no usaban. `BLOCK_PATH` sube 284 instrucciones ponderadas (0,8 %) y sigue dentro
del límite del 10 %; el motivo es el mismo interruptor: algunos ayudantes del bloque de mezcla ya no se copian
dentro de `events_block`.

## 7. Qué se ejecutó

- `./build.sh` en limpio (rc=0), `tests/target_budget.py` (rc=0), `tests/resources_test.py` (13/13),
  `tools/mem_budget.py`, `tools/mem_map.py`.
- `NO_PACKAGE=1 tests/run_tests.sh` completo en host x86: `ALL HOST TESTS PASSED`, con los renders dorados y
  `tests/ui_pixels.txt` rebasado sólo en los frames del splash y ABOUT que dibujan la versión.

## 8. Lo que queda pendiente

- Un corte de 4 KiB más para las próximas fases tendrá que salir de código, no de banderas: el inliner ya no
  tiene copias grandes que dar. El inventario del map sigue señalando `ui_input_frame` (30 754 B),
  `fm1_main` (18 076 B), `draw_columns` (14 168 B) y `graph_tech` (10 146 B) como los candidatos estructurales.
- `.pool` (303 833 B de constantes de 32 bits) es el mayor consumidor individual del mapa; reducirlo exige
  cambios de código en los sitios que repiten constantes, no una bandera.
