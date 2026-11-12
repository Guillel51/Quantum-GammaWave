# JIANT FM vs QUANTUM GAMMAWAVE — Auditoría comparativa (Fase 1/6)

**Fecha:** 12 de noviembre de 2026
**Objeto:** auditoría profunda de dos firmwares para el mismo hardware (M-VAVE FM-1, AC791N/WL82, 240×240)
centrada en **EDIT, SCL, HOME, motores de síntesis, FX y edición sonora**.
**Fuera de alcance (por pedido explícito):** el rediseño del emulador de QG, el generador de canciones de ambos
proyectos y el motor ANIMAL de JIANT.

---

## 0. Método y límites de la evidencia

Qué se hizo:

- Se clonó JIANT (`github.com/juanjiant-bit/JIANT`, `main`, V10.7.17) en `audit_external/JIANT/` (ignorado por git,
  no forma parte del árbol de QG) y se leyó su firmware completo (112 archivos `.c/.h`), su manual
  (`docs/MANUAL.md`, 3000+ líneas), su changelog, sus documentos de diseño (`FELUCCA-TONIC-SPEC.md`,
  `docs/TONIC-UI.md`, `docs/TONIC-AUDIT.md`) y sus capturas (`docs/jiant-screens.png`,
  `docs/navigation-map.png`, `docs/drumx-screen.png`, `docs/cheatsheet-*.png`).
- Se auditó el árbol de QG en su estado real (`main`, `FELUCCA_VERSION "v2.7.4.8"`, `firmware/src/felucca.c:41`).
- **No se compiló ni se ejecutó ninguno de los dos firmwares en esta fase.** Toda afirmación sobre
  comportamiento es lectura de código y de documentación, no medición. Donde un documento de los repos
  contradice al código, se señala y se toma el código como fuente.

Límites que esta auditoría **no** puede cerrar:

| Pregunta | Por qué no se puede cerrar acá |
|---|---|
| CPU real en el AC791N | No hay unidad física ni build de target en esta sesión (`JIELI_TOOLCHAIN`/`AC79_SDK` ausentes) |
| Latencia, calor, comportamiento de encoders/LEDs | Requiere hardware |
| ¿El paquete publicado corresponde a este árbol? | Requiere verificar hashes del release, no es auditoría de código |
| Coste exacto de cada propuesta | Sólo hay presupuesto medido del árbol v1.9.0 (`docs/MEMORY_BUDGET.md` §2.1); las cifras nuevas de este informe son **estimaciones razonadas**, marcadas como tales |

Presupuesto de referencia de QG (medido, `docs/MEMORY_BUDGET.md` §2.1, **no re-medido acá**):

| Región | Medido | Presupuesto | Holgura |
|---|---|---|---|
| Imagen de app | 531 204 B | 581 632 B | **50 428 B** |
| `.data + .bss` | 96 088 B | 98 304 B | **2 216 B** |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 444 B | 24 576 B | **5 132 B** |
| Pool spare | 9 248 B | ≥ 8 192 B | **1 056 B** |

Conclusión de arranque: **en QG la flash de código no es el cuello de botella; la RAM sí.** Cualquier propuesta
que pida estado nuevo en `.bss` compite por 2 216 B. JIANT ya pagó ese peaje antes (su `docs/TONIC-AUDIT.md` §2b
documenta 7 084 B libres en `.bss` cuando tenía 4 tracks; hoy tiene 6).

---

## 1. Linaje y punto de divergencia

Los dos proyectos son el mismo instrumento con el mismo linaje declarado:

```
Felucca (Hügelton) → Pocket GammaWave (GW-1, @fvalente) → JIANT FM (@kurogedelic + Juan Jiant)
                                    ├── JIANT FM  V10.7.17   (internamente "v0.9.18", felucca.c:41)
                                    └── QUANTUM GAMMAWAVE v2.7.4.8 (felucca.c:41, marca "QG-1")
```

Evidencia del tronco común y del punto de corte (todo leído en ambos árboles):

