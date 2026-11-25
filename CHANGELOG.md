## v2.3.16 — Fase 3/3: la ronda de estabilidad, y el último paso rojo en verde

El firmware no cambia: esta ronda mide y documenta, y lo que corrige son dos presupuestos de pruebas que estaban
mintiendo.

- **Estrés con sanitizadores**: `tests/stress_run.sh 5` — proyecto, persistencia, robustez y MIDI, cinco corridas
de cada uno bajo ASan/UBSan, más dos pasadas completas de los goldens: 0 fallos, 0 fugas, 0 UB.
- **Sesión larga**: `audio_stress` ya no se corta a los 43 s; una sola corrida cubre **600 s de audio** con seis
motores, seis pistas, todas las buses, MIDI y cambios de preset bajo las notas: peak 0.300/0.300 sin una trama
sobre escala completa, DC 0, ningún salto de muestra sobre 0.5, ninguna muestra no finita y un **p99 de 237 µs**
de los 725 µs que tiene el dispositivo por bloque.
- **El paso rojo que quedaba**, `tests/target_budget.py`, tenía dos causas y las dos eran del presupuesto, no del
código: `events_block` caía 27 % por el anidamiento con el que el compilador cuenta las instrucciones (1 783 →
1 797 instrucciones, el mismo trabajo), y `fm6_render` estaba presupuestado en `daf5e39`, antes de que `f5e0ad7`
le añadiera los dos pases condicionales de CHARACTER. Re-base revisado, con el porqué escrito en el archivo; las
otras diez líneas no se tocaron porque estaban dentro de su tolerancia. El paso sale 0.
- **Visual y audio sin regresiones**: `ui_render` sale 0 y la puerta de píxeles da los **3 241 cuadros idénticos**
a la baseline de la fase 2; `six_live` 61/61 s de audio, `click_test`, `slicer_test`, `morph_six` y `perform_test`
sin un click.
- **Instalador y paquete**: `install_test.py`, las pruebas web y de backup, `release_gate_test`,
`make_release --check` y `coherence_check` en verde; paquete 610 933 B, sha256 `25a55a92…`.

## v2.3.16 — Renovación visual: negros más hondos, luminosos más ricos, y el lint de UI en cero

- **Las dos zonas, en los tokens.** El ajuste no se hizo pantalla por pantalla: se hizo una vez, en
  `palette_set()` (`gfx.c`), que es de donde salen `T_BG`, `T_SURF`, `T_THEME`, `T_ACCENT` y todo lo derivado de
  ellos (rampas, degradados, placas, tramas). Las 22 paletas y las 203 pantallas lo heredan sin que ninguna se
  quede atrás. **Zonas oscuras**: en una paleta oscura el fondo baja un 16 % y la superficie un 5 %, así que el
  negro y el azul profundo quedan definidos y el escalón entre fondo, placa y pieza **se agranda** en vez de
  cerrarse. **Zonas claras**: el tema y el acento se alejan un 18 % de su propia luma, sin cambiar de tono: más
  cian, más azul, más violeta, más turquesa, y los degradados construidos sobre ellos (el calor de los gráficos,
  los medidores, los anillos) llegan más lejos. Nada se re-tiñe y ningún color entra: sólo se mueven los tres
  números, y despacio.
- **GREY, MONO y las claras quedan como estaban.** GREY conserva cada tinta sobre la línea de grises (R = B,
  G = 2 R) porque la comprobación recorre la pantalla entera (`ui_test.c`); MONO se queda en negro y blanco
  puros; PAPER y las demás claras no se oscurecen — conservan su papel.
- **El lint de UI, en cero.** `ui_render` salía 1 con **4 267** hallazgos desde antes de la fase 1 de esta
  entrega (483 «FM6 part through a box» en los charts de algoritmo, 113 «spills out of its cell» en DX IMAGE, y el
  resto repartido entre los mismos dos dibujos). No eran cosméticos:
  * **Charts de FM6.** La ruta de un operador arrancaba en el borde de su caja, o sea **dentro de la sombra de 2 px
    que la caja proyecta**: el cable salía por debajo de la sombra, no por debajo de la caja. Ahora arranca dos
    píxeles más abajo, el codo se dibuja en la única fila libre de la banda entre dos filas (la de arriba proyecta
    su sombra, la de abajo empieza en su placa) y el punto de bifurcación/fusión va una fila más abajo que el codo,
    donde su fila superior no toca esa sombra. La realimentación sale por el lado libre: el lado se **elige**
    probando el rectángulo que necesita contra cada caja, así que un operador con vecino a la derecha la lleva a la
    izquierda en vez de dibujarla encima.
  * **DX IMAGE.** Las etiquetas de dos letras (BD SD CP CH OH TM RS CB) se dibujaban un píxel **por encima** de su
    placa: se veían colgando del borde. Ahora entran en su celda.
  * **El mensaje del lint.** Cuando un cable pasaba cerca de una caja, el informe imprimía dos veces el mismo
    píxel; ahora imprime el rectángulo de la caja, que es lo que hace falta para arreglarlo.
- **Resultado**: `ui_render` sale **0**; sólo quedan las cuatro líneas del auto-test (las que comprueban que el
  lint *puede* fallar). 3 241 cuadros de UI regenerados en la baseline, 0 desvíos de 1 px en 3 967 423 medidas,
  0 tintas sobre su propio color, 0 píxeles fuera de gris/neutro en GREY y MONO.
- **Sin cambios de comportamiento**: ni un parámetro, ni un atajo, ni una animación tocada. Esta versión cambia
  cómo se ve y arregla lo que estaba mal dibujado.

## v2.3.15 — LFO en nueve pantallas, DEST con profundidad, y el knob 2 de TUNE que ya no escribe el 3

- **MOD MAP, fuera.** Era la página MOD leída de otra manera (solo los slots asignados, con un chip que contaba las
  rutas que comparten destino) y no tenía ni un control propio: una vista de la página de al lado gastando una de
  las diez pantallas de la familia LFO. Se eliminó la entrada de `PAGES[]`, su enumeración y todo su código de
  dibujo. El contador de la familia pasa de `x/10` a `x/9` en todas sus pantallas (LFO, LFO DEST, LFO CURVE, LFO
  SHAPE, MOD, MSEQ, MSEQ 2, MACROS, SCENES): el índice lo calcula `params.c` desde `PAGES[]`, así que no hay ningún
  número escrito a mano que actualizar y `tests/ui_render` recorre las nueve sin huecos.
- **LFO DEST / ENV DEST, rediseñadas.** Eran cuatro columnas bipolares sin nombre, sin valor y sin escala. Ahora
  son dos bandas: (1) **la fuente en profundidad** — el ciclo del LFO, o el ADSR del track, dibujado tres veces con
  el motor de profundidad que ya usa la página ENV (`qz_curve`: la de delante a 2 px y encendida, las de atrás
  más chicas, levantadas y más oscuras), con la aguja del momento, el punto del nivel actual y, en el envelope, una
  regla encendida con el nivel que tienen las voces; (2) **una placa por destino** con su nombre, un raíl bipolar
  con el cero y los cuartos, el alcance que permite el knob (barra tenue), la parte de ese alcance que la fuente
  está usando **ahora** (barra del acento hacia un lado, tinta media hacia el otro, con pip en la punta) y el valor
  con el formato de la tarjeta. `LOOP` de ENV DEST no es una profundidad: dibuja su chip y las envolventes que
  repite, sin raíl, para que un modo no se lea como una ruta en cero. Todo sale de `lfo_ph` / `lfo_val` / las
  voces, nada de un temporizador.
- **El S&H de LFO DEST no es una raya.** Un ciclo de sample-and-hold es un valor sostenido: dibujar uno solo daba
  una línea plana que parecía rota. La banda muestra ahora los ocho ciclos que forman la escalera (el mismo hash
  que la página LFO usa), así que se ve el escalón y se mueve cuando la fase cruza un ciclo.
- **SCL TUNE: la escritura cruzada, cerrada.** `tune_ui_sync()` (`tune.c`) releía los cents de la nota nueva en
  `tune_ui_ct_l` pero seguía comparando con el `ct` local de la nota **anterior**; al diferir, llamaba a
  `tune_set(nota_nueva, cents_viejos)` y el knob NOTE reescribía en silencio el parámetro del knob CENTS. Ahora el
  local acompaña a la selección, la comparación ve "sin edición" y la tabla no se toca: cada knob modifica solo el
  suyo. `tests/tune_test` cubre el caso (mover la nota no cambia ningún cent).
- **SCL TUNE: la mano se ve y se mueve.** Las notas sostenidas llevan una regla bajo su propia letra (se leen
  aunque el temperamento esté en 0 cents), un pip recorre su radio hacia el borde y se apaga al llegar, y el
  desplazamiento de la nota se sigue leyendo en su radio. La animación es la fase de `ui.frame` (cuatro frames por
  paso, ~15 fps) con el índice de la nota como desfase: doce radios de geometría, nada por muestra, y la página
  queda quieta cuando no hay teclas. `graph_signature()` no incluía ni el temperamento ni las teclas sostenidas —
  la página conservaba el dibujo con que nació —, así que ahora los cuatro números de la tabla, el `dirty` y las
  notas sostenidas entran en el hash: un knob repinta en el frame siguiente.
- **El anillo que el comentario prometía.** El círculo decía "cada nota, un radio desde un anillo común" y el
  anillo no existía: a 0 cents cada radio es un punto y la rueda se leía como polvo. Un polígono de doce segmentos
  en `T_LINE` es ese anillo, y una desviación ya tiene de dónde salir.
- **Medido en host, no en la unidad.** `ui_render`: 203 pantallas x 22 paletas + el barrido de páginas/valores,
  0 desviaciones de 1 px en 3 967 423 mediciones de alineación, 0 tintas sobre su propio color, 0 píxeles fuera de
  gris/neutro en GREY/MONO. El estado `LFO DEST` del barrido pone el LFO en marcha (`lfo_ph`, `lfo_val`,
  `lfo_fade`) para que el frame pruebe que la aguja, el punto y las barras vivas se dibujan desde el estado.
  Clase de cada número: `[HOST]`; sin unidad FM-1, nada aquí es una medición de hardware.

## v2.3.14 — FM6 devuelve sus voces, y sus tests dejan de medir el panel

- **El defecto que quedaba abierto, cerrado.** Una nota soltada no volvía a quedar libre: la etapa 4 del envelope
  (el release llegó a L4) seguía devolviendo su nivel, así que el gain del operador quedaba en L4 y
  `fm6_note_done()` —que exige `gain_out < FM6_DONE` en cada portadora— no la daba por terminada nunca. `regress` lo
  veía como HEALTH FAIL en `FM6/00_SILT_KEYS` (DC 19, pico 582 a los 12 s) y `FM6/31_FERROUS_PAD` (DC −172, pico
  2288); con seis voces, dos presets que no vuelven son dos voces menos para el resto de la canción. Ahora la cola
  del patch se respeta tal cual y la voz tiene un presupuesto: `rel_t` cuenta las muestras desde que se suelta la
  tecla y a los 5 s (`FM6_REL_MAX`) la voz se desvanece en 60 ms (`FM6_REL_FADE`) y vuelve al motor. Las dos voces
  quedan libres a los 5.06 s, con DC 0 y cola 0.
- **Lo que se probó y se descartó, anotado.** Hacer que la etapa 4 devolviera silencio arreglaba los dos HEALTH FAIL
  pero rompía `tests/fm4_test`: las conversiones de FM4 se comparan contra el motor retirado a 0.05/0.3/1.0 s y sus
  colas viven en L4 — el centroide pasaba de 18/18 a 7/18 (peor caso 3.78 octavas). Un suelo en la tasa de release
  (~4 s) cambiaba 28 renders y tampoco hacía falta con el presupuesto de tiempo. Ninguno de los dos está en el
  código; los dos están explicados en `docs/FM6_VOICE_RELEASE.md`.
- **`tests/fm6_test`: de 5 fallos a 0.** Cuatro no eran del motor: `setup()` dejaba los knobs de envelope del track
  en los de SILT KEYS y `eng_fm6.c` los aplica al patch (ATK/DEC/REL restan de las tasas de los operadores y SUSTAIN
  **reescribe** L3 de todos: `50 + SUS*49/126`). Con DEC 74 la R2 50 del patch de prueba quedaba en 1 (~40 s de
  decay) y con SUS 116 su L3 0 pasaba a 95. El fixture neutraliza los tres knobs de tasa y, en el caso percusivo, el
  de sustain. Ninguna aserción se tocó.
- **29 renders golden de FM6 rebaselinados**: son los presets cuya cola ahora termina. `tests/fm4_test` sigue en
  18/18 y 18/18, y ningún otro motor cambió un render (311 comparados).
- **`tools/budget_report.py` compila otra vez**: su sonda incluía `core.h`, que llama a `pc_arm()` (ui.c) desde un
  helper inline; ahora la sonda define el símbolo en vez de no enlazar, y el informe de RAM vuelve a salir.

Detalle, evidencias y mediciones: `docs/FM6_VOICE_RELEASE.md`. Clase de cada número: `[HOST]`; sin unidad FM-1,
nada aquí es una medición de hardware.

## v2.3.13 — EXTRA auditada, PANEL Y legible, y el jack con su limitación escrita

- **PANEL Y dice qué es.** Sus dos valores eran `0` y `80` y no significaban nada: es la franja de la memoria del
  controlador de 240x320 a la que está pegada la pantalla de 240x240 (`lcd.c lcd_yoff`). Ahora los valores son
  `0 TOP` y `80 LOW`, y el sentido es ascendente (0 → 80 → 0), no "de 80 a 0": son dos franjas, guardadas en un bit.
  La función **no estaba desconectada** — `tests/lcd_st7789_test.c` demuestra que con 80 la fila 0 de la UI se
  escribe en la fila 80 de la memoria del panel y que un relleno de pantalla completa limpia también las filas que la
  franja deja fuera. Lo que pasa es que no cambia nada del dibujo: en una unidad pegada a 0, cambiar a 80 mueve la
  imagen 80 filas y se ve rota, que no es "un ajuste haciendo algo". Está documentado en `docs/EXTRA_AUDIT.md`.
- **EXTRA auditada fila por fila** (`docs/EXTRA_AUDIT.md`): KNOB SELECT, CPU MODE, VIS QUALITY, SPLASH, VOICES y
  PANEL Y, cada una con su byte de almacenamiento, rango, momento de aplicación, consumidor y el test que la cubre.
  Las seis estaban vivas: ninguna desconectada, invertida ni con rango imposible. `VOICES` parece invertida porque su
  byte lo está a propósito (un registro viejo lee 12).
- **La línea `RUNNING` de EXTRA estaba muerta y se retiró.** Con seis filas el panel termina en la 214 y las teclas en
  la 224: nueve filas de banda donde una línea de fuente pequeña necesita once. Se probó una línea de ayuda ahí, se
  midió que no cabe y no se dejó: los núcleos y la carga que esa línea mostraba están en ABOUT.
