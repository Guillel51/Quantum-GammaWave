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

