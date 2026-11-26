# CHORUS con techo de 6 Hz, EDIT y LFO unificados, fondo a sangre — 2026-11-26

Estado de esta pasada: **implementado, probado en host y construido para AC791N**. No hay ninguna medición
en una FM-1 física en este documento.

Clases de medición usadas aquí: **[HOST]** pruebas y renders en x86, **[AC791N]** build con el toolchain
pi32v2 y el SDK, **[EMU]** no se ejecutó el emulador en esta pasada.

## 1. CHORUS: el RATE, recortado en todo el recorrido

El bus del chorus leía `LFO_INC[song.g[G_CRATE] & 127]`, la tabla del LFO de pista, que va de 0.05 a **40 Hz**.
La página, el encoder, un CC, un preset viejo o un proyecto restaurado podían entregar cualquier byte y el
motor lo aceptaba: no había ningún techo de 6 Hz en el recorrido del parámetro.

Lo que hay ahora:

| pieza | qué hace |
| --- | --- |
| `tools/gen_tables.py` | genera `CH_HZ_X100` (uint16, 0.05 … 6.00 Hz, exponencial con razón 120) y `CH_INC` (uint32, incremento por muestra), 768 B de XIP |
| `firmware/src/core.h` | `F_CHHZ` en el enum de formatos y `chorus_inc(int32_t)`, que **recorta** en 0 … 127 (no da la vuelta a la tabla) |
| `firmware/src/params.c` | `G_CRATE` con `F_CHHZ` y su formateador (`.05 Hz` … `6.00 Hz`); página CHORUS con `GR_CHORUS` |
| `firmware/src/fx.c` | `fx_sleep` y `fx_buses` usan `chorus_inc(song.g[G_CRATE])` |
| `web/editor.html` | `F.CHOHZ` (índice 15, el mismo del firmware) con la ley 0.05 … 6.00 Hz |

El techo vale para todos los caminos porque **todos escriben el mismo byte** y el byte se recorta al leerlo:
interfaz y encoder (clamp a `d->max` = 127), MIDI/CC (misma tabla de descriptores), presets y proyectos
restaurados (el byte, sea cual sea). [HOST] `tests/chorus_rate_test.c` mide la velocidad real del bloque de
audio: **6.000 Hz** en RATE 127 y **6.000 Hz** con un byte corrupto (4000), nunca 40 Hz.

## 2. La gráfica del CHORUS

Antes la página era `GR_NONE`: cuatro números y un panel vacío. `graph_chorus()` dibuja el bus tal como es —
tres derivaciones de una línea de retardo separadas un tercio de ciclo — con:

- **RATE**: la velocidad del ciclo (en cámara lenta: de ~14 s a ~1.5 s por ciclo, porque a 6 Hz el movimiento
  real es un borrón, no una lectura);
- **DEPTH**: la amplitud del barrido, 12 … 46 % del panel;
- **STEREO**: el desfase del par izquierda/derecha, que se cierra en una sola traza en 0;
- **TYPE**: la forma (ensemble seno, muescas del phaser, peine del flanger).

Dos copias más oscuras, levantadas y mezcladas hacia el fondo, dan la profundidad del resto del instrumento.
Nada es aleatorio: cada valor sale de `song.g[]`.

## 3. EDIT: DETUNE y TILT en una pantalla

- La pantalla exclusiva de DETUNE (`VOICE 3`) se eliminó; `TILT` lleva `{P_DETUNE, P_HARM, 0xFF, 0xFF}`.
- `graph_tilt()` dibuja los dos: TILT inclina y aplana la curva de transferencia (y su pasada de profundidad);
  DETUNE dibuja dos copias de la misma curva desplazadas a izquierda y derecha, tantos píxeles como separa a
  las voces del unísono (`k * DETUNE * 56 / 889` cents, `voice.c`), más oscuras y de un píxel.
- [HOST] `ui_test` mueve los dos encoders de la página y compara los píxeles: DETUNE cambia 10 506 px y TILT
  2 260 px del frame.

## 4. LFO: CURVE con SLEW e INVERT

- La página `LFO SHAPE` se eliminó; `LFO CURVE` lleva `{P_LSHP, P_X2, P_X3, 0xFF}` y `page_desc()` sirve los
  tres (el slot 1 es CURVE y cae en la tabla de parámetros).
