## 2026-11-13 — v2.9.1: auditoría de motores, seguridad y cierre

- **Corregido**: un proyecto que se importa con la ventana de voces de una pista a 0 hacía `% 0` en
  `voice_alloc` (y devolvía un puntero que el llamador desreferenciaba). Lo encontró `tests/fuzz_proj.c`
  bajo ASan/UBSan, con el seed 7; esa semilla queda en la suite como regresión. Cuesta 16 B de flash:
  imagen 580 404 B, 1 160 B de margen.
- **Añadido**: `tests/engine_param_test.c` — 216 mandos EDIT barridos de mínimo a máximo en tres presets
  por motor, comparados muestra a muestra: ninguno sin efecto. Su modo `--levels` mide el RMS de los 267
  presets; la familia NOISE queda 12 dB por debajo de ANALOG y ese es el desequilibrio que hay que arreglar.
- **Documentación**: fases 5 y 6 (`docs/QG_EVOLUTION_PHASE_5_ENGINES.md`, `docs/QG_PRESET_REMASTER.md`,
  `docs/QG_EVOLUTION_PHASE_6_SECURITY.md`), re-verificación del Hub y este cierre final.
- **Sin motores nuevos**: con el margen que hay (1 160 B tras recuperar 956 del código de pantallas), GRANULAR (buffer de 22 051 B frente a 15 392 libres), SAMPLE
  y VOCODER no caben. SID, ACID y POLY se resuelven con presets sobre ANALOG/PHASE/WTABLE.
- **README**: decía `v2.7.4.7` y 212 sonidos de fábrica; ahora dice `v2.9.1` y los 267 que cuenta la
  auditoría.
- **Niveles de la biblioteca**: 52 presets llevan ahora su `P_LEVEL` en el `x[]` que ya tenían (0 B de
  flash). La familia NOISE pasa de **−29,7 a −21,8 dBFS** de mediana y sus 22 presets dejan de caer 12 dB
  por debajo del resto; los 30 que estaban a ≥6 dB de su propio motor quedan en su familia. Techo: pico
  ≤ −2 dBFS y nivel ≤ 127, así que los sonidos de eventos espaciados (GEIGER CNT, RISER, SOLAR WIND) siguen
  bajos por diseño, no por nivel.
- **Slot de la aplicación**: `APP_SLOT` pasa de `0x8DFBC` a `0x8E7BC` (+2 048 B) usando el hueco que la
  imagen ya tenía y nadie direccionaba (2 774 B reales, no los 3 493 que decía la documentación). La
  imagen de flash y las particiones BTIF/USR no cambian. Margen de código: **3 208 B**.
- **Margen de XIP**: `tools/size_fns.py` pide ahora `noinline` además de `minsize` a las funciones del
  bucle principal, que hasta aquí se copiaban dentro de un único despachador (`draw_graph` medía 39 812 B).
  Imagen 581 360 → **580 404 B**: quedan **1 160 B** en vez de 204. El audio no cambia —la pasada excluye
  las 141 funciones que alcanza una interrupción— y `target_budget` da los mismos costes.

## 2026-11-13 — Banda galáctica en el visualizador

- `gw_viz_deep()` dibuja una banda galáctica de 24 columnas antes de los cuerpos: núcleo con bulbo, halo y polvo,
  en el tono de la página para que los datos medidos sigan liderando. Sin trigonometría por píxel, sin memoria.
  No se dibuja en calidad LOW.

## 2026-11-13 — FX > BUSES: distribución rehecha

- La pantalla tenía 318 hallazgos de lint por textos fuera de su celda: los títulos de los seis bloques se
  dibujaban por encima de su rectángulo interior y los nombres completos de la cadena se pisaban en ocho celdas
  de 28 px. Ahora la cadena es una fila de tres letras, cada bloque es un panel con título y dibujo dentro de su
  propio suelo, y el master queda siempre enmarcado como salida común. Cada efecto conserva su dibujo propio,
  animado sólo con señal real (`fx_vu`).
- Se revirtió el gráfico por motor de EDIT 1 (`19b5414`): el barrido exige 32 gráficos FM6 y esa página dejó de
  dibujarlos, así que la comprobación bajó a 0. `SEQ > TOOLS` de ese commit se conserva.

## 2026-11-13 — SEQ > TOOLS y un gráfico por motor

- **SEQ > TOOLS** (nueva página de SEQ, tras MUTATE): DUP, HALF, ROT <, ROT >, REV y CLEAR sobre el patrón de la
  pista elegida, con TIMES 1..8, punto de UNDO antes de cada edición y el patrón dibujado en la propia página.
  Sólo se reescriben las notas: tiempo, compuerta y probabilidad de cada paso se conservan.
- **EDIT 1** tiene ya gráfico: uno por motor, con los parámetros de ese motor (osciladores y filtro, operadores,
  pistas de batería, granos, cortes, onda del sample, morph de la tabla, modos de la cuerda y escalones de LOFI).

## 2026-11-12 — SCL en tres páginas y FX > BUSES

SCL se rehace como herramienta de tres páginas y FX gana la consola de ruteo. El detalle medido está en
`docs/QG_SCL_FX_REDESIGN.md`.

- **SCL 1/3 SCALE**: las doce notas en un círculo (tónica arriba, grados de la escala unidos en el color de la
  pista, notas de afuera tenues, un radio por grado) y debajo el teclado con la escala encendida y cada nota que
  suena marcada. Columna derecha: tonalidad, familia, cuántas notas, QUANT y el acorde que tocaría CHORD.
