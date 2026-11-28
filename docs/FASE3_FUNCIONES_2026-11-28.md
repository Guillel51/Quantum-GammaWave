# Fase 3/4 — una función sonora nueva, y lo que ya estaba (medido)

Fecha: 2026-11-28. Versión **v2.23.0**, sobre v2.22.0 (`57cee704`).

Clases de medición, separadas (AGENTS.md §4): **host x86-64** = `tests/phase_wave_test.c` y la suite de host
compilados con `cc`; **build AC791N** = `./build.sh` con el toolchain `pi32v2` (instrucciones estáticas sobre
`build/felucca.dis`, ventanas sobre `build/felucca.map`); **WASM** = el emulador. No hay FM-1 física: ninguna de
estas cifras es un ciclo ni un timbre de la unidad.

## 1. Lo que se entrega: PHASE, la décima forma de onda (RECT)

**Qué es.** `eng_phase.c` ofrecía nueve formas (SAW, SQR, PLS, DSIN, SPLS, RSAW, RTRI, RTRP, SYNC). Se agrega
**RECT**: el coseno con su media onda negativa eliminada. La rectificación de media onda añade armónicos pares
—el segundo queda tan fuerte como el fundamental— así que la nota suena hueca y con un cuerpo una octava arriba
de la que toca la tecla: un timbre de órgano/reed que el motor no tenía. El motor ya termina en su bloqueo de DC
(FASE 5), así que el desplazamiento que introduce la rectificación no llega a la voz.

**Ruta DSP real, sin estado nuevo.** Una rama en `pd_setup` (sin doblar la fase: `b->x = 0`) y una en
`pd_wave`: `y = pd_cos(ph); return y < 0 ? 0 : y;` — un compare y un select por muestra, sin memoria, sin
división. `DCW` conserva su significado (índice de la segunda línea) como en todas las demás formas.

**Neutro y compatible hacia atrás.** El valor es **9**, se agrega al final del enum y el descriptor pasa de
`0..8` a `0..9`. Los nueve valores anteriores conservan nombre y sonido: ningún preset, proyecto ni preset de
usuario cambia de onda al cargarse, y los 316 renders dorados de `tests/regress` salen **0 changed**. El clamp de
"un valor guardado más allá de la última onda toca la última" se movió de 8 a 9; un store de antes que traiga
8 sigue tocando SYNC.

**Edición, persistencia y las otras interfaces.** El byte que lo guarda es el mismo de siempre (`P_E0` por
pista, en el proyecto y en el preset), así que no hay migración: la edición física es el knob WAVE de la página
EDIT 1 del motor, el nombre sale del mismo descriptor que dibujan la página, el editor y el emulador, y el
`desc.json` que consume Web Studio se genera de esa tabla (no hay lista paralela que actualizar).

**La segunda línea también.** WAVE2 cuenta desde `"-"`, así que su lista se completa hasta la onda 9: se agregan
**SYNC** y **RECT**, y la línea 2 puede sonar con las dos (antes llegaba hasta RTRP). Son dos valores al final del
enum de WAVE2 (0..10); los guardados de antes no cambian de onda.

**Prueba nueva** (`tests/phase_wave_test.c`, en `tests/run_tests.sh`): 10 renders, uno por valor de WAVE, en
procesos separados; **10 hashes distintos**; RECT dentro del headroom de la parte (pico 36 194) y **centrado**
(media 763/6400 frente a un pico de 36 194: el bloqueo de DC hace su trabajo); determinista (la misma onda dos
veces, los mismos bytes); WAVE2 = SYNC y WAVE2 = RECT con sonido propio; y los nueve nombres de antes intactos,
que es lo que protege los stores.

| medición | valor | clase |
|---|---|---|
| formas de onda de PHASE (WAVE) | 9 → **10** (RECT) | código |
| formas de WAVE2 | 8 → **10** (SYNC, RECT) | código |
| renders dorados | 316, **0 changed** | host x86-64 |
| pico / media de RECT en la prueba | 36 194 / 763 de 6 400 muestras | host x86-64 |
| `phase_render` (coste estático) | ver §6 | build AC791N |

## 2. Los otros nueve motores: qué se auditó y por qué no se agregó nada a ciegas

