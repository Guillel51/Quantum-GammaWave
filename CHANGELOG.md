## v1.2.6 — motor ADDITIVE, SOUND EDITED y segunda vuelta de motores

### El motor ADDITIVE (nuevo, `firmware/src/eng_additive.c`)
- **Ocho parciales por voz, cada uno con su fase, su amplitud y su decaimiento.** Es la única arquitectura que
  los nueve motores no cubrían: ANALOG y PHASE filtran una onda, FM6 y WAVETABLE construyen el espectro desde
  una sola fase, WHEEL es aditivo pero fijo a los nueve footages del órgano. Acá los armónicos son el sonido.
- **SPECTR** (SPECTRUM; E0, el primer destino de la matriz, `MD_E1`) camina por ocho perfiles armónicos (ORGAN, SAW,
  SQUARE, BELL, GLASS, REED, HOLLOW, METAL) interpolados por bloque: un LFO o una envolvente barren el timbre,
  no lo conmutan. **TILT** inclina el espectro, **DECAY** es espectral (el parcial k + 1 cae con DECAY · k y el
  fundamental se queda), **STRETCH** vuelve inarmónicos los parciales (hasta 3,375 f0 en el tercero), y
  CUT / RES / DRIVE / SUB son los mismos del resto de los motores, así que un sonido de acá se mezcla igual.
- **CHARACTER: EVOLVE**, un paseo lento de SPECTRUM por parte, invertido como los dos primeros slots del
  banco (127 = quieto, y de ahí para abajo camina); `tests/additive_test` lo fija: a 127 el mismo sonido dos
  veces es idéntico muestra por muestra, y a 0 difiere en 0,30 de escala.
- **Límite de banda sin aliasing**: entre 4,5 y 6 kHz el parcial se apaga en pendiente y arriba de 6 kHz no se
  renderiza (no se pliega como en el órgano: plegado, un parcial cae *entre* los armónicos).
- **Ocho presets nuevos** (`ADDTV ORGAN`, `GLASS BELL`, `CRYSTAL PLUCK`, `SPECTRAL PAD`, `TINE EP`,
  `GONG DEEP`, `HARMONIC BASS`, `METAL CLUSTER`) y su página EDIT con el espectro dibujado (barras por parcial,
  en el lenguaje de las barras de WHEEL).
- **Medido** (`tests/eng_bench`, host x86-64, ocho voces, mejor de tres): ADDTV **3,16 µs/bloque** menos el piso
  silencioso, la banda de FM6 (3,24) y por debajo de WTABLE (4,52) y WHEEL (4,73) en la misma corrida.
  `tests/additive_test`: perfiles, crossfade, TILT (10 dB), DECAY (−27,6 dB en el 4.º y −0,1 dB en el 1.º a
  0,8 s), STRETCH (el 3.º en 3,375 f0), SUB, límite de banda en C7/A7/C8, salud de los ocho presets (0 clipping,
  0 DC, silencio tras el release), esquina de todos los EDIT (7,25 × VOICE_FS a ocho voces).
- **Coste en la imagen: no medido** (no hay toolchain AC791N en esta sesión), ver «Bloqueado».

### SOUND EDITED (el indicador de parámetros modificados)
- **Una estrella junto al nombre del sonido** en el pie de todas las páginas y en HOME > OVERVIEW, y un punto
  en la fila del sonido cargado del navegador de PRESETS: el sonido cambió desde que se cargó o se guardó.
- **Se calcula comparando**, no con un hook en cada setter: un hash FNV-1a del sonido (P_COUNT + la matriz +
  motor y preset) contra la huella que dejó la última carga o el último guardado, así que cubre perillas, la
  capa rápida, el editor, A/B y el recall de una sección sin que ninguno de ellos lo sepa. Se evalúa una vez
  por cuadro y **nunca con el transporte andando** (una automatización o un lock escribe p[] para su paso y lo
  devuelve: no es una edición del sonido). No se guarda: es estado de la interfaz.