- **SCL 2/3 CHORD**: el acorde de la tecla con su símbolo, sus notas, el voicing y el strum en ms, los grados como
  pastillas (el que pisa el acorde en el acento) y las voces en cuatro carriles sobre la extensión del acorde, que
  se corren con STRUM. Con CHORD en OFF lo dice y no dibuja voces.
- **SCL 3/3 PROG**: las ocho progresiones como pastillas y el acorde de cada paso en esta tonalidad. Las viejas
  páginas SCL, VOICING, INTERVAL y KEYS desaparecen porque su contenido es ahora el círculo, el teclado y la página
  CHORD; no se perdió ningún parámetro. `SCL 2` (transposición global de las secuencias) pasa a GLO.
- **FX > BUSES**: consola de ruteo con KNOB 1 seleccionando TRACK/BUSES/DLY/REV/CHO/SLI/PUNCH/MASTER y KNOB 2..4
  editando los parámetros reales de ese eslabón (los mismos ids de las otras páginas). Seis bloques con dibujo
  propio y datos reales: repeticiones del delay, cola de la reverb, ondas del chorus, compuertas del slicer,
  transiente y duck del punch, ganancia/pico/clip del master. Sin DSP nuevo: son los tres buses globales, el
  slicer y el master que ya corren.
- **Pruebas**: `ui_render` 189 pantallas × 22 paletas con **0 hallazgos de lint** y 0 de alineación; `ui_test`
  verde con la navegación nueva; capturas actualizadas en `.shots/{scl,chord,prog,buses}.png`.

## 2026-11-12 — Remasterización de la biblioteca: 212 → 267 presets

Los sonidos de fábrica, rehechos uno a uno para que usen lo que el firmware ya tiene. El detalle medido
está en `docs/QG_PRESET_REMASTER.md`; aquí, lo que cambia al tocar el FM-1.

- **Movimiento en 210 de los 212 sonidos existentes** (antes, 40): LFO y matriz de modulación al filtro,
  a la amplitud, al tono y al envío de reverb, con `VEL → CUT` o `AT → VIB` según la familia. Los dos kits
  `legacy` de DRUM (ids 10/11) siguen byte a byte, para que un proyecto viejo cargue el mismo sonido.
- **Los envíos dejan de ser un wash común**: la reverb baja en bajos, percusión y leads y sube en pads,
  coros y campanas; el chorus se queda donde el sonido lo pide, el delay donde hay ataque que repetir y la
  saturación donde el motor tiene drive (`SOFT`, `TAPE`, `HEARTH`, `FLUX`). Nueve sonidos van sin envíos.
- **55 sonidos nuevos**, originales y repartidos por familia (leads, pads y drones, campanas y teclas,
  experimentales, bajos, percusión y una familia de rock electrónico / electrónica atmosférica de nombres
  propios, sin copiar presets ni muestras). Todos los motores reciben: ANALOG 56→68, FM6 29→37, PHASE
  25→32, LOFI 25→29, VOICE 20→25, WHEEL 18→22, NOISE 17→22, DRUM 12→18, WTABLE 10→14.
- **Niveles recalculados**: la dispersión de RMS entre presets pasa de 31,5 dB a 11,7 dB con el pico más
  alto en −2,8 dBFS (ningún preset recorta). 26 sonidos siguen por debajo de lo que el fader alcanza y se
  listan como pendiente.
- **Herramientas**: `tools/preset_audit.py` audita la biblioteca como datos (movimiento, envíos,
  categorías, parecidos) y `tests/preset_audit_test --dump` publica las tablas del firmware en JSON, así
  que la auditoría no puede desviarse del código. El navegador de presets gana el orden de LOFI para los
  cuatro sonidos nuevos y el generador de categorías filtra los kits `legacy` por id, no por posición.
- **Verificación**: host x86 con el motor real — 267 presets, 0 silenciosos, 0 duplicados, 0 fuera de
  rango; `preset_library_test` y `preset_categories_test` en verde con los conteos nuevos. Nadie ha
  escuchado estos sonidos y no hubo ninguna prueba en la unidad física.

## 2026-11-12 — v2.9.0: las cinco fases, en un solo paquete

La versión que este repositorio, su web, su emulador, su instalador y su release sirven ahora. Reagrupa
el trabajo de las cinco fases; cada una tiene su informe con la clase de cada medición.

- **Fase 1 — optimización medida** (`docs/QG_PHASE_1_OPTIMIZATION.md`): memoria, `.ram_text` y el coste
  de los caminos calientes, con las cifras de host y las que quedan fuera de alcance.
- **Fase 2 — la interfaz** (`docs/QG_PHASE_2_UI.md`): el rediseño de HOME y sus vistas, medido en el
  render de host y en el emulador compilado desde este árbol.
- **Fase 3 — motores** (`docs/QG_PHASE_3_ENGINES.md`): WAVETABLE entra como motor propio y la auditoría
  de presets deja los nueve vivos.
- **Fase 4 — herramientas musicales** (`docs/QG_PHASE_4_MUSICAL_TOOLS.md`): FLANGER como tercer
  `CHORUS > TYPE`, **HOME 9/9** con la pantalla ROUTING y una sola balística de medidores.
