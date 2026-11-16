# FASE 3 — interfaz y visualizaciones (2026-11-16)

Revisión visual del firmware sobre el árbol publicado en `main` (`f61b89b`). Todo lo de acá está implementado y
medido con el renderizador real (`tests/ui_render.c`, 190 pantallas × 22 paletas) y las capturas del proyecto
(`tools/generate_screenshots.py` → `assets/screenshots/`, `assets/ui-art/art/`), regeneradas con este árbol.

## 1. HOME: el osciloscopio estaba debajo del degradado

`gw_draw_forge_bay` (el panel de HOME) dibujaba el trazo de la señal **primero** y después el suelo con degradado
(`(7.3) THE FLOOR UNDER THE CURVE`) y las 28 bandas del espectrograma. El trazo quedaba tapado: se veía solo donde
sobresalía del relleno. Ahora el orden es suelo → bandas → **trazo**, y el trazo usa la tinta de la paleta
(`T_TEXT`) cuando su rampa es neutra (GREY, MONO) y la rampa de calor en las demás, así que cruza el degradado
como una línea en las 22 paletas. Los 293 renders de audio y las 190 pantallas de UI siguen idénticos en todo lo
demás (0 hallazgos de lint, 0 desalineaciones de 3 554 448 medidas).

## 2. Los dos osciloscopios del visualizador, el mismo instrumento

El trazado x(t) y el plano de fase x(t) contra x(t−τ) tenían dos recetas: pozos de distinto tono (`vbg` y `vsf`),
marcos con dos mezclas distintas, una sola línea de grilla en el segundo y la traza directa en el acento. Ahora:

- **los dos pozos** son el tono oscuro de la página, con el mismo marco de 1 px;
- **la misma grilla** de tres divisiones por eje y la misma escala de pies en los dos;
- **las dos trazas** en la rampa de calor, de 2 px, para que el par se lea como un instrumento con dos sondas.

## 3. Los cuatro cuadros de los knobs: una sola tipografía

La etiqueta de cada tarjeta elegía la cara M (15 px) cuando entraba y la S (12 px) cuando no, así que en la misma
fila de cuatro cuadros `DIST` salía en 15 px y `CHORUS` en 12, y la misma palabra cambiaba de tamaño según la
página (`MODE` en M, `FEEDBK` en S). Ahora **todas las etiquetas van en la cara de 12 px**, en la misma línea,
con icono o sin él: los cuatro cuadros de una pantalla y el mismo cuadro en cualquier pantalla se leen igual, y el
valor en M mantiene la jerarquía. Se comprobó en las 190 pantallas × 22 paletas (incluidas MENU > LARGE y STYLE
LINE) con el lint y el chequeo de alineación del propio renderizador.

## 4. ARP

- **Todas las teclas tienen nombre.** Las catorce blancas ya lo tenían pero en `T_DIM` sobre una tecla clara
  (gris sobre gris): ahora en la tinta que la cara acepta, y en el tono del acento la que suena. Las cinco negras
  no tenían ninguno: como una tecla de 9 px no admite `C#` en una línea, llevan la letra arriba y el sostenido
  abajo, las dos dentro de la tecla y en la tinta que se lee sobre su cara.
- **El círculo de notas y el teclado dicen lo mismo a la vez.** Los chips de la ruta ya marcaban el lugar del
  arpegio; ahora además llevan marco en la tinta del texto cuando su nota es una de las que el sintetizador está
  sonando en ese paso (la nota y las tres voces de un acorde), igual que el teclado.
- **Texto y medidores.** Las etiquetas de GATE / SWING / PROB pasan de `T_DIM` a `T_MID` y sus valores a `T_TEXT`;
  la barra baja de 5 px a 3 y sube dos filas, así que ya no se cruza con la línea del pie (`RATE 1/16 …`), que
  además sube a y=106 para quedar entera dentro del panel.
- **Teclado.** Las teclas blancas son 4 px más bajas y el nombre sube a y=42, separado del borde inferior; `ROUTE`
  pasa a `T_MID`.

## 5. Lo que revisó el barrido, y lo que no cambió

`tests/ui_render.c` recorre las 190 pantallas en las 22 paletas y falla si un texto se sale de su celda, se pisa
con otro, toca un divisor, queda cortado por el canvas, se elide sin ser texto libre, o si una tinta cae sobre su
mismo color. Con este árbol: **0 hallazgos**, 0 desalineaciones de 1 px o más en 3 554 448 medidas, 0 píxeles
GREY/MONO fuera de gris. Ese chequeo de "tinta sobre su propio color" es el que cubre el contraste de las pantallas
que no se tocaron.

## 6. Capturas

`python3 tools/generate_screenshots.py` regeneró **todas** las capturas del proyecto desde el renderizador real
(`assets/screenshots/quantum_01_home.png` … `quantum_35`, las cuatro de tema) y los recortes de
`assets/ui-art/art/`. Las que muestran los cambios son `quantum_01_home` (trazo sobre el degradado),
`quantum_16_modern_arp` (teclado, ruta, medidores) y `quantum_18_fullscreen_visualizer` (los dos osciloscopios).

## 7. Medido

- `./build.sh` con el toolchain y el SDK fijados: imagen **579 548 B** (holgura XIP **4 064 B**), `.data+.bss`
  94 772 B de 98 304. La revisión visual cuesta **400 B** de imagen sobre `f61b89b`.
- `tests/ui_render.c`: 190 pantallas × 22 paletas, 0 lint, 0 desalineaciones, 0 tintas ilegibles.
- `tests/regress`: 293 renders de audio (el sonido no se tocó).
- Suite de host completa (`NO_PACKAGE=1 ./tests/run_tests.sh`).

No verificado: **nada en una unidad FM-1** (no hay aparato); el emulador WASM no se recompiló (no hay `emcc` en
este entorno). Las capturas son del renderizador de host del propio firmware, no de la pantalla física.
