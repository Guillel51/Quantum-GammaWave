# FASE 3/6 — profundidad y ambiente compartidos

Fecha: 2026-11-26. Alcance: `firmware/src/gfx.c` (componente nuevo), `firmware/src/ui_layer.c` (banda de cuerpo y
su firma de redibujo), `firmware/src/core.h` (tablas generadas), y los pendientes de las fases 1 y 2.

## 1. Pendientes de las fases 1 y 2

### 1.1 Resuelto: la suite no compilaba (`core.h` + `CH_INC`)

`tests/upreset_test.c` incluye `core.h` por su cuenta y el inline `chorus_inc()` lee `CH_INC`, la tabla que
`tools/gen_tables.py` escribe en `build/gen/felucca_tables.h`. Sólo `felucca.c` incluía ese encabezado, así que el
firmware compilaba y `upreset_test` no: el fallo cortaba `tests/run_tests.sh` en la primera parada. `core.h` incluye
ahora las tablas generadas bajo `__has_include` (el archivo es `#pragma once`, así que al firmware no le cuesta
nada); `upreset_test` compila y pasa. **Clase de la medida: compilación + test de host.**

### 1.2 Medido y registrado: CLOUD, 0,1 dB en el extremo (SIZE 127, RATE 127)

`tests/cloud_hot_test.c` pide que la cola sostenida quede a menos de 6 dB de la señal con el envío. Con SIZE 127 y
RATE 127 da **80,8 dB contra un umbral de 80,9**. Probé la corrección que parecía obvia —compensar la realimentación
del lazo con RATE (`fg = SIZE*110 + RATE*5/4`)— y **no cambió un solo decimal**: con SIZE 127 la línea está
congelada (`freeze`), la realimentación no se ejecuta y el nivel lo fija la normalización de solapamiento de los
granos. Revertido: tocar el lazo no es la palanca. Queda registrado con la medida, sin relajar la aserción.

### 1.3 Medido y registrado: dos presets de fábrica demasiado parecidos

`tools/preset_audit.py --gate-sound 0.05` marca **ANALOG 'HARP PLUCK' / 'SLOW TIDE' a 0,044** (5 567 pares
medidos, 1 por debajo del umbral). Reproducido en el árbol anterior a esta fase (archivo de `05f7191c`): es
**preexistente**. Arreglarlo es rediseñar uno de los dos presets —contenido, no un ajuste— y queda registrado con la
medida y el par exacto.

### 1.4 Medido y registrado: interletraje 'Gl' a 12 px (0,258 px contra 0,25)

`tests/text_spacing_test.py` mide el hueco entre glifos contra el propio font a 16× y falla sólo en el par `Gl`
del cuerpo S (12 px): **0,258 px**, 0,008 por encima del límite; los cuerpos M (0,044) y L (0,432 contra 0,5) están
holgados. La causa es la cuantización del rodamiento del glifo en la tabla rasterizada, no la colocación de
`cv_text` (que `text_ref_test` fija aparte). Arreglarlo exige regenerar las tablas AA y volver a basar los 3 305
cuadros del barrido: es un cambio de fuente, con su propio presupuesto de píxeles, no un ajuste de esta fase.

## 2. El componente: `ux_ambient()`

Uno solo, en `gfx.c`, junto a `ux_ground_rows` y `ux_ground_bands`:

- **El rango de fondo**: seis bloques en tres filas de profundidad. Cada bloque es **más oscuro que la placa**
  sobre la que está y lleva borde superior iluminado, pie en sombra y canto lateral, y la fila cercana toma el
  acento en su borde. Así se lee como un muro más atrás y no como un agujero en el panel, y la separación de
  planos se ve por los 2–4 px que quedan entre dos controles.
- **El polvo**: dieciocho motas en tres profundidades; tamaño 1/2/3 px, recorrido 1/2/3 px por paso y brillo
  creciente con la cercanía. Es textura, no clima.
- **Determinista**: cada posición sale de un hash de (índice, semilla). El mismo `t` dibuja los mismos píxeles, así
  que una pantalla quieta no cuesta nada y dos pasos de deriva difieren en unas motas. Sin reservas de memoria, sin
  estado de RNG, sin coma flotante y sin tabla.
- **Sembrado por capa** (`l * 2654435761 + ux.gen`): dos capas no abren con la misma imagen.

## 3. Dónde vive y qué cuesta

