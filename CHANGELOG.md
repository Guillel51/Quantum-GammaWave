## v1.0.0 — 2026-11-16 — Quantum GammaWave 1.0

La primera versión con un nombre que empieza en uno. **No es un retroceso**: es el mismo árbol que venía
numerándose en la línea 4.x, publicado una vez como 1.0 para que la numeración pública arranque limpia. Todo lo
que estaba en `main` está acá, y además lo que se cerró en estas últimas fases (los detalles de cada una siguen
abajo, en sus propias entradas).

### El instrumento

- **Seis pistas, doce voces, nueve motores** (ANALOG, WHEEL, PHASE, FM6, FORMANT, LOFI, NOISE, WTABLE, DRUM) con
  sus presets de fábrica, el kit DRUM-X y la voz de usuario.
- **Matriz de modulación de ocho slots por pista**: los cuatro primeros son parámetros (automatizables, viajan en
  el proyecto como siempre) y los cuatro nuevos viven en el track, con su página MOD de ocho filas, la barra del
  valor vivo y la polaridad del AMT.
- **Página MACROS** (LFO > MACROS): M1..M4 del proyecto en KNOB 1..4, leídos y escritos con las mismas funciones
  que usan las fuentes M1..M4 de la matriz, con la barra de valor y los destinos que mueve la pista actual.
- **Página ARP contra su propio reloj**: la ruta, el nombre de lo que está sonando y una franja por paso que se
  llena con el tiempo transcurrido del paso y marca dónde cierra el GATE, leída de `arp_pos` / `arp_off`.
- **Longitud de patrón por pista** con evidencia: 4 y 6 pasos girando por separado en el secuenciador real y un
  proyecto que guarda 5 / 7 / 12 / 64 por pista (tests/pattern_len_test.c, docs/PATTERN_LENGTH.md).
- **Web Studio**: el navegador de presets del dispositivo con categorías (11 + ALL), búsqueda por lo que la lista
  enseña y la estrella del favorito en cada fila; los textos siguen al idioma y la lista vacía distingue búsqueda
  de favoritos.
- **Fase 8 de rendimiento**: cada placa se pinta una sola vez (MENU −42,7 %, CONFIRM −45,4 %, FX −29,7 % de
  píxeles rellenados, −5,5 % de tiempo), sin cambiar un píxel de 189 pantallas × 22 paletas.

### Interfaz: la pantalla, más instrumento

La cabecera y el pie cedieron cuatro filas cada uno y las cuatro tarjetas de los knobs ganaron seis (20..76),
con el panel 78..200 y el pie 204..240; MENU > LARGE acompaña (tarjetas 20..124, panel 128..200). La etiqueta de
una tarjeta se dibuja en la cara de 15 px cuando entra —se leía fina al lado de su propio valor—, el gauge
cerró el pie de la tarjeta y el medidor de carga y el chip "A" quedaron centrados en la cabecera que ahora
mide lo que sus iconos. Las filas de lista pintan su rampa con un solo relleno tramado en vez de cuatro bandas.
Detalle y mediciones en `docs/UI_LAYOUT_1.0.md`.

### Corrección posterior al paquete 1.0.0

Las siete comprobaciones de `tests/ui_test.c` que esta entrega listaba como abiertas **quedaron arregladas en
`main` después de empaquetar** (`docs/CARD_STRIP.md`): la franja del valor de una tarjeta repintaba `T_SURF` plano
sobre una tarjeta que es un degradado, y cada dígito que rodaba dejaba una banda de otro tono. `tests/ui_test.c`
pasa entero, también con `FELUCCA_FM4=1`. El paquete de esta versión no incluye todavía ese arreglo: hace falta un
`1.0.1` para que el binario publicado y el árbol vuelvan a ser el mismo.

### Verificación de esta entrega

- **Build AC791N real** (toolchain de JieLi + los tres blobs del SDK fijados por SHA-256), no una simulación: el
  paquete, su manifiesto, `SHA256SUMS` y el registro del build están en `release/`.
- **Emulador WASM** compilado con Emscripten 6.0.11 a partir del mismo árbol, con su manifiesto (versión, commit,
  SHA-256 del wasm).
- **Host**: la suite de `tests/run_tests.sh` (goldens de audio 293/0 sin cambios, health, voz, CPU, UI, editor,
  persistencia, formatos de proyecto, matriz, ARP, RATCH, FM6, DRUM-X y el resto).
- **Lo que NO está verde y no se presenta como verde**: siete comprobaciones de `tests/ui_test.c` fallan y ya
  fallaban en `main` antes de esta entrega (el roll de las tarjetas que no termina idéntico al render estático, el
  recuento de tarjetas obsoletas del soak, la tarjeta de CPU de GLO > SYSTEM y la disposición LARGE de HOME y
  PRESETS). Quedan abiertas y listadas en `docs/RELEASE_1.0.0_STATE.md`; no se relajó ninguna aserción para
  taparlas.
- **Nada medido en el FM-1 físico**: no hay unidad conectada y no se escribe en ella.

## Sin publicar — 2026-11-15 — Longitud de patrón por pista: verificado y cubierto

### Estado

