# FASE 2 — polifonía, gestión de voces y estrés de audio (v1.5.23)

Lo que sigue es la segunda fase de recursos: primero los pendientes y las regresiones heredadas de la Fase 1, después
una revisión del asignador de voces **medida**, y sólo las mejoras que un número sostiene. Cada cifra lleva su clase:

- `[AC791N]` — build de destino (`./build.sh --release 1.5.23`), tamaños del ELF, estático.
- `[HOST]` — x86-64 de este entorno, las fuentes reales (`tests/hostsim.c` incluye `voice.c`, los motores, `fx.c`,
  `seq.c`); dice qué hace el asignador, no cómo suena el DAC.
- `[ESTÁTICO]` — el presupuesto de ciclos de `tests/target_budget.py` sobre el ELF.
- Física — lo que sólo una FM-1 puede decir. **No hay unidad en este entorno**: nada de lo que sigue afirma sonido,
  panel, ni DAC. Lo que necesita la unidad está listado al final.

## 1. Pendientes de la Fase 1 y regresiones

La Fase 1 se cerró publicada (v1.5.22: `d4b4fd6` código, `1b5dc1c` paquete, tag y release con cinco assets). Antes de
tocar el asignador se corrió la suite completa sobre `1b5dc1c` y sobre el árbol de trabajo:

- `[HOST]` **6 315 `ok`**, 176 secciones, sin FAIL más que los dos esperables de un bump de versión (48 fotogramas
  `about`/`about_rec`/`splash` en `tests/ui_pixels.txt` y el índice de `docs/EVIDENCE.md`, ambos regenerados).
- `[HOST]` **306 renders golden, 0 cambiados**, 0 fallos de salud, 0 de voces/routing: la Fase 1 no dejó regresión
  audible en el banco.
- `[ESTÁTICO]` `target_budget.py` en verde, `DRUM path 1 153/1 153`.
- No se repitió ninguno de los arreglos ya cerrados (cola de diferidos `VQ_N`, tope de voces por ECO, `quiet_tail`, el
  gobernador que ya no corta colas con presupuesto libre): están leídos y sus tests siguen pasando, y este documento
  dice dónde están en vez de rehacerlos.

## 2. La revisión: qué hace hoy el asignador (`firmware/src/voice.c`)

Leído completo, con `core.h` al lado. Lo que se verificó, para no volver a "arreglarlo":

| mecanismo | comportamiento leído | cubierto por |
|---|---|---|
| reparto entre tracks | ventanas por track (`vidx`/`vn`), `voice_window_grow` sólo presta un slot **que no suena** (nunca mueve una voz viva), `vshare()` decide desempates | `poly_six_test` B/C/E, `poly_phase2_test` E |
| robo | `quiet_tail` (colas < −40 dB) → `voice_victim` (clase: release, extra de UNISON, sostenida no-bajo; después fresco → sobre-cuota → energía → edad); el bajo sostenido y el lead MONO son intocables | `poly_six_test` D2/S, `audio_test` |
| colas de release | el gobernador de carga ya no corta una cola con presupuesto libre (v1.5.17); a tope de presupuesto, la cola es la que paga, no la nota que suena | `poly_six_test` D/D2/E, `poly_phase2_test` D |
| ráfagas de MIDI/teclado | `MQ` 64 paquetes de entrada, `VQ_N` 8 diferidos que se aplican en el acto si se llenan (nunca se pierde una nota por mailbox) | `voice_defer_test`, `poly_phase2_test` C |
| core 1 | la nota que llega mientras el otro core renderiza espera al bloque siguiente; el note-off nunca se difiere | `voice_defer_test` |
| sobrecarga | dos mitades > 80 % seguidas → se baja una voz con fade de 6 bloques; el presupuesto se aprieta a 65 % y se suelta tras 16 mitades bajo 60 % | `audio_test` (nuevo, abajo) |

Dos huecos reales aparecieron en esa lectura, y los dos son de **notas que se pierden sin que nadie las cuente**:

1. **Un acorde de doce notas no sobrevivía a un cambio de sonido que cambia de motor.** `track_t.xp_note/xp_vel`
   guardaban cuatro notas; `engine_block` re-engancha en el motor nuevo lo que encuentra ahí. Con doce teclas
   apretadas, ocho no volvían: la tecla seguía apoyada, su nota no sonaba y su note-off caía en el vacío.
2. **Una parte MONO / LEGATO con PRIO LOW o HIGH perdía la tecla que decide la altura.** `mono_stack` guardaba ocho;
   la novena apretada tiraba la primera, que con PRIO LOW era exactamente la que ganaba.

## 3. Las mejoras, con el número delante

### 3.1 La capacidad de acorde: doce notas, no cuatro ni ocho

Las dos pilas son ahora `NVOICE` (doce), que es lo máximo que una parte puede tener sonando, así que el instrumento no
puede desbordarlas. Medido `[HOST]`, las mismas escenas contra el binario viejo (`-DXP_N=4 -DMONO_STACK_N=8`, que es
exactamente el comportamiento de v1.5.22) y contra el nuevo:

| escena (`tests/poly_phase2_test.c`) | v1.5.22 | v1.5.23 |
|---|---|---|
| A: doce teclas apoyadas, carga de sonido que cambia de motor | **4 notas** vuelven a sonar | **12** |
| B: doce teclas en LEGATO, PRIO LOW | suena **64** (la tecla baja se perdió) | suena **60** |

### 3.2 Y por qué la capacidad va *al lado* de los tracks y no dentro

Esto costó cuatro builds y es la parte que conviene que quede escrita. Poner los arrays dentro de `track_t` **no entra
en el slot**:

| dónde vive la capacidad | `[AC791N]` imagen | libre | Δ |
|---|---|---|---|
| v1.5.22 (4 y 8) | 581 816 B | 1 796 B | — |
| los arrays crecidos **en su sitio** (12 y 12) | **584 768 B** | −1 156 B | **+2 952 B** → no enlaza |
| los arrays crecidos **al final del struct** | 584 736 B | −1 124 B | +2 920 B → no enlaza |
| la capacidad **al lado de los tracks** (esta release) | **582 144 B** | **1 468 B** | **+328 B** |

El crecimiento no está en los ficheros que se tocaron: `nm --size-sort` sobre los dos objetos lo reparte por toda la
UI — `ui_input_frame` +608 B, `tools_key` +210, `gen_make` +210, `graph_signature` +182, `draw_graph` +170,
`note_edit` +140, `part_dice` +112, `mix_block` +98 — porque `track_t` mide 1 760 B, lo lee todo el instrumento, y
mover sus campos un byte alarga miles de desplazamientos. La solución no fue pelear con el compilador: `track_t`
conserva **su tamaño y su orden** (los primeros `XP_IN_T` = 4 y `MS_IN_T` = 8 slots siguen donde estaban) y los slots
restantes viven en `xp_more`/`xp_morev`/`ms_more`, indexados por track, con tres accesores en línea
(`xp_note_p`, `xp_vel_p`, `ms_p`). Costo: **+128 B de `.bss`** (`[AC791N]` 86 784 → 86 912 B, 11 392 libres) y +160 B
de los 360 B de flash de toda la fase.

### 3.3 El guardián de sobrecarga: la voz menos audible, y una sola por episodio

`audio.c shed_voice` tenía dos defectos de criterio, y ninguno era de contadores:

- **Cortaba la más vieja, no la menos audible.** Entre las notas sostenidas de una parte elegía la de edad menor (el
  contador de note-ons), que es justo el cuerpo sostenido del acorde —lo más audible que tiene la parte— mientras la
  nota recién atacada, a una quinta parte de nivel, se quedaba. Ahora elige por `v_energy` (la envolvente por el nivel
  de la parte, la misma medida que usa el asignador) y la edad sólo desempata; el bajo y el lead MONO siguen
  protegidos.
- **Podía bajar tres o cuatro voces por un solo pico.** Una voz rebajada tarda seis bloques (~4,4 ms) en irse y
  mientras tanto cuesta lo mismo; un pico más largo que eso armaba otro `shed_req` y otro más. Ahora, si hay una voz
  en su fade (`stage 4`), el guardián ya actuó y no vuelve a pedir: un episodio, una voz.