### Alrededor del motor
- **G_ENGSEL** (`firmware/src/params.c`) suma el nombre del motor 15: sin eso el editor podía guardar un sonido
  ADDTV pero no elegir el motor (el `ENG` del dispositivo terminaba en WTABLE).
- **Editor y librería web** (`web/editor.html`): la tabla del dispositivo simulado gana el motor 15 con sus
  ocho EDIT y sus ocho presets, y `ENGINE_ORDER` lo pone antes de DRUM como el firmware. `web/test_web.mjs`
  compara ahora 16 motores (y sus listas) contra `build/host/desc.json`.
- **Tres pruebas que enumeran la banca**: `tests/preset_library_test.py` (281 sonidos, ADDTV 8),
  `tests/ui_test.c` (la lista de motores mostrada) y `tests/additive_test.c` (el motor nuevo).
- **Etiqueta corta**: el EDIT 1 se llama `SPECTR` y la página `PARTIAL` porque el barrido de `tests/ui_render`
  (190 pantallas × 22 paletas) marcaba `SPECTRUM` elidido en la tarjeta; quedó en 0 hallazgos de lint y 0
  desalineaciones de 3 894 154 medidas.

### Bloqueado
- **El emulador WASM y el paquete**: no hay `emcc` ni toolchain JieLi / SDK en este entorno, así que no hay
  `.fwsc` de 1.2.6, ni XIP / `.data+.bss` medidos del binario, ni registro del emulador, ni release. Todo lo de
  arriba está medido en host y compilado desde las fuentes reales; **nada se probó en una unidad FM-1**.

## v1.2.5 — paquete de firmware y capturas

- Firmware subido a **v1.2.5** y capturas del proyecto regeneradas desde ese árbol.
- Paquete AC791N: `release/felucca-v1.2.5.fwsc` — **610 959 B**, SHA-256
  `166fbc8bbf39bca7162b2fc8d8e3841d601ac8555cd3da389cb61a4a7380b896`, con manifiesto de paquete y de build
  (commit `571ff4d`, árbol limpio) y sus tres líneas en `SHA256SUMS`. `check_package_identity`: ok.
- Imagen 580 196 B; XIP 420 432 B de código + 139 288 B de constantes; `.data+.bss` 95 076 B de 98 304;
  pool spare 9 248 B. Sin cambios de tamaño respecto de v1.2.0: la subida de versión no cuesta imagen.
- **Sigue bloqueado**: el emulador WebAssembly (no hay `emcc` en este entorno), y con él el registro del
  emulador, la entrada del sitio y la release publicada: no se publica 1.2.5 con un emulador 1.1.0.

## v1.2.0 — paquete de firmware

- Firmware subido a **v1.2.0** (`firmware/src/felucca.c`), capturas del proyecto regeneradas desde ese árbol
  (splash y ABOUT muestran la versión que el binario lleva).
- Paquete AC791N: `release/felucca-v1.2.0.fwsc` — **610 959 B**, SHA-256
  `c38bc917cdbcc6b9f8af1af6b5c461707b3dd739178e73210f375aa52da0fbf6`; manifiesto de paquete, manifiesto de
  build (`release/build-manifest-v1.2.0.json`, commit `060ebd8`, árbol limpio) y las tres líneas en
  `SHA256SUMS`. `tools/check_package_identity.py` responde "package identity: ok" (ota-FM-1_965).
- Imagen 580 196 B, XIP 420 432 B de código + 139 288 B de constantes, `.data+.bss` 95 076 B de 98 304,
  pool spare 9 248 B.
- Suite de host completa: `ALL HOST TESTS PASSED` (EXIT=0).
- **Bloqueado**: el emulador WebAssembly no se reconstruye en este entorno (no hay `emcc`), así que
  `release/emu` sigue siendo v1.1.0 y `coherence_check.py` marca dos problemas, los dos por eso (el registro
  del emulador y la copia que sirve el sitio). Por la misma razón el sitio y `site/firmware/versions.json`
  siguen ofreciendo 1.1.0: publicar 1.2.0 allí sin un emulador de 1.2.0 rompería la regla de que todos los
  componentes sirven la misma release.