- **Ya estaba implementado**: `P_SLEN` es un parámetro de cada pista (id 30 de 125, presente desde el primer
  formato de proyecto) y `seq_tick()` da la vuelta a cada pista con su propio largo. No hay cambio de
  comportamiento ni de formato en esta entrega, ni hizo falta una migración: un proyecto que no toca LEN lo deja
  en su default (16) sin escribir nada, y todos los formatos FUN1..FUN6 llevan el parámetro en su sitio.

### Lo que se añade

- **`tests/pattern_len_test.c`**: dos pistas con LEN 4 y 6, 60 pasos de `seq_tick`, cada una da la vuelta en su
  propio paso 0 (15 y 10 veces); un proyecto FUN6 con LEN 5 / 7 / 12 / 64 vuelve con los cuatro largos por pista;
  el default es 16 y un proyecto con LEN sin tocar carga 16.
- Entra en `tests/run_tests.sh` (y en el bucle ASan/UBSan). Documentado en `docs/PATTERN_LENGTH.md`.

## Sin publicar — 2026-11-15 — PRESETS del Web Studio: categorías, búsqueda y favoritos

### Revisión y arreglos

- **Categoría por motor e índice**: la tabla `preset_categories.json` se indexaba por nombre, y ocho nombres de
  fábrica se repiten entre motores (`SUB BASS`, `PLUCK`, `BRASS`...). Hoy las copias comparten categoría, así que
  no había una fila mal puesta; el índice ahora es `motor:índice` (`presetCategoryOf`) para que un preset nuevo
  con nombre repetido no herede la categoría del otro.
- **La búsqueda mira lo que la lista enseña**: un preset de usuario responde también a su etiqueta `U07`.
- **Lista vacía con el mensaje correcto**: antes decía «No favorites yet» aunque estuvieras filtrando por
  búsqueda o categoría; ahora distingue los dos casos.
- **Textos traducidos**: `SEARCH`, `CATEGORY`, la opción `ALL` y el placeholder estaban fijos en inglés; van por
  `data-t` y la lista de categorías se reconstruye al cambiar de idioma conservando la elegida.

### Verificación

- `node web/test_web.mjs` (con un check nuevo de `presetCategoryOf`: dos presets del mismo nombre en motores
  distintos, categorías distintas), `web/test_i18n.py`, `web/test_backup.mjs`, `web/test_pkg_bounds.mjs`,
  `tests/install_test.py`: pasan.
- Navegador real (Chromium contra `web/` servido en local, simulador QG-1): 279 filas, 12 categorías, `BASS` 18,
  `BASS` + `sub` 4, `LEADS` + `saw` 2, estrella `☆`→`★` con `aria-pressed` y `Favorites only` con esa fila,
  mensaje correcto sin coincidencias. Es navegador y simulador, no el FM-1.

## Sin publicar — 2026-11-15 — FASE 8: la página ARP dibuja el reloj

### Interfaz

- La página ARP enseña **el patrón y lo que está sonando, en el tiempo**: junto a `ROUTE`, los nombres de las notas
  que el arp está tocando ahora (`arp_note` y, en CHORD, `arp_ch`), y encima de cada paso una franja que se llena
  con el tiempo transcurrido del paso y un tick donde cierra el GATE.
- Los números son los del motor (`arp_pos`, `arp_off`, `div_samples(P_ARATE)`), así que el swing y el GATE se ven
  como suenan y la franja se para con el transporte.

### Verificación (host)

- `ui_render`: 190 pantallas × 22 paletas, **0 hallazgos de lint**, 3 551 691 mediciones de alineación con 0
  desalineaciones; la pantalla `arp` se renderiza ahora con el reloj a un tercio del paso.
- `midi_control_test` pasa (es el que ya comprueba que el arp avanza con el reloj interno y externo y que el GATE
  cierra en tiempo real).

## Sin publicar — 2026-11-15 — FASE 8: página MACROS

### Interfaz

- **LFO > MACROS**: una página global con M1..M4 del proyecto en KNOB 1..4. Son las fuentes M1..M4 de la
  matriz, así que el número que se gira es el que la matriz lee: no hay copia ni doble escritura
  (`macro_get`/`macro_set` en `mod.c`, `params.c` la pinta, `ui_input.c` la escribe).
- La gráfica enseña cada macro como **barra de valor** y, a la derecha, **los destinos que la pista
  seleccionada mueve con él** (hasta dos, luego `+n`) recorriendo sus ocho slots. Un macro sin rutas dice
  `NO ROUTES`, apagado, y conserva su barra.
- Guardado sin cambios: `project_capture` ya copiaba `macro_v` al proyecto, y la capa MACRO (LFO mantenido)
  usa el mismo `macro_set`.

### Verificación (host)

- `ui_render`: la página entra como pantalla propia (190 pantallas × 22 paletas), **0 hallazgos de lint** —en
  la primera versión el nombre `M4` y el valor `120` se solapaban 1 px y el barrido lo cazó al incluirla—,
  3 551 691 mediciones de alineación con 0 desalineaciones.
- `mod_test`: la escritura de la página (`macro_set`) recorta a 0..127 y la matriz lee el valor nuevo como
  fuente M1; `theme_test` con peor margen 4,08:1; `install_test.py` y `web/test_web.mjs` pasan.