- **El jack, con la verdad delante** (`docs/AUDIO_JACK_FM1.md`): el firmware tiene **una** salida estéreo (ALNK0 → códec
  externo) y **un** único control de salida, PC6, que es el mute de la etapa de salida del códec y calla **las dos**
  salidas — no sirve de interruptor de parlante. No hay ninguna línea de detección de jack en el HAL (todos los pines
  usados están documentados en `firmware/hal/fm1_input.h`) ni en `firmware/`, y el SDK de este entorno no trae sus
  cabeceras de audio. Sin saber si el jack del FM-1 es conmutado por contacto (y entonces el hardware ya lo hace) o si
  su contacto va a un GPIO, **no se implementa**: callar PC6 al conectar los auriculares los callaría a ellos. El
  documento trae el procedimiento de banco de 10 minutos para resolverlo en la unidad.
- **`[HOST]`** `ui_render` 204 pantallas × 22 paletas, 4 267 hallazgos de lint, 0 desvíos de alineación; baseline de
  píxeles regenerada (el texto de PANEL Y cambia).

Sigue abierto, y anotado en el documento del jack: **FM6 no libera voces siempre tras el note-off** (`tests/regress`
HEALTH FAIL en `FM6/00_SILT_KEYS` y `FM6/31_FERROUS_PAD`; `tests/fm6_test`).

## v2.3.12 — la Página 3 de SCL dice lo que tocas, los mini-knobs crecen, la barra de STORED entra y la batería al cargar

- **SCL > TUNE (3/4) responde a la mano.** Los doce radios de la rueda encienden las notas que la pista tiene
  sonando —con un punto en el radio, nunca encima de la letra— y la línea del pie nombra lo que suena con las mismas
  `scl_recognise` / `scl_chord_name` de la página CHORD: `PLAY C` para un Do, `PLAY C` para un Do mayor, `PLAY Cm7`
  para el acorde. Se lee de las teclas que la pista sostiene, así que sigue igual un toque en el panel que una nota
  por MIDI. El toque en la rueda ya elegía la nota editada y el hub ya volvía a temperamento igual: eso queda.
- **Los radios ya no cruzan las notas.** El recorrido hacia arriba era de 16 px desde `TUNE_RB` 20 y la letra vive
  en el radio 40: el extremo agudo terminaba *sobre* el nombre que señala. Son 12 px (máximo 32 contra 40), con 8 px
  de aire hasta la letra.
- **LFO > SCENES 10/10: la barra de A SCENE / B SCENE entra de una vez.** `PANEL_X0` es 10, no 4: la fila que insetiné
  en 2.3.10 medía 228 desde 12 y terminaba en la columna 239 — un píxel fuera del panel, que es lo que se veía como
  barra salida por los dos laterales. Ahora mide 218 (12..229) y el `STORED` / `KNOB 4: STORE` va alineado a la
  derecha del panel (`cv_text_r`), así que ningún estado del texto puede volver a tocar el marco.
- **EDIT 1..3: los cuatro mini-knobs del módulo WAVE 1, más grandes.** Radio 9 → **11** (diámetro 19 → 23 px, +58 % de
  área): el paso de 26 px deja 4 px entre aros, y la fila guarda 5 filas de aire con la raya de arriba y 5 con el pie
  del módulo. Nada se superpone.
- **El icono de batería al cargar (USB) se lee como un rayo.** Los tres tramos se tocaban esquina con esquina y el
  resultado era un bloque con una barra cruzada. Ahora la cabeza va arriba a la derecha (5x4), el cuello es una celda
  de solape y el pie cae ancho a la izquierda (5x5): la diagonal es lo que lee el ojo.
- **`[HOST]` el barrido de render cubre la Página 3 de SCL**: `scl_tune` y `scl_tune_play` (con C-E-G sostenidos)
  entran en la barrida, que pasa de 202 a **204 pantallas × 22 paletas**; 4 267 hallazgos de lint (la constante), 264
  textos elipsados, **0 desvíos de alineación en 3 968 509 medidas**, y `tests/ui_pixels.txt` regenerado (3 257 fotogramas).

## v2.3.11 — el slot legacy de DRUM vuelve a ser su alias

- **`CYBER DRUM` vuelve al id 11, con `.legacy = 1`.** En v2.3.8 la colección curada sustituyó ese alias por `CONCRETE KIT`
  «en el mismo índice, para que ningún proyecto guardado se mueva» — y era justo al contrario: el alias existe para que
  un proyecto, un favorito o un preset de usuario guardado antes de 1.6 cargue **ese** kit, y al volverse navegable la
  librería DRUM pasó de 16 a 17 sonidos y `eng_preset_nth` desplazó uno a todos los que vienen después del 11.
  `CONCRETE KIT` se retira: nunca tuvo un slot propio, se había metido en el de un alias.
- **`[HOST]` primera suite completa desde v2.3.7** (`NO_PACKAGE=1 bash tests/run_tests.sh`), 3 480 checks verdes y
  **4 en rojo**, ninguno de audio o de motor: los dos del alias de DRUM (`tests/ui_test.c` y su variante `FELUCCA_FM4=1`,
  arreglados aquí), el índice de evidencia (`docs/EVIDENCE.md`, regenerado con `tools/evidence_index.py`) y los dos
  checks de `tests/ui_render.c` — estos últimos **rojos por construcción**: el binario devuelve 1 si `nfind` no es cero,
  y su lint reporta 4 267 hallazgos conocidos (etiquetas de dos letras que sobresalen de su celda a propósito, partes de
  FM6 medidas sobre su caja). Esa condición es anterior a esta versión y no la cambio aquí: arreglarla exige decidir
  cuáles de los 4 267 son aceptables y ligar el check a una línea base, no relajarlo.
- **Auditoría MIDI y de ajustes sin hallazgos nuevos:** `midi_control_test` 96 checks (CC, learn, canales, bend, clock,
  panic CC120/121/123, RPN/NRPN) y `midi_uart_test` 14 (running status, realtime, SysEx, ring lleno y timeout) en verde;
  `settings_test` (PER1/2/3, paletas, calibración, HOLD, LEDS, CLICK, COUNT-IN) y `persistence_test` (fallo de flash,
  reintento al cruzar el reloj, proyecto intacto) en verde. Nada que corregir en esos dos subsistemas con la cobertura
  que ya tienen.
- **`[HOST]`** `ui_render`: 202 pantallas × 22 paletas, 4 267 hallazgos de lint, 0 desvíos de alineación; audit de presets
  `passed` (293 presets, 0 silenciosos, 0 duplicados).

Sin hacer de la fase 10: la segunda ronda de optimización de CPU y la mejora del diagrama de SCL Página 3 siguen
pendientes; esta entrega es el bug funcional que la fase pedía resolver primero, más los pendientes cerrados en v2.3.10.

## v2.3.10 — el degradado hasta el suelo, las barras de SCENE dentro del panel y CENTS sin solape

- **El degradado de la página llega al suelo.** `graph_ground_ramp()` terminaba en el 97 % de la altura del panel: en
  las pantallas de cuerpo corto —todas las de SCL, el kit— quedaba una banda de fondo liso debajo del degradado. Ahora
  baja hasta la última fila del panel (`graph_h() - 2`), en todas las pantallas a la vez.
- **LFO > SCENES 10/10: las barras de A SCENE y B SCENE entran en el panel.** La fila se dibujaba desde `PANEL_X0`
  con 232 px, así que su barra arrancaba sobre el borde redondeado del panel y terminaba encima del otro: ahora se
  insetá 2 px por lado (228 px) y el texto acompaña.
- **SCL > TUNE: el indicador de CENTS ya no se pisa.** El número grande en `AF_L` bajaba hasta la fila de la barra;
  sube dos filas (17 en vez de 20) y la barra baja tres (43 en vez de 40), con su marca de cero corrida igual.
- **`[HOST]`**: `ui_render` 202 pantallas × 22 paletas, 4 267 hallazgos de lint, **0 desvíos de alineación**.

Pendiente de esta tanda, sin hacer: el diagrama de SCL Página 3 (que reaccione al toque y nombre el acorde que suena,
y las líneas azules que se cruzan con las notas) y los mini-knobs de EDIT 1/2/3 más grandes.

## v2.3.9 — profundidad donde no había curvas, el chorus legible y dos recortes de flash

- **Las páginas de diagrama ganan profundidad sin apilar copias.** OPERATOR / OPERATOR 2 (las cajas del algoritmo
  FM6 y del chart DIGITAL), SOUND del kit (los ocho pads) y STEREO (las fichas del kit) llevan ahora una **sombra
  proyectada**: la proyección de la forma 2 px abajo y a la derecha con su propia tinta al 26 %, y su contorno a 1 px
  al 14 % — la misma degradación hacia el fondo que las capas de las curvas, pero dos pasos en vez de cuatro.
- **FX > CHORUS, legible y con el stack de cuatro capas.** La función es `tech_chorus()` (`ui_tech.c`), servida por
  `GR_NONE` → `graph_tech()` → `case G_CRATE`. Dibujaba la lane tres veces con un empujón de fase falso y todas casi
  al mismo brillo: ahora son cuatro pasadas de la MISMA barrida (`sy = 1000 − k·80`, `dy = −k·5`, `qz_back_ink`),
  el frente a tinta plena y 2 px, las de atrás hundiéndose hacia el fondo.
- **El campo profundo del visualizador, 11 % más barato sin cambiar un píxel.** `frame_bench` [HOST], mínimo de 6
  series de 120 fotogramas: **250 467 → 222 600 ns** por fotograma del visualizer (−11.1 %); `gw_viz_milky`
  33 250 → 22 929 ns (−31 %). 44 renders (22 paletas × normal y LARGE) byte a byte idénticos.
- **`UAC_FIR` de Q30 a Q15: −3 840 B.** El resampleador de 48 kHz guarda sus 80 × 24 taps en `int16` Q15.
  Medido en host (`tests/uac_test.c`): THD de 1 kHz **−118.3 → −115.4 dB**, SNR en banda **94.6 → 87.8 dB**
  (piso de ruido, no distorsión), respuesta 20 Hz..18 kHz sin cambios. Las tolerancias del bench que estaban escritas
  para Q30 se ataron a `UAC_FIR_Q`, y el chequeo de fidelidad de la tabla quedó más estricto (13 unidades absolutas
  contra 21 474).
- **`--sort-section=alignment` en el enlace: −128 B** de relleno entre secciones, sin tocar una función.
- **`preset_t` no tiene relleno que recuperar.** En el host mide 40 B (punteros de 64 bits); en el AC791N mide
  **36 B** — `ANALOG_PRESETS` 2 628 B / 73, `FM6_PRESETS` 1 332 B / 37, `PHASE` 1 224 B / 34: exactamente 36 B por
  preset. Las tablas de nombres ya comparten sus cadenas: `-fmerge-all-constants` no movió un byte.
- **`[HOST]`**: `ui_render` 202 pantallas × 22 paletas, 4 267 hallazgos de lint, **0 desvíos de alineación**;
  `tests/uac_test.c` **all ok**; baseline de píxeles regenerado.

## v2.3.8 — la colección curada, y el DC de MERCURY BLOOM

**Seis presets casi duplicados dan su plaza a seis sonidos nuevos.** No se borró la entrada: se **sustituyó en su
mismo índice**, así que ningún proyecto guardado cambia de número y el flash no crece ni un byte.

| Se va | Por qué | Llega |
|---|---|---|
| `TRAP 808` | el 808 lo da `808 SUB`; esto era el mismo con más drive | **`SUB FLOOR`** — el grave limpio, con la cabeza del env recortada para no saturar |
| `CYBER DRUM` | `HEARTH KIT` ya es el kit completo; esto era una variación de niveles | **`CONCRETE KIT`** — kit seco, con más cuerpo de bombo y caja |
| `DEEP MARIMBA` | `MARIMBA` + mono + chorus | **`FERROUS PAD`** (textura) — FM con ataque lento, algorithm 6 |
| `GAMELAN MET` | misma algorithm y misma dosis de HARM que `GLASS KEYS` | **`IRON PLUCK`** (performance) — púa FM brillante, cola corta |
| `TINE EP` (FM6) | **mismo nombre** que el `TINE EP` aditivo | **`SILT KEYS`** (híbrido) — FM con segunda onda de anillo |
| `HALO CHORUS` | `MYSTIC CHOIR` ya es el coro | **`VEIL PAD`** (textura) — formantes altos, sin vibrato de coro |

- **`MERCURY BLOOM`, DC corregido:** 12,8 % → **por debajo del 2 %**, ya no aparece en la lista de DC. El desvío venía de la segunda onda de la distorsión de fase
  (`WAVE2 = 4`), que es asimétrica: con la principal en `WAVE 2` las dos mitades del render caían a −27,8 % / −19,4 %.
  Pasa a `WAVE 0` con el mismo `DCW`, conservando el pad. El máximo de la casa baja a **3,64 %** (era 12,8 %).
- **Audit de presets:** 293 presets, **0 silenciosos, 0 calientes, 0 nombres repetidos, 0 duplicados exactos,
  0 fuera de rango**, 5 con DC real por encima del 2 % pero señalando las dos mitades (bytebeats asimétricos por
  diseño, reportados y no fallados): **PASSED**.
- **`[HOST]`**: `ui_render` 0 hallazgos, 0 desvíos de alineación.

## v2.3.7 — los tipos de filtro, y el teclado del ARP

- **FILTER 5/9: siete tipos, siete curvas.** La gráfica que entró en v2.3.4 sólo distinguía LP, HP, BP y COMB;
  VOWEL, SCREAM, NOTCH y SHELF caían los cuatro en la rama LP, así que SHOW, NOTCH, SCREAM y VOWEL se veían
  **idénticas**. Ahora cada tipo tiene su forma: **LP** rolloff, **HP** espejo, **BP** una cresta, **COMB** el
  tren de muescas, **VOWEL** tres formantes fijos (250 / 520 / 780), **SCREAM** un pico estrecho sobre una
  pendiente, **NOTCH** un pozo donde está el corte, **SHELF** un escalón.
- **RES y DRIVE ya mueven la curva.** La resonancia suma sobre el corte (`q · bump / 150 · 0,5`, hasta +75 de
  1 180) y **DRIVE** levanta la curva entera (`dr · 140 / 127`): girar cualquiera de los dos se ve en el panel,
  que era lo que faltaba.
- **ARP: teclado más alto y sin solapes.** Las teclas blancas crecen de 36 a 39 px y su nombre pasa a la fila
  46, dentro de la tecla y **por debajo** del pie de las negras, que era el solape que se veía; ROUTE, la fila de
  notas, los medidores y el pie quedan exactamente donde estaban, así que nada se sale del panel.
- **`[HOST]`**: `ui_render` 0 hallazgos de lint, **0 desvíos de alineación** en 3 964 841 medidas; cuatro
  pantallas nuevas del barrido (`filter_shelf`, `filter_notch`, `filter_vowel`, `filter_scream`) para ver cada
  tipo.

## v2.3.6 — cierre de la auditoría final

**Sin cambios de firmware de sonido ni de dibujo.** Esta versión empaqueta la auditoría de la fase 9: la suite
completa en verde y el último hueco de cobertura cerrado.

- **Suite completa `[HOST]` en verde**: `NO_PACKAGE=1 bash tests/run_tests.sh` termina en
  **`ALL HOST TESTS PASSED`**, con ASan/UBSan, los dos fuzzers (20 000 iteraciones de proyectos, 5 000 de
  editor), los renders golden, la CPU, el DSP, el emulador y las 3 161 imágenes de píxeles.