## FASE 5 — visualizador (sin publicar) — 2026-11-16

- **Los dos osciloscopios del visualizador son vidrio, no cajas oscuras.** El pozo era el tono de la página
  oscurecido en 14: una placa opaca por la que no se veía el campo de estrellas. Un framebuffer RGB565 no tiene
  alfa que ajustar, así que el pozo se mezcla hacia el tono de la página (62 %): las estrellas siguen viéndose a
  través del tinte y la grilla y las trazas siguen mandando.

## FASE 4 — mejoras funcionales — 2026-11-16

- **El progreso de los mini knobs de HOME sigue a la aguja.** Era un arco de dos píxeles en el borde, barrido
  desde abajo a la izquierda: en la parte baja del rango se leía como un manchón en el lado izquierdo del dial y
  desde la mitad como "la mitad izquierda encendida", con la aguja apuntando a otro sitio. Ahora se rellena toda
  la banda barrida —cara y borde— así que la parte encendida termina exactamente en la aguja, en las 22 paletas
  (en las neutras el relleno toma la tinta, que es lo que usa la aguja).
- **A/B del sonido, permanente.** En HOME, **EDIT mantenido** guarda el sonido de la pista en un lado y **EDIT
  tocado** intercambia los dos lados, con el mensaje diciendo cuál suena (SOUND A / SOUND B). Viaja el sonido
  (sus valores y las ranuras de la matriz), nunca los pasos ni la automatización, y el ISR ve un sonido o el otro
  (motion_guard). Es además la forma no destructiva de audicionar un preset: se guarda el sonido propio, se gira
  el knob de PRESETS todo lo que haga falta y EDIT lo trae de vuelta. Una ranura: 278 B de `.bss`.

## FASE 3 — interfaz y visualizaciones — 2026-11-16

- **HOME: el osciloscopio ya no queda debajo del degradado.** El panel dibujaba el trazo antes que el suelo con
  degradado y las 28 bandas, así que se veía solo donde sobresalía; ahora va último y en la tinta de la paleta
  cuando su rampa es neutra. Ver `docs/QG_FASE3_INTERFAZ_2026-11-16.md`.
- **Los dos osciloscopios del visualizador son el mismo instrumento**: mismo pozo oscuro, mismo marco de 1 px,
  la misma grilla de tres divisiones y la misma escala de pies, y las dos trazas en la rampa de calor a 2 px.
- **Una sola tipografía en los cuatro cuadros de los knobs**: la etiqueta elegía la cara de 15 px o la de 12
  según su largo (`DIST` en M y `CHORUS` en S en la misma fila). Todas van en la de 12 px, en la misma línea.
- **ARP**: nombre en las catorce teclas blancas (y en la tinta que se lee sobre la tecla), letra y sostenido en
  las cinco negras, marco en los chips cuya nota está sonando, medidores con contraste y sin cruzarse con el pie.

## FASE 6 — segunda optimización profunda — 2026-11-16

### El espacio
- **La última banda del WAVETABLE se arranca, no se guarda**: `WT_L5` (4 096 B de flash) se construye en
  `wt_gen_init` como las otras dos, con las amplitudes de `WT_COEF` que ya estaban. El sonido no cambia de
  forma medible: los 2 048 valores generados difieren del guardado en ±1 sobre 32 000 en 972 puntos, y
  **293 renders golden salen idénticos**. La holgura XIP pasa de **416 B a 4 464 B** y `.data+.bss` de
  90 676 a 94 772 B (holgura 3 532 B). Ver `docs/QG_FASE6_OPTIMIZACION_2026-11-16.md`.

