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

## 6. Dos experimentos más, los dos a cero

Buscando bytes sin escribir código nuevo, probé dos cosas más y las dos dieron exactamente cero:

| Prueba | Medición |
|---|---|
| `-fmerge-all-constants` añadido a `CFLAGS` (fusionar constantes idénticas) | imagen **583 600 B**, idéntica: el linker ya las fusiona, o los cuerpos no son idénticos porque cada descriptor lleva su nombre |
| Descriptores de parámetro duplicados a mano en `params.c` | hay **6 repetidos** (`FILTER`, `PITCH`, `SHAPE`, `GATE`, `SWING` y uno más), 192 B si se fusionaran — pero la tabla `PDESC` es un array **de structs**, no de punteros: convertirla a punteros costaría 125 × 4 = 500 B de punteros para ahorrar 192. No sale |

`-fmerge-all-constants` no se quedó en el build: no aporta y es una opción que cambia la identidad de
direcciones, así que no se adopta sin ganancia medida.

## 7. Lo que esto significa para la etapa de optimización

Con estos cinco resultados (FIR ya espejado, keycaps sin redundancia, fuentes a 4 bpp, `-Os` peor por 33 KB,
constantes ya fusionadas), la conclusión de la etapa 1 es que **el firmware actual no tiene recursos que
liberar por compresión ni por flags**. Lo que queda es cambiar el modelo: guardar menos y calcular más.

Y ahí aparece el círculo que hay que resolver primero, porque es el bloqueo real de todo lo demás: cualquier
generador en el arranque es **código nuevo**, y para que ese código entre hace falta flash libre. Con 12 B
libres no entra ni el bucle que lo haría. La primera pieza tiene que ser, entonces, una cesión explícita de
una de estas tres cosas:

1. **RAM por flash**: mover una tabla a `.bss` y regenerarla al arrancar (hay 10 156 B de RAM; `AF_L` son
   4 270 B), aceptando el coste de arranque.
2. **Un matiz de calidad**: bajar las fuentes a 3 bpp (~5 KB) y verlo en el antialias del texto.
3. **Más flash**: cambiar el mapa de memoria o el chip, que es una decisión de hardware y no de firmware.

Sin una de esas tres, la etapa 1 está terminada en lo que respecta a liberar recursos, y las etapas 2 a 7 no
tienen dónde meterse.

## 8. Resultado: el modelo cambió, y funcionó

La cesión que se eligió fue la primera: **guardar menos y calcular más**, sin tocar calidad ni RAM crítica.
`WT_DATA` guardaba 8 formas × 256 puntos × 3 límites de banda (12 288 B). Ahora guarda:

* las amplitudes armónicas de las 8 formas en Q30 (`WT_COEF`, 2 048 B), de donde se sintetizan el nivel de
  64 y el de 16 armónicos al arrancar (`wt_gen_init`, 8 192 B de `.bss`, 292 B de código);
* el nivel de 5 armónicos, entero (`WT_L5`, 4 096 B): es el que leen las notas agudas y no debe esperar a
  ningún cálculo.

Medido: **583 600 → 577 760 B de imagen (5 840 B liberados)**, `.bss` 97 380 de 98 304, y la onda generada
difiere de la guardada en 3 LSB de 32 000. Los 22 goldens de WAVETABLE se rebasaron; los otros 271 no se
movieron.

Con esos bytes entró **VOICE 3 (DETUNE)**: 24 B, un control que el DSP aplicaba desde siempre y que la
v4.1.0 había dejado fuera por falta de espacio. Quedan **5 816 B** libres para la siguiente pieza.