Se leyeron los diez motores y sus tablas de parámetros. El hallazgo que decide esta fase: **cada motor tiene
exactamente ocho parámetros de motor** (`P_E0..P_E7`, `param_desc_t edit[8]`), y el formato de proyecto, el de
preset y el del editor se definen sobre ese espacio ("los últimos 8 son P_E0..P_E7", `core.h`). Agregar un noveno
knob por motor no es un `enum` más: cambia el formato de almacenamiento y todas las páginas EDIT. Lo que sí es
barato y seguro —y lo que se hizo— es **agregar una posibilidad nueva a un parámetro enumerado que ya existe**,
al final de su lista, donde los valores viejos conservan su número y su sonido.

Con ese criterio, lo que ya estaba en cada motor (medido con `tests/param_sweep_test.c`: 80 knobs de motor y 118
de pista, 0 muertos) y lo que queda realmente pendiente:

| motor | lo que ya existe (y no se repite) | lo que faltaría de verdad |
|---|---|---|
| ANALOG | SYNC, SUB, PM, dos osciladores con MIX, MORPH entre formas | un **modo** de mezcla (no un knob): necesita una ranura de enum libre en MIX |
| PHASE | nueve formas, WAVE2 alterno, LINE MIX/RING/PM, FB, SYNC, SUB, DC block | **RECT (hecho)** |
| LOFI | CHIP, WAVE, DUTY, CRUSH, CUT, VIB, BEND, MASK | resolver CRUSH/MASK es un cambio de semántica: altera presets existentes |
| VOICE | dos vocales, TALK, SHIFT, BUZZ, BREATH, Q, RAND | articulación nueva = estado por voz (RAM) |
| WHEEL | REG, SUB, BODY, TOP, PERC, CLICK, DRIVE, ROTOR | evolución de componentes = estado por voz |
| NOISE | FREQ, PEAK, TRK, DENSE, DRIFT, CRUSH, COLOUR | movimiento nuevo = estado por voz |
| DRUM | 8 carriles DRUM-X con MORPH A/B, TUNE, TONE, choke | otro modo de síntesis = más código en el camino más medido |
| FM6 | 32 algoritmos, FB por operador y global, ratios, 6 operadores | snap de ratios: cambia lo que ya suena |
| WTABLE | 8 frames, MORPH, HARM, UNI, SUB, CUT/RES/DRIVE | recorrido nuevo = otra tabla (XIP) |
| ADDTV | 8 parciales, DRAW, SPECTRUM, TILT, DECAY, STRETCH, SUB | niveles por parcial = 8 bytes por voz |

Ninguno de esos cambios es "de una línea", y varios cambian el sonido de presets que ya existen. Con el árbol
sin oídos y con 5 620 B de XIP, esta fase entrega el que sí se puede probar de punta a punta en este entorno
(RECT) y deja los demás documentados con su coste, en vez de dibujar controles sin ruta.

## 3. EDIT y LFO: lo que la fase pide ya está implementado

Revisado `voice.c` (`track_lfo_tick`, `lfo_skew`), `params.c` (las páginas LFO, LFO DEST, LFO CURVE) y
`ui_graph.c` (los gráficos):

- **formas**: 12 (`SIN TRI SAW SQR S&H RAMP EVO HSIN PULSE PLUK TRAP STEP`), agregadas al final para que cada
  WAVE guardado conserve su número;
- **sincronización BPM y divisiones**: 14 (`4BAR .. 1/32`), `RATE` negativo = en tiempo;
- **fase y retrigger**: `PHASE` con `-1 = FREE`, y **RETRIG** como tarjeta propia (LFO CURVE, OFF/ON) que escribe
  el mismo byte, así que las dos páginas nunca se contradicen;
- **fade, slew**: `FADE` por pista y `SLEW` en LFO CURVE (neutral 127 = apagado);
- **polaridad e inversión**: `INVERT` (neutral 127), y AMT bipolar por destino;
- **profundidad y destinos**: cuatro destinos (PITCH, FILTER, SHAPE, AMP) con AMT en LFO DEST, más la matriz MOD
  con `LFO` como fuente.

**Lo único del listado que no existe es un DELAY** (una espera antes de que el LFO empiece, distinta de FADE, que
es la rampa de entrada). No se agregó en esta entrega por una razón concreta: cada página tiene exactamente
cuatro tarjetas y las tres del LFO están llenas; una tarjeta nueva obliga a una página nueva, y el número de
página es un índice que la configuración guarda entre sesiones — eso se hace con su migración y sus píxeles, no
al final de una entrega. Queda anotado como el siguiente paso del LFO, con su ruta ya clara (`P_LDLY`, F_TIME,
leído en `track_lfo_tick` antes del fade, neutral 0 = exactamente el comportamiento de hoy).

