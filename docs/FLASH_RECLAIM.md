# Recuperar flash: los dos candidatos medidos, y por qué no hay nada que sacar

Objetivo: liberar XIP para poder añadir cosas. Estado de partida: **583 600 B de 583 612 — 12 B libres**.
Antes de escribir un decodificador, medí los dos assets que parecían más fáciles. Los dos ya están en su
forma densa.

## 1. `UAC_FIR` — 7 680 B, ya reducido a la mitad

`firmware/src/uac_fir.h` guarda **80 fases de 24 taps** (Q30, `int32_t`), no 160: el propio header lo dice —
*«phases 0..79 (phase 159 - p = phase p backwards)»*. La simetría del prototipo ya está explotada en la
dimensión de fase.

Lo que queda por explotar sería la simetría **dentro** de cada fase (`h[k] == h[N-1-k]`), que es lo único que
permitiría guardar 12 taps en vez de 24. No existe: mirando la fase 0, `15446, -50808, 120879, … , 20041,
-2478`, los extremos no coinciden. Un FIR de fase fraccionaria no tiene esa simetría por rama. **No hay
mitad que sacar.**

Bajar los coeficientes a Q15 (`int16_t`) ahorraría 3 840 B, pero cambia los bytes del audio: los 293 renders
dorados dejarían de coincidir. No es una optimización invisible, es otra versión del resampler.

## 2. `KC_DATA` — 4 792 B, y el RLE no gana nada

Los keycaps ya están **empaquetados a 4 bits por píxel**, dos píxeles por byte, con la etiqueta rasterizada
dentro de la misma píldora (`tools/gen_aa_keycaps.py`). Probé la compresión más simple que se le puede
aplicar a un dato así —RLE por byte con escape `0xFF`— sobre las 28 píldoras reales:

```
bytes crudos 4888 → RLE 4887   (ahorro: 1 byte)
```

Un byte. El dato es prácticamente ruido a nivel de byte: cada píldora mezcla borde antialias, relleno y
glifos, y las repeticiones no llegan al umbral de 3 bytes que hace rentable un token de 3. Un decodificador
costaría 40-60 B de XIP para ahorrar uno. **Descartado por medición, no por intuición.**

## 3. Fuentes — 20 474 B, también a 4 bits

`AF_S` 9 523 B, `AF_M` 6 681 B, `AF_L` 4 270 B: **4 bits de alfa por píxel**, recortadas a la caja de tinta
(`tools/gen_aa_font.py`). 4 bpp es el suelo razonable en 240 × 240 con texto de 9-16 px; bajar a 3 bpp
ahorraría ~5 KB y se vería en los degradados de letra.

## 4. Y el compilador tampoco tiene margen

Medido, no supuesto: con `-Os` en vez de `-Oz` la región XIP **desborda 33 708 B**. El `-Oz` que ya está en
`tools/build.py` no es una elección conservadora: es la que sostiene la imagen.

## 5. Conclusión

La vía «comprimir los assets» está cerrada por medición: los tres candidatos ya están densos y el cuarto
(elegir otro flag) empeora 33 KB. Lo que queda es otra cosa, con otro coste:

* **Regenerar en el arranque** lo que hoy viaja como tabla (las 24 tablas band-limited de `WT_DATA` son
  aritmética; las fuentes, rasterizado). Ahorra flash de verdad —12 KB y 20 KB— pero gasta **RAM** (las
  tablas regeneradas viven en `.bss`: 10 156 B libres, no 32 KB) y **tiempo de arranque**.
* Eso acota el objetivo realista: se puede regenerar una de las tres fuentes (~5-9 KB de RAM) o una parte de
  `WT_DATA`, no todo. Es un trabajo de una sesión por pieza, con los 293 renders y las 189 pantallas × 22
  paletas como prueba de que no cambia un píxel ni una muestra.