### La pantalla
- **Los glifos que no se ven no se decodifican**: `cv_alpha` calcula una vez por glifo las columnas que caen
  en el lienzo y se salta las filas fuera del recorte (el lazo interior pierde el test de límites por
  píxel), y `cv_alpha_hc` no recorre el flujo Huffman de una celda que cae entera fuera. Las 190 pantallas ×
  22 paletas quedan idénticas (0 lint, 0 desalineaciones) y el costo de dibujo medido en host baja 3,2 % de
  mediana (16,3 % en los créditos). Cuesta +56 B de XIP, pagados por lo anterior.

### Lo que se revisó sin cambiar
- Detector de código duplicado sobre 53 848 renglones: 2 bloques repetidos de 6 líneas, sin byte que valga
  el riesgo. Cadenas literales: el compilador ya las fusiona. RAM, POOL, `-Oz` y las 39 líneas de
  `tests/target_budget.py` (todas a 0 %) quedan como estaban, cada una con su número escrito.

## v1.1.0 — 2026-11-16

La versión que junta todo lo que se hizo después de la 1.0.0 en un solo binario, un solo paquete, un solo
manifiesto y un solo sitio. No agrega funciones nuevas a las que ya están: ordena, corrige y publica. Las
entradas 1.0.1 y 1.0.2 de abajo son el detalle de cada arreglo; acá está el conjunto.

### La interfaz, como quedó

- **La franja del valor de una tarjeta** repinta las filas de su placa con las mismas bandas del degradado
  (`cv_plate_rows`), así que el dígito que rueda ya no deja un parche de otro tono. Cierra las siete
  comprobaciones de `tests/ui_test.c` que llevaban rojo desde antes de la 1.0.0. Ver `docs/CARD_STRIP.md`.
- **Geometría 1.0**: cabecera 0..16, tarjetas 20..76, panel 78..200, pie 204..240; MENU > LARGE 20..124 /
  128..200. Etiquetas de tarjeta en la cara de 15 px, gauge al pie de la tarjeta, medidor de carga y chip "A"
  centrados. Las filas de lista pintan su rampa con un solo relleno tramado. Ver `docs/UI_LAYOUT_1.0.md`.
- **MACROS y MOD rehechas** con el lenguaje del resto: placas con rampa, pastilla de etiqueta, valor en la letra
  de las tarjetas, gauge redondeado en el color de calor, chips de destino; cada slot de MOD en su placa con su
  pestaña, el número fuera y la barra viva redondeada.

### El arranque

- **Se fue el pop del encendido**: la salida del códec se desmuteaba con ALNK0 apagado y otra vez 5 ms después,
  o sea sobre un DAC sin asentar. Ahora se abre recién con 25 ms de silencio corriendo.
- **El splash del aparato y el del emulador son el mismo en los tres modos** (FULL / SHORT / OFF): el emulador
  lee el mismo ajuste y el mismo lapso que `main.c`, en vez de animar siempre. Ver `docs/BOOT_SPLASH_AND_POP.md`.

### El presupuesto

- **Render**: `phase_render` 98 → 94 y `phase_render_noise` 117 → 113 (vuelven al límite del +10 %), y
  `analog_ext` re-basado a 174 con su razón escrita en `tests/target_budget.txt`: tres ramas por muestra menos y
  el resto es la PM aprobada. Cuenta estática del build AC791N, no ciclos del aparato.
- **Host, `tests/regress`**: 293 renders dorados **0 cambiados**, 0 fallos de salud/voz/ruteo, **0 entradas de
  CPU sobre presupuesto**.

### Medido (clases)

| Qué | Clase | Resultado |
| --- | --- | --- |
| Firmware | build AC791N (toolchain JieLi + blobs del SDK por SHA-256) | imagen 583 540 B, XIP 418 740 B, `.data+.bss` 96 836 de 98 304 B |
| Paquete | build AC791N | `felucca-v1.1.fwsc`, 610 959 B, `FM-1_965` |
| Sonido | host, `tests/regress` | 293 goldens idénticos, CPU por bloque sin cambios |
| Interfaz | host, `ui_render` | 190 pantallas × 22 paletas, 0 lint, 3 554 249 mediciones sin desalineaciones |
| Coherencia | `tools/coherence_check.py` | paquete ↔ manifiesto ↔ registro del build ↔ emulador ↔ sitio, `SHA256SUMS` |
| Sitio e instalador | `tools/site_check.py` | portal, instalador con la versión publicada, Verify Only, emulador |