## Sin publicar — 2026-11-15 — FASE 8: la matriz de modulación, ocho slots por pista

### Interfaz

- **Ocho slots por pista** (antes cuatro). KNOB 1 recorre `1..8` (`SLOT n/8`); KNOB 2..4 editan SRC, DST y AMT
  del elegido, con las mismas tarjetas grandes que los cuatro primeros.
- La **página MOD** los dibuja de una vez, con la **polaridad** de cada asignación siempre visible (AMT es
  bipolar: `+50`, `-50`, `0`) y el **valor vivo** de su fuente en una barra a la derecha: del centro hacia un
  lado para las fuentes bipolares (LFO, KEY, RAND, NOISE) y desde la izquierda para el resto. Un slot sin SRC,
  DST o AMT se ve apagado y sin barra.
- Los slots 5..8 **no son parámetros**: los ids son de 7 bits y `P_COUNT` está en 125 de 127, así que viven en
  `track_t.mx`. Consecuencia dicha en `docs/MOD_MATRIX_SLOTS.md`: no son automatizables y el editor de
  escritorio no los transporta (los deja a cero al guardar un preset).

### Compatibilidad

- **Proyecto**: bloque nuevo `'M' 'X'` al final, solo si algún slot 5..8 está puesto; un firmware anterior lo
  ignora y carga el resto del proyecto igual.
- **Preset de usuario**: los slots 5..8 van en los bytes del registro más allá de `np` (130..141, marca en 142),
  donde no vive ningún parámetro: **sin cambio de versión** y un registro antiguo se lee como ninguno.

### Verificación (host)

- `mod_test`: un slot de 5..8 modula **exactamente igual** que el mismo routing en el slot 1, los slots 1..4 y
  5..8 suman, llega al audio, y `mod_src_live` da el valor de la fuente (y 0 en un slot vacío).
- `project_test` y `upreset_test`: los slots 5..8 van y vuelven en ambos formatos.
- `ui_render`: 189 pantallas × 22 paletas, **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY/MONO fuera de gris, 3 401 666 mediciones de alineación con 0 desalineaciones. `theme_test`: peor margen
  4,08:1. `install_test.py` y `web/test_web.mjs` pasan.

## Sin publicar — 2026-11-15 — FASE 8 (rendimiento): cada placa se pinta una vez

### Rendimiento (host, `ui_render` a `-O1`; detalle en `docs/PERFORMANCE_FASE8.md`)

- **Cada placa se pintaba dos veces.** `cv_rrect` rellenaba el cuerpo y las dos bandas laterales, y la rampa
  del degradado pintaba después esos mismos píxeles otra vez (marco y esquinas incluidos). Medido con un
  contador **exacto** de píxeles rellenados, nuevo en el arnés (`px filled` en la tabla de coste): un frame de
  HOME escribía **118 014** píxeles para una pantalla de 57 600. Con la rampa encendida el relleno base ya no
  se hace: **MENU −42,7 %**, **CONFIRM −45,4 %**, **FX −29,7 %**, HOME −9,4 % de píxeles rellenados.
- Tiempo del redibujado completo de las 17 pantallas, mínimo de cuatro corridas intercaladas de los dos
  binarios: **−5,5 %** (MENU −14,8 %, FX −14,3 %, CONFIRM −12,2 %).
- **La imagen no cambia**: 189 pantallas × 22 paletas comparadas contra el árbol anterior, **0 ficheros
  distintos**.

### Verificación (host)

- `ui_render` sale con 0: 0 hallazgos de lint, 0 tintas sobre su propio color, 0 píxeles GREY / MONO fuera de
  gris, 3 400 410 mediciones de alineación con 0 desalineaciones.
- `theme_test`: todas las paletas pasan, peor margen **4,08:1**.

## Sin publicar — 2026-11-15 — FASE 8 (visual): el mini knob es un dial redondo con aro iluminado

### Interfaz

- **Tercera y última pasada a `gw_mini_bakelite_knob`**, esta vez midiendo el píxel. Lo que quedaba mal:
  la fila central del círculo es un píxel más ancha que la de al lado, y eso se veía como una **protuberancia
  a las 3 y a las 9**; el aro no tenía gradiente; y el arco del valor, dibujado a radio `r-1`/`r-2` sin
  comprobar el círculo, **colgaba fuera del contorno** por la izquierda.
- Ahora cada fila calcula su **semicuerda exacta** (`gw_isqrt`) y el píxel exterior se mezcla con el fondo por
  la fracción del círculo que realmente lo cubre: silueta redonda, sin púas ni escalones. El **aro lleva rampa
  vertical** (claro arriba, oscuro al pie) sobre un contorno oscuro de 1 px, la **cara** es una segunda rampa
  más oscura que el aro, y el **arco del valor va dentro del aro**, con el punto descartado si el redondeo lo
  saca de la banda. La aguja termina en el borde de la cara, con la punta en el acento.
- **Son un poco más grandes**: r 9 → 11 en HOME. Comprobado que no chocan: la fila mide 132 px de ancho en
  pasos de 31, así que el primer aro ocupa 107–129 (el cuadro empieza en 100), el cuarto 200–222 (termina en
  232), y en vertical 68–90 con la regla de cabecera en 66 y el divisor en 92.

