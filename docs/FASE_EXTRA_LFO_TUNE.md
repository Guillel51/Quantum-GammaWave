# Fase extra — LFO en nueve pantallas, DEST con profundidad, SCL TUNE sin escrituras cruzadas

Clase de toda medición de este documento: **[HOST]**. No hay unidad FM-1 en esta sesión: nada aquí es una
medición de hardware, ni del emulador, ni del AC791N.

## 1. Lo que estaba mal

Auditado sobre el árbol en `3ff9efd`, antes de tocar nada:

| Dónde | Defecto |
| --- | --- |
| `firmware/src/params.c` `PAGES[]` | `FAM_LFO` tenía **diez** pantallas y la sexta, `MOD MAP`, era la página MOD en modo mapa: sólo lectura, sin un solo control propio (`mod_ui_slot` no la seleccionaba, ningún knob la editaba). |
| `firmware/src/tune.c` `tune_ui_sync()` | Al cambiar la nota, el código releía los cents de la nota **nueva** en `tune_ui_ct_l` pero comparaba con el `ct` local de la nota **anterior**. Al diferir, `tune_set(nota_nueva, cents_viejos)` escribía el valor del knob CENTS en otro grado: mover NOTE movía —y reescribía— el parámetro de CENTS. |
| `firmware/src/ui_graph.c` `graph_signature()` | No incluía ni el temperamento (`tune_ui_*`, `tune_dirty`) ni las notas sostenidas (`t->nheld`, `t->held[]`). La página TUNE sólo repintaba cuando algo ajeno cambiaba: los knobs no movían el círculo y las notas de la mano no se encendían. |
| `firmware/src/ui_tech.c` `tech_dest()` | LFO DEST / ENV DEST eran **cuatro columnas bipolares** bajo las tarjetas: sin nombre, sin valor, sin escala, sin decir hacia dónde mueve una ruta. |
| `firmware/src/ui_scl.c` `graph_scl_tune()` | El comentario prometía «un radio desde un anillo común» y el anillo no existía: con el temperamento en 0 cents cada radio mide 0 px y la rueda se leía como polvo. Las notas sostenidas eran un punto fijo. |

## 2. Lo que se hizo

### MOD MAP, eliminada

- Se quitó su entrada de `PAGES[]` (quedan **nueve** pantallas `FAM_LFO`: LFO, LFO DEST, LFO CURVE, LFO SHAPE,
  MOD, MSEQ, MSEQ 2, MACROS, SCENES), su valor `GR_MODMAP` del enum de gráficos y las dos ramas `map` de
  `graph_mod()`, que vuelve a ser una sola función de ocho filas. El contador de familia (`2/9`, `4/9`…) lo
  calcula `page_desc`/`params.c` desde `PAGES[]`: no había ningún número escrito a mano que actualizar.
- `tests/ui_render.c`: fuera el estado `S_MODMAP` (enumeración, nombre y setup). El barrido pasa de 204 a 203
  pantallas; la baseline de píxeles pasa de 3 257 a 3 241 cuadros (16 `mod_map.ppm` desaparecen, 231 cambian por
  el contador y por las pantallas rediseñadas).
- `docs/MANUAL.md` y `docs/MANUAL.es.md`: fuera la fila y el párrafo de MOD MAP; las dos páginas DEST quedan
  descritas como son ahora.

### LFO DEST / ENV DEST

Dos bandas en el panel (x 10..230, y 4..105):

1. **La fuente en profundidad.** El ciclo del LFO (`lfo_wave`) — u ocho ciclos cuando la onda es S&H, que es una
   escalera y no una raya — o el ADSR del track, dibujado tres veces con `qz_curve` (el motor de profundidad que
   la página ENV ya usaba: la capa de delante a 2 px y encendida, las de atrás más chicas, levantadas y más
   oscuras). Encima, la aguja de la fase (`lfo_ph`) y el punto del nivel actual (`lfo_val × lfo_fade`); en el
   envelope, una regla encendida con el nivel del voice más fuerte.
