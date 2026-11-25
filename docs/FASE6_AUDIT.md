# Fase 6 — el estado medido de la UI antes de rediseñarla

Clase de cada cifra: **[HOST x86-64]** (`tests/ui_render.c`, la barrida de render y su lint) y
**[TARGET AC791N]** para tamaño y memoria. Ninguna es una lectura de la unidad.

## Lo que la barrida ya mide hoy

| puerta | cifra | estado |
|---|---|---|
| pantallas × paletas | 203 pantallas × 22 paletas, 3 241 cuadros en `tests/ui_pixels.txt` | todos idénticos al baseline |
| lint de dibujo | **0 hallazgos** (solapes, tintas sobre su propio color, cajas fuera de su celda) | verde |
| texto | 264 textos libres elididos, **3 distintos** | ver abajo: dos son cadenas de muestra del arnés |
| alineación | 3 968 383 medidas en 76 lugares, **0 desvíos de 1 px** | verde |
| valores de columna | 250 482 barridos (`text_audit.tsv`), 8 440 etiquetas RATE | verde |
| idle | 29 952 px escritos y **0 cambiados** en las escenas quietas (tope 33 000) | verde |
| audio | 311 renders dorados, **0 cambiados**; 0 fallos de CPU budget | verde |

Los tres textos elididos son `GREY/song_named` (`'MWMWMW…'`), `GREY/message_key` (`'HOLD TO…'`) y
`GREY/message` (`'LOADED 07 A VE…'`): son **cadenas de muestra del propio arnés** (`ui_render` alimenta
nombres imposibles a propósito para probar el recorte), no textos del firmware que se corten en la consola.
Por eso la fase 6 no arranca de "arreglar textos truncados": esa cuenta ya está en cero en las pantallas reales.

Lo mismo con lo demás que la fase pide "corregir": no hay solapes, ni cajas fuera de celda, ni tintas
ilegibles en las 22 paletas, y el idle no cambia un píxel. Lo que la fase tiene por delante es **diseño**
(profundidad, jerarquía, gráficas, animaciones, feedback), no reparación — y el diseño es justo lo que estas
puertas no pueden aprobar por mí: miden que nada se rompa, no que se vea mejor.

## El orden en que conviene hacerlo, con su medición

1. **Profundidad sin copias** (puntos 1-2). El camino es un helper único de sombra/contorno en `gfx.c`
   (`cv_*` ya tiene el contrato de cajas y el lint de solapes): una sombra de 1 px desplazada y un contorno
   de 1 px por panel, aplicados en el sitio donde se dibujan las tarjetas, no en 203 pantallas. Medición:
   la barrida de píxeles dirá exactamente cuántos cuadros cambian; el lint debe seguir en 0.
2. **Gráficas** (punto 3: envolventes, ondas, filtro, espectro, LFO, FX). Cada una tiene su lienzo y su
   helper; la medición es la misma barrida más `tests/ui_pixels.txt`. Ya hay precedente: la consola FX de
   2.5.0 cambió 26 cuadros y el resto quedó idéntico.
3. **Indicadores de voces y medidores** (puntos 4-5). Aquí hay una ganancia real de CPU escondida: si el
   indicador se redibuja entero cada cuadro, la caché de filas de `gfx.c` ya evita la escritura pero no el
   recorrido. Medición: `ui_render --idle` (hoy 29 952 px, tope 33 000) y `ui_flicker`.
4. **Transiciones y feedback** (puntos 6-7, 11, 13). Sin estado nuevo: fase desde `ui.frame`, como el túnel
   del DELAY. Medición: píxeles + idle + el lint de alineación.
5. **Tipografía y jerarquía** (puntos 8, 12, 14). La alineación ya está en cero desvíos; lo que falta es
   decisión de diseño (qué es título, qué es valor, qué es etiqueta) y se mide con el mismo lint de cajas.
6. **Optimización** (dirty rects, firmas, buffers, SPI). Parte ya está: la caché de filas de `gfx.c`, las
   bandas, `gfx_forget()` en el cambio de paleta y `ui.force`. Lo que falta medir es **cuántas filas viajan
   por cuadro en cada pantalla** y si alguna redibuja sin cambiar: `ui_render --bands` y `--idle` son el
   instrumento, y el objetivo es bajar 29 952 sin tocar un solo píxel del resultado.

## Lo que esta ronda hizo

Nada de la fase 6 en el firmware: sólo esta auditoría, con las cifras de arriba como punto de partida. Un
rediseño de 203 pantallas × 22 paletas hecho a ciegas, sin poder mirar el resultado en la consola, es
exactamente lo que las reglas del repo prohíben: la barrida aprobaría que nada se rompe y yo no podría decir
que se ve mejor.
