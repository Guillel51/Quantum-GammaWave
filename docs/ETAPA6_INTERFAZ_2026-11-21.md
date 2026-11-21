# Etapa 6/8 — interfaz: lo que se revisó, lo que se cambió y por qué no se rehizo todo

Clases: **`[HOST]`** = `tests/ui_render` (x86, los mismos caminos de dibujo del firmware) y los PNG de
`build/qt9/`; **`[AC791N]`** = el build del destino. Nada `[HW]`.

## 1. Punto de partida medido, no supuesto

Antes de tocar nada se renderizaron **191 pantallas × 22 paletas** (más el barrido página/valor y el modo LARGE) y se
miraron las hojas de contacto en `build/qt6/sheet_GAMMA.png` y las ampliaciones. El estado real del repo **no es una
UI plana**: ya hay tarjetas con degradado, biseles, gráficos por motor (ADSR, algoritmo FM, transferencia del
MASTER, teclado de SCL, roll, rejilla de DRUM), cabecera con transporte, LEDs, iconos y una paleta por sección. El
lint de esa corrida ya daba **0 hallazgos y 0 desalineaciones** (3 787 506 medidas de 74 sitios).

Lo que sí estaba flojo, visto en las capturas:

| Pantalla | Lo que se veía |
|---|---|
| **CHANCE** (SEQ, paso a paso) | la tarjeta era el **piano roll** con la pantalla casi vacía: la probabilidad de los 16 pasos —lo único que la página trata— era invisible; sólo se leía el paso del cursor en el knob |
| **MOD MAP** | cuatro filas de rutas arriba y **media tarjeta vacía** abajo; la suma de las rutas a un mismo destino no existía (sólo un chip con el número de veces) |

## 2. Cambios hechos

### CHANCE — la probabilidad del patrón, dibujada (`graph_chance`, `ui_graph.c`)

- 16 columnas (la página del cursor) medidas desde una línea de piso, con **guías de 25 / 50 / 75 / 100 %**,
  separadores cada cuatro pasos y el pozo de cada columna visible tras la barra.
- El color dice la probabilidad: `T_THEME` al 100 %, `heat_col(0..200)` más cálido cuanto más baja, con un remate
  de 1 px mezclado con el texto para que la altura se lea también con el brillo.
- **RATCH** como pips bajo la barra (1..4), el cursor enmarcado y el paso que suena marcado en el piso.
- Los pasos fuera de `LEN` quedan como muñones, como en las barras de STEP.

### MOD MAP — dónde cae la modulación

- Bajo las rutas, una banda nueva: por cada destino asignado, una **barra bipolar con la suma de las cantidades**
  de los slots que lo mueven (hasta cuatro), con el nombre del destino y el cero marcado. Sin rutas, la banda lo
  dice: `NO ROUTES`.
- Sigue siendo de lectura: los slots se editan en MOD.

## 3. Coste `[AC791N]`

| | antes | después | Δ |
|---|---|---|---|
| imagen (app slot) | 580 180 B | **581 300 B** | **+1 120 B** |
| libre en el slot | 3 432 B | **2 312 B** | −1 120 B (piso 2 048) |
| `.data+.bss` | 93 612 B | 93 612 B | **0** |

### El bug que encontró el barrido

La primera versión de la banda de MOD MAP recorría `for (d = 1; d < N_MDST; d++)`. `N_MDST` **es la tabla**, no su
tamaño: con `-w` (las advertencias que usa esta suite) la comparación se compiló como puntero y el barrido de
pantallas **se colgaba** en la primera paleta después de GREY (36 de 3 056 PPM, CPU al 100 %). Se midió bisecando
con `git stash` (sin el cambio: 3 056 en menos de 100 s), se cambió el tope a `sizeof N_MDST / sizeof N_MDST[0]`
y volvió a 0 hallazgos. Sin el barrido completo, ese `for` habría entrado al firmware.

## 4. Verificación `[HOST]`

`tests/ui_render`: **191 pantallas × 22 paletas + barrido página/valor + LARGE**, **0 hallazgos de lint**,
**0 desalineaciones** (3 787 294 medidas de 74 sitios), 0 tintas sobre su propio color, 0 píxeles fuera de
gris/neutro en GREY/MONO (`build/qt9/report.txt`). `tests/ui_test`: **all passed**. `tests/regress`: **307 golden
renders, 0 cambiados**, 0 fallos de salud/voz/routing. Los PNG están en `build/qt9/<paleta>/` y las hojas en
`build/qt9/sheet_<paleta>.png`.

## 5. Lo que **no** se hizo, y la razón medida

El pedido es un rediseño de las 23 pantallas listadas. Este repositorio ya viene de seis fases de rediseño y el
presupuesto lo dice: **2 424 B libres** de XIP (piso 2 048) y **4 692 B** de RAM. Dos gráficos nuevos costaron
**1 008 B**. Repintar veintitrés pantallas con degradados nuevos, animaciones y estados implica reescribir decenas
de miles de instrucciones de dibujo (`draw_graph` sola son 15 704 B, `draw_columns` 13 636 B): **no cabe**. Habría
que liberar XIP primero (los pendientes de las etapas 1–5: `UAC_FIR` 7 680 B, las tablas de iconos, el código de
UI común), y eso es trabajo de otra etapa, no un retoque de esta.

Por eso esta etapa es **precisa en vez de amplia**: dos pantallas con un hueco real y medido, corregidas con datos
que ya existían y sin tocar el resto, que ya pasaba el lint y la alineación al píxel.

## 6. Pendientes

1. **XIP primero**: 2 424 B es el mínimo con el que se puede trabajar; el siguiente paso es liberar los ~7 KB de
   `UAC_FIR` o medir `draw_graph`/`draw_columns` para código común.
2. Con ese margen: **HOME OVERVIEW** (la lista de 6 pistas puede llevar una mini-actividad por pista ya dibujada
   en el mezclador), **EDIT por motor** (una fila de contexto con los ocho knobs y su curva) y **SAVE/LOAD** (la
   barra de ocupación del banco ya existe en PROJECT; falta en SAVE).
3. **Web Studio**: esta etapa no cambió el editor; su mock y sus tablas ya se sincronizaron en la Etapa 5
   (`tools/sync_editor_mock.py --check` limpio, `groups.sh web` en 0).
4. Nada `[HW]`: la FM-1 física no está disponible aquí; todo lo anterior es render del host por los caminos del
   firmware y build del destino.
