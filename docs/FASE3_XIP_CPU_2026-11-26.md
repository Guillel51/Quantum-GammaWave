# Fase 3/4 — XIP, CPU y rendimiento DSP (2026-11-26)

Clases: **[AC791N]** enlace real con el toolchain pi32v2 (`./build.sh`, 34 s);
**[HOST x86-64]** `tests/cpu_baseline.txt` por `tools/budget_report.py`; **[leído]** código.
Sin unidad FM-1 y sin `objdump` de JieLi en este entorno (por eso `target_budget.py` y
`memory_map_test.py` no pueden desensamblar aquí).

## 1. El estado medido

```
XIP   image  583 608 B  of an app slot of 583 612 B  (4 free, floor 2048)
```

- **Déficit contra el suelo: 2 044 B.** Y el arreglo de ocho fases en toda la cara S pedía
  **5 512 B** más (commit `931def40`): las dos cosas juntas son **7 556 B**.
- Constantes en `.rodata`: **124 900 B en 633 secciones**. `xip_audit`: **0 B de relleno y
  DEAD: ninguna**. Cada byte constante está referenciado desde la imagen.
- `minsize` ya cubre prácticamente todo el código de main loop: `SIZE_FILES` en
  `tools/size_fns.py` incluye ui*.c, gen*.c, editor*.c, project.c, storage*.c, seq.c, song*.c,
  usb/midi, ota, panel/lcd/gfx, params, engines, fm4_convert… Sólo el audio (voice, eng_*, fx, dsp,
  master, mod, morph, slicer, tune, pfx) queda en `-Os`, y lo está a propósito.
- Mayores objetos de código [AC791N] (`nm -S`): `ui_input_frame` **30 758 B**, `fm1_main` 18 074,
  `draw_columns` 14 184, `graph_tech` 10 142, `gen_make` 9 974, `ed_handle` 9 448,
  `draw_gen_page` 8 282, `edit_param` 7 938, `draw_layer` 7 572, `part_dice` 6 852.

## 2. Por qué recortar "los 7 556 B" no es recortar 7 556 B

Medido en esta sesión [AC791N]: marcar `noinline` las **67** funciones estáticas de `ui_input.c`
(una edición que no toca una sola línea de lógica) **empeora el enlace: desborda XIP por 224 B**.
Revertido; el árbol quedó limpio. Es decir: el inlining ya ahorraba, y el tamaño del código no
responde de forma monótona a los ajustes.

Ya estaba documentado con medidas del mismo tipo (`docs/FASE6_XIP_NARROW.md`):

| cambio | XIP |
|---|---|
| `if (cv_win_seen(x, y)) return;` (diez instrucciones) | **+2 592 B** |
| `-mllvm -inline-threshold=400` | **+165 524 B** (descartado) |
| `minsize` en `ui_graph.c` + `gen.c` | −1 024 B |
| `minsize` en todos los `.c` | −1 272 B (no alcanza, y cuesta CPU) |
| **hoy: `noinline` en 67 ayudantes de `ui_input.c`** | **+228 B** (peor) |

La pasada de tamaño de pi32v2 vuelve a decidir 306 de 1 645 funciones (mediana +4 B, máximo
+370 B) con sólo cambiar el IR. **Ningún recorte por debajo de ~2 500 B es siquiera medible** en este
árbol: el ruido del compilador es del mismo orden que la poda.

## 3. La tipografía: el umbral ya está cumplido

`931def40` no dejó el problema abierto: la cara S rasteriza a 8 fases **sólo en los 20 glifos de los
pares que fallaban** (`AF_S_GI`, `AF_S_X`, 2 bits por glifo) y **text_spacing cierra en 0,202 px**
(rms 0,059, **0 pares sobre 0,25**), con `text_ref_test` en 0 pines y 0 píxeles distintos en 6 732
cadenas. Extender las 8 fases a los 95 glifos de la cara costaba 5 512 B y **no añade nada al
umbral**: sólo suaviza pares que ya están por debajo de 0,25. No hay fallo escondido aquí: el
criterio de la fase se cumple, y lo que falta es presupuesto, no corrección.

## 4. Dónde están de verdad los bytes recuperables

