# QG — HOME multipantalla

**Fase:** 4/6 · **Estado:** navegación + OVERVIEW + SYSTEM **implementados y medidos**; las otras cuatro vistas, especificadas.

## 1. Lo que hace ahora HOME

`ui.c` `go_home()` camina una lista de pantallas y la que sigue a la última vuelve a la primera:

| # | Pantalla | Qué muestra | Estado |
|---|---|---|---|
| 0 | **HOME** (ecosistema) | las cuatro cards con sus parámetros, los planetas del sistema | ya existía, intacto |
| 1 | **VISUALIZER** | osciloscopio, espectro, fase, VU de las seis pistas | ya existía, intacto (`home_viz`) |
| 2 | **OVERVIEW** | motor, patch, sonido, tempo, swing, transporte, master, y los patrones de las seis pistas en 64 celdas cada una | **nuevo** |
| 3 | **SYSTEM** | versión, carga del ISR de audio, voces en uso, batería, uptime, reloj, y las voces de cada pista | **nuevo** |

Desde otra sección, la primera pulsación **sólo** vuelve a HOME (es lo que ya hacía); las siguientes avanzan una
pantalla cada una. Al terminar el ciclo se vuelve a HOME. Sin botones nuevos: el mismo HOME.

**Estado en RAM:** `ui.home_view` (1 byte). El visualizador conserva su bandera `ui.home_viz`, así que todo el
código que preguntaba "¿está el visualizador arriba?" sigue leyendo un solo flag y no se tocó ninguna de sus
condiciones.

## 2. Detección de pulsaciones (lo que pedía la fase)

`ui_input.c` `btn_hold()` es la única fuente, y ya tenía la forma correcta —verificada, no supuesta:

| Gesto | Qué entrega | Efecto en HOME |
|---|---|---|
| pulsación corta | **un** `BT_TAP`, al soltar | avanza una pantalla |
| pulsación repetida | un `BT_TAP` por pulsación (el contador `t0` se limpia al soltar) | una pantalla por pulsación, nunca dos |
| pulsación larga (0,7 s) | **un** `BT_HOLD`; el bit 2 de `t0` marca que ya disparó | abre el menú |
| soltar después de una larga | `BT_NONE` (el bit 2 anula el tap) | no avanza nada |

Un detalle que hubo que arreglar: `menu_close()` llamaba a `go_home()` "para volver a HOME", y con el ciclo nuevo
esa llamada **avanzaba** una pantalla desde HOME. Ahora cierra en HOME (la primera), con el pulso del sistema, sin
caminar nada: cerrar el menú no es una pulsación de HOME. Antes, además, esa misma llamada encendía el visualizador
por un doble toggle (`home_viz = 0` y después `home_viz = !home_viz`).

## 3. Las dos pantallas nuevas

Cuerpo completo (filas 26..201), panel propio, **sin cards**: las cards siempre ajustan un parámetro, y estas dos
pantallas no ajustan nada. Se dibujan en dos pasadas de 240 × 88, como el visualizador, porque `CV_MAX` es 240 × 124.

**OVERVIEW** — "¿qué hay cargado y qué está haciendo?":

- `ENGINE` + nombre grande, `SOUND` (el preset de usuario o el de fábrica, la misma función que usa el pie), `07/56`.
- `TEMPO` con BPM y swing, `STATE` con PLAY / STOP / REC, `CLOCK EXT/INT`, `MASTER` con su barra y su %.
- `STEPS`: las seis pistas, cada una con sus 64 pasos en celdas de 2 px (llena = ese paso tiene notas o golpes,
  acento = la pista seleccionada), un `-` si está vacía, y a la derecha el pico que está midiendo ahora.

**SYSTEM** — "¿cómo está el instrumento?":

- `v2.7.4.8`, `AUDIO CPU` con barra y % (`song.cpu_q8`, el mismo número que GLOBAL > SYSTEM), `VOICES` en uso,
  `BATTERY` con los cuatro escalones de `ui_draw.c` (531 / 561 / 591), `UPTIME` desde `fm1_ms`, `CLOCK`.
- `PARTS`: por pista, su motor, sus voces ahora y una barra sobre 12.

Ningún número es inventado: todos salen de campos que el firmware ya mantiene. `BATTERY` es un **escalón de
cuatro**, no un porcentaje (el ADC no da uno), y `NOT READ` sale mientras `batt_raw` sea 0.

## 4. Evidencia

| Comprobación | Resultado |
|---|---|
| `tests/ui_test.c` `test_home_views` (nueva) | verde: 1 tap desde una sección → HOME; 4 taps → SCOPE, OVERVIEW, SYSTEM, HOME; la larga abre el menú y ni ella ni su suelta caminan; las dos pantallas no tocan página ni sonido; entrar a una sección desde ellas no deja nada de HOME |
| `tests/ui_test.c` completo | **all passed** (incluye el test de MOTION en las cards, adaptado a que HOME ya no alterna visualizador) |
| `ui_render` | **187 pantallas** × 17 paletas + el barrido de páginas: **0 hallazgos de lint**, **0 px de desalineación** en 2 777 796 medidas, 0 tintas sobre su propio color |
| `ui_render` escenas nuevas | `home_overview`, `home_system` (revisadas en PNG) |
| Capturas | `build/ui_new/GREY/home_overview.png`, `home_system.png` |
| STYLE LINE | las reglas de las cards ya no se dibujan en estas pantallas (eran 170 hallazgos de "texto sobre divisor") |
| Emulador | `web/emu/felucca_web.c` incluye las fuentes nuevas; `startup_test` compila y pasa |
| CPU de las pantallas | **no medido en target**: sin toolchain AC791N. Se dibujan con las mismas primitivas y en las mismas dos pasadas que el visualizador |

## 5. Lo que falta (entregas siguientes)

| Entrega | Contenido | Por qué en este orden |
|---|---|---|
| 1 | **PERFORMANCE** | efectos, macros y estado de interpretación: es lo que se mira en vivo, y sus datos ya existen (FX por pista, macros, cadena) |
| 2 | **MIXER** | niveles, pan y medidores **por pista** en grande; hoy el mixer vive en GLOBAL, y su sitio natural es HOME. Antes hay que confirmar los campos de nivel/pan que usa el mixer de GLOBAL |
| 3 | **MODULATION** | LFO, destinos y profundidad de la pista: la página AUTOMATION ya los lista, ésta los resume |
| 4 | **SOUND ENGINE** | motor activo y sus parámetros destacados, con el dibujo que cada motor ya tiene (no uno común) |

Ninguna de las cuatro repetirá lo que ya dice OVERVIEW: OVERVIEW es "qué hay", PERFORMANCE "qué está sonando",
MIXER "a qué volumen", MODULATION "qué se mueve", SOUND ENGINE "cómo se edita".
