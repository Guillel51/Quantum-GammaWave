# FASE 6 — segunda optimización profunda (2026-11-16)

Parte del árbol publicado en `main` (`e96fbb4`, FASE 2). La restricción que mandaba era el **espacio XIP**:
416 B libres sobre un slot de 583 612 B, el margen más fino del proyecto. Esta pasada no toca sonido,
funciones, compatibilidad ni calidad visual, y todo lo que sigue está **medido**, no estimado.

## 1. Lo que cambió

### 1.1 La tercera banda del WAVETABLE se construye al arrancar (XIP −4 096 B)

`tools/gen_tables.py` guardaba en flash la tabla `WT_L5` (8 frames × 256 puntos × 2 B = 4 096 B): el límite
de banda de 5 armónicos, el que leen las notas por encima de ~1,4 kHz. Las otras dos bandas (64 y 16
armónicos) ya se construían al arrancar desde `WT_COEF` desde la FASE 1, con el mismo código
(`eng_wavetable.c`, `wt_gen_init`), para no gastar flash. La tercera había quedado guardada.

Ahora `WT_GEN` tiene las tres bandas (12 288 B de `.bss` en vez de 8 192 B) y `WT_L5` desaparece del
generador: **4 096 B de flash menos y 4 096 B de `.bss` más**. El costo es una sola pasada de arranque
(~0,3 ms en host), fuera de todo bloque de audio.

Comprobación de que el sonido no cambia:

- La tabla generada se comparó **antes** de cambiar el código contra la guardada, con la misma aritmética
  entera que usa el firmware (coeficientes Q30, tabla `SINE` Q15, normalización al pico 32 000): de 2 048
  valores, 972 difieren en **±1 sobre 32 000** (el redondeo del generador en coma flotante contra la ruta
  entera). Es 0,003 % del pico, en la banda de 5 armónicos.
- `tests/regress`: **293 renders golden, 0 cambiados, 0 perdidos**, 0 fallas de salud, 0 chequeos de voz o
  ruteo fallados, 0 entradas de CPU sobre presupuesto.

### 1.2 Los blitters de glifos no decodifican lo que no se ve (pantalla)

`gfx.c`:

- `cv_alpha` (la cara S, de 4 bits por píxel: todas las etiquetas y valores) calcula **una vez por glifo**
  las columnas que caen en el lienzo y salta las filas fuera del recorte, así que el lazo interior pierde
  el test de límites por píxel.
- `cv_alpha_hc` (caras M y L, comprimidas en Huffman, e iconos) no decodifica **nada** cuando la celda cae
  entera fuera del lienzo. Antes recorría el flujo completo para descartar las filas una por una: en una
  página que scrollea (los créditos) eso era casi todo el trabajo del cuadro.

Lo que se midió: los **293 renders golden de UI y las 190 pantallas × 22 paletas siguen idénticos** (0
hallazgos de lint, 0 desalineaciones de 3 554 331 medidas), y el costo de dibujo del barrido (`ui_render`,
`cc -O1` en este host, mediana de 5 corridas alternadas contra el binario anterior) baja:

| pantalla | antes (µs) | después (µs) | |
|----------|-----------:|------------:|---|
| about_credits | 228,4 | 191,2 | **−16,3 %** |
| roll_chords | 130,6 | 115,6 | −11,5 % |
| drumx | 133,5 | 126,0 | −5,6 % |
| menu | 121,7 | 116,7 | −4,1 % |
| drum | 125,2 | 120,4 | −3,8 % |
| reverb2 | 132,2 | 127,7 | −3,4 % |
| fx | 134,8 | 130,8 | −3,0 % |
| step | 118,8 | 116,3 | −2,1 % |
| **mediana de 12 pantallas** | | | **−3,2 %** |

Clase de la medición: **host x86, `cc -O1`, tiempo de CPU con el medidor del propio `ui_render`**, mediana de
5 corridas por binario alternadas para que el ruido de la máquina no se lea como mejora. El aparato no se
midió: no hay unidad.

Costo: +80 B de código XIP (el código de recorte). Se paga solo con lo del punto 1.1.

