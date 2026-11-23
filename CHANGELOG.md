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