- **Matriz de modulación audible**: nuevo `test_slots_audible()` en `tests/mod_test.c`. Los cuatro slots
  almacenados (LFO → CUT, profundidad 90) **cambian la frase renderizada** respecto de la misma frase sin
  matriz. Hasta ahora se probaban la matemática y la caché por bloque, pero ningún test movía la profundidad de
  un slot contra una nota: un routing que resolviera la fuente y nunca llegara al destino habría pasado todo.
  Era el hueco que la matriz de validación declaraba abierto.
- **El pendiente de `drumx` PUNCH y `live`** que la matriz arrastraba **no se reproduce** en este árbol: la
  suite entera pasa. Quedaba anotado como «falla en `main` antes de tocar nada»; hoy no falla.

## v2.3.5 — OP ENV entra en la profundidad

**Cambio visual deliberado** en la página OP ENV de FM6. Baseline de píxeles regenerado por esto.

La auditoría de las páginas de EDIT que dibujan una forma, una por una:

| Página | Gráfica | Era plana | Qué se hizo |
|---|---|---|---|
| MORPH | `graph_morph` | no | cuatro capas desde v2.3.4 |
| TILT | `graph_tilt` | no | cuatro capas desde v2.3.4 |
| FILTER (dos variantes) | `graph_filter` | no | cuatro capas desde v2.3.4 |
| OP1..4 ENV | `graph_adsr` | no | stack de ENV desde v2.3.3 |
| **OP ENV** | `graph_fm6env` | **sí** | **cuatro capas ahora** |
| OPERATOR / OPERATOR 2 | `graph_fm6` | sí | **no**: es un diagrama de algoritmo (cajas y rutas), no una curva; apilarlo no significaría nada |
| SOUND / SOUND 2..4 / X-MOD | `graph_colony` | sí | **no**: son las 18 líneas del kit como barras, no una forma; el degradado de una curva ahí sería decoración |
| STEREO / KIT 3 | `graph_dximg` / `graph_colony` | sí | **no**: puntos y colocación, sin forma que apilar |
| EDIT 1/2, CHARACTER, RETRIG, LEVELS, VOICE, OP LEVEL | `GR_NONE` | — | no dibujan gráfica |

- **OP ENV** (`graph_fm6env`): la envolvente del operador FM6 son seis puntos y cinco segmentos. Ahora se dibuja
  **cuatro veces** —cada capa más plana sobre la línea base, 4 px más alta y más oscura (`qz_back_ink`)— con la
  misma geometría (`1000 − k·80` per-mille, `−4k` px) y el mismo degradado que MORPH, TILT y la respuesta del
  filtro. La etapa elegida sigue marcada **sólo en la frontal** (el acento y el punto), como estaba.
- **`[HOST]`**: `ui_render` 0 hallazgos de lint, **0 desvíos de alineación** en 3 957 005 medidas; pantalla nueva
  `fm_op_env` en el barrido para poder verificar la página.

## v2.3.4 — la profundidad de las gráficas, en todas y a cualquier valor

**Cambio visual deliberado** en ENV, LFO, MORPH, TILT y FILTER. Baseline de píxeles regenerado por esto.

- **MORPH ya tiene profundidad a 0.** El stack dibujaba los dos EXTREMOS del crossfade, y sólo mientras el knob
  estaba entre ellos: en MORPH 0 había una línea y nada detrás, y al girar el knob la frontal se movía mientras
  las dos de atrás quedaban clavadas. Ahora son **cuatro capas de la misma curva**, cada una 6,4 % más plana,
  5 px más alta y más oscura que la anterior: el efecto existe en 0, en 127 y en todo el recorrido, y **las
  cuatro se mueven juntas** cuando gira el knob.
- **TILT igual que MORPH**: cuatro capas de la curva en la que está el knob (antes: las dos puntas del recorrido,
  con una sola línea cuando el valor caía sobre un extremo).
- **ENV: una capa más** (seis, antes cinco), con el degradado repartido sobre la pila entera
  (`58 % … 14 %`) en vez de un paso fijo.
- **FILTER (5/9) ya tiene gráfica.** La página era cuatro knobs y un panel vacío: la única pantalla que va
  entera de una forma no mostraba ninguna. Dibuja la **respuesta del filtro de la pista** leída de sus propios
  ids (LP/HP/BP/COMB, corte y resonancia; en ANALOG desde `E4/E5`, en el resto desde `FCUT/FRES`), en **cuatro
  capas** con el mismo degradado de profundidad que el resto.
- **LFO S&H legible.** Al echar atrás una onda sample-and-hold un tercio de ciclo, los valores aleatorios son
  OTROS: cuatro escaleras sin relación cruzándose, la pantalla ilegible del reporte. El eco conserva la fase de
  la frontal (los mismos escalones) y sube 2–3 px por capa en vez de un sexto del panel; LFO pasa de tres a
  **cuatro capas**.
- **`[HOST]`**: `ui_render` 0 hallazgos de lint, **0 desvíos de alineación** en 3 955 127 medidas; el banco
  `morph_depth` pasa sus cinco comprobaciones con **4 capas en 0, 64 y 127**, la trasera a **3 px** de mediana y
  la frontal junto a una trasera en **110 de 110** columnas.

## v2.3.3 — los gráficos en profundidad: la misma curva, cada vez más oscura

**Cambio visual deliberado** en las cuatro páginas con stack (ENV, LFO, MORPH, TILT). El baseline de píxeles se
regeneró por esto, no por ruido.

- **ENV: las capas de atrás eran de otros tracks.** El stack dibujaba la envolvente de este track delante y las de
  otros tres detrás, así que **mover un knob movía solo la capa frontal** y el resto quedaba congelado: se leía
  como un dibujo roto. Ahora las cinco capas son **la misma envolvente**, la de esta pista, y lo que las separa es
  la distancia —cizalla, elevación, tamaño y tinta— no los datos. Un knob mueve las cinco a la vez.
- **Más profundidad, dentro del panel.** Primero subí la elevación a `h/4` por capa y las dos de atrás **se
  salían por el borde superior**. La geometría es ahora `dx = w/16`, `dy = h/14`, encogimiento 9,5 % por capa: el
  encogimiento baja el techo de cada capa más de lo que la elevación la sube, así que las cinco quedan dentro.
- **Se oscurecen de verdad.** `qz_ink` mezcla hacia el fondo de la página con porcentajes **58 / 44 / 30 / 16**:
  la segunda capa ya está claramente detrás y la quinta es un tono por encima del fondo. En MORPH y TILT la tinta
  de cada pasada trasera sale de su propio `sy` (`qz_back_ink`: 86 / 70 / 54 %), así que el degradado sigue el
  mismo orden en que las capas se alejan.
- **LFO**: el eco va hacia el fondo con la misma escala (58 / 40 %) y sube `graph_ht/16 + 2` px por capa, en vez
  de 5 px fijos y una mezcla hacia el tono frío.
- **Menos facetas.** Los muestreos del stack pasan de **41 a 81 puntos** (uno cada 2,7 px en un panel de 220): la curva
  frontal ya no son 41 segmentos rectos. Con 111 el compilador desenrolla esos bucles y la imagen crecía
  1 776 B, así que 81 es el punto donde la curva se ve lisa y el presupuesto aguanta.
- **`[TARGET]`**: **580 692 B de 583 612, 2 920 libres** (por encima del suelo de 2 048); RAM `.data+.bss`
  4 976 B libres.
- **`[HOST]`**: `ui_render` 0 hallazgos de lint, **0 desvíos de alineación** en 3 953 168 medidas, 22 paletas.

## v2.3.2 — el stack de MORPH, probado donde se dibuja

**Sin cambio visual.** El único motivo de esta versión es que la prueba que faltaba exista en el repo y se
ejecute en la suite.

- **`tests/morph_depth.c`**: banco headless que dibuja el panel real y lee, **columna a columna (0..110)**, la
  `y` de cada una de las tres pasadas, más el rango visible del panel (x 10..229, y 12..80, lienzo 122 px).
  La geometría no se recalcula: `graph_morph` guarda el estado de cada pasada (`morph_last`) y `morph_y` es la
  función que usa el dibujo, así que el banco mide lo pintado y no una segunda copia de las matemáticas.
- **Lo que comprueba**: `[HOST]` con SAW→SQR al 50 % hay **3 pasadas**, la frontal aparece en el lienzo en
  **110 de 110** columnas y la frontal junto a una trasera en **110 de 110**; la separación con la trasera más
  cercana es de **4 px en la mediana** (0 donde las ondas se cruzan, que es geometría) y **91 de 110** columnas
  muestran un hueco ≥ 2 px. Con SQR→TRI al 25 %: mediana 6 px, 104 de 110. Con MORPH en 0 o en 127 (exactamente
  sobre una onda) hay **una sola pasada**, como antes del stack.
- **Píxeles frente a geometría**: el banco busca un píxel encendido a ±1 px de la `y` que dice cada pasada; si
  la geometría y el lienzo no coincidieran, el check falla. Los 48 fotogramas que cambian en el baseline son
  `about`, `about_rec` y `splash` de las 16 paletas — el número de versión, no una curva.
- **Coste `[TARGET]`**: **580 596 B, 3 016 libres**; el estado de las pasadas son **144 B de `.bss`** y el
  refactor dejó la imagen 48 B por debajo de v2.3.1 (580 644 → 580 596).
- **Refactor**: `morph_y()` (geometría, sin color) separado de `morph_pass()` (color y trazo). Una sola
  implementación de la fórmula, usada por el panel y por el banco.

## v2.3.1 — la profundidad llega a MORPH y TILT

**Dos curvas más con el stack de ENV, ninguna copia de JIANT y ninguna decorativa.** Cada capa es un valor real
del mismo parámetro, no una repetición de la misma forma.

- **MORPH**: la onda que suena delante y detrás **las dos puntas del recorrido** (la onda A y la onda B), más
  pequeñas, levantadas y en el extremo frío de la paleta. Ahora el panel dice dónde está el sonido entre dos
  formas, no solo cómo suena. Con MORPH en 0 o con A = B se dibuja una sola pasada, idéntica a la de antes:
  PM, TUBE, FOLD y PWM/SYNC/RING/SAW3 no cambian un píxel.
- **TILT**: las **dos puntas del knob** (-64 y +63) detrás y el ajuste actual delante, con el mismo valor que
  corre el DSP. Al moverse el knob se ve hacia dónde va la curva, no solo dónde está.
- **Verificación `[HOST]` por diff de píxeles**, que es lo que faltaba la vez anterior: MORPH cambia **427**
  píxeles (y 100..150) y TILT **436** (y 110..150) respecto al mismo build sin las capas, exactamente en la
  zona de la curva. Barrido de UI: **0 hallazgos de lint**, **0** desvíos de alineación en 3 953 168 medidas,
  **0** tintas sobre su propio color, **0** píxeles GREY/MONO fuera de gris. Los 264 textos elididos son los
  tres textos libres (un mensaje, un nombre de canción), no etiquetas de página.
- **Coste `[TARGET]`**: **+464 B** (MORPH +304, TILT +160) → imagen **580 644 / 583 612 B, 2 968 libres**,
  por encima del suelo de 2 048. RAM sin cambios.
- **Sobre los bugs visuales listados**: parpadeos, controles que desaparecen y zonas desplazadas no se
  reproducen en el barrido, que dibuja cada pantalla una vez y mide alineación por tinta; los tres fallos que
  sí aparecían en pantalla (la cola del reverb re-cuantizada en v2.3.0, el visualizer sin cachés y la costura
  de sus dos pasadas) están corregidos o medidos en `docs/`. Un detector de parpadeo por fotograma es trabajo
  de la fase de rendimiento, no de esta.

## v2.3.0 — FX BUSES: la cola del reverb deja de saltar

**Incremental, de una sola cosa y medida.** No cambia ni una muestra de audio: los 311 renders dorados salen
idénticos y la suite de FX (`reverb_test`, `fx_meter_test`) pasa entera. Cambia un dibujo que mentía.

- **El fallo.** El bloque REVERB de FX > BUSES dibujaba la cola con **`4 + size·5/127` barras**: cinco anchos
  y cinco alturas para todo el recorrido de SIZE. Al girar el knob, el dibujo se re-cuantizaba de golpe —cuatro
  barras se volvían cinco, con todas sus anchuras y alturas cambiando a la vez— y eso es lo que se leía como
  la línea dando un salto al tocarla.
- **El arreglo.** Nueve barras fijas y **SIZE como pendiente**: `fall = 300 − size·250/127` por mil por barra,
  multiplicado barra a barra. SIZE 0 llega muerta al final del bloque (0,7⁹ ≈ 4 %), SIZE 127 sigue sonando
  (0,95⁹ ≈ 63 %): la cola se desliza de una barra a la siguiente en vez de multiplicarse, y el recorrido del
  knob se lee continuo.
- **Coste `[TARGET]`**: **0 B**. La imagen queda en **580 180 / 583 612 B** (3 432 libres): el compilador saca
  el mismo tamaño que la versión de cinco escalones.
- **Verificación `[HOST]`**: barrido de UI 196 pantallas × 22 paletas, **0 hallazgos de lint**, **0** desvíos de
  alineación en 3 953 168 medidas; el baseline de píxeles se regeneró para esa pantalla con este motivo.
  `reverb_test` y `fx_meter_test` enteros; lazo `[HOST]` 1 655 ns y mezcla 277,7 ns/muestra (sin cambio).
- **Auditoría de la fase**: el resto de lo pedido ya estaba — reverb con 5 modelos (ROOM SPRING SHIMR RESO
  CLOUD), delay ANALOG/DIGITAL con saturación, bus de chorus con CHORUS / PHASER / FLANGER, sends y retornos
  por bus con sus vúmetros (`fx_vu`), DIST con 6 modelos, y los 127 ids de parámetro **agotados**, así que un
  efecto nuevo tendría que entrar como valor dentro de un parámetro existente. Se detalla en
  `docs/FASE4_FX_BUSES_v2.3.0.md`.

## v2.2.0 — MOTOR DE PROFUNDIDAD QUANTUM: ENV, LFO Y CHORUS EN TRES DIMENSIONES

**La línea 2.2, primera versión.** No cambia una muestra de audio (`tests/regress` da los **311** renders
idénticos) ni un parámetro, una tecla o una ruta. Cambia la forma en que estas páginas dicen lo que ya sabían.

- **Etapa 1 — memoria, medida y no prometida.** Auditoría símbolo por símbolo del `.bss`/`.data` del build
  AC791N (los diez bloques mayores están todos vivos: `dly_buf`, `cv_buf`, `sl_buf`, `cloud_buf`, `proj_slot`,
  `trk`, `rev_comb`, `proj_scratch`, `sec_stage`, `undo_st`; ninguna tabla duplicada ni buffer sin uso, y nada
  de lo que el proyecto ya había rechazado con números se da por bueno). Sin issues ni PRs abiertos, sin trabajo
  sin commitear. La memoria se recuperó donde de verdad aprieta, **la imagen**: el dibujo por píxel de la
  envolvente (tres bucles más el relleno) se reemplazó por el motor compartido y la imagen bajó de
  **581 476 a 580 180 B** (3 432 B libres, suelo 2 048): **1 296 B de flash recuperados** antes de añadir nada.
  El costo en RAM es de **212 B** (`qz_v` 82 B + el contexto de la envolvente 100 B + 30 B de estado), con
  **5 456 B libres** sobre un suelo de 4 096: la imagen es el recurso escaso y es el que se liberó.
