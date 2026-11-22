## v1.11.0 — Fase 4/4: el parpadeo del menú de presets, cerrado con su causa

**Corregido: el parpadeo del menú de presets al mover el knob**

- La causa no era el volumen de trabajo sino **el mismo error del visualizador, en otro sitio**: `lcd_blit` envía las
  filas que un blit cambió, así que una fila pintada **dos veces en el mismo cuadro se envía dos veces**, primero con
  el contenido del listado y después con el pop-up encima. En cada paso del knob eran cinco filas (las del pop-up)
  enviadas dos veces con contenido distinto entre los dos envíos: el destello del reporte.
- Ahora el pop-up de PRESETS se compone en una banda de **ancho completo (240 × 86)** y el listado **deja sus cinco
  filas** a esa banda: cada fila se escribe y se envía **una vez**. El plato sigue siendo de 208 px y las páginas que
  no son el listado (el MIXER, HOME) conservan su caja de 208 × 86 exacta: `--regions` mide **0 px** escritos fuera
  de la caja del pop-up sobre el MIXER, como antes.
- Las filas que window se quedan sin enviar son **0** en el escenario nuevo `ui_render --bands` ("PRESETS (knob)"):
  el cuadro incremental es **píxel a píxel** el de un repintado completo forzado. Un paso del knob cuesta
  **64 848 px escritos** (tope 71 400 = lo medido +10 %; antes 62 096 con las cinco filas escritas dos veces).

**La otra mitad, medida y descartada**

- `preset_hinted()` hacía `ui.force = 1` en **cada** carga de sonido: probado, sin él quedan **103 filas viejas** en
  las tarjetas y el pie de PRESETS. Se mantiene (las tarjetas muestran los parámetros del sonido cargado); lo que se
  arregla es la doble escritura del pop-up, que es la que produce el destello.

**Fase 4/4: perfilado y estabilidad**

- **CPU/DSP `[HOST x86]`**: bloque de 32 muestras `fx_bench` **167,7 ns/muestra** (mejor de 5) = 0,01 del
  presupuesto de 725 µs; `eng_bench` 10 motores (FM6 12,0 ns/muestra, WTABLE 13,7); `isr_bench` `fm1__frame`
  **16,7 µs** para las 11 marcas, `fm1__rows` 2,9 µs, `fm1__keys` 2,9 µs, `fm1__breath_frame` 8,6 µs.
- **Render**: 8 400–10 800 px escritos por cuadro en el visualizador (antes de la fase 3: 16 080–18 960, −45 %);
  `--idle` 29 952 px escritos y **0 cambiados**; `--viz` verde; `--bands` 5/5 escenas en tope con 0 filas viejas.
- **RAM/flash**: sin cambios de presupuesto esta fase (`.data+.bss` 93 504 / 98 304, POOL 9 248 libres); el
  firmware crece sólo con el código del pop-up y su comentario.
- **Estabilidad**: `tests/stress_run.sh 3` (ASan + UBSan, `project_test`, `persistence_test`, `robust_test`,
  `midi_control_test` × 3 ejecuciones y 2 pasadas completas de `regress`) **todo ok**; 15 binarios de test nativos
  en verde; 16 suites Python de seguridad/instalación/flash en verde; `regress` 307 renders de audio, 0 cambiados.

## v1.10.0 — Fase 3/4: el parpadeo del visualizador, la batería y lo que el menú de presets cuesta

**Corregido: la sección superior del visualizador**

- El *cap* que tapa las seis filas entre la cabecera y el panel (20..25) se dibujaba **26 filas de alto**:
  pintaba y hacía enviar las filas 20..45, o sea **las 20 filas de arriba del panel dos veces por cuadro**,
  primero con el tono plano de la página y después con el panel y su título. Una banda enviada dos veces con
  contenido distinto entre los dos envíos es un parpadeo en el cristal, y estaba justo donde el reporte lo
  nombra. Ahora el cap mide las seis filas que le tocan (`Y_LABEL_GAP_H`).
- Medido `[HOST]` con el modo nuevo `ui_render --viz` (cuadros consecutivos con la misma señal): **16 080–18 960
  px escritos por cuadro antes, 8 400–10 800 después** — entre un 45 % y un 55 % menos de escrituras por cuadro
  en la pantalla, y esas filas ya no se envían dos veces.

**Corregido: el icono de batería**

- Era un glifo Fukiai de 16 px que a distancia de brazo se leía como un borrón: el nivel no se distinguía del
  contorno. Ahora se **dibuja**: cuerpo de 18 × 11, terminal de 2 px y tres celdas altas, centrado en la barra
  como el resto de sus elementos (filas 3..13, x 214..235). Cinco estados como antes: vacía (contorno MID), una
  celda ACENTO, dos y tres celdas en THEME, y USB con su rayo recortado dentro del cuerpo.
- Verificado con el barrido: 0 hallazgos de lint, 0 desalineaciones en 3 806 950 medidas, y los seis estados
  renderizados en `batt_0/1/2/3`, `batt_usb` y `home`.

**Medido, y NO cerrado: el parpadeo del menú de presets**

- Se agregó el escenario al barrido (`ui_render --bands`, "PRESETS (knob)"). Un paso del knob escribe
  **62 096 px** para cambiar **14 161**: 4,4 veces las escrituras que necesita, la mayor parte bajo el popup,
  que se pinta encima acto seguido. Las filas que no se envían son correctas (0 obsoletas).
- Se probó y **no** era la causa el `ui.graph_sig = 0` que forzaba el repintado del grafo: sacarlo no cambió el
  número. El parpadeo del panel no se pudo reproducir en el harness y **queda pendiente** con su medición y su
  escenario listos; no se declara corregido.

**Barrido**

- 191 pantallas × 22 paletas, 0 lint, 0 alineación, 3 056 frames de píxeles regenerados a propósito (la batería
  está en todas las cabeceras), idle 29 952 px escritos y 0 cambiados.

## v1.9.0 — Fase 2/4: cuántas voces aguanta de verdad

**Medido, no supuesto**

- Escenario nuevo en `tests/fx_bench` (escena 4): seis pistas pidiendo **24 notas** a un pool de 12, el tope
  mordiendo de verdad. `[HOST x86]` 12 voces **142,6 ns/muestra**, 16 voces **150,7** → **~2,0 ns por muestra
  por voz** en el caso más denso, +5,7 % de un bloque ya a polifonía llena.
- Se probó subir el tope a 16 y **no se subió**, con el número en la mano: `[AC791N]` el `.bss` sube 1 360 B
  (pool + ventanas) y deja **3 440 B libres contra el piso de 4 096** de `tests/resources_test.py`, o sea no
  entra; y el pool más grande mueve **52 de 220 renders** (los seeds por slot), hasta 1,4 dB. Falta recuperar
  ~1 300 B de `.bss` antes de que la pregunta sea de CPU, y la CPU sobra.

**Corregido en los tests**

- Los cinco tests de polifonía afirmaban el literal 12 donde querían decir *el tope en vigor*: ahora comparan
  con `voice_budget()`, así que el día que la RAM alcance, subir el número no rompe cinco ficheros ni deja
  pasar un tope que no es el que suena.

**Cómo se midió**

- Los números y su clase están en `docs/VOICE_POOL.md` §"1.9", y el escenario se reproduce con
  `cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/fx_bench tests/fx_bench.c -lm && ./build/fx_bench 8`.

## v1.8.0 — Fase 1/4: el perfilado del bloque de audio, y el gate que vuelve a medir lo que dice medir

**Medido primero, no adivinado**

- Perfilado del camino de audio con `gprof` sobre `tests/fx_bench` (host x86; la clase del número está dicha):
  buses de FX **18,6 %**, `rev_room` **16,3 %**, `master_out` **11,6 %** (32 llamadas por bloque), `mix_part`
  **11,6 %**, FM6 **16,3 %** (`fm6_render` 9,3 % + `fm6_op_run` 7,0 %), `mix_block` propio 7,0 %,
  `events_block` 4,7 %, `analog_dispatch` 4,7 %. `tests/fx_bench` da 78–80 ns por muestra en el host: el
  bloque entero son ~2,5 µs de x86 por 32 muestras.
- Tres intentos de micro-optimización medidos con A/B y **descartados** por no mover la aguja (el compilador
  ya los hacía): fundir la pasada de los vúmetros del master con la del loop de salida (+46 en el costo
  estático del target y 81,3 vs 79,1 ns/sample en el host), `always_inline` sobre `master_out` (sin cambio,
  más código), y sacar la constante de damping de la reverb fuera del loop (el target ya la sacaba: 243 → 243).
  Quedan documentados en `docs/PERFORMANCE.md` para que nadie los repita.

**El defecto que sí había, y el arreglo**

- `tests/target_budget.py` marcaba **`mix_block` +31 % sobre su baseline de 1.4.5** (42 775 contra 32 667) y
  el gate salía en rojo: el compilador estaba metiendo **todo el secuenciador** (scan de pasos, MOTION,
  FUGUE, RATCHET, arpegiador) dentro de la función del bloque, donde corría en el anidamiento más profundo y
  no había manera de ver qué parte había crecido.
- `events_block()` es ahora su propia función (`noinline`, como `mix_block`, `drum_out` y `drum_side_*` por
  la misma razón). Medido: `mix_block` **115**, `events_block` **45 589**, y una línea nueva **`BLOCK_PATH`**
  que suma las dos (45 704) para que mover trabajo de una a otra no pueda esconder un total crecido. El gate
  vuelve a verde con líneas que describen el código que hay, y sigue fallando si cualquiera de las dos crece
  un 10 %.
- En el host el cambio es neutro (79,4 / 79,3 / 78,4 ns por muestra con el split, 78,8 / 79,5 / 79,6 sin él):
  no se vendió una mejora de velocidad que no existe. Lo que se ganó es que el número significa algo.
- `tests/regress`: 307 renders, **0 cambiados** (el audio es idéntico byte a byte); barrido de UI 3 056
  frames idénticos; `resources_test` 13/13; y la CPU del host queda **0 entradas sobre presupuesto** (antes 1).

## v1.7.0 — SCENES (morph entre cuatro estados) y el cierre de la auditoría

**Nuevo: MORPH SCENES**

- Cuatro escenas por pista: **STORE A / STORE B** capturan el sonido completo del track (todos sus parámetros
  y los cuatro slots del matrix 5..8) y el knob **MORPH** lo lleva de A a B. Interpola sólo lo que MOTION ya
  trata como sonido; las elecciones (MUTE, routing, modos) siguen la escena más cercana, y el motor, el
  transporte y el allocator no se tocan. Un salto se recorre a 3/127 por frame, así que no hay click.
- NO entra en el formato de preset ni de proyecto: ningún byte guardado cambió y una escena es el buffer de
  edición tal como está (capturar el sonido intacto es el "antes" del sonido que estás editando).

**Corregido — instalación desde el V16 oficial**

- «this copy is not tied to a unit yet» con una copia real en la mano: la asociación se hacía **sólo** al
  seleccionar el archivo, leyendo un VERIFY en memoria, así que cargar la copia antes de VERIFY (o después de
  recargar) la dejaba sin atar para siempre y VERIFY no la reconciliaba. Ahora se ata al cargar, al verificar y
  al instalar, y sobrevive al reload.
- El `unit id` de la metadata de M-Flasher se lee, se muestra y se compara; es el único identificador por unidad
  disponible (la identidad SysEx es `FM-1_016` en toda unidad V16) y no autoriza nada por sí solo.
- El reporte decía `gate.backup: pass` y `v16-backup-not-taken` a la vez; la limitación sigue a la copia. La
  atadura es automática sobre la identidad que responde, así que el informe no retiene una copia «sin unidad»
  mientras la página la aceptaría; lo que sigue rechazándose es una copia atada a otra identidad.

> Estado de esta corrección: **ejecutada**. `web/test_v16_path.mjs` (73 checks) y `web/test_verify_only.mjs`
> (179 checks) pasan, incluidos la copia cargada antes de VERIFY, la recargada y la de otra unidad. Ver
> `docs/VALIDATION_MATRIX.md` §36.3.

**Corregido**

