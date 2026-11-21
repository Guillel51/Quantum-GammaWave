# FASE 6 — las ventanas angostas: medida, implementada, y el XIP que falta

Objetivo de esta tanda: recuperar presupuesto de XIP (quedan **888 B** en la imagen de v1.5.27) para que entre la
caché de ventanas angostas que la Fase 4 midió y rechazó, y volver a medirla.

Resultado corto: **la caché está implementada y mide exactamente lo que se buscaba** (21 440 px por frame quieto,
−28,4 %), con **0 píxeles cambiados** en el arnés; pero **no entra en el enlace**: el build de destino desborda
XIP por **2 752 B**. Queda detrás de `FELUCCA_WINCACHE`, apagada por defecto, y este documento dice por qué no
alcanza con recortar "los 2 752 B" y qué hay que liberar de verdad.

## 1. La caché que entra con este código, no la de 3,9 KB

La tabla de la Fase 4 (`cv_side`) guardaba un hash **por fila y por ventana** más la invalidación por rectángulo:
~3 900 B de código. Esta versión guarda **un hash de 32 bits por ventana completa** (FNV-1a sobre las palabras del
canvas, el mismo que ya usa una fila) para las últimas `CV_WIN_N = 4` ventanas dibujadas, y la descarta cuando algo
la pisa —otro canvas, una banda de 240, un relleno— o cuando el panel se durmió (`gfx_panel_forget`). Una ventana
que cambió se manda entera (3 192 px por columna de EDIT); una que está igual **no se manda**.

Los cuatro slots viven en `.noinit` (ventana propia del linker: no mueven ningún global de `.bss`) y se dan por
vacíos mientras `cv_rowh_on` diga que ninguna banda de 240 los ha refrendado.

## 2. Medición `[HOST]`

`tests/ui_render.c --idle` sobre las escenas quietas (191 pantallas × 22 paletas):

| | px escritos por frame | px cambiados |
|---|---|---|
| árbol de v1.5.27 (flag off) | 29 952 | **0** |
| con `FELUCCA_WINCACHE=1` | **21 440** | **0** |

−8 512 px por frame quieto (−28,4 %), el mismo número que la tabla de la Fase 4 consiguió con 3,9 KB de código.
Los 0 cambiados son la prueba de que el panel no recibe un píxel distinto: si la caché se equivocara en una fila,
ese contador subiría. `tests/ui_test.c` pasa también con el flag encendido (`ui: all passed`).

## 3. El enlace `[AC791N]`: 2 752 B de más, y por qué no son 2 752 B de recorte

```
FELUCCA_WINCACHE=1 ./build.sh
region `XIP' overflowed by 2752 bytes
```

Con 888 B libres, la caché cuesta en total **3 640 B**, no los ~500 B que ocupa ella. La causa está medida y no es
la caché: en este árbol **cualquier cambio de código re-decide el tamaño de casi todas las funciones**. La prueba es
un `cv_blit` con una llamada de diez instrucciones y un `return` (cuerpo de la función vacío, `return 0`):

| build | resultado |
|---|---|
| base v1.5.27 | 582 724 B, **888 B libres** |
| + `if (cv_win_seen(x, y)) return;` (función `return 0`) | **+2 592 B** de XIP |
| + la caché real (hash + 4 slots) | +3 640 B |
| + `.noinit` en vez de `.bss` para la tabla | 2 752 B de desborde (sin cambio) |
| + `noinline` en `cv_blit` | 2 752 B (sin cambio) |
| + `minsize` en `ui_graph.c` y `gen.c` | 1 760 B de desborde (recupera 1 024 B) |
| + `minsize` en todos los `.c` | 2 480 B de desborde (no alcanza y cuesta CPU) |
| + `-mllvm -inline-threshold=400` | **+165 524 B** (descartado) |

Comparando los dos `.o`: **306 de las 1 645 funciones** cambian de tamaño, mediana **+4 B**, máximo +370 B (p. ej.
`graph_arp` pasa de 905 a 924 instrucciones). No es la tabla, no son los globales (probado con sección propia), no es
un inlining concreto: es la pasada de tamaño del `pi32v2` tomando otras decisiones por el solo hecho de que el IR
cambió. La Fase 2 ya lo había visto desde el otro lado: **+120 B de struct costaron +2 952 B de código**.

Conclusión honesta: para que esta caché entre hay que liberar **~3,6 KB** de XIP, y recortar código *dentro del mismo
parche* no sirve porque el mismo efecto se come el recorte.

## 4. Lo que ya está descartado por medición (no volver a probarlo)

- **Assets**: `UAC_FIR` ya guarda 80 de 160 fases (la simetría del prototipo, explotada); los keycaps y las fuentes
  están a 4 bpp; el RLE de los keycaps ahorra 1 byte; `-Os` en vez de `-Oz` desborda 33 KB (`docs/FLASH_RECLAIM.md`).
- **`.hot_text`/`.ram_text`**: su copia vive una sola vez en flash; sacar funciones de `HOT_FNS` no libera XIP, sólo
  las devuelve a `.text` y les quita la RAM.
- **Duplicados de código**: 19 ventanas de 12 instrucciones compartidas en 2 438 funciones; no hay 4 KB ahí.

## 5. El camino que sí tiene números

La única pieza grande y reversible es **descomprimir una fuente en el arranque** en vez de decodificarla por glifo
en cada dibujo:

| fuente | flash hoy | si se decodifica una vez al boot |
|---|---|---|
| S (12 px, 4 fases) | 7 293 B de datos + 2 470 B de tabla + 1 437 B de kerning | −7 293 B de XIP, **+8 431 B de `.bss`** (la forma en nibbles) |
| L (18 px, 2 fases) | 4 270 B de datos + 574 B de tabla | −4 270 B de XIP, **+5 0xx B de `.bss`** |

RAM libre hoy: **11 392 B**. La fuente L sola libera 4 270 B (más que los 3 640 que hacen falta) y deja ~6 KB de
RAM; además el dibujo de texto deja de gastar el Huffman por glifo, así que **no cuesta CPU: la ahorra**. Es la
sesión siguiente: decodificador de una pasada al `gfx_init`, apuntar `aafont_t` a la copia en RAM, y los 191
pantallas × 22 paletas + `--idle` como prueba de que no cambia un píxel.

## 6. Cómo se reproduce todo esto

```
FELUCCA_WINCACHE=1 ./build.sh                 # el desborde de 2 752 B, con el toolchain y el SDK puestos
cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/ui_render tests/ui_render.c -lm
./build/ui_render --idle                      # 29 952 px
cc -O1 -w -DFELUCCA_WINCACHE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/ui_render_w tests/ui_render.c -lm
./build/ui_render_w --idle                    # 21 440 px, 0 cambiados
```

El flag está en la lista de `tools/build.py`, así que llega al compilador del target como cualquier otro
(`FELUCCA_SIZE`, `FELUCCA_UAC`, …). Apagado, la imagen es la de v1.5.27 byte por byte: 582 724 B, 888 B libres.