| recurso | bytes | qué haría falta para recuperarlos |
|---|---:|---|
| tablas de presets de fábrica (9 motores) | **13 564** | Codificar cada preset como diferencias contra el vector por defecto del motor (máscara + valores), decodificar al cargar (scratch de ~240 B de RAM). Exactitud demostrable valor por valor; toca el cargador de 9 motores y la compatibilidad de presets. |
| `.rodata.str1.1` (etiquetas de UI) | **25 708** | Diccionario de subcadenas + decodificador en el camino de dibujo; hoy el compilador ya fusiona literales idénticos, así que sólo comprime repetición *parcial*, y cuesta CPU en cada texto. |
| `ui_input_frame` | **30 758** | Es una sola función de 7 700 instrucciones. Partirla por familia de pantallas con despacho por tabla: es el único sitio con 5–10 KB plausibles, y exige que el barrido de píxeles (`ui_pixels.txt`, 3 305 cuadros) quede idéntico. |
| fuentes AA (`AF_*_DATA` + registros) | **23 812** | Ya en Huffman y ya recortadas a medio ciclo/4 fases donde se podía; una segunda pasada exige descomprimir la cara entera en RAM. |
| `UAC_FIR` 80×24 | **3 840** | Ya sólo guarda media polifase (el código lee `UAC_FIR[159-ph]` al revés): no hay simetría extra. |
| `AI12/AI16/AI24_DATA` | **~7 500** | Máscaras de alfa por tamaño: derivarlas entre tamaños cambia píxeles. No se toca. |
| tablas de nombres (`GENRE` 2 548, `DSTYLE` 1 344, `RECIPE` 1 276, `ICON_MAP` 1 160, `PAGES` 1 092, `DRUM_MAP` 1 408) | **8 828** | Cada entrada guarda un `const char *` de 4 B (y campos de 16/32 bits donde caben 8). Cambiar el puntero por un índice de 16 bits al pool de cadenas que ya existe ahorra ~2 B por nombre y no cambia una letra; hay que tocar cada tabla y sus accesos, con el barrido de píxeles como contrato. |

## 5. CPU

No se cambió una línea de DSP en esta fase, así que **no hay ninguna mejora de CPU que declarar**.
La única medida disponible aquí es de host y ya existía (`tests/cpu_baseline.txt`, clase **HOST
x86-64**, no AC791N):

```
mix    max 1574 (3parts_full_drums)  avg  686
PHASE  max 1558 (05_WIRE)            avg 1160
FM6    max 1475 (04_PAD)             avg 1210
ANALOG max 1461 (06_RAVE)            avg 1192
```

El DSP ya lleva varias rondas de "no optimizar por corazonada": los efectos tienen sus salidas
tempranas (`if (gain_db == 0)` en las tres etapas de `fx.c`), las tablas trigonométricas se
reconstruyen al arrancar (`sine_init`, `wt_coef_init`, `fm6_exp2_init`) y el reparto dual-core está
fijado. Cualquier cambio de motor tiene que demostrarse con los goldens bit a bit y con
`tests/target_budget.py` en el destino —que en este entorno **no puede correr** por falta de
`objdump`—, y con el host no se puede afirmar nada sobre el AC791N.

## 6. Qué queda para la siguiente pasada, en orden

1. **Presets por diferencias** (objetivo 5–6 KB, determinista y exacto): empezar por ANALOG (2 628 B)
   y GENRE (2 548 B), que juntos son el 38 % del total; el decodificador es una función pura y se
   puede probar contra las tablas actuales valor por valor antes de tocar el enlace.
2. **`ui_input_frame` por familias** (5–10 KB plausibles): refactor puro con el barrido de píxeles
   como contrato; hacerlo con el enlace por debajo del suelo obliga a medir cada corte.
3. Con (1) hecho, reevaluar las 8 fases en toda la cara S: si sobran ≥5 512 B y el ruido del
   compilador lo permite, entra; si no, el umbral ya está cumplido y se documenta.

Nada de esto se aplicó a medias: una tabla comprimida sin su decodificador probado en las nueve
rutas de carga de motor es exactamente el tipo de cambio que rompe presets.