| Rasgo | JIANT | QG | Lectura |
|---|---|---|---|
| `SCL > CHORD` con `chord_build` (JIANT 0.6.81) | sí (`seq.c:230`) | sí (`seq.c:230`) | tronco común ≥ 0.6.81 |
| `DLY 3 > TYPE/PING` (0.6.90) | sí | sí | tronco común ≥ 0.6.90 |
| Punch-in lane `pfx.c` (0.6.99) | sí, **con página FX > PUNCH** | sí la lane, **sin página** | QG cortó antes de 0.7.6 |
| Destinos de modulación de buses (0.7.0: `D.TIM D.FB D.TON …`) | sí (28 destinos) | no (22 destinos) | QG cortó antes de 0.7.0 |
| `SCL` KNOB 4 = **ALL TR** en la página del track (0.7.6) | sí (`params.c` `ID_ALLTR`) | no: `SCL 2` como página aparte | QG cortó antes de 0.7.6 |
| Motor ANIMAL (0.8.3, engine 14) | sí | no | divergencia posterior |
| Lente `ui_lens.c` (navegación por cadenas, 0.9.2) | sí | no | divergencia posterior |

**Conclusión:** el fork ocurrió alrededor de **JIANT 0.6.9x**. Desde ahí JIANT profundizó en *control* (páginas,
navegación, performance, voz cantada, análisis tonal) y QG profundizó en *carácter DSP y presentación* (modelos de
distorsión propios, filtros VOWEL/SCREAM, 10 formas de onda ANALOG, EQ maestro paramétrico, ancho M/S, 5 modelos
de reverb con animación propia, HOME planetario, monitor de diagnóstico). **No son versiones una de la otra: son
dos ramas del mismo árbol con prioridades distintas.** Por eso "portar" JIANT a QG nunca es copiar: es re-derivar
la función sobre el código de QG.

---

## 2. Arquitectura comparada

| Constante | JIANT | QG | Consecuencia |
|---|---|---|---|
| Tracks/partes (`NPART`) | 6 (`core.h:13`) | 6 (`core.h:88`) | igual |
| Voces (`NVOICE`) | **8** (`core.h:12`) | **12** (`core.h:58`, subido en 3.8) | QG tiene 50 % más polifonía; JIANT prioriza latencia/estabilidad |
| Pasos (`NSTEP`) | 64 (`core.h:15`) | 64 (`core.h:90`) | igual |
| Motores en la tabla (`NENGINES`) | 15 (0..14, con ANIMAL) | 14 (0..13) | QG no tiene ANIMAL |
| Motores ofrecidos (`NENG_SHOWN`) | 9 | 8 | QG: ANALOG FM6 PHASE LOFI VOICE WHEEL NOISE DRUM |
| Páginas en `PAGES[]` | **82** | **79** | ver desglose |
| Páginas FAM_EDIT | 28 | 25 | JIANT suma FOCUS, WORDS, VOICE 2 (ALLOC/DETUNE) y ROTOR (4); QG suma VOICE 2 (M.STRT/N.RATE) y STEREO, y no tiene los otros tres: 28 = 25 + 4 − 1 |
| Páginas FAM_SCL | 2 | 3 | QG separa `SCL 2`; JIANT mete ALL TR en SCL K4 |
| Páginas FAM_ENV | 3 | 2 | JIANT añade STAGE |
| Páginas FAM_LFO | 6 | 5 | JIANT añade LFO 2 (PHASE) |
| Páginas FAM_GLO | 4 | 3 | JIANT añade MANO y RIELES (LIVE) — cuenta 4 por MIXER como FAM_TRK |
| Páginas FAM_TRK | 2 | 1 | JIANT añade SENDS |
| Páginas FAM_FX | 16 | 16 | igual en cantidad, distinto contenido (BUSES vs MASTER EQ, PUNCH vs STEREO M/S) |

Familias de botón (idénticas en ambos): `ENV LFO FX SCL EDIT GLO SAVE ARP SEQ` + `FAM_TRK` (MIXER).

---

## 3. EDIT — la diferencia más grande entre los dos proyectos

### 3.1 Cómo se llega hoy a EDIT en cada uno

**QG** (leído en `ui.c:441` `page_visible`, `params.c:560-600`, `ui_layer.c`):