### Verificación (host)

- `ui_render` 189 pantallas × 22 paletas: **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY / MONO fuera de gris, 3 400 410 mediciones de alineación con 0 desalineaciones.
- `theme_test`: todas las paletas pasan, peor margen **4,08:1**.
- Banco de presets auditado (`tools/preset_audit.py` sobre el volcado de `preset_audit_test`): **275 presets en
  9 motores, 0 problemas**, todos con envío a algún bus, 273 con movimiento.

## Sin publicar — 2026-11-15 — FASE 7: el degradado de las placas deja de costar frame

### Rendimiento (host, `ui_render` a `-O1`, mínimo de cuatro corridas)

- La campaña visual costaba **+4,9 %** en la suma del redibujado de las 17 pantallas medidas (`edit_analog`
  +24 %, `env` +26 %, `pattern` +20 %). Con esta campaña queda en **+1,5 %**, y `edit_analog`, `env` y
  `pattern` vuelven a +1,5 %, +0,2 % y +0,5 %.
- **La rampa se cachea por `(filas, hi, lo)`**: una pantalla dibuja 4–8 placas iguales y cada una recalculaba
  sus 21 `ux_mix`. Igual para la rampa del panel (`ramp_cache`). Coste en `.bss`: **~276 B**, declarado.
- **Las placas bajas (filas de lista, 16–18 px) van en cuatro bandas** en vez de ocho rellenos: MENU era la
  pantalla más castigada (+10,6 %), ahora el sobrecoste es 13 µs de host por frame y es tinta real.
- Dos pantallas **mejoran** porque dejaron el tramado al 50 % por bandas sólidas: `roll_chords` −2,4 % y
  `about_credits` −2,9 %.
- Sin `malloc`/`calloc`/`realloc` en el firmware: no hay fragmentación que medir.
- Detalle y método en `docs/PERFORMANCE_FASE7.md`. Lo que no se pudo medir (`.bss` del AC791N, ISR, WASM)
  está listado ahí como no medido, no como aprobado.

## Sin publicar — 2026-11-15 — FASE 6 (visual): los mini knobs de HOME son un dial, no un polígono de puntos

### Interfaz

- **`gw_mini_bakelite_knob` reescrito.** Apilaba cuatro `cv_disc` y pintaba el valor con un píxel cada `1/(5r)` de
  vuelta: a r = 9 el aro salía poligonal, el arco eran seis motas sueltas (las cian en las 12, 3, 6 y 9) y la aguja,
  dibujada hasta `r-2` atravesando el bisel, dejaba una púa blanca sobre el aro y un hueco en el medio. Además cada
  fila sin cuerda escribía un píxel suelto arriba y abajo, que es el "desperfecto" del primer knob.
- Ahora se dibuja por **líneas de barrido** entre dos círculos: aro, bisel y cara son un círculo real en cualquier
  radio, la cara lleva **rampa vertical** (claro arriba, sombra al pie) para que el knob tenga volumen en 16 bits, el
  valor es un **arco continuo** de 1 px sobre el aro, la aguja termina **dentro** de la cara con su cabeza en el
  acento, y el borde exterior lleva un tono mezclado para que la silueta no tenga púas. Nada de píxeles sueltos.
- Todo sigue saliendo de `ux_mix` sobre tokens de paleta: en GREY y MONO el knob es gris neutro (0 píxeles fuera de
  gris en el barrido).

### Verificación (host)

- `ui_render` 189 pantallas × 22 paletas: **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY / MONO fuera de gris, **3 400 410 mediciones de alineación con 0 desalineaciones**.
- `theme_test`: todas las paletas pasan, peor margen **4,08:1**.

## Sin publicar — 2026-11-15 — FASE 3 (diseño): los degradados llegan a los costados y dejan de verse en bandas

### Interfaz

- **La rampa de placa llega a los costados.** En el estilo *chamfer* (QUANTUM / GAMMA / WRAITH / JIANT) la placa se
  rellenaba como tres rectángulos (cuerpo y dos bandas laterales) y la rampa solo pintaba el cuerpo: quedaban dos
  columnas lisas de `r` px a cada lado de **cada tarjeta de knob**, que se leían como dos paneles pegados. Ahora la
  rampa es por fila, con el recorte de la esquina respetado (`cv_plate_cut_ground`), y cubre toda la superficie.
- **Pasos de 2 px en vez de 3.** Sobre una tarjeta el panel de 16 bits tiene unas diez entradas: un paso más fino
  repite color, y uno más grueso ya se ve como banda. Igual en las listas.
- **HOME y FX > BUSES: suelo por fila, sólido.** Los dos suelos se pintan con `ux_ground_rows`, que recorre la rampa
  **fila a fila** y rellena un tramo por color real de la paleta (1–4 px), en vez de 12 bandas de 14 px con damero
  (HOME) o 6 bandas con damero (BUSES). Mismos extremos de color (22–55 % y 2–24 % en HOME; 22–62 % y 10–45 % en
  BUSES), sin trama y sin escalones anchos.