**Sin medir**: nada en el FM-1 físico (no hay unidad conectada y no se escribe en ninguna), y el pop es una
secuencia de encendido verificada, no una escucha.

### Lo que queda pendiente, dicho

- **Categorías de preset en el FM-1**: la clasificación vive en `tools/gen_preset_categories.py` y en el Web
  Studio; el firmware no tiene tabla ni navegación por categoría. Necesita una tabla generada (~273 B) y código
  de navegación, y el XIP está a 72 B de su límite: hay que liberar espacio primero.
- **HOME > SCOPE**: el campo profundo (la banda de la Vía Láctea y su polvo) se ve a través de la traza y compite
  con ella. La causa está localizada en `graph_home_viz`; el arreglo no se incluyó porque el render de host de esa
  escena no cambió al parchearlo y no hubo forma de verificarlo en el emulador antes de esta release.

## v1.0.2 — 2026-11-16

Arranque y presupuesto: el encendido deja de hacer pop, el splash del aparato y el del emulador son el mismo en
los tres modos, y las tres líneas del target que llevaban dos releases en rojo vuelven a estar en verde con la
cuenta y la razón escritas. Detalle en `docs/BOOT_SPLASH_AND_POP.md`.

### Arreglado

- **El pop al encender.** La etapa de salida del códec se desmuteaba dos veces en `fm1_audio_init`: la primera
  con ALNK0 todavía apagado y la segunda 5 ms después de encenderlo, o sea sobre un DAC que no había terminado
  de asentarse y un FIFO que el motor todavía no había llenado. Ahora queda muteada durante el bring-up, el
  motor arranca sobre el buffer en cero y se desmutea recién a los 25 ms de silencio.
- **El splash del emulador y el del aparato.** Es el mismo dibujo (`draw_splash`/`splash_sig`), pero el emulador
  animaba siempre: con MENU > STARTUP SPLASH en SHORT u OFF las dos pantallas no coincidían. El emulador lee el
  mismo ajuste y usa el mismo lapso, así que FULL, SHORT y OFF se ven igual en los dos.
- **`tests/target_budget.txt`**: `phase_render` 98 → 94, `phase_render_noise` 117 → 113 (las dos vuelven a
  entrar en el límite del +10 %) y `analog_ext` 157 → 174 re-basado con su razón escrita: tres ramas por muestra
  menos (PM plegada en la suma de fase, mezcla decidida por bloque, desbordes de fase como valor) y el resto es
  la PM aprobada. Es cuenta estática del código AC791N, no ciclos del aparato.

### Medido (clases, una por línea)

- **Host**, `tests/regress`: 293 renders dorados, **0 cambiados**, 0 fallos de salud, 0 de voz/ruteo, **0
  entradas de CPU sobre presupuesto** (9 notas).
- **Análisis estático** del build AC791N, `tests/target_budget.py`: **0 líneas sobre presupuesto**.
- **Host**, `tests/groups.sh ui`: `ui_render` 190 pantallas × 22 paletas, 0 hallazgos de lint, 3 554 249
  mediciones de alineación sin desalineaciones.
- **Build AC791N**: imagen 583 540 B, XIP 418 740 B, `.data+.bss` 96 836 de 98 304 B, paquete 610 959 B,
  identidad `FM-1_965`.

El pop **no está medido**: no hay unidad conectada y no se escribe en ninguna. Lo verificado es la secuencia de
encendido del códec, no el resultado acústico.

## v1.0.1 — 2026-11-16

Cierra lo que la 1.0.0 dejó listado como abierto y pone la interfaz nueva en un binario de verdad. El árbol y el
paquete vuelven a ser el mismo.

### Arreglado