- Botón EDIT → recorre en ciclo sus 25 páginas visibles. El orden es el de la tabla, filtrado por
  `page_visible()`: `EDIT 1, EDIT 2, RETRIG*, MORPH*, KIT 3*, X-MOD*, OPERATOR*, OP ENV*, OPERATOR 2*, FILTER,
  FILTER (otros), SOUND*, SOUND 2..4*, LEVELS*, LEVELS 2*, STEREO*, OP1..OP4 ENV (sólo DIGITAL), OP LEVEL,
  VOICE, VOICE 2 (M.STRT/N.RATE)` (`*` = condicionada por motor/ajuste).
- SELECT gira las páginas de la familia; una pulsación de EDIT avanza a la siguiente; la capa (EDIT sostenido)
  da INIT, RECALL, mutes de los 8 sonidos DRUM-X y KNOB 1-4 = EDIT 1.
- **No hay grupos, ni pestañas, ni imágenes propias por motor salvo tres excepciones**: `graph_fm6`,
  `graph_lofi`, `graph_phase`, `graph_wheel`, `graph_colony` (DRUM) y —para todo lo demás— **`graph_being()` o el
  osciloscopio** (`ui_graph.c:3266-3280`).

**JIANT** (leído en `ui_lens.c`, `params.c`, `docs/navigation-map.png`):

- EDIT ya no es un ciclo plano: es una **cadena con grupos y pestañas**. Para un sintetizador
  (`CE_SYN`, `ui_lens.c`):

  ```
  grupo SOUND   : EDIT 1 · EDIT 2
  grupo TONE    : FILTER · MORPH · RETRIG · ROTOR · VOICE · VOICE 2 · WORDS
  grupo FOCUS   : FOCUS
  ```
  Para FM6 (`CE_FM6`): `SOUND (EDIT 1, EDIT 2)` · `TONE (FILTER, VOICE, VOICE 2)` · `OPS (OPERATOR, OP ENV,
  OPERATOR 2)`. Para DRUM-X (`CE_DRM`): `KIT · SOUND · LEVELS · TONE` con 13 entradas.
- El pie de pantalla dice **`EDIT · TONE · WORDS   6/7`** (sección · grupo · página + posición en la cadena), y
  arriba hay pestañas con un punto por página (`docs/jiant-screens.png`, panel `EDIT > WORDS`).
- La página **FOCUS** es la pieza que QG no tiene: elige un parámetro del motor (PARAM = E1..E8), lo edita
  (VALUE) y **le apunta una ranura de la matriz de modulación** (SRC, AMT) sin salir de EDIT
  (`params.c` `{"FOCUS", FAM_EDIT, SC_TRACK, GR_FOCUS, …}`, `ui_input.c focus_knob`).
- La página **STAGE** hace lo mismo con la envolvente: elige STAGE (ATK DEC SUS REL), su valor y LOOP.

### 3.2 Lo que cada motor deja editar

Las tablas `EDIT 1..8` son **idénticas byte a byte entre los dos proyectos** en casi todos los motores. Diff
real (leyendo `eng_*.c` de ambos):

| Motor | Parámetros EDIT 1..8 | Diferencia JIANT vs QG |
|---|---|---|
| ANALOG | WAVE DETUNE MIX NOISE / CUT RES DRIVE INT | **QG tiene 10 formas (0..9, +TUBE +FOLD); JIANT 8 (0..7)** |
| PHASE | WAVE WAVE2 DCW ENV / DETUNE LINE SUB FB | idéntico |
| LOFI | CHIP WAVE DUTY CRUSH / CUT VIB BEND MASK | idéntico (+ páginas RETRIG y MORPH) |
| VOICE (FORMANT) | VOWEL VOWEL2 TALK SHIFT / BUZZ BREATH Q RAND | **JIANT añade página WORDS (LANG LYRIC POS) y canto con formantes/fonemas (`sing.c`); QG no tiene `sing.c`** |
| WHEEL | REG SUB BODY TOP / PERC CLICK DRIVE ROTOR | JIANT añade página **ROTOR** (WRDEP, WDUST) |
| NOISE | MODE COLOR FREQ PEAK / TRK DENSE DRIFT CRUSH | idéntico |
| FM6 | ALG FB MOD RATIO / MEG VMOD DETUNE SLOT | idéntico (+ páginas OPERATOR / OP ENV / OPERATOR 2 en ambos) |
| DRUM-X | kit + 4 páginas SOUND + LEVELS + STEREO/X-MOD/KIT 3 | QG: idéntico; JIANT ordena STEREO entre KIT y SOUND y suma VOICE al grupo TONE |
| ANIMAL | (JIANT 0.8.3, MODE PRESS CHAOS SIZE / SWEEP RATE GROUP TEMP) | **excluido por pedido** |

