# Auditoría de XIP: presets, `UAC_FIR` y descriptores (2026-11-26)

Objetivo: buscar margen en el slot de aplicación sin cambiar nada audible ni visible. Clase de todas las cifras:
**build AC791N** (`./build.sh`, pi32v2 clang + ld, `build/felucca.map` y `build/felucca.bin`). Ninguna medida de una
FM-1 física, ninguna del emulador.

## Punto de partida (mapa real)

| | bytes |
|---|---|
| imagen | **576 916** de 583 612 (slot de aplicación) |
| libre en el slot | **6 696** |
| XIP | código 451 172 + constantes 120 964 = **572 136** desde `0x2000120` |
| RAM `.data+.bss` | **75 616** de 98 304 (22 688 libres) |
| POOL | 296 156 B, **24 352 B de holgura** |
| LOWBUF | **28 488** de 32 768 |
| loader OTA | 7 777 |

## 1 · Presets de fábrica: 11 016 B, ya densos

Once tablas `static const preset_t`, 306 registros de **36 B** exactos (`const char *name` 4 + `e[8]` 8 + `env[4]` 4 +
`fenv` 1 + `mono` 1 + `fx[4]` 4 + `pat` 1 + `x[6]` 12 + `legacy` 1; sin relleno de alineación):

| tabla | B | tabla | B |
|---|---|---|---|
| `ANALOG_PRESETS` | 2 628 | `WT_PRESETS` | 936 |
| `FM6_PRESETS` | 1 332 | `FORMANT_PRESETS` | 900 |
| `PHASE_PRESETS` | 1 224 | `WHEEL_PRESETS` | 792 |
| `LOFI_PRESETS` | 1 044 | `NOISE_PRESETS` | 792 |
| `DRUM_PRESETS` | 648 | `AD_PRESETS` | 432 |
| `DIGITAL_PRESETS` | 288 | | |

Lo medido sobre los bytes reales del `.bin` (no una estimación):

| codificación sin pérdida | tamaño | frente a 11 016 |
|---|---|---|
| RLE por bytes | 17 670 | **+6 654, peor** |
| delta entre presets + RLE | 17 012 | **+5 996, peor** |
| máscara de 32 bits de valores ≠ 0 + los valores (media 24 de 32 no nulos) | 8 554 | −1 238 |
| punteros de nombre a 16 bits (612 B) sobre lo anterior | — | −612 |
| **mejor caso estructural** | **~9 166** | **−1 850** |

Es decir: el «packer delta/RLE de 3–4 KB» que estimaba `docs/XIP_RECOVERY_2026-11-23.md` **no se sostiene al medirlo**
(las dos codificaciones obvias empeoran el tamaño). Lo único que ahorra es cambiar el formato a máscara + offsets, lo
que exige decodificador, búfer de RAM y tocar los ~40 puntos que leen `preset_t` (UI, editor, `gen`, tests); a cambio
de 1 850 B. **No aplicado**: no es un ahorro neto libre de obra y riesgo, y no es lo que hoy falta.

## 2 · `UAC_FIR`: 3 840 B, sin nada que recortar

80 fases × 24 coeficientes `int16` Q15 = 3 840 B, todas usadas: `uac_tap48` lee las fases 0..79 y las 80..159 son
la misma tabla leída al revés (simetría del prototipo, ya explotada por el generador y por `usb.c`). Comprobado
sobre la tabla enviada, columna a columna: **ninguna columna es cero en todas las fases** (la 0 tiene 72 de 80
valores no nulos y la 23, 61), así que no hay taps que se puedan quitar sin cambiar la suma; y los taps de una fase
no son simétricos entre sí (no hay otra mitad que deducir). Ya está a la mitad de lo que costaba en Q30
(7 680 B): no queda margen sin pérdida.

## 3 · Tablas de descriptores USB: 258 B, todas vivas

Lo que el `.bin` contiene: `DEV_DESC_PLAIN` 18 + `CFG_DESC_PLAIN` 178 + `STR0` 4 + `STR1` 42 + `STR2` 16 = **258 B**.
Las variantes CDC (`DEV_DESC`, `CFG_DESC`) no se compilan en esta imagen. Las cadenas ya están fusionadas por el
enlazador: 2 112 cadenas en `.rodata.str1.1`, **0 duplicadas**. No hay copia que quitar ni tabla compartible (los
descriptores deben ser contiguos para el host).

## Palanca descartada al medirla

`--sort-section=alignment` fuera del enlace: imagen **577 044 B** (+128) y RAM 75 628 B (+12). Peor; revertido y
vuelto al mapa de arriba (576 916 / 75 616).

## Veredicto

**No se aplicó ningún cambio**: ninguna de las tres áreas da un ahorro neto demostrable sin pérdida. Los 11 016 B de
presets son 306 × 36 con el 75 % de la carga no nula; `UAC_FIR` usa sus 3 840 B; los descriptores son 258 B. El único
recorte realista (formato de presets con máscara y offsets) son ~1 850 B a cambio de un decodificador y RAM, y no
compensa hoy: quedan 6 696 B libres en el slot y 22 688 B de RAM.

Comprobación tras revertir: el build vuelve **byte a byte** a las cifras de partida (imagen 576 916 B, RAM 75 616 B,
POOL 24 352 B de holgura, LOWBUF 28 488 B). Al no cambiar una línea de firmware, los goldens de audio y de píxeles
del árbol son los mismos que ya estaban verificados; no se regeneró ninguno.