- `ux_ground_ramp` (el suelo del panel de la gráfica) pasa también a fila a fila: eran 10 bandas de ~16 px.

### Verificación (host)

- `ui_render`: 189 pantallas × 22 paletas — **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY / MONO fuera de gris, 454 textos libres elididos, **3 400 410 mediciones de alineación con 0
  desalineaciones**.
- `theme_test`: todas las paletas pasan, peor margen de contraste **4,08:1** (mínimo 4,00).
- `mod_test`: la matriz de modulación pasa sus comprobaciones (NOISE mueve el filtro, NOISE vaga suave y bipolar,
  PAT 15 y NOISE 14 con los números guardados intactos).
- Coste del barrido completo: **33,93 s antes / 34,06 s después** (189 × 22 pantallas). Diferencia de 0,13 s, dentro
  del ruido de ±10 % del host; no se declara mejora ni regresión.

## Sin publicar — 2026-11-15 — FASE 3: las placas son degradados, y los suelos dejan de verse tramados

Los cuadros de la interfaz estaban pintados como una placa plana con una banda dura bajo el bisel, y los dos
suelos que sí tenían degradado lo tenían en 6 y 12 bandas anchas, mezcladas al 50 %: a 240×240 eso se lee como
un damero de píxeles, no como una luz que cae.

### Interfaz

- **Toda placa es un degradado** (`gfx.c`, `cv_plate_ground` y la rama *chamfer*): de una cresta apenas iluminada
  (7–14 % de `T_TEXT` mezclado) a un pie en sombra (58 % de `T_BG`), en bandas de 3 px. Llega a las 189
  pantallas por la primitiva que ya usaban todas: las tarjetas de knob, los módulos de FX > BUSES, los paneles y
  las filas de lista. **Las filas de lista (desde 16 px de alto) reciben ahora el mismo tratamiento**; antes solo
  lo tenían las cajas de 36 px o más, y por eso las filas se veían como barras muertas.
- **El degradado de la rama *chamfer*** (GAMMA / WRAITH / JIANT / QUANTUM): esa rama dibujaba la placa lisa entre
  dos líneas de 1 px, y es la que usan las tarjetas de knob del tema por omisión. Ahora también tiene su rampa.
- **HOME y FX > BUSES sin damero**: sus suelos pasan de 12 y 6 bandas al 50 % a **40 y 24 bandas sólidas**, así
  que la luz cae en pasos de 1–4 px sin el patrón de píxeles que se veía sucio.
- Nada nuevo en colores: todo sigue saliendo de `ux_mix()` sobre los tokens de la paleta, así que GREY y MONO
  siguen neutros (0 píxeles fuera de gris en el barrido) y el contraste no cambia.

### Corregido en la misma tanda

- **El degradado no entra en el panel de la gráfica.** Ese panel (HOME y las páginas de EDIT) pinta su propio
  suelo y su propio bisel justo después, así que la rampa de placa solo asomaba por los pocos px que el suelo
  del panel no repinta: la franja bajo el bisel y los 3 px de cada costado, justo donde el osciloscopio apoya
  la traza. El panel se declara **plano** (`cv_plate_flat`, `gfx.c`) y se queda como estaba: el área de la
  gráfica de HOME vuelve a ser **píxel a píxel idéntica** a la de antes de este cambio, en las 22 paletas.

### Coste (medido)

- El barrido completo `ui_render` (189 pantallas × 22 paletas) tarda **32,9 s antes y 32,8 s después**: la rampa
  nueva usa un relleno por banda de 3 px y solo repinta cuando el color cuantizado cambia, mientras que los dos
  suelos abandonan el tramado pixel a pixel. Se compensa; no hay regresión medible.

### Verificación

- `ui_render`: 189 pantallas × 22 paletas, **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY / MONO fuera de gris, y **3 400 410 mediciones de alineación con 0 desalineaciones**.
- `theme_test`: todas las paletas pasan; el peor margen de contraste es **4,08:1** contra el mínimo de 4,00.
- Los goldens de audio (`tests/golden.txt`) no se tocan: son de sonido, no de pantalla, y siguen 293/0.

### Pendiente

- **Subir la versión y publicar no se hizo**: `FELUCCA_VERSION` sigue en `v4.1.0` a propósito. El paquete
  `.fwsc`, su manifiesto y `release/SHA256SUMS` solo se generan con el build del AC791N, y este entorno no tiene
  `JIELI_TOOLCHAIN` ni `AC79_SDK`; el emulador necesita `emcc`, que tampoco está. Subir la versión sin esos
  artefactos dejaría el árbol y el paquete publicados en desacuerdo, que es justo lo que `coherence_check.py`
  prohíbe.

## Sin publicar — 2026-11-15 — FASE 1: 1 KB de RAM de vuelta, con las tablas idénticas

La campaña de optimización de esta fase empieza por el margen que estaba más apretado. En la entrega anterior
el `.bss` quedó en 97 380 de 98 304 B: **924 B libres**. `wt_gen_init` gastaba ahí un array de 1 024 B que solo
usaba una vez, al arrancar, para normalizar las tablas del WAVETABLE al mismo pico.

### RAM