- **Fase 5 — The Firmware Hub** (`docs/FM1_HUB_READINESS.md`): la tarjeta `fmhub.json` validada por
  `tools/check_fmhub.py`, la identidad leída del paquete publicado con
  `tools/check_package_identity.py`, el job `verify` de atestación en cada tag y la procedencia de cada
  audio y binario en `docs/FM1_HUB_AUDIO_PROVENANCE.md`.
- **La sincronización de esta entrega**: el portal deja de nombrar v2.7.2 y toma la versión del
  despliegue (v2.9.0), dice **nueve** motores con WAVETABLE en la lista, **22** paletas y ya no
  promete un conteo de presets que no puede verificar; la advertencia de instalación explica que los
  manifiestos van sin firma Ed25519 desde 2.8.0 (integridad y trazabilidad, no autenticidad); y las
  capturas —incluidas ROUTING, la matriz de modulación y el navegador de presets— se regeneraron desde
  el código de esta versión con `tools/generate_screenshots.py` y `tools/fw_shots.py`.

## 2026-11-12 — FASE 5/5: preparación para The Firmware Hub

- **La tarjeta del Hub, verificable**. `fmhub.json` en la raíz con un `what` de **274 caracteres** que
  describe lo que el firmware **hace** (nueve motores, secuenciador de 64 pasos con probabilidad y
  ratchets, song mode, escalas y acordes, matriz de mod y tres buses de FX, emulador y editor) en lugar del
  texto anterior que prometía «a sampler with mic input»: SAMPLE, GRAIN y SLICE están **retirados**
  (`ENG_GONE`) y no hay entrada de micrófono. Color **`#35CFFF`** —único entre los 52 listados con color del
  catálogo— con **11,5:1** sobre negro y tinta oscura `#101014` a **10,4:1**; imagen 240 × 240 PNG de
  14 630 B. `tools/check_fmhub.py` valida el archivo contra las reglas publicadas (campos aceptados,
  longitudes, HTML, kinds, enlaces por HTTP, cabecera y tamaño de la imagen, contraste) y ahora corre en CI.
- **La identidad, leída del paquete publicado**. `tools/check_package_identity.py` saca `FM-1_965` de los
  bytes del asset de `v2.8.4` (610 959 B, sha256 `37f7276e…`), encuentra el loader `ota-FM-1_965` y
  **rechaza `FM-1_015`**; `FM-1_965` sigue **libre** en el catálogo del Hub (198 listados, 2026-11-12).
- **Atestación verificada, no supuesta**. `.github/workflows/release.yml` gana un job `verify` que, en cada
  tag `v*`, descarga el asset del release y corre `gh attestation verify` contra el workflow compartido
  antes de dar el tag por bueno; nunca reconstruye ni reemplaza un paquete ya atestado. El pin se documentó:
  `@85a003ab…` **es** el tag `v1` revisado (`git ls-remote`, 2026-11-12). El job no pudo ejecutarse acá
  (§ limitaciones de `docs/FM1_HUB_READINESS.md`).
- **Procedencia completa, con una prueba de reproducibilidad**: `web/banks/gw1-keys.syx` (4 104 B, 32 voces
  con las 11 claves FM6 propias) **se regenera byte por byte** con `tools/gen_fm6_patches.py --syx`
  (`7c39bf4b…`), y ahora CI lo comprueba con `cmp`. No hay ningún `.wav`, `.mp3`, `.ogg` ni `.flac`
  distribuido: todo el sonido es síntesis en tiempo de ejecución. `docs/FM1_HUB_AUDIO_PROVENANCE.md` lista
  cada binario publicado con autor, licencia y uso.
- **Lo que queda pendiente, dicho como pendiente**: las cinco pruebas que exigen una unidad física
  (instalación desde V15, desde otro firmware, salida hacia otro, identidad USB e interrupción/reanudación)
  **no se hicieron** —el FM-1 del dueño no se escribe— y `gh attestation verify` no pudo correr en este
  entorno. `docs/FM1_HUB_READINESS.md` da el estado de cada requisito con su clase de evidencia y el mensaje
  en inglés para Sven, incluida la aclaración de que *Pocket GammaWave* (`FM-1_981`, otro autor) es el
  proyecto upstream y no éste.

## 2026-11-12 — FASE 4/5: FX, ruteo, secuenciador y herramientas musicales

- **FLANGER, el tercer `CHORUS > TYPE`**. Usa la línea del chorus con un retardo corto barrido (48..302
  muestras, 1,1..6,8 ms según DEPTH) y **realimentación 0,6 dentro de la línea**: los notches del peine en
  múltiplos impares de `1/(2d)` son el «jet». **0 B de RAM nueva** y **0 B de flash**: el tap y el LFO ya
  estaban, y cuesta **dos productos por muestra**, menos que los tres taps del ensemble. Medido en host x86:
  peine de **12,0 dB**, pico **7 033** (Q15) —estable—, los notches se mueven con el barrido. En el proyecto
  el `TYPE` pasa a **dos bits** (bits 1..2 del bloque `DT`): un proyecto viejo se lee igual y un firmware
  anterior lee `FLANGER` como `PHASER`, nunca como silencio.
- **Una sola balística de medidores**. La página FX traía su propia caída (1 px por frame) mientras
  `fx_meter_step()` —ataque inmediato, un cuarto por frame, 30 frames de retención— sólo existía para su
  prueba. Ahora la usa también la página FX: **una** ley para todas las barras y **un** origen por medidor
  (`fx_vu[0..5]` para los buses, `fx_vu[6..7]` y `trk[].peak` como en HOME). No quedó medidor duplicado.