### 3.3 El hallazgo duro: en QG hay parámetros que existen y suenan, pero no están en ninguna pantalla

Leído en el árbol de QG:

| Parámetro | Definido | Leído por el DSP | Página en la FM-1 | Dónde se edita hoy |
|---|---|---|---|---|
| `P_ALLOC` (ALLOC ROT/REUSE) | `core.h:150`, `params.c:106` | `voice.c:622` | **ninguna** | sólo el editor web (`web/editor.html:2231`) |
| `P_DETUNE` (UNISON detune) | `core.h:150`, `params.c:107` | `voice.c:770` | **ninguna** | sólo el editor web; el generador lo escribe |
| Punch-in lane (64 pasos × 4 compases, `pfx_lane[32]`) | `perform.c:225`, `pfx.c:26` | `pfx.c`, `seq.c` | **ninguna** (sólo FX+G5 borra) | **no se puede ver ni editar**, sólo grabar en vivo y borrar |
| 2ª banca del layer FX (`PF_REV FRZ TAPE GLCH RTRG`) | `perform.c:34`, `ui_layer.c FX_BANK2` | `perform.c:305` | **sí, pero oculta**: larga pulsación de A#4 | no está en `docs/MANUAL.md` (documentación atrasada) |

JIANT resolvió exactamente los dos primeros casos: `VOICE 2` (ALLOC, DETUNE) es una página real
(`{"VOICE 2", FAM_EDIT, …}`) y `FX > PUNCH` es una página real con gráfico (`ui_lzfx.c`, entrada `PUNCH` en
`CE_FX` con grupo `PCH`).

**Éste es el punto de mayor retorno inmediato de toda la auditoría**: hay función ya implementada, ya guardada en
proyecto y ya audible, que el usuario de QG no puede alcanzar. No requiere DSP nuevo.

### 3.4 Visualización del motor dentro de EDIT

| Aspecto | JIANT | QG |
|---|---|---|
| Retrato por motor | `ui_engvis.c` (~129 KB): VOICE = tracto vocal animado (lengua por F1/F2, velo, glotis), DRUM-X = colonia de bichos que se aplastan al golpe, ANALOG = célula polar con CUT/RES/DRIVE, FM6 = colonia de 6 operadores cableados según el algoritmo (`fm6_routes`) | `graph_being()` (criatura genérica de HOME) + `graph_fm6` (chart de algoritmo), `graph_phase`, `graph_lofi`, `graph_wheel`, `graph_colony` |
| Envolvente | página ENV con STAGE; el ENV se dibuja con la etapa elegida | `graph_adsr` clásico |
| Coste | declarado "sólo lectura del estado que la UI ya tiene; nada en el camino de audio" | igual criterio |

No recomiendo portar `ui_engvis.c` completo (es el archivo más caro de JIANT en complejidad, y QG ya tiene su
propia identidad visual): ver `QG_EDIT_EXPANSION.md` §4 para la versión reducida propuesta.

---

## 4. SCL — resumen ejecutivo

El análisis completo está en **`docs/JIANT_SCL_ANALYSIS.md`**. Adelanto de la comparación:

