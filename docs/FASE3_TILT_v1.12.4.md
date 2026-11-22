# TILT — la inclinación espectral por voz (v1.12.4)

## Qué es y por qué esa y no otra

Antes de escribir DSP se miró qué le falta al instrumento. Hay tres maneras de mover el espectro y cada una está
en su sitio: el FILTER de la parte (un SVF resonante sobre la suma de la parte), las TONE de FX > DIST y el EQ de
MASTER (después de la mezcla), y las E de cada motor (dentro de su propia matemática). Ninguna es **por voz**: la
parte filtra el acorde entero, los buses filtran todo, y un motor sólo puede tocar lo suyo.

TILT es el sitio vacío: **un polo entre la voz y la parte**. Un estante agudo de primer orden cuyo DC queda
exactamente en 1 (`lp` con la constante que se le pide y `hp = x - lp`), de modo que el fundamental y el cuerpo
de la nota no se mueven y sólo el borde de **esa** nota se inclina. Doce voces inclinan doce veces, el acorde
conserva su balance, y los tambores y FM6 —que no pasan por el filtro de parte— tienen el mismo mando que un saw.

No duplica: el polo es fijo (~880 Hz a 44,1 kHz), no resuena, y en 0 **no se llama**: `k = 64` y `lp + hp` es
`x` bit a bit, y `voice.c` salta la llamada entera. Por eso los 307 renders dorados del árbol dan **0 cambiados**
con el parámetro en su default.

## Dónde está

| pieza | sitio |
|---|---|
| parámetro | `P_HARM` (id 125, el último libre antes de `P_E0`; `P_COUNT` 125 → 126 de 127) |
| DSP | `firmware/src/voice.c` → `tilt_voice()`, tras `e->render()`, en el bucle de voces |
| estado | `voice_t.tilt_lp`, un polo por slot del pool (**+72 B** de `.bss`), limpiado al arrancar una voz libre |
| EDIT | página nueva **TILT** (`params.c` PAGES), un mando, −64..+63, formato `F_BIPCT` (±%) |
| modulación | destino **HARM** (`MD_HARM`, número 30, añadido al final: los números guardados no cambian) |
| MIDI | **CC 76** (sound controller 7) sobre la parte seleccionada |
| rango | −64..+63 → ganancia del borde 0,25× a 1,75× (−12 dB .. +4,9 dB); `k = 64 + (amt*3)/4` |

## Medido (host x86-64, `tests/tilt_test.c`)

| medida | resultado |
|---|---|
| +45 en una nota con borde sobre el pivote (C6) | energía del borde **×1,62** |
| −64 en la misma nota | **×0,66** (dos tercios del borde fuera) |
| pico a +45 | **×1,30**, por debajo del techo de `k` (1,75×) |
| slot prestado a otra nota | **0 polos heredados** |
| coste de una voz a 45 | **−3,9 ns/muestra** de diferencia medida, dentro del ruido del host: cuatro instrucciones por muestra |

**Lo que este trabajo no mide:** el cuerpo bajo el limitador de MASTER (que recorta el nivel global cuando sube
el borde) y, por supuesto, el AC791N. Nada de esto es una medida de la unidad.

## Ejemplos representativos (valores listos para cargar)

| sonido | TILT | por qué |
|---|---|---|
| pad ANALOG ancho | **+40** | abre el aire sin tocar el cuerpo; con el LFO a CUT da movimiento |
| bajo WTABLE / FM6 | **−25** | quita el filo del ataque y deja el peso |
| lead PHASE | **+55** | el borde se vuelve el carácter, sin drive ni alias |
| DRUM (caja / hats) | **+30** | más presencia en el golpe, el kick intacto |
| modulación viva | **MATRIX: LFO → HARM, AMT 30** | el borde respira con el tempo |

Estos valores están en `x[]`-listos (`XP(P_HARM, v)`): no se retocaron los presets de fábrica en esta versión
porque su tabla de niveles (`preset_level.c`) y los 307 renders dorados son medidas generadas, y moverlas habría
invalidado las dos en el mismo commit que dice "0 cambiados". Queda como el siguiente paso natural.
