# Etapa 1/8 — auditoría real de recursos del FM-1 y primera optimización profunda

Clase de cada número: **`[AC791N]`** = lo que el linker y el compilador del destino escribieron (`build/felucca.elf`,
`build/felucca.map`, el log de `./build.sh`). **`[HOST]`** = x86 (`tests/regress`, `tests/ui_render`). No hubo unidad
física: nada de esto dice cómo se comporta una FM-1, y nada se le escribió.

## 1. Estado real medido (no de informes anteriores)

| recurso | usado | presupuesto | libre | uso |
|---|---|---|---|---|
| **imagen XIP (app slot)** | 580 900 B | 583 612 B (`STOCK_APP_SIZE` = `0x8E7BC`) | **2 712 B** | 99.54 % |
| `.data + .bss` | 91 540 B | 98 304 B (96 KiB, AGENTS §8) | 6 764 B | 93.1 % |
| pool (buffers grandes) | 311 253 B | ventana `0x50000` = 327 680 B | 9 248 B + stacks | 95.0 % |
| `.lowbuf` (buffers del main loop) | 30 564 B | 32 768 B | 2 204 B | 93.3 % |
| `.c1_stacks` (núcleo 1) | 7 168 B | dentro del pool | — | — |
| render (DSP) | `target_budget.py` | +10 % por lazo | ninguno fuera | — |

**El recurso más limitado es la imagen XIP del slot de aplicación: 728 B libres antes de esta etapa (0.12 %).**
RAM y pool tienen entre 7 y 9 KB libres; el presupuesto de DSP no tiene un solo lazo fuera de rango. Siete versiones
seguidas se pelearon por esos cientos de bytes, y por eso la etapa apunta ahí.

Consumidores más grandes (símbolos del `.text`, que es donde viven código **y constantes** en XIP):

| bytes | objeto | clase |
|---|---|---|
| 15 700 | `draw_graph` | código UI |
| 13 846 | `ui_input_frame` | código UI |
| 13 636 | `draw_columns` | código UI |
| 10 928 | `graph_tech` | código UI |
| 9 992 | `gen_make` | generador |
| 8 172 | `draw_gen_page` | código UI |
| 7 680 | `UAC_FIR` | constantes (resampler USB) |
| 7 293 / 5 906 / 4 270 | `AF_S_DATA` / `AF_M_DATA` / `AF_L_DATA` | constantes del SDK |

En RAM mandan los buffers, no las voces: `cv_buf` 59 520 B (lienzo 240×124), `sl_buf` 32 768 B (grabación del
FX), `cloud_buf` 32 768 B (línea de CLOUD), `trk` 10 560 B. Ninguno tiene relleno: `cv_buf` es exactamente
240 × 124 × 2 B.

## 2. Cuello de botella e idea

La tabla `FM6_EXP2` eran **4 100 B de XIP** para 1 025 puntos de `2^(i/1024) − 1` en Q30 — el 0.70 % de la
imagen, **casi seis veces** el margen total que había. No es un dato de audio: es una curva suave.

## 3. La optimización implementada

La curva se guarda como su **segunda diferencia**: `d2` vale 491..983 en los 1 023 pasos y entra en un `int16`,
así que la tabla queda en **2 054 B** y los 1 025 enteros se reconstruyen en el arranque con sumas
(`fm6_exp2_init`, al lado de `wt_gen_init`, que ya usaba ese patrón). Los puntos decodificados viven en `.bss`
(4 100 B de los 6 764 libres) y **cada consulta pasa a ser una lectura de RAM en vez de una de XIP**: el camino
caliente del motor FM6 no se hace más lento.

- `tools/gen_tables.py`: emite `FM6_EXP2_V0`, `FM6_EXP2_D1`, `FM6_EXP2_N`, `FM6_EXP2_D2_N` y `FM6_EXP2_D2[1023]`.
- `firmware/src/fm6_core.c`: `fm6_exp2_tab[1025]` en `.bss`, `fm6_exp2_init()` y `fm6_mant()` leyendo de ahí.
- `firmware/src/main.c`: `fm6_exp2_init()` en el arranque, antes del audio.

### Métricas antes / después `[AC791N]`

| | antes | después | Δ |
|---|---|---|---|
| imagen (app slot) | 582 884 B | **580 900 B** | **−1 984 B** |
| libre en el slot | 728 B | **2 712 B** | **+1 984 B (×3.7)** |
| constantes XIP | 133 796 B | 131 764 B | −2 032 B |
| código XIP | 428 496 B | 428 596 B | +100 B (la decodificación) |
| `.bss` | 87 460 B | 91 560 B | +4 100 B |
| pool libre | 9 248 B | 9 248 B | 0 (no se tocó el pool) |

### Calidad: nada se movió `[HOST]`

- `tests/regress`: **306 golden renders, 0 cambiados**, 0 fallos de salud/voz/routing, 0 crash.
- `tests/groups.sh ui`: **0 hallazgos de lint**, **0 de 3 785 110** medidas de alineación.
- `tests/target_budget.py`: exit 0, ningún lazo por encima del +10 %.

## 4. Herramientas nuevas de diagnóstico

- **`tools/resource_map.py`** — mapa reproducible desde el ELF: RAM (`.data+.bss` contra 96 KiB y el hueco real),
  pool (usado, ventana y spare, con la ventana **leída del `app.ld`**), `.lowbuf`, `.c1_stacks`, XIP y el slot
  (leyendo `STOCK_APP_SIZE` de `web/fm1pkg.js`), más los objetos más grandes en RAM y en flash. `--json` escribe
  `build/resources.json` para comparar entre builds; `--check` falla si se cruza un techo.
- **`tests/resources_test.py`** — corre `--check` y además **fija la optimización**: `FM6_EXP2_D2` tiene que ser
  2 046 B en `.text`, `fm6_exp2_tab` 4 100 B en `.bss` y el `FM6_EXP2` de 4 100 B no puede volver. Una tabla que
  regrese al flash rompe un test, no un release.
- **`tests/fm6_exp2_test.c`** — decodifica los 1 025 puntos y los compara uno por uno contra la fórmula del
  generador (con libm en double y el mismo redondeo), más monotonía, tope exacto (`2^30`) y rango `int16`.

## 5. Bug encontrado en el camino

El primer `resource_map.py` reportaba **3 B de spare de pool**: calculaba el hueco como `_pool_end − último objeto`,
pero `.pool` es `NOLOAD` y el hueco es lo que queda de la **ventana** `POOL` después de la sección y los stacks del
núcleo 1. Se corrigió leyendo la ventana del linker script; ahora coincide con el `spare 9248` que imprime el build.

## 6. Pendientes para la Etapa 2

1. **`UAC_FIR` (7 680 B)** y `AF_*` (17 KB) siguen en XIP; el resampler USB es el próximo candidato real, pero sus
   coeficientes no son una curva suave y cualquier esquema tiene que ser exacto (los goldens lo verificarían).
2. **Los tres buffers de 32 KB** (`sl_buf`, `cloud_buf`) y `cv_buf` son el 40 % de la RAM: un análisis de vida útil
   podría unirlos si nunca coinciden en el tiempo, y liberaría decenas de KB — hoy no hay evidencia de que no
   coincidan, así que no se tocó.
3. **`draw_graph` (15 700 B) / `draw_columns` (13 636 B)**: código UI grande; medir cuánto es código común antes de
   intentar fusionarlo.
4. El uso del **núcleo 1** (tracks 4–6) no se midió en esta etapa: no hay contador de carga por núcleo expuesto en
   el ELF.