| Pieza de SCL | JIANT | QG |
|---|---|---|
| ROOT / SCALE / QUANT globales | sí | sí |
| 16 escalas (`SCALE_MASK`, `seq.c:17`) | idénticas | idénticas |
| TRANS por track | sí | sí (KNOB 4 de SCL) |
| ALL TR (todo el proyecto ±14 grados) | KNOB 4 de SCL (0.7.6) | página `SCL 2` |
| CHORD (11 modos: 5 diatónicos + 6 formas) | sí | sí |
| Construcción de acordes en escalas de 5/6/8 notas (0.8.3: intervalos pedidos o el más cercano) | **sí** (`seq.c`, ~30 líneas extra) | **no**: usa "cada otra nota de la escala" y produce apilamientos impares (Cm pent: C F Bb) |
| VOICING CLOSE/LEAD/OPEN/+BASS | sí | sí |
| STRUM | sí | sí |
| Vista gráfica de la escala | `graph_key`: **círculo de 12 notas** con la escala encendida y unida, ROOT en acento, teclado de 2 octavas debajo, nombre y grado (`ui_lzpage.c:231-296`) | `graph_scale`: **piano de 7 blancas + 5 negras** con franja de escala y nota sonando (`ui_graph.c:691-790`) |
| Vista gráfica de acordes | `graph_chord`: **un acorde por grado (I..VII)**, con las notas de cada uno como barras a altura real (`ui_lzpage.c:297-330`) | **ninguna**: la página CHORD cae al osciloscopio (`ui_tech.c:882`, default) |
| Relación escala→acorde→notas en pantalla | sí, en la misma pantalla de CHORD | no existe |
| Escala con LEDs del teclado físico | sí (`MI_SCLLED`, PREF_SCALE_LEDS) | sí (`MI_SCLLED`, `PREF_SCALE_LEDS`) |
| Escala por sección (0.8.3) | sí: ROOT y SCALE se guardan con cada sección y la canción los aplica | no: global del proyecto |

**El hueco de QG es la vista de acordes y la vista circular.** El resto del modelo tonal ya está.

---

## 5. HOME

| Aspecto | JIANT | QG |
|---|---|---|
| Modelo | **cadena con grupos** (`CE_HOME`, `ui_lens.c`): `ECO` · `MACROS` → `BEING` · `ROUTES` → `GEN 1..3` | dos estados: `ui.home` (HOME normal) y `ui.home_viz` (visualizador a pantalla completa al tocar HOME estando en HOME, `ui.c:802-807`) |
| HOME normal | 6 seres ("beings") nadando libres por toda la pantalla, cada uno con **cara** (ojos que se cierran al golpe, boca que se abre con el nivel), isothermas por nivel, número debajo, el seleccionado grande; `eco_step`/`eco_being`/`eco_draw` (`ui_lzhome.c`) | 3 bahías: curva de respuesta del filtro sobre espectro de 28 barras; sol "Hearth" con luna orbitando; 4 diales de los knobs de HOME + tira T1–T6 con barras (`ui_being.c`: `gw_draw_forge_bay`, `gw_draw_system`, `gw_plasma_bar`) |
| Vista por track | **BEING**: el ser grande en el centro con sus 4 knobs como órganos en las esquinas y una vena a cada uno; nombre del sonido debajo (`graph_home_being`) | `graph_being(t)`: la criatura genérica del motor en el panel de la página (no es una vista de HOME sino el gráfico de EDIT) |
| Vista de macros | **MACROS** (M1..M4 con su valor) y **ROUTES**: el ser pequeño a la izquierda y M1..M4 con sus rutas dibujadas y etiquetadas ("T1 CUT+40") | las macros existen (capa LFO, latcheada) pero **no hay vista de rutas** en HOME |
| Pantalla completa | HUD skin: los seres **por debajo** de header, pestañas, fila de knobs y breadcrumb (`hud_home()`) | visualizador a pantalla completa (scope armónico + Lissajous), con toggle HOME→HOME |
| Skins | 4 (AIR, COLORS, BOTTOM, HUD) × todas las paletas | estilos FLAT/LINE + paletas; el rediseño reciente es la identidad de QG |

Lectura: **QG tiene mejor HOME "escénico"** (planetas, forja, ribbons) y **JIANT mejor HOME "instrumental"**
(ver el sonido de cada track como un ser, ver qué hace cada macro, y una vista de detalle por track). Son
complementarios, no rivales. Ver `docs/QG_HOME_NAVIGATION_PLAN.md`.

