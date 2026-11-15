# Interfaz Quantum — estado real y qué cambió (FASE 4)

## 1. Lo que ya estaba, comprobado en el código (no hace falta hacerlo)

* **Los temas AMBER, ICE y VIOLET ya no se ofrecen.** `firmware/src/ui_palettes.h:47`:
  `#define UI_NPAL_LIVE 19u /* (3.5) the palettes the menu offers: AMBER, ICE and VIOLET retired */`. Las 20
  entradas siguen en la tabla a propósito, porque un proyecto guardado referencia la paleta por índice y
  sacarlas cambiaría los colores de proyectos viejos.
* **El indicador de página existe y es común a todas las familias**, en la zona inferior (el "footer"): el
  `draw_foot()` de `ui_draw.c` escribe `HOME 1/5`, `ENV DEST 2/2`, `DLY 2/2`… incluso para HOME, que era el
  único sitio donde se había perdido. Moverlo a la barra superior es un rediseño, no un arreglo: hay que
  rehacer el reparto de la barra (transporte, BPM, OCT, íconos) y medirlo contra el lint de alineación.
* **La identidad ya es Quantum y no un naranja plano**: `ui_being.c:1257` documenta la decisión de los cinco
  tonos de cuerpo distintos para los planetas, y `heat_col()` da el degradado térmico de la paleta GAMMA.
  Las capturas de `docs/gallery/` son el estado actual, renderizado desde el firmware.

## 2. Lo que cambió en esta fase

**Los knobs que no hacen nada en una página ya no muestran su "K1".."K4".** En el layout de tarjetas altas
(`draw_column_tall`), una tarjeta vacía dibujaba igual la píldora del knob en `T_DIM`: un control etiquetado
donde no hay control. Ahora la tarjeta vacía se dibuja y retorna antes de la píldora, así que no gasta ni los
píxeles de la píldora ni su glifo.

Medido, no estimado:

| | Antes | Después |
|---|---|---|
| Imagen AC791N | 583 600 B | **583 584 B** (−16 B) |
| XIP código | 412 976 B | 412 956 B |
| XIP constantes | 150 232 B | 150 236 B (el texto del comentario no; el del cartel, sí) |
| Diagnóstico | — | `image 583584 B; RAM .data+.bss 88148 B of 98304; pool 305096 B, spare 15392 B` |

O sea: además de sacar una etiqueta falsa, **liberó 16 B de XIP** en vez de costarlos. El margen pasa de
12 a 28 B.

Verificación: `tests/ui_test` → *all passed*; barrido de render `ui_render` sobre las **189 pantallas × 22
paletas** → **0 hallazgos de lint, 0 tintas sobre su propio color, 454 textos elididos (los mismos 9 de
siempre), alineación 0 px de desvío en 3 398 185 medidas**. Clase: host x86 sobre el mismo código que compila
el AC791N; no es una medición en la unidad.

## 3. Galería

`docs/gallery/all-screens.png` — hoja de contactos con **las 189 pantallas** en la paleta GAMMA, generada del
render del firmware. Los originales 1:1 de las cinco vistas de HOME y de EDIT > VOICE están al lado
(`home-main.png`, `home-visualizer.png`, `home-overview.png`, `home-mixer.png`, `home-performance.png`,
`edit-voice.png`).

## 4. Lo que no se hizo, y por qué

El rediseño integral que pide la fase (gráficos nuevos de HOME, espectro en el visualizador, medidores en el
MIXER, columna vertebral visual de EDIT, rejilla de SEQ, consola de FX, pantalla de PUNCH) es código de
dibujo nuevo, y cada pantalla así cuesta entre cientos de bytes y unos pocos kilobytes de XIP. Con **28 B**
libres, ni la más chica entra; la página `VOICE 3` de la fase anterior ya lo demostró midiendo el fallo del
linker.

El orden para poder hacerlo es el de `docs/ENGINE_EXPANSION_PLAN.md` §3: recuperar flash de los assets
generados (fuentes 31 273 B, `WT_DATA` 12 288 B, line art 10 861 B, `UAC_FIR` 7 680 B, keycaps 4 792 B), con
el barrido de render y los 293 renders dorados como prueba de que nada cambia de aspecto ni de sonido.
