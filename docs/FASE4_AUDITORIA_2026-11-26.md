# FASE 4/6 — auditoría de todas las pantallas y pulido

Fecha: 2026-11-26. Base: `7539974f` (FASE 3). Alcance del cambio: `firmware/src/ui_graph.c` (gráfico de la página
FILTER), `tests/ui_pixels.txt` (base de píxeles).

## 1. Método

El barrido de `tests/ui_render` pinta **207 pantallas × 22 paletas = 3 305 cuadros**; el lint de alineación, las
tintas sobre su propio color y los píxeles GREY/MONO fuera de gris se comprueban en ese mismo paso. A eso le sumé dos
medidas por pantalla, para no juzgar por impresión:

- **tinta**: píxeles distintos del color dominante, y filas totalmente planas;
- **hueco muerto**: la corrida más larga de filas consecutivas sin nada dibujado, y en qué `y` empieza.

Sobre las 195 pantallas interactivas (fuera el arranque, la calibración, los diálogos y los mensajes), el ranking de
hueco muerto dijo dónde mirar. Revisé las 207 en tres hojas de contacto a tamaño real y amplié 22 de ellas.

## 2. Lo corregido

**La respuesta del FILTER no llenaba su panel.** `filter_pass` escalaba la curva contra el techo fijo de
`flt_mag` (1180). Con eso, un tipo cuya curva nunca baja —SHELF vive entre 400 y 1000, y su único desnivel es el
escalón del codo— se dibujaba en el tercio superior y dejaba dos tercios de banda vacía debajo: exactamente el
«panel con tarjeta vacía» que este gráfico vino a arreglar. NOTCH y VOWEL tenían el mismo problema al revés (su
única caída era un pellizco). Ahora cada tipo se escala a **su propio recorrido** sobre el barrido (`lo..hi` con un
duodécimo de margen por lado), las cuatro pasadas de profundidad comparten ese recorrido —así las copias siguen
siendo copias—, y la línea base es el fondo de ese recorrido, no un 0 dB absoluto que el boceto nunca prometió.

Medido en el barrido (GREY, corrida de filas planas más larga y `y` donde empieza):

| pantalla | antes | después |
|---|---|---|
| `filter_shelf` | 71 px desde y=142 | **41 px desde y=172** |
| `filter_notch` | 55 px desde y=158 | **41 px desde y=172** |
| `filter_vowel` | 44 px desde y=169 | **41 px desde y=172** |
| `filter_scream` | 60 px desde y=153 | **41 px desde y=172** |

Los 41 px que quedan son el margen inferior común de toda pantalla con panel, no hueco de la gráfica. La curva ocupa
ahora el alto útil en LP, HP, BP, COMB, VOWEL, SCREAM, NOTCH, SHELF y FOLD. La base de píxeles se regeneró
(**65 cuadros cambiados**, 0 perdidos, 0 nuevos: las páginas FILTER en sus paletas), y el lint del barrido sigue en
**0 hallazgos, 0 tintas sobre su propio color, 0 píxeles GREY/MONO fuera de gris**.

Medido [AC791N]: imagen **580 132 B** de 583 612 (**3 480 libres**), alcance de llamadas desde RAM 10 396 B
(mínimo 8 192), RAM 75 664, POOL y LOWBUF iguales.

## 3. Lo que la auditoría encontró y queda registrado

Con la medida al lado, para que la fase siguiente empiece por lo peor:

| pantalla | hueco | qué le falta |
|---|---|---|
| `auto_list_add` (SEQ > AUTO LIST sin locks) | **109 px** desde y=97 | lista vacía: una línea de puntos y `ATK`/`GRID`. Necesita la nota de lista vacía que el browser ya tiene. |
| `shift` (SHIFT 9/15) | **102 px** desde y=111 | la banda de gráfica queda vacía; sólo hay una traza en el extremo superior derecho. |
| `pattern` (PATTERN 8/15) | **102 px** desde y=111 | ídem: gráfica sin contenido visible en la mayor parte de la banda. |
| `motion_locks` / `motion_off` / `motion` | **96 / 59 px** | igual que `auto_list_add`: sin locks, la banda no dice qué hacer. |
| `tilt` | 73 px desde y=140 | la respuesta de TILT usa media banda por diseño (el cuerpo es plano); se puede dar escala propia como a FILTER. |
| `lfo_shape` | 70 px desde y=143 | la onda no usa el alto del panel; escala propia pendiente. |
| `presets_nofav` | 63 px desde y=150 | la nota está a 30/60/82 y el resto es fondo: podría llevar el filtro activo y el conteo de la categoría. |
| `master` | 42 px desde y=171 | el panel PUNCH deja el tercio izquierdo con la curva sola. |

Estas pantallas **no** se tocaron en esta fase: son correcciones independientes, cada una con su propia verificación,
y no quise meterlas a medias en el mismo commit que el cambio de FILTER.

## 4. Pendientes de fases anteriores (sin cambios, con su razón)

- **CLOUD 0,1 dB** (SIZE 127/RATE 127): 80,8 contra 80,9. La realimentación del lazo no se ejecuta a SIZE 127
  (línea congelada), así que compensar con RATE no mueve la medida; probado y revertido en la FASE 3. La palanca es
  la normalización de solapamiento de los granos, en `fx.c`, con su propio banco de pruebas.
- **Interletraje `Gl` a 12 px**: 0,258 contra 0,25 por cuantización del rodamiento en la tabla rasterizada. Exige
  tablas AA por fase (×4 de flash) o posicionamiento subpíxel en `cv_text`, y después re-basar los 3 305 cuadros.
- **Dos presets de fábrica demasiado parecidos** (`HARP PLUCK` / `SLOW TIDE`, 0,044): es rediseño de contenido y
  arrastra `tests/golden.txt` y `tests/cpu_baseline.txt` de `regress` (316 renders + presupuestos por preset) en el
  mismo commit; regenerarlos a ciegas debilita esa red, así que se hace aparte y a propósito.
- **`tilt_test` dentro de la suite**: ruido de reloj del host (1 562,5 ns en la suite, 0,0 ns suelto, mismo binario
  en los dos árboles). No es un fallo del firmware.

## 5. Lo que sí quedó comprobado

- 207 pantallas × 22 paletas: **0 lint, 0 tintas sobre su propio color, 0 desalineaciones, 0 píxeles fuera de gris**.
- Navegación (`nav`, dentro de la suite de la fase 3): 14 estados, 0 difieren de un repintado completo.
- Parpadeo (`flicker`): 0 px borrados y devueltos, 0 px de banda perdida.
- Cabecera y pie (BPM, batería, grabación, ruta) presentes en las 207: la dibuja la banda común, no cada página.
- No probé en hardware FM-1: no hay unidad. Todo lo anterior es [HOST] salvo el tamaño de imagen y el alcance, que
  son [AC791N].