- **ROUTING, la novena pantalla de HOME** (`HV_ROUT`, el pie cuenta `/9` en vez de llevar `/8` escrito a mano).
  Dibuja el recorrido real —PARTE → INSERT (`DIST`) → los tres envíos → CHORUS / DELAY / REVERB → MASTER (EQ,
  W, LIM) → OUT— con las cifras que el DSP lee: cada bus muestra **lo que se le envió** (`fx_vu[0..2]`) y **lo
  que devolvió** (`fx_vu[3..5]`), y el pie, los dos picos del master. Una barra oscura es que no pasó audio.
- **El ruteo, medido**: `tests/fx_probe.c` alimenta sólo el bus de chorus y llama a `fx_buses()` 40 bloques:
  `in 11999 / 0 / 0 → out 9060 / 0 / 0`. El envío alimentado levanta su medidor y ningún otro, su bus devuelve
  audio, y un bus sin envíos no devuelve nada.
- **Evaluación de los otros quince efectos** (`docs/QG_PHASE_4_MUSICAL_TOOLS.md` §2.3): once ya existían con
  otro nombre (tremolo y vibrato son destinos del LFO; compresor y limitador, el master; shimmer, `G_RTYPE`;
  tape y overdrive, `DIST > TYPE`), uno se agregó, y los que piden memoria que acá no se puede medir
  (frequency shifter, reverse delay) quedaron **descartados con su razón**, no a medias.
- **Auditoría del secuenciador** (§4): probabilidad, condiciones `WHEN A/B`, ratchets, `MUTATE`, automatización
  y `AUTO LIST`, escenas por fila de canción, macros y punch-in ya están, cada uno con la prueba que lo cubre.
  Las tres carencias reales —fills a demanda, recall de escena desde SEQ, punch-in a mano desde las teclas
  negras— quedan planteadas con su costo: las tres cambian el mapeo de teclas o el formato del proyecto.
- **Herramientas musicales** (§5): escalas, `CHORD`, `VOICING`, `INTERVAL`, `PROG` y `KEYS` ya cubren lo que
  el pedido enumera; la encuesta de JIANT, X0X, Optimist, Melodee y Jangada (todas GPL-3.0) deja dicho qué se
  tomó, qué ya estaba y qué se descartó (los 70 scales, el círculo de quintas, el sampler).
- **Pruebas**: `tests/fx_probe.c` suma el peine del flanger y el ruteo; `tests/fx_bench.c` suma una tercera
  escena con el chorus en FLANGER para medirlo contra el ensemble en la misma corrida; `tests/ui_render.c`
  renderiza la pantalla nueva (**192 pantallas × 22 paletas, 0 hallazgos de lint, 0 desviaciones en 3 164 009
  medidas de alineación**); `tests/ui_test.c` recorre las nueve pantallas de HOME y comprueba que ROUTING no
  toca página, parámetro ni sonido. Sin placa ni toolchain AC791N: nada de esto es `[TARGET]`.

## 2026-11-12 — FASE 3/5: motores, auditoría y un motor nuevo (WTABLE)

- **WTABLE, el motor 14**: un oscilador de tabla de una sola ciclo con morphing. Ocho fotogramas (SINE,
  TRIANGLE, SAW, SQUARE, PULSE 25 %, HOLLOW, METAL, VOCAL) de 256 puntos, normalizados al mismo pico y con
  fase 0 para que el morph no salte ni meta continua, guardados en tres límites de banda (64 / 16 / 5
  armónicos) que el motor elige por la nota: nunca lee por encima de ~11 kHz, así que el teclado entero queda
  sin alias. **MORPH** es el primer destino de motor de la matriz (`MD_E1`), **HARM** inclina el espectro de
  forma continua (un polo a ~1 kHz, ±0,75), y hay **SUB** (octava abajo), **UNI 2 VOICES** con **DETUNE**,
  filtro SVF de 2 polos con seguimiento de tecla y **DRIVE**.
- **Un defecto corregido antes de adoptarlo**: el saturador con SUB dejaba hasta **41,5 % de la escala de una
  voz en continua** (10 voces sumaban). Ahora la voz termina en un bloqueador de continua a ~5 Hz con el
  producto redondeado (con el truncado el bucle se sesgaba ±1 LSB acumulado): **0,00 %** en los diez presets.
- **Presupuesto**: tabla de **12 288 B de flash** `const` (XIP) y **0 B de RAM nueva** — el estado de cada voz
  cabe en `voice_t` (`ph[0..2]`, `s[0..4]`, `keep = 0x1f`). Polifonía `NVOICE` (12), dos lecturas por muestra
  con UNI. En host: **11–12 ns** por muestra y voz aislado, **62,7–116,6 ns** con la ruta de voz completa, y
  un peor caso de nivel de **1,94 × `VOICE_FS`** por voz (ANALOG mide 2,07, PHASE 1,47 en la misma prueba).
- **10 presets**: GLASS PAD, MORPH LEAD, SINE KEY, VOCAL AAH, METAL BELL, HOLLOW WOOD, BRASS SWELL, SUB BASS,
  PULSE ORGAN, QUANTUM MASS. Los knobs de HOME del motor son MORPH, HARM, CUT y UNI.
