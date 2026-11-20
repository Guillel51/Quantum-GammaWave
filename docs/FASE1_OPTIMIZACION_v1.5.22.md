# FASE 1 — Recursos: media tabla de wavetable, y las tres palancas de flash que se midieron y no pagan

*20 de noviembre de 2026. Punto de partida: `v1.5.21`, commit `0594c9b`. Clases de evidencia: `[AC791N]` es el
binario compilado con el `pi32v2` clang 4.0.1 y el SDK AC79 fijados por `tools/build.py`; `[HOST]` es x86-64 con
`cc`; `[ESTÁTICO]` es el desensamblado de destino leído por `tests/target_budget.py`. **No hay FM-1 física en este
entorno**: nada de este documento afirma nada sobre el panel, el DAC, el amplificador ni el sonido audible.*

## 1. De dónde se partía, medido y no citado

| Región | Al cierre de `v1.5.21` | Libre | Uso |
|---|---:|---:|---:|
| **XIP** (slot de la app) | 581 720 de 583 612 | **1 892** | 99,7 % |
| XIP código / constantes | 427 368 / 133 780 | — | — |
| RAM `.data+.bss` | 92 880 de 98 304 | **5 424** | 94,5 % |
| POOL | 311 256, spare 9 248 | 9 248 | suelo 8 KiB |
| LOWBUF | 30 564 de 32 768 | 2 204 | — |
| RAM code | 19 760 de 24 576 | 4 816 | — |

Las cuatro fases anteriores dejaron cerrado, con medida, que **el recurso escaso es el flash**, que el código ya
está en `-Oz` (a `-Os` la región desborda 33 708 B), que las fuentes, los keycaps y los iconos ya están a 4 bits por
píxel en Huffman, y que los assets grandes (`UAC_FIR`, `KC_DATA`, `AF_*`) ya están en su forma densa. Esta fase no
repite esas búsquedas: busca **lo que queda**, y lo que queda es el banco del wavetable y la duplicación de código
del camino caliente.

## 2. Lo que se probó y no paga, con el número delante

| Palanca | Medición | Veredicto |
|---|---|---|
| `ld --icf=all` (plegado de código idéntico) | el `ld` del toolchain responde `unrecognized option '--icf=all'` | **no existe** en esta herramienta |
| `-fno-inline-functions` en todo el árbol | imagen **574 844 B** (−6 876, 8 768 B libres); `.hot_text` 15 820 → 5 552 B, `(rest)` +5 238 B | **descartado**: el camino DRUM pasa de **1 153** a **1 980** de coste estático (`tests/target_budget.py`, +72 % sobre un límite del +10 %). Los 10 KB que se recuperan son justo las copias inlineadas que hacen rápido el audio: no se venden |
| Iconos que ninguna pantalla dibuja | auditoría de los **132** ids contra `firmware/src`: 10 no se nombran (`tape`, `x_usb`, `x_hpf`, `x_tstop`, `x_freeze`, `x_oct_up`, `x_oct_dn`, `x_matrix`, `x_gauge`, `x_dyn`) | **0 B que recuperar**: los 10 ya estaban en `NOT_DRAWN` / fuera de las listas del generador desde 1.1 y 4.2; sólo queda su nombre en el enum. El árbol ya estaba limpio |
| Duplicados de rodata | `tools/ram_flash_map.py --dups` en la suite | **0 grupos** (fase anterior, sigue en verde) |

La conclusión honesta de esta mitad: **no queda grasa de flash que se pueda quitar sin tocar comportamiento**.
La única palanca de tamaño real que aparece (dejar de inlinear) se paga en el camino de audio, que es justo lo que
el presupuesto protege.

## 3. Lo que sí entró: una sola mitad del wavetable, por simetría

Cada frame del WAVETABLE es una suma de senos del armónico `k` del ciclo, en fase 0 (`tools/gen_tables.py`,
`wt_frame`). Una suma de senos es **impar**: `w[WT_POINTS − i] = −w[i]` para todo `i`, y `w[0] = w[128] = 0`. La
tabla guardaba el ciclo entero (3 niveles de banda × 8 frames × 256 puntos × `int16`); ahora guarda **una mitad por
frame** (`WT_PTS_H = 129` puntos) y `wt_pts()` lee la otra mitad al revés y negada, con **un solo cálculo de índice
para los dos frames** que un MORPH interpola.