---

## 6. Motores de síntesis — estado real de QG

Ocho motores implementados y ofrecidos (`engines.c` `ENGINE_ORDER`), con estas diferencias de carácter frente a
JIANT:

| Motor | Estado QG | Nota |
|---|---|---|
| ANALOG | completo, **más amplio que JIANT**: 10 formas (SAW SQR TRI SIN PWM SYNC RING SAW3 TUBE FOLD) | el par TUBE/FOLD es aporte QG |
| FM6 | completo (6 operadores, 32 algoritmos, páginas OPERATOR/OP ENV/OPERATOR 2, banco `fm6_vbank.c`) | igual a JIANT |
| PHASE | completo, con pantalla propia (`graph_phase`, `v2.7.4.7` añadió DCW al encabezado) | igual a JIANT |
| LOFI | completo (BYTE/FLOAT/WRAM/PCM/PULSE, RETRIG, MORPH) | igual a JIANT |
| VOICE (FORMANT) | completo como **formantes**: VOWEL/VOWEL2/TALK/SHIFT, Q, RAND | **sin palabras**: no hay `sing.c`, no hay LYRICS, no hay WORDS. JIANT sí (0.8.5) |
| WHEEL | completo (16 registros de barrido, rotor) | JIANT añadió página ROTOR (WRDEP/WDUST) |
| NOISE | completo (4 modos incl. LFSR y META) | igual a JIANT |
| DRUM-X | completo (8 lanes, 16 pads, morph A/B, colony, image estéreo) | igual a JIANT |

Motores **retirados pero con número reservado** en QG (`engines.c`): 1 DIGITAL (se compila con `FELUCCA_FM4=1`),
4 SAMPLE, 6 TRIO, 8 GRAIN, 9 PHYS, 13 SLICE. La tabla es append-only por los formatos de proyecto: **no se pueden
reutilizar esos índices** para STRING/ADD/MATH.

### 6.1 STRING, ADD y MATH: viabilidad (detalle en `docs/QG_IMPLEMENTATION_ROADMAP.md` §5)

- **STRING** (cuerda pulsada / Karplus-Strong extendido): cabe en RAM si se reutiliza un buffer existente. La
  línea de delay del master ya ocupa 32–64 KB; el `sl_buf` del slicer 32 KB es **compartido** con `perform.c`
  (REPEAT/REV/FRZ/TAPE/GLCH). Un KS de 6 voces a 44.1 kHz necesita como máximo ~2 000 muestras por voz si se
  limita el registro grave por voz (o una sola línea por track con repitch). Estimación: 8–16 KB si se hace por
  track con una línea corta y decimada; **0 B de `.bss` nuevo** sólo si se toma prestado `sl_buf` cuando el
  slicer está OFF (precedente exacto: `perform.c` ya lo hace). Riesgo: colisión con el slicer.
  JIANT no tiene STRING: **no hay implementación de referencia en ninguno de los dos**. Es diseño propio.
- **ADD** (aditivo por tablas de armónicos): JIANT tampoco lo tiene. Coste típico: 8–16 parciales × seno por
  tabla + suma = ~3–6 instrucciones por parcial. Con 6 voces y 12 parciales ya roza el presupuesto de render
  (725 µs/bloque de 32 muestras). Viabilidad: **sí, con 8 parciales y 1 voz por track**, o con tabla precalculada
  por nota (más flash, menos CPU). Ver §5 del roadmap para números.
- **MATH** (operadores lógicos/bitwise sobre señal, tipo "bytebeat" aplicado a señal): el precedente real es LOFI
  BYTE, que ya es bytebeat (`eng_lofi.c`). Un motor MATH "puro" sería un LOFI sin chip emulation, con más
  operadores. Coste bajo (ya existe el modelo). Riesgo: solapamiento conceptual con LOFI.
