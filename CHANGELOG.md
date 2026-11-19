## v1.5.6 — El panel deja de recibir lo que ya tiene

**La medición primero.** `tests/ui_render.c --idle` (nuevo, registrado en la suite) recorre las 201 escenas del
barrido, fuerza un frame y después dibuja otra vez sin mover nada: cuenta los píxeles que el dibujo **manda al
panel** (`blit_px`) y los que **cambian** en la pantalla. En `v1.5.5` ese frame quieto escribía **1 485 216 px**
sumando las escenas, **29 280 px por frame en cada página** (240 × 122: la banda de página entera), con **0**
píxeles distintos: el panel recibía otra vez lo que ya tenía. En un frame de 16,7 ms eso es la mayor parte del
tiempo de core 0 en SPI, al lado del audio.

**El arreglo, en el único camino que llega a `lcd_blit`.** `gfx.c` guarda el hash FNV-1a de la última banda enviada
junto con su ventana y **no la manda si el canvas es el mismo** (32 bits sobre palabras de 32: dos píxeles por
carga). La comparación es sobre los **píxeles**, no sobre el estado de la página, así que ninguna firma puede
saltarse un cambio; y todo lo que escribe el panel fuera de ese camino — un `lcd_fill` (`draw_frame`, la pantalla
GEN, un borrado completo) — llama a `gfx_forget()`, igual que el arnés cuando vacía su framebuffer.

| | antes | después |
|---|---|---|
| frame quieto, 201 escenas | 1 485 216 px | **109 056 px (−92,7 %)** |
| frame quieto, una página | 29 280 px | **0 px** |
| flash (imagen de la app) | 582 200 B (1 412 libres) | 582 504 B (**1 108 libres**) |
| RAM `.data+.bss` | 91 612 B (6 692 libres) | 91 648 B (6 656 libres) |

Costo medido: **+304 B de flash y +36 B de RAM**. Clase: `[HOST]` (x86-64, `ui_render`) para los píxeles,
`[ESTÁTICO]` (build AC791N con el toolchain `pi32v2` fijado) para flash y RAM. Los 3 056 frames del barrido siguen
siendo idénticos al baseline; las únicas 48 líneas que cambian son las pantallas ABOUT, que muestran la versión.

## v1.5.5 — Los tres defectos de UI de la fase 2, medidos por región

**Primero el arnés, como pedía el informe.** `tests/ui_render.c --regions` (registrado en `tests/run_tests.sh`)
dibuja las mismas pantallas que la suite y compara **dos estados de la UI sobre rectángulos con nombre**, no sobre
frames enteros: los defectos son cajas de unos cientos de píxeles en un frame de 57 600. Por caso da tres números:
los píxeles que quedan **distintos** dentro del rectángulo (lo que ve el ojo: el fragmento que deja un pop-up),
los que el dibujo **escribió** en ese frame (`blit_px`: un repintado debajo de algo opaco los gasta sin cambiar un
píxel, que es exactamente «redibujar todo el MIXER»), y la tinta de **un** elemento, aislada cambiando sólo su
valor. El paso falla si un número no es el que el arreglo promete, así que los tres casos quedan como guarda.

| caso | antes | después |
|---|---|---|
| A. pop-up de preset sobre el MIXER, al cerrarse | 0 px escritos, **8 172 px distintos** (fragmentos del nombre sobre la zona dB y el nivel) | 27 816 px escritos, **0 px distintos** |
| B. la tinta del tempo contra las almohadillas de T3/T4 | **0 px compartidos** (seis columnas y LARGE) | igual: no se reproduce, ver abajo |
| C. cambio de pista en el MIXER (encoder ALGORITHM) | **53 064 px escritos** para 872 que cambian | **34 520** escritos, 872 distintos, y el frame **idéntico** a un redibujo entero |

**1. El nombre de preset en la zona dB (corregido).** El pop-up pinta una caja de 208 × 86 sobre el panel; al
cerrarse, `ui_draw.c` limpiaba sólo la firma del grafo y **después** de dibujarlo, así que cada columna conservaba
su stamp («en ésta no cambió nada») con la tinta del HUD encima, para siempre. Ahora su fin se atiende al principio
de `ui_draw_page`, con `graph_invalidate()` (`ui_graph.c`): el panel y **todas** las columnas se pintan de nuevo en
ese mismo frame. Medido: 8 172 px distintos → 0.

**3. El cambio de pista en ALGORITHM (corregido).** `track_select()` forzaba el frame entero: `draw_frame()`
limpiaba 240 × 240 px que las bandas volvían a pintar, y `draw_tracks()` hacía `lcd_fill` de toda la tira y
rebliteaba las seis columnas. Ahora el cambio lleva `ui.sel_only`: no hay limpieza de frame ni relleno de la tira,
y cada banda se repinta por la firma que le corresponde (la cabecera lleva `song.sel`; las columnas, el acento de
la seleccionada; las tarjetas y el pie, sus valores). Medido: 53 064 → 34 520 px escritos, y los 872 px que
cambian son los de las dos columnas de la selección, con el frame **pixel por pixel igual** al de un redibujo
entero (lo comprueba el mismo caso C).

**2. El tempo sobre los números de T3/T4 (no se reproduce).** La medición aísla la tinta del bloque del tempo
cambiando sólo el BPM (124 → 60, los dos de tres cifras: el bloque no se mueve): su tinta queda en **y 1..14**, y
las almohadillas de T3/T4 en **y 83..99** (seis columnas) y **y 132..148** (MENU > LARGE). Cero píxeles
compartidos en las dos variantes. La superposición que el informe describe es horizontal —el bloque del tempo
ocupa x 76..132 y las columnas de T3/T4 empiezan en x 81 y 120—, no vertical: con la cabecera de 16 px y las
tarjetas desde y 20 no hay fila que compartan. Se deja el caso B en el arnés como guarda; si el defecto se ve en
una pantalla concreta, hace falta la foto o el estado exacto para medirlo.

## v1.5.4 — El tick de 10 kHz vuelve a su presupuesto, y con él la publicación del sitio

**Numeración.** El trabajo de esta ronda llegó a `main` como v1.6.0 y v1.6.1; acá se renumera como **v1.5.4**, la
que sigue a v1.5.3, y así queda en una sola versión: firmware, paquete, emulador, instalador, editor, web y
README. Los artefactos 1.6.x salen del árbol para que la selección de «última versión» no los elija por encima
de ésta; sus etiquetas en el remoto son historia y no se tocan desde acá.

**El sitio público llevaba desde el 2026-11-18 20:58 sirviendo v1.4.17.** No era Pages ni el despliegue: el
workflow `emulator` —el único publicador, no hay `pages.yml` ni rama `gh-pages`— moría en el paso 16
(`tests/target_budget.py` + `web/test_pkg_bounds.mjs`) y un paso de build que falla deja sin armar el sitio y
sin desplegar. El log recortado de `ci-diag` no lo mostraba porque el mensaje es «OVER BUDGET», que no entra
en el filtro de errores.

**Causa medida.** `tests/target_budget.py` sobre `build/felucca.dis` (clase: build AC791N con el compilador
del target, cuenta estática —no una medición en la unidad):

| commit | `fm1_timer5_irq` |
| --- | --- |
| `2fa2953`, el último que llegó a publicarse | 27 018 (baseline 27 007, ok) |
| `c865155` (v1.5.0, Screen Reader) y todos los siguientes | **31 098 → +15 %, límite +10 % = FAIL** |

`c865155` dejó el anillo SysEx de salida compilado en todas las build y agregó la rama del lector de pantalla
dentro de `sysex_byte()`, que el compilador inlinaba en el bucle de recepción byte a byte del ISR de TIMER5:
el bucle `ep1rx` pasó de 138 a 197 instrucciones.

**Arreglo.** `sysex_byte()` pasa a `noinline`, el mismo patrón que `fm1__anim_frame()` en `hal/fm1_input.h`
(«el código del tick queda como estaba»). Medido de nuevo: **21 962**, 19 % por debajo del baseline y por
debajo del commit que sí se publicaba. El comportamiento es el mismo: un byte de SysEx cuesta una llamada, y
SysEx llega byte a byte cuando el lector pregunta. El paquete nuevo pesa 610 933 B (imagen 582 142 B).

**Lo que esto NO afirma:** ninguna medición en un FM-1. Es un conteo estático del compilador del target más
la suite de host; latencia real, CPU del core 1 y sonido siguen pidiendo la unidad.

### Validación completa: el índice de evidencia y el baseline de píxeles

**La suite completa encontró dos fallos, y están corregidos.**

1. **El índice de evidencia estaba desincronizado.** `docs/EVIDENCE.md` decía 617 filas cuando la matriz tiene
   675: el archivo se genera desde `docs/VALIDATION_MATRIX.md` (`tools/evidence_index.py`) y había quedado
   atrás. Regenerado; `--check` vuelve a pasar dentro de la suite, así que una fila que cambie de estado no
   puede quedar invisible otra vez.
2. **El baseline de píxeles era anterior a v1.5.3.** Los 197 frames marcados «changed» incluían ABOUT, FX y
   MONITOR además de las pantallas de HOME, porque el cuerpo de HOME queda detrás de esas capas y la barra del
   icono de página viaja con él: la suite dibuja con ANIM OFF, y ahí el pulso ya no late (el arreglo de v1.5.3).
   No era una regresión sino el cambio pretendido sin baseline nuevo. Regenerado con
   `tests/ui_pixels.py --update`: **3 056 frames, 0 changed**, y el motivo queda escrito acá, como pide la regla
   del arnés.

**Barrido de la fase (clase: host x86-64 sobre este árbol).** `NO_PACKAGE=1 ./tests/run_tests.sh` entero:
polifonía y robo de voces, seis pistas, todos los motores, presets, FX y buses, ARP/SEQ/LFO/SCL/macros,
entrada y rebotes de contactos, parser MIDI TRS, SysEx de accesibilidad, storage A/B con escrituras cortadas,
auto-test del segundo núcleo, ASan/UBSan, lints de alineación (3 783 017 mediciones, 0 fuera de 1 px),
`regress` 303 renders **0 changed**, y 3 056 frames de píxeles idénticos.

**README al día.** El encabezado seguía diciendo `v1.3.9`; ahora nombra la versión que el firmware lleva. Los
diez motores seleccionables del README son los que `ENGINES[]` muestra (`NENG_SHOWN` = 10 con FM4 retirado).

**Lo que esta versión NO puede afirmar:** todo lo anterior es software. Pop de encendido, latencia real, CPU
del core 1 y el comportamiento acústico de los fades siguen pidiendo la unidad; no hubo flasheo, borrado ni
escritura en ningún FM-1 en esta fase.

## v1.5.3 — ANIM OFF en HOME 2–5: la pantalla deja de repintarse para nada

**El parpadeo del reporte, con la causa encontrada.** «Parpadeo continuo en los encabezados de HOME, páginas 2 a 5
(OVERVIEW, MIXER, PERFORMANCE), independientemente de FLAT/LINE o ANIM ON/OFF.» Eran dos mitades del mismo error:

- `hv_strip()` hacía **respirar** la barra bajo el icono de página encendido leyendo `ui.frame` **aunque ANIM
  estuviera en OFF** (y en LOAD LITE / ECO);
- `hv_view_sig()` — la firma que decide si el cuerpo de esas tres pantallas se redibuja — metía esa misma fase
  en el hash, así que el cuerpo entero (240 × 176, y con él la banda del encabezado) se re-bliteaba cada 8
  frames sobre una imagen que no había cambiado.

Ahora, con ANIM OFF la fase no es parte del estado y el pulso no late: la pantalla queda quieta de verdad. Con
la animación encendida la fase sigue siendo estado y un cambio de frame sí repinta, como corresponde.

`tests/ui_test.c` gana `test_still_when_anim_off()`: con ANIM OFF la firma no cambia al avanzar 8 frames (nada
se repinta) y con ANIM ON sí cambia. `regress` 303 renders **0 changed**; `ui_test` completo en verde.