| Medida | Antes | Después | Clase |
|---|---:|---:|---|
| `WT_GEN` (`.bss`) | 12 288 B | **6 192 B** | `[AC791N]` (`nm` sobre el ELF enlazado) |
| RAM `.data+.bss` | 92 880 de 98 304 | **86 784** de 98 304 | `[AC791N]` |
| RAM libre | 5 424 B | **11 520 B** (+112 %) | `[AC791N]` |
| `wt_render` | 908 B | 924 B (+16) | `[AC791N]` |
| Imagen / libre en el slot | 581 720 / 1 892 | **581 816 / 1 796** | `[AC791N]` |
| POOL, LOWBUF, RAM code | 311 256 / 30 564 / 19 760 | idénticos | `[AC791N]` |

### Lo que cuesta, dicho sin adornos

- **La tabla no es bit a bit la de antes, y se midió cuánto**: el guardado redondeaba mitad hacia arriba, así que las
  dos mitades del ciclo guardado no eran exactamente opuestas. Con la mitad y la negación, **3 246 de los 6 144
  valores** del ciclo reconstruido difieren del guardado en **≤ 2 LSB de ±32 000** (0,006 % de escala plena, −84 dB
  sobre la tabla). Es la simetría que la propia construcción implica, no un cambio de forma de onda.
- **El render de WAVETABLE se movió en la misma proporción**: dos presets (`GLASS PAD`, `MORPH LEAD`), tres notas,
  dos segundos, clase `[HOST]`: **63 792 de 88 200 muestras difieren, |Δ| máximo 22 sobre un pico de 98 448**
  (−73 dBFS), media de 3,7 en las que difieren. **Los 306 goldens: 23 cambiados** (los 23 presets de WAVETABLE,
  00..22) y **283 idénticos**; `tests/golden.txt` se rebaseó con `GOLDEN_UPDATE=1`, como en la FASE 6, y no se tocó
  ningún otro render.
- **Cuesta CPU en ese motor**: `tests/wavetable_test` mide **12 → 14 ns por muestra y voz** (mínimo de 5 corridas,
  `[HOST]`), **+17 % del coste de una voz de WTABLE**. El presupuesto estático de destino no tiene línea para
  `wt_render` y las demás no se movieron: `DRUM path 1 153/1 153`, `perf_block 253/254`, `rev_room 243/243`,
  `rev_spring 126/126`, todo **ok**; `tests/regress` CPU **PASS** (207 filas). Es el precio de la mitad de RAM, y
  está aquí para poder revertirlo con el número delante.

### La lección de implementación que no hay que repetir

La primera versión espejaba **la fase** en vez de la tabla (`m = −ph`, misma fracción). Es falso: `w(ph) = −w(1 − ph)`
invierte también la fracción, y el test lo cazó — 2 comprobaciones en rojo y un render con |Δ| de 165 998. La versión
que queda camina la mitad guardada **hacia atrás** (índices `n` y `n − 1`, con `n = 256 − i`), que es exactamente la
misma aritmética del guardado sobre los mismos números.

## 4. Qué compra la RAM liberada, y qué sigue cerrado

`.bss` libre pasa de 5 424 a **11 520 B** (y el suelo de 8 KiB del POOL no se toca). Con eso:

- **FM6_POLY 6** (6 pistas × 2 notas × ~296 B = **3 552 B** de `.bss`) **entra en RAM** pero sigue **bloqueado por
  flash**: pide ~**3 268 B** de XIP y hay **1 796**. La decisión sigue siendo de la fase de flash, no de ésta.
- Cualquier estado nuevo de hasta ~3 KiB por debajo del margen de 4 KiB ya no compite con el wavetable.
- El flash **no** se liberó: esta fase lo deja 96 B más apretado. Preferí decirlo a maquillarlo con un cambio que
  rompa el audio.

## 5. Reproducir

```sh
export JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/fw-AC79_AIoT_SDK
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"
./build.sh                                     # imagen, XIP, .data+.bss
python3 tools/ram_report.py                     # WT_GEN y el reparto por región
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/wavetable_test tests/wavetable_test.c -lm && ./build/host/wavetable_test
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/regress tests/regress.c -lm && ./build/host/regress tests/golden.txt tests/cpu_baseline.txt
NO_PACKAGE=1 ./tests/run_tests.sh               # la suite entera
```

## 6. Sin verificar, y se dice

- **Nada de esto está medido en la FM-1**: no hay unidad. El coste de +17 % es `[HOST]` sobre una CPU x86-64; el
  presupuesto estático de destino no cubre `wt_render`, así que **no hay una cifra de ciclos de destino** para la
  lectura espejada. Es la limitación principal de este cambio y está puesta donde se ve.
- El sonido audible del wavetable tras el cambio (≤ −73 dBFS de diferencia) no se escuchó: no hay DAC.
- `memory_map_test` sigue dependiendo del `objdump` de JieLi; con el toolchain instalado lo corre la suite, y si se
  salta, se informa como no ejecutado.