- **Etapa 2 — el motor: `qz_` en `ui_graph.c`.** Un stack de capas en pseudo-3D con geometría 2D: cada capa es
  la MISMA curva dibujada más chica (`sx`/`sy` por mil), corrida a la derecha y arriba (`dx`/`dy`) y mezclada
  hacia el extremo frío de la paleta; la de adelante va a 2 px con un piso de luz de dos filas y las de atrás a
  1 px. Una función de niveles por página (`qz_lvl_t`) llena **un solo buffer estático de 41 muestras** por capa:
  no hay asignaciones, no hay matrices y el costo de una capa es el de una polilínea. Nada del motor sabe qué es
  una envolvente, un LFO o un chorus.
- **Etapa 3 — dónde se aplicó, y dónde no.** **ENV** (prioridad): la envolvente de este track adelante y detrás
  las de los otros tres tracks, con sus valores reales — cuatro capas que se separan en perspectiva y cambian si
  se mueve cualquier knob de cualquier track. **LFO**: tres capas de la misma onda con un paso de fase entre
  ellas, así la modulación muestra también por dónde pasó. **CHORUS**: cada canal con dos copias detrás — el
  carril y su rastro. **DELAY** ya tenía profundidad real (el túnel de ecos concéntricos de v2.1.0) y **BUSES**
  la conserva en sus miniaturas; REVERB, SLICER, SEQ y MASTER quedaron como estaban a propósito: ahí una escalera,
  unas compuertas y un medidor se leen mejor planos.
- **Etapa 4 — identidad.** Fondo negro, rampa de color entre los tokens de la paleta (`ux_mix`) del ámbar del
  frente al frío del fondo, línea principal nítida de 2 px y secundarias progresivamente más tenues. GREY y MONO
  siguen neutros (0 píxeles fuera de gris en la barrida).
- **Etapa 5 — medido.** `[TARGET]` **580 180 / 583 612 B** y `.data+.bss` **92 848 / 98 304**; `[HOST]` el dibujo
  nuevo cuesta una polilínea de 41 puntos por capa y no toca el camino de audio. Lint de alineación **0** en
  3 953 168 mediciones; los píxeles que cambiaron están en el baseline con este motivo.
- **Etapa 6 — publicado.** Paquete, emulador, sitio, instalador y editor sincronizados con el mismo build.
  Sin unidad FM-1: ninguna compuerta de instalación cambia de estado.

## v2.1.0 — ENV, FX, BUSES Y SCL REDISEÑADOS: CADA PANTALLA TIENE SU PROPIO DIBUJO

**La línea 2.1, primera versión.** Cambia la composición y el dibujo de ENV, DLY, DLY 2, REVERB, REVERB 2,
CHORUS, BUSES y SCL; no cambia un parámetro, una tecla ni una muestra de audio (`tests/regress` da los **311**
renders idénticos). Los píxeles de la UI sí cambian a propósito: **166 de 3 129** frames, y el baseline se
regeneró con este motivo.

- **DELAY: el túnel de ecos.** Las seis barras sobre un eje se fueron: ahora la página dibuja un marco seco y,
  dentro, un rectángulo por repetición audible -- más pequeño, más frío y desplazado por SPREAD, PITCH y
  PING-PONG. El número de marcos no es una constante: `dly_echoes()` cuenta las repeticiones que el feedback
  deja por encima del 1 % del golpe seco, que es el número de ecos que el DSP produce. DLY 2 es el mismo túnel
  partido en dos, uno por canal, con PITCH inclinando cada lado y WIDTH separándolos.
- **REVERB: la cola como escalera.** La cola era una nube de columnas de 1 px; ahora son barras de 3 px con
  1 px de aire y la cabeza de cada barra más clara, así la pendiente, el largo con SIZE y el brillo con DAMP
  se leen de un vistazo. REVERB 2 es la misma escalera partida en dos (izquierda arriba, derecha abajo), con
  WIDE y FILT.
- **CHORUS: dos carriles.** Cuatro sinusoides finas se convirtieron en dos ondas grandes, L y R, con la
  amplitud de DEPTH, la separación y el desfase de STEREO y el movimiento sólo cuando el motor tiene señal.
- **BUSES: la consola.** Los seis bloques eran 2 × 3 de 116 × 34 con dibujos de 16 px; ahora son **3 × 2 de
  76 × 52**, con cabecera (icono, nombre, lámpara), el dibujo propio de cada efecto y su valor en el pie, y
  los medidores de envío/retorno a la derecha. DELAY dibuja el túnel, REVERB la escalera, CHORUS los dos
  carriles, SLICER las ocho compuertas con el paso actual, PUNCH la transitoria contra la caída que DUCK le
  hace y MASTER su medidor con la línea de pico y la lámpara de clip.
- **ENV: la forma, no la silueta.** La envolvente se dibuja rellena hasta el eje, con las cuatro esquinas
  marcadas sobre la curva (el pico del ataque, la llegada del decay, el inicio y el pie del release) y el
  playhead de la voz más fuerte. La curva sigue saliendo de `ENV_K()`, la misma exponencial que corre el DSP.
- **SCL: cada grado con su color.** El teclado marcaba la escala con un solo tono; ahora la tónica va en
  acento y los demás grados recorren la paleta entre tema y acento, con el mismo color en la cara de la tecla
  natural y en la franja del accidental, así la función de cada nota se lee en la tecla y no en una leyenda.
- **Y la imagen no creció.** `[TARGET]` **581 476 / 583 612** (2 136 B libres, suelo 2 048) y `.data+.bss`
  **92 636 / 98 304** (5 668 libres): 80 B menos de imagen y 16 B menos de RAM que v2.0.3, porque el rediseño
  borró lo que reemplazaba (el estado del tren de ecos, el brillo móvil de la consola, los anillos de aire de
  las dos páginas de reverb, el wobble por columna de REVERB 2). `[HOST]` el dibujo sube ~100 ns por frame
  (+6.2 % en `draw`, +12.3 % en el lazo) con el camino de audio intacto. Detalle y clases de cada medición en
  `docs/FASE2_IDENTIDAD_VISUAL_v2.1.0.md`. Sin unidad FM-1: ninguna compuerta de instalación cambia.

## v2.0.3 — CPU, DSP Y RAM MEDIDOS CON UN ARNÉS REPRODUCIBLE, Y EL MARGEN DE PILA POR PRIMERA VEZ

**La línea 2.0, parche 3.** No cambia una sola muestra de audio ni un pixel: `tests/regress` da los **311**
golden renders idénticos y `tests/ui_pixels` los **3 129** frames idénticos. Lo que cambia es lo que el
proyecto puede demostrar sobre sí mismo.

- **Arnes reproducible: `tools/dsp_bench.py`.** Un solo comando mide y compara, por clases: `[HOST]` CPU por
  muestra de las escenas de mezcla (`tests/regress`), ns por tick de la TIMER5 (`tests/isr_bench`), ns por
  frame del lazo principal y por firma de autoguardado (`tests/frame_bench`); `[TARGET]` instrucciones de
  los lazos de render y divides de `build/felucca.dis` (`tests/target_budget.py`), las cinco ventanas de
  memoria (`tools/mem_budget.py`) y el tamaño de la imagen; y el margen de pila. `--record` escribe
  `tests/bench_baseline.json`; `--compare` lo diffa con las tolerancias del propio proyecto (+25 % host,
  +3 % target) y **falla si un golden render cambia**, porque eso no es una optimización invisible.
- **Margen de pila medido: `tests/stack_watermark.c`.** El compilador del AC791N no acepta `-fstack-usage`,
  así que no había número de pila en ninguna parte. Ahora cada escena corre en un hilo cuyo stack es un
  buffer asignado y pintado con un patrón, y se lee la frontera que el C pisó: **mezcla a 6 pistas con todos
  los buses abiertos 1 783 B**, captura/empaquetado de proyecto < 256 B, y **40 frames de UI 4 815 B**
  (host x86-64, frames de este ABI, no de la unidad) contra las ventanas de 13.75 KiB / 7.75 KiB de
  `app.ld` y un presupuesto de 24 KiB: la cadena profunda es la de la UI, no la de audio, y no hay recursión
  en ninguna de las tres. Corre en la suite y falla si una cadena se hunde más.
- **Auditado y rechazado con números** (no con opinión): compartir `proj_scratch` con `sec_stage` sigue sin
  ser demostrable (un guardado durante la reproducción tendría que parar la cadena); `gen_keep` no es
  disjunto de la sesión de backup del editor; el tick de 10 kHz mide **25.7–35 ns** en `isr_bench` (0.03 %
  de sus 100 µs), así que su contabilidad de diagnóstico no justifica tocar la ISR más caliente; y la ruta
  de audio está a **320 ns/muestra** en host (10.2 µs por bloque de los 725 µs de presupuesto), con las
  rampas de DRUM-X aún rechazadas por el presupuesto de instrucciones del objetivo.

## v2.0.2 — EL LAZO PRINCIPAL DEJA DE HASHEAR DOS VECES, Y LA SUITE DEJA DE SALTARSE EL EMULADOR

**La línea 2.0, parche 2.** Reparación y optimización medidas sobre v2.0.1; ningún camino de audio, tabla,
parámetro, preset ni pantalla cambia de contenido. Clases de medida: `[HOST]` x86-64 nativo, `[TARGET]` build
AC791N real. No hubo unidad FM-1 física.

- **La firma del autoguardado se calculaba dos veces cada 250 ms.** `autosave_poll()` tenía dos lectores del
  mismo hash — el aviso de llenado (`proj_fill_poll`) y el autoguardado — y cada uno llamaba a `autosave_sig()`
  por su cuenta: los ~9 KB del proyecto (6 pistas × todos sus parámetros, pasos y parches FM6) se recorrían
  **dos veces por cuarto de segundo** para responder lo mismo. Ahora se calcula una vez por sondeo y ambos
  lectores la comparten; cada uno conserva su propio reloj, así que un sondeo que sólo le toca a uno cuesta un
  hash y uno que no le toca a ninguno no cuesta nada. `[HOST]`
- **La propia firma es un 66 % más barata.** Era FNV-1a byte a byte; ahora pliega cuatro bytes por multiplicación
  (el valor es de RAM: `as.saved`/`as.seen` se comparan dentro de un mismo encendido, así que el ancho del
  plegado es libre) y agrupa los parámetros de ocho en ocho antes de mezclarlos. Medido con `tests/frame_bench`
  (`cc -O2`, 1 200 llamadas, mínimo de seis series, mismo host): **`autosave_sig()` 10 904 → 3 661 ns (−66 %)**
  y, con la deduplicación de arriba, **una sola firma por sondeo en vez de dos (−50 % de llamadas)**.
- **La suite ya no se salta el emulador cuando Emscripten está instalado.** `tests/run_tests.sh` sólo comprobaba
  `command -v emcc`; en este proyecto `emcc` vive en `$HOME/emsdk` y se alcanza con `emsdk_env.sh`, así que la
  suite imprimía "skip the browser emulator" y la comprobación de paridad (`= native`, 88 200 frames bit a bit)
  **nunca corría como parte de la suite**. Ahora, si `emcc` no está en `PATH`, la suite busca el SDK donde el
  proyecto lo instala y lo carga: un grupo saltado es un hueco en la evidencia, no un resultado verde.
- **Coste en el slot:** imagen AC791N **581 556 B** (583 612), **2 056 B libres** sobre el suelo de 2 048; RAM
  `.data+.bss` 92 688/98 304, pool 8 384 de holgura, LOWBUF 31 648/32 768, NOINIT 124 libres. Todo `[TARGET]`.

## v2.0.1 — SCL: EL TECLADO QUE OCUPA SU SITIO, LAS NOTAS QUE SUENAN Y EL RATÓN QUE TOCA EL ACORDE

**La línea 2.0, parche 1.** v2.0.0 ya está publicado desde un commit anterior del historial, así que esta fase —que
sí es la renovación de SCL pedida— sale como 2.0.1 y no reutiliza ese tag. No cambia ningún camino de audio, ni una
tabla, ni un parámetro, ni el guardado: cambia lo que la pantalla de SCL muestra y lo que deja tocar.

- **El teclado ocupa el fondo de la página.** Era una tira de 26 px con 4 px de placa debajo y los nombres de las
  naturales centrados en la tecla entera, así que su última fila caía sobre el borde de la tecla. Ahora mide **38 px
  en SCALE (8,84)** y **36 en CHORD (12,86)**, y el nombre vive en la **banda blanca que queda bajo las alteradas**
  (con respaldo al centro solo si esa banda no da para 12 px). Ningún nombre sale de su tecla.
- **La fila que sobraba pasó a ser columna.** SCALE tenía tres renglones (nombre, DEG, IVL) con el tercero casi
  siempre vacío: ahora son dos, y `IVL` empieza donde termina el texto de `DEG`, de modo que doce pasos cromáticos
  caben en media línea cuando no hay nada seleccionado. El panel de CHORD subió su regla 4 px (`ay` 72 → 68) para
  que los nombres de intervalo queden libres sobre el teclado.
- **Una nota que suena no es una nota elegida.** La tecla que suena se enciende en la **cara** (mezcla al tema) y
  lleva una barra de acento de 2 px al pie; la elegida conserva **borde de acento** y su tira superior. Además, la
  máscara viva se compara con la del cuadro anterior: la nota que **empieza** toma un 24 % más de acento durante diez
  cuadros, así que un paso del secuenciador, una nota MIDI o el arpegiador se leen como evento. Sale de `voice_t`
  (las voces que el motor suena), de modo que no pulsa nada cuando no hay nada sonando.
- **La regla de CHORD es un teclado más.** Los chips del eje son las notas del acorde y ahora responden al toque: un
  tap elige o quita ese semitono con la misma geometría con la que se dibuja (`scl_ruler_geo()`), sin duplicar la
  función de los controles que ya están en las tarjetas.
