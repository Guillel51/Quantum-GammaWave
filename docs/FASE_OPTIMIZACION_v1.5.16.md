# Fase de optimización y corrección — v1.5.16 (2026-11-20)

Clases de evidencia: **host x86-64** (UBSan/ASan, `ui_test`, `regress`, `ui_render`), **build AC791N** (imagen,
mapa de enlazado, desensamblado pi32v2). **No hay unidad FM-1**: nada de este documento afirma nada sobre el
panel, el DAC ni el comportamiento físico.

## 1. Punto de partida (heredado de v1.5.15)

| región | tamaño | usado | libre |
|---|---:|---:|---:|
| XIP (slot de la app) | 583 612 | 580 328 | **3 284** |
| RAM `.data+.bss` | 98 304 | 92 832 | 5 472 |
| POOL | 327 680 | 318 432 | 9 248 (suelo 8 KiB) |
| NOINIT | 15 696 | 15 572 | 124 |

Lectura: **el recurso escaso es el flash**. RAM, POOL, LOWBUF y NOINIT están por encima de sus suelos y sus
consumidores están todos vivos.

## 2. Búsqueda de recursos: qué se probó y qué se midió

| palanca | resultado | veredicto |
|---|---|---|
| `-fmerge-all-constants` | imagen 580 328 B, idéntica | ya lo hacía el compilador: **no hay nada** |
| `-fno-unroll-loops` | imagen idéntica | a `-Oz` no desenrolla: **no hay nada** |
| `-mllvm -enable-machine-outliner` | el build falla | **no disponible** en este clang |
| `noinline` en `draw_column`, `draw_column_tall` | imagen y símbolos idénticos | no estaban inlineadas: **hipótesis refutada** |
| `noinline` en `param_format` (690 B, 131 sitios) | idéntico | tampoco estaba inlineada |
| auditoría de símbolos `.bss`/`.pool`/`.noinit` | mayores: `WT_GEN` 12 288 (scratch de generación), `cv_buf` 59 520 (canvas máximo), `sl_buf`/`cloud_buf` 32 768 (SLICER, GRAIN) | todos en uso; solaparlos exige demostrar vidas disjuntas y ya se documentó que no se puede |
| `midi_sel_on[16][64]` (1 024 B) | ya empaquetado a 2 notas/byte | óptimo |
| `hc_tab` (1 548 B) | 3 ranuras × (puntero + LUT de 512 B) para M, L e iconos | exactamente lo que hay |

Conclusión medida: **no queda grasa de tamaño ni de RAM en este árbol**. El código grande (`draw_columns` 13 636 B)
es lógica propia de ~15 familias de página, no copias inlineadas: reducirlo pide un rediseño dirigido por tabla, que
no entra en esta fase sin cambiar el aspecto de las pantallas.

## 3. Lo que sí había: comportamiento indefinido

El camino de audio y el de dibujo estaban llenos de operaciones cuyo resultado el compilador puede plegar como
quiera. La suite no las veía porque su UBSan **excluye** `signed-integer-overflow` y `shift` (a propósito: el código
usa desplazamientos Q15 por todas partes). Una pasada con UBSan **estricto** (`-fsanitize=undefined,bounds`) sobre
`ui_test` — que ejercita los ocho motores, el FX, el secuenciador y todas las pantallas — nombró **14**:

```
fx.c:592,593        left shift of negative value      (clip del master, ×2)
fx.c:1790           left shift of negative value      (clave de la cola de eco)
eng_noise.c:123     left shift of negative value      (DRIFT)
eng_noise.c:244     left shift of negative value      (CRUSH)
fm6_core.c:267      left shift of negative value      (seno del operador)
fm6_core.c:440      left shift of negative value      (escala de velocidad)
eng_wheel.c:314     left shift of negative value      (salida del motor)
eng_fm6.c:434       left shift of negative value      (nivel del patch)
eng_analog.c:109    left shift of negative value      (paso del MORPH)
drumx_voice.c:98    signed integer overflow           (fase de la capa, ×2 sitios)
drumx_voice.c:269   left shift of negative value      (corte de DC)
ui_tech.c:55,64     signed integer overflow           (curva del DIST: fases y cúbico TAPE)
```

Cada corrección es **el mismo valor en aritmética definida**: `x << n` → `x * 2^n` donde el producto no desborda,
producto **sin signo** donde el envolvente es intencional (`drumx_voice`), y el cúbico TAPE del dibujo en 64 bits
**igual que en `dist_shape`** del audio (antes dibujaba un pliegue que el audio no hace).

Prueba de que no cambia el sonido: `tests/regress` con los dorados → **303 renders, 0 cambiados**. Prueba de que no
cambia el dibujo: `ui_render` completo → **3 056 fotogramas idénticos** salvo las 48 líneas de la cadena de versión.

## 4. Un instrumento que no medía lo que decía

`al_keycaps()` (alineación de los keycaps en el barrido de UI) leía `KC_DATA` como nibbles: ese flujo es **Huffman**
(`cv_alpha_hc`). Iba más allá del final del array (ASan lo nombró: `global-buffer-overflow`, índice 1947 de 1947) y
las cifras publicadas en esa fila del informe eran ruido. Ahora decodifica con el decodificador real. Con el
instrumento arreglado: **0 desalineados** en 3 781 017 medidas de 74 sitios.

## 5. Guardián nuevo

`tests/run_tests.sh`, sección de sanitizadores: `ui_test` con `-fsanitize=undefined,bounds -fno-sanitize-recover=all`
y `UBSAN_OPTIONS=halt_on_error=1`. Un UB de esta clase vuelve a fallar con archivo y línea.

## 6. Presupuestos después (clase build AC791N)

| medida | antes | después |
|---|---:|---:|
| imagen | 580 328 | **580 344** (+16 B: cúbico en 64 bits + productos sin signo) |
| RAM `.data+.bss` | 92 832 | 92 832 |
| `mix_block` (estático) | 32 907 | 32 907 |
| `noise_render` (estático) | 218 | 218 (se probó con máscara: 223, +2 %; se descartó) |

## 7. Pendiente, y de qué clase

- Nada de esto está medido en la FM-1: no hay unidad. Parpadeo, legibilidad, latencia y sonido físico siguen sin
  evidencia de clase física.
- El flash sigue con 3 268 B libres: cualquier función nueva compite con eso. Recuperar más exige rediseñar
  `draw_columns` (dirigido por tabla) o comprimir datos con coste de CPU, ambos con el aspecto delante.