- **EXT** (entrada externa / audio USB entrante por los FX): **sólo como posibilidad futura**. JIANT tiene
  Bluetooth MIDI y `EXT › SWARM` (melodía por proximidad) pero **no** audio externo por FX; su propio spec lo
  declara fuera de alcance por falta de evidencia (`FELUCCA-TONIC-SPEC.md`, "FUERA DE ALCANCE"). No hay ruta de
  audio entrante demostrada en el AC791N de la FM-1.

---

## 7. FX, master y modulación

| Pieza | JIANT | QG |
|---|---|---|
| Páginas FX | 16, con **BUSES** (lente de los buses: DMIX RSIZE CDEPTH SLDEP), **PUNCH** (lane), MASTER | 16, con **MASTER EQ** (biquad paramétrico QG-1 v1.4) y **STEREO M/S** (ancho maestro) |
| Distorsión | 6 tipos: SOFT HARD FOLD CRUSH RECT AMP | 4 tipos propios: SOFT TAPE HEARTH FLUX |
| Filtro por track | 4 tipos: LP HP BP COMB | **6 tipos: LP HP BP COMB VOWEL SCREAM** |
| Delay | hasta 60 tiempos libres (1.67–5.90 s con `DLY_LEN 65536`) | 48 tiempos libres (hasta 1.48 s), TYPE ANALOG/DIGITAL, PING-PONG, WOW/FLUTTER, PITCH/SPRAY |
| Reverb | 5 modelos (ROOM SPRING SHIMR RESO CLOUD) | 5 modelos, con animación propia por modelo (v2.7.4.4–2.7.4.8) |
| Chorus | CHORUS (ensemble) | CHORUS **o PHASER** (`G_CTYPE`, QG-1) |
| Slicer | uno en un bus, GATE/STUT, 6 tracks + FX, 16 pasos de pitch (CHR/SCL) | idéntico |
| Punch-in FX | 5 audio (REPEAT ×3, GLITCH, TAPE STOP) + 10 MIDI + sweeps CRUSH/THROW + FREEZE + SHUFFLE; **lane editable en pantalla** | 5 audio (REPEAT ×3, LPF, HPF) + 10 MIDI + VIBRATO/DIVE/GATE + 2ª banca (REV FRZ TAPE GLCH RTRG); **lane sin pantalla** |
| Macros | M1..M4 con página y mapa de rutas; dado de rutas | M1..M4 latcheadas, sin mapa de rutas |
| Destinos de modulación | 28 (incluye los globales de los buses y de los efectos) | 22 (sin los globales de buses) |
| Master | clipper + PUNCH (transientes) + DUCK + release; DJ FILTER por track | clipper + PUNCH + DUCK + release; EQ paramétrico; ancho M/S |

**El hueco funcional más visible de QG en FX no es DSP: es la página PUNCH y el mapa de rutas de macros.**

---

## 8. Performance

| Herramienta | JIANT | QG |
|---|---|---|
| Capa FX | sí, con 2ª banca, sweeps, glitcher con 4 knobs propios, FREEZE granular, cuantización al 1/16 | sí, con 2ª banca oculta, latcheo, VIB/DIVE |
| Lane punch-in | grabable **y editable** (página PUNCH) | grabable, borrable, **no visible** |
| LIVE (MANO/RIELES) | **sí (0.9.11)**: 6 secuencias en paralelo con modificadores en vivo (rate /2 x2, TR, DT, LOOP 8/4/2/1, REV, PING, RATCH x2/x3, THIN, OCT±, RETURN, LATCH, GIRO, RND) y COMMIT al patrón | no |
| Mutes | por track, por grupo DRUM-X, por sonido DRUM-X | por track (GLO), por grupo DRUM-X, por sonido (EDIT) |
| Solo | sí (GLO + F3..D4) | sí (GLO) |
| Teclas con velocidad/aftertouch | sí, calibración por tecla, curvas y VEL MIN (V10.6z7) | sí, curva TOUCH > VEL CURVE (QG-1 1.6) y `P_AT` polifónico |
| ANIMAL / EXT SWARM | sí | excluido / futuro |

---

## 9. Síntesis de la comparación

1. **Modelo tonal:** QG y JIANT comparten todo el motor de escala/acorde. QG está atrás en *presentación* del
   acorde y en la construcción de acordes en escalas no heptatónicas.