- **Regresión de la fase anterior, corregida:** `DLY`, `DLY 2`, `DLY 3`, `REVERB`, `CHORUS` y `MASTER` estaban
  declaradas `GR_NONE` porque **ya tenían gráfico propio**: `ui_tech.c` dibuja `tech_dly`, `tech_rev`,
  `tech_chorus`, `tech_master` desde el id de la página, con animación real (los anillos de la reverb siguen
  `fx_vu[5]`). La fase 1.13.0 las envió a cuatro dibujos estáticos añadidos en `ui_graph.c` y tapó esos gráficos.
  Esas cuatro funciones se eliminaron, las seis páginas vuelven a `GR_NONE` y `tests/ui_test` ("REVERB graph: the
  rings follow the bus's return") —que fue el test que lo cazó— vuelve a pasar.
- **Mini optimización, medida:** `scl_key()`, el bisel privado de la capa SCL (29 líneas), era una segunda copia de
  `scl_pkey()`, el que dibujan las páginas de SCL; se eliminó, y con él los cuatro gráficos FX duplicados. Con el
  teclado más alto, la animación y los taps nuevos, la **imagen AC791N baja de 583 524 a 581 476 B (−2 048 B)**,
  con **2 136 B libres** en el slot de aplicación (antes 88, suelo 2 048): `.data+.bss` intacto en 92 688/98 304,
  pool con 8 384 B de holgura, LOWBUF 31 648/32 768. Las tres tablas de notas y el escaneo de voces duplicados en la
  capa SCL, el doble recorrido de voces de ENV y el destello de la tecla tocada (que repetía el borde de acento) se
  fueron en el mismo pase: 4 recortes medidos, ninguna función perdida.
- **Barrido de UI:** las 196 pantallas × 22 paletas con **0 hallazgos de lint, 0 desalineaciones**, 0 tintas sobre su
  propio color; no aparecieron superposiciones, textos cortados ni elementos fuera de su caja en las pantallas
  afectadas. `tests/scl_test` verifica la geometría del teclado, la de la regla y cuatro toques reales;
  `tests/ui_test` entero vuelve a pasar (18 000+ comprobaciones), incluida la de la reverb que delató la regresión.

**Nada de esto se probó en una FM-1 física.** Sin unidad en este entorno: no se tocó el teclado del instrumento, no se
miró el panel y no se conectó ningún controlador MIDI. El feedback de notas y el golpe de tecla son el renderer
leyendo `voice_t`, ejercitado por el barrido y los tests, no por un dedo. `INSTALL BLOCKED` sigue en pie.

## v1.13.0 — JIANT INSPIRED: LAS ETAPAS DE ENV Y LAS PÁGINAS DE FX QUE NO TENÍAN IMAGEN

**Una versión de presentación, no de DSP.** 1.11.2 → 1.13.0 (`docs/VERSIONING.md`). La línea 1.12.0..1.12.18 ya
está tomada en el historial del repositorio por otros paquetes, así que esta fase —una función nueva, no un parche—
sale como 1.13.0 y no reutiliza ninguno de esos tags. No cambia ningún camino de
audio, ni una tabla, ni un parámetro, ni el guardado: los mismos valores suenan igual. Lo que cambia es lo que la
pantalla dice de ellos.

- **ENV: las cuatro etapas, marcadas sobre la curva.** La página ya dibujaba la envolvente real (ataque lineal,
decaimiento y release recorridos con el mismo `ENV_K` que corre la voz) y desde v1.12 un punto que camina esa
misma recurrencia, así que el punto cae sobre la tinta. Faltaba nombrar los tramos: ahora hay una guía callada en
cada límite, `ATK` `DEC` `SUS` `REL` bajo su propio segmento (con el de la etapa viva en acento) y el punto de
posición donde ya estaba. Todo sale de `P_ATK/P_DEC/P_SUS/P_REL` y de `voice_t.stage/env`: ninguna etapa se
inventa. Vale para ENV y para OP1..OP4 ENV, que comparten el mismo trazado.
- **DLY / DLY 2 / DLY 3: las repeticiones, dibujadas.** Las tres páginas eran cuatro tarjetas sobre un panel vacío
(`GR_NONE`). Ahora cada eco es una barra: `TIME` fija su separación, `FEEDBK` su decaimiento (la cuenta se corta a
−26 dB), `TONE` su color y PING-PONG pone los impares debajo del eje, que es lo que hace el motor con los dos
canales.
- **REVERB: la cola con su amortiguación.** `SIZE` da la longitud, `DAMP` la curvatura real de la caída (no una
recta) y `PRE` el silencio antes de que la sala conteste; la cola son barras, no una cuña, para que el damping se
lea.
- **CHORUS / PHASER / FLANGER: el LFO del modelo.** Dos a cuatro taps según el tipo, `RATE` el período, `DEPTH` el
barrido y `STEREO` la fase entre taps, con interpolación lineal para que la onda no salga escalonada.
- **MASTER: la transferencia del limitador.** Diagonal de unidad, rodilla de `CLIP`, estante de `DUCK` y el pico
estéreo real (`audio_out_peak`) como punto; cuando llega a fondo el punto va en acento.
- **Lo que se reutilizó**: el mismo motor, los mismos parámetros que editan las tarjetas y los mismos `fx_vu` de
  `FX > BUSES`. **Lo que no se tocó**: SCL (sus tres páginas, el círculo, el teclado con grados y los voices ya
  existen desde 6.1), `SEQ > TOOLS` (6.2) y las páginas que sí tenían gráfico. **Cero estado nuevo**: los cuatro
gráficos son funciones de `song.g[]`, `t->p[]`, `voice_t` y `fx_vu[]`; la firma de la página ya los redibuja.
- **El coste, y lo que hubo que quitar para pagarlo.** La imagen AC791N queda en **583 524 B de 583 612** —88 B de
  margen, el más ajustado del árbol— con `.data+.bss` en **92 688 B de 98 304** (sin cambios) y el pool con 8 384 B
  de holgura. El primer enlace se pasó 584 B de XIP: se quitaron los cuatro rótulos que repetían los títulos de las
  tarjetas, la guía punteada de ENV pasó a una línea y la onda del chorus a interpolación. El barrido
  `tests/ui_render` da **196 pantallas × 22 paletas con 0 hallazgos de lint y 0 desalineaciones**.

**Nada de esto se probó en una FM-1 física.** Sin unidad en este entorno: ni transferencia, ni arranque, ni
lectura de flash. `INSTALL BLOCKED` sigue en pie. El emulador corre el mismo C; se reconstruyó para esta versión y
la paridad bit a bit la comprueba la suite, no el oído.

## v1.11.2 — FASE 1/2: LA VENTANA DE RAM QUE LIMITA TODO, CON HOLGURA

**Un parche de memoria, no de sonido.** 1.11.1 → 1.11.2, sin renumerar (`docs/VERSIONING.md`). No cambia
ningún camino de audio, ni una tabla, ni el layout de la UI, ni una compuerta de instalación: los 311
golden renders, el emulador bit a bit contra el build nativo y los 3 129 frames de píxeles quedan
idénticos.

- **La ventana que se quedaba sin aire.** `.data+.bss` tenía **4 160 B libres contra un suelo de 4 096**
  —64 B de margen real— mientras LOWBUF guardaba 1 720 B ociosos por encima de su suelo y POOL 9 248 B
  por encima de los 8 KiB que exige `AGENTS.md`. Tres buffers del editor, de lazo principal y reescritos
  desde su primer byte en cada uso (`ed_out`, `ed_bk_settings`, `ed_bk_buf`), se movieron a esas dos
  ventanas: **92 688 B usados y 5 616 B libres** (+1 456 B, +35 %), margen sobre el suelo de 64 B a
  1 520 B. LOWBUF queda con 1 120 B (suelo 1 024) y POOL con 8 384 B de holgura (suelo 8 KiB).
- **Lo que se auditó y se rechazó, con números.** 274 B de padding en `.bss` (777 símbolos), 128 B de
  constantes idénticas en XIP, este linker es **GNU ld 2.26 sin ICF** y el firmware ya es una sola TU;
  `step_t` mide 15 B exactos, `WT_GEN` ya guarda media onda (6 192 B), `SINE` y `fm6_exp2_tab` ya se
  decodifican desde deltas, y `midi_sel_on` necesita exactamente 4 bits por nota. Nada de eso se tocó
  porque nada de eso se podía demostrar más pequeño sin cambiar el sonido o la UI.
- **Los artefactos.** `felucca-v1.11.2.fwsc` **610 933 B**, sha256
  `7b04f44dd18db254f28d35bd826d34b6c844441e3191d4e8d9288dbdd5ec76f0`; `qg1-emulator-v1.11.2.zip`
  **521 813 B**, sha256 `bbfc322128cdf846ef70d044465f3086cd8f268dfc4f9248f2d3fa5673910dd6`. Imagen
  **581 508 B** con 2 104 B libres; XIP 446 556 B de código + 130 152 B de constantes, sin cambios.
  Construido desde `3820dd8` con el toolchain JieLi `20260730.1` y el SDK `AC79NN_SDK_V1.2.1_2023-12-13`.

**Nada de esto se probó en una FM-1 física.** No hay unidad en este entorno: ni transferencia, ni arranque,
ni lectura de flash, ni recuperación. `INSTALL BLOCKED` sigue en pie.

## v1.11.1 — FASE 9: EL CIERRE INTEGRAL, LA SEGURIDAD AUDITADA Y EL RELEASE REAL

**Un parche sobre la línea 1.11, no un redesign.** 1.11.0 → 1.11.1, sin renumerar y sin saltar números
(`docs/VERSIONING.md`). No cambia el camino de audio, la tabla del panel, el layout de la UI ni una sola
compuerta de instalación.

- **El árbol vuelve a decir su propia versión.** El README anunciaba `v1.8.6` y el instalador citaba
  `v3.1.1` y `v2.9.7`, paquetes de una numeración que esta línea no construye; ahora el número sale de
  `FELUCCA_VERSION` y ningún texto público inventa otro. `docs/VALIDATION_MATRIX.md` dejó de declarar
  "current release v1.6.0".
- **Se fue lo que ya no era el árbol.** Un `gmon.out` de 427 KiB en la raíz y un
  `web/emu/manifest.json` commiteado que era el registro de build de **v2.7.3** (1 112 286 B, otro commit).
  El registro real del emulador es `release/emu/manifest.json` y `site/emulator/manifest.json`, generados
  desde el build; `coherence_check` compara el suyo con el build record.
- **Un nombre que no era el de la parte.** `tools/fm1_probe.py` llamaba `P25Q80H-2` a la UCUN; la
  allowlist dice `UC25HQ80` (JEDEC `0xB36014`). Un nombre inventado en una herramienta de flash es
  exactamente el tipo de detalle que un informe de instalación no debe arrastrar.
- **Los golden renders del número.** Splash y about dibujan la versión: 48 frames de 3 129 cambiaron y se
  regeneraron con `tests/ui_pixels.py --update`. Verificado después: 3 129 idénticos.

- **El emulador del sitio era de otro árbol.** Al verificar la publicación real, la página raíz decía
  v1.11.1 pero servía el WASM de **v1.12.18** (1 299 656 B, otro commit): el paso de CI que elige "el release
  más nuevo" ordenaba por número de versión, y 1.12.18 —publicada antes, en la numeración vieja— ganaba a
  1.11.1. Ahora ordena por `created`, la misma clave que `make_site.py`, así que la selección y la página no
  pueden discrepar; y `make_site.py` **se niega a publicar** un release cuyo emulador nombre otra versión,
  de modo que un fallo así detiene el deploy en vez de salir al aire. Comprobado con un manifiesto falso
  (v1.12.18): exit 1, sitio no escrito.

**La auditoría de seguridad, sin relajar nada.** Se revisaron y volvieron a probar: SHA-256 + tamaño del
paquete contra su manifiesto antes de habilitar INSTALL; la identificación real del chip con `0x12` en la
misma sesión; la allowlist de dos partes de 1 MiB (`0x856014`, `0xB36014`) y el rechazo de las cinco partes
conocidas pero no validadas para escritura; la ventana del loader `0x4000–0x92FFF` y el marcador
`FELUCCA-LOADER-1`; el V16 `FM-1_016` como destino de restauración y nunca como contenedor (su área de app
termina en `0xA8071`); el "Verify only" que solo manda un frame de identidad; backup y restauración por
objeto; y el rechazo de paquetes truncados, sin fin de lista, con otra clave de chip o con la app fuera de
la imagen. No se añadió ningún bypass, no se eliminó ningún mecanismo de recuperación y el bootloader se
sigue sin escribir.

**Lo medido.** Imagen **581 508 B** y **2 104 B libres** (suelo 2 048); RAM 94 144 de 98 304 (**4 160 B
libres**, suelo 4 096); pool 311 256 B con 9 248 de holgura; XIP 446 556 B de código + 130 152 B de
constantes; loader 7 777 B. Es **idéntico a v1.11.0**: el parche no costó un byte. El emulador se
reconstruyó con emcc 6.0.11 (**1 316 630 B**, sha256 `e417d568…`) y `target_budget` sigue en verde, con
`BLOCK_PATH` al **−27 %** de su presupuesto.

**Los artefactos.** `felucca-v1.11.1.fwsc` **610 933 B**, sha256
`cae3e5a43b3904722e7f3a46cea88fd2627d8f1aa30dd14b4ca6990a47a231e9`; `qg1-emulator-v1.11.1.zip`
**521 811 B**, sha256 `0c0588871b98fec1982abbcb2d5f4cbd08e543665490e6e4b09f2486859e1752`; el manifiesto, el
build record y `SHA256SUMS` van al lado. Construido desde el árbol de este release —el commit exacto y los hashes de app, loader y paquete están en
`release/build-manifest-v1.11.1.json`— con el toolchain JieLi `20260730.1` y el SDK
`AC79NN_SDK_V1.2.1_2023-12-13` (los tres archivos del SDK verificados contra su pin).

**Las pruebas.** La suite completa (`tests/run_tests.sh`, con el paquete real) terminó en **ALL HOST TESTS
PASSED: 6 514 comprobaciones `ok`, 0 fallos**, con ASan/UBSan, los 3 129 golden frames y el lint de alineación.
La suite salta el emulador porque busca `emcc` en el PATH; se corrió aparte con emsdk 6.0.11 y dio
`emu: all ok`, con la canción pesada **bit a bit** contra el build nativo (88 200 frames) y 24,9 ms por
segundo de audio. Aparte: instalador CLI, preflight, chip, firma, restauración, límites y fallos de paquete,
backup web, coherencia, release set y sitio, todos en verde.

**Nada de esto se probó en una FM-1 física.** Ni transferencia, ni arranque, ni lectura de flash, ni
recuperación. El informe completo, con la clase de cada medición y lo que queda pendiente, es
`docs/FASE9_CIERRE_2026-11-24.md`.

## v1.11.0 — FASE 8: LA PANTALLA EN PALABRAS, COMPLETA; Y UNA AUDITORÍA CON PRUEBAS

**El lector de pantalla (issue #105) cubre lo que le faltaba.** Hasta la 1.10 solo se decía la pantalla, la fila
de MENU y el valor de las tarjetas de HOME. Ahora:

- **Los valores de cualquier página**, no solo HOME: la línea que ya existía se pide con `page_desc()` (lo mismo
  que dibujan las tarjetas y leen los encoders), así que EDIT, SEQ, FX, SCL y el resto dicen `CUTOFF 96` igual
  que HOME decía `LEVEL 96`. Se dice cuando el giro lleva ~120 ms quieto, nunca un mensaje por paso.
- **La fila de las listas, con su lugar**: `PATTERN 5 OF 40` en SEQ > PHRASES y `STEP 7 OF 16` en el cursor del
  secuenciador, dichas al moverse y otra vez cuando el lector pregunta WHERE. El nombre del patrón se sigue
  diciendo entero al cargarlo (`LOADED 03 NIGHT BASS`); repetirlo en la fila costaba 96 B de un slot que tenía 40.
- **WHERE vuelve a decir la página y la fila** en cualquier pantalla, no solo en HOME y MENU.
- **Las pronunciaciones, publicadas**: `firmware/words.tsv` (60 abreviaturas que la pantalla muestra de verdad,
  comprobadas contra `params.c`), `tools/gen_words.py` las convierte en el JSON que pide el protocolo, lo nombra
  con la huella de su propio contenido y lo publica en el sitio (`site/sr/w/2ab0d08f.json`). El HELLO del
  instrumento lleva ese enlace exacto (`ANN_WORDS_URL`), y `gen_words.py --check` falla si los dos dejan de
  coincidir o si el nombre + el enlace no caben en un frame.
- **El emulador habla, por fin.** La página del emulador compilaba con `FELUCCA_ANNOUNCE` en 0 (su unidad no
  incluía `announce.c`), así que el dispositivo del navegador nunca decía nada. Ahora hay un panel "Screen
  reader" con su casilla, un botón "Read the whole screen" y dos regiones `aria-live` (una educada y una
  asertiva para los avisos), y los frames van y vuelven por la misma puerta que en el cable: `web_sysex_byte`
  para el HELLO/WHERE del lector y `USB_WIRE_HOOK` en `usb.c` para tomar lo que el aparato contesta.
  `emu_test.mjs` lo mide: silencio antes de HELLO, la respuesta con nombre y enlace, `HOME MAIN`, y WHERE.

**Auditoría y estabilidad.** Se re-corrieron las pruebas de las fases anteriores que cubren lo que se pidió
revisar (voice stealing y polifonía, la nota diferida entre núcleos, las colas MIDI y el soak), y la suite
completa quedó en verde. Los dos defectos que aparecieron estaban en el camino del lector y son los dos
arreglados arriba: el emulador sin `announce.c`, y un HELLO que podía cortar el enlace de palabras a media URL.

**Lo que se midió y lo que no cupo.** Imagen **581 508 B**, **2 104 B libres** (suelo 2 048), RAM 4 196 libres.
Entraron el aviso de valores, la fila con su lugar, WHERE y el enlace de palabras; **no** entró el aviso
`QUEUE FULL` en la barra cuando se llena la cola MIDI/SysEx (~60 B, medido, y el slot quedó en 56): queda como
pendiente documentado, con los contadores que ya existen (`midi_in_overflow`) y la fila CPU LOAD de
GLO > MONITOR como diagnóstico de carga. Nada de esto se probó en la FM-1 física.

## v1.10.0 — FASE 7: QUANTUM TUNING, y los mini knobs un pixel más grandes

**Lo que no existía y ahora sí.** La FM-1 no tenía temperamentos: `cents` existía como *detune* de un oscilador
(`cents_inc`) y la escala/chord eran conjuntos de notas, no frecuencias. Ahora hay doce cents por perfil, cinco
temperamentos de fábrica y tres ranuras de usuario.

- **SCL > TUNE**: una página nueva en la familia SCL. El círculo de las doce notas con la desviación de cada una
  dibujada como un radio (hacia fuera, sostenida; hacia dentro, bemol), y a la derecha CENTS del nota que se
  edita, su barra alrededor del cero, NOTE, REF y el estado de la ranura (FACTORY / STORED / EDITED).
- **Cuatro knobs**: PROFILE (EQUAL JUST PYTHAG MEAN WERCK USER 1..3), NOTE, CENTS (±99) y REF (el perfil entero,
  A 440 → A 432 es -32). Tocar una nota del círculo la elige; tocar el centro vuelve a EQUAL, que también está a
  un click en PROFILE.
- **Guardar**: USER 1..3 guardan lo que se edita en ellas y se escriben en el objeto de settings dos segundos
  después del último giro (un write por gesto, no uno por detent). Un slot de fábrica no se sobrescribe: se
  edita, suena, y queda marcado `* EDITED`.
- **Dónde se aplica**: en `tune_inc()`, donde el pitch en 1/16 de semitono se vuelve incremento de fase, y dentro
  de `cents_inc()` para los motores que ya detunean en cents. La nota sale del propio pitch (`(p16 >> 4) % 12`), así
  que una nota transportada, una octava arriba o el segundo oscilador desafinado leen su propia clase. Con todo a
  cero el incremento es `pitch_inc()` bit a bit: el temperamento estándar no cuesta nada.
- **Precisión medida** (`tests/tune_test.c`, host): el peor error en todo el recorrido del knob es **0,432 cents**
  (la rejilla de 1/16 de semitono es 6,25: sin el segundo término de la expansión el error a 99 cents era 2,8). El
  pitch bend se suma encima y no se come la desviación (50 + 15 medidos). Un registro de settings PER4 importa
  como EQUAL; un byte corrupto se recorta al extremo del knob; un registro con otro magic se rechaza.
- **Mini knobs**: los cuatro del módulo WAVE de las pantallas de edición pasan de r=8 a r=9 (26 px de paso: 8 px
  de aire entre aros, 13 px hasta los bordes del módulo). Nada se mueve ni se toca.

**Lo que se pagó y con qué se pagó.** La página y el modelo costaban 2 032 B de XIP y el suelo ya estaba 488 B por
debajo. El espacio vino de `noinline` **en la fuente** de los dibujantes de página (117 funciones `graph_*`/`gw_*`/
`draw_*` de los `ui_*.c`): `draw_graph` los absorbía todos en sí mismo (51 228 B) y ahora los llama. Es lo que el
viaje por IR no conseguía aplicar. Resultado: **imagen 581 156 B, 2 456 B libres** (suelo 2 048) y **RAM 4 160 B
libres** (suelo 4 096), con `target_budget` en 0 líneas fuera (DRUM_PATH 1 155 de 1 153, dentro del 10 %).

## v1.9.4 — 1 284 B de vuelta en el slot de aplicación

El suelo de XIP llevaba rojo desde v1.9.0. Esta pasada devuelve **1 284 B** sin quitar ninguna función y sin
tocar un píxel ni una muestra: la imagen pasa de **583 336 a 582 052 B** y el espacio libre de **276 a 1 560 B**
(faltan 488 B para el suelo de 2 048).

**Lo que se encontró midiendo, no leyendo.**

| palanca | resultado |
|---|---|
| compilar el único TU directo a `-Oz` en vez del viaje por IR (`size_fns.py`, `minsize`+`noinline`) | **−1 180 B**: el viaje por IR compila código sin optimizar primero y sale 1 180 B más grande (583 232 frente a 582 052 en el mismo árbol). Los presupuestos de audio siguen dentro: `tests/target_budget.py`, 0 líneas fuera |
| `-fdata-sections` (con `--gc-sections` ya activo) | −8 B y ningún símbolo muerto que recoger |
| `-fno-unwind-tables -fno-asynchronous-unwind-tables` | 0 B (ya era el comportamiento de `-Oz`) |
| `gw_ring_tab` y `gw_ring_clip`: el mismo bucle de 24 segmentos dos veces, con `cv_line` y `sys_line` | **−96 B**: un solo bucle `gw_ring`, `noinline`, con la primitiva de línea como parámetro; los píxeles son los mismos |

**Lo que se midió y NO se adoptó**, porque el audio lo paga: `-fno-inline` global da −9 072 B pero deja
`DRUM_PATH` a +72 % y `analog_render` con 20 divisiones en bucle; `-mllvm -inline-threshold=2/3` da −5 008 B y
rompe `dx_run` (+247 %) y `lofi_render` (+109 %); `minsize` sobre los motores cuesta +112 B. La única forma de
recuperar esos 9 KB sin tocar el audio es anotar `noinline` en el código frío *a nivel de fuente* (el viaje por IR
no lo consigue: con las 1 350 anotaciones puestas el enlace sale igual de grande), y eso es una obra de
herramientas de compilación, no de esta pasada.

**Estado de los gates.** `target_budget`: 0 líneas fuera (DRUM_PATH 1 153 de 1 153). `ui_test`: todo en verde.
`resource_map`: RAM 4 336 libres, POOL 9 248, LOWBUF 1 720, NOINIT 124. El suelo de XIP sigue rojo por **488 B**,
con la aritmética delante: el slot no puede crecer (`APP_SLOT` ya absorbe los 2 048 B de holgura de la región y
quedan 725 B hasta `0x8F000`), y no queda código ni constante muerta que recoger.

## v1.9.3 — el A/B manual de HOME, retirado

El "EDIT mantenido para aparcar, EDIT tocado para intercambiar" que HOME prometía no era un camino: era un
bloque que se quedó sin puerta. EDIT es una capa **SET** (`ui_layer.c`): su mapa se abre con la tecla, y el
bloque de HOME, que se saltaba con `ui.layer`, no volvía a mirar cuando el mapa ya estaba arriba.

**Medido antes de decidir** (`tests/ui_test.c`, `test_home_edit_is_the_layer`, sobre el código que había): con
EDIT mantenido en HOME el bloque **sí** llegaba a correr un frame, y hacía las dos cosas a la vez — aparcaba
(`SOUND A STORED`), intercambiaba (`SOUND B`) **y** el mapa de EDIT subía igualmente. Un toque, en cambio, se lo
quedaba la capa y acababa en la página de EDIT. Es decir: ni era alcanzable como se describía, ni era inocuo lo
que sí corría.

**Decisión: retirarlo.** Hacerlo alcanzable otra vez significa quitarle el mapa de EDIT a HOME, que es la página
cuyos cuatro mandos son los del motor, para recuperar un aparcado manual que el A/B que existe ya no necesita:
**PRESET COMPARE** (capa de EDIT, tecla **A3**) tiene los dos lados siempre puestos — uno en la pista y otro en el
slot, el preset armado en cada carga, guardado, recall y UNDO — así que "probar presets y volver" ya es una tecla
cada vez, sin preparar nada y sin poder perder nada.

**Lo que se fue**: el bloque de HOME en `ui_input.c`, `ab_store`, `ab_side` y el byte `used` del slot; el
comentario de `ui.c` dice ahora qué queda y por qué. **Lo que queda**: `ab_swap`, que es el primitivo que usa
`pc_compare`, y la tecla A3. La imagen baja 176 B (583 512 → 583 336).

**Pruebas** (`tests/ui_test.c`, tres checks nuevos): EDIT mantenido en HOME abre el mapa y **nunca** dice
`SOUND A STORED` / `SOUND A` / `SOUND B` / `NOTHING TO COMPARE`, con el sonido intacto; la tecla A3 sigue
intercambiando preset y edición, una vez cada una; y un toque de EDIT en HOME abre la página de EDIT sin tocar el
sonido. Los tres fallan contra el código retirado (el primero y el tercero, medidos).

## v1.9.2 — el MIDI Learn que sobrevive al apagado

Fase 6/9: el MIDI Learn, auditado antes de tocarlo y completado con lo que el slot de aplicación permite pagar.

**Lo que había, revisado antes de empezar.** `midi_control.c` (QG-1 1.7): ocho enlaces `{cc, param}` en RAM, CC 119
arma un parámetro del motor de la pista del canal y el siguiente CC se enlaza, CC 118 lo olvida todo. Funcionaba,
pero **se perdía en cada apagado**, no se veía en ningún sitio, un CC que ya era de otro parámetro se lo quitaba en
silencio, no llegaba a las macros y no tenía perfiles ni forma de deshacer.

**Lo que se construyó.** El mapa es ahora **ocho enlaces de dos bytes en `favorites.factory[15][0..15]`**, bytes que
ningún motor usa y que valen 0 en todos los registros de ajustes anteriores: un registro viejo simplemente no tiene
mapa, así que **no hay formato nuevo ni migración** y no se toca ningún preset ni dato de fábrica. El byte 0 guarda
`CC + 1` (0 = libre) y el 1 el id del parámetro (o `G_MAC1 + m` para una macro). Al leerlo, un id que ese motor no
tiene **se ignora**: un byte corrupto no puede llegar a una voz. Cada cambio escribe los ajustes por la vía de
siempre (`settings_save`), que difiere la escritura hasta que el transporte está parado.

**Interfaz.** CC 119 arma un parámetro, **CC 111 una macro** (0..3 = M1..M4) — el CC enlazado mueve la macro como su
página —, CC 118 borra el mapa. El pie dice qué ha pasado, con el CC: `CC20 BOUND`, `CC20 TAKEN` (el CC era de otro
parámetro y se lo ha llevado), `CC20 ALSO STD` (pisa un CC del mapa estándar), `MAP NO ROOM`, `MAP CLEARED`.

**Lo que se midió y no cupo.** La fase completa —un perfil guardado aparte (CC116/115), deshacer/restaurar (CC117),
volcado por consola y una página propia del mapa — costaba **~2,2 KB** de XIP y el slot tenía **444 B** libres: la
imagen no enlazaba (se quedaba 1 812 B corta). Se probaron y se descartaron, con medida: `-fdata-sections` (+4 B),
mover dibujantes fríos a `.ram_text` (el `AT > XIP` de `app.ld` deja la copia en el mismo sitio: 0 B), quitar el
kerning (prohibido por `tests/text_spacing_test.py`, que exige ≤ 0,25 px) y `FELUCCA_SIZE=0` (864 B libres, pero
todo el DSP a `-Oz`: no paga). El mapa de flash tiene además solo 725 B hasta el final de su región (`0x8F000`).

**Pruebas (host x86-64, `tests/midi_control_test.c`, seis checks nuevos).** Dos CCs enlazados a ATK y PAN que mueven
cada uno lo suyo; un CC que era de otro parámetro se lo lleva una sola vez y el pie dice TAKEN; el mapa viaja en el
registro de ajustes y vuelve igual tras exportar e importar (un apagado); un enlace que nombra un parámetro que el
motor no tiene no se aplica; una macro (CC111 → M1) se enlaza y se mueve 0..127; y dos canales con el mismo CC en un
mismo bloque mueven cada uno el parámetro de su pista.

## v1.9.1 — la perilla que se mueve, grabada sobre la automatización que ya había

Fase 5/9: el Motion Recorder, auditado y completado donde cabía, sin un segundo sistema de automatización.

**Lo que había, revisado antes de empezar.** `motion.c` (64 registros por proyecto, `motion_store_t`: cabecera
`count`/`on`/`rsv[2]` y 260 B en total), dos tipos de registro (AUTO, que se mantiene, y LOCK, de un solo paso),
captura ya cuantizada al paso (`motion_capture`), reproducción en `motion_step`, la lista AUTO LIST (`ui_events.c`)
para editar y borrar, y `motion_sound_loaded` para no arrastrar curvas al cambiar de motor. Nada de eso se duplica:
esta fase escribe sobre ello.

**La rejilla.** Una toma ya caía en el paso en que estaba el transporte; ahora cae en el paso de su **rejilla**
(1, 2 o 4 pasos, por pista): `motion_res`, `motion_set_res`, `motion_res_steps`, y el redondeo dentro de
`motion_capture`. Se guarda en los **dos bits ya reservados** de la cabecera (`motion.rsv[0]`, dos bits por pista):
cero es la rejilla de un paso, así que **todos los proyectos antiguos se leen igual, sin migración y sin formato
nuevo**, y `project.c` ya guardaba esos cuatro bytes.

**El movimiento, dibujado.** La página AUTOMATION gana un gráfico (`ui_graph.c` → `graph_motion`): el nombre del
parámetro, `GRID n`, y la curva que el recorder tiene para él, sostenida entre registros sobre el rango real del
parámetro. KNOB 2 gira la rejilla, KNOB 3 elige el parámetro que se dibuja (los que la pista tiene grabados; con
ninguno, la página toma el primero que encuentre, para que el movimiento se vea sin tocar nada), KNOB 4 sigue siendo
CLEAR con su confirmación de siempre — los checks que ya existían siguen pasando sin cambios.

**El overdub.** Una toma **se suma** a la curva que encuentra: los registros de otros pasos, los LOCK (que son del
paso, no de la toma) y los de otros parámetros quedan intactos; el mismo paso se reemplaza a sí mismo. Es lo que el
recorder hacía, ahora escrito y probado.

**Lo que se midió y no cupo.** Un modo de toma REPLACE (borrar la curva del parámetro una vez por PLAY, con un
bitmap de 6 × 16 B) se llegó a implementar y a probar: costaba ~400 B de código y dejaba el slot de aplicación en
**28 B libres**. Se retiró: el byte reservado `motion.rsv[1]` sigue libre para cuando la fase de tamaño devuelva
espacio, y el comportamiento es el de siempre (overdub). Queda anotado, no escondido.

**Pruebas (host x86-64, `tests/ui_test`, cuatro checks nuevos).** La rejilla (un movimiento en el paso 5 con rejilla
2 cae en el 4, con rejilla 4 en el 8; el parámetro y el paso no se pierden), el overdub (cinco registros antes y
después de una toma: nada se va, el mismo paso se reemplaza), la página (KNOB 2 recorre 1 → 2 → 4 y vuelve, KNOB 3
elige el parámetro grabado, un frame con el gráfico no falla, KNOB 4 deja CLEAR en pie), y el caso extenso: 200
movimientos en una pasada dejan los registros dentro de los 64 que caben y suenan con lo que se guardó.

## v1.9.0 — el preset guardado contra la edición, una tecla cada vez

Fase 4/9: comparación real en el instrumento, sin duplicar el A/B que ya existía ni un solo preset.

**Lo que había, revisado antes de empezar.** En Web Studio no hay ningún A/B de presets: los `.js` de `web/` solo
comparan paquetes y huellas (`web/*.js`), así que no había nada que no duplicar allí. En el instrumento sí había un
A/B manual: en HOME, EDIT mantenido aparca el sonido (`ab_store`), EDIT tocado lo intercambia (`ab_swap`), con
`ab_snd`/`ab_side` en `ui.c` y el intercambio seguro a través de `motion_guard`/`motion_unguard`/`motion_rebase`.
Ese A/B **se reutiliza tal cual**: lo nuevo es la referencia automática al preset, no un segundo mecanismo.

**Hallazgo al probarlo.** El toque de EDIT en HOME que el A/B manual esperaba no llega a su bloque: la maquinaria
de capas se queda el toque (el `ui_test` lo enseña: tras el toque el instrumento está en la página de EDIT y el pie
dice `HOLD [EDIT] QUICK`, no `SOUND A`). El camino llevaba tiempo inalcanzable desde el panel. Por eso el control
nuevo vive donde el toque sí llega: **en la capa de EDIT**, sin tocar el bloque de HOME (quitarlo de ahí tampoco
era el encargo, y el A/B manual sigue donde estaba).

**El control.** EDIT mantenido abre su capa; la tercera tecla blanca, **A3** (F3 INIT, G3 RECALL, A3 A/B), compara.
Su celda dice `A/B`, y `PRESET` mientras suena el lado del preset. Una pulsación pone el preset en la pista y la
edición en el slot, la siguiente devuelve la edición: las dos partes existen siempre (una en la pista, otra en el
slot), así que pulsar de más no puede perder nada y el ISR nunca ve medio sonido (el intercambio es `ab_swap`).

**La referencia.** `snd_clean()` (core.h) la toma sola cada vez que un sonido pasa a ser el de la pista: cargar un
preset de fábrica, guardar uno de usuario, recordar un proyecto o una sección, un UNDO; y en el arranque, la ruta
`snd_first` de `ui_notices`. Es un único slot de RAM (`pc_orig`, ~600 B, en `.lowbuf` desde esta versión: ningún
ISR lo lee). **No se duplica ningún preset en flash y no cambia ningún formato**: es el mismo `track_t` que ya
había, en RAM, y solo mientras el instrumento está encendido.

**Qué viaja con el sonido.** Los valores, la matriz de modulación, el motor (`eng_req`), el número de preset y —en
FM6— el parche (`fm6_patch`/`fm6_set_patch`/`fm6_adopt`, que el A/B manual tampoco copiaba antes): un sonido FM6 es
su parche tanto como sus valores, así que comparar sin él habría comparado dos veces el mismo motor.

**Indicadores.** El pie y HOME siguen la regla que ya existía: `nombre A` cuando suena el preset tal cual se cargó,
`nombre*` (la estrella de SOUND EDITED) cuando suena la edición. Si se edita mientras suena el lado del preset, ese
lado pasa a ser una variante y el indicador deja de decir `A` — sin mentir sobre lo que se oye.

**Pruebas (host x86-64, `tests/ui_test`, cinco checks nuevos).** Valores (una tecla pone el preset y la siguiente
devuelve la edición, valor por valor, comparando la tabla entera); motor (ANALOG ↔ LOFI en los dos sentidos);
FM6 (parche y valores cambian de lado juntos); una pista sin referencia propia no se compara por error; y la tecla
de verdad (EDIT mantenido, A3: la capa, el mensaje `PRESET (A)`, y la edición de vuelta intacta). Suite completa:
todo en verde.

**Píxeles.** 128 fotogramas cambian: 48 por el texto de versión (about / about_rec / splash en cada estilo y
paleta) y 80 por la celda nueva de la capa de EDIT (cada paleta y estilo). 0 hallazgos de lint, 0 alineaciones.

**Presupuesto (medido en el enlace, `tools/resource_map.py`).** Imagen **582 300 B de 583 612** (1 312 B libres);
RAM 93 976 de 98 304 (4 328 libres); pool 9 248 libres; LOWBUF 31 052 de 32 768 (1 716 libres). La RAM vuelve a
estar por encima de su suelo (4 096) gracias a mover `pc_orig` a `.lowbuf`; **el suelo de XIP (2 048 B libres) no
se cumple: faltan 736 B**. La fase anterior dejó 2 100 B y esta función cuesta ~790 B de imagen (728 B de código
medidos símbolo a símbolo más 60 B de constantes); con `-Oz` + LTO ya en uso no queda margen de banderas. Queda
dicho, no escondido, y es la deuda a pagar en la próxima fase de tamaño.

**Límite.** Todo lo anterior es host x86-64 y emulador. No hay ninguna medida hecha en la FM-1 física: escribir en
la unidad está prohibido en este repositorio.

## v1.8.9 — los buses se duermen cuando ya están mudos

Fase 3/9: DSP y audio. El perfil (gprof sobre `tests/fx_bench`, host x86-64) ponía `fx_buses` al 24 % del bloque y
`rev_room` solo al 22 %: los tres buses —coro, delay, reverb— trabajaban cada bloque aunque no hubiera nada que
enviarles, con su entrada exactamente a cero.

**Lo que ya estaba bien y no se tocó.** Las pistas sin voz no cuestan nada (`mix_part` sale antes del mezclador),
una pista seca no paga los envíos (`if (c | d | r)`), el reparto 0/1 va por el coste medido de cada parte
(`mc_tk`, `mc_fx_tk`) y `beat_samples` ya cacheaba su división. El escaneo de envíos por bloque ya decía cuándo no
se envía nada (`any`, los tres picos): era el único dato que faltaba para no procesar.

**El cambio.** `fx_gate` ya sabe cuándo ha dejado los buses a cero y cuánto lleva sin un solo envío; de ahí sale
`fxg_shut` (gate a 0 y 8 s sin envío). `fx_buses` lo comprueba con `any` y, si se cumple, llama a `fx_sleep`, que
pone el wet a cero y avanza las posiciones y la fase del LFO: **cada muestra que cubre el sueño ya iba a salir a
cero** (el gate la multiplicaba por cero). Las líneas se dejan como están —el gate las había dejado en −54 dB o
menos y vuelve a entrar desde 0, como siempre— y al despertar el bus arranca desde el silencio que ya sonaba. Un
envío, CLOUD en FREEZE (su gate queda abierto) o un cambio de modelo despiertan: el cambio de modelo se resuelve
dentro de `fx_sleep` (limpiar y tomar el nuevo), porque dormido la salida es cero de cualquier forma y no hay
crossfade que oír.

**Medido (host x86-64, `cc -O2`, `tests/fx_bench`, mejor de 5, ns por muestra).**

| escena | antes | después |
|---|---|---|
| idle: los buses armados, nada sonando | 77,3 | **36,4** (−53 %) |
| pistas secas (envíos 0) | 119,4 | **96,9** (−19 %) |
| los tres buses alimentados | 122,1 | 119,3 (sin cambio) |
| buses alimentados, CHORUS en FLANGER | 116,4 | 112,6 (sin cambio) |
| seis partes, cuatro notas cada una | 137,0 | 132,4 (sin cambio) |

**El sonido, comprobado.** `tests/regress` con los renders dorados: **311 renders, 0 cambiados, 0 fallos de salud,
0 de voces/routing, 0 de presupuesto de CPU** — cada motor × preset de fábrica, los modos de voz, los envíos, la
mezcla de 4 pistas y el SLICER, muestra a muestra iguales. `reverb_test` (32 comprobaciones: modelos, cambio de
modelo en marcha, colas, train de ecos, coste, disparo por fuera de rango), `dly_echo_test`, `six_track_test`,
`six_track_live_test`, `fx_meter_test`, `audio_test`, `eq_state_test`, `poly_stress_test` (12/12, nada denegado
con robo posible) y `audio_stress`: p99 180,8 µs de los 725 de presupuesto del bloque, 0 % por encima, sin muestras
no finitas. Barrido de UI: 3 129 tomas (48 de ellas por el texto de la versión), 0 lint, 0 alineaciones.

**Lo que se midió y no se tocó.** `master_out` (12 % del perfil) es dos DC-blocks, el leveler, el limitador y dos
softclips por muestra: todo él es el contrato de salida y no hay nada redundante que quitar. El reparto entre
cores queda como está: `mc_split` ya elige por coste medido. Y ningún bus con envío baja nada: no hay nada que
dormir mientras suenan.

**Presupuesto.** El XIP es el recurso que aprieta: el cambio entra con la condición en un byte que pone el gate y
el trabajo en una función fuera de línea (`fx_buses` va inlineada dos veces, flash y RAM), y el app slot queda en
**2 100 B libres** con el suelo en 2 048.

**Límite declarado.** Nanosegundos de host x86-64 sobre el C del camino de audio; no son ciclos del AC791N ni una
lectura de la FM-1. El ahorro en la unidad no está medido aquí.

## v1.8.8 — el pase de CPU: la música se recorre dos veces de seis, no seis de seis

Fase 2/9: CPU, entrada y sistema. Se midió antes de tocar, se tocó lo que la medida señaló y lo que se midió
neutro se devolvió.

**Lo que la medida encontró.** El perfilado del bucle principal quieto (gprof sobre la escena «pantalla
asentada, sin entrada», host x86-64) ponía el 78 % del tiempo en una sola función: `snd_hash` (core.h), la
huella del sonido de cada pista con la que el aviso SOUND EDITED sabe si el sonido cambió desde la última carga
o guardado. `ui_notices` la llamaba **seis veces por llamada de `ui_input`**, y `ui_input` no se llama una vez
por frame: el bucle la vuelve a llamar una y otra vez dentro de su espera de 15 ms (main.c). Con la perilla
quieta, el instrumento recorría las seis tablas de parámetros completas decenas de veces por frame para
llegar siempre al mismo número.

**El arreglo.** Dos de seis, no seis de seis: la pista **seleccionada** (la que dibujan el punto del navegador
y el pie) se hashea en cada llamada, así que un cambio se marca en la misma llamada que lo ve, como antes; las
otras cinco se turnan, una por llamada, y un cambio en ellas se marca dentro de seis llamadas — menos de un
frame en el bucle, y el punto de una pista no seleccionada sólo se dibuja cuando la página muestra esa pista.
Y `snd_hash` va ahora a **dos valores por paso** de FNV (16 bits leídos y combinados; `p[]` está alineado a 2,
no a 4: una lectura de 32 bits sería un acceso desalineado en pi32v2 y una excepción), la mitad de pasos por el
mismo sonido, valor por valor.

**Medido (host x86-64, `cc -O2`, `tests/frame_bench`, 60 000 llamadas, tres corridas).**

| | antes | después |
|---|---|---|
| `ui_input()` sola, pantalla quieta | 2 005 – 2 141 ns | **392 – 441 ns** (−80 %) |
| cuerpo del bucle (input + LEDs + dibujo) | 3 462 – 3 528 ns | **1 714 – 1 748 ns** (−50 %) |

**Lo que se midió y no se tocó.** `tests/isr_bench`, el tick de TIMER5 (una columna de 11, su 595, sus
teclas, cada 11 ticks el frame): **25,3 ns/tick quieto, 26,8 con LEDs encendidos y uno respirando, 28,5 con
una columna de teclas cerrada** (0,03 % del tick de 100 µs en este host). El escaneo ya venía optimizado
(FASE 1 anteriores: el módulo una vez, el pulso de brillo medido con TIMER4, la respiración sin divisiones) y
no hay nada que quitar sin cambiar la tasa de escaneo, el brillo o el debounce: no se tocó. El test de teclas
y LEDs (`tests/input_test.c`) sigue pasando entero.

**Lo que se midió y se devolvió.** Unificar los dos relojes de 250 ms de `autosave_poll` (el aviso de sección
llena y el autoguardado) en un solo recorrido de la música: el bench (`autosave_poll` por frame) da **697–751 ns
antes y 692–708 ns después** — neutro, el compilador ya calculaba una sola firma por tick. Devuelto, sin
commitear: la regla de la fase es no quedarse con lo que no se demuestra.

**Concurrencia y núcleos.** Revisado y sin cambios, con la razón: el bucle de core 1 (`fm1_multicore.h`) gira
sobre la caja de correo y core 0 espera en `fm1_mc_wait` con un tope de 8 ms; pasar el giro a WFE exigiría una
señal SEV desde cada escritura de la caja (audio incluido) y su efecto no es medible en este entorno — no se
inventa. La comprobación de arranque (`fm1_mc_selftest`) sigue siendo la puerta antes de darle audio a core 1.

**Regresión.** Barrido 196 pantallas × 22 paletas: 0 lint, 0 tintas sobre su color, 0 alineaciones fuera de
1 px; **3 129 tomas de píxeles idénticas** a la base (el cambio no dibuja nada distinto). `ui_test` entero,
incluidos los siete checks de SOUND EDITED (el knob marca, guardar limpia, una carga de proyecto es la
referencia, la automatización no es una edición). `ui_flicker_test` y `ui_render --idle` (29 952 px escritos,
0 cambiados) sin cambio.

**Nuevo instrumento.** `tests/frame_bench` (construido por la suite, no ejecutado por ella, como `isr_bench`):
mide `ui_input`, `ui_leds`, `ui_draw`, el cuerpo entero con una tecla pulsada y `autosave_sig`/`autosave_poll`,
para que la próxima fase compare con números.

**Límite declarado.** Son nanosegundos de host x86-64 sobre el C que corre el bucle; no son ciclos del AC791N
ni una lectura de la FM-1. El ahorro en la unidad no está medido aquí.

## v1.8.7 — la tarjeta de presets: una pantalla que parpadeaba, y lo que se midió

Fase 1/9 del ciclo nuevo: una optimización medida, la UI repasada entera y el parpadeo de la pantalla transitoria
de presets, rediseñada como tarjeta Quantum y con pruebas que impiden que el defecto vuelva.

**El parpadeo, encontrado y medido.** La pantalla transitoria de presets (`graph_pop_plate`, 208 × 86 sobre el
panel) tenía **tres** intentos anteriores de arreglo (v1.11 dejó de reutilizar el lienzo de la página, v1.5.15 le
dio su propio lienzo de su tamaño, v1.7.20 la hizo cabalgar el blit del panel para no enviar dos veces las mismas
filas). El defecto que quedaba era otro: con la perilla quieta y la ventana de 1,5 s corriendo, **la tarjeta se
volvía a enviar en cada frame** — 17 888 px, treinta veces por segundo, de píxeles idénticos a los que ya estaban
en el cristal. Un panel que muestra cada escritura eso lo pinta como un parpadeo. Ahora la tarjeta tiene su propia
firma (`graph_pop_sig`: el sonido elegido, la lista, el filtro, la estrella, el estilo) y **se envía una sola vez**;
un cambio de página o cualquier invalidación del panel la vuelve a habilitar. Medido: `--regions` caso A, «el
pop-up: 17 888 px escritos», «un frame después y sin cambios: **0 px**» (antes: 17 888 en cada uno).

**Rediseño.** La transitoria decía el nombre y nada más. Ahora es una tarjeta: cuerpo con degradado (el bisel de
las cajas de operador de FM6 y de las filas de MENU), raíl de acento en la cabecera, y una cabecera con el motor y
la categoría del sonido (**ANALOG KEYS**), su nivel medido como barra (el mismo número que las filas del navegador)
y su sitio en la lista (**6/291**). Debajo, las cinco filas de siempre —dos arriba, la elegida encendida y con su
estrella si es favorita, dos abajo— en 13 px de paso. Misma caja, mismo sitio, un solo envío.

**Regresión.** `ui_flicker_test` tiene un cuarto caso, `browse_pop`: el navegador con la tarjeta arriba en **cada**
frame mientras la perilla gira ocho pasos, medido en los dos sentidos que el instrumento sabe (dentro del frame:
px escritos dos veces; entre frames: un elemento borrado y vuelto en un frame). 0, 0 y 0. Y `--regions` caso A
comprueba ahora, además, que un segundo frame con la tarjeta arriba y nada movido escribe **0 px**: el parpadeo no
puede volver sin que una prueba lo diga. Nueva escena `presets_pop` en el barrido (196 pantallas × 22 paletas).

**Repaso de la UI.** Barrido completo: **196 pantallas × 22 paletas**, 0 de lint, 0 tintas sobre su propio color,
0 medidas de alineación fuera de 1 px (76 sitios declarados), 0 textos cortados nuevos; **3 129 tomas de píxeles**
idénticas a la base (actualizada sólo por la escena nueva). `--idle`: 29 952 px escritos y 0 cambiados, el mismo
piso que v1.8.6 — la tarjeta no añade gasto en reposo. `ui_flicker_test`: las cuatro escenas, 0 hallazgos.

**Lo que se revisó y no se tocó.** Los defectos de las fases anteriores siguen cubiertos por sus pruebas y no se
reabrieron: vúmetro de FX (`tests/fx_meter_test.c`, 3 comprobaciones), solapes de HOME 4/5 (`--regions`), nombres
de preset sobre el MIXER (`--regions` caso A), parpadeo del navegador (`ui_flicker_test` caso `browse`), MORPH y
escenas (`tests/morph_test.c`, escenas `S_SCENES`).

**Numeración.** Progresiva por parche (1.8.6 → 1.8.7), `docs/VERSIONING.md`.

**Límite declarado.** Nada medido en la FM-1 física: son las mismas fuentes en el host x86-64. Los «Issues» de
GitHub no son consultables desde este entorno (sin navegador ni credenciales): el estado de los defectos
reportados se comprobó en el código y en las pruebas que los guardan, no en el rastreador.

## v1.8.6 — la auditoría de cierre: presets, parpadeo, reposo y texto

Fase extra final, sin funciones nuevas: mirar todo lo que las tres fases tocaron y decir lo que se midió.

**Presets.** La biblioteca entera, leída y sonada: **293 presets, 0 en silencio, 0 pasados de nivel, 0
duplicados exactos, 0 nombres repetidos, 0 fuera de rango** (`tests/preset_audit_test.c`, que los pasa por el
motor de verdad). El censo (`tools/preset_audit.py`): PADS 68 · KEYS 58 · TEXTURES 51 · DRUMS 24 · BASS 21 ·
BELLS 21 · ORGANS 18 · LEADS 13 · PLUCKS 12 · ARPS 7; **291 de 293 llevan movimiento** en sus `x[]` y ninguno
se queda sin envíos. Ningún par cae por debajo de la distancia 0,035 (el más cercano es 808 SUB / TRAP 808 a
0,045) y ningún nombre se repite. No se reescribió ningún preset: la identidad de los clásicos es el activo, y
la auditoría no encontró nada que justificara tocarla. Lo que sí se comprueba es que las funciones nuevas no
rompen la carga: `upreset_test`, `preset_slots_test`, `preset_library_test`, `preset_change_order_test` y
`preset_categories_test` en verde.

**Parpadeo y reposo.** `ui_flicker_test` sobre las escenas que más se mueven, 8 pasos de encoder cada una:
**0 px borrados y reescritos, 0 px de banda perdidos, 0 px escritos dos veces**. Y el cuadro quieto:
`ui_render --idle` da **29 952 px escritos y 0 cambiados** en las escenas que no se mueven (tope 33 000: el
piso medido más un 10 %), el mismo número que antes de las tres fases.

**El barrido.** 195 pantallas × 22 paletas: **0 hallazgos de lint, 0 tintas sobre su propio color, 0 medidas
fuera de 1 px**, 3 120 tomas de `ui_pixels` idénticas a su base.

**Limpieza y texto.** La página SCENES separa su pie del borde de la placa; el manual en inglés vuelve a
envolver la línea de controladores que la fase 3 había estirado. `git status` limpio: no se generó ningún
archivo suelto que quedara en el árbol.

**La numeración, ahora progresiva.** De aquí en adelante una fase es **un parche** (1.8.N → 1.8.N+1) y el
minor sólo se mueve al cerrar una serie (`docs/VERSIONING.md`). **No hubo retroceso**: 1.8.3, 1.8.4 y 1.8.5
siguen publicadas, etiquetadas y en la lista del sitio; bajar la línea a 1.7.4 habría sido exactamente el
downgrade que se pidió evitar.

## v1.8.5 — QUANTUM MORPH: un extremo de otro track, y el controlador que lo mueve

**MORPH SCENES ya existía** (LFO > SCENES: cuatro estados del sonido de la pista, una pareja A/B y la posición
continua entre ellos, con slew de 3/127 por cuadro). Lo que faltaba era lo que la propia auditoría del proyecto tenía
abierto: *"MORPH con estados A/B persistentes — el formato de preset no tiene dos estados"*, y mover la posición
desde un controlador en vez de sólo la perilla.

**Un extremo puede ser otra pista (A < T1..T6 / B < T1..T6).** KNOB 4 de la página recorre ahora dieciséis acciones:
las dos capturas del buffer de edición, DIFF y doce `A < Tn` / `B < Tn` que copian *el sonido completo de otra pista*
—sus `p[]` y sus cuatro slots de matriz— dentro de la escena. La escena es una copia, no una referencia: editar esa
pista después no la cambia. Y como el proyecto ya guarda las seis pistas, una pareja hecha así **sobrevive a guardar
y a apagar** sin un byte de almacenamiento nuevo (la imagen crece 1 116 B; el registro de proyecto, que está lleno
hasta el último byte de su objeto de flash, no se toca).

**CC 78 mueve la posición, CC 79 y CC 80 capturan.** El controlador entra por la misma variable que la perilla, así
que la posición sigue pasando por el slew: entre dos bloques de audio nunca llega más de un paso de 127, venga de
donde venga. Los valores fuera de 0..127 se recortan, no dan la vuelta. La captura de un CC es la de la pista del
canal que lo manda.

**La página muestra lo que hace.** LFO > SCENES dibuja el raíl de A a B: la parte recorrida rellena, el marcador de
la posición, los números de escena en los extremos (color de tema para A, acento para B; grises cuando ese extremo
está vacío) y una fila por extremo con su número y `STORED` o `KNOB 4: STORE`. El pie dice lo que un par hace y qué
controladores lo mueven. Un extremo vacío no dibuja relleno y el pie avisa: nada de un deslizador que no mueve nada.

**Lo que no cambia.** Ni motores, ni voces, ni efectos, ni la escala de la posición; los tres tests de MORPH siguen
en verde (21 casos audibles, 6 transiciones sin click, la página y su interpolación) más once comprobaciones nuevas
de esta versión: la copia desde otra pista, que no es una referencia, el recorrido 0 → 127 → 0, el slew del
controlador, el recorte en los extremos y las dos capturas por CC.

## v1.8.4 — FX: el vumetro de DIST y la escala que lo tenía clavado

**El vúmetro del FX pegado arriba con la distorsión al 1 %.** No era la distorsión: era la escala. Las cuatro
tarjetas de FX (DIST, CHORUS, DELAY, REVERB) recortaban su nivel a **7000**, una escala que venía de cuando el
envolvente de la pista era un pico de 0..7000; `fx.c` mide desde v1.9 el `|x|` de la muestra de la pista en **Q15
(0..32767)**. Una nota normal mide **17 638** —252 % de 7000—, así que la barra de DIST llegaba al tope con la
distorsión a cero y el dueño lo leyó como "subir la distorsión un 1 % clava el vúmetro". Comprobado con una sonda
sobre el motor (`DIST 0 → vu 17 638`, `DIST 1 → 16 836`, `DIST 64 → 15 441`): el nivel ni sube con la distorsión.

Corregido a **32767**, la misma escala que los medidores del MIXER y el VU de MASTER; el punto de clip pasa de 6800 a
32 700 (el mismo umbral que el VU de MASTER). La conversión vive ahora en `fx_meter_target()` (core.h) para que la
página y `tests/fx_meter_test.c` no puedan volver a separarse: tres comprobaciones nuevas fijan 0, 17 638 (54 %) y
32 767 (tope, y más allá también tope).

**Análisis del motor, medido.** `tests/eng_bench` da el coste por motor con 8 teclas (host x86-64, por bloque de
32 muestras, sobre el silencio de 5,5 µs): ANALOG 2,7 µs · LOFI 3,1 · DRUM 4,1 · FM6 5,9 · PHASE 6,3 · VOICE 6,5 ·
NOISE 6,6 · ADDTV 6,8 · WHEEL 7,4 · WTABLE 7,5. Seis pistas con el motor más caro suman ~50 µs de los 725 del
presupuesto. `tests/fx_bench` (best of 3): todas las vías alimentadas 135,2 ns/muestra, seco 134,1, flanger 157,9,
**reposo con las vías armadas 119,4** y seis pistas con cuatro notas cada una 203,3. El perfil (`gprof`) del bloque
pone `fx_buses` al 28 %, `mix_block` al 16 %, `track_render` al 12 %, `rev_room` al 9 %.

**Lo que no se cambió, y por qué.** Ninguna optimización del DSP se quedó en esta versión: para tocar el camino de
audio la barra es que los 311 renders golden queden idénticos, y los candidatos que se midieron (saltar el bloque
de vías cuando su entrada y su cola son exactamente cero, fusionar los pases de medición de la reverberación) mueven
posiciones de línea o la fase del LFO del chorus, que sí cambian el sonido al volver la señal. Quedan anotados con su
coste en `docs/OPT_DSP_2026-11-23.md` para que el siguiente intento empiece por ahí en vez de repetir el recorrido.

## v1.8.3 — HOME: el MASTER del MIXER y la batería cargando

**MIXER, sección MASTER (HOME 4/5): los porcentajes se leían encima de los vumetros.** Las barras L y R iban de
x 82 a 214 (132 px) y su porcentaje está alineado a la derecha en x 226: con tres cifras y el signo el número empieza
hacia x 202, así que doce píxeles de barra quedaban debajo del texto en cada fila, y la barra de LEVEL (desde x 72)
llegaba a dos. Ahora las tres barras comparten columna (x 82, 112 px: 194 como máximo) y los números conservan su
franja de 196 a 228. Nada se oculta ni se acorta: los vumetros siguen midiendo lo mismo, el porcentaje se lee entero
y LEVEL conserva su fila.

**La batería cargando era un borrón.** Con USB conectado el icono dibujaba tres celdas y les recortaba tres muescas
de fondo para insinuar el rayo: a 20 x 13 las muescas dejaban las celdas como un bloque con mordiscos y el rayo no se
leía. Ahora, con corriente externa, no se dibujan celdas: el cuerpo mantiene su marco y el rayo se dibuja en tinta
— cabeza arriba a la derecha, caída en diagonal, pie abajo a la izquierda —, tres trazos de 3 a 6 px que se leen a
la distancia del panel. Los cuatro estados de nivel no cambian.

**FPS de HOME: perfilado hecho, un candidato probado y descartado por su propio test.** Con `gprof` sobre HOME 1/5
el cuadro costaba ~90-105 µs (host x86-64) y el 36 % de las muestras estaba en `ux_mix`; el 17 % en los cuatro diales
de la bahía HEARTH. Se les dio firma propia para no repintarlos cuando no cambiaban y el dibujo del panel bajó de
102,7 a 78,8 µs (mediana de cinco series) — pero `ui_flicker_test` cazó el fallo de fondo: `draw_graph` empieza cada
panel con un `cv_begin` que **limpia el lienzo**, así que un widget que se salta su dibujo deja un agujero de 375 px
en la bahía. El cambio se revirtió entero y no hay ganancia de FPS que declarar en esta versión. Se deja escrito en
el código y en `docs/OPT_HOME_2026-11-23.md` para que el siguiente intento no repita el camino: para cachear dentro
del panel hace falta un lienzo persistente, no una firma.

## v1.8.2 — Cierre de ciclo: lo que la web decía y el firmware ya no

**Números públicos contra números medidos.** `tests/preset_library_test.py` cuenta **291 sonidos de fábrica** en
diez motores; la portada decía 283, el `fmhub.json` 287 y el README anunciaba `v1.7.0`. Los tres se corrigen a lo
que el código tiene (291 sonidos, versión 1.8.2). Ninguno de esos números se tocó de la tabla de presets: lo que
estaba mal era la copia.

**Un pendiente que ya estaba resuelto.** La desincronización del punto de ENV con el ADSR dibujado se cerró en
v1.12: el punto se coloca con `env_x_of`, que recorre la misma recurrencia que la curva muestra a muestra en vez de
invertir una recta. La matriz lo daba por abierto; ahora dice dónde y cómo se cerró.

**Auditoría de seguridad (sólo lectura, nada debilitado).** `sign_test` (firma válida, alterada, clave desconocida,
downgrade), `key_custody_test` (la clave nunca se sobreescribe, PKCS#8), `install_test` con sus compuertas,
`preflight_test` sobre el paquete real y `test_pkg_bounds`: todos en verde. No hay claves, tokens ni `.env` en el
repositorio; el único `BEGIN PRIVATE KEY` del árbol es una aserción de test sobre una clave que él mismo genera.

**Limpieza.** Sin artefactos de build, `.o`, `.pyc`, `.log` ni respaldos versionados; sin `TODO`/`FIXME` en el
firmware; `build/` y los worktrees `wt/` no están en el repo. Se conservan las auditorías históricas de `docs/`
como registro, marcando cuál es el estado actual.