- **Auditoría de los motores que ya estaban** (`docs/QG_PHASE_3_ENGINES.md` §1): los 8 vivos con sus
  parámetros reales, su polifonía y sus límites. **No se cambió ninguno**: tocar sus parámetros cambia el
  sonido de 200 presets y sus renders dorados, y eso necesita su propia pasada medida. STRING, ADD y MATH
  **no existen** en el árbol (lo aditivo es WHEEL, lo vocal/cuerda es VOICE, lo «math» es FM6); el pedido
  pedía además exactamente un motor nuevo, y ése es WTABLE.
- **Pruebas**: `tests/wavetable_test.c` (fotogramas y morph medidos por espectro, HARM, SUB, UNI batido,
  aliasing en cada octava de C1 a C8, continua, clipping, release, determinismo, coste) y **230 renders
  dorados: 0 cambiados y 10 nuevos**, o sea que ningún sonido anterior se movió. Auditoría de presets: 212,
  0 silenciosos, 0 duplicados. El motor se llamó `WAVETABLE` hasta que el lint de maquetación demostró que el
  campo lo cortaba: es **`WTABLE`**, seis caracteres, y el barrido vuelve a 0 hallazgos.
- **Sin medida de placa**: no hay toolchain AC791N en este entorno (sin `.bss` enlazado, sin flash real, sin
  desensamblado del bucle) ni unidad física (sin escucha). Lo que hay es host x86 y está dicho así en el
  documento de la fase.

## 2026-11-12 — FASE 2/5: la interfaz, viva (medida)

- **Arranque**: el splash no era lento, estaba congelado. Su reloj era `fm1_ms`, que sólo empieza con TIMER5, así que toda la inicialización (panel, motores, audio, USB, LEDs, segundo núcleo, autosave, reel) corría con el contador en 0 y el splash se quedaba en su primer fotograma: de ahí el segundo o dos de pantalla quieta antes de la animación. Ahora se temporiza con TIMER4 desde el primer píxel y repinta al terminar cada etapa (`splash_step`), y lo que la inicialización gasta ya no se vuelve a pagar en el bucle. Además `SPL_HOLD` 2800 → 2000: otro 0,8 s menos, con la firma y el fundido final intactos (4,5 s desde la primera luz).
- **HOME, la estrella**: cuarto disco de aliento, corona de ocho rayos que gira y cambia de longitud con la energía, llamaradas cada ~7 s, y la estrella como blanco: al tocarla, un pulso expansivo enciende y agranda los cinco mundos.
- **HOME, los mundos**: cinco tonos distintos en vez de cinco ámbar (rocoso cálido, gaseoso dorado, anillado rosa, oceánico violeta, congelado azul hielo), un clima propio por cuerpo (polvo, ojo de tormenta, destello, brillo del anillo, escarcha) a su propio período, y una hairline del fondo en `r + 2` para que dos órbitas que se cruzan no fundan dos cuerpos en una silueta.
- **HOME, profundidad**: dos mundos que no orbitan — giran donde están, con terminador propio y mezclados lejos del fondo — y las órbitas pasan por delante de uno.
- **Coste**: HOME 180,0 → 174,9 µs por fotograma en `ui_render` (host) y su visualizador 99,7 → 93,5 µs, con más dibujo que antes.
- **Pieles**: NEBULA, PLASMA, ORGANIC, MINIMAL y RETRO se suman a THERMAL, cada una con su propia rampa térmica además de la paleta (curvas, medidores, estrella y planetas incluidos), elegibles en MENU > COLOR y guardadas como el resto. Las seis pasan los controles de contraste del generador; ninguna toca el audio.
- **Pie de página**: HOME tiene ocho páginas y el pie las llamaba todas «HOME»; ahora dice «HOME 3/8», como el resto de las familias.
- **Dos defectos corregidos de paso**: `firmware/src/ui_palettes.h` (versionado) tapaba a la cabecera generada, así que una paleta nueva no llegaba al build; y la reacción del cuerpo oceánico nacía dentro del cuerpo, donde la nueva hairline la borraba (5 px de respuesta medidos → 62 px).
- **Verificación**: 191 pantallas × 22 paletas en el barrido de render, 0 hallazgos de lint, 0 desalineaciones en 2 863 797 medidas; las cinco reacciones al toque siguen siendo cinco y reiniciables; emulador web compilado desde el árbol (build `fb4e0f867ba6`) con arranque, splash, toque en la estrella y «HOME 2/8» comprobados a mano. Nada de esto es una medida de placa: sin toolchain AC791N ni unidad no hay `[TARGET]`.
- **Pendiente declarado** (`docs/QG_PHASE_2_UI.md` §7): FX ROUTING y SEQUENCER TOOLS no se implementaron; MOD MATRIX, PERFORMANCE y PRESET BROWSER ya existían y se documentan en su sitio.

## 2026-11-12 — FASE 1/5: optimización total (medida)