- `fx_probe` corría sin `sine_init()` igual que el arnés de la Etapa 7: medía un LFO mudo, así que el phaser
  daba 6.6 dB de peine, el flanger no movía su null y el shelf del EQ leía su propio corte a 4 kHz. Con la
  senoide decodificada mide 23.6 dB de peine, un null que viaja de 4383 a 240 Hz y un shelf de -24.0 dB en el
  fondo y -0.0 dB a 4 kHz.
- El master EQ y el shelf fijo del perfil LOWCUT/BASS+ escribían el **mismo** biquad: elegir un perfil
  sustituía el EQ y, como el perfil no invalidaba la caché de parámetros, nada lo reconstruía hasta mover un
  knob del EQ — mientras la gráfica seguía dibujando el shelf pedido. El perfil tiene ahora su propio filtro
  (`prof_eq`), aplicado después del EQ, así que los dos pueden estar a la vez. `fx_probe` mide el EQ por la
  ruta aplicada (el struct que corre `qg_biquad_process`, no una copia) y comprueba que sobrevive al cambio de
  perfil.
- El MIXER de MENU > LARGE cortaba el nombre del sonido con puntos suspensivos donde las seis columnas del
  mixer normal lo cortan sin ellos (`PULSE…` en una tarjeta de 57 px contra `PULS` en una columna de 38),
  dos formas de imprimir el mismo dato en la misma pantalla. Ahora las dos usan la misma regla: la tarjeta
  dice `PULSE`, `GLASS`, `SAW`. Los textos recortados del barrido bajan de 456 a 264.
- El FLANGER barría con un producto de 32 bits desbordado: `(sin + 32768) * amt` llega a 4,26e9, así que a
  partir de media vuelta del LFO el tap caía de 302 a 9 muestras y el peine saltaba en vez de viajar. Ahora
  recorre 48..302 muestras (null de 73 a 450 Hz) y `fx_probe` lo mide con un impulso por la ruta real.
- El guard de presets de `fm6bank` seguía en 171 cuando la lista ya tenía 172 (los tres sonidos del remaster
  de 1.5.19); el propio comentario del test decía 172.
- El consola serie de diagnóstico (`FELUCCA_CDC`) queda **off por defecto**: pagan los 3 668 B de enlace del
  motor de escenas. `FELUCCA_CDC=1 ./build.sh` lo devuelve; MENU > USB SERIAL muestra `--` sin él.
- XIP: 556 944 B con 6 040 B libres — el floor de 2 048 B vuelve a pasar.

## v1.6.0 — Etapas 1–8: síntesis, herramientas, interfaz y el cierre del proyecto

Primera versión que reúne las ocho etapas en un solo paquete publicado, con la instalación desde el V16
oficial documentada y probada hasta donde el software puede probarla.

**Síntesis y herramientas**

- **PHASE**: la octava onda es **SYNC**, una portadora reiniciada por el ciclo maestro (Etapa 4).
- **ARP**: modos **PEDAL** y **ALBERTI**, además de los ocho existentes (Etapa 5).
- **Escalas**: **HIJAZ** y **HUNG MINOR** en el motor de escalas y acordes (Etapa 5).
- **LFO**: divisiones con puntillo y el extremo rápido ordenado — `1/32T` y `1/64` cierran la lista (Etapa 7).

**Interfaz**

- **CHANCE** dibuja la probabilidad de todo el patrón, no un valor suelto, y **MOD MAP** muestra en una banda
  dónde aterrizan las rutas (Etapa 6).

**Corregido en la auditoría de la Etapa 7**

- El arnés de audio corría sin `sine_init()` desde que la senoide se mudó a `.bss`: medía un oscilador mudo y
  dejaba `security` en rojo.
- La afirmación del test del EQ (medio gain en la esquina) sólo vale lejos de Nyquist; se mide a 495 Hz y la
  esquina de 20 kHz contra lo que el diseño bilineal promete.
- El lazo de `stress` medía ns de CPU mientras él mismo era la carga.
- Las dos divisiones que agregó la Etapa 5 estaban fuera de orden: al pasar de `1/32` el LFO saltaba a lento.

**Instalación desde el V16 oficial**

- La página toma la versión de la identidad que la unidad responde (`FM-1_016` → `16`), no una constante.
- El chip se **diagnostica**: eco, cuerpo corto, silencio, CRC, negativa, JEDEC desconocido y geometría tienen
  frases distintas; el mensaje del informe («did not answer its JEDEC Truncated identification reply») ya no se
  puede componer.
- Una unidad stock **no** implementa `0x12`: el loader del paquete lee la parte en la unidad antes de borrar, y
  la página no inventa una identificación.
- El camino V16 exige la **copia completa de 1 MiB** de esa unidad, la confirmación de que se sabe restaurar y
  el `FM-1.fwsc` oficial como vuelta atrás; el registro de M-Flasher se lee y se ata a la copia por SHA-256.
  Estado del gate: **GO CONDITIONAL** (la restauración sobre un layout V16 no se ha observado en hardware).

**Verificación de esta versión**: `security` completo, `stress 2`, `presets` 287/287, `web` 0, `sync` con el WASM
del mismo build, `regress` 307 goldens, `install_test` + los tests de la página V16 (175 checks), `make_release
--check` sobre `release/` y `site/`. Build AC791N: ver el informe de la Etapa 8 para los bytes y el margen.

## Etapa 7/8 — auditoría: cuatro defectos encontrados y corregidos, con test de regresión

- **P1 test de audio sobre un instrumento sin inicializar**: `tests/audio_test.c` llamaba a `delay_edge()` antes
  del boot; desde la Etapa 2 la senoide se decodifica en `.bss` (`sine_init`), así que el arnés medía un oscilador
  mudo y `groups.sh security` estaba en rojo. Ahora arranca con `fresh()` y lleva un check propio de que la senoide
  está viva.
- **P1 test del EQ con la afirmación equivocada**: el medio gain en la esquina sólo vale lejos de Nyquist; se mide
  a 495 Hz y la esquina de 20 kHz se contrasta con lo que el diseño bilineal promete. `eq_response` pasa.
- **P2 `stress` medía tiempo de CPU con la máquina cargada** (`CPU_NS_BASE=/dev/null` en el lazo; el presupuesto
  se mide en reposo y en el destino). `stress: all ok`.
- **P2 divisiones del LFO fuera de orden** (introducido en la Etapa 5): las dos agregadas pasan a ser `1/32T` y
  `1/64`, el extremo rápido y en orden; mismo significado para todo valor guardado.
- **Test nuevo** `tests/etapa7_audit_test.c` (ASan/UBSan, registrado en `run_tests.sh`): índices del ARP para
  1..64 notas, ondas de PHASE 0..15, coherencia de las cuatro tablas de escalas, orden de las divisiones del LFO y
  la tabla de destinos de MOD que causó el cuelgue de la Etapa 6.
- Verificación: `security` completo, `stress 2`, `presets` 287/287, `web` 0, `regress` 307 goldens sin cambios,
  `target_budget` 0, build 581 296 B (2 316 libres). Informe en `docs/ETAPA7_AUDITORIA_2026-11-21.md`.

## Etapa 6/8 — interfaz: CHANCE y MOD MAP dibujan lo que antes sólo se leía

- **CHANCE** (SEQ): la tarjeta era el piano roll con la pantalla casi vacía. Ahora los 16 pasos de la página son
  columnas medidas desde el piso, con guías de 25/50/75/100 %, separadores cada cuatro pasos, el color como
  probabilidad (tema al 100 %, más cálido cuanto más baja), los pips de RATCH, el cursor enmarcado y el paso que
  suena marcado.
- **MOD MAP**: debajo de las rutas, una banda nueva con cada destino asignado y la **suma de las cantidades** que
  lo mueven, en barra bipolar con su valor; sin rutas, dice `NO ROUTES`.
- **+1 120 B** de XIP (2 312 B libres, piso 2 048), **0 B** de RAM. Barrido UI completo: 191 pantallas × 22
  paletas, **0 hallazgos de lint, 0 desalineaciones**; `ui_test` all passed; 307 goldens, **0 cambiados**.
- El barrido encontró un `for` que comparaba contra la tabla (`N_MDST`) en vez de su tamaño: compilaba con `-w` y
  colgaba el render. Corregido y documentado en `docs/ETAPA6_INTERFAZ_2026-11-21.md`.
- Lo que no se hizo y por qué (el rediseño de 23 pantallas no cabe en 2,3 KB de XIP): mismo documento.

## Etapa 5/8 — herramientas nuevas en ARP, SCL y LFO (sin parámetros nuevos)

- **ARP > MODE = PEDAL**: el bajo de los acordes sostenidos en cada tiempo fuerte de cuatro y la línea subiendo
  entre ellos (pedal point). **ARP > MODE = ALBERTI**: la figura 1-3-2-4.