`[HOST]`, `tests/audio_test.c`: la nota sostenida fuerte sobrevive a la nota recién atacada y floja; el segundo pedido
con el fade en vuelo no mueve el contador; cuando el fade termina, el guardián puede volver a actuar.

Y una trampa que el presupuesto estático cazó: con el barrido nuevo, el compilador **inlineó** `shed_voice` dentro de
`fm1_alnk0_irq` y el coste estático del ISR pasó de 306 a **854 contra un presupuesto de 291** (`[ESTÁTICO]`,
`tests/target_budget.py`: 290 instrucciones, 158 en lazos, 8 llamadas). El trabajo era el mismo; lo que cambió fue
dónde se mide. `shed_voice` va `__attribute__((noinline))`, como `shed_check` justo al lado: el ISR baja a **163
instrucciones, 55 en lazos, coste 94 (−68 % de la línea base)** y la imagen queda 32 B más chica. Es el mismo arreglo
que ya usaba este fichero para su otro barrido.

### 3.4 El monitor: lo que la carga costó, y qué hace cada track

`MENU > GLO MONITOR` ya contaba voces sonando / en release / en fade, pico, robos y denegadas (Fase 2 anterior,
issue #107). Se le agregaron las dos cosas que faltaban para leer una saturación:

- **`CPU LOAD … SHD n`**: el contador del guardián de sobrecarga (`load_diag.shed`) estaba sólo en `MENU > ABOUT`, así
  que la pantalla que muestra la carga no podía decir qué había costado. Ahora va en la misma fila, y sólo cuando es
  distinto de cero.
- **T1..T6 con dos estados**: el bloque de cada parte era un solo color para "tiene alguna voz", y una parte con el
  acorde apoyado y otra con el acorde ya soltado se leían igual. Ahora el bloque va en color de acento mientras una
  tecla o el secuenciador sostiene la voz, en color de tema cuando sólo quedan colas de release, y con una barra
  debajo tan larga como colas queden: el mismo dato que la fila POLYPHONY da para todo el instrumento, por parte.

Los 48 fotogramas de `tests/ui_pixels.txt` que cambian son los de la versión (`about`, `about_rec`, `splash`) y los
del monitor; se rebasearon y `python3 tests/ui_pixels.py build/ui_new/ppm` da **3 056 fotogramas idénticos**.

## 4. Pruebas de estrés nuevas (`tests/poly_phase2_test.c`, `[HOST]`)

Escenas que los tests anteriores no pedían, cada una contada donde el asignador cuenta:

| escena | qué pide | resultado |
|---|---|---|
| **A** | doce teclas apoyadas y una carga de sonido que cambia de motor | 12 notas vuelven a sonar en el motor nuevo, y siguen 8 bloques después; los doce note-offs encuentran su nota |
| **B** | doce teclas en LEGATO con PRIO LOW, soltando once | suena 60 antes y después de soltar las once altas |
| **C** | ráfaga de **24 note-ons en un bloque** | 12 suenan (el presupuesto), 12 robos contados, 0 denegadas, la última nota de la ráfaga suena, ninguna queda pegada tras los note-offs, el pool queda vacío |
| **D** | **doce colas de release acumuladas** (REL 110, seis partes) y un acorde nuevo | las 12 colas pagan (12 robos), las 12 notas nuevas suenan, 0 denegadas |
| **E** | **seis partes compitiendo**: una pide ocho notas al lado de cinco que sostienen diez | la que pide crece a 6 voces (ventana 7), el pool queda exacto en 12, ninguna parte se queda sin nota, nada se movió bajo una voz que sonaba |
| **F** | cambio de preset **bajo el acorde** con chorus, delay y reverb en las seis partes | las 12 siguen, 0 robos, el bloque lleva señal (pico 6 827) y no toca el riel |
| **G** | **tres ciclos de saturación y recuperación** (24 notas → todo off → colas) | el pool vuelve vacío y entero en los tres; después una sola parte vuelve a sostener doce |

Además `tests/audio_test.c` gana las dos comprobaciones del guardián (§3.3) y la suite registra las dos cosas como
secciones propias.

## 5. Comparación con el inicio de la fase

| medida | v1.5.22 | v1.5.23 | clase |
|---|---|---|---|
| notas de un acorde de 12 que sobreviven a un cambio de motor | 4 | **12** | `[HOST]` |
| altura con 12 teclas en LEGATO y PRIO LOW | 64 | **60** | `[HOST]` |
| notas perdidas en una ráfaga de 24 note-ons | 0 (12 robos contados) | 0 (12 robos contados) | `[HOST]` |
| colas de release cortadas con presupuesto libre | 0 | 0 | `[HOST]` |
| voces bajadas por un pico de sobrecarga | hasta 3–4 | **1** | `[HOST]` |
| criterio del guardián sobre notas sostenidas | la más vieja | **la menos audible** | `[HOST]` |
| imagen | 581 816 B (1 796 libres) | 582 144 B (**1 468** libres) | `[AC791N]` |
| `.data+.bss` | 86 784 B (11 520 libres) | 86 912 B (**11 392** libres) | `[AC791N]` |
| XIP (`.text`, código + constantes) | 561 244 B (427 468 + 133 776) | **561 604 B** | `[AC791N]` |
| RAM code | 19 760 B | 19 728 B | `[AC791N]` |

## 6. Lo que el límite físico no deja hacer (y por qué no se fuerza)

- **Subir `NVOICE` a 16.** El pool ya tiene los slots (`VPOOLN` 18), pero cada voz sonando cuesta ciclos en el camino
  del audio: `tests/target_budget.py` da el camino DRUM en 1 153/1 153 (`[ESTÁTICO]`) y `[HOST]` el bloque de seis
  partes con doce voces y FX ya usa el 70-80 % del deadline en este x86. Sin unidad no hay forma de medir el margen
  real de la AC791N, así que no se sube.
- **`track_t` más grande.** Medido arriba: +2 952 B de XIP por 120 B de struct. Cualquier campo nuevo en `track_t` se
  paga a ~25 B de flash por byte mientras el slot tenga 1 468 B libres.
- **El acorde de doce con motor FM6.** Su `engine_t.poly` es 4 (ingeniería del motor: seis operadores por voz). Un
  acorde de doce ahí no es una decisión del asignador y el monitor ya muestra el tope real (`voice_budget()`); el test
  A usa WAVETABLE justamente por eso.

## 7. Lo que sigue necesitando la FM-1 física

Nada de esta fase se probó en la unidad: **no hay FM-1 en este entorno**. Queda para quien la tenga:

1. `AUDIO > VOICES 12`, seis tracks con un acorde de seis notas cada uno y `TRACKS 6`: que el monitor lea 12 y que el
   DAC no corte (el pool no puede pasar de 12 por construcción; lo que no se puede medir aquí es el margen de CPU).
2. Un preset que cambie de motor con doce teclas apoyadas (§3.1 A): las doce deben volver, sin clics en el cruce.
3. `LOAD ECO` con las seis partes y FX a tope, mirando `SHD` en el monitor: que baje una voz por episodio y no más.
4. La fila T1..T6 con colas largas: que la barra de colas siga a lo que se oye.

## 8. Cierre

- `[HOST]` suite completa: 6 3xx `ok`, 306 golden renders sin cambios, CPU PASS en un host descargado (el primer pase
  dio "22 % más lento" con la máquina cargada por otros chats: un segundo pase, con carga 0.09, da PASS con mediana
  1,14 y 18 filas sobre +40 % anotadas, no falladas).
- `[AC791N]` imagen y `.bss` en la tabla de §5. Paquete `release/felucca-v1.5.23.fwsc`: **610 933 B**, sha256
  `a980a5f05697217e35a964b4182eed91f1f3aad50716fcd6bf2e219601b63635` (build `b46d06d`); emulador `v1.5.23-b46d06d`
  (1 284 953 B de wasm, sha256 `d5412afa39165b8c53dcf1695db02174d4b236ee5063a6a4dec92ce4a68395b5`).
- Documentación: este informe, `CHANGELOG.md`, `docs/VOICE_POOL.md` (§9, la regla del layout), `docs/BUILD_AC791N.md`.