**Lo que esta versión NO arregla, y no voy a decir que sí:** los otros cinco defectos del informe —fragmentos del
nombre de preset en la zona dB del MIXER, icono/valor de tempo sobre los números de T3/T4, redibujado completo
del GLO MIXER al cambiar de pista en ALGORITHM, y la auditoría de invalidaciones (#106)— siguen abiertos. La
causa probable de los tres primeros está localizada en `hv_mix`/`draw_head`, pero cada uno necesita su
medición diferencial de región antes de tocar el dibujo, y el arnés actual dibuja frames enteros.

## v1.5.2 — Cambiar un preset ya no resetea la capa de performance de las otras pistas

**El bug del reporte, encontrado y corregido.** «El audio se interrumpe al cambiar el preset desde GLO MIXER,
incluso cuando el preset que elijo no está sonando.» El flush de los buses ya estaba acotado a la pista que
suena (issue #106, fase 2), pero `apply_preset_to` seguía llamando **`perf_reset()`**, que es global por
construcción:

- levanta `perf_kill`: el ISR toca el mapa de FX **vacío durante un bloque**, en todas las pistas;
- borra las cuatro perillas de performance (`perf_k[0..3]`);
- suelta el latch de la capa MACRO y resetea **cada** buffer de punch-in (`pb_reset_all`).

Cambiar el sonido de una pista muda tiraba todo eso por la borda. Ahora las dos llamadas globales
(`perf_reset` y `fx_preset_flush`) están detrás del mismo gate: la pista tiene una voz activa. Una pista que
suena sigue recibiendo su arranque limpio (el comentario de `apply_preset_to` lo promete y el gate lo
respeta); una pista muda no toca nada que no sea suyo.

`tests/preset_change_order_test.py` (nuevo, en `run_tests.sh`) fija exactamente eso: ambas llamadas dentro del
`if (any)`, el gate leyendo las voces de esa pista, y ninguna llamada suelta fuera de él.
**La ausencia del corte es de oído: queda pendiente de la unidad**, como el resto de lo acústico.

Sin cambios de audio ni de UI: `regress` 303 renders, 0 changed; la imagen y la RAM no se mueven.

## v1.5.1 — Auditoría de memoria medida, y la optimización que no se envió

Versión sin cambios de comportamiento: es el resultado de la pasada de optimización profunda de RAM sobre el
código real, con la regla del proyecto aplicada al pie de la letra (**no se incorpora una optimización que no
demuestre un beneficio**). Lo medido:

**Dónde está la RAM** (`tools/ram_report.py` sobre el `.elf` de AC791N, 91 608 B de `.data+.bss`):

| Símbolo | B | Dónde |
|---|---|---|
| `WT_GEN` | 12 288 | `eng_wavetable.c`, banco de tablas generado en la primera nota |
| `trk[6]` | 10 560 | `track_t` de 1 760 B por pista (parámetros, pasos, ventana de voces) |
| `fm6_note[6][·]` | 7 104 | estado por voz del motor FM6 |
| `drum_kit` | 5 376 | kit de DRUM |
| `upf` | 3 728 | staging de backup del editor |

Los 273 B de padding de sección y los 2 B de padding por `voice_t` (18×2 = 36 B) son lo único que queda sin
usar en las estructuras, y reordenar `voice_t` no lo reduce (la alineación de 4 B lo vuelve a 88). No hay
arreglos muertos: los sospechosos grandes (`midi_sel_on` 1 024, `gen_keep` 2 176, `motion_base` 1 500,
`hc_tab` 1 548) tienen entre 4 y 20 usos reales cada uno.

**El pool no puede absorber `.bss`**: quedan 9 248 B y la regla del proyecto exige ≥ 8 KiB libres ahí, así
que sólo 1 056 B serían movibles y ninguna estructura grande entra en ese margen (`fm6_note` 7 104, `WT_GEN`
12 288, `drum_kit` 5 376).

**La optimización evaluada y rechazada.** `voice_victim` recorría el pool dos veces por robo (contar las seis
partes y después elegir), y el conteo de cada parte sólo se usa dentro de su propia iteración: se unificó en
un solo recorrido. En el binario de AC791N la función bajó de **474 a 426 B (−10,1 %)** pero `voice_room`
subió 8 B por inlining y la **imagen creció 16 B** (libres 1 508 → 1 492), con el bench de host incapaz de
resolver la diferencia (49–308 ns de dispersión entre corridas del mismo binario). Beneficio no demostrable y
coste real en flash: **revertida**, con este registro como evidencia.

Lo que sí queda de la pasada: el mapa exacto de la RAM, el techo de las tres regiones críticas (XIP 1 492 B,
RAM 6 696 B, pool 9 248 B con su mínimo de 8 KiB) y la lista de lo que habría que rediseñar —no ajustar— para
moverlo. Nada de audio, UI ni funciones cambió: `regress` 303 renders, 0 changed.

## Fase 3 — Los contadores del allocator dicen la verdad (#107)

**#107 reproducido con los presets del reporte.** `tests/poly_report_test.c` corre el camino exacto (keybed
del HAL, `keyboard_block`, `trk_note_on`) con ANALOG BRASS (preset 9) y ANALOG PLUCK (8): 1, 4, 6, 7, 8 y 12
teclas de a una y de golpe dan **12 voces sonando, 0 robos, 0 denegadas**, también con SUSTAIN 100, con el
gobernador de carga puesto y con DUAL CORE asignando todas las partes al core 1. Ninguna nota se pierde.

**Dos defectos de medición, corregidos** (el audio no cambia; `regress` sigue en 303 renders, 0 changed):

1. **El pico contaba las voces que ya se están desvaneciendo.** `voice_peak` se tomaba de `voice_live`, que
   incluye los ~6 ms de fade de una voz ya reemplazada: con 12 voces de presupuesto el monitor imprimía
   **PK 17**. El pico es ahora `sounding + release` y no puede pasar del presupuesto; una ráfaga más corta
   que un frame no es polifonía que el oyente escuchó.
2. **Un reemplazo en el sitio no se contaba como robo.** Una parte con la ventana llena reusa su voz más
   vieja en el lugar, sin `voice_kill` y sin fade: 20 notas en 12 voces informaban **STL 0**. Ahora una voz
   que sonaba y se toma para otra nota cuenta como robo (el retrigger de la misma nota es una repetición).

**Prueba de estrés nueva** (`tests/poly_stress_test.c`, en `run_tests.sh`): seis partes con seis motores
distintos, acordes de cuatro notas cada una (24 notas en 12 voces), efectos abiertos, ataques y releases
rápidos, y un cambio de motor bajo un acorde sostenido. Medido en host x86-64: **12 voces ocupadas, PK 12,
STL 8, DEN 0**, ninguna parte quedada en cero, **14 µs por bloque** con las seis partes y los FX (presupuesto
725 µs), y **0 voces activas** después de soltar todo y dejar correr las colas. Memoria del target sin
cambios: RAM 91 608 B de 98 304 (6 696 libres), paquete 610 959 B.

**Polifonía efectiva:** se mantiene en **12 voces** (`NVOICE=12`, `VPOOLN=18`, `VRESET`/`WSEED` ya activos en
`main`). No se subió el número: el coste medido en `docs/VOICE_POOL.md` es 1 134 B de `.bss` por voz de
presupuesto y el margen de RAM es de 6 696 B; subir a 14 dejaría el pool por debajo del mínimo que el propio
documento exige (`VPOOLN >= NVOICE + NPART`) sin re-medir el sonido. El CPU del host no es el de la unidad:
esa comparación queda pendiente de hardware.

## Fase 2 — El fade de arranque ya no se gasta detrás del mutismo

**Defecto de orden corregido (audio de encendido).** `fm1_audio_init` arranca ALNK0, renderiza 25 ms por
`audio_block` y recién al final desmutea la salida (pc6). `main.c` llamaba a `audio_boot_hold()` **después** de
`audio_init()`, así que esas 25 ms corrían el fade de 27 ms hasta ~92 % con la etapa todavía muteada: el
amplificador se abría contra un DAC ya casi al máximo y el fade que existía para tapar ese escalón ya estaba
gastado. Ahora el hold va **antes** de `audio_init()`: todo el bring-up (DAC, ALNK0, FIFO, DMA, arrange del
segundo núcleo con interrupciones apagadas) es silencio digital exacto, el desmuteo cae sobre ceros y el fade
se arma después, al final, cuando `audio_fade_in()` lo abre. No agrega latencia (el ramp sigue dentro del
splash) y no silencia nada: el primer bloque tras armar sale desde 0 y llega a unidad en 1 200 muestras.

`tests/boot_order_test.py` (nuevo, en `run_tests.sh`) fija el orden sobre el código que lo implementa: hold
antes del motor, apertura después del segundo núcleo, y en el HAL el arranque del motor antes del desmuteo con
su espera en medio. **La ausencia del pop es acústica: queda pendiente de verificación en la unidad.**

## Estabilidad (corrección de pendientes): carrera entre núcleos en la asignación de voces

**Bug real corregido (#107, sincronía de núcleos).** El escaneo del teclado corre desde TIMER5, **anidado
dentro del ISR de audio**, y en ese momento el render ya le entregó las pistas 3..6 al core 1. Una nota
pulsada ahí podía entrar en `voice_alloc` y, con ella, en `voice_window_grow` (que reasigna slots de la
ventana de **otra** parte y hace `memset` de un `voice_t`), en `voice_start` (escribe la voz) o en un robo
que escribe la voz de otra parte — todo mientras el core 1 las estaba leyendo. Ahora:

- `firmware/hal/fm1_multicore.h`: `fm1_mc_job_busy()` (`job != done`), la pregunta "¿el core 1 está
  renderizando?" desde un contexto anidado.
- `core.h` / `felucca.c`: `FELUCCA_MC_BUSY()`, 0 en el host y en builds de un solo núcleo, y
  `fm1_mc_job_busy()` en el firmware con DUAL CORE.
- `voice.c`: si el otro núcleo tiene un trabajo en vuelo, el note-on/note-off se guarda en una cola de 8 y
  se reproduce al principio del bloque siguiente (`voice_defer_flush`), cuando el core 1 está quieto. Un
  note-off nunca se descarta (se aplica en el acto si la cola está llena): nunca una nota colgada. Un
  note-on que no entra en la cola se pierde, y está dicho en el código.
- `seq.c`: `events_block()` vacía la cola antes de que el bloque reparta trabajo.

Nuevo `tests/voice_defer_test.c` (host, con el flag simulado, ejecutando el código real): 9 comprobaciones,
incluido el acorde de 12 teclas diferido y la cola llena.

**Código muerto eliminado.** `hv_system()` (la pantalla SYSTEM de HOME, ~90 líneas) no la llamaba nadie:
`graph_home_view()` sólo alcanza OVERVIEW / MIXER / PERFORMANCE. Se borró con su helper `hv_batt()`, que
sólo ella usaba. Libera XIP y quita un comentario que prometía una pantalla inexistente.

## v1.5.0 — Fase 3: accesibilidad, optimización de note-on y cierre de la fase 2

**Issue #105 — FM-1 Screen Reader.** `firmware/src/announce.c`: el protocolo del lector (SysEx
`F0 7D 53 52`, versión 1) implementado en el firmware, con el anillo SysEx de salida compartido con M-UPGRADE
(`usb.c`, SysEx antes que cualquier nota) y el parseo en el mismo sitio donde ya se parseaba el SysEx del
editor. Se anuncia la pantalla al cambiar, el valor de una tarjeta de HOME mientras gira su perilla (a los
~120 ms quieto, como pide el protocolo), el sonido cargado y los mensajes de la barra superior; WHERE
reanuncia la pantalla. Silencioso mientras no hay lector: ningún byte, ninguna cadena formateada, ninguna
llamada nueva desde el ISR. Sin archivo de palabras propio todavía (el lector aplica sus reglas comunes);
filas de MENU, perillas fuera de HOME y listas quedan documentadas como pendientes en
`docs/ACCESSIBILITY.md`, junto con Tonitor y Bigorna como alternativas autónomas estudiadas. Nuevo
`tests/announce_test.c`: 23 comprobaciones del protocolo y del empaquetado USB-MIDI (CIN 4/5/6/7).

**Optimización medida (host).** El pico del monitor escaneaba las 72 voces del pool en cada note-on. Ahora el
contador de voces vivas se mantiene donde una voz empieza y donde termina, y `voice_stats()` lo recalcula una
vez por frame. `tests/alloc_bench.c` (mismo compilador y flags, dos árboles): iniciar la misma nota
**162–169 → 67–77 ns**, una nota nueva **269–278 → 146–179 ns**. `voice_alloc` además hace un solo escaneo del
pool por nota (ninguno bajo el presupuesto de carga) en vez de dos. Los 303 renders dorados siguen idénticos.

**Fase 2, lo que quedó pendiente se cierra aquí en lo que se pudo verificar.** Sin toolchain AC791N en este
entorno no hay paquete ni release nuevos: el sitio y `release/` siguen sirviendo v1.4.17, y la versión de
fuente pasa a `v1.5.0`.

**v1.5.0 PUBLICADA el 2026-11-19.** Construida con el toolchain y el SDK ya instalados y verificados por
hash (`tools/get_ac791n.py`, `tools/toolchain.sha256`, `SDK_SHA256`), no anunciada antes de existir:
`.fwsc` de **610 959 B**, sha256 `acbad1144b568711…`, identidad **FM-1_965**, build `2dd157e`, emulador del
mismo commit, `release/`, `site/firmware/felucca-1.5.0.fwsc` y `versions.json` coherentes
(`tools/make_release.py --check`). Reproducir:

```sh
python3 tools/get_ac791n.py          # toolchain (hash fijado) + los 3 blobs del SDK
ulimit -n 9000
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK ./build.sh
sh web/emu/build.sh                   # emulador del mismo commit (emsdk 6.0.11)
python3 tools/make_release.py --sync   # release/, site/, SHA256SUMS
python3 tools/make_release.py --check
``` Los pendientes de UI de la fase 2 (región de pop-up, TEMPO sobre
T3/T4, repintado de ALGORITHM) siguen abiertos y con la causa localizada.

## Fase 2 — Issues #106 (UI) y #107 (polifonía)

**#107:** la polifonía se mide ahora donde arrancan las voces. `voice.c` lleva el pico real
(`voice_peak`), las voces negadas (`voice_denied`) y `voice_stats()` con **sounding / release / fading /
peak / steals / denied** separados; el monitor de `ui_graph.c` los imprime (`REL`, `FADE`, `PK`, `STL`,
`DEN`) y el pico se reinicia al abrir la pantalla, así que ya no depende del frame. `vshare()` reparte
sólo entre las pistas vivas: las dormidas por `MENU > TRACKS` ya no hacen que una parte con tres voces
lea como excedida. Nuevo `tests/poly_report_test.c`: 1..12 teclas en ANALOG BRASS / PLUCK y POLY, de una
vez y una por una, con SUSTAIN, con el presupuesto de carga, con DUAL CORE, acorde largo y note-on/off
rápido — **12 de 12 voces en todos los casos, cero robos en el host**. El "7" del reporte no se
reproduce en el host; el detalle y lo que falta medir en la unidad están en
`docs/FASE2_ISSUES_106_107.md`.

**#106:** el cuerpo de HOME (OVERVIEW / MIXER / PERFORMANCE) metía la carga de audio y los medidores
con paso `>> 9` en la firma que decide repintar 240×176 en tres blits: con PLAY se rebliteaba hasta 60
veces por segundo sin que un píxel cambiara, y eso es el parpadeo de la línea superior. Ahora la carga
no entra en la firma y los medidores se cuantizan a lo que se dibuja (9 segmentos, 12 px). El cambio de
preset escribe los parámetros con la IRQ de audio cerrada (un bloque ve el sonido viejo o el nuevo,
nunca una mezcla) y ya no vacía las líneas de delay/reverb de todas las pistas cuando la pista que
cambia de sonido no tiene ninguna voz: ese era el corte que se oía con un preset mudo. Pendientes de
región/pop-up, TEMPO y ALGORITHM, con la causa localizada, en el mismo documento.

## Fase 1 — compatibilidad con M-VAVE V16 (`FM-1_016`)

El firmware oficial V16 (718 196 B, `d017fb65…`, identidad `FM-1_016`) se midió y se incorporó como
segunda release oficial: un FM-1 que corre V16 se reconoce como **stock** (no como "otro firmware") y
puede ser el origen de una instalación de Quantum GammaWave sin pasar por V15. También se puede volver
al archivo oficial V16, con su propio pin.

- `web/fm1pkg.js`: `STOCK_RELEASES` con V15 y V16 (digest, tamaño, imagen lógica, layout medido y las tres
  regiones de referencia de V16); `isStockIdentity`, `stockReleaseOf`, `stockLayoutIssue`,
  `loaderWindowIssue`, `hasFeluccaLoader`.
- `web/fm1ota.js`: donantes V15 y V16 (identidad y firmware deben coincidir); la ventana del loader
  (`0x4000–0x93000`) se exige **antes de cualquier frame** a todo paquete que lleve el loader de este
  proyecto.
- `web/fm1verify.js` y la página: `FM-1_016` es stock; el retorno al oficial usa el digest y la identidad
  del archivo verificado, no constantes V15.
- `tools/fm1_install.py`, `tools/fm1_preflight.py`, `tools/check_package_identity.py`, `tools/fm1pkg_make.py`:
  los dos pines oficiales, y el builder falla si el área de app sale de la ventana del loader.
- Pruebas: `web/test_stock_releases.mjs`, `tests/stock_profile_test.py`, `web/test_donor.mjs` y
  `web/test_verify_only.mjs` actualizados. Verificado contra el archivo real: sus tres hashes de región se
  reproducen byte a byte.

**Pendiente de hardware:** que el actualizador V16 acepte el paquete y que el SPL V16 arranque el área de
app de QG; el `UPDATA_PARM` `ota-FM-1_015` de `ota.c`; JEDEC de una unidad V16; corte y reanudación; y el
backup verificado con M-Flasher. Detalle en `docs/FM1_V16.md` §5. Nada de eso se declara medido.

## v1.4.17 — URGENTE: `-flto` sale del build

v1.4.16 fue la primera versión compilada con `-flto` (Fase 6) y **la unidad del dueño no volvió a arrancar después
de instalarla**. Con la unidad brickeada y sin poder instrumentarla, el build vuelve a las banderas que corren hoy
en las unidades: sin `-flto`, sin plugin, misma imagen de 583 036 B.

- `-flto` ahorraba 4 196 B de imagen y eso no vale una unidad. Vuelve sólo con causa encontrada y evidencia de
  hardware, no con una ganancia de tamaño.
- El sitio deja de ofrecer 1.4.16: `site/firmware/versions.json` ya no la lista y sus archivos salen del paquete.
- `docs/BRICK_v1.4.16.md` lista los mecanismos por los que LTO puede romper este firmware, qué está descartado y
  qué falta para confirmarlo.

La diferencia entre 1.4.16 y 1.4.15, además de LTO, es sólo código de UI del panel: nada cambió en el arranque, el
loader o la tabla del panel.

## v1.4.16 — LTO: la imagen baja 4 196 B

`-flto` estaba mal medido: `ld` no carga `LLVMgold.so` por su cuenta, y sin el plugin el enlace de bitcode falla.
Con `-plugin <toolchain>/common/bin/LLVMgold.so` en los dos enlaces el build entra y:

- imagen **578 840 B** de 583 612 (**4 772 B libres**, antes 576)
- código XIP 422 364 B (−4 356), constantes 135 780 B (−40)
- `.data+.bss` 91 312 B de 98 304; pool spare 9 248 B; LOWBUF 30 564 B — sin cambios

No gana por unificar unidades (la app ya se compilaba a un solo módulo IR): el objeto final era código máquina y el
linker sólo concatenaba; con el plugin corre LLVM sobre el bitcode.

Verificado con el cambio puesto: 303 renders dorados idénticos (0 cambiados, 0 perdidos), 3 056 cuadros del
barrido idénticos al baseline, lint y alineación sin hallazgos, `target_budget.py` sin entradas fuera de
presupuesto. Clase: build AC791N para los bytes, host x86 para audio y pantalla.

## v1.4.15 — el build de release aceptaba sólo versiones de una cifra

`tools/build.py --release` validaba el número con `(\d)\.(\d)(?:\.\d){0,2}`, un dígito por segmento. Toda versión
con una cifra de dos dígitos —`1.4.10`, `1.4.13`, `1.4.14`— se rechazaba con «use X.Y, X.Y.Z…», y ése fue el paso
que falló en el job `build` de **todas** las releases desde la 1.4.10: v1.4.9 es la última que compiló su paquete
en CI. Ahora los segmentos son `\d+` (`X.Y`, `X.Y.Z`, `X.Y.Z.W` y el sufijo `-rc1` siguen aceptándose, `x1` no), y
`./build.sh --release 1.4.15` corta el paquete desde un árbol limpio, como lo corre el workflow.

Esta versión no cambia una línea del firmware: es la 1.4.14 soplando el `--release` que el tag necesita. Con
esto el job `build` del tag compila el paquete (15 m 13 s, artefacto `packages` y su atestación); el job
`verify` de ese mismo run sigue en rojo y es la única cosa abierta de esta versión: su log necesita
credencial y no se pudo leer desde acá, y el paquete publicado es byte a byte el del árbol (`sha256
191493bc…`), así que lo que resta comprobar es la atestación, no los bytes del paquete.

## v1.4.14 — PANEL Y: la franja del panel se elige en Settings y se recuerda

Esta versión integra lo que el trabajo paralelo publicó hasta la 1.4.13 (DIST FOLD con la fuente S comprimida, el
pop de encendido, la compactación de las píldoras y el cache de TONE) con PANEL Y, el fade del motor arreglado y
los arreglos de pruebas de este lado; las 1.4.11 a 1.4.13 ya publicadas no se reescriben.

### El problema, dicho entero
El controlador tiene **240x320** de memoria de cuadro y el panel muestra **240 filas**: está pegado a una franja de
240 de esas 320. Todo el firmware escribe la pantalla en las filas **0..239**, que es la franja de arriba; si esta
unidad está pegada a la de **80**, cada pantalla aparece 80 filas más arriba de lo que se ve: la cabecera y los
títulos se van fuera del vidrio y el tercio de abajo muestra memoria que nadie dibuja. Eso no se arregla con la
tabla de init —es el `LCD_SEQ` de 1.4.10— sino moviendo la ventana.

Hasta ahora eso era el comando `off N` de la consola, que se perdía al apagar. Ahora es un ajuste:

- **MENU > EXTRA > PANEL Y: `0` o `80`.** El 0 es el valor por defecto y el que leen todos los registros viejos,
así que nada se mueve en una unidad que ya estaba bien; el 80 es la franja que usan los módulos ST7789 de 240x240 de
siempre (Adafruit, TFT_eSPI, Pimoroni).
- **Se aplica en `lcd_window` y en el despeje.** `lcd.c` suma la franja a toda ventana de 240x240 y el relleno de
fondo de pantalla completa limpia las filas que quedan **fuera** de la franja (0..79 con 80; 240..319 con 0), así
que al cambiarla no queda una banda del color anterior.
- **Se recuerda.** Vive en `favorites.factory[15][16]` (un byte que ningún motor usa), con el clamp de siempre: un
valor corrupto o de otra versión lee como 0. Se aplica en `settings_init`, antes de que `lcd_init` escriba nada.
- **La consola dice lo mismo.** `off N` ahora escribe el mismo ajuste (y lo guarda), así que los dos caminos no
pueden discrepar; `status` sigue publicando `lcd_off` con la franja en uso.

### Cómo se mide
En la unidad, con el mismo build: MENU > EXTRA > PANEL Y en `0`, mirar; pasarlo a `80`, mirar. La que muestre la
pantalla **completa** (cabecera arriba, nada cortado, sin banda abajo) es la buena, y queda guardada. No hay que
recompilar ni reflashear para probar la otra: es un paso de encoder.

### Clase y límite
Esto es código y build AC791N; la franja correcta **no está medida en la unidad** — la dice el dueño mirando. La
tabla de panel de 1.4.10 sigue intacta y su prueba (`tests/panel_table_test.py`) no cambia.

### Lo que entró con la fusión
Esta versión integra también lo del trabajo paralelo: la pantalla **MOD MAP** del grupo MOD, la fase 1 de DSP (dos
divisiones por bloque menos en los buses), DIST FOLD con la fuente S comprimida para pagarlo, el arreglo del pop de
encendido y la auditoría de arranque. Con eso el árbol cierra en **582 356 de 583 612 B (1 256 B libres)**: la
primera fusión de estas ramas desbordaba 160 B y hubo que acortar 36 claves de consola a mitad de camino; cuando la
1.4.11 del otro lado pagó el espacio, **se restauraron los nombres completos** (`usb_frame_stalls`,
`key_noteon_max_us`, la ayuda de `memr`/`flr`, `gap_hist`, …): la consola de esta versión dice lo mismo que decía
la 1.4.10, y el árbol queda con más de un kilobyte de sobra. También quedó arreglado el test que la fusión anterior
de la 1.4.8 había dejado sin compilar (`text_ref_test.c` usaba `f->adv[]`; la versión del otro chat lee `hc` por el
puntero `AF_x_HC`, que es la buena, y esta rama aporta `advance()` para las avanzadas en bytes).

### La 1.4.11 publicó el motor en silencio en el host, y eso se arregla acá
El fade de encendido de la 1.4.11 gatea el master con un flag (`audio.c` `boot_armed`) que sólo `main.c` armaba.
Cualquier otro que renderiza por `audio_block` —la suite host entera, y el emulador— quedaba en **cero exacto**:
`poly_test`, `audio_test` y `master_test` daban −180 dB en todas las mediciones. Ahora el flag **nace armado**
(la rampa corre desde el primer bloque, como antes de la 1.4.11) y es `main.c` el que sostiene el silencio con
`audio_boot_hold()` justo después de `audio_init()`, hasta `audio_fade_in()` cuando terminaron las etapas con
interrupciones apagadas. El arranque de la unidad queda exactamente como lo diseñó la 1.4.11 (el desmute cae
sobre ceros) y nadie más tiene que conocer ese tiempo. `boot_fade_test` sigue pasando, y los 303 renders dorados
quedan idénticos.

### El baseline de píxeles de la FASE 5, regenerado a propósito
`tests/ui_pixels.txt` cambia en 64 de sus 3 056 capturas, y las dos razones son las de esta versión: el
splash y ABOUT dibujan la versión real (`v1.4.14` en vez de `v1.4.13`) y MENU > EXTRA tiene una fila más
(PANEL Y). No se movió ningún otro píxel: el resto del barrido queda byte a byte igual, y las 3 056
capturas vuelven a coincidir con el baseline nuevo.

### Medido, en el host
- `tests/lcd_st7789_test.c` tiene cuatro comprobaciones nuevas: con PANEL Y 80 la fila 0 del UI va a la memoria
  80 y el relleno de pantalla completa limpia 0..79; con PANEL Y 0 vuelve a 0 y limpia 240..319. **Las cuatro pasan.**
- El barrido de pantallas (`tests/ui_render.c`) queda en **200 pantallas × 22 paletas, 0 hallazgos** de lint y
  **4 208 795 medidas de alineación, 0** fuera por 1 px, con la fila PANEL Y incluida en el MENU.
- La suite host completa pasa con este árbol (el único fallo era el test del conjunto de release, que exige
  `site/` sincronizado, y se sincroniza antes del commit del paquete).

## v1.4.13 — el cache de TONE, y los píxeles como baseline

### El cache de coeficientes TONE (la optimización que la Fase 1 no pudo pagar)
`tone_set` corría dos veces por bloque —1 378 veces por segundo— con tres divides y una tangente cada una, en un
chip sin divide. Sus coeficientes sólo dependen de dos perillas (el TONE y DIGITAL del delay, el FILT de la
reverb) y los demás argumentos son constantes, así que ahora se guardan para el valor con el que se construyeron
y se rehacen cuando una perilla gira (`fx.c` `tc_dly` / `tc_rev`). El render es **bit-identico**: 303 renders
dorados sin cambios. Coste: +2 276 B de código, pagados por la compactación de las píldoras de tecla.

`tests/tone_cache_test.c` lo fija: dos renders frescos con las mismas perillas dan los mismos bytes, perillas
distintas dan sonidos distintos, y A → B → A vuelve a A (nada queda clavado al primer valor).

### Los píxeles, como baseline (`tests/ui_pixels.txt`)
El barrido ya fallaba por lint y por alineación, pero nadie comparaba los **píxeles**: un cambio en una tabla de
glifos, una paleta o un desplazamiento de 1 px podía pasar toda la suite. Ahora `tests/ui_pixels.py` hashea las
3 056 capturas y las compara contra la lista versionada; CI lo corre en cada push. Cambiar los píxeles a
propósito exige `--update` y decirlo en el commit.

### Lo que se probó y no entra
- `-flto`: el linker del toolchain (GNU ld 2.26) no lo enlaza — falla. Se midió; no está disponible.
- Zero-init de los `ui_shapes` duplicados: no hay ninguno, 0 B.
- Kerning de S (~476 B), `ui_shapes` comprimido (~2,3 KiB), unión de `sec_stage`/`proj_scratch` (~9 KiB de
  `.bss`), buses a bloque de 64, blit en el núcleo 1, reescritura del barrido de teclas a 10 kHz: quedan
  documentados con sus números en `docs/OPTIMIZATION_PHASE1_AUDIT.md` y `docs/INDEX.md`, sin empezar.

## v1.4.12 — 1 936 B de flash recuperados sin tocar un píxel

Las 22 píldoras de tecla (`ui_keycaps.h`) se guardaban en nibble crudo (3 939 B) aunque el 90 % de una
píldora es relleno (5) o afuera (0). Ahora van en el códec canonical de las fuentes, al que se le puede pedir
runs de valores que no sean 0/15, y las dibuja el mismo `cv_alpha_hc` de los glifos: **1 947 B + 45 B** con
los mismos píxeles (3 056 capturas del barrido idénticas byte a byte) y los 303 renders dorados sin cambios.

La imagen queda en **580 492 B de 583 612** (3 120 B libres) — el margen que necesitaba el cache de
coeficientes TONE, que desbordaba por 2 160 B. `docs/OPTIMIZATION_PHASE1_AUDIT.md` deja la tabla de lo que se
midió y no daba nada: alinear funciones, fusionar constantes y quitar las tablas de unwind (0 B), menos
inlineado (+788 B), `-fno-jump-tables` (+916 B), y las tablas grandes que ya estaban optimizadas.

## v1.4.11 — el pop de encendido, y el DIST que pliega

### El pop al encender: el fade se gastaba con el altavoz todavía en mute
`fm1_audio_init` mantiene el estado de salida (pc6) en mute mientras el códec arranca y desmutea recién al
final, 25 ms después de que el motor empezara a correr sobre el buffer en ceros. El fade de encendido, en
cambio, arrancaba en el primer bloque: cuando el altavoz se abría, la rampa ya estaba al ~93 %, así que lo que
llegaba al parlante era justamente el escalón que el fade existía para esconder.

Ahora `audio.c` tiene un estado armado/desarmado: hasta que `main.c` llama a `audio_fade_in()` — después de
las etapas del arranque que corren con interrupciones apagadas, incluido el arranque del segundo núcleo (hasta
20 ms, siete medias de 2.9 ms) — el master entrega **cero exacto**. El desmute cae sobre ceros, las medias que
se pierden en esas secciones se pierden sobre ceros, y la rampa 0 → full (~27 ms) ocurre ya con el sistema
estable, dentro del hold del splash: no agrega tiempo de arranque.

`tests/boot_fade_test.c` lo fija en host x86: con ganancia 0 el bloque es silencio digital exacto, la rampa es
monótona (0 → 2 809 → 5 578 → 10 847 de pico) y en 32768 el render es byte a byte el mismo que sin fade.

### DIST > FOLD
Quinto modelo del insert: un plegador de onda triangular. Lo que pasa el punto de plegado vuelve espejado, así
que DRIVE suma parciales hacia arriba donde SOFT, TAPE, HEARTH y FLUX saturan. El período es una potencia de
dos (8192 → 1024 con DRIVE), de modo que envolver y plegar son una máscara y una resta: ningún divide en el
camino de audio. La salida se re-escala para que DRIVE cambie el timbre y no el volumen.
`tests/dist_models_test.c` pincha los cinco modelos (suenan, quedan en 16 bits, FOLD loudness-neutral y sin DC,
los cinco renders difieren, y a DRIVE 0 el TYPE queda fuera de la cadena).

### La fuente S, comprimida (1 224 B de flash recuperados)
La cara que más se dibuja pasa a Huffman como M y L, con el mismo códec: decodifica a los mismos píxeles
(3 056 capturas del barrido, 0 difieren byte a byte). Es lo que pagó el presupuesto de FOLD con la ranura en
8 B libres: la imagen queda en 582 428 B de 583 612.

### De paso
`tests/text_spacing_test.py` leía el flag `adv8` como si fuera la tabla Huffman y decodificaba toda cara como
nibbles; con la cara L (adv8 off, HC presente) reventaba con `IndexError`. Ahora lee el puntero del final de
`aafont_t` y vuelve a medir.

## v1.4.10 — el panel: la tabla de init vuelve completa, con los valores del driver del módulo

### Qué se revirtió, y por qué
**Se revierte la decisión de 1.4.2.1 de no escribir nada después de MADCTL** (y con ella la de 1.4.4.1 que la
repitió). Aquella vez se quitó la gamma y el brillo porque los números de 1.4 se habían inventado comparando con
el emulador y en la unidad aplastaban el medio tono bajo — y eso era correcto. Lo que no era correcto es la
conclusión: quitar los registros dejó el panel en sus **defaults de encendido** para VGH/VGL, VCOM, alimentación y
gamma, un estado que nadie eligió ni midió. Una pantalla casi negra con fragmentos legibles es justo lo que se ve
cuando ese bloque no es el del módulo. Quitar un valor equivocado no es escribir el bueno.

### Lo que hay ahora
`LCD_SEQ` (`firmware/src/lcd.c`) lleva la secuencia **completa** de un ST7789 de 240x240, byte por byte, tal como
la publica el driver con el que se usan estos módulos:

```
0x01 SWRESET · 0x11 SLPOUT · 0x3A 0x55 COLMOD · 0x36 0x00 MADCTL
0xB2 0C 0C 00 33 33   PORCTRL (los porches)
0xB7 35               GCTRL (VGH/VGL)
0xBB 19               VCOMS (la tensión de común)
0xC0 2C               LCMCTRL
0xC2 01               VDVVRHEN
0xC3 12               VRHS
0xC4 20               VDVS
0xC6 0F               FRCTRL2
0xD0 A4 A1            PWCTRL1
0xE0 D0 04 0D 11 13 2B 3F 54 4C 18 0D 0B 1F 23   PVGAMCTRL
0xE1 D0 04 0C 11 13 2C 3F 44 51 2F 1F 1F 20 23   NVGAMCTRL
0x21 INVON · 0x13 NORON
```

Es el bloque de `init_240x240` de Adafruit_ST7789 (el mismo que llevan TFT_eSPI y Pimoroni para 240x240). La
diferencia con 1.4 no es el tamaño de la tabla: es de dónde salieron los números. Entonces se ajustaron contra el
emulador; ahora son los del driver del módulo, sin tocar uno solo.

**Siguen fuera, y a propósito:** el brillo (`0x51`/`0x53`, el backlight de la FM-1 es un GPIO y la unidad ignoró
el registro cuando 1.4 lo escribió) y CABC (`0xC7`, atenuación adaptativa en la pantalla de un instrumento).

### La prueba cambia de regla, y no se debilita
`tests/panel_table_test.py` antes **prohibía** `0xE0/0xE1/0xBB`; ahora **exige** la secuencia completa con esos
bytes exactos, cada registro una sola vez, enviada entre `SLPOUT` e `INVON`, y sigue fallando si vuelven el brillo
o el CABC, si falta un comando o si la tabla deja de parsear. La regla vive en `AGENTS.md` §8 en el mismo commit:
los valores del panel nunca se inventan contra el emulador, y se escriben completos.

### Clase y límite
La tabla es la del driver publicado y se verifica en el código: eso es estático y está medido. **El efecto en la
unidad no está medido** — no hay FM-1 en este entorno. Si al encender el contraste no es el de los módulos que usan
la variante de Pimoroni para 240x240, son dos bytes (`GCTRL 0x14` y `VCOMS 0x37` en vez de `0x35`/`0x19`) y se
prueban. `off N` sigue en la consola para la otra mitad del problema: si la imagen aparece entera pero desplazada,
es la franja de la memoria, no la tabla.

## v1.4.9 — toda la UI auditada: el dibujo está limpio, y `off N` pregunta cuál franja de la memoria ve el panel

Esta versión integra la 1.4.8 del trabajo paralelo (el velo de los pozos del visualizador y los 448 B de XIP que
recuperó la reverb y las avanzadas de las fuentes) con la auditoría de pantallas y la franja del panel de este
lado; el 1.4.8 ya publicado no se reescribe.

### La auditoría, con números
El renderizador del propio firmware (`tests/ui_render.c`, el mismo código de dibujo que corre la unidad) se pasó
sobre **190 pantallas × 22 paletas** — 3 040 cuadros — más el barrido de páginas y valores del MENU: **0 hallazgos**
de lint (ningún texto se sale de su celda, ninguno se pisa con otro, ninguno queda cortado por su canvas, ninguno
toca un divisor), **0 tintas sobre su propio color**, y **3 776 583 medidas de alineación** de 74 sitios, **0** desviadas
un píxel o más. En NIGHT, GREY, MONO, PAPER, LARGE, LINE y FLAT. Dicho de otro modo: **no hay bug de composición
en ninguna pantalla**. Lo medido es el dibujo; lo que la unidad muestra no es el dibujo.

### Si el dibujo está bien, lo que cambia es qué franja de la memoria se ve
La memoria del controlador es **240x320** y el panel **240x240**: está pegado a una franja de 240 filas de esa
memoria. Todo el firmware escribe la pantalla en las filas **0..239**, que es la franja de arriba. Los módulos
ST7789 de 240x240 que todo el mundo usa (Adafruit, Arduino_GFX, TFT_eSPI) están pegados a la franja de **80**. Con
esa franja y este firmware, la imagen aparece 80 filas más abajo de donde se escribe: **la cabecera y los títulos de
cada página quedan fuera del vidrio**, el tercio de abajo enseña memoria que nadie dibuja, y lo que llega a la vista
son fragmentos y palabras sueltas — exactamente el reporte, y también el del splash, cuyo cielo y cuyo texto están
en esas filas de arriba.

Por eso el comando nuevo **`off N`** (consola) mueve la ventana de la UI a la franja N de la memoria, rellena el fondo
de pantalla completa de las filas que quedan fuera (0..N-1, no siempre 240..319) y fuerza un repintado completo.
`status` publica **`lcd_off`** con el valor en uso. La medición es del dueño, en un segundo: `off 0` y `off 80` en la
misma unidad; la que muestre la pantalla entera es la buena. Cuando lo diga, el valor se fija en el próximo build y
el comando se queda como herramienta de taller.

### Sigue en pie lo de 1.4.7
La espera de la DMA del panel ya se escala con la transferencia (antes se agotaba a los 64 ms y cortaba los blits
de cuadro completo a los 77 ms) y `status` sigue publicando `lcd_timeouts`. Son dos causas distintas del mismo
síntoma y cada una tiene su lectura: `lcd_timeouts` dice si el envío se cortó, `off N` dice si el panel mira otra
franja.

## v1.4.7 — la pantalla de la unidad: la transferencia del panel ya no se corta a mitad de cuadro

El dueño reporta, en la FM-1, que **todas las pantallas siguen casi negras** (el splash incluido), que sólo se ven
partes y que faltan palabras. Después de descartar la gamma (1.4.4.1 dejó de escribirla y el síntoma no cambió),
la causa está en el camino de datos del panel, no en el dibujo: lo que el firmware pinta es idéntico al del
emulador (los 303 dorados y el barrido lo cubren), pero lo que llega al panel no es el cuadro entero.

### La espera de la DMA tenía un tope fijo, más corto que un cuadro completo
`fm1_lcd_wait()` (`hal/fm1_lcd_hw.h`) esperaba **4 000 000 de iteraciones para cualquier transferencia**. A 12 MHz
un byte tarda 8/12e6 s = 666 ns y una iteración de ese lazo es una lectura de periférico (~16 ns en este núcleo),
así que un blit de 240x240x2 = **115 200 B (≈77 ms)** necesita unos **4,8 millones**: el tope se agotaba antes,
limpiaba el flag de pendiente y deseleccionaba CS **con la DMA viva**, y el panel se quedaba con la parte del
cuadro que hubiera llegado. Eso es exactamente "sólo se ven algunos detalles": los blits que entraban en el tope,
y los demás truncados. El splash y las pantallas de cuerpo entero son los blits más grandes, por eso también.

Ahora el tope es **el de la transferencia**: `200 + bytes * 160` iteraciones (~2,5 µs por byte, tres veces lo que
necesita el cable) y sólo se paga entero cuando la transferencia de verdad no termina. Clase: build AC791N y
código leído; **no medido en la unidad**.

### `status` dice si alguna se cortó
`lcd_timeouts` sale en la consola: 0 significa que ninguna transferencia agotó su tope (y entonces lo que falla es
el panel, no el envío); un número que sube, que era esto. Es la lectura que convierte el próximo reporte en una
medición en vez de una impresión.

### El instalador ya no reproduce el arranque
La tarjeta "el instrumento encendiéndose" del instalador web arrancaba el `felucca.wasm` en la página y mostraba el
splash animado sin relación con lo que el jugador estaba haciendo. Se quitó de `web/index_pkg.html` (sección,
CSS, textos en los tres idiomas y el guion que lo arrancaba): el instalador vuelve a ser lo que hace.

## v1.4.6 — el panel vigilado por una prueba, la memoria legible en la unidad y `mem` en tres líneas

Dos agregados, los dos pensados para que lo que se midió en el host se pueda comprobar (o volver a comprobar) sin
depender de la memoria de nadie. Sin cambios de sonido ni de dibujo: los 303 renders dorados y el barrido de UI no
se mueven.

### `tests/panel_table_test.py`: la pantalla negra no puede volver sin que falle el build
1.4 escribió una curva gamma y encendió el brillo del ST7789 desde una comparación con el emulador; en la FM-1 eso
aplastó los tonos bajos de todas las pantallas y 1.4.2.1 lo deshizo. Lo que quedaba era disciplina y un comentario:
ninguno de los dos es una prueba. La nueva prueba lee la tabla `LCD_SEQ` de `lcd.c`, la parsea entrada por entrada y
falla si aparece **0xE0, 0xE1, 0x53, 0x51, 0x55 o 0xBB** (gamma, brillo, CABC, VCOM), si la tabla deja de parsear
limpia, o si desaparecen las tres que hacen falta para ver algo (`0x3A` con `0x55`, `0x36`, `0x21`). Verificada en
las dos direcciones: verde con la tabla actual, roja metiendo `0x53, 1, 0x24` en una copia. Corre en `run_tests.sh`.

### `mem`: el presupuesto de memoria, preguntado a la unidad
El comando de consola `mem` imprime, leídos de los símbolos del linker, los mismos números que el build:
`ram_used` 91 300 de 98 304 (`ram_free` 7 004), `lowbuf_used` 30 564 de 32 768 (`lowbuf_free` 2 204),
`pool_used` 318 432 de 327 680 (`pool_free` 9 248). Es la única parte del trabajo de memoria que un host no puede
contestar por una unidad: ahora el dueño escribe `mem` en la FM-1 y compara con lo que reportó el CI. Clase: el mapa
del linker, no una medición — no dice cuánto de una pila está USADA.

### Lo que cambia en los números
La imagen sube de 583 172 a **583 384 B** (228 libres de 583 612): el comando y sus claves cuestan ~210 B de XIP, y
se recortaron dos claves que eran constantes de compilación para no gastar más. `.data+.bss` sigue en 91 300 B.
`tests/memory_map_test.py` tenía la ventana LOWBUF vieja en 0x01C07000: corregido a 0x01C08000 (si no, la prueba
fallaba con la mudanza de 1.4.5). `./build.sh` ahora imprime LOWBUF junto a RAM y pool.

La imagen de 1.4.6 dejó **228 B libres de 583 612** en la ranura de la aplicación. Antes de tocar nada se buscó lo
que el enlazador ya tira y lo que queda vivo, porque la pregunta era "¿hay código o constantes que ya no se usen?".

#### Lo que se buscó, y lo que se encontró (clase: build AC791N)
- **Código muerto: no queda.** `ld --gc-sections --print-gc-sections` sobre los mismos objetos quita exactamente
  siete secciones (`.text.fx_master_eq_db`, `.text.fx_boot_gain`, `.text.fx_master_g`, `.text.fx_lim_env`,
  `.text.scl_toggle`, `.text.scl_tap`, `.text.ui_tap`): todas son la copia fuera de línea de una función que el
  compilador ya inlinó en el único TU del firmware. Un relink con esa bandera da un `.text` **idéntico byte a byte**
  al del build, así que no hay nada más que quitar: `--gc-sections` ya lo quitó.
- **Constantes muertas: cero bytes.** El compilador ya emite cada tabla en su propia sección (`.rodata.SINE`,
  `.rodata.GENRE`, …), así que el `--gc-sections` las descarta por separado. Probado con `-fdata-sections` en
  `CFLAGS`: la imagen pasó de 583 384 a **583 388 B**, cuatro bytes *más* (el relleno de alineación de más
  secciones). Se revirtió: no había nada que ganar.
- **132 constantes de 96 B o más** (`.rodata` entre `_ecode` y `_etext`, 139 368 B en 1.4.6) aparecen todas
  nombradas en el desensamblado: ninguna es huérfana.

#### `mem`: de seis claves a tres líneas
`mem` imprimía seis pares `clave valor` (`ram_used`, `ram_free`, `lowbuf_used`, `lowbuf_free`, `pool_used`,
`pool_free`): 62 B de claves en XIP para un comando diagnóstico. Ahora imprime `usado/límite` por ventana, con el
mismo dato (el libre es la resta):

```
ram 91300/98304
lowbuf 30564/32768
pool 318432/327680
```

Medido: **rodata −48 B, código +4 B, imagen 583 384 → 583 340 B**; libres en la ranura **228 → 272 B**. `.data+.bss`
sigue en 91 300 B, pool y LOWBUF también. Clase: mapa del linker (el build AC791N), no una medición de la unidad.

## En el árbol (1.4.4.1, `6d33c44`) — el build AC791N de ese árbol, medido
## En el árbol — FASE EXTRA v1.4.8: los osciloscopios del visualizador se ven a través

Los dos pozos del visualizador de HOME (`graph_home_viz`) eran `cv_rect(T_SURF)`: placas opacas que tapaban el
campo profundo —la banda de la Vía Láctea, su polvo, sus dos mundos y las brasas—. Ahora son un velo:
`cv_rect_a()` mezcla un 60 % de tono de página sobre **los píxeles que ya están en el lienzo**, píxel a píxel,
con el mismo `ux_mix()` que usa la tinta translúcida (`cv_pset`, `cv_ink`); el fondo se lee a través del
cristal y la rejilla y la traza, dibujadas después, siguen siendo lo más brillante del pozo. Es blending real
del framebuffer (que es legible), no un color sólido ni un dither que lo imite.

Sin fila de menú: **se escribió y se midió MENU > EXTRA > VIZ VEIL (id 25, MID / CLEAR / DEEP)**, y no entra.
Con el trabajo de panel de 1.4.7 la imagen quedaba 152 B por encima del XIP; la fila cuesta ~90 B y la versión
barata del velo otros ~60, así que el 60 % quedó fijo en `VIZ_VEIL` y la imagen cierra en **583 564 de
583 612 B (48 B libres)**. Decirlo es parte del resultado: no hay holgura para un ajuste, y menos para las
fases 3–6, sin liberar flash de los assets.

Medido en el host (`cc -O1`; sólo las proporciones significan algo en el aparato): la pantalla del
visualizador pasa de **213,3 a 363,4 µs** por cuadro en HIGH (+70 %). La versión con tablas de 128 entradas
bajaba a 247,5 µs pero costaba 144 B de XIP, que ya no existen.

El barrido de `ui_render` queda en **0 hallazgos** de lint sobre 190 pantallas × 22 paletas, **0 píxeles GREY /
MONO fuera de gris o neutro** y 0 alineaciones fuera por 1 px; los volcados de QUANTUM, MONO, GREY y PAPER
muestran la banda y las estrellas atravesando los dos pozos con la traza legible.

## En el árbol — el build AC791N de este árbol, medido

La cadena de destino (`tools/get_toolchain.sh` + los tres archivos del SDK AC79) se descargó y verificó en esta
sesión, así que por primera vez en este trabajo el árbol **se compiló para el aparato**: imagen **583 156 B de
583 612** (456 B libres), `.data+.bss` **95 140 de 98 304**, pool con **9 248 B** libres (el mínimo es 8 192),
RAM code 4 868 B libres y **`.noinit` con 124 B** — el recurso más apretado del proyecto hoy. El paquete sale con
identidad `FM-1_965` y manifiesto `v1.4.4.1` en `6d33c44`.

`tests/memory_map_test.py` pasa con ese `.elf` y `tests/target_budget.py` queda en **+0 %** en todos los lazos
del DSP. No se publica un release nuevo: la versión sigue siendo 1.4.4.1 porque el emulador WASM necesita
`emcc`, que no está en este entorno, y el sitio tiene que servir el emulador del mismo commit.

## En el árbol — Web Studio: MOD ROUTING, la matriz de un vistazo

El grupo **MOD** del panel de sonido mostaba los cuatro slots como tres selectores y un número, uno debajo de
otro: para saber qué modula qué había que leer doce campos. Arriba de esos slots ahora hay una tarjeta
**MOD ROUTING** con una línea por slot activo — número, fuente, destino y la cantidad con su signo — y marca en
ámbar los destinos que dos slots comparten, porque ahí las dos cantidades se suman. Se arma leyendo los mismos
controles que recibe el dispositivo (no una copia), así que se actualiza cuando llega un valor del aparato o
cuando se mueve un selector, y no cuesta nada en el firmware: el dispositivo no cambia ni un byte.

## En el árbol — auditoría de RAM/flash sin toolchain

`tools/ram_flash_map.py` (nuevo, gate en la suite) compila las fuentes para host y publica, por sección de
`app.ld`, todos los objetos de más de 1 KiB con su tamaño: cuánto pesa cada buffer de la piscina, cada tabla y
cada array de estado. Con eso se comprobó que **no hay ninguna tabla de rodata duplicada** (0 grupos de 680
objetos: el mismo flash no se paga dos veces) y quedaron medidos los mayores consumidores de RAM estática
(`WT_GEN` 12 288 B, `trk` 10 560 B, `fm6_note` 7 104 B, `drum_kit` 5 376 B).

**No se tocó el DSP ni el reparto de voces**: sin `JIELI_TOOLCHAIN`/`AC79_SDK` no hay mapa del linker ni
presupuesto de XIP (328 B libres a 1.4.2) ni de `.data+.bss` (3 164 B), y un cambio de ese tipo sin medir en
el destino y sin las capturas doradas idénticas sería una suposición. Los candidatos concretos están en
`docs/VALIDATION_MATRIX.md`, con su clase.

## v1.4.4.1 (en el árbol) — MASTER > STEREO M/S movía el release del FM3 de la pista

Integrado desde el trabajo paralelo, encima de 1.4.4.1: la matriz de controles nueva
(`tests/control_map_test.c`) encontró una colisión de ids. `P_DXIMG` (`P_FM3_REL`) es **74** y `G_MWIDTH`,
el ancho estéreo del master, también es **74**; en `params.c page_desc()` la rama de **ENV VEL** iba antes del
despacho por `scope` y comparaba el número pelado, así que **MASTER > STEREO M/S** contestaba `VEL` y escribía
`t->p[P_DXIMG]`: el ancho maestro era inalcanzable desde su página y el knob cambiaba el release del FM3 de la
pista seleccionada. La rama ahora exige una página de pista (`pg->scope != SC_GLOBAL`).

El barrido cubre 86 páginas × 10 motores y cada estado que una página calcula (8 links de FX > BUSES, 8 slots de
MOD, 16 pasos de MSEQ y SL PITCH): 23 763 comprobaciones, 0 fallos, con la pareja de regresión (`STEREO M/S` →
`M-WIDE` sobre `song.g[G_MWIDTH]`; `VOICE 2` → `VEL` sobre `P_DXIMG`).

### POLY: medido por todos sus caminos, no reproducido
`tests/poly_voices_test.c` (la suite) recorre el asignador, el reparto entre seis partes, el gobernador de carga
(`load_tight`, `LOAD` LITE / ECO), el presupuesto de MENU VOICES, el teclado y la mezcla renderizada: 18
comprobaciones en verde, incluidas 12 voces para 12 notas y 4 por parte con `LOAD` ECO. En un mix de seis partes
las 12 voces son compartidas y los acordes se roban entre sí por diseño. El asignador no se tocó.

## v1.4.4.1 — la pantalla oscura en la unidad, las filas que se trababan y el pop del arranque

(Pedida como 1.4.2.1: se publica como **1.4.4.1** porque el proyecto ya está en 1.4.4 y el sitio y el instalador
rechazan un número de versión menor — lo comprueba `make_release --check`.)

Tres causas reales, encontradas en el código, y una tercera que era un ajuste imposible.

### La pantalla oscura (y las cosas negras) en la FM-1 real
1.4 empezó a escribir en el ST7789 una **curva gamma 2.2** (0xE0/0xE1) y a **encender el control de brillo al
78 %** (0x53/0x51) — valores elegidos comparando contra el emulador y **nunca medidos en un panel**, como el propio
CHANGELOG de 1.4 admite. En la unidad eso aplasta el extremo bajo del rango: las placas (`T_SURF`), los divisores
`LINE`, las etiquetas `DIM` y los pozos de los osciloscopios se van a negro, y el síntoma aparece en **todas** las
pantallas porque es el panel, no el dibujo — el splash incluido. Los dos escritos se fueron: el panel arranca con
sus valores de fábrica, como en 1.3.9, que es como se veía antes.

### MENU > BRIGHT no podía funcionar, y se fue con la causa
El brillo del registro 0x51 no hace nada en esta unidad porque **no hay camino de atenuación**: el backlight es el
GPIO PA2, encendido/apagado, y además es el /OE del 595 del teclado (con PA2 alto no se lee ni una tecla), así que
tampoco se puede PWM. Ese registro sin efecto era además el que podía dejar el panel atenuado. La fila BRIGHT se
elimina con él: un control que no puede funcionar es peor que ninguno. Fuera también el byte de ajuste.

### Las filas del MENU que "se trababan" (SPLASH, CPU MODE, VIS QUALITY, VOICES, TRACKS…)
`draw_menu()` no redibuja mientras una **firma escrita a mano** de unos pocos ajustes no cambie, y esa lista nunca
creció con las filas que agregó 1.4: SPLASH, CPU MODE, VOICES, TRACKS y VIS QUALITY **cambiaban el valor y dejaban el
texto viejo en pantalla**, que es exactamente "se traba al cambiar entre FULL, SHORT y OFF". Ahora `menu_put()`
invalida la firma del menú en **cada** cambio de fila: arregla la clase entera, incluidas las filas futuras, y con
`ui_test.c` recorriendo todas las filas para que no vuelva.

### Pop de audio en el splash
El codec y su amplificador suben mientras los primeros bloques ya se están renderizando: ese escalón iba derecho al
parlante debajo del splash. La salida del DAC ahora **entra con un fade de 1200 muestras (~27 ms)**: demasiado lento
para oírse como rampa, suficiente para que el primer bloque arranque en silencio. Medido en host: pico del primer
bloque 4 480 contra 942 528 después del fade (0,5 %).

### SPLASH FULL por defecto, y una prueba de los tres modos
El arranque sigue por defecto en **FULL animado** (0), con SHORT y OFF elegibles. `tests/startup_test.c` arranca la
unidad **en los tres valores** y comprueba: la fila lee lo que el equipo arrancó, OFF no dibuja un solo fotograma de
splash, FULL/SHORT sí lo dibujan antes de la UI, y en los tres la UI aparece después.

## v1.4.4 — el POLY, medido en la salida (y por qué un bajo suena a una nota)

### El POLY no estaba roto: ahora hay una prueba que lo demuestra en el audio
`tests/poly_test.c` (nuevo, paso de la suite) toca **C3 · E3 · G3 en un mismo track en POLY** y mira la **salida**:
tres voces con gate y las tres fundamentales presentes en el espectro (Goertzel sobre 4096 muestras, cada una por
encima del 20 % de la mayor). Se repite con `load_tight` puesto —el gobernador de carga, la sospecha que quedaba— y
da lo mismo: tres voces, tres notas. Medido en host x86: C3 1402, E3 1385, G3 1331 (y 1461 / 1444 / 1382 con el
gobernador).

### Por qué un sonido puede no dar polifonía aunque el modo diga POLY
El modo de voz **es parte del sonido**: `ui.c apply_preset_to` pone `P_VOICE` desde el preset (`mono` → LEGATO). Un
bajo o un lead marcado mono carga el track en LEGATO y suena de a una nota por más que el modo estuviera en POLY
antes de cargar el sonido. La prueba lo fija: un preset mono carga LEGATO, dos teclas dan una voz, y elegir POLY en
la página VOICE devuelve el acorde —hasta el próximo cambio de sonido—. Si en la unidad se oye una sola nota con un
sonido así, no es el asignador: es el sonido. Elegí POLY en VOICE *después* de elegir el sonido.

## v1.4.3 — limpieza de lo que dejaron 1.4.0 / 1.4.1 / 1.4.2 (sin cambiar nada de lo que se ve)

Pasada con ojos frescos sobre lo que agregaron esas tres versiones. **No cambia el comportamiento**: el arnés de
render da los mismos píxeles en 2 992 de las 3 040 capturas (las 48 restantes son ABOUT y el splash, que imprimen
`__DATE__`, la fecha de compilación), 0 hallazgos de lint y 0 medidas de alineación fuera de 1 px; los contrastes de
las 22 paletas y la suite host completa pasan.

- **Paletas:** `panel_dark()` y `PANEL_DARKEN` se fueron. Desde 1.4.2 el factor era 1.0: la función sólo hacía un
  `round()` por canal y una ida y vuelta por RGB565 para no cambiar nada. El header generado es idéntico byte a byte.
- **Pozos de los osciloscopios:** `vwell` era un alias de una línea usado dos veces; se usa `T_SURF` directo y el
  comentario de FASE 5 —que todavía describía el pozo *mezclado*, reemplazado en 1.4— se corrigió en vez de quedar
  como documentación falsa.
- **Firma de repintado de HOME:** `hv_view_sig()` hasheaba el motor y el preset de `TSEL` aparte y otra vez dentro
  del bucle de las seis pistas, porque `TSEL` es una de ellas. Esa línea era redundante y se fue; la firma es la
  misma función de lo que la pantalla muestra.
- **Popup de presets:** el `(void)r;` que quedó del patrón de líneas borrado en 1.4.1 se fue, y con él el comentario
  que explicaba trabajo que ya no existe.

## v1.4.2 — el brillo del panel se ajusta desde el equipo (MENU > EXTRA > BRIGHT)

La gamma 2.2 (0xE0/0xE1) y el brillo 200/255 (0x53/0x51) que 1.4 escribe en el ST7789, y el oscurecido al 72 % de
las paletas, salieron de comparar contra el emulador: **no están medidos en ninguna unidad**, y un panel con otro
pegado puede pedir otro valor. Hasta acá la única forma de moverlo era un build nuevo; ahora hay una fila.

**MENU > EXTRA > BRIGHT**: 25 %, 50 %, 78 % y 100 % (el valor por defecto, y el que lee un registro de ajustes que
nunca tocó la fila). Escribe el registro 0x51 en caliente al paso del knob, y el valor queda guardado en un byte
libre del bloque del motor 15 (`favorites.factory[15][25]`, 0 en todo registro viejo = 100 %): al arrancar, `main.c`
aplica lo guardado antes del splash, así que no hay un fotograma al brillo de fábrica. La secuencia de init del
panel ya no lleva 0x53/0x51: hay **un solo escritor**, `lcd.c lcd_bright_set()`, en el arranque y en el cambio.

### Los temas quedaban casi negros: el oscurecido de las paletas se fue
1.4 guardaba los dos colores de suelo de cada tema oscuro al **72 %** de cada canal para compensar la gamma del panel.
El mismo release empezó a escribir la gamma real del ST7789 (0xE0/0xE1) **y** un brillo del 78 % (0x53/0x51): las dos
correcciones se apilaban, el dato iba 28 % más oscuro y el panel comprimía otra vez el extremo bajo. Fondo,
divisores `LINE` y texto `DIM` desaparecían — "todos los temas quedan prácticamente negros". El oscurecido se fue
(`PANEL_DARKEN = 1.0`): las paletas vuelven a sus valores de diseño, que son los mismos que ven el emulador y el
sitio, y la compensación queda donde está el panel (el driver, y BRIGHT para lo que sólo la unidad puede juzgar), no
en los datos. La tabla de contrastes del generador pasa entera con los 22 temas (texto/fondo 14,2–21,0), el barrido
de render da 190 pantallas × 22 paletas con **0 hallazgos** y 0 medidas de alineación fuera de 1 px, y las hojas
`build/ui_new/sheet_*.png` de GAMMA, NIGHT y PAPER se miraron pantalla por pantalla.

Lo que **no** cambió y sigue sin medir: la curva de gamma del panel y el nivel de brillo que elige el dueño. Eso no
se resuelve desde acá, necesita la unidad: por eso BRIGHT es un ajuste y no una constante, y así queda en la matriz.

## v1.4.1 — el LFO SHAPE tenía tres knobs para dos valores, y el knob PRESETS repintaba todo

Dos de los cuatro puntos del reporte del dueño. El POLY no se reprodujo: abajo, lo que se miró y lo que falta.

### LFO SHAPE: SLEW, INVERT y otra vez INVERT, y cualquier knob movía los tres
La página tiene **dos** parámetros y su tabla los nombra así: `{"LFO SHAPE", ..., {P_X2, P_X3, 0xFF, 0xFF}}`, con
`P_X2` el SLEW y `P_X3` el INVERT. Pero `params.c page_desc()` tiene una rama propia para esa página —porque el
`SLEW`/`INVERT` no son descriptores de la tabla de parámetros sino dos structs suyos— y esa rama atendía **los
cuatro slots** igual: devolvía `D[slot & 1]` y `&t->p[slot ? P_X3 : P_X2]`. Con eso el knob 2 leía INVERT, el 3
volvía a leer SLEW y el 4 leía INVERT otra vez, y los tres escribían **el mismo byte** (`P_X3`): mover cualquiera
movía los otros dos. Es exactamente lo que reportó el dueño. Ahora los slots que la tabla deja en `0xFF`
responden sin parámetro, como en toda página vacía: el render de la página muestra **dos** tarjetas, SLEW 69 % e
INVERT 29 %, y las otras dos quedan libres (`build/ui_new/*/lfo_shape.png`; el barrido de alineación del arnés
bajó de 3,89 M a 3,78 M medidas porque las dos tarjetas fantasma ya no existen).

### El knob PRESETS hacía parpadear la pantalla
Cada paso del knob hacía `ui.force = 1`, y eso redibuja **el cuadro entero** —cabecera, las cuatro tarjetas, el
grafo y el pie— con el popup de presets encima; al vencer el popup, otro cuadro entero para quitarlo. En el panel
eso es el parpadeo. El nombre del sonido y todos sus valores viven en el grafo, y `graph_signature()` ya lleva el
preset: ahora el paso del knob pide **sólo el grafo** (`ui.graph_sig = 0`) y, cuando el popup se va, se repinta
sólo esa zona. Además el popup dejó de pintar su patrón de líneas en `T_BG` antes de dibujar su placa: era
trabajo tapado por la placa y, en las esquinas redondeadas, rayas de fondo sobre la página.

### POLY: no reproducido en el modelo
El señor dice que el modo POLY no da polifonía. En código no lo encontré: `trk_note_on()` en POLY toma
`voice_alloc()` para cada nota (una voz nueva por nota, sin tocar las que suenan), el pool reparte 17 slots entre
6 partes (2 por parte, hasta `NVOICE` con los libres), `trk_vcap()` sólo limita por `ENGINES[..]->poly` (FM6 2,
FORMANT 4, el resto 12) y el teclado es por tecla (`kb_note[27]`, un note-on por tecla). `tests/audio_test.c`
ya toca acordes de tres notas en POLY y pasa. La sospecha que queda es el gobernador de carga: con `LOAD` LITE /
ECO o con `load_tight` puesto por el render, `voice_room()` deja de crecer y se lleva la voz que suena, y en host
la carga lee 0 (no se mide). Eso hay que verlo en la unidad, no adivinarlo: **no se tocó el asignador**.

## v1.4.0 — el splash animado, HOME sin repintarse de más, y los pozos de los osciloscopios

Cuatro cosas del reporte del dueño, cada una con su causa.

### STARTUP SPLASH FULL no se podía tener: se veía igual que SHORT
La animación del arranque estaba condicionada por **MENU > ANIM**: con `ANIM OFF` —el ajuste de la unidad del
dueño— `draw_splash()` escribía la firma entera de una vez y `splash_write()` volvía sin dibujar, así que FULL y
SHORT daban la misma imagen fija. Ese bit es de los valores (dígitos que ruedan, glides: `ui.c` ANIM_SNAP), no
del arte del arranque: ahora decide sólo el modo. Además el bucle del boot tenía un tope fijo de 600 pasos de
4 ms (2,4 s) mientras FULL dura `SPL_WRITE + SPL_HOLD` (4,5 s): el tope sigue al span, y el splash se escribe
hasta el final. FULL queda como opción por defecto (0 en el ajuste). Medido en host: con ANIM OFF la línea de
tiempo escribe 127 fotogramas distintos, y con ANIM ON 244 de 1125 pasos cambian la pantalla (la suite lo
comprueba en `tests/ui_render.c`).

### HOME 2 a 5 parpadeaban y se sentían lentas
OVERVIEW, MIXER, PERFORMANCE y SYSTEM redibujaban su cuerpo entero —tres canvas de 240 px y tres blits por
SPI— en cada fotograma, hubiera cambiado algo o no. Ahora hay una firma de lo que la pantalla muestra (página,
paleta, medidores cuantizados, CPU, batería, motor y preset de cada pista) y el cuerpo se dibuja **sólo cuando
esa firma cambia**; `ui.force` sigue mandando en un cambio de página o de tema. Las brasas del fondo pasan de
avanzar por fotograma a 1/8 de fotograma: siguen derivando, pero una pantalla quieta no gasta bus.

### Los osciloscopios tenían dos cuadros negros de fondo
1.3.9 hizo opacos los pozos oscureciendo `T_BG` otros 26 niveles: en el panel eso se lee como dos rectángulos
negros sobre la pantalla. El relleno hace falta (el campo y sus mundos no deben verse bajo la rejilla ni la
traza), así que ahora el pozo se pinta en el tono de la placa donde vive, `T_SURF`, con su marco de 1 px.

### La pantalla, y la mayoría de los temas, se veían más claros en la FM-1
Dos causas, dos arreglos. En los datos: los dos colores de suelo de cada tema oscuro se guardan al 72 % de cada
canal (PAPER queda claro a propósito), y todo lo derivado —MID, DIM, LINE, SEL, TINT— los sigue; la tabla de
contrastes del generador sigue pasando entera. En el panel: la secuencia del ST7789 escribía sólo el color y la
inversión y dejaba **gamma y brillo en los valores de fábrica**, que es lo que levanta el extremo bajo del rango
565; ahora se escriben las curvas 2.2 (0xE0/0xE1) y el brillo a 200/255 (0x53/0x51). Eso es un cambio de
valores por defecto: **no está medido en una unidad**, y así queda anotado en la matriz.

## v1.3.9.1 — un publicador de Pages, el emulador en el release y un índice de lo medido

Cambios de publicación y de evidencia: **no toca el DSP, ni la UI, ni el formato de proyecto**. El paquete
sólo se diferencia de 1.3.9 en la cadena de versión.

### Pages tenía dos publicadores
`pages.yml` subía `site/` tal como está commiteado y `emulator.yml` subía `build/site`, cada uno en su grupo de
concurrencia: ganaba el que terminara último y la portada podía ser cualquiera de los dos, nombrando la versión
de dos maneras (`1.3.8` contra `v1.3.8-<sha>`). Publica sólo el workflow que construye el sitio; las dos
comprobaciones que `pages.yml` aportaba (`make_release --check` y `test_site`) viven ahora en `coherence`, que
ya fija Python y hace checkout completo. Medido en el sitio en vivo: la raíz sirve `v1.3.9-33e9b0e`.

### El release no traía con qué escuchar el firmware
Se adjunta `qg1-emulator-<v>.zip`, armado por `make_release --sync` desde `site/emulator/` (la copia
versionada, no la generada: `release/emu/fonts/` está en `.gitignore` y un checkout no la tiene) con nombres
ordenados y fecha fija, así que dos corridas dan el mismo sha256. `--check` verifica que cada miembro del zip
sea el fichero que está al lado.

### El run decía verde y el asset no existía
El paso que publica el release daba por bueno cada `gh release upload`: en v1.3.8 el archivo del emulador
quedó en la lista y no en el release, un 404 para quien lo descarga. Ahora el paso **vuelve a leer los assets**
al terminar, los publica en el resumen del run y falla si falta alguno.

### La atestación se podía leer sólo con credencial
`gh attestation verify` corría en el job `verify` y su salida quedaba en el log, que pide token. Ahora se
guarda como artefacto `attestation-<v>` (JSON + sha256 del paquete, 90 días) al lado del run.

### Peso del repositorio
`site/firmware/` se queda con el paquete actual y los dos anteriores; los más viejos siguen en su página de
release. Eran cuatro copias de 600 KB de la misma imagen.

### El índice de la evidencia ya no se escribe a mano
`docs/EVIDENCE.md` se genera desde `docs/VALIDATION_MATRIX.md` con `tools/evidence_index.py`, y `--check` está
en la suite del host: una fila que vuelve a PENDING rompe el árbol en vez de esperar a que alguien la lea.
En la primera corrida aparecieron tres filas PENDING que ya estaban medidas (el audio del LATCH en
`tests/perform_test.c#test_latch_audio`, el WASM y la atestación) y una cuarta, el mapeo control→DSP, que
enumeraba como medido lo que no lo estaba.

## v1.3.9 — la banda de HOME, el visualizador entero y FX legible

Los seis bugs de UI del reporte, cada uno con su causa. Medido en host x86 (render) y en el enlace del AC791N.

### El visualizador mostraba las tarjetas de HOME
Sus dos pasadas de 88 filas empiezan en la fila 26 y nadie pintaba `Y_LABEL` (20) .. 25: `draw_frame()` sólo
rellena los huecos **entre** tiras, así que los bordes de las cuatro tarjetas de knobs que HOME acababa de dibujar
seguían en pantalla — los "cuadrados" del reporte. Una pasada-cap de 26 filas del tono de la página, bliteada en
`Y_LABEL`, las cubre; las pasadas dibujan después sobre 26..202.

### La línea atravesaba OVERVIEW, MIX y PERFORMANCE
Esas tres vistas pintaban las seis filas de arriba con `T_SURF` liso y arrancaban su degradado en 26, o sea una
franja más clara sobre el fondo, y su nombre iba en `y = 34`, cuyas mayúsculas de `AF_S` ocupan las filas 37..45:
la raya de separación de `y = 41` lo cortaba por la mitad (en MIX se leía "MIX" y "ER" con una línea en medio).
El cap pinta ahora `hv_grad()` —la misma rampa de la página— y el nombre va en `y = 28` (tinta 31..39) en las tres
pantallas y también en ROUTING, con lo que la raya queda limpia bajo el texto.

### Los osciloscopios dejaban ver el campo
Los dos pozos del visualizador usaban el tono de la página **mezclado hacia `T_BG`**, pensado para que la vía
láctea se viera a través; con la rejilla y la traza encima, lo que se veía era el campo debajo del instrumento.
Cada pozo tiene ahora su propio tono opaco (`bg_darken(T_BG, 26)`), y el campo queda fuera de los dos.

### FX: BYP, el porcentaje y el OFF encima de sus barras
Los tres módulos de la primera sección dibujaban su lectura en `y = 99`, y su barra ocupa 108..116: el texto caía
sobre el borde de la barra. Van en `y = 94` (tinta 97..105), y la rampa de esos tres módulos baja de 22..62 % /
10..45 % a **14..50 % / 6..32 %**, con `ux_ground_at(13, 37, 6, 32)` para que el texto siga el mismo degradado:
los cuadros se leen más oscuros y el texto no compite con el relleno.

### Coste
**+128 B** de imagen: 582 644 → **582 772 B de 583 612** (840 B libres), `.data + .bss` sigue en **95 124 B de
98 304** y el paquete en **610 959 B** con identidad **FM-1_965**. Nada se mueve en RAM.

### Cómo se comprobó
`tests/ui_render`: 190 pantallas × 22 paletas, **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
GREY/MONO fuera de gris/neutro, **0 desalineaciones en 3 894 154 medidas**. Los volcados de `home_viz`, `home_mix`,
`home_perf` y `fx` muestran las filas 20..25 en un solo color y las tres lecturas de FX separadas de sus barras.
No hay unidad FM-1 en este host: nada de esto es una medición de hardware.

## v1.3.8 — el banner se vuelve a dibujar desde cero, y el portal deja de estar en blanco

### El banner es otro, y lo dibuja el repositorio
- **`tools/gen_banner.py` se reescribió entero.** El motivo ya no es el planeta: es un estado ligado. Un
  campo de densidad de probabilidad —anillos de fase con caída 1/r, dibujados antes que el texto y a baja
  opacidad—, un núcleo con tres órbitas elípticas a 60° y un electrón en cada una, una nube de puntos
  repartida por ángulo áureo y una escalera de niveles `n=1..5` con la flecha de absorción. La rejilla de
  cruces y los puntos discretos son la pantalla del aparato, no un campo de estrellas: la semilla es fija,
  así que la imagen se regenera idéntica y se puede revisar en el diff.
- **Nada del banner es inventado.** Los colores salen de la paleta QUANTUM decodificada de `ui_palettes.h`
  (`0x0022` fondo, `0x10a5` panel, `0x3eff` acento, `0xfd44` tema), las tipografías son las de
  `assets/fonts` y las dos imágenes pequeñas son las pantallas reales de 240 × 240 —
  `quantum_01_home.png` (HOME / ORBITAL) y `quantum_03_fm6_algorithm.png` (FM6 / ALGORITMO) —, enmarcadas
  como referencia, no como maqueta.
- **Se mantienen las características y no aparece la versión.** Las píldoras dicen **12 voces, 6 pistas,
  10 motores, 285 presets, secuenciador de 64 pasos, 4 buses de FX y 19 paletas**, todas ellas cifras que el
  árbol sostiene (`UI_PAL_NLIVE` = 19, `preset_categories` = 283 + 2 kits, `engines.c` = 10). No hay
  `v1.3.x`, ni hash, ni build: el banner es el mismo en cada release.
- **Las cifras anteriores estaban viejas y se corrigieron de paso:** el banner anterior anunciaba **273
  presets** y **17 temas**; el README decía *nine synthesis engines* y *nine palettes including GREY and
  MONO*. Ahora dicen 285 y 19 paletas, y diez motores. `tools/generate_banner.py`, un segundo generador que
  la matriz citaba y que no producía el fichero publicado, se retiró.

### El portal raíz estaba commiteado con cero bytes
- **`site/index.html` estaba vacío en `main`.** `pages.yml` publica `site/` *tal como está commiteado*, así
  que el portal servía una página en blanco mientras el emulador, el instalador y los paquetes seguían
  respondiendo. Nadie lo vio porque `web/test_site.py` valida `build/site` —la copia que construye el
  workflow del emulador— y la coherencia miraba `versions.json` y el paquete, nunca el HTML del portal.
- **Arreglado regenerando `site/` con `web/make_site.py`** (portal con `{{VERSION}}` sustituido, `assets/`,
  `webapp/installer`, `webapp/editor`, `plan/` y `firmware/`), y con una guarda nueva en
  `tools/coherence_check.py`: `site/index.html` tiene que existir, no estar vacío y nombrar la versión que
  lleva el firmware. Un portal en blanco vuelve a fallar en CI en vez de publicarse.
- **Los paquetes ya publicados siguen descargables.** `site/firmware/` conserva 1.3.4 a 1.3.8, y cada
  `.manifest.json` de la carpeta se comprobó contra los bytes que tiene al lado: cinco de cinco coinciden.

### Lo que se midió en esta versión
- **Build del AC791N `v1.3.8`** (`d2fe9d2`): imagen **582 644 B de 583 612** (968 B libres), XIP código
  **422 928 B** + constantes **139 288 B**, `.data+.bss` **95 124 B de 98 304**, pool con **9 248 B** de
  sobra, RAM code **19 632 B de 24 576**. Paquete **610 959 B**, sha256 `6446de87…`, identidad `FM-1_965`.
- **El firmware no cambió de comportamiento:** la única diferencia con 1.3.7 es la cadena de versión, y el
  reparto de memoria es exactamente el mismo (mismos bytes, mismas constantes, mismo código). Esta release
  es de identidad, portal y documentación; no trae un motor nuevo y no lo finge.
- **Emulador `v1.3.8`** recompilado con Emscripten 6.0.11: WASM **1 240 205 B**, sha256 `a45e639e…`,
  etiquetado con el commit del árbol. `site/emulator/` y `release/emu/` son esa compilación.

### Lo que sigue sin poder afirmarse
- **Nada de esto se probó en una unidad física.** Las mediciones de arriba son **build del AC791N**,
  **host x86** y **emulador**; el FM-1 del dueño no se escribe desde este repositorio.

## v1.3.7 — 904 B de vuelta de los keycaps que nadie dibuja, y el precio medido del parámetro siguiente

### Lo que se recuperó, medido
- **Seis píldoras de keycap que ninguna pantalla dibujaba, fuera del generador.** `KC_ARP`, `KC_K3`, `KC_K4`,
  `KC_HOLD`, `KC_ON` y `KC_OFF` se rasterizaban en `tools/gen_aa_keycaps.py` y ningún `cv_keycap` las nombraba
  (el audito de las 29 etiquetas contra el código no encontró ni una llamada). Quitarlas deja `KC_DATA` en
  **3 939 B** (de 4 792) y el bloque de keycaps en **4 699 B** (de 5 564).
- **La imagen del AC791N bajó lo mismo que el dato: 583 548 → 582 644 B, 968 B libres** (de 64). El XIP de
  código no se movió (**422 928 B**): los 904 B salieron enteros de las constantes (140 192 → 139 288), que es
  exactamente lo que se esperaba de un dato que ya no se guarda. `.data + .bss` sigue en **95 124 B de 98 304**,
  el pool con **9 248 B** de sobra, la RAM code en **19 632 B de 24 576**.
- **El blob de formas no costaba imagen, y eso quedó medido, no supuesto.** `build/gen/ui_shapes.h` son
  **10 861 B de puntos**; bajarlo a 9 661 B con knobs de simplificación nuevos (`--eps=0.6 --flat=0.8`) dejó la
  imagen **idéntica**, porque `nm -S build/felucca.elf` no tiene **ningún** símbolo `SH_PLATES_*` ni `SH_*_PTS`:
  `ui_organic.c` se incluye desde `ui_graph.c` pero su arte no se referencia, y el enlazador no lo lleva. Esa
  premisa de partida era falsa para esta configuración y la vía se cerró con la prueba, no con una opinión.

### El parámetro que debía gastar esos bytes: implementado, medido, y no entra
- **KEYTRK** (profundidad de keytracking del filtro) era el candidato con más diferencia sonora: el seguimiento
  está escrito a mano como `(pitch16 - 60 * 16) * 4` en seis sitios (`eng_analog.c` ×4, `eng_wavetable.c`,
  `eng_additive.c`) y ninguna página podía tocarlo. Se implementó entero —enumeración, descriptor `F_PCT`,
  la fila libre de **VOICE 3**, los seis sitios de motor y default 127 = el 4 de siempre, bit a bit— y se midió
  **tres veces**:
  - insertado antes de `P_E0` (donde el mapa por conteo lo exige): **585 100 B**, +2 456;
  - añadido después de `P_E7`: **585 084 B**, +2 440;
  - con un segundo parámetro reserva: **585 088 B**, +2 444.
  El reparto dice por qué: constantes **+36 B**, RAM **+48 B** y **código +2 404 B** repartidos en **147
  funciones** (`ui_input_frame` 13 842 → 14 444, `draw_graph` +150, `graph_signature` +146, `note_edit` +140…).
  Ninguna de esas funciones lee el parámetro: crecer `track_t.p[]` 2 B mueve **todos** los campos de una
  estructura de **1 760 B** y el compilador rehace las codificaciones de acceso en toda la UI. No es un coste del
  parámetro, es el precio de tocar esa estructura.
- **Conclusión honesta**: 904 B recuperados, 2 456 B de coste. El parámetro **no entra sin recortar calidad o
  contenido**, y por eso no se publica a medias: queda documentado con la ruta que sí cabe —
  `track_t` tiene **un byte de relleno exacto** (offset 327, entre `latched` y `arp_pos`; `sizeof` 1 760,
  alineación 4), y un control sin id de `p[]` puede exponerse por el mismo embudo que ya usan los slots 5..8 de
  la matriz y los macros (`params.c page_desc`). Lo que falta decidir es el **guardado**: los ids de 7 bits están
  en 125 de 127 y los formatos de proyecto y preset no tienen un byte libre por pista, así que un parámetro de
  primera clase necesita una decisión de formato, no 900 B más de flash.

### La atestación, de vuelta: el tag que crea el runner ahora sí dispara el build atestiguado
- **El eslabón que faltaba estaba en GitHub, no en el árbol.** `tag-and-publish.yml` crea el tag con
  `GITHUB_TOKEN`, y GitHub **no arranca workflows** desde un `push` de tags hecho con ese token: `release.yml`
  —el que compila con `zvenson/fm1-build` y consigue el «Built from source on GitHub»— nunca corrió para los
  tags que el propio repositorio creaba. Por eso las releases 1.3.x salieron sin atestación aunque el paquete
  verificado estuviera ahí.
- **El arreglo**: el job pide `actions: write` y, después de publicar, **dispara `release.yml` por
  `workflow_dispatch`** nombrando el tag en un input. Va en un input y no en el `ref` del dispatch porque GitHub
  lee el fichero del workflow **en el ref donde se dispara**: un tag cuyo árbol es anterior al trigger
  (v1.3.6 y todo lo previo) se rechazaría en su propio tag. Disparado en la rama por defecto, el gate y el
  verify hacen `checkout` del tag nombrado y el build mueve el árbol al tag antes de compilar, así que se
  atestigua el árbol del tag, no el de `main`.
- **Una versión, una run**: `release.yml` estrena `run-name: release <tag>` y `concurrency` por versión, y el
  disparo consulta las runs existentes: si ya hay una en curso la deja, si ya terminó en verde no la repite.
  `release/attest-backfill.txt` nombra las versiones ya publicadas sin atestación (hoy `v1.3.6`) para que el
  siguiente push las repare solo, sin que nadie tenga que acordarse de un comando.

### Lo que sigue sin poder afirmarse
- Nada de esto se probó en una unidad física: el FM-1 del dueño no se escribe desde este repositorio. Las
  mediciones de arriba son **build del AC791N** y **host x86**; la suite completa, los renders dorados y el
  emulador se corren en esta versión como en todas.

## v1.3.6 — fase final: el árbol sin avisos y el ecosistema en una sola versión

### Optimización y limpieza, medidos
- **Seis avisos del compilador del AC791N a cero, sin tocar una línea de comportamiento.** El build del target los
  venía arrastrando y cada uno era una costura: `ui_seqtools.c` escribía los pasos del secuenciador a través de un
  accesor `const` (ahora usa `TSEL->step` directo, que es el dueño de esos bytes); `eng_phase.c` pasaba el estado del
  ruido como `uint32_t *` a `noise32(int32_t *)` (ahora el estado es `int32_t`, los mismos bits); `ui_scl.c` declaraba
  `nt[12]` en el manejador de la progresión y nunca lo leía (**12 B de pila de vuelta**); y `ui_being.c` (dos veces) y
  `main.c` tenían locales muertos. `./build.sh` queda en **0 warnings y 0 errors**.
- **Presupuesto del target, remedido tras la limpieza y sin regresión**: imagen **583 548 B de 583 612** (64 B libres,
  los mismos que la 1.3.5), `.data + .bss` **95 124 B de 98 304** (3 180 B libres), pool **311 256 B con 9 248 B de
  sobra** (el mínimo exigido son 8 KiB), RAM code **19 632 B de 24 576**, XIP 563 120 B. Ningún motor, FX ni preset
  nuevo costó nada en esta versión: no se añadieron, y los 285 presets siguen entrando en los mismos bytes.
- **CPU y DSP sin regresión**: los 303 renders dorados quedaron **0 cambiados** y el presupuesto de CPU del regresor
  en **0 entradas por encima** (20 notas medidas), así que la limpieza no movió el sonido ni el coste por bloque.

### El ecosistema, en la misma versión
- **Una sola v1.3.6 en todas partes**: `FELUCCA_VERSION`, paquete `.fwsc` y su manifiesto, `build-manifest`, emulador
  WASM (`site/emulator` y `release/emu`), Web Studio (su `data-version` y la pantalla que dibuja la versión), el
  instalador USB (su `meta` con el build, el emulador y el paquete), `site/firmware/versions.json`, ambos
  `SHA256SUMS`, y las capturas y el banner regenerados desde el firmware de esta versión.
- **El README dejó de mentir sobre sí mismo**: decía `v1.3.2`, «9 motores» (son diez: ANALOG, PHASE, FM6, LOFI, VOICE,
  WHEEL, NOISE, DRUM, WTABLE, ADDTV), «281 presets» (son 285: 283 navegables más dos kits legacy que los proyectos
  viejos siguen nombrando) y «293 renders dorados» (son 303). Y `fmhub.json` anuncia ahora esos mismos 285.
- **El `SHA256SUMS` de la raíz volvió a estar completo**: le faltaba la entrada de la 1.3.5 (se había actualizado el
  del directorio `release/` y no el de la raíz). Ahora lista 1.3.5 y 1.3.6 con su paquete, su manifiesto y su
  `build-manifest`.

### Lo que sigue sin poder afirmarse
- Nada de esto se probó en una unidad física: el FM-1 del dueño no se escribe desde este repositorio. Todo lo de
  arriba es **build del AC791N**, **host x86** (suite completa) y **emulador WASM**; la clase de cada medición está
  dicha donde aparece.

## v1.3.5 — el visualizador deja ver lo que hay detrás: tinta translúcida de verdad

### Lo que está hecho y medido
- **Las ondas del visualizador son translúcidas de verdad, no de color parecido.** El framebuffer se puede leer, así
  que cada punto de un trazado ahora es **el color que ya había allí mezclado hacia la tinta** (`gfx.c`: `cv_ink`
  global leído por `cv_pset`, y `cv_line_a(x0,y0,x1,y1,c,pct,thick)` que pone la tinta, recorre con el Bresenham de
  siempre y la devuelve a 100). Antes el trazo tapaba la banda de espectro, la rejilla y el campo de estrellas que
  cruza; ahora los atraviesa: se ven las dos capas a la vez.
- **Cuatro trazados, tres niveles, ningún control nuevo.** El reparto está en la escalera `CV_A_SIGNAL 84`,
  `CV_A_CURVE 70`, `CV_A_WAVE 74`, `CV_A_MOD 56` (`gfx.c`): el osciloscopio de la bahía de HOME y su página WAVES
  (**84 %**, lo más sólido porque es el dato que se lee), la respuesta del filtro (**70 %**), la onda del kit de DRUM
  (**74 %**) y la envolvente de modulación (**56 %**, lo más tenue porque es la referencia de fondo). Todos son puntos
  ya existentes de `ui_being.c` (`gw_live_trace`, la curva del bay, `gw_drum_wave`, la envolvente): la opacidad va
  atada al papel de cada trazo en vez de a un ajuste que haya que tocar.
- **Comprobado con una prueba que falla si se ignora la mezcla.** `tests/ui_test.c` suma «VISUALIZER: the scopes and
  waves are see-through ink over what they cross, in three ordered shares»: sobre un lienzo conocido pinta una banda,
  dibuja la misma tinta encima a los tres repartos y exige que el píxel sea **exactamente** `ux_mix(fondo, tinta,
  pct)` (o sea, que el fondo esté dentro del resultado), que la misma tinta sobre dos fondos distintos dé dos colores
  distintos, que `cv_line_t` a 100 % siga siendo opaco, y que la escalera esté ordenada con la más tenue conservando al
  menos la mitad de su tinta. La distancia se mide canal a canal y no por luminancia: dos colores de la paleta pueden
  diferir en tono sin que uno sea más brillante, y el trazo tiene que caer entre ellos igual.
- **Costo: 96 B de imagen y 0 B de RAM.** La imagen del AC791N queda en **583 548 B de 583 612** (**64 B** libres),
  `.data + .bss` en **95 124 B de 98 304**, pool con **9 248 B** de sobra y RAM code en **19 632 B de 24 576**. La
  primera versión duplicaba el Bresenham y el enlazador la rechazó (`region XIP overflowed by 96 bytes`): la que
  queda reutiliza `cv_line_t` y no añade ni un bucle.
- **Capturas regeneradas porque el dibujo cambió de verdad** (`tools/fw_shots.py --render`, y las nuevas copias en
  `site/assets`): `fw-home.png` y `fw-presets.png`. Los renders dorados de audio siguen en **303, 0 cambiados**,
  porque la tinta es de la interfaz y no toca la voz.

### Lo que NO entra, y por qué (medido, no estimado)
- **No hay parámetros de motor nuevos, ni filas nuevas en EDIT/LFO/ARP/SEQ/SCL, ni buses de FX nuevos.** El visualizador
  tenía **160 B** libres al empezar la fase y esta mejora se queda con **64 B**: en la 1.3.2 una sola página nueva
  costaba **92 B** de descriptores (`page_desc +92`, ver `docs/BUILD_AC791N.md` §8.4) y un parámetro de motor es más
  caro todavía (entrada en la tabla, nombre, rango y su caso en el dibujo). Con 64 B no hay forma honesta de meterlo:
  queda documentado como lo que es, un pendiente que necesita flash, no un cambio que se pueda simular.
- **El repaso de las trece pantallas no encontró huecos de transparencia que arreglar**: el visualizador a pantalla
  completa ya tenía sus dos pozos de cristal (`graph_home_viz`, mezcla `ux_mix(bg_darken(T_BG, 14), T_BG, 62)`) y sus
  medidores de 16 bits, y MIXER, OVERVIEW, PERFORMANCE, PRESETS, GLO, SCL, ARP, SEQ, FX y MENU ya usan las mismas
  piezas compartidas (`gw_seg_v`, `ux_mix`, `heat_col`). Lo que faltaba era justamente la bahía de HOME, que es lo que
  esta versión arregla.

## v1.3.4 — fase 3: el banco medido por cómo suena, y los cuatro knobs contestan en HOME

### Lo que está arreglado y medido
- **Los knobs 2 y 3 no animaban la gráfica principal.** En HOME el dibujo del motor pulsa cuando algo cambia
  (`ui.home_pulse`, `ui_being.c`), pero solo lo ponían PRESETS, un toque en HOME y el menú: girar el knob 1 o el 4
  se veía vivo (sus filas de valor se redibujan) y girar el 2 o el 3 movía un valor que **ninguna otra cosa
  mostraba**. Ahora lo ponen los cuatro, así que el anillo del ser responde a la columna que se haya girado, en
  **todos los motores**: ANALOG, FM6, PHASE, LOFI, VOICE, WHEEL, NOISE, DRUM, WTABLE y ADDTV.
- **Comprobado con la regresión que falla sin el arreglo.** `tests/ui_test.c` suma
  «HOME: each of the four knobs pulses the picture, in four different engines» (4 knobs × 4 motores): **falla con la
  línea quitada y pasa con ella**, y `ui_test` entero queda en verde (`all passed`).
- **La auditoría ahora mide el SONIDO, no solo los parámetros.** `preset_audit_test --fingerprint` pasa cada preset
  por el camino real de carga y render (una nota sostenida; en DRUM tres golpes, bombo, caja y charles, porque un kit
  son ocho canales) y saca diez bandas logarítmicas por Goertzel más la forma de la envolvente (bloque al 90 % del
  pico y bloque de vuelta al 50 %). `tools/preset_audit.py --sound` ordena los pares más cercanos por eso, y
  `--gate-sound` es la puerta de la suite: **5 037 pares medidos, 0 por debajo de 0,05**.
- **La distancia por parámetros ya no confunde una elección con un número.** FM6 escribe su sonido en `ALG` y
  `SLOT` (0 y 14 no son «un poco más brillante»: son otro parche) y el audit los normalizaba como si fueran
  continuos: por eso marcaba **43 pares casi iguales** que en realidad eran presets distintos. Con `ALG/SLOT/WAVE/
  WAV#/ALGO/TYPE/KIT/MODE/SCALE/ORDER/DIV/PATCH` contados como elección quedan **2 pares por debajo de 0,035**
  (MUSIC BOX / FROST MUSIC 0,017 y TINE EP / EMBER TINES 0,025), y los dos se distinguen por FB, envolvente de
  filtro y envíos: el motor elige el timbre en el parche y lo demás son envíos, que es exactamente lo que el audit
  de sonido mide aparte (0,148 y 0,108 de distancia real).
- **Menos presets que se pisaban, con el número antes y después** (distancia de sonido, 0 = el mismo sonido; el peor
  par por motor, antes → después): LOFI **0,043 → 0,080**, ANALOG **0,059 → 0,071**, DRUM **0,071 → 0,102**, FM6
  **0,077 → 0,095**, y **cero pares por debajo de 0,06 en todo el banco** (antes había cuatro). Todo con edición de
  valores in-place: **ningún ID se movió, ningún array se reordenó**.
- **DRUM: los kits que se parecían ahora son tres kits distintos.** `LINNDRUM` pasa a seco, brillante y corto (TONE
  104, DECAY 44, DRIVE 14, sin reverb) y recupera los 15 dB que le faltaban al banco (`P_LEVEL` 124, 972 → 2 653 de
  RMS); `ELECTRO` pasa a profundo, ruidoso y largo (TUNE 60, DECAY 92, NOISE 80, más delay); `QUANTUM BEAT` pasa a
  oscuro y metálico (MORPH 104, TONE 30, DECAY 100, FM ENV 44, FOLD 48). `DRUM-X` queda como el kit de referencia.
- **LOFI, ANALOG y FM6: lo mismo, en los pares que quedaban.** `STEP LEAD` (WAVE 3, DUTY 96, VIB 40, BEND 20) y
  `CIUDAD DE PLAYA` (CHIP 2, CRUSH 60, CUT 70, MASK 45) dejan de parecerse; `CHIP ARP` (CHIP 1, DUTY 8, BEND 30) y
  `POCKET CONSOLE` (WAVE 2, CRUSH 34, CUT 104) también; `SIESTA DORADA` pasa a caña con intervalo (WAVE 6, MIX 96,
  RES 40, DRIVE 28, INT −12) y `HARP PLUCK` a cuerda pulsada de verdad (CUT 74, RES 58, fenv 62). **GAMELAN MET
  tenía el mismo parche que VIBES y solo cambiaban los envíos**: ahora usa el parche de barra percutida (slot 5, el
  de MARIMBA) con su propio shimmer. Y **las tres campanas del mismo parche** (TEMPLE BELL, GONG SHIMMER y TEMPLE
  CIRCUIT) dejan de ser tres envíos distintos del mismo sonido: GONG SHIMMER toma FB 4, MOD 16, MEG −10 y DETUNE 26
  (un gong que ruge en vez de un campanilleo) y TEMPLE CIRCUIT MOD −12, RATIO −5 y MEG 12; **de 0,009–0,022 de
  distancia pasan a 0,31–0,34**. El par más cercano de FM6 sube de 0,077 a 0,095.
- **ADDTV, el motor más pobre (8), suma dos sonidos que no tenía**: `REED HORN` (parciales impares y codo suave, mono,
  como lead de caña) y `AIR CHOIR` (parciales estirados, LFO lento abriendo el filtro). Su par más cercano queda en
  **0,132**, el más separado de todo el banco.

### Inventario (medido, no copiado de la documentación anterior)
- **285 presets en 10 motores, 283 visibles y 2 legacy** (HEARTH KIT y CYBER DRUM, ocultos por `legacy` sin romper
  la carga de proyectos viejos). ANALOG 68, FM6 37, PHASE 32, LOFI 29, VOICE 25, WHEEL 22, NOISE 22, WTABLE 22,
  DRUM 18, ADDTV 10. Categorías: PADS 65, KEYS 57, TEXTURES 49, DRUMS 24, BELLS 21, BASS 20, ORGANS 18, LEADS 12,
  PLUCKS 12, ARPS 7. `docs/QG_PHASE4_PRESET_INVENTORY.json` ahora **se genera** con
  `python3 tools/preset_audit.py --inventory docs/QG_PHASE4_PRESET_INVENTORY.json` desde las tablas del firmware y
  el render, así que no puede volver a quedar viejo.
- Niveles por motor (RMS mediano, una nota sostenida por preset): ANALOG -17.9 dB, FM6 -19.9, ADDTV -17.3, WTABLE -16.5, DRUM -14.4,
  NOISE -29.0 (con GEIGER CNT, que es un sonido disperso a propósito). Ningún preset silenciado, ninguno por encima del clipper: 0,00 % en los 285.

### Los renders dorados que cambian, y por qué
- **12 de los 301 renders dorados cambian y se regeneran a propósito** (`GOLDEN_UPDATE=1`), porque son exactamente
  los presets retocados: ANALOG HARP PLUCK y SIESTA DORADA; LOFI STEP LEAD, CHIP ARP, POCKET CONSOLE y CIUDAD DE
  PLAYA; DRUM LINNDRUM, ELECTRO y QUANTUM BEAT; FM6 GONG SHIMMER, GAMELAN MET y TEMPLE CIRCUIT. Los otros 289 quedan
  **idénticos** y los dos presets nuevos de ADDTV entran como renders nuevos: **303 en total, 0 cambiados, 0
  perdidos** en la pasada de verificación. Ningún cambio de UI: el barrido de pantallas y su lint salen igual.

### Memoria y mediciones (build AC791N de este árbol)
- Imagen 583 352 → **583 452 B** de 583 612 (**160 B** de margen: los dos presets nuevos y los retoques cuestan
  **100 B**). `.data + .bss` **95 124 B** de 98 304 (3 180 B libres), pool **311 256 B con 9 248 B de sobra**, RAM
  code **19 632 B de 24 576**. El paquete sigue en 610 959 B con identidad **FM-1_965**.

## v1.3.3 — la escena de FX ya no transpone

### Lo que está arreglado y medido
- **OCT+ guardaba la escena de FX y además subía la octava; OCT- la llamaba de vuelta y además la bajaba.** En la
  página FX esas dos teclas son la escena de la pista (1.3.2), pero el `case B_OCTDN/B_OCTUP` que resuelve la octava
  cuando el botón se suelta seguía actuando: guardar la tira de envíos cambiaba el tono de lo que se iba a tocar, y
  llamarla de vuelta lo cambiaba otra vez. La página FX ahora sale antes de la octava, igual que ya salía con
  `LAYER_FX` (el LATCH del mapa) y con las páginas de acción; **fuera de esa página las dos teclas siguen moviendo la
  octava, arriba y abajo**, que es lo que comprueba el segundo caso.
- **La ranura de escena que la página «usaba» no existía.** `fx_scn_pick` estaba declarada y nadie la escribía ni la
  leía; se va, y el comentario dice lo que hay: cuatro ranuras por pista, la página usa la primera y el selector para
  las otras tres todavía no existe.
- **Comprobado en host, con la regresión que falla sin el arreglo.** `tests/ui_test.c` suma dos casos: «FX page: OCT+
  stores and OCT- calls back, and neither moves the octave» — **falla sin el cambio y pasa con él** — y «off the FX page
  they still shift the octave, up and down». `ui_test` completo en verde (`all passed`) y la suite entera también.
- **`morph_six_test` se revisó entero y está completo**: seis pistas, los buses de FX y el clipper, notas por MIDI,
  una carga de preset encima y el giro de MORPH, con los dos renders idénticos antes del giro. 6 transiciones, 0
  fallos: latencia 0,09–0,27 ms, slew al 90 % en 5,4–7,1 ms y ningún click (paso 4 385 contra 6 714 antes del giro).
  No se tocó.

### Memoria y mediciones (build AC791N de este árbol)
- Imagen 583 320 → **583 352 B** de 583 612 (**260 B** de margen: el arreglo cuesta **32 B**). `.data + .bss`
  95 124 B de 98 304 (3 180 B libres), pool **318 432 B con 9 248 B de sobra**, RAM code **19 632 B de 24 576**,
  LOWBUF 26 724 de 28 672 y NOINIT 15 572 de 15 696. El paquete sigue en 610 959 B con identidad **FM-1_965**.
- **Sin cambios visuales**: los 301 renders dorados salen idénticos y el barrido de las 190 pantallas × 22 paletas no
  encuentra nada.

### Lo que se revisó y no se tocó, con el motivo
- **MIDI Learn es RAM por diseño.** `midi_control.c` lo dice y lo razona: la tabla que un apagado limpia, para no
  cambiar el layout de ajustes, de los presets ni del proyecto. Persistirla pide formato nuevo, no un bug fix.
- **La escena de FX no entra en el proyecto**: el payload está lleno byte a byte (medido en la 1.3.2), y meterla pide
  un formato `FUND`/`FUNC` entero.
- **SLICER con DEPTH y modo por paso, y el A/B de la edición en las páginas de grupos**: el SLICER está implementado y
  verde en host pero el enlace no cierra (**496 B** sin su gráfico contra 260 B libres); el A/B no está empezado.

## v1.3.2 — HOME sin las tarjetas de atrás, los nombres enteros y la escena de FX de cada pista

### Lo que está arreglado y medido
- **Las cuatro tarjetas de la fila de arriba ya no asoman por ninguna pantalla de HOME menos en HOME misma.**
  La supresión está donde corresponde, en el flujo compartido de dibujo: `draw_columns()` sale antes de las
  tarjetas cuando `ui.home && ui.home_view >= HV_OVERVIEW` (fuente única de esa fila), en vez de taparlas con
  un panel encima. `graph_home_view()` abre ahora en `Y_LABEL`, porque el nombre de la pantalla ocupa la banda
  16..20 que antes se dibujaba a ciegas.
- **OVERVIEW, MIXER y PERFORMANCE se leen enteros.** El cuerpo de la vista se dibuja en dos pases de 88
  filas (26 y 114) sobre un pase-cap de 26 filas a `Y_LABEL`: ningún pase corta una fila de texto, que es la
  condición que imponía el lint. Los nombres van en `cv_text(14, 34, &AF_S, …)` con la regla en `y = 41`, y el
  fondo (`hv_grad`, `hv_air`) se recortó al nuevo cuerpo en vez de seguir midiendo desde 0.
- **Números, no impresiones.** `build/ui_render` sobre 190 pantallas × 22 paletas: **0 hallazgos de lint**
  (ningún texto tapado, cortado ni fuera de su canvas), **0 desalineaciones en 3 894 154 medidas** de 74
  posiciones, y `tests/ui_test.c` completo en verde (`all passed`). Los volcados 2× de las tres pantallas
  están en `build/shots/home_overview.png`, `home_mix.png` y `home_perf.png`.

### Memoria y mediciones (build AC791N de este árbol)
- **La entrega completa mide 583 320 B de 583 612 B: 292 B de margen.** La escena de FX por pista (120 B de
  `.bss`, cuatro ranuras por pista) cuesta **456 B** de imagen sobre la 1.3.2 anterior: 582 864 → **583 320 B**, y el
  margen baja de 748 a **292 B**. `.data + .bss` queda en **95 124 B de 98 304 (3 180 B libres)**, pool **311 256 B
  con 9 248 B de sobra**, RAM code **19 632 B de 24 576**, y el paquete sigue en **610 959 B** con identidad
  **FM-1_965** (el contenedor llena un tamaño fijo por slot). Con 292 B libres, el siguiente feature de este tamaño ya
  no entra: por eso el SLICER por paso queda fuera y su coste está medido abajo.
- **La imagen baja 96 B y el margen del slot sube de 684 a 748 B**: el arreglo de HOME (una salida temprana en
  `draw_columns()` en vez de cuatro tarjetas tapadas, y el nombre de pantalla que abre en `Y_LABEL`) dibuja menos
  código y deja la imagen en **582 864 B de 583 612**. `.data+.bss` queda en **95 012 B de 98 304 (3 292 B
  libres)**, pool **311 256 B con 9 248 B de sobra** (el piso de 8 KiB se respeta), RAM code **19 676 B de
  24 576**, LOWBUF **26 724 de 28 672** y NOINIT **15 572 de 15 696**.
- **`tools/ram_report.py` medía mal dos regiones y ahora no puede.** El XIP se calculaba sumando sólo `.text`,
  así que informaba **21 252 B libres** cuando la región la llena la imagen entera (`.text` más las copias de
  carga de `.ram_text`, `.c1_text`, `.hot_text` y `.data`, `AT > XIP` en `app.ld`): los 748 B reales. La RAM se
  calculaba sumando `.data` y `.bss` sin el hueco de alineación entre las dos (36 B), o sea 3 328 B libres donde
  hay 3 292. Las dos filas salen ahora de los mismos mojones que comprueba `tools/build.py` (`_bss_end`,
  `_pool_end`, `_c1_stack_hi`, `_low_end`, `_hot_end` y el tamaño de `build/felucca.bin`), y coinciden al byte
  con el build en las seis regiones.
- **Lo que se midió y se descartó** (detalle en `docs/BUILD_AC791N.md` §8.2): memorizar la máscara de
  portadoras de FM6 costaba **+48 B de XIP**; una caché de «última fuente» delante de `hc_lut()` costaba **+16 B
  de XIP** para ahorrar hasta dos comparaciones por glifo; y `-fdata-sections` da **582 828 B frente a 582 832 B** (medido sobre el árbol previo al arreglo de SELECT, de 582 832 B)
  (−4 B, dentro del relleno de sección). Ninguno de los tres compra algo que el presupuesto pueda ver en el
  recurso que aprieta, que es la imagen, así que no quedaron en el árbol.

### Lo que está arreglado después: el salto de SELECT a ARP en HOME

- **El camino, medido y no supuesto.** Con trazas en `page_jump()`, `page_tap()` y `open_family()`, girar SELECT en
  HOME con el botón ARP apretado terminaba en `page 65 (ARP)` por dos sitios encadenados: `page_jump(fam=8)` con
  `ui.home` puesto —que el propio `page_jump()` lee como «desde otro lado» y resuelve a la primera página de la
  familia, y la primera de ARP es ARP— y después el **tap del botón** al soltarlo, porque el bit armado en el
  escaneo sobrevivía al giro y el bucle de taps leía «bit armado + botón arriba» como una pulsación completa.
- **El arreglo, en los dos sitios.** En HOME el giro de SELECT ya no pagina: consume el botón (`btn_used`) y deja la
  familia a su suerte de siempre —los dados de EDIT, SEQ, LFO y FX, y DICE ALL con HOME apretado—; el paginado desde
  HOME sigue siendo el mismo botón con PRESETS. Y un tap pasa a exigir un *release* de verdad (el botón estaba abajo
  en el pasada anterior y ahora está arriba), no un bit armado por una arista repetida del escaneo.
- **Comprobado.** `tests/ui_test.c` con una regresión nueva —«SELECT on HOME turns its knob: no page, and ARP's button
  under it takes none»— que **falla sin el cambio y pasa con él**; la suite entera en verde (`all passed`, incluida la
  de 0.6.67 con el paso de ENV por PRESETS, que es donde viven las páginas desde HOME) y el barrido del render con
  **0 hallazgos de lint** y **0 desalineaciones en 3 894 154 medidas**.

### La escena de FX de cada pista


- **La escena de FX, una por pista: los cuatro envíos de la página FX (DIST, CHOR, DLY, REV) y el tipo de DIST.** En la
  página FX, **OCT+ guarda** la tira y **OCT- la llama de vuelta** (con UNDO, que devuelve la música de antes); una escena
  que la pista no guardó dice `FX SCENE EMPTY` y no escribe nada, y OCT+ otra vez la escribe encima. Son las dos teclas
  que esa página tiene libres: sus perillas son los envíos, PRESETS el sonido, SELECT las páginas, y los botones sus
  secciones y sus capas. Ocupa 120 B de `.bss` (cuatro ranuras por pista; la página usa la primera).
- **Un bug viejo que salió en el camino**: `N_DTYPE` es la tabla de nombres, no una cantidad — `N_DTYPE - 1` era
  aritmética de punteros y guardaba basura en el tipo de DIST. El rango sale ahora de `TP[P_DTYPE]`, como el de cualquier
  otro parámetro.
- **Comprobado en host**: `tests/ui_test.c` suma `test_fx_scenes` — guarda, llama, escribe encima, una escena por pista y
  la llamada vacía que no toca nada — en verde en tres corridas seguidas (`all passed`), con el barrido del render en
  **0 hallazgos de lint** y **0 desalineaciones en 3 894 154 medidas**.

- **No está guardada en el proyecto.** El payload del proyecto está lleno, medido: las pistas terminan en 2880, la cadena
  ocupa 36 y el motion 260 (3176) y el bloque del punch-in empieza en `PROJ_PFX_OFF` 3168; después vienen el kit (3216),
  los FM6 (3312), el nombre (3824) y el hash (3836) en un store de 3840 bytes con el hash pegado al final. No hay un solo
  byte libre: 120 B por proyecto piden formato nuevo (magia `FUND`, payload más grande, lectura de `FUNC` al lado), y eso
  es una migración que hay que hacer entera y con su paquete delante, no de paso.

### Lo que sigue sin hacer
- **DEPTH y modo por paso en el SLICER: implementado y verde en host, fuera de la imagen.** El modelo, el DSP, la
  página FX > SL STEP y el bloque `'S' 'P'` del proyecto están escritos y probados (`tests/ui_test.c`,
  `tests/project_test.c`), pero el enlace del AC791N **no cierra**: 816 B por encima del slot con el gráfico de la
  página y 496 B sin él, contra **292 B** libres. Medido por símbolos: `graph_slstep` 310 B, `proj_unpack_c` +120,
  `page_desc` +92, `proj_pack` +84, `graph_signature` +84, `project_capture` +64, `project_restore_runtime` +48,
  `slicer_on` +34, `sl_enter` +32, `slicer_bus` +28, `system_init` +20 y ~88 B de cadenas. Detrás del slot sólo
  quedan **725 B** antes de la partición siguiente, así que el feature espera a que se pague esa flash. Vive en la
  rama `sl-step`.
- **A/B de la edición en las páginas de grupos: no empezada.**

## v1.3.1.1 — el empaquetado de `step_t` no entra: medido y descartado

### Lo que se probó
- **`step_t` de 17 a 15 B** (n/flags 3+5, time/comp 2+5, vel/probability/ctime 7 bits, `cwhen` entero porque su
  nibble alto es el micro-timing del paso). La estructura compila, pasa la suite entera de host
  (`ALL HOST TESTS PASSED`, exit 0) y devuelve **1 024 B de `.bss`** (`trk` 10 560 → 9 792 y `gen_keep`
  2 176 → 1 920).
- **Y no entra en la imagen.** El enlace del AC791N lo rechaza:
  `section .hot_text will not fit in region XIP`, `region XIP overflowed by 3776 bytes`. La misma árbol sin el
  cambio vuelve a enlazar en su tamaño de siempre (imagen 582 928 B, 684 B libres), así que el empaquetado
  cuesta ~4,4 KB de código: cada escritura a un bit-field es leer el byte, enmascarar, o, escribir, y hay
  cientos de sitios de escritura repartidos por `seq.c`, `ui_*.c` y `project.c`.
- **Cambiar 1 KB de RAM por 4,4 KB de flash en un build con 684 B libres es un mal negocio**, así que el
  cambio queda revertido y anotado en `docs/BUILD_AC791N.md` §8.2. La RAM no es el recurso escaso: quedan
  3 292 B de `.bss` y 9 248 B de pool. El recurso escaso es la imagen.

### Lo que sigue: liberar flash, no RAM
- `fm6_note` (7 104 B), `hc_tab` (1 548 B) y los índices de 12 bits son RAM, y la RAM tiene margen. Lo que
  aprieta es el slot: **908 B libres** con la imagen de 1.3.1, y los iconos (7 747 B) y las fuentes
  (27 216 B) ya viven en XIP como `const`. El siguiente paso rentable es deduplicar iconos repetidos, no
  mover más estado fuera de `.bss`.
## v1.3.1 — la página FX, legible; 1,5 KB de RAM devueltos

### La página FX
- **CHORUS y REVERB ya no tocan el medidor.** Son seis glifos de AF_S y su línea llegaba a cx + 39, justo
  sobre la columna del medidor (cx + 35): la barra cortaba las dos últimas letras. El nombre sube una fila
  (tinta 8..16) y el medidor abre en la fila 22, con seis filas de aire; sigue siendo de 16 × 43.
- **Los tres indicadores de abajo se leen en tiempo real.** EQ es bipolar (el centro es 0 dB, el relleno va
  del centro a la posición de la ganancia y la cabeza marca el punto; BYP se ve plano); DEPTH tiene escala
  de 25/50/75 % sobre el relleno y una cabeza que marca la compuerta; SLICER enciende los 16 pasos del
  patrón que va a compuerta (`sl_pattern()`) y el paso que suena se para un píxel más alto, en acento. Las
  tres barras siguen la ley de los medidores (`fx_ease`: ataque inmediato, un cuarto por frame), así que un
  encoder girado rápido no rompe el relleno y un valor que se detiene se asienta en dos frames.
- **Comprobado con el barrido de renders (host x86)**: 190 pantallas × 22 paletas, 0 hallazgos de lint, 0
  tintas sobre su propio color, 0 píxeles fuera de gris en GREY/MONO, 0 desalineaciones de 3 894 154 medidas.

### Memoria
- **1 536 B de `.bss` devueltos**: el anillo de salida de la consola USB era de 2 048 B para una cola de
  texto cuyo estallido es una línea de diagnóstico; 512 B aguantan seis. El anillo de entrada queda en 256,
  que es lo que el host manda en una ráfaga.
- **La unión de búferes de trabajo que propuse para esta ronda no es segura y no la hice**: `gen_keep` es el
  UNDO de GEN y sobrevive entre frames, `upf` es el índice del banco de FM6 que se guarda en flash y
  `st_buf` se devuelve a quien lo pide (editor_backup) como cuerpo del objeto: sus vidas se solapan. Queda
  anotado en `docs/BUILD_AC791N.md` §8.2 junto con el plan para `WT_GEN` (12 288 B = 3 niveles × 8 frames ×
  256 puntos × 2 B), que sí es RAM de verdad pero cambia el timbre y necesita una prueba de escucha.

## v1.3.0 — la página FX con el panel entero

La primera página de FX usaba 98 de las 122 filas del panel: los cuatro cuadros medían 62 px y los tres
módulos de abajo se apiñaban en una franja de 28, mientras las filas 99..121 quedaban vacías. Ahora el
panel llega hasta el final y todo lo que hay dentro es más grande.

### Lo que cambió
- **Los cuatro cuadros de envío: 54 × 75** (eran 53 × 62). El nombre va arriba, el valor debajo, el
  icono bajo el valor y la etiqueta del tipo al pie, así que el medidor se queda la columna derecha
  entera en vez de compartirla con el texto.
- **Los medidores están dentro de su cuadro y son más grandes**: 16 × 49 px de pista con la barra de
  plasma de 12 px (eran 10 × 42 dentro de una pista de 12). Los cuatro arrancaban en `cx + 43` sobre un
  cuadro de 53 px, o sea que colgaban 2 px por fuera del borde: eso era lo que se veía "corrido". Ahora
  `mx = cx + 35` y `35 + 16 + 3 = 54`, el ancho del cuadro, en los cuatro y con el mismo margen.
- **Los tres módulos de abajo (EQ / DEPTH / SLICER): 76 × 39** (eran 72 × 28) y bajaron a las filas
  82..120. La barra pasó de 26 × 3 px a la mitad derecha de una línea a **68 × 9 px** bajo el valor, que
  es lo que hace legible de un vistazo la posición del EQ, el recorrido de DEPTH y los 16 pasos del
  SLICER. DEPTH se dibuja como un relleno continuo (es una compuerta 0..100 %) y el SLICER conserva sus
  16 pasos, que ahí sí son el tema.
- **Verificado en el barrido de renders (host x86)**: 190 pantallas × 22 paletas, **0 hallazgos de
  lint**, 0 tintas sobre su propio color, 0 píxeles fuera de gris en GREY/MONO, y **0 desalineaciones
  de 3 894 154 medidas**. Ninguna otra página se movió.

## v1.2.7 — el paquete, el sitio, el instalador y el emulador al día

Lo que la v1.2.6 dejó como build de trabajo ahora es un release completo: un paquete con su manifiesto,
las capturas regeneradas desde ese árbol, el portal, el instalador y el emulador sirviendo la misma
versión, y `tools/coherence_check.py` en verde.

### El paquete
- **`release/felucca-v1.2.7.fwsc`**, 610 959 B, con `felucca-v1.2.7.fwsc.manifest.json` y
  `release/build-manifest-v1.2.7.json` (commit, árbol, hashes de la imagen, del cargador y de los tres
  ficheros del SDK, toolchain y emulador).
- **Medido en el build AC791N**: imagen **582 720 B de 583 612** (892 B libres), `.data+.bss`
  **96 548 B de 98 304** (1 756 B libres), XIP 422 168 B de código + 140 080 B de constantes, RAM code
  19 676 B de 24 576, *pool* 311 256 B con 9 248 B de sobra, cargador `ota.bin` 7 803 B. Identidad
  `FM-1_965`.

### Lo que trae el release
- **El motor ADDITIVE (ADDTV)**, diez motores en total, con sus ocho presets y su página EDIT; medido en
  la imagen: **1 832 B de XIP y 1 440 B de `.bss`** (`tools/engine_size.py`, `docs/BUILD_AC791N.md` §8).
- **SOUND EDITED**: la estrella del pie, la de OVERVIEW y el punto del navegador de PRESETS.
- **Las cuatro tarjetas y las gráficas**: etiquetas en negrita, degradado en todos los cuadros de la
  primera página de FX y el relleno del biquad EQ con el degradado del gráfico principal.
- **281 sonidos de fábrica** (ADDTV suma ocho) y **diez motores** en el editor del Studio, en el emulador
  y en la librería.

### Las otras piezas
- **Capturas**: `assets/screenshots/` y el arte del portal regenerados desde este árbol con
  `tools/generate_screenshots.py`, que lee la versión del propio firmware.
- **Portal**: `site/index.html` anuncia 1.2.7 (diez motores, 281 sonidos) y `site/firmware/versions.json`
  lleva la entrada 1.2.7 con el hash y el tamaño del paquete; el emulador de la portada es el de este
  build.
- **Instalador**: `site/webapp/installer/` ofrece 1.2.7 y verifica el SHA-256 y el tamaño de ese paquete
  antes de habilitar INSTALL.
- **Emulador**: `release/emu/` y `site/emulator/` con el WASM compilado desde este árbol, su commit y su
  SHA-256, y la misma versión que el firmware.

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
- **Coste en la imagen, medido en el build AC791N**: **1 832 B de XIP y 1 440 B de `.bss`** (10 símbolos;
  `ad_v` 1 224 B, `add_render` 810 B, `add_block` 310 B, `AD_PRESETS` 288 B, `ENG_ADDITIVE` 232 B,
  `AD_PROF` 64 B). El build cierra con la imagen en 582 336 B de 583 612 B y `.data+.bss` en 96 548 B de
  98 304 B (1 756 B libres). Tabla por motor y método: `docs/BUILD_AC791N.md` §8.

### SOUND EDITED (el indicador de parámetros modificados)
- **Una estrella junto al nombre del sonido** en el pie de todas las páginas y en HOME > OVERVIEW, y un punto
  en la fila del sonido cargado del navegador de PRESETS: el sonido cambió desde que se cargó o se guardó.
- **Se calcula comparando**, no con un hook en cada setter: un hash FNV-1a del sonido (P_COUNT + la matriz +
  motor y preset) contra la huella que dejó la última carga o el último guardado, así que cubre perillas, la
  capa rápida, el editor, A/B y el recall de una sección sin que ninguno de ellos lo sepa. Se evalúa una vez
  por cuadro y **nunca con el transporte andando** (una automatización o un lock escribe p[] para su paso y lo
  devuelve: no es una edición del sonido). Un proyecto cargado —y una sección de la canción, y un UNDO, que
  pasan por el mismo `project_restore_runtime`— también es la referencia: sus pistas no son ediciones de
  nadie. No se guarda: es estado de la interfaz.

### Limpieza de esta misma entrega
- **El decaimiento por parcial no ocupa memoria**: es `DECAY · k`, así que se calcula en el lazo del bloque y
  `ad_trk_t` baja de 68 a 36 B (**−192 B de `.bss`**, todas las pruebas y todos los renders iguales).
- **El gráfico pide las ganancias** con `ad_partial()` en vez de leer `ad_t` y repetir el índice por parte; el
  motor sigue siendo el dueño de su estado.
- **Casos que no podían darse**: `if (d < 0)` sobre un `uint32_t` (el decaimiento nunca puede envolver) y el
  `% NPART` de un índice que ya está en 0..5 (NTRK == NPART: era una división por seis por bloque).
- **Una sola forma de escribir la estrella**: OVERVIEW la pega al nombre como el pie, en vez de medir el texto
  y dibujarla aparte.
- **La prueba, más corta**: `E_FULL` era idéntico a `E_SAW`, `(void)f` tapaba una constante que nadie usaba y
  trece pares `voice_render` + `spectrum` son ahora `spec_of()`.
- **Un arreglo que salió de la revisión**: cargar un proyecto (o recordar una sección, o un UNDO) dejaba las
  cuatro pistas marcadas como editadas —esas pistas las escribió el firmware, no el usuario—. Ahora
  `project_restore_runtime` es la referencia, con su caso en `tests/ui_test.c` (que falla si se quita).

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

### Las cuatro tarjetas y las gráficas (pulido pedido por el dueño)
- **Las letras de las cuatro tarjetas de knob, en negrita.** La etiqueta de cada tarjeta era la tinta más fina de su
  propio cuadro: 12 px de una cara ligera al lado de un valor en la de 15 px. Ahora se dibuja con la curva de
  cobertura `CURVE_BOLD` (`firmware/src/gfx.c`, `cv_text_fit_bold`): los mismos glifos, el mismo ancho, la misma
  línea de base y el mismo recorte con puntos suspensivos, con los trazos más sólidos. No hay una segunda fuente en
  flash —la imagen cierra con 1 276 B de margen— y la caja de tinta que mide el lint es la misma, así que el
  barrido sigue en 0 hallazgos.
- **Todos los cuadros de la primera página de FX llevan degradado, incluidos los de abajo.** Los cuatro módulos de
  la fila inferior (EQ / DEPTH / SLICER) y el panel de los cuatro sends eran placas planas porque el estilo
  achaflanado sólo pintaba su rampa en cajas de 36 px de alto o más (`cv_rrect`, `gfx.c`): el umbral baja a 24 px
  (`PLATE_MIN_H`), que es lo que mide el módulo típico, y la página pinta además el degradado del panel, de cada
  send y de cada módulo con `ux_ground_rows` — el mismo que usan las vistas de HOME y FX > BUSES — y dibuja sus
  textos sobre el color real de su fila (`ux_ground_at`), no sobre un `T_SURF` que no está debajo.
- **El degradado de la gráfica del biquad EQ es el de la pantalla principal.** El relleno bajo la curva eran seis
  franjas escalonadas mezcladas desde `T_BG`, reiniciadas bajo cada columna: textura, no luz. Ahora es la ley de
  `graph_ground_ramp` / `hv_grad` (panel 22..62 %, acento 10..45 %), resuelta por fila absoluta, y el cuerpo del
  filtro (lift y cut) se rellena en tres pasos que siguen la altura de la fila en vez de un bloque plano al 40 %.
- Barrido de `tests/ui_render`: **0 hallazgos de lint, 0 desalineaciones de 3 894 154 medidas**, 0 píxeles
  GREY / MONO fuera de gris o neutro — medido en host, sobre el renderizador real, no en la unidad.

### Cómo se mide (y cómo se instala el toolchain)
- **`tools/get_ac791n.py`**: el toolchain pi32v2 de JieLi y los tres blobs del SDK en un solo paso, con los
  SHA-256 fijados en el repositorio (`tools/toolchain.sha256`, `SDK_SHA256` en `tools/build.py`); no escribe
  nada que no coincida. `--check` informa sin escribir.
- **`tools/engine_size.py`**: XIP y `.data+.bss` **por motor**, desde `build/felucca.syms` (la tabla de
  símbolos) con `--map` como verificación cruzada contra el mapa del enlazador — 88 funciones, 0 discrepancias.
  `./build.sh` lo ejecuta con `--strict` al final: si un motor o la imagen no entran, el build falla en vez de
  dejar el número para después. `tests/engine_size_test.py` lo cubre en host, sin toolchain.

### Bloqueado
- **El emulador WASM**: no hay `emcc` en este entorno, así que `release/emu` sigue en 1.1.0 y la release
  pública sigue parada por la regla de que todos los componentes sirven la misma versión.
- **La unidad FM-1**: no hay aparato. El build AC791N corre y empaqueta (`build/felucca.fwsc`, identidad
  `FM-1_965`, manifiesto v1.2.6), y es un build de trabajo: no se publica como release.

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

## v1.4.5 — la pasada de optimización, medida (RAM, .data/.bss, pool, coste del ISR y del render)

Sin cambios de sonido ni de dibujo: 303 renders dorados idénticos, las 3 040 imágenes del barrido de UI idénticas
byte a byte, `audio_test`, `poly_test`, `ui_test`, `startup_test`, `backup_test`, `persistence_test`,
`project_test`, `settings_test`, `install_test` y las pruebas web en verde.

### La pantalla: revisada, y el pendiente que la podía traer de vuelta
La tabla del ST7789 sigue **sin gamma, sin brillo y sin VCOM** (1.4.4.1 quitó los escritos que aplastaban los tonos
bajos). Lo que quedaba era el comentario que *argumentaba a favor* de escribirlos, encima de `LCD_SEQ`: cualquiera
que lo leyera podía volver a meterlos. Ahora dice la regla —"no gamma, no brightness, no VCOM"— y por qué: los
valores de fábrica del panel son los únicos que nunca estuvieron mal.

### El pop de encendido, sin coste en el ISR
El fade de ~27 ms sigue ahí, pero ya no es una segunda pasada por todas las muestras: se aplica a la **ganancia del
máster que el bloque ya rampa** (`lev_cur`/`lev_dg` en fx.c), así que la multiplicación que ya existía hace el
trabajo. En el compilador del objetivo el coste estático del ISR de audio baja de **354 a 300** (presupuesto 291,
tolerancia +10 %: de "OVER BUDGET +22 %" a "ok +3 %"), y durante el arranque desaparecen dos multiplicaciones por
muestra sobre 1 200 muestras. Medido con `tests/target_budget.py` sobre `build/felucca.dis` (clase: build AC791N,
no es una medición en la unidad).

### Memoria: 3 840 B devueltos al presupuesto de 96 KiB
El buffer de staging del driver de flash (`st_buf`, 3 840 B, sólo lo toca el bucle principal; el ISR de audio nunca
lee a través de una escritura) se mudó a la ventana **LOWBUF**, que pasa a 0x01C08000 —la última página estaba
reservada "para depuración en RAM" y nada la usaba— y ahora lleva un `ASSERT` de linker: un buffer que no entre falla
al enlazar, no en la unidad.

| medida (clase) | antes | después |
|---|---|---|
| `.data+.bss` (build AC791N) | 95 140 B de 98 304 (3 164 libres) | **91 300 B de 98 304 (7 004 libres, 7,1 %)** |
| LOWBUF (build AC791N) | 26 722 B de 28 672 | 30 562 B de 32 768 |
| ISR de audio, coste estático (pi32v2) | 354 (OVER +22 %) | **300 (ok, +3 %)** |
| imagen (build AC791N) | 583 140 B de 583 612 | 583 172 B (440 libres) |
| pool (build AC791N) | 311 256 B, spare 9 248 | igual |

`gen_keep` (2 176 B) se probó en la misma ventana y se dejó en `.bss` a propósito: con los dos dentro, LOWBUF
terminaba a **28 B** del límite, que es una trampa para el siguiente buffer. Se prefirieron los **2 204 B** de
holgura que deja la mudanza de `st_buf` sola.

### Lo que se midió y NO se quedó
Tres candidatos se implementaron, se midieron y se revirtieron, porque una optimización sin número es una opinión:

- **`cv_rect` a dos píxeles por escritura** (el canvas ya es una unión de 32 bits) + salida temprana del rect
  vacío. Perfil de host: 43,3 M llamadas, 14,6 % del render. Resultado: **9 % más lento** (41,5 s contra 37,9 s del
  mismo barrido), imagen idéntica, y +80 B de XIP. Revertido.
- **Bloquear WIDTH y LOW CUT una vez por bloque** en `master_out` (se leían una vez por muestra). Coste estático de
  `mix_block`: **2 103 → 2 102** instrucciones en bucle. El compilador ya los había sacado del bucle. Revertido.
- **`gen_keep` a LOWBUF**, por la holgura de 28 B de arriba: revertido.

### Cobertura nueva
`tests/target_budget.py` vigila también **`mix_block`** (32 667), que es donde viven `fx_buses`, `master_out`, el
leveler y el fade: era la función más caliente del perfil y no tenía línea de presupuesto.

## v1.4.4.1 — la pantalla oscura en la unidad, las filas que se trababan y el pop del arranque

(Pedida como 1.4.2.1: se publica como **1.4.4.1** porque el proyecto ya está en 1.4.4 y el sitio y el instalador
rechazan un número de versión menor — lo comprueba `make_release --check`.)

Tres causas reales, encontradas en el código, y una tercera que era un ajuste imposible.

### La pantalla oscura (y las cosas negras) en la FM-1 real
1.4 empezó a escribir en el ST7789 una **curva gamma 2.2** (0xE0/0xE1) y a **encender el control de brillo al
78 %** (0x53/0x51) — valores elegidos comparando contra el emulador y **nunca medidos en un panel**, como el propio
CHANGELOG de 1.4 admite. En la unidad eso aplasta el extremo bajo del rango: las placas (`T_SURF`), los divisores
`LINE`, las etiquetas `DIM` y los pozos de los osciloscopios se van a negro, y el síntoma aparece en **todas** las
pantallas porque es el panel, no el dibujo — el splash incluido. Los dos escritos se fueron: el panel arranca con
sus valores de fábrica, como en 1.3.9, que es como se veía antes.

### MENU > BRIGHT no podía funcionar, y se fue con la causa
El brillo del registro 0x51 no hace nada en esta unidad porque **no hay camino de atenuación**: el backlight es el
GPIO PA2, encendido/apagado, y además es el /OE del 595 del teclado (con PA2 alto no se lee ni una tecla), así que
tampoco se puede PWM. Ese registro sin efecto era además el que podía dejar el panel atenuado. La fila BRIGHT se
elimina con él: un control que no puede funcionar es peor que ninguno. Fuera también el byte de ajuste.

### Las filas del MENU que "se trababan" (SPLASH, CPU MODE, VIS QUALITY, VOICES, TRACKS…)
`draw_menu()` no redibuja mientras una **firma escrita a mano** de unos pocos ajustes no cambie, y esa lista nunca
creció con las filas que agregó 1.4: SPLASH, CPU MODE, VOICES, TRACKS y VIS QUALITY **cambiaban el valor y dejaban el
texto viejo en pantalla**, que es exactamente "se traba al cambiar entre FULL, SHORT y OFF". Ahora `menu_put()`
invalida la firma del menú en **cada** cambio de fila: arregla la clase entera, incluidas las filas futuras, y con
`ui_test.c` recorriendo todas las filas para que no vuelva.

### Pop de audio en el splash
El codec y su amplificador suben mientras los primeros bloques ya se están renderizando: ese escalón iba derecho al
parlante debajo del splash. La salida del DAC ahora **entra con un fade de 1200 muestras (~27 ms)**: demasiado lento
para oírse como rampa, suficiente para que el primer bloque arranque en silencio. Medido en host: pico del primer
bloque 4 480 contra 942 528 después del fade (0,5 %).

### SPLASH FULL por defecto, y una prueba de los tres modos
El arranque sigue por defecto en **FULL animado** (0), con SHORT y OFF elegibles. `tests/startup_test.c` arranca la
unidad **en los tres valores** y comprueba: la fila lee lo que el equipo arrancó, OFF no dibuja un solo fotograma de
splash, FULL/SHORT sí lo dibujan antes de la UI, y en los tres la UI aparece después.

## v1.4.4 — el POLY, medido en la salida (y por qué un bajo suena a una nota)

### El POLY no estaba roto: ahora hay una prueba que lo demuestra en el audio
`tests/poly_test.c` (nuevo, paso de la suite) toca **C3 · E3 · G3 en un mismo track en POLY** y mira la **salida**:
tres voces con gate y las tres fundamentales presentes en el espectro (Goertzel sobre 4096 muestras, cada una por
encima del 20 % de la mayor). Se repite con `load_tight` puesto —el gobernador de carga, la sospecha que quedaba— y
da lo mismo: tres voces, tres notas. Medido en host x86: C3 1402, E3 1385, G3 1331 (y 1461 / 1444 / 1382 con el
gobernador).

### Por qué un sonido puede no dar polifonía aunque el modo diga POLY
El modo de voz **es parte del sonido**: `ui.c apply_preset_to` pone `P_VOICE` desde el preset (`mono` → LEGATO). Un
bajo o un lead marcado mono carga el track en LEGATO y suena de a una nota por más que el modo estuviera en POLY
antes de cargar el sonido. La prueba lo fija: un preset mono carga LEGATO, dos teclas dan una voz, y elegir POLY en
la página VOICE devuelve el acorde —hasta el próximo cambio de sonido—. Si en la unidad se oye una sola nota con un
sonido así, no es el asignador: es el sonido. Elegí POLY en VOICE *después* de elegir el sonido.

## v1.4.3 — limpieza de lo que dejaron 1.4.0 / 1.4.1 / 1.4.2 (sin cambiar nada de lo que se ve)

Pasada con ojos frescos sobre lo que agregaron esas tres versiones. **No cambia el comportamiento**: el arnés de
render da los mismos píxeles en 2 992 de las 3 040 capturas (las 48 restantes son ABOUT y el splash, que imprimen
`__DATE__`, la fecha de compilación), 0 hallazgos de lint y 0 medidas de alineación fuera de 1 px; los contrastes de
las 22 paletas y la suite host completa pasan.

- **Paletas:** `panel_dark()` y `PANEL_DARKEN` se fueron. Desde 1.4.2 el factor era 1.0: la función sólo hacía un
  `round()` por canal y una ida y vuelta por RGB565 para no cambiar nada. El header generado es idéntico byte a byte.
- **Pozos de los osciloscopios:** `vwell` era un alias de una línea usado dos veces; se usa `T_SURF` directo y el
  comentario de FASE 5 —que todavía describía el pozo *mezclado*, reemplazado en 1.4— se corrigió en vez de quedar
  como documentación falsa.
- **Firma de repintado de HOME:** `hv_view_sig()` hasheaba el motor y el preset de `TSEL` aparte y otra vez dentro
  del bucle de las seis pistas, porque `TSEL` es una de ellas. Esa línea era redundante y se fue; la firma es la
  misma función de lo que la pantalla muestra.
- **Popup de presets:** el `(void)r;` que quedó del patrón de líneas borrado en 1.4.1 se fue, y con él el comentario
  que explicaba trabajo que ya no existe.

## v1.4.2 — el brillo del panel se ajusta desde el equipo (MENU > EXTRA > BRIGHT)

La gamma 2.2 (0xE0/0xE1) y el brillo 200/255 (0x53/0x51) que 1.4 escribe en el ST7789, y el oscurecido al 72 % de
las paletas, salieron de comparar contra el emulador: **no están medidos en ninguna unidad**, y un panel con otro
pegado puede pedir otro valor. Hasta acá la única forma de moverlo era un build nuevo; ahora hay una fila.

**MENU > EXTRA > BRIGHT**: 25 %, 50 %, 78 % y 100 % (el valor por defecto, y el que lee un registro de ajustes que
nunca tocó la fila). Escribe el registro 0x51 en caliente al paso del knob, y el valor queda guardado en un byte
libre del bloque del motor 15 (`favorites.factory[15][25]`, 0 en todo registro viejo = 100 %): al arrancar, `main.c`
aplica lo guardado antes del splash, así que no hay un fotograma al brillo de fábrica. La secuencia de init del
panel ya no lleva 0x53/0x51: hay **un solo escritor**, `lcd.c lcd_bright_set()`, en el arranque y en el cambio.

### Los temas quedaban casi negros: el oscurecido de las paletas se fue
1.4 guardaba los dos colores de suelo de cada tema oscuro al **72 %** de cada canal para compensar la gamma del panel.
El mismo release empezó a escribir la gamma real del ST7789 (0xE0/0xE1) **y** un brillo del 78 % (0x53/0x51): las dos
correcciones se apilaban, el dato iba 28 % más oscuro y el panel comprimía otra vez el extremo bajo. Fondo,
divisores `LINE` y texto `DIM` desaparecían — "todos los temas quedan prácticamente negros". El oscurecido se fue
(`PANEL_DARKEN = 1.0`): las paletas vuelven a sus valores de diseño, que son los mismos que ven el emulador y el
sitio, y la compensación queda donde está el panel (el driver, y BRIGHT para lo que sólo la unidad puede juzgar), no
en los datos. La tabla de contrastes del generador pasa entera con los 22 temas (texto/fondo 14,2–21,0), el barrido
de render da 190 pantallas × 22 paletas con **0 hallazgos** y 0 medidas de alineación fuera de 1 px, y las hojas
`build/ui_new/sheet_*.png` de GAMMA, NIGHT y PAPER se miraron pantalla por pantalla.

Lo que **no** cambió y sigue sin medir: la curva de gamma del panel y el nivel de brillo que elige el dueño. Eso no
se resuelve desde acá, necesita la unidad: por eso BRIGHT es un ajuste y no una constante, y así queda en la matriz.

## v1.4.1 — el LFO SHAPE tenía tres knobs para dos valores, y el knob PRESETS repintaba todo

Dos de los cuatro puntos del reporte del dueño. El POLY no se reprodujo: abajo, lo que se miró y lo que falta.

### LFO SHAPE: SLEW, INVERT y otra vez INVERT, y cualquier knob movía los tres
La página tiene **dos** parámetros y su tabla los nombra así: `{"LFO SHAPE", ..., {P_X2, P_X3, 0xFF, 0xFF}}`, con
`P_X2` el SLEW y `P_X3` el INVERT. Pero `params.c page_desc()` tiene una rama propia para esa página —porque el
`SLEW`/`INVERT` no son descriptores de la tabla de parámetros sino dos structs suyos— y esa rama atendía **los
cuatro slots** igual: devolvía `D[slot & 1]` y `&t->p[slot ? P_X3 : P_X2]`. Con eso el knob 2 leía INVERT, el 3
volvía a leer SLEW y el 4 leía INVERT otra vez, y los tres escribían **el mismo byte** (`P_X3`): mover cualquiera
movía los otros dos. Es exactamente lo que reportó el dueño. Ahora los slots que la tabla deja en `0xFF`
responden sin parámetro, como en toda página vacía: el render de la página muestra **dos** tarjetas, SLEW 69 % e
INVERT 29 %, y las otras dos quedan libres (`build/ui_new/*/lfo_shape.png`; el barrido de alineación del arnés
bajó de 3,89 M a 3,78 M medidas porque las dos tarjetas fantasma ya no existen).

### El knob PRESETS hacía parpadear la pantalla
Cada paso del knob hacía `ui.force = 1`, y eso redibuja **el cuadro entero** —cabecera, las cuatro tarjetas, el
grafo y el pie— con el popup de presets encima; al vencer el popup, otro cuadro entero para quitarlo. En el panel
eso es el parpadeo. El nombre del sonido y todos sus valores viven en el grafo, y `graph_signature()` ya lleva el
preset: ahora el paso del knob pide **sólo el grafo** (`ui.graph_sig = 0`) y, cuando el popup se va, se repinta
sólo esa zona. Además el popup dejó de pintar su patrón de líneas en `T_BG` antes de dibujar su placa: era
trabajo tapado por la placa y, en las esquinas redondeadas, rayas de fondo sobre la página.

### POLY: no reproducido en el modelo
El señor dice que el modo POLY no da polifonía. En código no lo encontré: `trk_note_on()` en POLY toma
`voice_alloc()` para cada nota (una voz nueva por nota, sin tocar las que suenan), el pool reparte 17 slots entre
6 partes (2 por parte, hasta `NVOICE` con los libres), `trk_vcap()` sólo limita por `ENGINES[..]->poly` (FM6 2,
FORMANT 4, el resto 12) y el teclado es por tecla (`kb_note[27]`, un note-on por tecla). `tests/audio_test.c`
ya toca acordes de tres notas en POLY y pasa. La sospecha que queda es el gobernador de carga: con `LOAD` LITE /
ECO o con `load_tight` puesto por el render, `voice_room()` deja de crecer y se lleva la voz que suena, y en host
la carga lee 0 (no se mide). Eso hay que verlo en la unidad, no adivinarlo: **no se tocó el asignador**.

## v1.4.0 — el splash animado, HOME sin repintarse de más, y los pozos de los osciloscopios

Cuatro cosas del reporte del dueño, cada una con su causa.

### STARTUP SPLASH FULL no se podía tener: se veía igual que SHORT
La animación del arranque estaba condicionada por **MENU > ANIM**: con `ANIM OFF` —el ajuste de la unidad del
dueño— `draw_splash()` escribía la firma entera de una vez y `splash_write()` volvía sin dibujar, así que FULL y
SHORT daban la misma imagen fija. Ese bit es de los valores (dígitos que ruedan, glides: `ui.c` ANIM_SNAP), no
del arte del arranque: ahora decide sólo el modo. Además el bucle del boot tenía un tope fijo de 600 pasos de
4 ms (2,4 s) mientras FULL dura `SPL_WRITE + SPL_HOLD` (4,5 s): el tope sigue al span, y el splash se escribe
hasta el final. FULL queda como opción por defecto (0 en el ajuste). Medido en host: con ANIM OFF la línea de
tiempo escribe 127 fotogramas distintos, y con ANIM ON 244 de 1125 pasos cambian la pantalla (la suite lo
comprueba en `tests/ui_render.c`).

### HOME 2 a 5 parpadeaban y se sentían lentas
OVERVIEW, MIXER, PERFORMANCE y SYSTEM redibujaban su cuerpo entero —tres canvas de 240 px y tres blits por
SPI— en cada fotograma, hubiera cambiado algo o no. Ahora hay una firma de lo que la pantalla muestra (página,
paleta, medidores cuantizados, CPU, batería, motor y preset de cada pista) y el cuerpo se dibuja **sólo cuando
esa firma cambia**; `ui.force` sigue mandando en un cambio de página o de tema. Las brasas del fondo pasan de
avanzar por fotograma a 1/8 de fotograma: siguen derivando, pero una pantalla quieta no gasta bus.

### Los osciloscopios tenían dos cuadros negros de fondo
1.3.9 hizo opacos los pozos oscureciendo `T_BG` otros 26 niveles: en el panel eso se lee como dos rectángulos
negros sobre la pantalla. El relleno hace falta (el campo y sus mundos no deben verse bajo la rejilla ni la
traza), así que ahora el pozo se pinta en el tono de la placa donde vive, `T_SURF`, con su marco de 1 px.

### La pantalla, y la mayoría de los temas, se veían más claros en la FM-1
Dos causas, dos arreglos. En los datos: los dos colores de suelo de cada tema oscuro se guardan al 72 % de cada
canal (PAPER queda claro a propósito), y todo lo derivado —MID, DIM, LINE, SEL, TINT— los sigue; la tabla de
contrastes del generador sigue pasando entera. En el panel: la secuencia del ST7789 escribía sólo el color y la
inversión y dejaba **gamma y brillo en los valores de fábrica**, que es lo que levanta el extremo bajo del rango
565; ahora se escriben las curvas 2.2 (0xE0/0xE1) y el brillo a 200/255 (0x53/0x51). Eso es un cambio de
valores por defecto: **no está medido en una unidad**, y así queda anotado en la matriz.

## v1.3.9.1 — un publicador de Pages, el emulador en el release y un índice de lo medido

Cambios de publicación y de evidencia: **no toca el DSP, ni la UI, ni el formato de proyecto**. El paquete
sólo se diferencia de 1.3.9 en la cadena de versión.

### Pages tenía dos publicadores
`pages.yml` subía `site/` tal como está commiteado y `emulator.yml` subía `build/site`, cada uno en su grupo de
concurrencia: ganaba el que terminara último y la portada podía ser cualquiera de los dos, nombrando la versión
de dos maneras (`1.3.8` contra `v1.3.8-<sha>`). Publica sólo el workflow que construye el sitio; las dos
comprobaciones que `pages.yml` aportaba (`make_release --check` y `test_site`) viven ahora en `coherence`, que
ya fija Python y hace checkout completo. Medido en el sitio en vivo: la raíz sirve `v1.3.9-33e9b0e`.

### El release no traía con qué escuchar el firmware
Se adjunta `qg1-emulator-<v>.zip`, armado por `make_release --sync` desde `site/emulator/` (la copia
versionada, no la generada: `release/emu/fonts/` está en `.gitignore` y un checkout no la tiene) con nombres
ordenados y fecha fija, así que dos corridas dan el mismo sha256. `--check` verifica que cada miembro del zip
sea el fichero que está al lado.

### El run decía verde y el asset no existía
El paso que publica el release daba por bueno cada `gh release upload`: en v1.3.8 el archivo del emulador
quedó en la lista y no en el release, un 404 para quien lo descarga. Ahora el paso **vuelve a leer los assets**
al terminar, los publica en el resumen del run y falla si falta alguno.

### La atestación se podía leer sólo con credencial
`gh attestation verify` corría en el job `verify` y su salida quedaba en el log, que pide token. Ahora se
guarda como artefacto `attestation-<v>` (JSON + sha256 del paquete, 90 días) al lado del run.

### Peso del repositorio
`site/firmware/` se queda con el paquete actual y los dos anteriores; los más viejos siguen en su página de
release. Eran cuatro copias de 600 KB de la misma imagen.

### El índice de la evidencia ya no se escribe a mano
`docs/EVIDENCE.md` se genera desde `docs/VALIDATION_MATRIX.md` con `tools/evidence_index.py`, y `--check` está
en la suite del host: una fila que vuelve a PENDING rompe el árbol en vez de esperar a que alguien la lea.
En la primera corrida aparecieron tres filas PENDING que ya estaban medidas (el audio del LATCH en
`tests/perform_test.c#test_latch_audio`, el WASM y la atestación) y una cuarta, el mapeo control→DSP, que
enumeraba como medido lo que no lo estaba.

## v1.3.9 — la banda de HOME, el visualizador entero y FX legible

Los seis bugs de UI del reporte, cada uno con su causa. Medido en host x86 (render) y en el enlace del AC791N.

### El visualizador mostraba las tarjetas de HOME
Sus dos pasadas de 88 filas empiezan en la fila 26 y nadie pintaba `Y_LABEL` (20) .. 25: `draw_frame()` sólo
rellena los huecos **entre** tiras, así que los bordes de las cuatro tarjetas de knobs que HOME acababa de dibujar
seguían en pantalla — los "cuadrados" del reporte. Una pasada-cap de 26 filas del tono de la página, bliteada en
`Y_LABEL`, las cubre; las pasadas dibujan después sobre 26..202.

### La línea atravesaba OVERVIEW, MIX y PERFORMANCE
Esas tres vistas pintaban las seis filas de arriba con `T_SURF` liso y arrancaban su degradado en 26, o sea una
franja más clara sobre el fondo, y su nombre iba en `y = 34`, cuyas mayúsculas de `AF_S` ocupan las filas 37..45:
la raya de separación de `y = 41` lo cortaba por la mitad (en MIX se leía "MIX" y "ER" con una línea en medio).
El cap pinta ahora `hv_grad()` —la misma rampa de la página— y el nombre va en `y = 28` (tinta 31..39) en las tres
pantallas y también en ROUTING, con lo que la raya queda limpia bajo el texto.

### Los osciloscopios dejaban ver el campo
Los dos pozos del visualizador usaban el tono de la página **mezclado hacia `T_BG`**, pensado para que la vía
láctea se viera a través; con la rejilla y la traza encima, lo que se veía era el campo debajo del instrumento.
Cada pozo tiene ahora su propio tono opaco (`bg_darken(T_BG, 26)`), y el campo queda fuera de los dos.

### FX: BYP, el porcentaje y el OFF encima de sus barras
Los tres módulos de la primera sección dibujaban su lectura en `y = 99`, y su barra ocupa 108..116: el texto caía
sobre el borde de la barra. Van en `y = 94` (tinta 97..105), y la rampa de esos tres módulos baja de 22..62 % /
10..45 % a **14..50 % / 6..32 %**, con `ux_ground_at(13, 37, 6, 32)` para que el texto siga el mismo degradado:
los cuadros se leen más oscuros y el texto no compite con el relleno.

### Coste
**+128 B** de imagen: 582 644 → **582 772 B de 583 612** (840 B libres), `.data + .bss` sigue en **95 124 B de
98 304** y el paquete en **610 959 B** con identidad **FM-1_965**. Nada se mueve en RAM.

### Cómo se comprobó
`tests/ui_render`: 190 pantallas × 22 paletas, **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
GREY/MONO fuera de gris/neutro, **0 desalineaciones en 3 894 154 medidas**. Los volcados de `home_viz`, `home_mix`,
`home_perf` y `fx` muestran las filas 20..25 en un solo color y las tres lecturas de FX separadas de sus barras.
No hay unidad FM-1 en este host: nada de esto es una medición de hardware.

## v1.3.8 — el banner se vuelve a dibujar desde cero, y el portal deja de estar en blanco

### El banner es otro, y lo dibuja el repositorio
- **`tools/gen_banner.py` se reescribió entero.** El motivo ya no es el planeta: es un estado ligado. Un
  campo de densidad de probabilidad —anillos de fase con caída 1/r, dibujados antes que el texto y a baja
  opacidad—, un núcleo con tres órbitas elípticas a 60° y un electrón en cada una, una nube de puntos
  repartida por ángulo áureo y una escalera de niveles `n=1..5` con la flecha de absorción. La rejilla de
  cruces y los puntos discretos son la pantalla del aparato, no un campo de estrellas: la semilla es fija,
  así que la imagen se regenera idéntica y se puede revisar en el diff.
- **Nada del banner es inventado.** Los colores salen de la paleta QUANTUM decodificada de `ui_palettes.h`
  (`0x0022` fondo, `0x10a5` panel, `0x3eff` acento, `0xfd44` tema), las tipografías son las de
  `assets/fonts` y las dos imágenes pequeñas son las pantallas reales de 240 × 240 —
  `quantum_01_home.png` (HOME / ORBITAL) y `quantum_03_fm6_algorithm.png` (FM6 / ALGORITMO) —, enmarcadas
  como referencia, no como maqueta.
- **Se mantienen las características y no aparece la versión.** Las píldoras dicen **12 voces, 6 pistas,
  10 motores, 285 presets, secuenciador de 64 pasos, 4 buses de FX y 19 paletas**, todas ellas cifras que el
  árbol sostiene (`UI_PAL_NLIVE` = 19, `preset_categories` = 283 + 2 kits, `engines.c` = 10). No hay
  `v1.3.x`, ni hash, ni build: el banner es el mismo en cada release.
- **Las cifras anteriores estaban viejas y se corrigieron de paso:** el banner anterior anunciaba **273
  presets** y **17 temas**; el README decía *nine synthesis engines* y *nine palettes including GREY and
  MONO*. Ahora dicen 285 y 19 paletas, y diez motores. `tools/generate_banner.py`, un segundo generador que
  la matriz citaba y que no producía el fichero publicado, se retiró.

### El portal raíz estaba commiteado con cero bytes
- **`site/index.html` estaba vacío en `main`.** `pages.yml` publica `site/` *tal como está commiteado*, así
  que el portal servía una página en blanco mientras el emulador, el instalador y los paquetes seguían
  respondiendo. Nadie lo vio porque `web/test_site.py` valida `build/site` —la copia que construye el
  workflow del emulador— y la coherencia miraba `versions.json` y el paquete, nunca el HTML del portal.
- **Arreglado regenerando `site/` con `web/make_site.py`** (portal con `{{VERSION}}` sustituido, `assets/`,
  `webapp/installer`, `webapp/editor`, `plan/` y `firmware/`), y con una guarda nueva en
  `tools/coherence_check.py`: `site/index.html` tiene que existir, no estar vacío y nombrar la versión que
  lleva el firmware. Un portal en blanco vuelve a fallar en CI en vez de publicarse.
- **Los paquetes ya publicados siguen descargables.** `site/firmware/` conserva 1.3.4 a 1.3.8, y cada
  `.manifest.json` de la carpeta se comprobó contra los bytes que tiene al lado: cinco de cinco coinciden.

### Lo que se midió en esta versión
- **Build del AC791N `v1.3.8`** (`d2fe9d2`): imagen **582 644 B de 583 612** (968 B libres), XIP código
  **422 928 B** + constantes **139 288 B**, `.data+.bss` **95 124 B de 98 304**, pool con **9 248 B** de
  sobra, RAM code **19 632 B de 24 576**. Paquete **610 959 B**, sha256 `6446de87…`, identidad `FM-1_965`.
- **El firmware no cambió de comportamiento:** la única diferencia con 1.3.7 es la cadena de versión, y el
  reparto de memoria es exactamente el mismo (mismos bytes, mismas constantes, mismo código). Esta release
  es de identidad, portal y documentación; no trae un motor nuevo y no lo finge.
- **Emulador `v1.3.8`** recompilado con Emscripten 6.0.11: WASM **1 240 205 B**, sha256 `a45e639e…`,
  etiquetado con el commit del árbol. `site/emulator/` y `release/emu/` son esa compilación.

### Lo que sigue sin poder afirmarse
- **Nada de esto se probó en una unidad física.** Las mediciones de arriba son **build del AC791N**,
  **host x86** y **emulador**; el FM-1 del dueño no se escribe desde este repositorio.

## v1.3.7 — 904 B de vuelta de los keycaps que nadie dibuja, y el precio medido del parámetro siguiente

### Lo que se recuperó, medido
- **Seis píldoras de keycap que ninguna pantalla dibujaba, fuera del generador.** `KC_ARP`, `KC_K3`, `KC_K4`,
  `KC_HOLD`, `KC_ON` y `KC_OFF` se rasterizaban en `tools/gen_aa_keycaps.py` y ningún `cv_keycap` las nombraba
  (el audito de las 29 etiquetas contra el código no encontró ni una llamada). Quitarlas deja `KC_DATA` en
  **3 939 B** (de 4 792) y el bloque de keycaps en **4 699 B** (de 5 564).
- **La imagen del AC791N bajó lo mismo que el dato: 583 548 → 582 644 B, 968 B libres** (de 64). El XIP de
  código no se movió (**422 928 B**): los 904 B salieron enteros de las constantes (140 192 → 139 288), que es
  exactamente lo que se esperaba de un dato que ya no se guarda. `.data + .bss` sigue en **95 124 B de 98 304**,
  el pool con **9 248 B** de sobra, la RAM code en **19 632 B de 24 576**.
- **El blob de formas no costaba imagen, y eso quedó medido, no supuesto.** `build/gen/ui_shapes.h` son
  **10 861 B de puntos**; bajarlo a 9 661 B con knobs de simplificación nuevos (`--eps=0.6 --flat=0.8`) dejó la
  imagen **idéntica**, porque `nm -S build/felucca.elf` no tiene **ningún** símbolo `SH_PLATES_*` ni `SH_*_PTS`:
  `ui_organic.c` se incluye desde `ui_graph.c` pero su arte no se referencia, y el enlazador no lo lleva. Esa
  premisa de partida era falsa para esta configuración y la vía se cerró con la prueba, no con una opinión.

### El parámetro que debía gastar esos bytes: implementado, medido, y no entra
- **KEYTRK** (profundidad de keytracking del filtro) era el candidato con más diferencia sonora: el seguimiento
  está escrito a mano como `(pitch16 - 60 * 16) * 4` en seis sitios (`eng_analog.c` ×4, `eng_wavetable.c`,
  `eng_additive.c`) y ninguna página podía tocarlo. Se implementó entero —enumeración, descriptor `F_PCT`,
  la fila libre de **VOICE 3**, los seis sitios de motor y default 127 = el 4 de siempre, bit a bit— y se midió
  **tres veces**:
  - insertado antes de `P_E0` (donde el mapa por conteo lo exige): **585 100 B**, +2 456;
  - añadido después de `P_E7`: **585 084 B**, +2 440;
  - con un segundo parámetro reserva: **585 088 B**, +2 444.
  El reparto dice por qué: constantes **+36 B**, RAM **+48 B** y **código +2 404 B** repartidos en **147
  funciones** (`ui_input_frame` 13 842 → 14 444, `draw_graph` +150, `graph_signature` +146, `note_edit` +140…).
  Ninguna de esas funciones lee el parámetro: crecer `track_t.p[]` 2 B mueve **todos** los campos de una
  estructura de **1 760 B** y el compilador rehace las codificaciones de acceso en toda la UI. No es un coste del
  parámetro, es el precio de tocar esa estructura.
- **Conclusión honesta**: 904 B recuperados, 2 456 B de coste. El parámetro **no entra sin recortar calidad o
  contenido**, y por eso no se publica a medias: queda documentado con la ruta que sí cabe —
  `track_t` tiene **un byte de relleno exacto** (offset 327, entre `latched` y `arp_pos`; `sizeof` 1 760,
  alineación 4), y un control sin id de `p[]` puede exponerse por el mismo embudo que ya usan los slots 5..8 de
  la matriz y los macros (`params.c page_desc`). Lo que falta decidir es el **guardado**: los ids de 7 bits están
  en 125 de 127 y los formatos de proyecto y preset no tienen un byte libre por pista, así que un parámetro de
  primera clase necesita una decisión de formato, no 900 B más de flash.

### La atestación, de vuelta: el tag que crea el runner ahora sí dispara el build atestiguado
- **El eslabón que faltaba estaba en GitHub, no en el árbol.** `tag-and-publish.yml` crea el tag con
  `GITHUB_TOKEN`, y GitHub **no arranca workflows** desde un `push` de tags hecho con ese token: `release.yml`
  —el que compila con `zvenson/fm1-build` y consigue el «Built from source on GitHub»— nunca corrió para los
  tags que el propio repositorio creaba. Por eso las releases 1.3.x salieron sin atestación aunque el paquete
  verificado estuviera ahí.
- **El arreglo**: el job pide `actions: write` y, después de publicar, **dispara `release.yml` por
  `workflow_dispatch`** nombrando el tag en un input. Va en un input y no en el `ref` del dispatch porque GitHub
  lee el fichero del workflow **en el ref donde se dispara**: un tag cuyo árbol es anterior al trigger
  (v1.3.6 y todo lo previo) se rechazaría en su propio tag. Disparado en la rama por defecto, el gate y el
  verify hacen `checkout` del tag nombrado y el build mueve el árbol al tag antes de compilar, así que se
  atestigua el árbol del tag, no el de `main`.
- **Una versión, una run**: `release.yml` estrena `run-name: release <tag>` y `concurrency` por versión, y el
  disparo consulta las runs existentes: si ya hay una en curso la deja, si ya terminó en verde no la repite.
  `release/attest-backfill.txt` nombra las versiones ya publicadas sin atestación (hoy `v1.3.6`) para que el
  siguiente push las repare solo, sin que nadie tenga que acordarse de un comando.

### Lo que sigue sin poder afirmarse
- Nada de esto se probó en una unidad física: el FM-1 del dueño no se escribe desde este repositorio. Las
  mediciones de arriba son **build del AC791N** y **host x86**; la suite completa, los renders dorados y el
  emulador se corren en esta versión como en todas.

## v1.3.6 — fase final: el árbol sin avisos y el ecosistema en una sola versión

### Optimización y limpieza, medidos
- **Seis avisos del compilador del AC791N a cero, sin tocar una línea de comportamiento.** El build del target los
  venía arrastrando y cada uno era una costura: `ui_seqtools.c` escribía los pasos del secuenciador a través de un
  accesor `const` (ahora usa `TSEL->step` directo, que es el dueño de esos bytes); `eng_phase.c` pasaba el estado del
  ruido como `uint32_t *` a `noise32(int32_t *)` (ahora el estado es `int32_t`, los mismos bits); `ui_scl.c` declaraba
  `nt[12]` en el manejador de la progresión y nunca lo leía (**12 B de pila de vuelta**); y `ui_being.c` (dos veces) y
  `main.c` tenían locales muertos. `./build.sh` queda en **0 warnings y 0 errors**.
- **Presupuesto del target, remedido tras la limpieza y sin regresión**: imagen **583 548 B de 583 612** (64 B libres,
  los mismos que la 1.3.5), `.data + .bss` **95 124 B de 98 304** (3 180 B libres), pool **311 256 B con 9 248 B de
  sobra** (el mínimo exigido son 8 KiB), RAM code **19 632 B de 24 576**, XIP 563 120 B. Ningún motor, FX ni preset
  nuevo costó nada en esta versión: no se añadieron, y los 285 presets siguen entrando en los mismos bytes.
- **CPU y DSP sin regresión**: los 303 renders dorados quedaron **0 cambiados** y el presupuesto de CPU del regresor
  en **0 entradas por encima** (20 notas medidas), así que la limpieza no movió el sonido ni el coste por bloque.

### El ecosistema, en la misma versión
- **Una sola v1.3.6 en todas partes**: `FELUCCA_VERSION`, paquete `.fwsc` y su manifiesto, `build-manifest`, emulador
  WASM (`site/emulator` y `release/emu`), Web Studio (su `data-version` y la pantalla que dibuja la versión), el
  instalador USB (su `meta` con el build, el emulador y el paquete), `site/firmware/versions.json`, ambos
  `SHA256SUMS`, y las capturas y el banner regenerados desde el firmware de esta versión.
- **El README dejó de mentir sobre sí mismo**: decía `v1.3.2`, «9 motores» (son diez: ANALOG, PHASE, FM6, LOFI, VOICE,
  WHEEL, NOISE, DRUM, WTABLE, ADDTV), «281 presets» (son 285: 283 navegables más dos kits legacy que los proyectos
  viejos siguen nombrando) y «293 renders dorados» (son 303). Y `fmhub.json` anuncia ahora esos mismos 285.
- **El `SHA256SUMS` de la raíz volvió a estar completo**: le faltaba la entrada de la 1.3.5 (se había actualizado el
  del directorio `release/` y no el de la raíz). Ahora lista 1.3.5 y 1.3.6 con su paquete, su manifiesto y su
  `build-manifest`.

### Lo que sigue sin poder afirmarse
- Nada de esto se probó en una unidad física: el FM-1 del dueño no se escribe desde este repositorio. Todo lo de
  arriba es **build del AC791N**, **host x86** (suite completa) y **emulador WASM**; la clase de cada medición está
  dicha donde aparece.

## v1.3.5 — el visualizador deja ver lo que hay detrás: tinta translúcida de verdad

### Lo que está hecho y medido
- **Las ondas del visualizador son translúcidas de verdad, no de color parecido.** El framebuffer se puede leer, así
  que cada punto de un trazado ahora es **el color que ya había allí mezclado hacia la tinta** (`gfx.c`: `cv_ink`
  global leído por `cv_pset`, y `cv_line_a(x0,y0,x1,y1,c,pct,thick)` que pone la tinta, recorre con el Bresenham de
  siempre y la devuelve a 100). Antes el trazo tapaba la banda de espectro, la rejilla y el campo de estrellas que
  cruza; ahora los atraviesa: se ven las dos capas a la vez.
- **Cuatro trazados, tres niveles, ningún control nuevo.** El reparto está en la escalera `CV_A_SIGNAL 84`,
  `CV_A_CURVE 70`, `CV_A_WAVE 74`, `CV_A_MOD 56` (`gfx.c`): el osciloscopio de la bahía de HOME y su página WAVES
  (**84 %**, lo más sólido porque es el dato que se lee), la respuesta del filtro (**70 %**), la onda del kit de DRUM
  (**74 %**) y la envolvente de modulación (**56 %**, lo más tenue porque es la referencia de fondo). Todos son puntos
  ya existentes de `ui_being.c` (`gw_live_trace`, la curva del bay, `gw_drum_wave`, la envolvente): la opacidad va
  atada al papel de cada trazo en vez de a un ajuste que haya que tocar.
- **Comprobado con una prueba que falla si se ignora la mezcla.** `tests/ui_test.c` suma «VISUALIZER: the scopes and
  waves are see-through ink over what they cross, in three ordered shares»: sobre un lienzo conocido pinta una banda,
  dibuja la misma tinta encima a los tres repartos y exige que el píxel sea **exactamente** `ux_mix(fondo, tinta,
  pct)` (o sea, que el fondo esté dentro del resultado), que la misma tinta sobre dos fondos distintos dé dos colores
  distintos, que `cv_line_t` a 100 % siga siendo opaco, y que la escalera esté ordenada con la más tenue conservando al
  menos la mitad de su tinta. La distancia se mide canal a canal y no por luminancia: dos colores de la paleta pueden
  diferir en tono sin que uno sea más brillante, y el trazo tiene que caer entre ellos igual.
- **Costo: 96 B de imagen y 0 B de RAM.** La imagen del AC791N queda en **583 548 B de 583 612** (**64 B** libres),
  `.data + .bss` en **95 124 B de 98 304**, pool con **9 248 B** de sobra y RAM code en **19 632 B de 24 576**. La
  primera versión duplicaba el Bresenham y el enlazador la rechazó (`region XIP overflowed by 96 bytes`): la que
  queda reutiliza `cv_line_t` y no añade ni un bucle.
- **Capturas regeneradas porque el dibujo cambió de verdad** (`tools/fw_shots.py --render`, y las nuevas copias en
  `site/assets`): `fw-home.png` y `fw-presets.png`. Los renders dorados de audio siguen en **303, 0 cambiados**,
  porque la tinta es de la interfaz y no toca la voz.

### Lo que NO entra, y por qué (medido, no estimado)
- **No hay parámetros de motor nuevos, ni filas nuevas en EDIT/LFO/ARP/SEQ/SCL, ni buses de FX nuevos.** El visualizador
  tenía **160 B** libres al empezar la fase y esta mejora se queda con **64 B**: en la 1.3.2 una sola página nueva
  costaba **92 B** de descriptores (`page_desc +92`, ver `docs/BUILD_AC791N.md` §8.4) y un parámetro de motor es más
  caro todavía (entrada en la tabla, nombre, rango y su caso en el dibujo). Con 64 B no hay forma honesta de meterlo:
  queda documentado como lo que es, un pendiente que necesita flash, no un cambio que se pueda simular.
- **El repaso de las trece pantallas no encontró huecos de transparencia que arreglar**: el visualizador a pantalla
  completa ya tenía sus dos pozos de cristal (`graph_home_viz`, mezcla `ux_mix(bg_darken(T_BG, 14), T_BG, 62)`) y sus
  medidores de 16 bits, y MIXER, OVERVIEW, PERFORMANCE, PRESETS, GLO, SCL, ARP, SEQ, FX y MENU ya usan las mismas
  piezas compartidas (`gw_seg_v`, `ux_mix`, `heat_col`). Lo que faltaba era justamente la bahía de HOME, que es lo que
  esta versión arregla.

## v1.3.4 — fase 3: el banco medido por cómo suena, y los cuatro knobs contestan en HOME

### Lo que está arreglado y medido
- **Los knobs 2 y 3 no animaban la gráfica principal.** En HOME el dibujo del motor pulsa cuando algo cambia
  (`ui.home_pulse`, `ui_being.c`), pero solo lo ponían PRESETS, un toque en HOME y el menú: girar el knob 1 o el 4
  se veía vivo (sus filas de valor se redibujan) y girar el 2 o el 3 movía un valor que **ninguna otra cosa
  mostraba**. Ahora lo ponen los cuatro, así que el anillo del ser responde a la columna que se haya girado, en
  **todos los motores**: ANALOG, FM6, PHASE, LOFI, VOICE, WHEEL, NOISE, DRUM, WTABLE y ADDTV.
- **Comprobado con la regresión que falla sin el arreglo.** `tests/ui_test.c` suma
  «HOME: each of the four knobs pulses the picture, in four different engines» (4 knobs × 4 motores): **falla con la
  línea quitada y pasa con ella**, y `ui_test` entero queda en verde (`all passed`).
- **La auditoría ahora mide el SONIDO, no solo los parámetros.** `preset_audit_test --fingerprint` pasa cada preset
  por el camino real de carga y render (una nota sostenida; en DRUM tres golpes, bombo, caja y charles, porque un kit
  son ocho canales) y saca diez bandas logarítmicas por Goertzel más la forma de la envolvente (bloque al 90 % del
  pico y bloque de vuelta al 50 %). `tools/preset_audit.py --sound` ordena los pares más cercanos por eso, y
  `--gate-sound` es la puerta de la suite: **5 037 pares medidos, 0 por debajo de 0,05**.
- **La distancia por parámetros ya no confunde una elección con un número.** FM6 escribe su sonido en `ALG` y
  `SLOT` (0 y 14 no son «un poco más brillante»: son otro parche) y el audit los normalizaba como si fueran
  continuos: por eso marcaba **43 pares casi iguales** que en realidad eran presets distintos. Con `ALG/SLOT/WAVE/
  WAV#/ALGO/TYPE/KIT/MODE/SCALE/ORDER/DIV/PATCH` contados como elección quedan **2 pares por debajo de 0,035**
  (MUSIC BOX / FROST MUSIC 0,017 y TINE EP / EMBER TINES 0,025), y los dos se distinguen por FB, envolvente de
  filtro y envíos: el motor elige el timbre en el parche y lo demás son envíos, que es exactamente lo que el audit
  de sonido mide aparte (0,148 y 0,108 de distancia real).
- **Menos presets que se pisaban, con el número antes y después** (distancia de sonido, 0 = el mismo sonido; el peor
  par por motor, antes → después): LOFI **0,043 → 0,080**, ANALOG **0,059 → 0,071**, DRUM **0,071 → 0,102**, FM6
  **0,077 → 0,095**, y **cero pares por debajo de 0,06 en todo el banco** (antes había cuatro). Todo con edición de
  valores in-place: **ningún ID se movió, ningún array se reordenó**.
- **DRUM: los kits que se parecían ahora son tres kits distintos.** `LINNDRUM` pasa a seco, brillante y corto (TONE
  104, DECAY 44, DRIVE 14, sin reverb) y recupera los 15 dB que le faltaban al banco (`P_LEVEL` 124, 972 → 2 653 de
  RMS); `ELECTRO` pasa a profundo, ruidoso y largo (TUNE 60, DECAY 92, NOISE 80, más delay); `QUANTUM BEAT` pasa a
  oscuro y metálico (MORPH 104, TONE 30, DECAY 100, FM ENV 44, FOLD 48). `DRUM-X` queda como el kit de referencia.
- **LOFI, ANALOG y FM6: lo mismo, en los pares que quedaban.** `STEP LEAD` (WAVE 3, DUTY 96, VIB 40, BEND 20) y
  `CIUDAD DE PLAYA` (CHIP 2, CRUSH 60, CUT 70, MASK 45) dejan de parecerse; `CHIP ARP` (CHIP 1, DUTY 8, BEND 30) y
  `POCKET CONSOLE` (WAVE 2, CRUSH 34, CUT 104) también; `SIESTA DORADA` pasa a caña con intervalo (WAVE 6, MIX 96,
  RES 40, DRIVE 28, INT −12) y `HARP PLUCK` a cuerda pulsada de verdad (CUT 74, RES 58, fenv 62). **GAMELAN MET
  tenía el mismo parche que VIBES y solo cambiaban los envíos**: ahora usa el parche de barra percutida (slot 5, el
  de MARIMBA) con su propio shimmer. Y **las tres campanas del mismo parche** (TEMPLE BELL, GONG SHIMMER y TEMPLE
  CIRCUIT) dejan de ser tres envíos distintos del mismo sonido: GONG SHIMMER toma FB 4, MOD 16, MEG −10 y DETUNE 26
  (un gong que ruge en vez de un campanilleo) y TEMPLE CIRCUIT MOD −12, RATIO −5 y MEG 12; **de 0,009–0,022 de
  distancia pasan a 0,31–0,34**. El par más cercano de FM6 sube de 0,077 a 0,095.
- **ADDTV, el motor más pobre (8), suma dos sonidos que no tenía**: `REED HORN` (parciales impares y codo suave, mono,
  como lead de caña) y `AIR CHOIR` (parciales estirados, LFO lento abriendo el filtro). Su par más cercano queda en
  **0,132**, el más separado de todo el banco.

### Inventario (medido, no copiado de la documentación anterior)
- **285 presets en 10 motores, 283 visibles y 2 legacy** (HEARTH KIT y CYBER DRUM, ocultos por `legacy` sin romper
  la carga de proyectos viejos). ANALOG 68, FM6 37, PHASE 32, LOFI 29, VOICE 25, WHEEL 22, NOISE 22, WTABLE 22,
  DRUM 18, ADDTV 10. Categorías: PADS 65, KEYS 57, TEXTURES 49, DRUMS 24, BELLS 21, BASS 20, ORGANS 18, LEADS 12,
  PLUCKS 12, ARPS 7. `docs/QG_PHASE4_PRESET_INVENTORY.json` ahora **se genera** con
  `python3 tools/preset_audit.py --inventory docs/QG_PHASE4_PRESET_INVENTORY.json` desde las tablas del firmware y
  el render, así que no puede volver a quedar viejo.
- Niveles por motor (RMS mediano, una nota sostenida por preset): ANALOG -17.9 dB, FM6 -19.9, ADDTV -17.3, WTABLE -16.5, DRUM -14.4,
  NOISE -29.0 (con GEIGER CNT, que es un sonido disperso a propósito). Ningún preset silenciado, ninguno por encima del clipper: 0,00 % en los 285.

### Los renders dorados que cambian, y por qué
- **12 de los 301 renders dorados cambian y se regeneran a propósito** (`GOLDEN_UPDATE=1`), porque son exactamente
  los presets retocados: ANALOG HARP PLUCK y SIESTA DORADA; LOFI STEP LEAD, CHIP ARP, POCKET CONSOLE y CIUDAD DE
  PLAYA; DRUM LINNDRUM, ELECTRO y QUANTUM BEAT; FM6 GONG SHIMMER, GAMELAN MET y TEMPLE CIRCUIT. Los otros 289 quedan
  **idénticos** y los dos presets nuevos de ADDTV entran como renders nuevos: **303 en total, 0 cambiados, 0
  perdidos** en la pasada de verificación. Ningún cambio de UI: el barrido de pantallas y su lint salen igual.

### Memoria y mediciones (build AC791N de este árbol)
- Imagen 583 352 → **583 452 B** de 583 612 (**160 B** de margen: los dos presets nuevos y los retoques cuestan
  **100 B**). `.data + .bss` **95 124 B** de 98 304 (3 180 B libres), pool **311 256 B con 9 248 B de sobra**, RAM
  code **19 632 B de 24 576**. El paquete sigue en 610 959 B con identidad **FM-1_965**.

## v1.3.3 — la escena de FX ya no transpone

### Lo que está arreglado y medido
- **OCT+ guardaba la escena de FX y además subía la octava; OCT- la llamaba de vuelta y además la bajaba.** En la
  página FX esas dos teclas son la escena de la pista (1.3.2), pero el `case B_OCTDN/B_OCTUP` que resuelve la octava
  cuando el botón se suelta seguía actuando: guardar la tira de envíos cambiaba el tono de lo que se iba a tocar, y
  llamarla de vuelta lo cambiaba otra vez. La página FX ahora sale antes de la octava, igual que ya salía con
  `LAYER_FX` (el LATCH del mapa) y con las páginas de acción; **fuera de esa página las dos teclas siguen moviendo la
  octava, arriba y abajo**, que es lo que comprueba el segundo caso.
- **La ranura de escena que la página «usaba» no existía.** `fx_scn_pick` estaba declarada y nadie la escribía ni la
  leía; se va, y el comentario dice lo que hay: cuatro ranuras por pista, la página usa la primera y el selector para
  las otras tres todavía no existe.
- **Comprobado en host, con la regresión que falla sin el arreglo.** `tests/ui_test.c` suma dos casos: «FX page: OCT+
  stores and OCT- calls back, and neither moves the octave» — **falla sin el cambio y pasa con él** — y «off the FX page
  they still shift the octave, up and down». `ui_test` completo en verde (`all passed`) y la suite entera también.
- **`morph_six_test` se revisó entero y está completo**: seis pistas, los buses de FX y el clipper, notas por MIDI,
  una carga de preset encima y el giro de MORPH, con los dos renders idénticos antes del giro. 6 transiciones, 0
  fallos: latencia 0,09–0,27 ms, slew al 90 % en 5,4–7,1 ms y ningún click (paso 4 385 contra 6 714 antes del giro).
  No se tocó.

### Memoria y mediciones (build AC791N de este árbol)
- Imagen 583 320 → **583 352 B** de 583 612 (**260 B** de margen: el arreglo cuesta **32 B**). `.data + .bss`
  95 124 B de 98 304 (3 180 B libres), pool **318 432 B con 9 248 B de sobra**, RAM code **19 632 B de 24 576**,
  LOWBUF 26 724 de 28 672 y NOINIT 15 572 de 15 696. El paquete sigue en 610 959 B con identidad **FM-1_965**.
- **Sin cambios visuales**: los 301 renders dorados salen idénticos y el barrido de las 190 pantallas × 22 paletas no
  encuentra nada.

### Lo que se revisó y no se tocó, con el motivo
- **MIDI Learn es RAM por diseño.** `midi_control.c` lo dice y lo razona: la tabla que un apagado limpia, para no
  cambiar el layout de ajustes, de los presets ni del proyecto. Persistirla pide formato nuevo, no un bug fix.
- **La escena de FX no entra en el proyecto**: el payload está lleno byte a byte (medido en la 1.3.2), y meterla pide
  un formato `FUND`/`FUNC` entero.
- **SLICER con DEPTH y modo por paso, y el A/B de la edición en las páginas de grupos**: el SLICER está implementado y
  verde en host pero el enlace no cierra (**496 B** sin su gráfico contra 260 B libres); el A/B no está empezado.

## v1.3.2 — HOME sin las tarjetas de atrás, los nombres enteros y la escena de FX de cada pista

### Lo que está arreglado y medido
- **Las cuatro tarjetas de la fila de arriba ya no asoman por ninguna pantalla de HOME menos en HOME misma.**
  La supresión está donde corresponde, en el flujo compartido de dibujo: `draw_columns()` sale antes de las
  tarjetas cuando `ui.home && ui.home_view >= HV_OVERVIEW` (fuente única de esa fila), en vez de taparlas con
  un panel encima. `graph_home_view()` abre ahora en `Y_LABEL`, porque el nombre de la pantalla ocupa la banda
  16..20 que antes se dibujaba a ciegas.
- **OVERVIEW, MIXER y PERFORMANCE se leen enteros.** El cuerpo de la vista se dibuja en dos pases de 88
  filas (26 y 114) sobre un pase-cap de 26 filas a `Y_LABEL`: ningún pase corta una fila de texto, que es la
  condición que imponía el lint. Los nombres van en `cv_text(14, 34, &AF_S, …)` con la regla en `y = 41`, y el
  fondo (`hv_grad`, `hv_air`) se recortó al nuevo cuerpo en vez de seguir midiendo desde 0.
- **Números, no impresiones.** `build/ui_render` sobre 190 pantallas × 22 paletas: **0 hallazgos de lint**
  (ningún texto tapado, cortado ni fuera de su canvas), **0 desalineaciones en 3 894 154 medidas** de 74
  posiciones, y `tests/ui_test.c` completo en verde (`all passed`). Los volcados 2× de las tres pantallas
  están en `build/shots/home_overview.png`, `home_mix.png` y `home_perf.png`.

### Memoria y mediciones (build AC791N de este árbol)
- **La entrega completa mide 583 320 B de 583 612 B: 292 B de margen.** La escena de FX por pista (120 B de
  `.bss`, cuatro ranuras por pista) cuesta **456 B** de imagen sobre la 1.3.2 anterior: 582 864 → **583 320 B**, y el
  margen baja de 748 a **292 B**. `.data + .bss` queda en **95 124 B de 98 304 (3 180 B libres)**, pool **311 256 B
  con 9 248 B de sobra**, RAM code **19 632 B de 24 576**, y el paquete sigue en **610 959 B** con identidad
  **FM-1_965** (el contenedor llena un tamaño fijo por slot). Con 292 B libres, el siguiente feature de este tamaño ya
  no entra: por eso el SLICER por paso queda fuera y su coste está medido abajo.
- **La imagen baja 96 B y el margen del slot sube de 684 a 748 B**: el arreglo de HOME (una salida temprana en
  `draw_columns()` en vez de cuatro tarjetas tapadas, y el nombre de pantalla que abre en `Y_LABEL`) dibuja menos
  código y deja la imagen en **582 864 B de 583 612**. `.data+.bss` queda en **95 012 B de 98 304 (3 292 B
  libres)**, pool **311 256 B con 9 248 B de sobra** (el piso de 8 KiB se respeta), RAM code **19 676 B de
  24 576**, LOWBUF **26 724 de 28 672** y NOINIT **15 572 de 15 696**.
- **`tools/ram_report.py` medía mal dos regiones y ahora no puede.** El XIP se calculaba sumando sólo `.text`,
  así que informaba **21 252 B libres** cuando la región la llena la imagen entera (`.text` más las copias de
  carga de `.ram_text`, `.c1_text`, `.hot_text` y `.data`, `AT > XIP` en `app.ld`): los 748 B reales. La RAM se
  calculaba sumando `.data` y `.bss` sin el hueco de alineación entre las dos (36 B), o sea 3 328 B libres donde
  hay 3 292. Las dos filas salen ahora de los mismos mojones que comprueba `tools/build.py` (`_bss_end`,
  `_pool_end`, `_c1_stack_hi`, `_low_end`, `_hot_end` y el tamaño de `build/felucca.bin`), y coinciden al byte
  con el build en las seis regiones.
- **Lo que se midió y se descartó** (detalle en `docs/BUILD_AC791N.md` §8.2): memorizar la máscara de
  portadoras de FM6 costaba **+48 B de XIP**; una caché de «última fuente» delante de `hc_lut()` costaba **+16 B
  de XIP** para ahorrar hasta dos comparaciones por glifo; y `-fdata-sections` da **582 828 B frente a 582 832 B** (medido sobre el árbol previo al arreglo de SELECT, de 582 832 B)
  (−4 B, dentro del relleno de sección). Ninguno de los tres compra algo que el presupuesto pueda ver en el
  recurso que aprieta, que es la imagen, así que no quedaron en el árbol.

### Lo que está arreglado después: el salto de SELECT a ARP en HOME

- **El camino, medido y no supuesto.** Con trazas en `page_jump()`, `page_tap()` y `open_family()`, girar SELECT en
  HOME con el botón ARP apretado terminaba en `page 65 (ARP)` por dos sitios encadenados: `page_jump(fam=8)` con
  `ui.home` puesto —que el propio `page_jump()` lee como «desde otro lado» y resuelve a la primera página de la
  familia, y la primera de ARP es ARP— y después el **tap del botón** al soltarlo, porque el bit armado en el
  escaneo sobrevivía al giro y el bucle de taps leía «bit armado + botón arriba» como una pulsación completa.
- **El arreglo, en los dos sitios.** En HOME el giro de SELECT ya no pagina: consume el botón (`btn_used`) y deja la
  familia a su suerte de siempre —los dados de EDIT, SEQ, LFO y FX, y DICE ALL con HOME apretado—; el paginado desde
  HOME sigue siendo el mismo botón con PRESETS. Y un tap pasa a exigir un *release* de verdad (el botón estaba abajo
  en el pasada anterior y ahora está arriba), no un bit armado por una arista repetida del escaneo.
- **Comprobado.** `tests/ui_test.c` con una regresión nueva —«SELECT on HOME turns its knob: no page, and ARP's button
  under it takes none»— que **falla sin el cambio y pasa con él**; la suite entera en verde (`all passed`, incluida la
  de 0.6.67 con el paso de ENV por PRESETS, que es donde viven las páginas desde HOME) y el barrido del render con
  **0 hallazgos de lint** y **0 desalineaciones en 3 894 154 medidas**.

### La escena de FX de cada pista


- **La escena de FX, una por pista: los cuatro envíos de la página FX (DIST, CHOR, DLY, REV) y el tipo de DIST.** En la
  página FX, **OCT+ guarda** la tira y **OCT- la llama de vuelta** (con UNDO, que devuelve la música de antes); una escena
  que la pista no guardó dice `FX SCENE EMPTY` y no escribe nada, y OCT+ otra vez la escribe encima. Son las dos teclas
  que esa página tiene libres: sus perillas son los envíos, PRESETS el sonido, SELECT las páginas, y los botones sus
  secciones y sus capas. Ocupa 120 B de `.bss` (cuatro ranuras por pista; la página usa la primera).
- **Un bug viejo que salió en el camino**: `N_DTYPE` es la tabla de nombres, no una cantidad — `N_DTYPE - 1` era
  aritmética de punteros y guardaba basura en el tipo de DIST. El rango sale ahora de `TP[P_DTYPE]`, como el de cualquier
  otro parámetro.
- **Comprobado en host**: `tests/ui_test.c` suma `test_fx_scenes` — guarda, llama, escribe encima, una escena por pista y
  la llamada vacía que no toca nada — en verde en tres corridas seguidas (`all passed`), con el barrido del render en
  **0 hallazgos de lint** y **0 desalineaciones en 3 894 154 medidas**.

- **No está guardada en el proyecto.** El payload del proyecto está lleno, medido: las pistas terminan en 2880, la cadena
  ocupa 36 y el motion 260 (3176) y el bloque del punch-in empieza en `PROJ_PFX_OFF` 3168; después vienen el kit (3216),
  los FM6 (3312), el nombre (3824) y el hash (3836) en un store de 3840 bytes con el hash pegado al final. No hay un solo
  byte libre: 120 B por proyecto piden formato nuevo (magia `FUND`, payload más grande, lectura de `FUNC` al lado), y eso
  es una migración que hay que hacer entera y con su paquete delante, no de paso.

### Lo que sigue sin hacer
- **DEPTH y modo por paso en el SLICER: implementado y verde en host, fuera de la imagen.** El modelo, el DSP, la
  página FX > SL STEP y el bloque `'S' 'P'` del proyecto están escritos y probados (`tests/ui_test.c`,
  `tests/project_test.c`), pero el enlace del AC791N **no cierra**: 816 B por encima del slot con el gráfico de la
  página y 496 B sin él, contra **292 B** libres. Medido por símbolos: `graph_slstep` 310 B, `proj_unpack_c` +120,
  `page_desc` +92, `proj_pack` +84, `graph_signature` +84, `project_capture` +64, `project_restore_runtime` +48,
  `slicer_on` +34, `sl_enter` +32, `slicer_bus` +28, `system_init` +20 y ~88 B de cadenas. Detrás del slot sólo
  quedan **725 B** antes de la partición siguiente, así que el feature espera a que se pague esa flash. Vive en la
  rama `sl-step`.
- **A/B de la edición en las páginas de grupos: no empezada.**

## v1.3.1.1 — el empaquetado de `step_t` no entra: medido y descartado

### Lo que se probó
- **`step_t` de 17 a 15 B** (n/flags 3+5, time/comp 2+5, vel/probability/ctime 7 bits, `cwhen` entero porque su
  nibble alto es el micro-timing del paso). La estructura compila, pasa la suite entera de host
  (`ALL HOST TESTS PASSED`, exit 0) y devuelve **1 024 B de `.bss`** (`trk` 10 560 → 9 792 y `gen_keep`
  2 176 → 1 920).
- **Y no entra en la imagen.** El enlace del AC791N lo rechaza:
  `section .hot_text will not fit in region XIP`, `region XIP overflowed by 3776 bytes`. La misma árbol sin el
  cambio vuelve a enlazar en su tamaño de siempre (imagen 582 928 B, 684 B libres), así que el empaquetado
  cuesta ~4,4 KB de código: cada escritura a un bit-field es leer el byte, enmascarar, o, escribir, y hay
  cientos de sitios de escritura repartidos por `seq.c`, `ui_*.c` y `project.c`.
- **Cambiar 1 KB de RAM por 4,4 KB de flash en un build con 684 B libres es un mal negocio**, así que el
  cambio queda revertido y anotado en `docs/BUILD_AC791N.md` §8.2. La RAM no es el recurso escaso: quedan
  3 292 B de `.bss` y 9 248 B de pool. El recurso escaso es la imagen.

### Lo que sigue: liberar flash, no RAM
- `fm6_note` (7 104 B), `hc_tab` (1 548 B) y los índices de 12 bits son RAM, y la RAM tiene margen. Lo que
  aprieta es el slot: **908 B libres** con la imagen de 1.3.1, y los iconos (7 747 B) y las fuentes
  (27 216 B) ya viven en XIP como `const`. El siguiente paso rentable es deduplicar iconos repetidos, no
  mover más estado fuera de `.bss`.
## v1.3.1 — la página FX, legible; 1,5 KB de RAM devueltos

### La página FX
- **CHORUS y REVERB ya no tocan el medidor.** Son seis glifos de AF_S y su línea llegaba a cx + 39, justo
  sobre la columna del medidor (cx + 35): la barra cortaba las dos últimas letras. El nombre sube una fila
  (tinta 8..16) y el medidor abre en la fila 22, con seis filas de aire; sigue siendo de 16 × 43.
- **Los tres indicadores de abajo se leen en tiempo real.** EQ es bipolar (el centro es 0 dB, el relleno va
  del centro a la posición de la ganancia y la cabeza marca el punto; BYP se ve plano); DEPTH tiene escala
  de 25/50/75 % sobre el relleno y una cabeza que marca la compuerta; SLICER enciende los 16 pasos del
  patrón que va a compuerta (`sl_pattern()`) y el paso que suena se para un píxel más alto, en acento. Las
  tres barras siguen la ley de los medidores (`fx_ease`: ataque inmediato, un cuarto por frame), así que un
  encoder girado rápido no rompe el relleno y un valor que se detiene se asienta en dos frames.
- **Comprobado con el barrido de renders (host x86)**: 190 pantallas × 22 paletas, 0 hallazgos de lint, 0
  tintas sobre su propio color, 0 píxeles fuera de gris en GREY/MONO, 0 desalineaciones de 3 894 154 medidas.

### Memoria
- **1 536 B de `.bss` devueltos**: el anillo de salida de la consola USB era de 2 048 B para una cola de
  texto cuyo estallido es una línea de diagnóstico; 512 B aguantan seis. El anillo de entrada queda en 256,
  que es lo que el host manda en una ráfaga.
- **La unión de búferes de trabajo que propuse para esta ronda no es segura y no la hice**: `gen_keep` es el
  UNDO de GEN y sobrevive entre frames, `upf` es el índice del banco de FM6 que se guarda en flash y
  `st_buf` se devuelve a quien lo pide (editor_backup) como cuerpo del objeto: sus vidas se solapan. Queda
  anotado en `docs/BUILD_AC791N.md` §8.2 junto con el plan para `WT_GEN` (12 288 B = 3 niveles × 8 frames ×
  256 puntos × 2 B), que sí es RAM de verdad pero cambia el timbre y necesita una prueba de escucha.

## v1.3.0 — la página FX con el panel entero

La primera página de FX usaba 98 de las 122 filas del panel: los cuatro cuadros medían 62 px y los tres
módulos de abajo se apiñaban en una franja de 28, mientras las filas 99..121 quedaban vacías. Ahora el
panel llega hasta el final y todo lo que hay dentro es más grande.

### Lo que cambió
- **Los cuatro cuadros de envío: 54 × 75** (eran 53 × 62). El nombre va arriba, el valor debajo, el
  icono bajo el valor y la etiqueta del tipo al pie, así que el medidor se queda la columna derecha
  entera en vez de compartirla con el texto.
- **Los medidores están dentro de su cuadro y son más grandes**: 16 × 49 px de pista con la barra de
  plasma de 12 px (eran 10 × 42 dentro de una pista de 12). Los cuatro arrancaban en `cx + 43` sobre un
  cuadro de 53 px, o sea que colgaban 2 px por fuera del borde: eso era lo que se veía "corrido". Ahora
  `mx = cx + 35` y `35 + 16 + 3 = 54`, el ancho del cuadro, en los cuatro y con el mismo margen.
- **Los tres módulos de abajo (EQ / DEPTH / SLICER): 76 × 39** (eran 72 × 28) y bajaron a las filas
  82..120. La barra pasó de 26 × 3 px a la mitad derecha de una línea a **68 × 9 px** bajo el valor, que
  es lo que hace legible de un vistazo la posición del EQ, el recorrido de DEPTH y los 16 pasos del
  SLICER. DEPTH se dibuja como un relleno continuo (es una compuerta 0..100 %) y el SLICER conserva sus
  16 pasos, que ahí sí son el tema.
- **Verificado en el barrido de renders (host x86)**: 190 pantallas × 22 paletas, **0 hallazgos de
  lint**, 0 tintas sobre su propio color, 0 píxeles fuera de gris en GREY/MONO, y **0 desalineaciones
  de 3 894 154 medidas**. Ninguna otra página se movió.

## v1.2.7 — el paquete, el sitio, el instalador y el emulador al día

Lo que la v1.2.6 dejó como build de trabajo ahora es un release completo: un paquete con su manifiesto,
las capturas regeneradas desde ese árbol, el portal, el instalador y el emulador sirviendo la misma
versión, y `tools/coherence_check.py` en verde.

### El paquete
- **`release/felucca-v1.2.7.fwsc`**, 610 959 B, con `felucca-v1.2.7.fwsc.manifest.json` y
  `release/build-manifest-v1.2.7.json` (commit, árbol, hashes de la imagen, del cargador y de los tres
  ficheros del SDK, toolchain y emulador).
- **Medido en el build AC791N**: imagen **582 720 B de 583 612** (892 B libres), `.data+.bss`
  **96 548 B de 98 304** (1 756 B libres), XIP 422 168 B de código + 140 080 B de constantes, RAM code
  19 676 B de 24 576, *pool* 311 256 B con 9 248 B de sobra, cargador `ota.bin` 7 803 B. Identidad
  `FM-1_965`.

### Lo que trae el release
- **El motor ADDITIVE (ADDTV)**, diez motores en total, con sus ocho presets y su página EDIT; medido en
  la imagen: **1 832 B de XIP y 1 440 B de `.bss`** (`tools/engine_size.py`, `docs/BUILD_AC791N.md` §8).
- **SOUND EDITED**: la estrella del pie, la de OVERVIEW y el punto del navegador de PRESETS.
- **Las cuatro tarjetas y las gráficas**: etiquetas en negrita, degradado en todos los cuadros de la
  primera página de FX y el relleno del biquad EQ con el degradado del gráfico principal.
- **281 sonidos de fábrica** (ADDTV suma ocho) y **diez motores** en el editor del Studio, en el emulador
  y en la librería.

### Las otras piezas
- **Capturas**: `assets/screenshots/` y el arte del portal regenerados desde este árbol con
  `tools/generate_screenshots.py`, que lee la versión del propio firmware.
- **Portal**: `site/index.html` anuncia 1.2.7 (diez motores, 281 sonidos) y `site/firmware/versions.json`
  lleva la entrada 1.2.7 con el hash y el tamaño del paquete; el emulador de la portada es el de este
  build.
- **Instalador**: `site/webapp/installer/` ofrece 1.2.7 y verifica el SHA-256 y el tamaño de ese paquete
  antes de habilitar INSTALL.
- **Emulador**: `release/emu/` y `site/emulator/` con el WASM compilado desde este árbol, su commit y su
  SHA-256, y la misma versión que el firmware.

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
- **Coste en la imagen, medido en el build AC791N**: **1 832 B de XIP y 1 440 B de `.bss`** (10 símbolos;
  `ad_v` 1 224 B, `add_render` 810 B, `add_block` 310 B, `AD_PRESETS` 288 B, `ENG_ADDITIVE` 232 B,
  `AD_PROF` 64 B). El build cierra con la imagen en 582 336 B de 583 612 B y `.data+.bss` en 96 548 B de
  98 304 B (1 756 B libres). Tabla por motor y método: `docs/BUILD_AC791N.md` §8.

### SOUND EDITED (el indicador de parámetros modificados)
- **Una estrella junto al nombre del sonido** en el pie de todas las páginas y en HOME > OVERVIEW, y un punto
  en la fila del sonido cargado del navegador de PRESETS: el sonido cambió desde que se cargó o se guardó.
- **Se calcula comparando**, no con un hook en cada setter: un hash FNV-1a del sonido (P_COUNT + la matriz +
  motor y preset) contra la huella que dejó la última carga o el último guardado, así que cubre perillas, la
  capa rápida, el editor, A/B y el recall de una sección sin que ninguno de ellos lo sepa. Se evalúa una vez
  por cuadro y **nunca con el transporte andando** (una automatización o un lock escribe p[] para su paso y lo
  devuelve: no es una edición del sonido). Un proyecto cargado —y una sección de la canción, y un UNDO, que
  pasan por el mismo `project_restore_runtime`— también es la referencia: sus pistas no son ediciones de
  nadie. No se guarda: es estado de la interfaz.

### Limpieza de esta misma entrega
- **El decaimiento por parcial no ocupa memoria**: es `DECAY · k`, así que se calcula en el lazo del bloque y
  `ad_trk_t` baja de 68 a 36 B (**−192 B de `.bss`**, todas las pruebas y todos los renders iguales).
- **El gráfico pide las ganancias** con `ad_partial()` en vez de leer `ad_t` y repetir el índice por parte; el
  motor sigue siendo el dueño de su estado.
- **Casos que no podían darse**: `if (d < 0)` sobre un `uint32_t` (el decaimiento nunca puede envolver) y el
  `% NPART` de un índice que ya está en 0..5 (NTRK == NPART: era una división por seis por bloque).
- **Una sola forma de escribir la estrella**: OVERVIEW la pega al nombre como el pie, en vez de medir el texto
  y dibujarla aparte.
- **La prueba, más corta**: `E_FULL` era idéntico a `E_SAW`, `(void)f` tapaba una constante que nadie usaba y
  trece pares `voice_render` + `spectrum` son ahora `spec_of()`.
- **Un arreglo que salió de la revisión**: cargar un proyecto (o recordar una sección, o un UNDO) dejaba las
  cuatro pistas marcadas como editadas —esas pistas las escribió el firmware, no el usuario—. Ahora
  `project_restore_runtime` es la referencia, con su caso en `tests/ui_test.c` (que falla si se quita).

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

### Las cuatro tarjetas y las gráficas (pulido pedido por el dueño)
- **Las letras de las cuatro tarjetas de knob, en negrita.** La etiqueta de cada tarjeta era la tinta más fina de su
  propio cuadro: 12 px de una cara ligera al lado de un valor en la de 15 px. Ahora se dibuja con la curva de
  cobertura `CURVE_BOLD` (`firmware/src/gfx.c`, `cv_text_fit_bold`): los mismos glifos, el mismo ancho, la misma
  línea de base y el mismo recorte con puntos suspensivos, con los trazos más sólidos. No hay una segunda fuente en
  flash —la imagen cierra con 1 276 B de margen— y la caja de tinta que mide el lint es la misma, así que el
  barrido sigue en 0 hallazgos.
- **Todos los cuadros de la primera página de FX llevan degradado, incluidos los de abajo.** Los cuatro módulos de
  la fila inferior (EQ / DEPTH / SLICER) y el panel de los cuatro sends eran placas planas porque el estilo
  achaflanado sólo pintaba su rampa en cajas de 36 px de alto o más (`cv_rrect`, `gfx.c`): el umbral baja a 24 px
  (`PLATE_MIN_H`), que es lo que mide el módulo típico, y la página pinta además el degradado del panel, de cada
  send y de cada módulo con `ux_ground_rows` — el mismo que usan las vistas de HOME y FX > BUSES — y dibuja sus
  textos sobre el color real de su fila (`ux_ground_at`), no sobre un `T_SURF` que no está debajo.
- **El degradado de la gráfica del biquad EQ es el de la pantalla principal.** El relleno bajo la curva eran seis
  franjas escalonadas mezcladas desde `T_BG`, reiniciadas bajo cada columna: textura, no luz. Ahora es la ley de
  `graph_ground_ramp` / `hv_grad` (panel 22..62 %, acento 10..45 %), resuelta por fila absoluta, y el cuerpo del
  filtro (lift y cut) se rellena en tres pasos que siguen la altura de la fila en vez de un bloque plano al 40 %.
- Barrido de `tests/ui_render`: **0 hallazgos de lint, 0 desalineaciones de 3 894 154 medidas**, 0 píxeles
  GREY / MONO fuera de gris o neutro — medido en host, sobre el renderizador real, no en la unidad.

### Cómo se mide (y cómo se instala el toolchain)
- **`tools/get_ac791n.py`**: el toolchain pi32v2 de JieLi y los tres blobs del SDK en un solo paso, con los
  SHA-256 fijados en el repositorio (`tools/toolchain.sha256`, `SDK_SHA256` en `tools/build.py`); no escribe
  nada que no coincida. `--check` informa sin escribir.
- **`tools/engine_size.py`**: XIP y `.data+.bss` **por motor**, desde `build/felucca.syms` (la tabla de
  símbolos) con `--map` como verificación cruzada contra el mapa del enlazador — 88 funciones, 0 discrepancias.
  `./build.sh` lo ejecuta con `--strict` al final: si un motor o la imagen no entran, el build falla en vez de
  dejar el número para después. `tests/engine_size_test.py` lo cubre en host, sin toolchain.

### Bloqueado
- **El emulador WASM**: no hay `emcc` en este entorno, así que `release/emu` sigue en 1.1.0 y la release
  pública sigue parada por la regla de que todos los componentes sirven la misma versión.
- **La unidad FM-1**: no hay aparato. El build AC791N corre y empaqueta (`build/felucca.fwsc`, identidad
  `FM-1_965`, manifiesto v1.2.6), y es un build de trabajo: no se publica como release.

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