## 2. Lo que se revisó y **no** se cambió (con el número que lo decidió)

- **Código duplicado**: un detector de bloques idénticos de 6 líneas sobre los 53 848 renglones de
  `firmware/src` encuentra **2** bloques repetidos (dos pares de 6 líneas, en `ui_draw.c`/`ui_homeviews.c` y
  `gen.c`/`song_dice.c`). Consolidarlos ahorra menos de 100 B de código y agrega una llamada a dos rutas
  calientes: no vale el riesgo.
- **Cadenas literales**: el compilador ya las fusiona (una cadena que aparece en tres archivos está una vez
  en la imagen, comprobado sobre `build/felucca.bin`). No hay bytes que recuperar ahí.
- **RAM**: los objetos grandes son estado de funciones, no holgura (`trk` 10 560 B, `fm6_note` 7 104 B,
  `drums_kit`, `proj_slot`, `up_bank`…). Un detector de búferes muertos no encuentra ninguno; el alias de
  `proj_scratch`/`sec_stage`/`undo_st` ya se había evaluado y rechazado por vidas solapadas
  (`docs/OPTIMIZATION_PHASES.md`). `.data+.bss` queda en 94 772 B de 98 304 B.
- **POOL**: 311 256 B con 9 248 B de holgura sobre el piso de 8 KiB; no cambió. `dly_buf` (131 072 B),
  `sl_buf` y `cloud_buf` (32 768 B cada uno) son el largo de delay y el tamaño de las ventanas que las
  funciones prometen: recortarlos es quitar función.
- **Compilador**: `-Oz` no es un default heredado sino una medición (`tools/build.py`: a `-Os` aparecen 20
  divisiones por bloque en `analog_render`, +355 %; a `-O2` la imagen no entra). No se movió.
- **DSP**: los motores ya tienen el trabajo por bloque fuera del lazo (coeficientes, tablas, ramas) y las 39
  líneas de `tests/target_budget.py` quedan en **+0 %** con este árbol. No se cambió nada sin medición.
- **Renders**: `tests/target_budget.py` 39/39 a 0 %; `tests/memory_map_test.py` passed.

## 3. Números medidos (build AC791N real, `./build.sh`)

Toolchain `jieli-linux-toolchains-20260730.1`, SDK `AC79NN_SDK_V1.2.1_2023-12-13`.

| Región | Antes (`e96fbb4`) | Después | |
|--------|------------------:|--------:|---|
| Imagen (`build/felucca.bin`) | 583 196 B | **579 148 B** | −4 048 B |
| Holgura XIP (slot 583 612 B) | 416 B | **4 464 B** | ×10,7 |
| Código XIP | 419 352 B | 419 408 B | +56 B |
| Constantes XIP | 143 364 B | 139 260 B | −4 104 B |
| `.data + .bss` | 90 676 B | 94 772 B | +4 096 B (holgura 3 532 B) |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 684 B | 19 684 B | igual (holgura 4 892 B) |
| Pool (holgura) | 311 256 B (9 248 B) | 311 256 B (9 248 B) | igual |

El intercambio es deliberado: **4 KiB de flash, que era el recurso agotado, por 4 KiB de `.bss`, que tenía
7 628 B libres y queda con 3 532 B** dentro del presupuesto de 96 KiB.

## 4. Lo que quedó verificado y lo que no

Verificado: `./build.sh` con el toolchain y el SDK; `tests/regress` (293 renders golden idénticos, CPU por
bloque dentro de presupuesto); `ui_render` (190 pantallas × 22 paletas, 0 lint, 0 desalineaciones; golden de
UI idénticos); `tests/target_budget.py` 39/39 a 0 %; `tests/memory_map_test.py` passed; suite de host
completa (`NO_PACKAGE=1 ./tests/run_tests.sh`).

No verificado: **nada de esto se probó en una unidad FM-1** (no hay aparato) ni se recompiló el WASM del
emulador (no hay `emcc` en este entorno). El costo de dibujo es tiempo de host, no ciclos del AC791N.