## 4. SCL: lo que existe

`ui_scl.c` ya tiene: el círculo de doce notas con el grado coloreado (`scl_degcol`), un teclado de una octava con
**nombre en las siete naturales y en las cinco alteradas**, la franja de la escala en cada tecla, el **grado** de
cada nota marcado por color, máscaras de **nota sonando** vs nota elegida (con el destello de ataque leído de las
voces reales, no de un temporizador), reconocimiento de acordes con inversión nombrada desde el bajo, y la
coherencia con el secuenciador y el MIDI por la misma máscara.

Pendientes reales, sin inventar producto en esta entrega:

- **comparar dos escalas lado a lado**: el modelo tiene *una* escala por pista (`P_ROOT`, `P_SCALE`); compararla
  con otra exige una segunda ranura en el modelo (y decidir si es por pista o global). Es una decisión de diseño,
  no un dibujo.
- **teclado más ancho que doce notas**: la cama actual es de una octava; ampliarla cambia la geometría del
  teclado, el hit test (`scl_tap`) y la firma de las páginas — trabajo con su propia revisión de píxeles.

## 5. Renovación visual: estado real, sin cambios a ciegas

La paleta **QUANTUM ya es la de fábrica** y lleva exactamente los cinco colores pedidos
(`0x0022, 0x10a5, 0xef9f, 0x3eff, 0xfd44`, `ui_palettes.h`), y **SOLARIS no es la identidad por defecto**. Las
pantallas de la lista (SHIFT, PATTERN, TILT, LFO SHAPE, PRESETS NOFAV, MASTER, los gráficos) ya se dibujan con
`ux_mix()`/`heat_col()` y con las capas de `gfx.c`.

En esta entrega **no se rediseñó ninguna pantalla**: un cambio de profundidad o de gradiente sin revisar los
3 353 frames del barrido, el lint de alineación por tinta y el coste por pantalla es exactamente la clase de
cambio que rompe la legibilidad que la fase pide mejorar. Queda como el trabajo principal de la próxima
iteración visual, con la herramienta que ya existe (`ui_render` + `ui_pixels.py` + el informe de alineación).

## 6. FX, ARP y secuenciador

Sin cambios: se volvieron a correr las regresiones que ya existen y **no se reprodujo ningún defecto** que
justificara tocar el código.

- FX LATCH / punch-in: `tests/perform_test.c` (1/16, efectos de buffer estéreo, REPEAT demasiado largo, SLICER,
  teclas silenciosas, reposo bit a bit, coste) — ok.
- DELAY/REVERB y sus trenes de eco: `tests/dly_echo_test.c`, `tests/reverb_test.c` (ROOM bit a bit, SPRING,
  chirp, estabilidad, coste) — ok.
- BUSES y sus seis bloques: la página dibuja el estado real de los buses; `ui_render --fxanim` mide que el
  chorus, los medidores, los seis bloques y el ataque del ADSR se muevan en píxeles — ok.
- ARP/SEQ: `tests/ratchet_test.c` (x1..x4, gates, chance, swing), `tests/motion_test.c` (ARP repeat y modos),
  `tests/pattern_len_test.c` (LEN por pista) — ok.

## 7. Pruebas y presupuestos de esta entrega

| grupo | resultado | clase |
|---|---|---|
| `tests/phase_wave_test.c` (nuevo) | 10 ondas distintas, RECT en rango y sin DC, 9 nombres viejos intactos | host x86-64 |
| `tests/run_tests.sh` completo | **ALL HOST TESTS PASSED** | host x86-64 |
| `tests/stress_run.sh 3` | ok (ASan/UBSan) | host x86-64 |
| `tests/regress` | 316 renders, 0 changed | host x86-64 |
| XIP / RAM / imagen | ver la tabla de la release | build AC791N |
| emulador, sitio, coherencia | ok (mismo commit) | WASM / datos |

## 8. Lo que esta fase NO entrega (dicho antes de que se pregunte)

- Nueve motores sin función nueva (§2): cada uno necesita su propio cambio de formato o de estado, con su
  presupuesto; quedan documentados con el coste.
- LFO DELAY: diseñado, no implementado (§3), por la ranura de página.
- Comparación de dos escalas y teclado más ancho (§4): decisiones de modelo y de geometría.
- Renovación visual de las pantallas de la lista (§5): es la próxima iteración, con revisión de píxeles.
- Nada de esto se declara terminado ni se dibuja sin ruta.