- `graph_lfo()` pasa la onda dibujada por **el mismo coeficiente de SLEW que aplica `voice.c`**
  (`32767 / (1 + sl*sl/8)`, recorrido a lo largo de la imagen) y por **la misma resta de INVERT**
  (`val -= val * 2 * (127 - inv) / 127`), así que SLEW redondea los escalones y INVERT da la vuelta la onda;
  el cabezal y el punto de nivel leen ese valor.
- [HOST] en el barrido de render, la escena con SLEW 40 / INVERT 90 difiere de la neutra en **3 721 px**
  dentro del área de la gráfica; `ui_test` comprueba además que CURVE mueve la onda dibujada (5 055 px).
  El camino del encoder de esta página es el del LFO y no llega a los dos slots extra: eso lo cubre
  `tests/chorus_rate_test.c` a través de `page_desc()`.

## 5. Fondo y degradé a sangre

`graph_ground_ramp()` pintaba `ux_ground_ramp(7, y0, 226, …)` hasta dos filas antes del pie del cuerpo: la
franja que quedaba a los lados y abajo se leía como fondo asomando por el borde. Ahora cubre **x 0 … 240** y
llega a la última fila del cuerpo (`graph_h()`), por debajo del cromo de la pantalla.

## 6. Nombres de elección que ya no se cortan

`param_format()` guardaba **6 bytes** para un `F_ENUM`: CHORUS se dibujaba «CHORU» y SPRING «SPRIN», y no era
un problema de la tarjeta sino de la cadena. Ahora se guarda el nombre entero (8 caracteres) y la tarjeta
elige la tipografía pequeña cuando no entra el **avance** (antes solo se miraba la tinta y el texto se cortaba
al dibujarse), centrando el valor en la tarjeta si aun así no entra.

## 7. Pruebas ejecutadas [HOST]

| prueba | resultado |
| --- | --- |
| `tests/chorus_rate_test.c` (nueva) | pasa: tabla, recorte, 6.000 Hz del DSP, texto de la página, páginas unificadas y sus mandos |
| `tests/ui_test.c` | pasa: incluye las comprobaciones nuevas de DETUNE/TILT/CURVE |
| `tests/ui_render.c` (lint, alineación, coste, `--regions/--idle/--bands/--headers/--announce`) | pasa; 203 pantallas × 22 paletas, 0 desalineaciones |
| `tests/ui_pixels.py` | base regenerada (los 3 241 frames son esta pasada: cambia el fondo y las páginas) |
| `tests/regress.c` | 316 goldens regenerados; 254 cambiados (presets con envío de chorus), 62 bit a bit; 0 fallos de salud/voces; la única falla es la CPU del host (171 % más lenta que su línea base) |
| `tests/six_track_live_test.c` | 61 hashes regenerados y verificados |
| `tests/phase2_test.c`, `tests/control_map_test.c`, `tests/reverb_test.c`, `tests/dly_echo_test.c` | pasan |
| `node web/test_web.mjs` | pasa con `build/host/desc.json` regenerado |

## 8. Build [AC791N]

Imagen **578 440 B** de 583 612 (5 172 libres), XIP 573 660 B, RAM 75 616 B de 98 304 (22 688 libres). Las
tablas nuevas cuestan 768 B de XIP.

## 9. Pendiente, dicho como pendiente

- **El rediseño de las 203 pantallas con una capa 3D nueva en cada una** no se hizo: esta pasada añade
  profundidad y respuesta a las páginas de CHORUS, EDIT > TILT y LFO CURVE, que son las que la fase pedía con
  parámetros concretos, y deja el barrido general para otra pasada.
- **Animaciones de partículas por control** (reacción al tocar un mando): no implementadas.
- **Ninguna medición física** en la FM-1: no se flasheó ni se tocó la unidad. Todo lo de arriba es [HOST] y
  [AC791N].
- Las dos fallas de CI anteriores a esta pasada siguen igual y no se tocaron: `text_spacing` (Gl −0,258 px) y
  `cloud_hot` (clamps de línea y cola `SIZE 127`), más la CPU del host que no es comparable entre máquinas.