- Se dibuja en la **banda de cuerpo** de `draw_layer` (la placa de 240 × 122 px que usan GLO, SCL, EDIT, SAVE,
  MACRO, REC y SEQ), dentro de la placa (x 8..232) y con el pie del rango 12 px por encima del suyo.
- **Bordes de la placa** (FASE 3): borde superior iluminado, pie teñido de acento y laterales en sombra. Tres fills
  que hacen que el cuerpo esté delante del ambiente en vez de ser un recorte plano.
- **Reloj de ambiente**: `ui.frame / 15`, ~2 Hz, y esa cuenta entra en la firma de redibujo (`sig`) sólo para las
  capas que no son FX y no están en MONO/BW. La banda se repinta dos veces por segundo en reposo, no treinta.
- **Coste por repintado**: 6 bloques × (cuerpo + 3 líneas) + 18 motas ≈ **1 500 px** de 29 280 de la banda (≈5 %),
  y sólo cuando la banda ya se repinta. La columna de µs del informe de `ui_render` no sirve como medida en este
  host (grano de 50 µs: 300 → 800 µs con px y fills idénticos); los contadores que sí son exactos —**px filled y
  fills**— no cambian en las pantallas que el informe mide, porque ésas no dibujan la banda de cuerpo.

## 4. Medido [AC791N] y [HOST]

| | antes (FASE FX LATCH) | ahora |
|---|---|---|
| imagen | 579 332 B (4 280 libres) | **579 988 B (3 624 libres)** |
| alcance de llamadas desde RAM | 11 184 B | **10 540 B** (mínimo 8 192) |
| RAM / POOL / LOWBUF | 75 664 / 296 156 / 28 488 | iguales |

`./build.sh`: los cinco controles pasan. `ui_render` (3 305 cuadros, 207 pantallas × 22 paletas): **0 hallazgos de
lint, 0 tintas sobre su propio color, 4 073 426 medidas de alineación con 0 desviaciones ≥ 1 px, 0 píxeles
GREY/MONO fuera de gris**. `ui_pixels.py --update`: la base se regenera porque el cuerpo de las capas cambia.
`install_test`, las tres pruebas de web, `upreset_test` y `ui_test`/`perform_test`/`regress` (316 dorados
idénticos) pasan. La suite completa se ejecutó hasta **4 511 comprobaciones** (un reinicio del ejecutor cortó su
cola, en la sección de tablas): **3 fallos, los tres medidos y registrados arriba**, más un artefacto de reloj del
host (`tilt_test` mide 1 562,5 ns dentro de la suite y **0,0 ns** suelto, con el mismo binario en los dos árboles:
el grano de 10 ms, la misma clase que el fallo de presupuesto de CPU que `regress` ya documenta). Las pruebas de
interfaz que importan a esta fase pasaron: `nav: 14 estados, 0 difieren de un repintado completo` y
`flicker: 0 px borrados y devueltos, 0 px de banda perdida`.

## 5. Lo que esta fase NO alcanzó, registrado

Nada de esto queda como sorpresa para la fase siguiente:

- **El rango y el polvo llegan a la banda de cuerpo de las capas**, que es donde dibujan GLO, SCL, EDIT, SAVE,
  MACRO, REC y SEQ. **No** llegan a las vistas de HOME (que tienen su propio visualizador, su estrella y sus
  partículas), al MIXER y las tiras de EDIT (paneles propios), a FX > BUSES, al mapa de FX LATCH (tiene su suelo y
  su fila fantasma propios), ni al Web Studio, que dibuja en el navegador con su propio CSS.
- **No hay variantes por módulo** del componente: hay un solo rango con semilla por capa. Densidad, velocidad y
  altura distintas por familia (por ejemplo más polvo en las pantallas de visualización, menos en las de edición)
  quedan pendientes de diseño.
- **No se añadieron animaciones de respuesta al tacto nuevas** (el pedido 5): lo que existe —el pulso de un botón,
  la ola del bay, el HOT de una tarjeta— sigue igual.
- Las capturas completas de las dos versiones existen: el barrido entero de la versión anterior está en
  `/tmp/ub/ppm` (efímero) y el actual en `build/ui_x/ppm`; en `docs/shots/{before,after}/depth_*.png` quedan tres
  pares revisados. **No revisé las 207 pantallas a ojo**: las revisó el lint y el barrido de píxeles.