- **`wt_buf` desaparece** (`eng_wavetable.c`): ahora se recorre cada frame dos veces, la primera para hallar su
  pico y la segunda para escribir los valores normalizados. El array era `.bss` permanente para un trabajo de
  arranque; el `.bss` de la unidad de compilación baja **1 024 B** (1 056 B en el binario, con la alineación) y
  los 924 B libres pasan a unos **1 948 B**.
- **Las tablas salen idénticas**: es la misma aritmética entera, solo repetida. Medido: el hash FNV de
  `WT_GEN` + `WT_L5` es `c1b4453e` antes y después, y los **293 renders golden siguen con 0 cambios**, los 22
  del WAVETABLE incluidos.

### Coste

- `wt_gen_init` en host x86: **172–183 µs → 325–337 µs** por llamada (+~0,15 ms), una sola vez en el arranque y
  nunca dentro de un bloque de audio. La cifra del AC791N no está medida (falta `JIELI_TOOLCHAIN`); a 320 MHz se
  espera del orden de 1,5 ms, y eso es una estimación, no una medición.

### Lo que se midió y no se tocó

- El regress da **ns por preset** en host. Las filas más caras son del WAVETABLE (90–118 ns) y superan a ANALOG
  (50–70 ns) en lo que cuestan sus dos frames por muestra; revisado el bucle, las lecturas ya están hoisteadas
  y no queda trabajo redundante que quitar sin cambiar el redondeo (y con él el sonido).
- El ruido del VM compartido es de ±10 % entre corridas: cualquier microoptimización por debajo de ese umbral no
  es demostrable aquí, así que no se declara.
- La medición de instrucciones del target (`tests/target_budget.py`) sigue bloqueada: necesita
  `build/felucca.dis`, y sin `JIELI_TOOLCHAIN` / `AC79_SDK` no hay build del AC791N.

### Estabilidad

- **`mflasher_test.py` dejó de ser intermitente** (fallaba 1 de cada 5 corridas). La causa no era la prueba sino
  el reloj: el nombre del archivo que escribe el importador lleva la hora UTC con granularidad de segundos, así
  que dos importaciones seguidas a veces caían en nombres distintos y no había nada que rehusar. El importador
  acepta ahora `--stem` para fijar ese nombre, y la prueba lo usa en las dos importaciones: sigue exigiendo el
  rechazo (`code == 1`, "already exists") y el hash intacto, pero ya no depende del segundo en que corra.
  10 corridas seguidas, 10 en verde.

Pruebas: `NO_PACKAGE=1 tests/run_tests.sh` completo; `wavetable_test` (13 ns por muestra y voz en host, dentro
de presupuesto); regress 293/0/0.

## Sin publicar — 2026-11-15 — modulación de fase en dos motores, sin parámetros nuevos

Dos motores estrenan una interacción nueva entre sus fuentes sin añadir un control que no suene y sin tocar el
formato guardado: los valores nuevos van al final de cada enum, así que un sonido viejo carga igual.

### Síntesis

- **ANALOG: WAVE = PM** (onda 11, `eng_analog.c`). El oscilador 2 (una sierra) pliega la fase del 1 antes de la
  mezcla: FM con los dos osciladores que el motor ya tiene. El índice es **MORPH** más ENV / LFO → **SHAPE**
  (0,78 rad con MORPH en 0 hasta ~4 rad) y la relación entre ambos, **INT** / **DTUNE**, como SYNC y RING (mismo
  despacho, `analog_ext`). Sin parámetro nuevo: lo que en SYNC es el reinicio y en RING el producto, en PM es la
  fase.
- **PHASE: LINE = PM** (modo 3 de LINE, `eng_phase.c`). La línea 2 pliega la fase de la 1 antes de la forma de
  onda; el índice sigue a **DCW**, así que el knob y ENV → DCW lo abren hasta ~1,5 rad. La línea 2 se calcula
  ahora antes que la 1: MIX y RING dan los mismos samples que antes.
- **La pantalla no miente**: la vista previa de MORPH dibuja la fase plegada y la marca del modo W2 lee `PM`.

### Pruebas (clase: host x86; no es una medición del AC791N ni del FM-1)

- **`tests/pm_test.c`** (nuevo, en la suite): sobre el DSP real, 8 comprobaciones. ANALOG PM difiere de sí mismo
  con MORPH 0 y 127 en **1,53× la señal** y su brillo (RMS de la diferencia primera sobre RMS de la señal) sube
  de **0,048 a 0,345**; PHASE PM difiere de MIX en **1,31×** con brillo **0,144 → 0,799** al mismo DCW; ninguno
  coincide con SIN, MIX ni RING; picos **13 946** y **11 764**, muy por debajo del bus (2^26).
- **`tests/six_live_golden.txt` regrabado** (61 hashes, `SIX_LIVE_ACCEPT=1`), y el motivo es uno solo: la
  escena barre `trk[0].p[P_E0]` de 0 a 127 y el motor lleva al máximo del enum todo lo que lo pasa; ese máximo
  es ahora PM y antes era FOLD. Dos corridas seguidas dan 61/61. El render de imágenes no se movió.
- `web/editor.html`: las tablas del editor llevan `PM` en WAVE y en LINE (la suite las compara con el
  firmware y fallaba con 2 diferencias hasta corregirlas).
