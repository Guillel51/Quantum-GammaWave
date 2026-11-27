# FASE 7 — HOME > el vúmetro de la tercera pantalla (2026-11-27)

Documento corto: recoge lo medido y lo implementado, y lo que la fase deja pendiente. **La fase no está
terminada** — la renovación visual global que pide el encargo no cabe en el presupuesto de XIP que hay hoy
(ver §3) y no se declara hecha.

## 1. El vúmetro estaba donde no se podía leer

`hv_air()` (`ui_homeviews.c`) dibuja, cuando recibe `floor = 1`, un suelo de seis barras —una por pista, de
`trk[].vu`, el nivel que publica `fx.c`— en las **últimas 18 filas del panel**. Ese suelo es el «vumetro
debajo de todo» de HOME: en OVERVIEW (tercera pantalla, «HOME 3/5») caía justo bajo la rejilla de pasos,
donde no hay ninguna pista a la que atarlo, y no es el medidor fantasma de PERFORMANCE (ese ya se había
quitado pasando `floor = 0`, y sigue quitado).

**Ahora:** OVERVIEW llama `hv_air(..., 0)` —sólo ascuas, la pantalla sigue respirando— y cada una de las
seis filas de pista lleva su medidor dentro: una línea de 2 px bajo sus pasos, `trk[].vu` como barra y
`trk[].peak` como tope de 1 px. Son los mismos dos números que ya muestran los medidores del MIXER, puestos
en la fila de la pista que miden.

## 2. Verificación (host x86, sin unidad física)

- `ui_render` + `ui_pixels.py`: **3 321 fotogramas**, base actualizada tras revisar las capturas y luego
  idéntica (`3321 frames identical to the baseline`). Cambian las cinco variantes de `home_overview`
  (una por paleta del barrido) y las pantallas que llevan la versión impresa.
- `ui_test`: `ui: all passed`. `text_ref_test`: 6 732 cadenas × fuentes × paletas, 0 diferencias.
- `regress`: **316 dorados, 0 cambiados** (el DSP no se toca).
- `target_budget.py`: 0 FAIL.
- AC791N (`./build.sh`): `image 581700 B`, `XIP code 456660 B + constants 120260 B` (+32 B de código
  respecto de v2.18.0), RAM 86 072/98 304, pool spare 16 672 B.

## 3. El límite que sigue abierto

En v2.18.0 se midió que XIP tiene ~1,9 KB libres (`.text` termina en `0x0208CE98`; el final de la región es
`0x0208E8DC`, y detrás van las imágenes de carga de `.ram_text`, `.c1_text` y `.data`). Este cambio costó
32 B y entra. Un rediseño global —profundidad en más gráficos, knobs con respuesta dibujada, pantallas
nuevas— no entra: cada pantalla nueva son cientos de bytes.

Por eso quedan **sin tocar** en esta pasada: SHIFT, PATTERN, TILT, LFO SHAPE, PRESETS NOFAV, MASTER, la
profundidad de ANALOG Morph, los iconos pequeños y el resto del barrido de 208 pantallas / 22 paletas. El
siguiente paso técnico correcto es recuperar 2–4 KB de XIP (mover código caliente a RAM o recortar rutas
muertas) y sólo entonces continuar con la renovación, pantalla por pantalla, con capturas antes/después.
