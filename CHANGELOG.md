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