2. **Una placa por destino.** Nombre (`page_desc`), raíl bipolar con el cero y los cuartos, **alcance** (barra
   tenue = lo que permite el knob) y **ahora** (barra del acento hacia un lado, tinta media hacia el otro, pip en
   la punta = `amt × fuente`), y el valor con el formato que usa la tarjeta (`param_format`, F_BIPCT / F_PCT).
   `LOOP` de ENV DEST es `F_ENUM`: dibuja su chip y las envolventes que repite, **sin raíl**, porque un modo no es
   una profundidad y no debe leerse como una ruta en cero.

Nada de esto es un temporizador: todo sale de `lfo_ph`, `lfo_val`, `t->p[]` y las voces, y la firma del gráfico
(`GR_NONE` → `ui.frame / 2`) ya repintaba estas páginas cada dos frames.

### SCL TUNE

- **La escritura cruzada, cerrada.** Los dos knobs se distinguen por el propio knob: `tune_ui_ct` es lo que tiene
  el encoder y `tune_ui_ct_l` lo último aplicado; difieren sólo si CENTS se movió desde entonces. Si se movió, ese
  giro es una edición de la nota ahora seleccionada y queda; si no, la selección trae los cents propios de la nota
  al knob y la comparación siguiente ve «sin edición» y no escribe nada.
- **La mano, visible.** Las notas sostenidas llevan una regla bajo su letra (se leen aunque el temperamento esté en
  0 cents) y un pip que recorre su radio hacia el borde y se apaga al llegar; el desfase por índice de nota hace
  que un acorde se lea como un abanico y no como un parpadeo.
- **El anillo común**, un polígono de doce segmentos en `T_LINE`: la desviación ya tiene de dónde salir.
- **La firma.** `graph_signature()` suma los cuatro números de la tabla, `tune_dirty` y las notas sostenidas, más
  `ui.frame / 4` mientras hay teclas: ~15 fps de animación, y cero coste cuando la página está quieta.

## 3. Pruebas

- `tests/tune_test.c`, caso 11 nuevo: con D a +17 cents, girar NOTE a F deja `tune_get(5) == 0` y no toca ningún
  otro grado; volver a D encuentra sus 17 cents. **13/13 ok** `[HOST]`.
- `tests/ui_render.c` + `python3 tests/ui_pixels.py build/ui_a/ppm --update`: 203 pantallas × 22 paletas + el
  barrido de páginas/valores; **0** desviaciones de 1 px en 3 967 423 mediciones de alineación; **0** tintas sobre
  su propio color; **0** píxeles fuera de gris/neutro en GREY y MONO; **0** hallazgos de lint nuevos (las 4 267
  líneas del informe son idénticas, categoría por categoría, a las del árbol padre: ver §4). El estado `LFO DEST`
  del barrido pone el LFO en marcha (`lfo_ph`, `lfo_val`, `lfo_fade`) para que el frame pruebe que la aguja, el
  punto y las barras vivas se dibujan desde el estado. `[HOST]`
- Suite completa `NO_PACKAGE=1 tests/run_tests.sh`: **6 387 `ok`**, `regress` con **311 renders golden, 0
  cambiados**, 0 fallos de health, 0 de voces/routing, 0 de presupuesto de CPU. `[HOST]`

## 4. Lo que sigue rojo, y no es de esta fase

`tests/run_tests.sh` termina en `HOST TESTS FAILED` por tres pasos, ninguno causado por estos cambios:

1. `release set: …` — el árbol va por delante del paquete publicado; es el estado esperado hasta publicar.
2. `UI renders: …` y `UI renders with FELUCCA_FM4=1: …` — `ui_render` devuelve 1 mientras haya hallazgos de lint
   (`return nfind || …`). **Reproducido en un worktree limpio del commit padre `3ff9efd`**: mismas 4 267 líneas
   (y 3 651 con `FELUCCA_FM4=1`), mismas categorías (483 «FM6 part through a box», 113 «spills out of its cell»,
   el resto en los gráficos de DX IMAGE y los charts de FM6), mismo `EXIT=1`. Esta fase **no añadió ni un
   hallazgo** y no silenció ninguno: la comparación de píxeles, la alineación y GREY/MONO sí pasan.