- **Mixer**: a part with no sends (CHOR / DLY / REV at 0) no longer clamps its signal for the buses nor divides for the send ceiling — the clamp and the division only happen when there is something to send. Bit for bit the same sound; 0.4 % cheaper per audio block on a dry six-part scene in the host bench, and neutral with every bus fed.
- **FX buses**: the chorus line's index is masked once per sample instead of twice (the write and the phaser tap).
- **Instrument**: `tests/fx_bench` now prints process CPU time next to the wall clock and measures two scenes (every bus fed / dry parts). That is what made a 1 % difference readable on a shared VM — and what got a candidate rejected instead of merged: hoisting the master's per-block settings cost 1.4 % more CPU and was reverted (measured, not assumed).
- **TIMER5 tick**: a key column that reads exactly as it did last frame and has no debounce in flight no longer walks its four `fm1__key` calls (each only stored a 0 over a 0): 35–36 → 27–28 ns of CPU per tick idle (−22 %) and −15 % with a column held, on a new host bench (`tests/isr_bench`, built by the suite). `main.c`'s tick also computes its `sub % 5u` phase once instead of twice. The 16-bit 595 shift is untouched: 48 of the tick's 60 GPIO register accesses are that shift, whose pace the board needs, so the hardware-bound part of the tick is unchanged. Cost: +24 B of `.bss` (`sizeof(fm1_in)` 280 → 304 B, measured).
- **Record**: `docs/QG_PHASE_1_OPTIMIZATION.md` has the recovered state (rebased onto v2.8.4), the host profile, the measurements, what was refused and why, the blocked target/WASM/hardware checks, and the extension points for phases 2–5. `run_tests.log` is a real run again: the tracked one was a stale, failing log from a tree that no longer exists.
- **Tests**: `ALL HOST TESTS PASSED` (119 sections), 220 golden renders unchanged, 0 lint findings over 191 screens × 17 palettes, ASan/UBSan and both fuzzers clean. No target build, no WASM, no unit: nothing here is a hardware measurement.

## 2026-11-12 — v2.8.4: the HOME screens breathe

- **Atmosphere**: the four new HOME screens share a background layer drawn before their text — sixteen embers that drift upward a pixel or three a frame (hashed positions, so they never clump) and a floor of six bars, each the level its track is making now. All of it is 1 px rects in the panel's own steps (`T_RAISE`/`T_LINE`), so no palette can make it unreadable and it costs no allocation and no per-pixel path.
- **Hot meters**: MIXER's level cells turn to the accent above 84 %, and MODULATION's depth bars above 96 %, so a track that is pushing reads at a glance.
- **Knob labels** on PERFORMANCE no longer carry a numeric suffix.

## 2026-11-12 — v2.8.3: HOME is a full set of screens