- **La franja del valor de una tarjeta** repintaba `T_SURF` plano sobre una tarjeta que es un degradado: cada
  dígito que rodaba dejaba una banda de otro tono. Ahora repinta las filas de la placa con sus mismas bandas y
  colores (`cv_plate_rows`, gfx.c), y las siete comprobaciones de `ui_test` que llevaban tiempo en rojo pasan —
  también con `FELUCCA_FM4=1`. Detalle en `docs/CARD_STRIP.md`.

### Interfaz

- **Geometría**: cabecera 0..16, tarjetas 20..76 (56 px), panel 78..200, pie 204..240; MENU > LARGE 20..124 /
  128..200. La etiqueta de una tarjeta se dibuja en la cara de 15 px cuando entra (a 12 se leía fina al lado de
  su propio valor), el gauge cierra el pie de la tarjeta y el medidor de carga de la cabecera queda centrado.
  Ver `docs/UI_LAYOUT_1.0.md`.
- **Las filas de lista** de MENU pintan su rampa con un solo relleno tramado en vez de cuatro bandas: los mismos
  píxeles, una llamada en vez de cuatro por fila (no se declara mejora de tiempo: la medición en host está
  dentro del ruido de la VM).
- **MACROS y MOD, rehechas** con el lenguaje del resto de la interfaz: cada macro en su placa con la pastilla
  M1..M4, el valor en la letra de las tarjetas, un gauge redondeado en el color de calor de la paleta y los
  destinos como chips; cada slot del MOD en su placa, con su pestaña, el número fuera de ella y la barra viva
  redondeada con su pip. Al tocar el knob, la fila toma el acento.

### Medido (clase: host, binarios nativos de esta VM)

- `tests/ui_test.c`: 0 FAIL, `ui: all passed` (y con `FELUCCA_FM4=1`).
- `tests/ui_render.c`: 0 hallazgos de lint en 190 pantallas × 22 paletas y 0 desalineaciones en 3 554 249
  mediciones de 73 sitios.
- `tests/theme_test.c`: contraste y blending en las 22 paletas, peor margen ×1,02.
- Build AC791N de release: imagen 583 572 B, XIP 418 772 B de constantes 144 320 B, `.data+.bss` 96 836 de
  98 304 B, paquete 610 959 B, identidad `FM-1_965`.

Nada medido en el FM-1 físico.


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


La cabecera y el pie cedieron cuatro filas cada uno y las cuatro tarjetas de los knobs ganaron seis (20..76),
con el panel 78..200 y el pie 204..240; MENU > LARGE acompaña (tarjetas 20..124, panel 128..200). La etiqueta de
una tarjeta se dibuja en la cara de 15 px cuando entra —se leía fina al lado de su propio valor—, el gauge
cerró el pie de la tarjeta y el medidor de carga y el chip "A" quedaron centrados en la cabecera que ahora
mide lo que sus iconos. Las filas de lista pintan su rampa con un solo relleno tramado en vez de cuatro bandas.
Detalle y mediciones en `docs/UI_LAYOUT_1.0.md`.

### Verificación de esta entrega
- **Lo que en esta entrega NO estaba verde**: siete comprobaciones de `tests/ui_test.c` fallaban y
  ya fallaban en `main` antes de ella. El paquete de esta versión no las incluye arregladas: se cerraron en la
  `1.0.1` (`docs/CARD_STRIP.md`).

- **Build AC791N real** (toolchain de JieLi + los tres blobs del SDK fijados por SHA-256), no una simulación: el
  paquete, su manifiesto, `SHA256SUMS` y el registro del build están en `release/`.
- **Emulador WASM** compilado con Emscripten 6.0.11 a partir del mismo árbol, con su manifiesto (versión, commit,
  SHA-256 del wasm).
- **Host**: la suite de `tests/run_tests.sh` (goldens de audio 293/0 sin cambios, health, voz, CPU, UI, editor,
  persistencia, formatos de proyecto, matriz, ARP, RATCH, FM6, DRUM-X y el resto).
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