- `engine_param_test`: 216 knobs × 3 presets, 0 inertes. `morph_audible_test`: 21 casos, 0 fallos.
  `phase2_test`: PASS.
- `tests/regress.c`: **293 renders golden, 0 cambiados** (ninguna página ni preset de fábrica usa PM todavía),
  0 fallos de salud, 0 de voces/routing, 0 cuelgues; CPU **PASS** (mediana del host 1,06, límite agregado
  +20 %, 0 filas fuera de presupuesto).

### Pendiente y límites

- La imagen del target **no se midió**: `JIELI_TOOLCHAIN` y `AC79_SDK` no están en este entorno, así que el
  `.fwsc` y su tamaño quedan sin comprobar. El margen documentado tras la entrega del WAVETABLE es de 5 852 B.
- Ningún preset de fábrica usa PM todavía: la onda existe y se elige en EDIT > WAVE, pero agregar un sonido de
  la casa sin poder escucharlo sería declarar un timbre que nadie oyó.

## Sin publicar — 2026-11-15 — el flash que faltaba: la tabla del WAVETABLE se genera en el arranque

La región XIP estaba en 583 600 de 583 612 B: doce bytes. La vía de comprimir assets se midió y estaba
cerrada (ver `docs/FLASH_RECLAIM.md`), así que esta entrega cambia el modelo: guardar menos y calcular más.

### Memoria

- **WAVETABLE: dos de sus tres límites de banda se construyen al arrancar** (`wt_gen_init`,
  `eng_wavetable.c`). Lo guardado eran 8 formas × 256 puntos por nivel; ahora se guardan las amplitudes
  armónicas de esas 8 formas en Q30 (2 048 B) y la síntesis —64 MACs por punto, suma de senos con la tabla
  `SINE`— corre una sola vez, antes de la primera nota, también desde `felucca_init`. El nivel de 5 armónicos
  (el que leen las notas agudas) sigue guardado tal cual.
  - Flash: **imagen 583 600 → 577 760 B, 5 840 B liberados** (código +292 B, constantes −6 132 B).
  - RAM: `.bss` 88 148 → 97 380 B de 98 304 (8 192 B de tabla generada + 1 024 B de buffer).
  - Sonido: la onda generada difiere de la guardada en **3 LSB de 32 000 (0,0094 %, ≈ −90 dB)**, medido en
    host x86 comparando ambos enteros punto por punto. Cambiaron exactamente los **22 goldens de WAVETABLE**;
    ningún otro motor se movió. `tests/golden.txt` queda rebasado con esos 22.

### EDIT

- **DETUNE estrena página** (VOICE 3): `P_DETUNE` repartía las voces del unisono hasta ±40 cents en
  `voice.c` desde siempre y ninguna página lo ofrecía — la v4.1.0 lo documentó como imposible por falta de
  flash. Con los 5 840 B liberados entra: cuesta **24 B** y se lee con el asignador (ALLOC) de VOICE 2.
  Barrido de renders: 189 pantallas × 22 paletas, 0 hallazgos de lint, 0 desalineaciones; 293 goldens sin
  cambios (el valor por omisión, 40, ya estaba en efecto).

## v4.1.0 — 2026-11-15 — un solo espacio de paleta, y la suite en verde

Cierra lo que dejó abierto la retirada de AMBER, ICE y VIOLET, expone un control que el DSP leía sin que
ninguna página lo ofreciera, y deja `NO_PACKAGE=1 tests/run_tests.sh` y `web/test_web.mjs` pasando enteros.

### EDIT

- **VOICE 2 estrena ALLOC** en el cuarto knob, que estaba vacío (`0xFF`): `ROT` rota la voz más vieja,
  `REUSE` retriggerea la misma. El asignador lo leía `voice.c` desde siempre y ninguna página lo ofrecía.
  Coste: **0 B de flash y 0 B de RAM** — ocupa un hueco ya declarado.
- **DETUNE sigue sin página**: `P_DETUNE` existe y los motores lo leen, pero exponerlo pide una página
  nueva y la región XIP no tiene lugar (medido: el linker rechaza una página más con 12 B libres).

### Interfaz

- **Un knob que no hace nada ya no se anuncia.** En el layout de tarjetas altas, una tarjeta vacía dibujaba
  igual la píldora `K1`..`K4` en `T_DIM`: una etiqueta para un control inexistente. Ahora retorna antes de
  la píldora, y además **libera 16 B de XIP** (imagen 583 600 → 583 584 B en su momento).
- **PAPER pasa su propio contraste.** `T_MID` sobre `T_SURF` daba 3.87:1 donde el proyecto exige 4.00, y al
  corregirlo apareció el acento a 3.90 sobre 4.50. Superficie `(226,218,202)` → `(232,224,208)` y acento
  `(172,56,8)` → `(150,44,0)`, en el generador. `tests/theme_test` recorre los 22 pares en las 22 paletas.

### Paletas: un solo espacio entre MENU, el editor y la Web Studio

