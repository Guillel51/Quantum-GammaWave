# La matriz de modulación: ocho slots por pista

**Clase de la verificación: host x86.** `tests/mod_test.c` (motor y audio), `tests/project_test.c` (formato de
proyecto), `tests/upreset_test.c` (presets de usuario), `tests/ui_render.c` (pantalla MOD: lint, alineación y
render). Nada de esto es una medición del AC791N ni del FM-1 físico.

## Qué había

Cuatro slots por pista (`NMSLOT 4`), cada uno tres parámetros guardados en `track_t.p[]`:
`P_M1SRC/P_M1DST/P_M1AMT` .. `P_M4SRC/P_M4DST/P_M4AMT`. La página MOD los dibujaba como cuatro filas y
KNOB 1 elegía el slot.

## Por qué los cuatro nuevos no son parámetros

Los ids de parámetro son **7 bits**: los eventos de `motion` guardan su id con el bit 7 reservado para el lock,
y `P_COUNT` ya está en **125 de 127**. Un slot necesita tres ids y quedaban dos. Por eso los slots 5..8 viven en
un campo propio de la pista, `track_t.mx[4][3]` (`core.h`), y no en `p[]`.

Consecuencias, dichas sin adornos:

- **No son automatizables** (`motion`, los locks de paso y el editor trabajan por id de parámetro). Los
  slots 1..4 siguen siéndolo.
- **El protocolo del editor no los transporta**: un preset guardado desde el editor de escritorio deja los
  slots 5..8 a cero, como los dejaba cualquier firmware anterior con los slots que no conocía.
- **La vista de HOME (`hv_mod`) sigue enseñando los cuatro primeros**: es un resumen de media pantalla, y ocho
  filas no caben sin quitarle la traza. La página MOD es donde están los ocho.

## Motor (`mod.c`)

`NMSLOT` pasa a 8. Un único accesor, `mslot(t, k, f)` (f: 0 SRC, 1 DST, 2 AMT), devuelve el valor de `p[]` para
los cuatro primeros y de `mx[]` para los otros cuatro, y es lo que leen `mod_scan`, `mod_block_slot` y
`macro_master`. El resto del motor no cambia: los slots 5..8 son fuentes, destinos, sumas, clampados y
semitones exactamente iguales que los 1..4.

`mod_src_live(t, k)` (nuevo) devuelve el valor **actual** de la fuente del slot en Q15, para la pantalla: la LFO
tal como la ve el bloque, las fuentes por voz del último note-on, los macros y la secuencia de modulación donde
está. Se lee desde el bucle principal, nunca desde la ISR: muestra un bloque de retraso y no toca nada.

## Almacenamiento

- **Proyecto**: bloque nuevo `'M' 'X'` al final del registro variable, 3 bytes por slot y pista (SRC, DST,
  AMT sesgado +64), escrito **solo si algún slot está puesto**. Un firmware anterior lee los bloques que
  conoce y termina antes de este, así que un proyecto con slots 5..8 sigue cargando en él (sin ellos), y un
  proyecto suyo se carga aquí con los cuatro nuevos a cero.
- **Preset de usuario**: los bytes del registro **más allá de `np`** (130..141, marca `'M'` en 142), donde no
  vive ningún parámetro. Sin cambio de versión y sin marca de fábrica: un registro antiguo se lee como ninguno,
  y `up_valid` no cambia (solo mira `i < np`). Mismo mecanismo que ya usaba `UP_FM_AT` en el byte 143.
- **Presets de fábrica**: no los llevan (su tabla no puede expresarlos). Los bancos cargan como siempre.

## Pantalla MOD (`ui_graph.c graph_mod`)

Ocho filas de 15 px (`1` .. `8`), cada una `n SRC > DST AMT` y, a la derecha, el **valor vivo** de la fuente:

- El **AMT** se formatea con `F_BIPCT`, así que el signo es siempre visible: `+50`, `-50`, `0`. Es la
  polaridad de la asignación.
- La **barra viva** va del centro hacia un lado para las fuentes bipolares (LFO, KEY, RAND, NOISE) — el lado es
  el signo del momento — y desde la izquierda para las demás (ENV, VEL, MODW, AT, EXPR, STEP, M1..M4).
- Un slot sin SRC, DST o AMT se dibuja apagado y sin barra, como antes.
- KNOB 1 recorre los ocho slots (`SLOT n/8`); KNOB 2..4 editan SRC, DST y AMT del elegido, con las tarjetas
  grandes igual que los cuatro primeros (`params.c page_desc` devuelve descriptores propios para 5..8).

## Verificación ejecutada

| Prueba | Qué comprueba | Resultado |
|---|---|---|
| `mod_test` | Un slot de 5..8 modula (M1 → PAN), **da exactamente el mismo offset** que el mismo routing en el slot 1, y los slots 1..4 y 5..8 **suman**; un slot de 5..8 llega al audio (LFO → CUT); `mod_src_live` devuelve el valor de la fuente y 0 en un slot vacío | pasa |
| `project_test` | Los slots 5..8 van en el proyecto y vuelven; las demás pistas y slots siguen a cero | pasa |
| `upreset_test` | Los slots 5..8 viajan en los bytes sobrantes de un registro, `up_valid` los acepta, y sin marca se leen como ninguno | pasa |
| `ui_render` | 189 pantallas × 22 paletas: **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles GREY/MONO fuera de gris, 3 401 666 mediciones de alineación con **0 desalineaciones** | pasa |
| `theme_test` | Contraste de todas las paletas (peor margen 4.08:1) | pasa |
| `install_test.py`, `web/test_web.mjs` | Instalador y web | pasan |

El render de la página MOD se revisó a ojo sobre `GAMMA_mod.ppm` (ocho filas, `2/8`, `+62` / `-100` y la barra
viva en las filas activas).

## Lo que no se hizo y por qué

- **No hay ids nuevos**: habría exigido pasar los eventos de `motion` a ids de 8 bits y migrar todos los
  proyectos y presets. Los slots 5..8 no son automatizables, y eso está dicho arriba.
- **No se tocó `hv_mod`** (HOME) ni el protocolo del editor.
- **No hay medición de coste en el AC791N**: `mod_scan` recorre ahora ocho slots en vez de cuatro (cuatro
  lecturas más por pista y bloque, con la matriz apagada incluidas); el bloque de audio son ~725 µs y esto es
  una fracción que no se puede afirmar sin el build de destino, que aquí no existe (falta `JIELI_TOOLCHAIN`).
  `mod_test` sigue pasando su presupuesto de coste con todos los slots en uso (`MOD_COST_MAX` 5 %).