- **SCL > SCALE = HIJAZ** (dominante frigia) y **HUNG MINOR** (menor armónica con #4), con nombre completo y
  familia para el explorador. La UI deja de limitar el tope a 16.
- **LFO > RATE = 1/2D y 1/16D**: las dos divisiones con puntillo que faltaban (RATE llega a −14).
- Todo son valores **al final de una lista existente**: presets, proyectos y MIDI guardados conservan su
  significado; no hay campos ni RAM nuevos. Web Studio sincronizado (y su mock, que además estaba atrasado en
  PHASE/LOFI WAVE).
- **+208 B** de XIP (3 432 B libres), **0 B** de RAM, `target_budget` exit 0 tras reescribir PEDAL sin la
  llamada recursiva que había hecho crecer `mix_block` un 37 % en el presupuesto estático.
- Test nuevo `tests/etapa5_tools_test.c`; 307 golden renders, **0 cambiados**; ui/presets/web en 0.
- Detalle y lo evaluado sin hacer en `docs/ETAPA5_HERRAMIENTAS_2026-11-21.md`.

## Etapa 4/8 — una onda nueva en PHASE: SYNC (relación 1..8x reiniciada por el ciclo maestro)

- **`PHASE > WAVE = SYNC`** (la novena): la portadora corre a una relación continua que fija **DCW** (1× … 8×, en
  octavos) y el ciclo maestro la reinicia — el timbre sync, de la senoide pura al timbre duro y metálico, en un knob que ya
  existía. **+96 B** de XIP (slot: 3 640 B libres), **0 B** de RAM, sin parámetro nuevo y sin cambio de formato:
  los valores 0..7 guardados suenan exactamente igual que antes.
- **Preset nuevo**: `PHASE > SYNC CATHEDRAL`. La biblioteca pasa a **287** sonidos (no se borró ninguno).
- **Test nuevo** `tests/pd_sync_test.c`: las nueve ondas dan nueve hashes distintos en la misma nota, y DCW en
  8/64/120 da tres sonidos distintos.
- Verificación: **307 golden renders, 0 cambiados, 1 nuevo**; presets 287/287; categorías 287/287; web 0;
  recursos 13/13; `target_budget` exit 0.
- Detalle, evaluación de los demás motores y el muro de XIP en `docs/ETAPA4_SINTESIS_2026-11-21.md`.

## Etapa 3/8 — el coste real del audio, un banco de estrés y dos optimizaciones medidas (una entra, una no)

- **Nuevo `tests/audio_stress.c`**: seis motores, seis partes, dieciséis teclas, todos los buses, MIDI y cambios
  de motor bajo las notas; vigila NaN/Inf, pico, DC, clicks (salto entre muestras) y el tiempo por bloque.
  Pasa: pico 0.300, DC −0.0001, salto máximo 0.151, 0 frames sobre escala completa.
- **Nueva escena 3 en `tests/fx_bench`** («idle, los buses armados»): con la caja encendida y sin notas el bloque
  cuesta **78 ns/muestra contra 128** con todo sonando, y `fx_buses`+`rev_room` son el 52 % de ese coste.
- **Saltar el coro cuando su línea está vacía** quedó **escrito, probado y exacto** (24 046 bloques saltados,
  diferencia 0.000e+00 en 20 s) pero **no se publica**: cuesta **+3 172 B** del slot, que tiene 3 736 libres.
  Detalle y mediciones en `docs/ETAPA3_AUDIO_2026-11-21.md`.
- **Saltar los buses enteros en silencio** se descartó con medición: la reverb guarda energía en sus líneas que
  la salida no muestra (0.26 de cola liberada al reanudar) y el residuo nunca llega a cero (40–42 LSB eternos).

## Etapa 2/8 — la senoide sale del slot, y un bug de host que dejaba pasar cualquier regresión del FM6

- **`SINE` (2 050 B de XIP, 1 025 puntos) se guarda como su segunda diferencia en `int8`** (1 023 B) y
  `sine_init()` la reconstruye en el arranque en `.bss`: **imagen 580 900 → 579 876 B**, **libre en el slot
  2 712 → 3 736 B (+38 %)**, `.data+.bss` 91 560 → 93 612 B. Exactitud punto por punto en
  `tests/sine_table_test.c` (6 checks) y los mismos 306 renders.
- **Bug encontrado y corregido**: ningún arnés de host inicializaba `fm6_exp2_tab` (Etapa 1), así que el FM6 se
  probaba con una curva de ceros y `tests/golden.txt` estaba viejo: `HEAD` difería en **65 de 306** renders.
  Las dos curvas se inicializan ahora en `voices_boot()` (el punto de entrada de todos los arneses), los 31
  hashes obsoletos se regeneraron y la suite vuelve a 0 cambiados, dos corridas idénticas.
- **Presupuestos de memoria por ventana**: `tools/resource_map.py` lee XIP, RAM, POOL, LOWBUF, **NOINIT** y
  RAMTEXT del `app.ld`, con un piso por ventana (`.noinit` no lo vigilaba nadie: 124 B libres, 99.2 %), reparto
  por subsistema y `--check` que falla al cruzar un piso; `tests/resources_test.py` lo corre (13 checks).
- Detalle y pendientes en `docs/ETAPA2_MEMORIA_2026-11-21.md`.

## Etapa 1/8 — la curva exp2 del FM6 sale del slot y el mapa de recursos tiene techo

- **La tabla `FM6_EXP2` (4 100 B de XIP, 1 025 puntos) se guarda como su segunda diferencia** (2 046 B, `int16`) y
  se reconstruye en el arranque con sumas, en `.bss`, al lado de `wt_gen_init`: **imagen 582 884 → 580 900 B**,
  **libre en el slot 728 → 2 712 B (×3.7)**, `.bss` +4 100 B (quedan 6 764), pool intacto. `[HOST]`: **306 golden
  renders, 0 cambiados**; **0 hallazgos de lint**; **0 de 3 785 110** medidas de alineación; `target_budget`
  exit 0. La decodificación es entera y exacta, y `tests/fm6_exp2_test.c` compara los 1 025 puntos contra la
  fórmula del generador.
- **Diagnóstico nuevo**: `tools/resource_map.py` lee el ELF (RAM contra 96 KiB, hueco del pool con la ventana
  leída del `app.ld`, `.lowbuf`, stacks, XIP contra el slot que fija `web/fm1pkg.js`) y `--check` falla si se
  cruza un techo; `tests/resources_test.py` lo corre y además fija la estructura de la tabla nueva.
- Detalle, números y pendientes para la Etapa 2 en `docs/ETAPA1_RECURSOS_2026-11-21.md`.

## v1.5.28 — la Fase 6 llega al firmware: un frame no vuelve a construir sus rampas

- **La caché de rampas de texto pasa de 6 a 20 entradas.** Fase 6 había quedado resuelta del lado del Web Studio
  y el firmware no se había movido; esto sí es del firmware y se mide. Una pantalla con cuatro tarjetas de
  perilla, un valor, una unidad, una etiqueta y un acento pide **10 a 17** pares `(tinta, fondo, calor, curva)`
  distintos, y con seis ranuras el que el siguiente glifo necesitaba ya había sido desalojado: el barrido de UI
  contaba **12 rampas reconstruidas por frame** en HOME y **17** en MIXER, cada una recorriendo los 16 niveles de
  cobertura por tres canales. Con 20 ranuras el mismo barrido cuenta **0 en las 12 pantallas medidas**, y el
  primer frame es el único que construye algo. **`[HOST]`:** 0 hallazgos de lint, 0 de 3 785 110 medidas de
alineación, **306 golden renders, 0 cambiados** (un acierto de caché devuelve el mismo array que la
  reconstrucción escribiría, así que esto no puede tocar un píxel ni una muestra). **`[AC791N]`:** cuesta sólo
  `.bss` — 20 ranuras son 760 B de los 11 KiB de RAM libres — y **0 B de XIP**. El barrido ahora **falla** si una
  pantalla reconstruye más de 2 rampas por frame: la tabla no puede volver a quedarse chica sin que se sepa.

Cierre de pendientes sobre lo anterior en esta misma versión. Todo `[HOST]`: no hubo unidad física y nada se
le escribió.

- **El instalador rechazaba todo paquete publicado desde v1.5.26.** `tools/build_manifest.py` registra el
  archivo que escribió (`felucca-X.Y.fwsc`) y la página exigía la clave literal `felucca.fwsc`: el mensaje era
  *“The file to install is damaged”* con bytes idénticos al registro. Ahora se usa **el nombre que el registro
  trae**, con la exigencia de que sea **exactamente uno** y que su campo `package` coincida; `make_site.py`
  valida ese mismo artefacto en vez de comparar contra `{}` (publicaba un sitio con un registro falso en
  silencio). `web/test_release_e2e.mjs` cubre además el par **publicado** (`release/build-manifest-vX.json`
  contra `release/felucca-vX.fwsc`) y que el instalador construido busque el paquete por ese nombre.
- **Instalar desde una FM-1_016 (V16).** La condición de respaldo ya no acepta “un archivo cargado”: pide el
  registro del dump (**1 MiB + SHA-256** calculado en el navegador). Con ese registro el gate da `pass`,
  la recuperación queda `conditional` y **GO CONDITIONAL habilita INSTALL desde la página**; las limitaciones
  siguen nombrando que la vuelta y la recuperación no se observaron. Un registro malformado la deja en
  `required`.
- **Seguridad del camino de instalación.** Los nombres de artefacto tienen que ser rutas relativas simples
  (sin `..`, sin `/` inicial, sin `__proto__`). Revisión completa, hallazgos aceptados y riesgo residual en
  `docs/INSTALL_SECURITY_REVIEW_v1.5.29.md`.
- **`tests/preset_categories_test.py` vuelve a pasar** (286 sonidos): el conteo ya no es una constante ni el
  ancho del array está escrito a mano, se derivan del header y de `table_all()`.
- **Degradados de todas las pantallas** (Fase 4): rampa de placa y de suelo *dark-heavy*, pie del panel al
  74 % del fondo y acento del suelo a la mitad; `[HOST]` **0 hallazgos de lint, 0 de 3 785 110 medidas de
alineación**, goldens intactos. Antes/después en `docs/shots/` y detalle en
  `docs/FASE4_GRADIENTES_v1.5.29.md`.
- **El sobrepintado es ahora un número con techo.** El barrido imprime **fills** y **px pintados** por
  redibujado y falla por encima de **200 000 px** (una pantalla entera son 57 600; la peor es FX con 160 481).
  Con esa medida, reescribir el dither de píxel por píxel se midió como **sin ganancia** (sólo tres llamadas lo
  usan, en áreas chicas) y **no se shippeó**: la imagen queda en **582 836 B de 583 612 (776 B libres)** y
  **11 392 B de RAM**, sin gastar esos bytes.

### LOFI gana WAVE = SYNC; PHASE LINE = FOLD queda medido y afuera

Fase 6. Dos opciones de motor se diseñaron para los 888 B de XIP libres. Una entra; la otra entra en flash pero
**rompe el presupuesto de DSP**, y el guardián del CI la rechaza con el número: eso es lo que se publica, no un
presupuesto retocado. (v1.5.28 llevaba las dos y su CI falló en `target_budget.py`.)

- **LOFI · WAVE = SYNC (valor 7), nuevo.** Un pulso cuya fase corre **2 a 5 veces** la de la nota y se reinicia con
  ella —el *hard sync* de los chips, que este motor no tenía—; **DUTY** elige la relación y el modo entra también en
  el morfado (MORPH hacia SYNC lo barre). **`[HOST]`:** suena (rms muy por encima del piso de 16 bits) y difiere de
  los otros siete WAVE en **105–141 %** de la señal, medido sobre las muestras; **0 presets** de fábrica lo usan, así
  que la biblioteca no se movió (**306 golden renders, 0 cambiados**). **`[AC791N]`:** **+64 B** (imagen
  **582 788 B**, **824 B libres**) y **+9 instrucciones** en `lofi_render` (+5 %, dentro del +10 % del
  presupuesto).
- **PHASE · LINE = FOLD: no entra, y está medido.** El *wavefolder* (la suma de las dos líneas envuelta en los
  rieles) costaba **+96 B** de XIP, que entraban, pero **+13 instrucciones por muestra** dentro del lazo de
  `phase_render`: **107 frente a un presupuesto de 94 (+14 %)**, y `phase_render_noise` 125 contra 113 (+11 %). Su
  propio lazo (para no pagarlo con FOLD apagado) no cabe en los 744 B que quedaban: duplicar el lazo son cientos de
  bytes. Descartado por medición, no por gusto. Detalle en `docs/FASE6_OPCIONES_v1.5.29.md`.
- **Tests.** `tests/engine_options_test.c` (nuevo, en `tests/run_tests.sh`) mide sonido, distancia entre valores y
  limpieza de la biblioteca, y además **verifica que FOLD no está**: el control vuelve a terminar en PM, así que
  código, docs y pantalla no pueden separarse.

### PHASE gana LINE = FOLD y LOFI gana WAVE = SYNC: dos opciones de motor, medidas

Fase 6 (cierre). La Fase 3 pedía opciones nuevas **en los motores** y lo que entró fue transversal (LFO, matriz,
tipos de filtro). Estas dos son del motor, audibles, con su control, su test y su medición de XIP. Sin FM-1 en este
entorno: nada de aquí afirma cómo suenan en la unidad.

- **PHASE · LINE = FOLD (valor 3).** La línea 2 ya podía mezclarse, modular en anillo o modular en fase la línea 1;
  ahora puede **plegarla**: la suma se envuelve en los rieles en vez de recortarse, y la cresta vuelve como armónicos.
  El control es el que ya existía: **LINE** elige el modo, **WAVE2** da la forma y **DTN** el detalle de la línea 2.
  Ningún preset de fábrica usa el valor 3, así que la biblioteca no se mueve.
- **LOFI · WAVE = SYNC (valor 7).** Un pulso cuya fase corre **2 a 5 veces** la de la nota y se reinicia con ella
  —un *hard sync* de chip, metálico— con **DUTY** eligiendo la relación. Ningún preset usa el valor 7.
- **Medido `[HOST]`** (`tests/engine_options_test.c`, nuevo): las dos opciones **suenan** (RMS sobre el piso de 16
  bits), **difieren de todos los demás valores de su knob** midiendo sobre las muestras (105–164 % de la señal
  cambiada) y la biblioteca queda intacta: **0 presets** con FOLD o SYNC, **306 golden renders, 0 cambiados**.
  El barrido de UI da **0 hallazgos de lint** y **0 alineaciones fuera** con la cuarta posición de LINE en pantalla.
- **Medido `[AC791N]`:** lo que cuesta en XIP, cada cosa por separado y juntas — **FOLD +96 B**, **SYNC +64 B**,
  **las dos +160 B** (imagen **582 884 B** de 583 612, **728 B libres**). Entran porque la lista de valores se
  extiende detrás de los viejos, sin tocar ids de parámetro, páginas, formato de preset ni el pool.

## v1.5.27 — la canción sale como .MID, y el barrido de encendido deja de recalcular 246 potencias por frame

Fase 6. El firmware no cambia salvo la versión: lo nuevo vive en el **Web Studio** y en el **emulador**, que es donde
estaban las dos oportunidades reales. Sin FM-1 en este entorno: nada de aquí afirma sonido ni panel de la unidad.

- **Nuevo: EXPORT .MID.** El estudio ya renderizaba la canción de 4 compases a `.WAV` y la guardaba como proyecto
  (`.GW1`), pero las notas no salían de la página: para usar el bajo en un DAW había que volver a tocarlo. El botón
  **🎼 EXPORT .MID** escribe un **Standard MIDI File tipo 1** con 480 ppq (una semicorchea = 120 ticks), el tempo de
  la canción, compás 4/4, **una pista por lane no muteada**, los tambores en el **canal 10** con notas GM (36/38/42),
  el pad **sostenido hasta el siguiente ataque** y el bajo/arp una semicorchea. Sin SysEx ni parches: notas, tempo y
  compás, que es lo que un DAW importa sin ayuda. Las lanes muteadas no se escriben.
- **Medido `[HOST]`:** `web/test_mid.mjs`, **64 comprobaciones**, con un lector SMF escrito en el propio test:
  cabecera, división, tempo, una pista por lane viva, canal 10, cada nota cerrada, el pad sostenido, la lane muteada
  ausente, y los bordes (canción vacía, lane de 200 pasos, tempo 0, valor de batería fuera de rango).
- **Optimización: el barrido de encendido.** `animFill` corría **246 `pow()` por frame** (41 LEDs × 6 potencias) para
  un byte con 256 valores posibles. Ahora el color de cada `(tipo, nivel)` se recuerda y los nodos de cada LED se
  buscan una vez. **`[HOST]`**: 0,021 → 0,001 ms por frame en la parte medida (**19,6×**), y los **512** valores
  posibles dan la **misma cadena** que la fórmula (`web/emu/emu_test.mjs`).
- **UI:** la fila de exportación tiene ahora tres botones y envuelve en vez de apretarse; el texto de la sección
  nombra `.WAV`, `.MID` y `.GW1`.

## v1.5.26 — el registro de M-Flasher se lee, la copia se comprueba contra él, y el emulador no divide por píxel

Fase 5. Unidad del dueño con **V16 oficial**: el archivo que entrega M-Flasher no es la copia de 1 MiB, es el
**registro JSON** de la lectura. Cargado en el instalador respondía «that file is not a 1 MiB full copy» — cierto e
inútil. Análisis en `docs/FASE5_BACKUP_V16.md`. **No hay FM-1 en este entorno**: nada de aquí afirma escritura,
sonido ni restauración sobre la unidad.

- **El registro se lee, no se rechaza.** `readMflasherRecord()` acepta el JSON de M-Flasher (modelo `FM-1`, un
  SHA-256 de 64 hex), guarda parte, *chip key*, bootloader, firmware y digest, lo muestra en el estado del bloque
  V16 y lo conserva entre recargas (`qg1.v16rec`). Es metadato: **no autoriza nada** — es un JSON que cualquiera
  escribe y no necesita unidad.
- **La copia se comprueba contra el registro.** Al cargar el `.bin` de 1 MiB su SHA-256 tiene que ser el que el
  registro nombra; si no, `v16DumpMismatch` y no se envía nada. El gate no cambia: copia completa, confirmación de
  restauración, paquete oficial V16, y la parte se sigue leyendo en la unidad antes de borrar.
- **El selector acepta `.json`** (antes sólo `.bin`, así que el registro ni se podía elegir) y el mensaje del gate
  nombra los dos archivos que M-Flasher deja: el `.bin` que queda junto al `.json`.
- **Por qué los dos mensajes del reporte eran correctos:** `Truncated identification reply` es el transporte ante
  una consulta `0x12` que una unidad stock no implementa; «take the complete 1 MiB copy» es el gate de la página.
  Causas distintas, ningún chip inventado. Ver `docs/INSTALL_FROM_V16.md` §7.
- **Emulador: la pantalla no divide por píxel.** `drawScreen` expandía los 57 600 píxeles de un frame con
  `* 255 / 31` y `* 255 / 63`: 57 600 divisiones para 96 valores posibles. Tres tablas construidas una vez con las
  **mismas expresiones** (en `Uint8ClampedArray`, que redondea igual que el canvas) dejan los mismos bytes: los
  **65 536** words del panel dan byte a byte lo mismo, comprobado en `web/emu/emu_test.mjs`. Medido `[HOST]`
  (node): **0,564 → 0,180 ms** por frame, **3,13×**.
- **Tests:** `web/test_v16_path.mjs` **45 → 62** comprobaciones (registro, campos inválidos, digest obligatorio,
  copia contra registro, persistencia, tres idiomas, y 20 000 entradas basura contra el lector: no lanza y sólo
  acepta lo que trae digest).

## v1.5.25 — el panel deja de recibir lo que ya tenía: dos cachés de filas y la barra de fader de FX

Fase 4: segunda optimización, la del **dibujo**. La Fase 1 fue de recursos (RAM, XIP); esta es de trabajo: qué se
escribe en el panel, cuándo y por qué. No baja la tasa de refresco, no apaga animaciones y no esconde nada: gana
trabajo repetido que no producía un píxel distinto. **No hay FM-1 en este entorno**: nada de aquí afirma sonido,
panel ni DAC. Todo es `[HOST]` (arnés de render) salvo el coste, que es `[AC791N]`.

- **El instrumento, primero.** `ui_render --idle` medía un transitorio: un solo frame quieto (el primero, que
  todavía paga el cambio de estado anterior), el **mínimo** de tres (que siempre encuentra dos frames de la misma
  paridad de `ui.frame` y sale 0 para una página que se repinta cada dos) y un reloj detenido (una transición
  temporizada nunca se asentaba). Ahora son tres frames quietos **sumados**, el reloj avanza 33 ms entre ellos, y
  el tope del test baja de 120 000 a **33 000** px. Con el instrumento corregido, v1.5.24 mide **38 272 px** por
  frame quieto; este build mide **29 952** (−22 %).
- **Un relleno olvidaba la tabla entera.** Cuatro franjas negras de 3 × 56 px entre las tarjetas de las páginas de
  operador de FM6 hacían que `lcd_gen++` tirara la tabla de hashes **completa** y el panel recibiera el cuerpo
  entero otra vez: 18 176 px por frame para volver a mandar lo mismo. Ahora `lcd.c` registra **qué filas** y **qué
  rectángulos** tocó cada relleno (hasta ocho por frame) y `gfx.c` olvida exactamente eso.
- **Lo que se midió y no entra, con su razón:** una tabla de ventanas angostas (`cv_side`) para las columnas de
  EDIT (12 768 px de 4 × 57 × 56 por frame) bajaba el total de 29 952 a **21 440** (−44 %), pero cuesta ~3,9 KB de
  código y el `ld` del pi32v2 rechazó el enlace con la imagen de v1.5.24 (*XIP overflowed by 2 960 bytes*,
  `[AC791N]`). Queda medido y escrito en `docs/FASE4_RENDER_v1.5.25.md` §2.2, no escondido.
- **La barra de fader de FX.** Cada tarjeta tiene ahora una barra de **2 px**, de la altura del vúmetro y 4 px a su
  izquierda: ranura oscura con labio, relleno desde el pie en seis bandas de un degradado tema → acento, con el
  mismo valor que el número (0..127 como %). El vúmetro dice lo que el bus **devuelve**; la barra, lo que se le
  **pide** — en un bus silencioso ahora se ve la diferencia.
- **Prueba de que no cambia nada más:** de los **3 056 frames** del barrido (191 pantallas × 22 paletas), **sólo
  cambian los 13 `*_fx`** de la barra nueva; todo el resto sale byte a byte igual. `--regions`, `--bands`,
  `--headers`, `--announce`: en verde; 0 hallazgos de lint y 3 785 084 medidas de alineación sin una sola
  desviación. Detalle y números en `docs/FASE4_RENDER_v1.5.25.md`.

## v1.5.24 — el LFO que cae como un pulso, la resonancia como destino, y dos filtros que el SVF ya sabía

Fase 3: evolución sonora, opciones de edición y modulación. El árbol llegaba con dos techos medidos —125 de 127
ids de parámetro y 1 272 B libres de XIP tras esta fase— así que la fase no agrega parámetros: agrega **valores
nuevos dentro de parámetros que ya existen**, que no rompen ningún formato. Las cuatro incorporaciones cuestan
**196 B** de imagen y 0 B de RAM. **No hay FM-1 en este entorno**: nada de aquí afirma sonido, panel ni DAC.

- **`PLUK`, la décima forma de LFO.** Una caída `(1 - x)^4` dentro de cada ciclo: no es otra onda, es un
  disparo atado al RATE (en tiempo, al paso de la rejilla), para plucks rítmicos de filtro o de amplitud sin
  secuenciador. Se distingue de SAW y de RAMP por la curva y el signo; SLEW e INVERT siguen actuando encima.
  `[HOST]`, `tests/mod_test.c`: empieza en 32 768, a un tercio vale 14 266, y no es RAMP.
- **`RES`, la resonancia del filtro como destino de la matriz.** El corte ya se modulaba; la resonancia no.
  Valor 26 al final de la lista (todo número guardado sigue significando lo mismo), per-block y amordazado a
  0..127, devuelto al terminar el bloque: un barrido de resonancia por LFO o por envolvente, que era lo que
  faltaba para el "wow" de un filtro ácido o una cola que se vuelve silbido.
- **Dos tipos de filtro nuevos: `NOTCH` y `SHELF`.** Salen de las salidas que el state-variable filter ya
  calculaba —`x - bp` y `x + v2`—, así que el filtro de la parte (todos los motores) y el filtro por voz de
  ANALOG y LOFI los tienen, con sus dos curvas en el visor. `[HOST]`, `tests/filter_types_test.c`: NOTCH
  abre un hueco de **−11,9 dB** con los dos extremos dentro de 0,3 dB de unidad; SHELF deja **+6,0 dB** en
  80 Hz y 0,0 dB en 6,4 kHz donde el LP está −32,2 dB.
- **Coste:** `[AC791N]` imagen 582 144 → **582 340 B** (1 272 libres), `.data+.bss` 86 912 B sin cambios, sin
  estado nuevo por voz. Compatibilidad: ningún formato cambia, los presets de v1.5.23 cargan igual y los 306
  renders golden del `regress` siguen idénticos.
- **Lo que no entró, con su razón medida:** un parámetro nuevo por motor (2 ids libres, formato de proyecto),
  un operador/parcial extra en FM6/ADDITIVE (CPU por voz sin unidad para medirlo), y presets nuevos (el banco
  mide el nivel de cada preset renderizándolo y aquí no hay oído). Está en `docs/FASE3_SONIDO_v1.5.24.md` §4.

## v1.5.23 — el acorde que sobrevive, el guardián que elige la nota menos audible, y el monitor que dice qué costó

Fase 2: polifonía, gestión de voces y estrés de audio. Primero los pendientes y las regresiones de la Fase 1 (suite
completa sobre `1b5dc1c`: 6 315 `ok`, 306 renders golden sin cambios), después una revisión medida del asignador, y sólo
las mejoras que un número sostiene. **No hay FM-1 en este entorno**: nada de aquí afirma sonido, panel ni DAC.

- **Un acorde de doce notas ya no pierde ocho en un cambio de sonido que cambia de motor.** `track_t.xp_note/xp_vel`
  guardaban cuatro notas y `engine_block` sólo re-engancha lo que encuentra ahí: la tecla seguía apoyada, su nota no
  sonaba y su note-off caía en el vacío. Ahora la capacidad es `NVOICE` (doce). `[HOST]`,
  `tests/poly_phase2_test.c`: **4 → 12** notas vuelven a sonar en el motor nuevo.
- **La pila MONO / LEGATO tiene doce teclas.** Guardaba ocho y tiraba la primera al llegar la novena: con PRIO LOW o
  HIGH eso era exactamente la tecla que decide la altura. `[HOST]`: con doce teclas apretadas de abajo hacia arriba,
  suena **60** y no 64.
- **Esa capacidad va al lado de los tracks, no dentro, y eso es lo que la hace entrar.** `track_t` mide 1 760 B y lo
  lee todo el instrumento: crecer los arrays en su sitio cuesta **+2 952 B** de XIP (imagen 584 768 B contra un slot de
  583 612: no enlazaba), y al final del struct +2 920 B. El crecimiento no está en los ficheros tocados sino repartido
  por la UI (`nm --size-sort`: `ui_input_frame` +608 B, `gen_make` +210, `graph_signature` +182, `draw_graph` +170 ..),
  un desplazamiento más largo en cada `trk[i].field`. Los primeros slots se quedan donde estaban y el resto vive en
  `xp_more`/`xp_morev`/`ms_more` con tres accesores en línea. `[AC791N]`: imagen **581 816 → 582 144 B** (libre
  1 796 → **1 468**), `.data+.bss` **86 784 → 86 912 B** (11 392 libres). La regla queda escrita en
  `docs/VOICE_POOL.md` §9.
- **El guardián de sobrecarga elige la nota menos audible y actúa una vez por episodio.** Entre las notas sostenidas de
  una parte cortaba la más vieja —el cuerpo sostenido del acorde, lo más audible— y dejaba la recién atacada; ahora
  ordena por `v_energy` (envolvente × nivel de la parte) y la edad sólo desempata, con el bajo y el lead MONO
  protegidos. Y como una voz rebajada tarda seis bloques en irse mientras cuesta lo mismo, un pico más largo armaba
  otro `shed_req`: un episodio podía llevarse tres o cuatro voces. Con una voz en fade, el guardián ya actuó.
  `[HOST]`, `tests/audio_test.c`: las cuatro comprobaciones nuevas.
- **El monitor dice qué costó la carga y qué hace cada track.** `CPU LOAD` gana `SHD n` (las voces que bajó el
  guardián, que estaban sólo en `MENU > ABOUT`) y la fila T1..T6 distingue **sostenida** (acento) de **sólo colas**
  (color de tema, con una barra tan larga como colas queden), en vez de un solo color para "tiene alguna voz". 48
  fotogramas de `tests/ui_pixels.txt` rebaseados (versión y monitor); 3 056 idénticos.
- **Escenas de estrés nuevas** (`tests/poly_phase2_test.c`): ráfaga de 24 note-ons en un bloque (12 suenan, 12 robos
  contados, 0 denegadas, ninguna nota pegada), doce colas de release acumuladas contra un acorde nuevo (las colas
  pagan), seis partes compitiendo (crece la que pide, ninguna se queda sin nota, el pool queda exacto en 12), cambio de
  preset bajo el acorde con FX, y tres ciclos de saturación/recuperación que dejan el pool entero.
- **Lo que el límite físico no deja**, con el número delante: `NVOICE` 16 no se sube (sin unidad no hay margen de CPU
  que medir; `[ESTÁTICO]` DRUM 1 153/1 153); `track_t` no crece más (~25 B de flash por byte); un acorde de doce en FM6
  no es asunto del asignador (`engine_t.poly` 4).
- **Informe completo**: `docs/FASE2_POLIFONIA_v1.5.23.md`, con la tabla antes/después por escena, la del XIP y lo que
  queda por validar en la FM-1 física.

## v1.5.22 — el wavetable guarda media tabla, y las palancas de flash que no pagan

Fase 1 de recursos. Se buscó capacidad nueva **midiendo**, no repitiendo las búsquedas ya cerradas (compresión de
assets, `-Os`, constantes fusionadas): la única palanca que paga es la simetría del banco del wavetable, y las de
flash que aparecieron se descartaron con el número delante.

- **`WT_GEN` guarda media onda por frame.** Cada frame es una suma de senos en fase 0, así que `w[256 − i] = −w[i]`:
  el `.bss` pasa de **12 288 a 6 192 B** y `wt_pts()` lee la otra mitad al revés y negada, con un índice para los dos
  frames de un MORPH. RAM `.data+.bss` **92 880 → 86 784 B**: libre **5 424 → 11 520 B (+112 %)**. Imagen
  **581 720 → 581 816 B** (libre 1 892 → 1 796): el cambio cuesta **96 B** de flash y devuelve **6 096 B** de RAM.
  `[AC791N]`, `nm` sobre el ELF.
- **El coste, medido y aceptado con el número delante**: la tabla reconstruida difiere de la guardada en **≤ 2 LSB de
  ±32 000** (3 246 de 6 144 valores, por el redondeo mitad-arriba del guardado); el render de WAVETABLE en dos presets,
  tres notas y dos segundos difiere **|Δ| ≤ 22 sobre un pico de 98 448 (−73 dBFS)**; `tests/golden.txt` rebaseó **23
  goldens** (los 23 de WAVETABLE) y los otros **283 no se movieron**; `tests/wavetable_test` mide **12 → 14 ns por
  muestra y voz** (+17 % de ese motor, `[HOST]`). El presupuesto estático de destino no tiene línea para `wt_render` y
  las demás no se movieron (`DRUM path 1 153/1 153`, `perf_block 253/254`); `tests/regress` CPU **PASS**.
- **Descartado con medida, no por intuición**: `ld --icf=all` no existe en el toolchain (`unrecognized option`);
  `-fno-inline-functions` da **−6 876 B** de imagen (8 768 libres) pero lleva el camino DRUM de **1 153 a 1 980** de
  coste estático (**+72 %**, límite +10 %), así que no entra: los 10 KB recuperados son las copias inlineadas que
  hacen rápido el audio; los **132** ids de icono se auditaron y los **10** que ninguna pantalla dibuja ya estaban sin
  rasterizar (**0 B** que recuperar).
- **Informe completo**: `docs/FASE1_OPTIMIZACION_v1.5.22.md`, con las dos tablas de medidas, el error de
  implementación que el test cazó (espejar la fase en vez de la tabla) y qué queda bloqueado: **FM6_POLY 6 entra en
  RAM pero no en flash** (3 268 B pedidos, 1 796 libres).

## v1.5.21 — el instalador desde V16, el mensaje que el informe citó, y el Web Studio que vuelve a ser el firmware

Fase 5: cerrar lo que quedaba abierto del camino V16 y del ecosistema. Dos defectos **reproducidos y corregidos**
en el instalador, uno de fidelidad en el Web Studio, y los tres nuevos guardas que impiden que vuelvan. Ninguna
escritura en una unidad física: todo es host (node, x86-64), build AC791N para el tamaño y el emulador para la web.

- **El mensaje exacto del informe ya no se puede componer.** `Chip: this unit did not answer its JEDEC Truncated
  identification reply: nothing was sent.` era una etiqueta única (`sayK(r.ok ? "chipOk" : "chipNo", …)`) con el
  motivo pegado sin separador, y **afirmaba silencio en una unidad que había respondido** (un eco, un cuerpo de 3
  bytes). Ahora hay tres frases: no respondió (`chipNo`), respondió pero no con una identificación (`chipOther`, con
  el motivo), e identificado (`chipOk`); todas terminan en `: ` para que dos frases no se toquen, en los tres
  idiomas. `web/test_v16_path.mjs` comprueba que la composición vieja no existe y que las etiquetas están completas.
- **La copia de 1 MiB del V16 está atada a su unidad.** El registro guardaba tamaño y SHA-256: la copia de una
  unidad satisfacía el gate en **otra** — «la copia de otro no es la copia del dueño». Ahora lleva la identidad que
  estaba verificada al cargarla; el gate rechaza una copia **sin unidad** (`v16Unbound`, pide VERIFY con la unidad
  conectada y volver a cargar) y una **de otra unidad** (`v16WrongUnit`). Sigue siendo un registro, no una prueba
  criptográfica — un firmware oficial no tiene comando de lectura — y así está escrito en `docs/INSTALL_FROM_V16.md`.
  El gate del V16 no se aflojó: copia + confirmación + identidad, y el chip lo sigue leyendo el loader en la unidad.
- **El Web Studio volvía a no ser el firmware.** `web/editor.html` conservaba las listas de presets de antes de
  v1.5.19: el dispositivo simulado respondía **22** sonidos WTABLE donde el firmware tiene **23** (faltaban
  `AURORA DRIFT`, `ROTOR GLASS`, `TIDAL MIRROR`), y el editor ofrecía un catálogo que el instrumento no tiene.
  `tools/sync_editor_mock.py` lo corrigió (5 listas de motor + PATTERNS) y el test de espejo contra
  `build/host/desc.json` da **0 diferencias**. El script tiene ahora `--check`, que no escribe nada y falla; la suite
  lo corre **antes** de los tests web, así que la próxima deriva rompe un build y no una release.
- **Verificado, con la clase de cada medida**: la sintaxis del instalador (`node --check` sobre su
  `<script type="module">` extraído, con los marcadores `/*META*/` sustituidos), `web/test_v16_path.mjs` **45 checks, 0 FAILED**, `node web/test_web.mjs` verde (0
  diferencias contra el firmware), `install_test.py`, `test_pkg_bounds`, `test_backup`, y los bancos de seguridad
  `negative_write`, `backup_audit`, `restore`, `gate`, `release_gate`, `manifest`, `sign`, `sign_request`,
  `key_custody`, `chip`, `preflight`, `transport_gate`, `stock_profile`, `mflasher`, `mflasher_accept`,
  `flash_evidence`, `boot_order`: **todos pasan**. `memory_map_test` **se salta** aquí (no hay `objdump` de JieLi
  en este entorno): se informa como no ejecutado, no como verde.
- **Sin optimización nueva, y con el número que lo justifica**: imagen **581 720 B** (1 892 B libres) y `.data+.bss`
  con 5 424 B libres; el único coste que las medidas señalan como optimizable es el frame quieto (**109 056 px**
  escritos en 201 escenas, 0 cambiados, ~542 px por escena). Recortarlo pide análisis del panel, no un cambio a
  ojo, y no se hace en una fase de cierre.

## v1.5.20 — la interfaz, medida: los encabezados de HOME, la fila de MENU dicha, y pruebas de UI para lo nuevo

Fase 4: revisar la interfaz entera contra los defectos que el informe de la fase 2 dejó abiertos (#106), cerrar el
pendiente de accesibilidad que la fase 3 documentó (#105) y dejar las dos cosas medidas por tests que corren con el
firmware de verdad, no por funciones aisladas. Nada de esto es una medición de la FM-1 física: es host x86-64 (los
bancos de `tests/`) y build AC791N para el tamaño.

- **#106, el primer síntoma, medido donde estaba.** Nuevo `tests/ui_render --headers`: dibuja HOME 2/5 VISUALS,
  3/5 OVERVIEW, 4/5 MIXER y 5/5 PERFORM dos veces con un paso del reloj entre medio, en **FLAT y LINE**, **con ANIM y
  con ANIM OFF** — 16 combinaciones — y cuenta los píxeles que cambian dentro de la banda de la cabecera (filas
  0..`H_HEAD`). Resultado: **0 px en las 16**. El encabezado ya no parpadea. El mismo modo mueve el tempo a propósito:
  escribe **74 px, todos dentro de la cabecera y 0 fuera**, así que la cabecera responde y el tempo no invade nada.
- **#106, los otros tres síntomas, re-verificados en el código actual** (no en la documentación): `--regions` A
  (el pop-up del preset sobre el MIXER no deja un píxel fuera de su caja y el panel vuelve exacto al cerrarse), B (la
  tinta del tempo no comparte un solo píxel con las almohadillas de T3/T4, en seis columnas y en LARGE) y C (un cambio
  de pista con ALGORITHM escribe **872 px, todos en las dos columnas cuya selección cambia, 0 en el resto**, y el
  frame que deja es idéntico a un redibujo entero). `--bands`: un paso del ambiente escribe **6 480 / 7 680 / 7 440 px**
  según la página y deja el frame exacto; `--idle`: una escena quieta cuesta **109 056 px** (tope 120 000).
- **#105, el pendiente cerrado: la fila de MENU se dice.** El accessor que faltaba existe (`menu_items.c`,
  `menu_ann_row`): tab, nombre y valor de la fila (`DISPLAY COLOR QUANTUM`, `CONTROL ANIM OFF`, `AUDIO TRACKS 6 FULL`).
  Al moverse la selección se anuncia **en el acto**; al girar su perilla, **una sola línea cuando el valor lleva
  250 ms quieto** (no un mensaje por paso, que es lo que el protocolo pide); WHERE repite pantalla y fila.
- **Y se prueba por el camino real.** `tests/ui_test.c` compila ahora `announce.c` donde `felucca.c` lo incluye (antes
  de `ui.c`), con una captura en lugar del cable USB-MIDI; `tests/ui_render --announce` conduce `ann_ui` sobre el
  firmware de verdad: silencio sin lector, HELLO → `HOME MAIN`, MENU → `MENU` + la fila, otra fila → dicha, perilla →
  nada en el acto y la línea al asentarse, WHERE → las dos, cerrar MENU → `HOME MAIN`. **Pasa.**
- **Costo**: la imagen AC791N pasa de 581 296 a **581 720 B** (1 892 B libres) y `.data+.bss` no crece. La
  funcionalidad nueva es una línea de texto por evento: no hay píxeles nuevos, ni animación nueva, ni RAM de audio.
- **Medido, no supuesto**: `ui_render` completo con el arnés nuevo — **191 pantallas × 22 paletas + el barrido de
  páginas/valores: 0 hallazgos de lint, 74 lugares y **3 783 981 medidas de alineación, 0 fuera por 1 px**,
  GREY y MONO limpios; y un paso del ambiente / de los medidores deja el frame exacto (`--bands`).

Lo que **no** se hizo: no se añadió ninguna pantalla ni control nuevo (la petición era corregir y medir, y cada
control nuevo cuesta píxeles y RAM que hoy no hay), y **no se probó con el lector físico de Charles Vestal ni con un
lector de pantalla del sistema**: el banco comprueba los bytes y la secuencia, no la locución. La voz y la página son
del lector.

## v1.5.19 — la biblioteca, medida y nivelada; tres sonidos nuevos

Fase de presets. No se añadió motor ni parámetro: la tabla de ids sigue en 125 de 127 y un motor nuevo pide RAM de
voz y, sobre todo, oído en la unidad. Lo que sí se hizo es lo que se puede **medir**: pasar la herramienta de
nivelación existente sobre la biblioteca entera y añadir sonidos con su nivel medido, no a ojo.

- **Nivelado: 11 de los 285 presets de fábrica cambiaron de nivel** al volver a pasar `tests/preset_levels` +
  `tools/gen_preset_levels.py` (que corrige desde los niveles que ya había, sin contar la corrección dos veces). Medido
  en el host, con los 285: mediana **−18,50 dB RMS**, recorrido **12,6 → 12,2 dB**, presets a más de 6 dB de la mediana
  **3** (los mismos: `AURORA FM` −29,7, `STATIC HAZE` −29,4, `GEIGER CNT` −28,8).
- **El techo del fader es el límite, y queda dicho con nombre y cifra.** El generador lista los sonidos que piden más
  de los **+13,5 dB** que el nivel puede dar: además de los tres de arriba, `FM6 MARIMBA` (+3,3), `STEEL DRUM` (+2,7),
  `DEEP MARIMBA` (+3,4), `ADDTV GLASS BELL` (+5,7) y `CRYSTAL PLUCK` (+5,9). Arreglarlos es cambiar el **sonido**, no el
  número, y eso no se hace sin oírlo: no se tocaron.
- **Tres sonidos nuevos**, uno por motor, con identidad propia y sin duplicar nada (el test de la biblioteca lo
  comprueba: 0 nombres repetidos, 0 duplicados exactos, 0 fuera de rango, 0 en silencio):
  `ANALOG · AURORA DRIFT` (pad lento, cut que respira con la rueda, chorus y reverb largos), `PHASE · ROTOR GLASS`
  (dos fases girando una contra otra, LFO al pitch) y `WTABLE · TIDAL MIRROR` (la tabla morphando bajo el cut, HSIN al
  filtro). Medidos con el mismo arnés: **−18,0 / −16,4 / −17,4 dB RMS** y picos −6,4 / −5,4 / −7,0, es decir dentro de
  ±3 dB de la mediana de la biblioteca.
- **Ningún preset DRUM-X nuevo** (pedido explícito) y **ninguna normalización indiscriminada**: se corrigió el nivel
  de lo que estaba fuera de sitio y se conservaron las diferencias de dinámica y de transitorio (los picos de los tres
  nuevos van de −5,4 a −7,0 dB, no todos iguales).
- **Medido**: `preset_audit` sobre **288** presets: 0 en silencio, 0 nombres repetidos, 0 duplicados exactos, 0 fuera
  de rango; `preset_levels` sobre los 288 para los niveles. Clase host x86-64. Nadie escuchó estos sonidos: la unidad
  no está aquí y no se afirma cómo suenan.

## v1.5.18 — cuatro operadores musicales en SEQ > TOOLS, y la biblioteca medida antes de tocarla

Fase de evolución musical. La incorporación nueva son **cuatro operadores** en la página que ya existía (SEQ > TOOLS),
elegidos porque el instrumento ya tiene arp, acordes, escalas, progresiones, fuga, automatización por paso, matriz de
modulación y punch-ins: lo que faltaba no era otro parámetro, sino **herramientas para trabajar sobre el patrón**.

- **EUCLID, MIRROR, THIN, CRESC** en SEQ > TOOLS, con el mismo KNOB 1 (operador), KNOB 2 (cantidad), GO y UNDO que
  DUP / HALF / ROT / REV / CLEAR, y la misma regla que la página ya imprimía: **el tiempo, la compuerta y la
  probabilidad de cada paso no se tocan, sólo cambia lo que suena**.

  | operador | qué hace | cantidad |
  |---|---|---|
  | **EUCLID** | reparte *las mismas notas* del patrón sobre onsets regulares (`(i·k) % len < k`, k = cantidad × 2) y las asigna en su orden; el acento y el golpe de línea viajan con su nota | HITS 2..16 |
  | **MIRROR** | invierte la línea alrededor de la tónica de la pista (`2·(60+ROOT) − n`) y la lleva a su escala, nota por nota; los acordes conservan su tamaño | IN KEY |
  | **THIN** | saca uno de cada N onsets: menos densidad, la misma forma | 1 OF 2..8 |
  | **CRESC** | un crescendo desde el primer onset hasta el último, que llega a velocidad plena (una frase que arriba) | RISE 12..96 |

  La página dice ahora la cantidad en la unidad de cada operador y una línea con lo que va a hacer, en vez de un
  "xN" genérico: nada se aplica a ciegas sobre el patrón que la propia página dibuja.
- **Los operadores viven en `seq.c` (`seq_tool_apply`) y no en la página**, así que lo que giran los knobs es exactamente
  lo que verifica el test: `tests/seq_tools_test.c` recorre los diez operadores sobre una frase conocida (arpeggio,
  acorde y golpe de línea) y comprueba el patrón resultante y lo que **no** debe moverse. Clase **host x86-64**; sin
  audio ni panel. Medido, no afirmado: EUCLID ×3 sobre 16 pasos da onsets en 0 3 6 8 11 14 con las seis notas intactas
  y en orden; MIRROR convierte 60 64 68 72 en 60 56 52 48 (y el acorde 67/71 en 53/49), todo dentro de la escala;
  THIN 1 de 2 deja 0 6 10; CRESC ×4 sube 79 → 95 → 103 → 111 → **127**.
- **Coste:** ningún parámetro nuevo (la tabla de ids está en 125 de 127), ningún byte de `.bss`, ningún cambio en los
  formatos de proyecto ni de preset: los operadores son acciones transitorias sobre `t->step`, con el UNDO que ya
  existía. Build AC791N: imagen **580 280 → 581 160 B** (+880 B de código, **2 452 B libres**), `.data+.bss`
  **92 832 B** sin cambios (5 472 libres).
- **La biblioteca, medida antes de tocarla.** `tests/preset_levels` renderiza los **285** presets de fábrica y
  `preset_level.c` ya nivela cada uno al cargarse. Medición de esta fase: mediana **−18,5 dB RMS**, y las medianas por
  motor planas (−18,5 / −18,6 en los diez motores). Quedan **tres** excepciones de 10–11 dB por debajo, todas con el
  fader ya en su tope (127): `AURORA FM` (FM6, −29,5 RMS y **−19,2 de pico**, el único realmente flojo de señal),
  `STATIC HAZE` y `GEIGER CNT` (NOISE, picos −4,5 y −3,5: texturas dispersas, no faders agotados). El generador ya
  documenta que un sonido más flojo que el tope del fader **no se arregla con el número, sino con el sonido**, y eso
  pide oído en la unidad; por eso **no** se reescribieron presets a ciegas en esta fase. Queda dicho, con nombre y
  cifra, en vez de maquillado.
- **Lo que no se hizo, y por qué:** no se añadieron presets DRUM-X (pedido explícito) ni se rediseñó la biblioteca
  (no hay FM-1 aquí y el propio proyecto prohíbe afirmar cómo suena algo que no se ha oído). No se añadieron
  parámetros nuevos: la tabla de ids de 7 bits que comparten los proyectos, los presets y la automatización está en
  125 de 127, y ningún operador necesita uno.

## v1.5.17 — las seis pistas y las doce voces, juntas y medidas; el gobernador de carga deja de cortar colas

Fase de audio: hacer que el pool sostenga **seis pistas a la vez y doce voces reales**, y comprobarlo contando lo que
el asignador cuenta (voces activas y con compuerta, en release, robadas, denegadas, pico) en vez de mirar un número
en pantalla. Lo primero que apareció fue un defecto de verdad, y está corregido en su causa.

- **EL GOBERNADOR DE CARGA CORRÍA COLAS DE RELEASE SIN NECESIDAD.** `audio.c` pone `load_tight` cuando el render de
  media trama pasa del **65 %** (el guardián de verdad, el que hace algo, es el de audio: dos medias tramas seguidas
  por encima del **80 %** y se desvanece una voz). Pues bien, con `load_tight` activo, `voice_room()` y
  `voice_alloc()` **mataban una cola en release aunque el presupuesto estuviera vacío**, para que el número de voces
  no subiera. El razonamiento no se sostiene: una cola en release renderiza exactamente las mismas muestras que la
  nota que ocupa su sitio, así que el corte **no ahorraba ni un ciclo de CPU** — sólo impedía acercarse al tope que
  el usuario eligió — y lo pagaba con una caída de envolvente cortada 6 ms antes de tiempo, que es justo lo que se
  oye como un chasquido en el decaimiento. Ahora, con hueco en el presupuesto, la nota usa el slot libre y la cola
  sigue sonando; con el presupuesto **lleno** (12 de 12) se recicla una cola, que es el comportamiento correcto y
  sigue igual. Medido en `tests/poly_six_test.c`, escenario E, con el gobernador puesto y cuatro colas sonando:
  **antes** release 4 → 3, robos 0 → 1; **después** release 4 → 4, robos 0 → 0. El guardián del 80 % no se toca.
- **UN TEST NUEVO QUE MIDE LA FRASE ENTERA, no una pista aislada** (`tests/poly_six_test.c`, cableado en
  `tests/run_tests.sh`). Cada escenario cuenta voces en el pool, clase host x86-64, fuentes reales:

  | escenario | lo que se pide | medido |
  |---|---|---|
  | **A** seis pistas vivas, dos notas cada una | 12 sonando | **gated 12, busy 12, pico 12**, robos 0, denegadas 0, y las seis pistas con 2 voces propias; ocho bloques después siguen las 12 |
  | **B** las doce en una sola pista | 12 sonando | **12**, la ventana crece a 12 prestando slots ociosos (pool 17), sin tocar ninguna voz que suena; al soltarlas, otra pista recupera su voz |
  | **C** reparto 4+4+2+1+1 | 12, cada una en su pista | **4 / 4 / 2 / 1 / 1 / 0**, robos 0, denegadas 0 |
  | **D** seis de doce sueltan | 6 con compuerta + 6 en release | **6 + 6, pico 12**, robos 0 |
  | **D2** al tope llega una nota más | se recicla una voz en release, no una sostenida | gated 11→12, release 1→0, **robo 1 sobre la cola**, ninguna nota sostenida tocada |
  | **E** gobernador de carga con hueco | no cortar colas | release 4→**4**, robos **0** (antes del fix: 3 y 1) |
  | **F** las doce con chorus, delay y reverb en cada pista | 12 sonando | **12**, robos 0, pico del bloque 15 679 (sin llegar al riel) |
  | **G** cambio de preset con el acorde de doce sostenido | no perder voces ni silencio | gated **12**, robos 0, pico del bloque 13 325 |
  | **H** TRACKS 4 (el valor de fábrica) | T5 y T6 duermen y lo dicen | T5 0, T6 0, `asleep_hit` 1, **robos 0**: pedir una nota a una pista dormida no es robar nada |
  | **S** saturación: dos notas más sobre las doce | suenan, no se deniegan | gated 12, **robos +2**, denegadas **0**, pico 12 |
  | **I** coste del cuadro completo | reloj del host | 4 pistas / 8 voces **6,9 µs/bloque**; 6 pistas / 12 voces **8,9 µs/bloque** (+29 %), mejor 8,4 µs, plazo **725 µs** |

  La distinción que pedía el encargo queda explícita en cada línea y en el propio test: **pedidas** (lo que entra),
  **asignadas** (lo que el pool da), **activas con compuerta**, **en release**, **robadas** y **pico**. No se usa el
  pico como si fuera "voces sonando": el pico lo toma `voice_stats()` una vez por trama, y el test lo llama antes de
  leerlo (la primera corrida, con el pico leído sin actualizar, daba 0 — el monitor no mentía, el test sí).
- **Lo que NO se cambió, y por qué.** `trk_live` sigue en **4** de fábrica: el modo **TRACKS 6** existe en el menú,
  está medido en el pool y ahora tiene test propio, pero subir el valor de fábrica es una decisión que necesita una
  medida en la unidad (no hay FM-1 en este entorno). Los topes por motor tampoco se mueven: `FM6_POLY 4` porque
  llevarlo a 6 cuesta ~3 552 B de `.bss` y sólo hay 3 268 B de imagen libres además del suelo de 4 KiB de RAM;
  `eng_formant.c` `.poly = 4` porque su render es el más caro por voz (238 en el presupuesto estático). Son topes de
  CPU/RAM declarados, no límites escondidos: el pool reparte los demás slots entre las pistas que los piden.
- **Recursos (build AC791N).** El fix no añade estado y quita dos ramas: imagen **580 344 → 580 280 B** (−64 B,
  **3 332 B libres** del slot de 583 612), `.data+.bss` **92 832 B** (5 472 libres, piso de 4 KiB). El test nuevo es
  de host y no entra en la imagen. Emulador: wasm 1 280 848 B, v1.5.17, sha256 `471e7495…`.
- **Lo que sigue sin verificarse, y se dice:** todo esto es clase **host x86-64** (y **build AC791N** para el tamaño).
  No hay FM-1 aquí: el pop de encendido, la latencia audible, los clicks y la polifonía *oída* no están medidos en el
  panel, el DAC ni el amplificador, y no afirmo que este cambio elimine ningún pop físico. El coste en la unidad no
  es el del host: 8,9 µs de host no se trasladan a ningún número de AC791N.

## v1.5.16 — la aritmética que el compilador podía leer de otra forma, y un instrumento que medía ruido

Fase de auditoría y corrección con una regla: **nada de lo que ya estaba medido se toca sin volver a medirlo**. El
firmware suena exactamente igual que v1.5.15 (303 renders dorados, **0 cambiados**, bit a bit) y se dibuja igual
(3 056 fotogramas, **0 distintos**). Lo que cambia es lo que había debajo.

- **CATORCE operaciones con comportamiento indefinido (UB), en el camino de audio y en el de dibujo.** Todas
  alcanzables desde los propios controles del aparato y todas invisibles en el host porque la suite desactiva a
  propósito `signed-integer-overflow` y `shift` (el código está lleno de desplazamientos Q15 y si no, la suite se
  ahoga). Compilando `ui_test` con **UBSan estricto + bounds** salieron a la luz:

  | sitio | qué era | corrección |
  |---|---|---|
  | `fx.c` clip del master (×2) | `negativo << 2` | `* 4`, mismo valor y definido |
  | `fx.c` clave de la cola de eco | `tn << 1`, `tn` puede ser negativo | `tn * 2` |
  | `eng_noise.c` CRUSH | `(y >> k) << k` sobre negativo | `(uint32_t)(y >> k) << k`: mismo piso, mismos bits, mismo coste |
  | `eng_noise.c` DRIFT | `tg << 4` | `tg * 16` |
  | `fm6_core.c` escala de velocidad | `(neg >> 3) << 4` | `* 16` |
  | `fm6_core.c` seno del operador | `fm6_sin(...) << 8` | `* 256` |
  | `eng_wheel.c` salida | `voice_amp(...) << 1` | `* 2` |
  | `eng_fm6.c` nivel del patch | `lvl << 16` | `* 65536` |
  | `eng_analog.c` paso del MORPH | `(mpos1 - mpos0) << 8` | `* 256` |
  | `drumx_voice.c` fase de la capa (×2) | `o * lx` desborda `int` al envolver la fase | producto **sin signo**: el envolvente es unsigned a propósito |
  | `drumx_voice.c` corte de DC | `v2 << 8` | `v2 * 256` |
  | `ui_tech.c` curva del DIST (×3) | dos fases `sine_i((uint32_t)(v * 28000))` y el cúbico TAPE en 32 bits | fases en 32 bits sin signo; el cúbico en 64 bits **como en el audio** (la curva dibujaba un pliegue que el audio no hace) |

  Después de las correcciones, la misma compilación estricta da **0 avisos** en toda la suite de UI. El audio queda
  bit a bit idéntico porque cada cambio es el mismo valor en aritmética definida, no un cambio de fórmula.
- **Un instrumento de medida medía ruido.** El chequeo de alineación de los keycaps (`tests/ui_render.c`,
  `al_keycaps`) leía `KC_DATA` **como nibbles sueltos**, pero ese flujo está **codificado en Huffman**
  (`tools/gen_aa_keycaps.py`, decodificado por `cv_alpha_hc`): recorría el array más allá de su final (lo cazó ASan
  al pasar el barrido completo por el sanitizador) y las cifras de "0 desalineados" de esa fila no significaban
  nada. Ahora decodifica con el decodificador real, mapeando el nibble al píxel. Resultado con el instrumento
  arreglado: **0 desalineados de verdad**, en 3 781 017 medidas de 74 sitios.
- **La suite gana el guardián que faltaba:** `ui_test` compilado con `-fsanitize=undefined,bounds` y
  `halt_on_error=1` (sección de sanitizadores de `tests/run_tests.sh`). Cualquier UB de esta clase vuelve a fallar
  en el acto, con archivo y línea, en vez de viajar al binario de destino.
- **Recursos, medidos y no movidos.** Imagen **580 328 → 580 344 B** (3 268 B libres del slot: +16 B por el cúbico
  en 64 bits y los productos sin signo; el resto del árbol no crece). `.data+.bss` **92 832 B**, igual (5 472
  libres, piso de 4 KiB). Presupuesto estático de CPU: **todo dentro**, `noise_render` vuelve a 218 (se probó con
  máscara y costaba +2 %: se dejó la pareja de desplazamientos, con el izquierdo sin signo). Se midieron y **se
  rechazaron**, con la imagen byte a byte idéntica: `-fmerge-all-constants`, `-fno-unroll-loops`, el *machine
  outliner* de LLVM (no compila) y `noinline` en `draw_column`/`draw_column_tall`/`param_format` — ninguna de esas
  funciones se estaba inlineando, así que no había nada que recuperar por ahí. El flash está en su óptimo para
  este compilador: lo que queda no es grasa, es el producto.
- **Lo que sigue sin verificarse:** todo esto es clase *host* (UBSan/ASan x86-64, `ui_render`, `regress`) y *build
  AC791N*. No hay FM-1 en este entorno, no escribí en ninguna unidad y no afirmo nada medido en el panel ni en el
  DAC físico.

## v1.5.15 — HOME deja de reenviar el cuerpo entero, y el pop-up deja de pintar sobre el MIXER

Los dos defectos que v1.5.13 dejó abiertos, corregidos en su causa. Ninguno baja la tasa de refresco, apaga una
animación ni recorta la calidad: lo que cambia es cuánto se le manda al panel y desde qué ventana se dibuja.

- **PARPADEO DE HOME 2–5: el panel recibía 48 480 px por cada paso de ambiente para 32 px que cambiaban.** Medido en
  el host (`ui_render --bands`): un paso de las brasas (`hv_air`, `ui.frame / 8`) o del pulso del strip movía 32–40
  px, y el cuerpo se reenviaba entero porque `gfx.c` sólo sabía saltarse una banda **idéntica de arriba a abajo**.
  Ahora `cv_blit` guarda un hash **por fila del panel** (960 B de `.bss` + 240 B de validez) y manda sólo los tramos
  de filas cuyos píxeles difieren. Medición del mismo paso, antes → después:

  | pantalla | antes | después | px que cambian |
  |---|---|---|---|
  | HOME 2/5 OVERVIEW | 48 480 | **6 480** | 32 |
  | HOME 3/5 MIXER | 48 480 | **7 680** | 40 |
  | HOME 4/5 PERFORM | 48 480 | **7 440** | 38 |
  | GLO MIXER (un medidor) | 29 280 | **4 636** | 324 |

  El test comprueba además lo que una optimización así puede romper: el frame que deja el dibujo incremental es
  **pixel por pixel** el de un redibujo entero (0 px distintos en las cuatro pantallas), y el paso **sigue animando**
  (un arreglo que congelara las brasas fallaría esa aserción). El `--idle` sigue en 109 056 px, el piso de v1.5.6.
- **Dos causas de tráfico que aparecieron al medir, también corregidas:** el **cap** del cuerpo de HOME era de 26
  filas (20..46) cuando la primera pasada repinta 26..114 en el mismo frame: 20 filas pintadas y tapadas, y como
  los dos ventanas no coincidían píxel a píxel, esas filas se mandaban **dos veces** (ahora 6 filas, 20..26); y
  `CV_SENT_MIN_H` era 64, así que una banda de 26 filas no se comparaba nunca (ahora 16: 6 240 px por paso menos).
- **NOMBRES DE PRESET SOBRE GLO MIXER: el pop-up componía sobre un canvas ajeno.** `graph_preset_pop()` dibujaba su
  placa (208 × 86 en 16, Y_GRAPH+18) sobre **el canvas que la página hubiera dejado** y lo enviaba como banda entera
  de 240 × graph_h. En el MIXER las columnas se dibujan una a una y no llenan ese buffer, así que lo que se enviaba
  era un canvas **viejo** —los nombres de preset de un dibujo anterior— con la placa encima. Eso es exactamente lo
  que se veía. Ahora el pop-up abre **su propio canvas del tamaño de su caja** y lo envía ahí y sólo ahí.
  Prueba nueva en `ui_render --regions`: con el pop-up arriba, **0 px del panel tocados fuera de su caja**; al
  cerrarse, el MIXER vuelve a su frame exacto (panel, caja, zona dB y cabecera: 0 px distintos), y el caso C
  (cambio de pista) sigue dejando el frame de un redibujo entero.
- **Coste:** imagen 580 056 → **580 328 B** (3 284 B libres del slot), `.data+.bss` 91 648 → **92 832 B** (5 472
  libres, piso de 4 KiB respetado). El hash por fila es el mismo trabajo que el hash de banda que sustituye (una
  palabra por vez) más un bucle de filas: no toca el DSP, cuyas líneas base de CPU siguen pasando sin regenerarse.
- **Los 3 056 goldens de UI quedan idénticos** salvo las 48 líneas con la cadena de versión: la corrección no mueve
  un píxel. El lint de layout: 0 hallazgos.
- **Lo que sigue sin verificarse:** esto es clase *host* (`ui_render --bands`, el modelo del panel del harness) y
  *build AC791N*. No hay FM-1 en este entorno y no escribí en ninguna unidad: el parpadeo y la legibilidad en el
  panel físico siguen sin medirse en hardware, y no lo afirmo.

## v1.5.14 — El slot de flash: 2 480 B recuperados del mapa de símbolos, sin tocar el DSP

- **Punto de partida medido** (build AC791N, `build/felucca.map` de v1.5.13): imagen **582 536 B** de 583 612,
  **1 076 B libres**. Después: **580 056 B**, **3 556 B libres** — el margen se multiplica por 3,3.
- **Lo que se midió y NO daba nada** (para que nadie lo vuelva a buscar):
  - *Tablas duplicadas:* ningún blob de ≥64 B aparece dos veces entre los 2 303 símbolos de `.text`
    (hash de los bytes reales del ELF, no de los nombres): **0 B** recuperables por fusión.
  - *Flags:* el build ya compila con `-Oz` (elegido por medición: `-Os` mete 20 divisiones de hardware en el loop
    de `analog_render`, +355 % de presupuesto; `-O2` no entra en XIP), `-ffunction-sections`, `--gc-sections` y
    `-flto`. Probado además `ld --relax`: **0 B** de diferencia (no implementado para pi32v2).
  - *Relleno de alineación:* 27 340 B (4,9 %) entre símbolos, pero es alineación de instrucciones y labels: sólo se
    recupera cambiando alineación, y eso sí toca el tiempo del DSP. No se tocó.
  - *Datos comprimibles:* `UAC_FIR` (7 680 B, entropía 7,31 bit/B), `FM6_EXP2` (4 100 B, 7,84), `AI12_DATA`
    (3 855 B, 7,85): densos, sin rodata muerta que recortar.
- **Lo que sí se recuperó: los glifos que ninguna pantalla dibuja.** Las caras S y M se generaban con todo el
  ASCII imprimible (32..126) más diez extras Latin-1. Un barrido de todos los literales de `firmware/src` y de las
  tablas generadas muestra que **ni un string usa** `© ° · Ä Ö Ü ä ö ü` ni `~` (126): nueve extras y el último
  código de la fila. `tools/gen_aa_font.py`: `EXTRAS` pasa a `[0x2026]` (los puntos suspensivos, que `ui_draw.c` sí
  dibuja) y la fila de S/M a `(32, 125)`. La cara L no cambia.
  - Generador: S 12 824 → **11 242 B** (−1 582), M 8 210 → **7 285 B** (−925); kerning de S 592 → 479 pares.
  - Imagen: −2 480 B. Es **sólo dato**: no se tocó una instrucción del motor, del mezclador ni de los FX, y las
    líneas base de CPU de la suite (`tests/cpu_baseline.txt`, `cpu_ns_baseline.txt`) siguen pasando sin regenerarse.
  - Los 3 056 goldens de UI quedan **idénticos** salvo las 48 líneas que llevan la cadena de versión: ningún glifo
    se movió, ningún texto perdió una letra.
- **La guarda, porque un glifo que falta no crashea: dibuja `?`** (`gfx.c glyph()` cae al `?`). `tests/font_charset_test.py`
  lee las caras reales del header generado (`first`, `last`, extras) y todos los literales de `firmware/src` +
  `build/gen`, y falla si un carácter no tiene glifo en S o M. Probado con una sonda: un `"A~B"` en el árbol hace
  fallar el test con `U+007E '~' has no glyph in AF_S`; sin la sonda pasa. L queda a cargo del chequeo en ejecución
  que ya tenía `tests/ui_test.c`. El test está en la suite.
- **Verificación:** `make_release.py --check` OK (paquete, ambos SHA256SUMS, `versions.json` e installer describen un
  solo artefacto), suite de host completa en verde, RAM sin cambios (`.data+.bss` 91 648 B de 98 304).

## v1.5.13 — GLO MONITOR: la fila que se escribía fuera de su buffer, y la polifonía medida

- **GLO MONITOR, la causa del texto que se superpone después de una secuencia.** No era un problema de refresco
  ni de formato de los campos: la página construía cada fila concatenando con
  `str_cpy(b + str_len(b), …, sizeof b - str_len(b))` alrededor de un `fmt_int` **sin límite**. La fila
  `POLYPHONY` crece mientras el secuenciador corre (STEALS y DENIED cuentan y ganan dígitos) y con los contadores
  altos mide 54 caracteres sobre un `char b[48]`: `fmt_int` escribía más allá del final del array, `str_len()`
  recorría memoria que no era la cadena, y `sizeof b - str_len(b)` se daba la vuelta a un `size_t` enorme, así que
  el siguiente `str_cpy` escribía **detrás** del buffer. Lo que se veía como campos encimados era memoria vecina
  pisada. Ahora las siete filas se arman con `rowb.h`, que lleva su longitud y nunca escribe más allá de la
  capacidad: una fila que no entra se corta, pero sigue siendo una cadena. `char b[64]` cubre la más ancha (63).
- **…y la fila `POLYPHONY` imprime el presupuesto que rige, no `NVOICE`.** Decía `12` aun con LOAD ECO puesto, que
  es exactamente la cifra contra la que un instrumentista compara un acorde. `voice_budget()` (voice.c) devuelve el
  tope del asignador — el ajuste del usuario recortado por ECO — y el monitor lo muestra.
- **La polifonía, medida y con números.** `tests/poly_budget_test.c` (host x86-64, las fuentes reales) mide voces
  *que suenan* (activas y con gate), no un contador dibujado:

  | caso | resultado medido |
  |---|---|
  | 8 / 10 / 12 notas sostenidas en un part (ANALOG) | 8 / 10 / 12 con gate, 0 robos, 0 notas denegadas |
  | ajuste VOICES en 10 / 8 | el tope aplicado es 10 / 8, no 12 |
  | LOAD ECO | **6 voces en total y 4 por part** (`ECO_POLY`), que es el "6–7 voces" del reporte |
  | FM6 (6 operadores por voz) | 4 por part (su `engine_t.poly`), por diseño |
  | cuatro tracks, 3 notas cada uno | 12 voces, ninguna denegada, el bloque las renderiza |
  | acorde de 6 + 6 con la mitad soltada | 6 suenan, 6 en release, el acorde nuevo entra sobre las colas |
  | cambio de preset con 8 notas sostenidas | el bloque sigue sonando (energía > 0), el acorde no se recorta, 0 voces quedan |
- **Lo que este release NO arregla, y por qué.** El parpadeo de HOME 2–5 y los nombres de preset sobre GLO MIXER
  siguen abiertos. Lo que hay, medido o leído en el código, es esto: (a) las tres pantallas de HOME pintan el
  cuerpo entero en tres blits (26 + 88 + 88 filas) cuando **cualquier** cosa de la firma se mueve, y la fase de las
  brasas (`ui.frame / 8`) está en esa firma mientras la animación está en FULL: el cuerpo completo se reenvía al
  panel ~7,5 veces por segundo sin que haya nada nuevo que leer, y con el secuenciador andando la tasa de frames
  baja, que es el "parpadeo más lento" del reporte; (b) en GLO MIXER el nombre de cada columna ya está recortado a
  su ancho (`track_col6`, primer palabra y letras que entran) y el pop-up de preset es una placa opaca de 1,5 s con
  su propia invalidación al cerrar: lo que queda por identificar es qué camino de dibujo deja tinta sobre el panel
  sin invalidarlo. Las dos son de la clase "hardware/emulador con captura de frames": el lint de layout y los 3056
  goldens de esta suite pasan, así que no hay una prueba de host que las reproduzca todavía, y prefiero decirlo a
  inventar un arreglo que no puedo verificar en el panel.
- **Pruebas nuevas en la suite:** `ROWB` (con ASan/UBSan, filas al peor caso y guardas detrás del buffer) y
  `POLY BUDGET` (los casos de la tabla). Ambas en `tests/run_tests.sh`. La suite completa pasa; los goldens de UI se
  regeneraron sólo por la cadena de versión (`tests/ui_pixels.txt`, 48 líneas de 3056 frames).
- **Flash:** imagen **582 536 B** de 583 612 (1 076 B libres), `.data+.bss` **91 648 B** de 98 304 (6 656 libres),
  paquete 610 933 B, identity `FM-1_965`. El builder acotado ocupó 148 B menos que las concatenaciones que reemplazó.

## v1.5.12 — Las cinco ventanas de RAM, medidas y vigiladas

- **La auditoría, con números del linker real** (`build/felucca.map`, build AC791N de esta versión):

  | ventana | usado | tamaño | libre |
  |---|---|---|---|
  | LOWBUF | 30 564 B | 32 768 | 2 204 |
  | RAM (.data+.bss) | 91 628 B | 98 304 | 6 676 |
  | POOL | 311 256 B | 327 680 | 16 424 |
  | RAMTEXT | 3 792 B | 24 576 | 20 784 |
  | **NOINIT** | **15 572 B** | **15 696** | **124** |

  La ventana más ajustada no era la flash (928 B) sino **NOINIT, con 124 B**, y no tenía ninguna regla que la
  protegiera. `.noinit` guarda lo que sobrevive a un reinicio (bootguard, motivo de reset) y está pegada a los
  guard words y a las pilas: un buffer que creciera enlazaba bien y después pisaba memoria viva en la unidad.
- **`firmware/app.ld`**: NOINIT gana el `ASSERT` que LOWBUF tiene desde 1.4.5. Un buffer que la desborde ahora
  **falla en el link**, con la unidad intacta.
- **`tools/mem_budget.py` + `tests/mem_budget.json`**: se lee el mapa del linker tras cada build y cada ventana
  conserva su piso (LOWBUF 1 KiB, RAM 4 KiB, POOL 4 KiB, RAMTEXT 1 KiB). Se suma a la suite cuando existe
  `build/felucca.map`; sin toolchain se saltea con una línea. Un cambio que se coma la holgura tiene que decirlo.
- **Lo que NO cambió:** ni un byte de DSP, motores, polifonía, FX, UI ni audio. En esta versión no hay una
  optimización de código nueva, y lo digo claro: las candidatas que quedaban (aliasing de los buffers de reverb,
  sustituir el hash del canvas, tablas de color) o cambian el sonido, o sólo se pueden validar midiendo en la
  unidad, o cambiarían los 3056 goldens sin forma de verificar el resultado en hardware. Preferí no vender como
  optimización algo que no puedo medir; lo que sí traje es la medición de las cinco ventanas y la regla que
  faltaba.

## v1.5.11 — El banco de presets, medido y nivelado

- **La medición.** `tests/preset_levels` renderiza los 285 presets de fábrica uno por uno (C4 a
  velocidad 100, ataque, cuerpo y el primer tramo del release, por el mix real) y da RMS y pico en dBFS.
  Antes de esta versión: mediana **−18,5 dB**, **28 presets fuera de ±3 dB**, dispersión **25,8 dB**, y los
  extremos eran `CRYSTAL PLUCK` a −36,8 dB y `QUANTUM BEAT` a −11,0 dB.
- **El problema no era el fader.** La tabla `firmware/src/preset_level.c` (generada por
  `tools/gen_preset_levels.py` a partir de esa misma medición) ya pone el nivel al cargar cada sonido, y
  **25 presets seguían fuera de alcance con el fader a tope**: su contenido era demasiado bajo, no su nivel.
- **Seis sonidos corregidos en su patch, no en el número:** `CRYSTAL PLUCK` y `METAL CLUSTER` (ADDTV) ganan
  cuerpo y SUB — el cluster tenía el suboscilador mudo —, `AURORA FM` baja el envío de reverb/delay que lo
  ahogaba, `ABYSSAL DRONE` baja la reverb de 96 a 72, y `STATIC HAZE` / `GEIGER CNT` (NOISE) recuperan densidad
  y dejan de esconderse en su propia reverb.
- **Después, medido con la misma herramienta:** **19 presets** fuera de ±3 dB, dispersión **12,6 dB**, el más
  fuerte `HEARTH KIT` a −16,9 dB (antes −11,0). La tabla de niveles se regeneró desde la nueva medición, así
  que cada preset carga al nivel que le corresponde con el sonido nuevo.
- **Lo que queda fuera a propósito:** `AURORA FM`, `STATIC HAZE`, `GEIGER CNT`, `ARCADE`, `ABYSSAL DRONE`,
  `CRYSTAL PLUCK`, `GLASS BELL` y `POLAR AURORA` siguen 4–11 dB por debajo de la mediana. Son texturas, colchones
  y drones: su RMS bajo es su carácter (un geiger contando chispas, una aurora que se abre en 30 ms de ataque) y
  subirlos más habría sido cambiar el sonido, no nivelarlo. Está documentado en
  `docs/PRESET_BALANCE.md` con la tabla completa antes/después.
- **Un golden que dependía del calendario.** ABOUT imprime la fecha de compilación (`__DATE__`), así que una
  corrida de la suite que cruzaba la medianoche encontraba 32 frames cambiados — la página about en cada
  paleta — y fallaba por un reloj, no por un dibujo. `tests/ui_render.c` fija esa fecha **sólo en el harness**:
  los píxeles de referencia dependen del dibujo, no del día; el dispositivo sigue mostrando su fecha real.
- **Sin tocar:** motores, algoritmos, polifonía, FX, routing, UI y gates. Sólo datos de preset y la tabla de
  niveles. Goldens de audio regenerados por el cambio audible (63 renders) y verificados por la suite: 0 fallos
  de salud, 0 de voces, 0 presupuestos de CPU excedidos.