- **MIXER, redrawn**: each track now reads as the instrument draws everything else — its number and the engine it is running, a nine-cell level meter with the peak held beside it, the pan as a two-pixel tick on a ruled centre line (it used to hide under the ruler's own line), a MUTE pill, and its fader position in cells. Below, the mix's own L and R peaks with a scale, plus the master level. Still nothing stored: `trk[].vu` and `trk[].peak` are what `fx.c` already writes.
- **PERFORMANCE**: octave, transpose, root, scale, tempo, swing, and the selected track's arpeggiator (mode, rate, octave, gate, swing) with the four knobs HOME turns and their values. Every row is a live parameter the DSP reads.
- **SOUND ENGINE**: the engine's own eight parameters with the labels and enum names from its descriptors, the preset it is playing and its number, voices in use against the cap, morph, cutoff and resonance.
- **MODULATION**: the four matrix slots (source → destination, depth as a bar, off rows drawn off) and the LFO's four destinations (`P_LD_*`) with their depths.
- **Tests**: 191 screens (from 188) in 17 palettes, 0 lint findings, 0 of 2 779 972 alignment measurements off; the two overlaps and one out-of-cell spill the sweep found in the first draft of these screens were fixed, and the HOME cycle test now walks all eight screens and asserts each of the four new ones leaves the page, the track and the sound untouched.

## 2026-11-12 — v2.8.2: HOME gets a desk

- **HOME > MIXER**: the fourth HOME screen (after VISUALIZER, OVERVIEW and SYSTEM), reached by the same tap that walks the list. Six rows, one per track: the level the block actually reached (the peaked envelope `fx.c` writes, not a sample), the pan as a tick on a centred ruler (`P_PAN` is −64..63, so 0 lands in the middle), MUTE or the voices the track holds now, and the mix's own two peaks at the bottom. Everything on it is a number the firmware already keeps; nothing new is stored, so a project saved before this screen loads and sounds exactly as it did.
- **Tests**: the render sweep goes from 187 to 188 screens — `home_mix` runs in all 17 palettes with 0 lint findings and 0 of 2 778 340 alignment measurements off. `docs/QG_V2.8_FINAL_AUDIT.md` moves HOME's views from PARCIAL: PERFORMANCE, SOUND ENGINE and MODULATION still do not exist, and this release does not pretend otherwise.

## 2026-11-12 — v2.8.1: the boot is 0.8 s shorter

- **Boot**: the splash's hold drops from 3.6 s to 2.8 s (total 5.3 s instead of 6.1 s). The 2.5 s signature write is untouched, so only the wait after it shrinks — the animation is the same, the instrument answers sooner. `MENU > STARTUP SPLASH` SHORT/OFF work as before.
- **Tests**: `ui_render` splash timeline re-measured; host suite unchanged.

## 2026-11-12 — v2.7.6: the EXTRA tab answers, and the delay shows its train

- **MENU > EXTRA**: under its rows it now says which cores are running (`CORES 1` / `CORES 2`, from fx.c `mc_status()`, the same line ABOUT shows), the audio ISR's own load (`CPU n%`) and, in the corner, when CPU MODE applies (`NEXT POWER-ON`). It did work — main.c reads it before core 1 starts — but a setting that changes nothing until the next power-on reads as a dead row, which is exactly what it looked like.
- **FX > DLY**: the panel above the six repeat slots was empty; it now draws the train's envelope from the peaks the effect actually returns (`fx_dly_rep`), joined and filled to the axis, with a dot on each repeat. Same data the bars had, in the shape of a decay.
- **Tests**: `ui_render` reports 187 screens x 17 palettes, 0 lint findings and 0 of 2 777 796 alignment measurements off by a pixel; 220 golden renders unchanged; `ALL HOST TESTS PASSED`.
- **Tooling**: `QG_DUMP_NAME=<screen>` renders one screen by name (`tests/ui_render.c`), so a visual review names the screen instead of counting an enum.

## 2026-11-12 — v2.7.5: SCL, HOME views, M-WIDE and the security pass

- **SCL**: `chord_build` handles 5, 6 and 8-note scales (ported from JIANT 0.8.3) and six SCL screens show them; `tests/chord_scale_test.c` checks 8 448/8 448 invariants and 360 chords identical to the old traversal.
- **HOME**: a second screen suite (HOME -> VISUALIZER -> OVERVIEW -> SYSTEM) with the engine/time/system summary; `test_home_views` and the 187-screen render sweep stay green.
- **FX**: M-WIDE compensates its own power (a +3.8 dB jump at 127 is now -0.0 dB, correlation still -0.594, mono still mono at -3.9 dB); `tests/fx_probe.c` measures phaser, M-WIDE, LOWCUT and EQ levels.
- **Presets**: `tests/preset_audit_test.c` audits all 202 factory presets (0 silent, 0 out of range, 0 repeated names, 0 duplicates).
- **Audio path**: `fx_buses` walks the chorus, delay and reverb buses once instead of twice (the peaks also answer "anything sent?"), measured with the new `tests/fx_bench`: median 127.2 -> 126.0 ns per sample on host x86, 220 golden renders bit-identical.
- **Security**: `release/SHA256SUMS` had committed merge markers and `coherence_check.py` crashed on them instead of reporting; both fixed, the manifest regenerated from the bytes on disk. `release.yml` pins the third-party build workflow to a commit instead of a mutable tag.
- **Release**: first v2.7.5 package built from this tree with the JieLi toolchain and the AC79 SDK (610 959 B, identity FM-1_965) and an emulator rebuilt with emcc 6.0.11; `coherence_check.py` reports ok.
- **Tests**: `ALL HOST TESTS PASSED`, ASan/UBSan and both fuzzers included; `tests/target_budget.py` reports 39 cost entries, all within budget.

## 2026-11-12 — v2.7.4.8: Reverb visuals and sync pass

- **FX / Reverb**: ROOM, SPRING, SHIMMER and RESO now have distinct geometries and livelier animation driven by the reverb bus return; ROOM adds ghost teeth and an air ring, SPRING a wobbling coil and a second chirp, SHIMMER denser animated sparks, RESO a second travelling pulse and a resonance flash.
- **Tests**: `ui_render` and host tests pass after the change.

## 2026-11-12 — v2.7.4.7: PHASE screen header and sync pass

- **PHASE**: the dedicated `graph_phase` header now reports DCW alongside WAVE, W2, MIX/RING and DT, so the bent-phase screen exposes the parameter that moves its knee.
- **Tests**: `ui_render` (0 lint findings, 0 alignment errors) and `ui_test` pass after the change.

## 2026-11-12 — v2.7.4.6: Reverb model-specific animations and sync

- **Reverb models**: ROOM now shows breathing comb teeth, SPRING a travelling chirp marker, SHIMMER keeps its octave sparks, RESO/CLOUD keep their distinct animations.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.6 firmware + SHA256.

## 2026-11-12 — v2.7.4.5: FX VU meter routing fix and sync

- **FX meters**: the Reverb meter now reflects only the reverb bus's own return; Chorus or Delay activity no longer moves it.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.5 firmware + SHA256.

## 2026-11-12 — v2.7.4.4: Reverb RESO animation fix and release sync

- **Reverb RESO**: removed the out-of-panel glitching orbit ring and added a travelling pulse along each resonant string, with frame-rate scaling tied to the reverb return.
- **Emulator/site/readmes**: synchronized with the rebuilt firmware and WASM.
- **Release**: v2.7.4.4 firmware + SHA256.

## 2026-11-11 — v2.7.4.3: FX VU meters rebuilt on HOME's plasma meter

- **FX meters**: replaced the old FX-section VU meters with HOME's segmented plasma meter component, reusing its design, animation and decay ballistics.
- **DSP**: verified send / wet-return / tail paths remain independent and settle to zero when effects are off.
- **Build**: firmware and WASM emulator rebuilt for AC791N, manifests and hashes synchronized.
- **Release**: v2.7.4.3 firmware + SHA256.

## 2026-11-11 — v2.7.4.2: FX visual polish, WASM rebuild and release sync

- **FX**: rounded panels and improved visual consistency.
- **Build**: firmware and WASM emulator rebuilt and manifests synchronized.
- **Release**: v2.7.4.2 firmware + SHA256.

## 2026-11-11 — v2.7.4.1: UI redesign pass, planets, visualizer, FX and splash

- **HOME / solar system**: deeper star field, more dust, nebula and reactive planets (spin, swell, ripple, sling, frost), all five answer taps.
- **Visualizer**: denser reactive particles and orbital elements, improved depth and transitions.
- **FX**: meters track real per-bus peaks; DIST meter idles when drive is off; splash timing extended ~1 s.
- **Splash**: longer writing animation (SPL_WRITE 2500 ms) with hand-over preserved.
- **Release**: firmware v2.7.4.1, package, manifests and hashes.

## 2026-11-11 — v2.7.4: FX meter fix and release

- **FX meter (DIST)**: the DIST bar in the FX screen now stays dark when drive is off (`P_DIST == 0`), instead of following the dry track signal. The other three buses already read `fx.c`'s real measured peaks.
- **Release**: firmware v2.7.4, package `release/felucca-v2.7.4.fwsc`, emulator (WASM) and manifests, same commit.

## 2026-11-11 — créditos con nombre y apellido

- **Créditos del linaje corregidos.** `README.md` y `MENU > ABOUT > CREDITS` atribuían Pocket GammaWave (GW-1) a
  Hügelton Instruments; la línea GW-1 es de **Frederic Valente (@fvalente)**, como ya decían `LICENSING.md` y este
  CHANGELOG. JIANT FM y Felucca quedan a nombre de **Leo Kuroshita (@kurogedelic) / Hügelton Instruments**.
- **Se suman los aportes de este fork**: **Guillermo Lauria** (instalador publicado, CI y registro de build del
  emulador, capa de FX, banco QUANTUM) en README y en el ABOUT del firmware; **David Konsumer (@konsumer)**
  (primera instalación en una unidad de fábrica, chequeo de chip del loader, reanudación, releases por tag) en README.
- **`firmware/src/ui_being.c`**: el encabezado decía `Copyright (C) 2026 ShadowTune / GammaWave // Wraith Custom
  Firmware`, un titular que no aparece en ningún commit, en `LICENSES/` ni entre los colaboradores. El archivo se
  creó en este repositorio y los mundos de HOME se desarrollaron acá: el aviso pasa a Guillel51 / Quantum
  GammaWave, aclarando que "Wraith" es un nombre propio de este firmware (la ribbon y la barra).
- **Marcadores de conflicto, fuera del árbol y con guard.** `firmware/src/felucca.c` llegó a `main` con un marcador de conflicto sin resolver dentro de su
  `#ifndef FELUCCA_VERSION` y `ci_artifact/manifest.json` con el JSON partido: el build
  AC791N no define la versión con `-D`, así que sólo el target fallaba. Los dos resueltos, `ci_artifact/` (registro
  de build commiteado, de un commit que ya no existe) retirado, y `tests/run_tests.sh` ahora nombra cualquier
  marcador commiteado con archivo y línea antes de compilar nada.

## 2026-11-11 — v2.7.3: emulador WASM, manifiestos y publicación completa

- Emulador WASM de calidad estrella (Web Audio Worklet, trabajo en tiempo real 44.1 kHz).
- Página de presentación y fotos del emulador actualizadas.
- Actualización del emulador WASM: 1 104 856 B → 1 112 286 B, SHA-256 `56c78aa354fd04f89e2d97b9a6e592dcbc68bcce643cb50c60aaae28cd8ae244`.
- Versionamiento de toda la web: `site/firmware/versions.json`, manfiestos y capturerros actualizados.
- Página del instalador y del editor actualizadas para v2.7.3.

- **El ABOUT publicado mostraba créditos viejos.** El paquete y el emulador de v2.7.1 se construyeron en
  `91029198`, antes del commit de créditos: el fuente ya decía Frederic Valente (@fvalente) para GW-1 y sumaba a
  Guillermo Lauria, pero el binario servido no. v2.7.2 se construye desde este árbol, así que `MENU > ABOUT >
  CREDITS` del paquete y del emulador publicados dicen lo mismo que `README.md`.
- **Mismo procedimiento, sin atajos:** paquete AC791N con la toolchain pineada, manifiesto de integridad
  (SHA-256 + tamaño, modelo de §9), emulador del mismo commit, espejo del sitio y `release/SHA256SUMS` regenerados
  juntos; `tools/fm1_sign.py verify` sobre el paquete antes de commitear.

## 2026-11-11 — v2.7.1: el conflicto del firmware, resuelto, y el paquete reconstruido

- **El arbol no compilaba.** El bump a v2.7.1 llego con marcadores de merge dentro de
  `firmware/src/felucca.c` (y de `ci_artifact/manifest.json`): `cc` fallaba con "version control conflict marker".
  Se resuelve conservando `#define FELUCCA_VERSION "v2.7.1"` y se elimina el `ci_artifact/` suelto, que era una
  copia de artefacto sin lectores.
- **Todo vuelve a nombrar una sola version.** README, `web/site/`, `web/index_pkg.html` y el banner pasan de
  v2.7.0 a v2.7.1; el arbol queda limpio (`dirty: []`) en el commit del build.
- **Paquete y emulador reconstruidos desde ese commit** (`9102919`): `release/felucca-v2.7.1.fwsc` 610 959 B,
  sha256 `8b1798b93892ffcc89956aeb3b6d5e1e8e14c1c48d3e9c472f08386fd3456de6`, imagen 541 484 B, identidad
  `FM-1_965`; WASM 1 104 650 B sha256 `e3388879…2d03`, Emscripten 6.0.11, mismo commit. Manifiesto
  `qg1-release-1` sin firma (`keyid: ""`), `fm1_sign.py verify` -> integrity only, y `sha256sum -c` limpio
  sobre las 22 entradas de `release/SHA256SUMS`.
- **Imagenes al dia:** banner y las 30 capturas de `assets/screenshots/` re-renderizadas del arbol v2.7.1
  (el splash del banner ya muestra la version real), y el espejo `site/` regenerado desde el paquete nuevo.