2. **EDIT:** QG tiene los mismos parámetros por motor, pero **una navegación plana** y **tres cosas sin pantalla**
   (ALLOC, DETUNE, punch-in lane). JIANT tiene navegación por cadenas/grupos y páginas FOCUS/STAGE.
3. **HOME:** QG gana en escena, JIANT gana en información instrumental (rutas de macros, detalle por track).
4. **FX:** paridad en DSP, con acentos distintos; QG gana en master (EQ, M/S) y filtros; JIANT gana en
   herramientas de performance y en la lane editable.
5. **Motores:** QG tiene paridad o ventaja excepto en VOICE-con-palabras (JIANT) y ANIMAL (excluido).
6. **Lo que QG puede ganar más rápido:** exponer lo que ya tiene (ALLOC/DETUNE, punch-in lane), la vista de
   acordes de SCL, y el mapa de rutas de macros. Nada de eso necesita DSP nuevo.

---

## 10. Riesgos comunes a cualquier port

| Riesgo | Por qué importa en QG | Mitigación |
|---|---|---|
| `.bss` casi lleno (2 216 B) | toda página nueva con estado propio (p. ej. un cursor por familia) come de ahí | usar estado ya existente (`ui.fam_last`, bits libres de `ui_prefs`) o `.noinit` |
| Regresión visual | QG mide la UI con `tests/ui_render.c` (lint + alineación + 17 paletas); JIANT mide con `ui_lens`/`align` | toda página nueva debe pasar `ui_render` en FLAT y LINE, todas las paletas |
| Renders dorados | QG tiene 220 renders dorados (`tests/golden.txt`); cualquier cambio de DSP los mueve | no tocar DSP en las entregas de UI; si se toca, actualizar dorados con justificación |
| Formato de proyecto | `P_COUNT` es contrato de los stores ("new common parameters go just before P_E0", `core.h:154-156`) | no agregar parámetros comunes; usar bits libres o campos ya reservados |
| Copiar código de JIANT | licencias compatibles (GPL-3.0-only ambos) pero **los árboles han divergido** | portar ideas y estructura, re-implementar sobre el código de QG; acreditar en el commit |

---

## 11. Verificación que esta auditoría sí y no hizo

**Sí:** lectura íntegra de las tablas `PAGES[]` de ambos, de las tablas de motores, de `seq.c` (`SCALE_MASK`,
`chord_build`), de `params.c` (`N_SCALE`, `N_QUANT`, descriptores), de `ui_lens.c`, `ui_lzhome.c`,
`ui_lzpage.c`, `ui_engvis.c`, `ui_layer.c`, `perform.c`, `pfx.c`, `engines.c`, `ui_graph.c`, `ui_tech.c`,
`ui_being.c`; manuales y changelogs de ambos; capturas de ambos; documentos de estado de QG
(`MEMORY_BUDGET.md`, `SYNC_1.7.md`, `AUDIT_1.7_INTEGRAL.md`, `PROJECT_STATUS.md`, `ROADMAP.md`).

**No:** no se compiló, no se ejecutó el firmware, no se midió CPU ni RAM nueva, no se probó en hardware.

**Contradicciones documentadas encontradas (para corregir en Fase 2):**

| Documento | Dice | El código dice |
|---|---|---|
| `docs/SYNC_1.7.md` (última sección) | "Reverse/Freeze/Tape Stop are not implemented" | `perform.c:34,305` los implementa desde 1.8 (`PF_REV PF_FRZ PF_TAPE`) |
| `docs/MANUAL.md` §3 (capas FX) | A#4 = elegir tracks | además, **larga pulsación de A#4 cambia de banca** (`ui_layer.c`) |
| `docs/MANUAL.md` tabla EDIT | no lista VOICE 2 | existe (`M.STRT`/`N.RATE`) |
| `docs/MANUAL.md` §10 | 22 destinos | correcto (22); JIANT tiene 28 — no es error, es divergencia |
| `ROADMAP.md` | EQ "Recommended for Next Development Cycle" | ya implementado (página MASTER EQ, `ui_tech.c tech_master_eq`) |
