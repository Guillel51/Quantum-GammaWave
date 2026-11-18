# Fase 1 — auditoría de optimización (2026-11-18)

**Clase de cada medición:** el tamaño de imagen, `.data+.bss`, el pool y las instrucciones de código RAM salen
del **build AC791N** (`./build.sh`, pi32v2 clang + GNU ld). El coste por función sale de
`tests/target_budget.py` sobre `build/felucca.dis` (instrucciones dentro de bucles del compilador de destino,
×4 por nivel de anidamiento, ×9 un divide: un número estático, no ciclos). Los renders salen de
`tests/ui_render` en **host x86**; el audio, de `tests/regress` en **host x86** (303 renders dorados).
**Ningún número de este documento es una medición sobre la FM-1.**

## Dónde está el recurso, medido antes de tocar nada

| Recurso | Ocupado | Libre | Regla del repo |
|---|---|---|---|
| imagen (slot de app) | 583 544 B | **68 B** | ≤ 583 612 B |
| XIP | 563 024 B (código 424 120 + constantes 138 904) | 0 | — |
| `.data+.bss` | 91 320 B | 6 984 B | ≤ 96 KiB |
| pool | 311 256 B | **9 248 B** | ≥ 8 192 B |
| código RAM (`.hot_text`) | 19 724 B | 4 852 B | ≥ 2 048 B |
| LOWBUF | 30 564 B | 2 204 B | ≤ 32 768 B |

La conclusión que ordena todo lo demás: **la XIP es el recurso agotado** (68 B), no la RAM. Cualquier
optimización que agregue código al camino de audio tiene que pagarse a sí misma en bytes.

## Dónde se va el trabajo (estático, compilador de destino)

Las diez funciones con más instrucciones dentro de bucles:

```
152848 draw_columns      107095 ui_input_frame      64532 draw_graph
 54381 draw_gen_page     32667 mix_block            27865 part_dice
 27050 fm1_timer5_irq    25484 mc_job               23282 pfx_block
 20513 ed_fm6_handle    19253 gen_make
```

- **La UI ya está cubierta por firmas**: `draw_frame`, el pie, las tarjetas y las páginas saltan el redibujo
  cuando su firma no cambió (`ui.head_sig`, `ui.foot_sig`, `ui.roll[].sig`), así que su coste aparece solo
  cuando algo se mueve. Las divides que quedan ahí son de valores de runtime (anchos, posiciones), no de
  constantes: no hay equivalencia exacta que las convierta en shifts.
- **`mix_block` (32 667)** es el bloque entero con los buses, el master y el nivelador inlinados; ya usa
  `div_by_n` para los largos de bloque, una sola pasada para los tres picos de los sends, y compuertas por
  bus.
- **`fm1_timer5_irq` (27 050)** es el barrido de teclas/LED a **10 kHz**: 16 bits de 595 por tick a mano
  (4 lecturas-modificación-escritura por bit, deliberadas: los pulsos cortos lanzaban la columna vecina).
  Cambiar su cadencia o su temporización es una decisión de **hardware** (brillo de LED, rebote de teclas) y
  no se puede validar aquí.

## Redundancia real encontrada y corregida

`dly_wow_adv` y `fx_gate` (fx.c) dividían por el largo del bloque con `/`, una llamada a libgcc por bloque
(1 378 bloques/s a 32 muestras y 44,1 kHz) en un DSP donde pi32v2 **no tiene divide**. Se cambió por
`div_by_n` (shift + corrección de signo cuando `n == CTL`, la misma división exacta si no), el idioma que el
propio archivo ya usaba en `lev_block`, `fx_glide` y la capa de performance.

- Invariancia: `tests/regress` **303 renders dorados, 0 cambiados**; `ui_render` **191 pantallas × 22
  paletas, 0 hallazgos de lint, 0 px de desalineación en 3 777 777 medidas**.
- Coste en bytes: `mix_block` 2 189 → 2 188 instrucciones; imagen 583 544 → 583 604 B (**68 → 8 B libres**),
  el precio de eliminar dos divides por bloque. El presupuesto estático de `target_budget.py` sigue en
  32 667 (los divides estaban fuera de los bucles).

## Lo que se probó, se midió y **no** entra

1. **Cachear los coeficientes de TONE de los buses** (delay y reverb) por valor de perilla en vez de
   reconstruirlos 1 378 veces por segundo (tres divides y una tangente cada uno). Los renders dorados quedaron
   idénticos, pero el build **desbordó la XIP en 2 160 B**: con 68 B libres no hay lugar para el cache, ni
   siquiera con `tone_set` marcado `noinline` (la imagen no cambió con el atributo: el compilador ya no la
   inlineaba). Guardado para cuando la XIP tenga ~2,5 KiB libres.
2. **`-fno-jump-tables`** en `CFLAGS` (las tablas de salto son 4 B por entrada en `.rodata`): la imagen
   **creció 916 B**. Revertido.
3. **Huffman de la fuente S** (el generador ya lo aplica a M y L): ahorraría ~1,4 KB de constante pero mete
   un decodificador en la cara que más se dibuja. El generador lo descarta a propósito desde antes
   (`tools/gen_aa_font.py`, cabecera): es un cambio de calidad, no de tamaño, y la consigna prohíbe pagar con
   eso.

## Sin cambios, y por qué

- **Código muerto**: no hay. `--gc-sections` + `-ffunction-sections` ya lo quitan, y `-Wunused-const-variable
  -Wunused-variable` sobre el TU completo no da ningún aviso.
- **Datos muertos**: añadir `-fdata-sections` no movió ni un byte.
- **Duplicados en la región de constantes**: los bloques repetidos son una tabla periódica (paso 0x14), no
  dos copias de una tabla. Nada que fusionar.
- **Relleno**: 824 B de ceros ≥ 16 B en toda la región de constantes, casi todo datos.
- **Tablas más anchas de lo necesario**: sólo quedan las que no caben en 16 bits (`FM6_EXP2` hasta 2³⁰,
  `ENV_LIN` 12 M, `SCALE_MASK` 0xFFF, `AF_L_ADV` 468, `SL_RATIO` 2³¹).
- **Bufers del pool** (`dly_buf` 128 KiB, `cv_buf` 58 KiB, `sl_buf` 32 KiB, `cloud_buf` 32 KiB): cada uno lo
  dimensiona una función audible (tiempo de delay, línea de chorus, cola de slicer y de reverb) y son
  simultáneos; unirlos o recortarlos cambia el sonido.
- **CPU de la UI/DSP**: sin valgrind en este entorno y sin unidad, el único instrumento exacto es el conteo
  estático de arriba; cualquier afirmación de ciclos o de % de CPU sería inventada.