- `menu_items.c` habla en **ranuras de la lista viva** (`UI_PAL_LIVE`) y `editor_preferences.c` asignaba el
  **índice crudo** de la tabla. Con dos espacios conviviendo, `UI_SET` podía dejar el instrumento en un tema
  retirado y la comparación de "preferencia sin cambios" no coincidía nunca: el editor reescribía la flash
  en cada pasada.
- Ahora `UI_SET` (id 0) toma la ranura y **guarda el índice de tabla**; `UI_STATE` devuelve **la ranura**,
  nunca un id retirado. La lista de respaldo del Web Studio pasa a las 19 paletas vivas y su estado inicial
  usa la ranura de QUANTUM; `web/EDITOR_PROTOCOL.md` lo documenta para los dos comandos.
- `tests/persistence_test`, `tests/editor_test`, `tests/settings_test` y `tests/theme_test` actualizados a la
  especificación de hoy (ranura ≠ índice, 19 paletas vivas, PAPER con su contraste).

### Pruebas

- `NO_PACKAGE=1 tests/run_tests.sh`: **ALL HOST TESTS PASSED**.
- `node web/test_web.mjs`: **web tests passed**.
- `tests/run_tests.sh` crea `build/capture` antes de escribir ahí: en un `build/` limpio el primer error era
  un `sed` sin directorio.

## v4.0.0 — 2026-11-15 — el instalador enciende el instrumento

La página del instalador arranca el firmware y lo dibuja a su propio reloj; el estudio de optimización del
dibujado queda medido, y se explica por qué no entra todavía.

### Instalador: la secuencia de arranque, no una imagen

- **La página del instalador enciende el instrumento.** Carga el mismo `felucca.wasm` que publica el emulador, lo
  arranca con las mismas llamadas que usa su worklet (`web_nor_erase`, `web_master`, `web_seed`, `web_boot`) y
  dibuja el panel de 240 × 240 con `web_render(n)`, que empuja el reloj del equipo muestra a muestra: el splash,
  el barrido de la tira de LEDs y la pantalla a la que llega, a velocidad real, con control para repetirlo. Antes
  esta página no dibujaba el panel en absoluto: no había canvas ni imagen animada, sólo una cabecera.
- **Comprobado sobre el binario que se publica:** arrancado en Node, ocho instantes del arranque (0, 400, 1200,
  2500, 3500, 4530, 5000 y 6500 ms) dan **ocho fotogramas distintos** — el splash se mueve y termina en HOME — y
  los 7 s de arranque se ejecutan en 117 ms de reloj real (**60× tiempo real**), que es lo que permite correrlos
  sin cargar el equipo.
- El reloj se detiene al llegar a HOME, se puede **repetir**, y no corre si la tarjeta está fuera de pantalla o
  la pestaña oculta. Si el wasm no está junto a la página, la tarjeta no aparece: no hay imagen de reemplazo.
- La CSP de la página suma `'wasm-unsafe-eval'`: sin eso Chromium no compila el módulo y la tarjeta quedaba
  oculta sin decir nada. Sigue siendo una única script inline con su hash.
- No se tocó el splash del firmware ni el del emulador: es el mismo binario, dibujado en otro sitio.

### Optimización del dibujado: medida, y por qué no entra

- El trabajo más caliente de la pantalla es `ux_mix` (cada degradado, cada glifo con antialias, cada vúmetro) y
  redondeaba tres canales por llamada con una **división por 100**; el AC791N no tiene instrucción de división.
  Reescrito como producto y desplazamiento — **exacto para todo |d| ≤ 43 599**, verificado valor por valor — el
  barrido completo de render baja de **79,4 s a 26,5 s** (26,5 s y 26,7 s en dos pasadas) para las mismas 189
  pantallas × 22 paletas, y `home` pasa de 584,7 µs a 131,8 µs (4,4×) con el informe del barrido idéntico (0
  lint, 0 tintas sobre su propio color, 0 píxeles fuera de gris/neutro) y sin cambiar un píxel.
- **No está en esta versión, y con razón: la medición era del banco, no del chip.** El pi32v2 **sí tiene
  división por hardware** (`r0 = r0 / r10 (u)` en `build/felucca.dis`), así que en el firmware el compilador
  nunca llamaba a un ayudante de división: los 3× venían de que el banco de render se compila con `-O1`, donde
  la división sí es una llamada. Sobre el objetivo el cambio **cuesta 16 B de XIP** (con 12 B libres, el link
  pasa a 4 B de más) y no quita ni una instrucción. La tabla de 257 entradas pide 244 B. Ninguna de las dos
  entra, y ninguna de las dos era la mejora que parecía: ver `docs/PERFORMANCE_BASELINE.md` §3.

### Pendiente, dicho

- El reequilibrio de niveles del banco de presets y el relleno degradado de EDIT > FILTER 4/6 siguen sin hacer:
  no hay unidad ni salida de audio real en este entorno y no se declaran niveles medidos sin medirlos.
- `release/SHA256SUMS` no listaba la v4.1.0 (estaba hasta v4.0.0). Corregido: se agregaron sus tres archivos con hash verificado.
- Attestation de GitHub: implementada en `release.yml` y `tag-and-publish.yml` (workflow reutilizable de `zvenson/fm1-build` fijado a un commit). No verificable aquí: `gh` 2.45 no tiene `attestation` y no hay sesión.

