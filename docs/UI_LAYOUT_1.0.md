# La pantalla, un poco más de instrumento: geometría 1.0

## Lo que cambió

| Franja | Antes | Ahora |
| --- | --- | --- |
| Cabecera (pista, REC, BPM, octavas, batería, medidor de carga) | 0..20 | **0..16** |
| Cuatro tarjetas de los knobs | 24..74 (50 px) | **20..76 (56 px)** |
| Panel (gráficas, listas, piano roll) | 76..198 | **78..200** |
| Pie (motor, preset, página) | 202..240 (38) | **204..240 (36)** |
| MENU > LARGE: tarjetas / panel | 28..132 / 136..198 | **20..124 / 128..200** |

Las dos franjas de estado cedieron cuatro filas cada una y las tarjetas —la superficie donde se trabaja—
ganaron seis. El pie sigue teniendo sus dos líneas (terminan en la fila 30, así que sobran dos filas de
relleno por abajo) y la cabecera ahora mide exactamente lo que miden sus iconos: 16 px.

Ajustes que vinieron con la geometría:

- **El medidor de carga de la cabecera** estaba colgado del pie de la barra: con 20 filas entraba, con 16
  perdía sus celdas de arriba. Ahora las cinco celdas (14 filas) van centradas en la barra.
- **La etiqueta de una tarjeta** se dibuja en la cara de 15 px (la misma de su valor) cuando entra en la
  celda que le deja el knob; antes sólo MENU > LARGE la usaba y en el resto se leía como una línea fina al
  lado de su propio número, que ya era M. Si no entra, sigue en la de 12.
- **El chip "A" de automatización** se centra en su propio chip con las mayúsculas de la fuente de 12, que es
  la que dibuja la letra: con la etiqueta en M el chip creció y la A quedaba dos filas alta.
- **La barra de valor de una tarjeta** (el gauge) bajó a las filas 48..50: la tarjeta es más alta y el gauge
  ahora cierra su pie en vez de quedar a media altura.

## Las filas de lista: un relleno tramado en vez de cuatro bandas

`cv_plate_ground` pintaba una placa corta (16..18 filas, las de una fila de lista) en cuatro bandas, porque a
esa altura el degradado de una fila por píxel no se resuelve y eran cuatro rellenos donde antes había uno.
Ahora, con `h < 24`, la placa se pinta con **un solo relleno tramado** entre los dos extremos de su propia
rampa (`gw_dither`, la trama ordenada 4×4 que el resto de la interfaz ya usa): el mismo paso sobre los mismos
píxeles que la fila plana que reemplazó, y una textura fina en lugar de cuatro escalones visibles. Las
tarjetas, que son donde la profundidad se lee, conservan la rampa por bandas.

**Lo que está medido y lo que no**: medido en host, `tests/ui_render.c` sobre MENU, los píxeles rellenados por
cuadro son **47 940** antes y después (los mismos: la trama pinta el mismo rectángulo), y el tiempo de
redibujado completo dio 118,5 µs antes y 121,5 µs después en dos corridas sueltas de la misma VM — dentro del
ruido de una máquina compartida, así que **no se declara ninguna mejora de tiempo**. Lo que sí cambia es el
número de rellenos por fila de lista: cuatro llamadas pasan a una.

## Verificación (clase: host, binarios nativos de esta VM)

- `tests/ui_test.c` completo: **0 FAIL, `ui: all passed`** (geometría, cabecera centrada, LARGE, roll, soak).
- `tests/ui_render.c`: **0 hallazgos de lint** en 190 pantallas × 22 paletas, **0 desalineaciones** en
  3 554 249 mediciones de 73 sitios (la auditoría de alineación atrapó el chip "A" y el medidor de carga, y
  los dos están corregidos).
- `tests/theme_test.c`: contraste y blending **pasados** en las 22 paletas (peor margen ×1,02).
- Build AC791N: imagen 583 188 B, `.data+.bss` y pool dentro de presupuesto, paquete 610 959 B `FM-1_965`.

Nada de esto está medido en el FM-1 físico.
