# Matriz de funciones — JIANT FM vs QUANTUM GAMMAWAVE

**Fecha:** 12 de noviembre de 2026 · **Fase:** 1/6 (auditoría, sin cambios funcionales)

Estado en QG:

- `SÍ` — implementado y accesible desde la FM-1 (pantalla, capa o tecla).
- `SÍ (oculto)` — implementado, funciona, pero el camino para alcanzarlo no está documentado o es un gesto no
  obvio.
- `SIN UI` — implementado en firmware y audible, sin ninguna forma de verlo/editarlo en la FM-1.
- `NO` — no existe en el árbol de QG.
- `N/A` — excluido de esta auditoría por pedido (ANIMAL, generador, emulador).

Utilidad: **A** = cambia cómo se toca o se entiende el instrumento; **B** = comodidad clara; **C** = cosmético o
de nicho.

Prioridad propuesta: **P0** = cerrar una promesa rota o exponer lo que ya existe; **P1** = el mayor valor
musical por byte; **P2** = deseable; **P3** = más adelante.

---

## 1. Navegación y estructura de la UI

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| Ciclo de páginas por familia | cadena única, sin wrap, con memoria por sección (`ui_lens.c` `nav_*`) | ciclo con wrap, memoria por familia (`ui.fam_last`, `ui.c:750-766`) | SÍ | A | P1 |
| Pestañas de grupo por sección | sí: `FX: TRK·BUS·DLY·REV·CHO·SLC·PCH·MST` (`CG_FX`) | no | NO | A | P1 |
| Breadcrumb `SECCIÓN · GRUPO · página n/N` | sí (`docs/jiant-screens.png`) | pie con `DLY 2/2` solamente | SÍ (parcial) | A | P1 |
| Salto de sección con 2 pulsaciones rápidas | sí (`open_family`, regla 2) | sí, sólo en FX (`ui.c:732-748`) | SÍ (parcial) | B | P2 |
| Memoria de página por tipo de track en EDIT | sí (`nav_key`: synth/FM6/DRUM/DIGITAL con memoria propia) | no (una sola memoria para toda la familia) | NO | B | P2 |
| Atajos `botón + SELECT` con destino fijo | sí (`nav_jump`: EDIT→SEQ, LFO→FX, GLO→tempo…) | sí, parcial (SELECT gira páginas) | SÍ | B | P2 |
| Discos/lente por sección (panel a 240×140 con mapas propios) | sí (`ui_lzfx.c`, `ui_lzedit.c`, `ui_lzpage.c`, `ui_lzsave.c`) | no | NO | B | P2 |
| Ayuda de teclas en el pie por página | sí (`khint_t`) | sí (`ui_draw.c` key prompts) | SÍ | B | — |

## 2. HOME

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| Vista de ecosistema por track | sí: 6 seres con cara, isothermas, número (`ui_lzhome.c`) | sí: sol + luna + forja + tira T1–T6 (`ui_being.c`) | SÍ (otra identidad) | A | — |
| Detalle de un track (BEING) | sí: ser grande + 4 knobs como órganos + vena | no (el `graph_being` es de EDIT, no de HOME) | NO | A | P1 |
| Vista de macros M1–M4 | sí (grupo `MACROS`) | no (sólo la capa LFO) | NO | A | P1 |
| Vista de rutas de macros (M1..M4 → destino, con etiqueta) | sí (`ROUTES`, `routes_count`) | no | NO | A | P1 |
| Visualizador a pantalla completa | sí (HUD skin) | sí (HOME→HOME, `graph_home_viz`) | SÍ | B | — |
| Monitor de CPU/voces/buses | no | sí (`GLO > MONITOR`, `GR_DIAG`) | SÍ | B | — |
| Skins de la interfaz | 4 (AIR/COLORS/BOTTOM/HUD) | estilos FLAT/LINE + paletas | SÍ (distinto) | B | — |
| Gráfico de envolvente con la etapa elegida | sí (`STAGE`) | no (ADSR fijo) | NO | C | P2 |

## 3. EDIT — estructura

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| Páginas EDIT 1 / EDIT 2 por motor | sí | sí | SÍ | A | — |
| Página FILTER por track (6 tipos en QG, 4 en JIANT) | sí | sí (con VOWEL/SCREAM extra) | SÍ | A | — |
| Página VOICE (POLY/MONO/LEG/UNI, GLIDE, MODE, PRIO) | sí | sí | SÍ | A | — |
| Página **VOICE 2** con **ALLOC** y **DETUNE** | sí | **parámetros existen y suenan (`voice.c:622,770`) sin página** | **SIN UI** | A | **P0** |
| Página VOICE 2 de QG con **M.STRT / N.RATE** | no la tiene (usa el slot para VEL) | sí | SÍ | B | — |
| Página **FOCUS** (elegir E1..E8 y apuntarle una ranura MOD) | sí (`GR_FOCUS`) | no | NO | A | P1 |
| Página **STAGE** (envolvente por etapa + LOOP) | sí (`GR_STAGE`) | LOOP está en ENV DEST; sin página de etapa | SÍ (parcial) | B | P2 |
| ROTOR de WHEEL | modo ROTOR en EDIT 2 KNOB 4 **+ página propia** con profundidad y DUST (WRDEP/WDUST) | modo ROTOR en EDIT 2 KNOB 4; sin profundidad ni DUST | SÍ (parcial) | C | P3 |
| Página **WORDS** (VOICE cantando con letra, LANG/LYRIC/POS) | sí (0.8.5, `sing.c`) | no (`sing.c` no existe) | NO | A | P2 |
| Retrato propio por motor dentro de EDIT | sí (`ui_engvis.c`: tracto vocal, colonia FM6, célula ANALOG, kit DRUM-X) | parcial (`graph_fm6`, `graph_phase`, `graph_lofi`, `graph_wheel`, `graph_colony`) | SÍ (parcial) | B | P2 |
| INIT / RECALL en la capa EDIT | sí | sí | SÍ | A | — |
| Mutes por sonido DRUM-X desde la capa EDIT | sí (8 teclas negras) | sí (`ui_layer.c`) | SÍ | A | — |
| Dado de sonido (EDIT + SELECT) con BEFORE/NOW | sí | sí (dado de sonido) | SÍ | B | — |

## 4. SCL (resumen; detalle en `JIANT_SCL_ANALYSIS.md`)

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| ROOT / SCALE / QUANT globales | sí | sí | SÍ | A | — |
| 16 escalas | sí | sí (mismas máscaras) | SÍ | A | — |
| TRANS por track | sí | sí (SCL KNOB 4) | SÍ | A | — |
| ALL TR (transposición por grados, global) | KNOB 4 de SCL | página `SCL 2` | SÍ | B | P3 (unificar) |
| 11 modos de CHORD | sí | sí | SÍ | A | — |
| Acordes correctos en escalas de 5/6/8 notas | sí (0.8.3) | **no**: apila "cada otra nota" y da C F Bb en Cm pent | NO | A | **P0** |
| VOICING CLOSE/LEAD/OPEN/+BASS | sí | sí | SÍ | A | — |
| STRUM | sí | sí | SÍ | A | — |
| Vista de piano con la escala y las notas sonando | sí (2 octavas, 24 teclas de 9 px) | sí (7+5 teclas, franja, nota sonando) | SÍ | A | — |
| Vista circular de las 12 notas con la escala unida | sí (`ui_lzpage.c:231`) | no | NO | B | P2 |
| Vista de acordes por grado (I..VII con sus notas) | sí (`graph_chord`) | no (cae al osciloscopio) | **NO** | **A** | **P0** |
| Etiqueta del grado en el teclado | sí (en la vista circular) | no (se quitó a propósito en 3.2 por ambigüedad) | NO | C | P3 |
| Escala y tonalidad guardadas por sección | sí (0.8.3) | no (global del proyecto) | NO | A | P2 |
| LED del teclado físico marcando la escala | sí (`PREF_SCALE_LEDS`) | sí | SÍ | B | — |
| SLICER y reverb RESO siguiendo la escala | sí | SLICER sí (`G_SLPQ`); RESO no (RESO usa PITCH/FOLLOW) | SÍ (parcial) | C | P3 |

## 5. FX y master

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| 4 envíos por track (DIST CHOR DLY REV) | sí | sí | SÍ | A | — |
| 6 tipos de distorsión | SOFT HARD FOLD CRUSH RECT AMP | 4 propios: SOFT TAPE HEARTH FLUX | SÍ (4) | A | — |
| Auto-pan por track (DEPTH/RATE/SYNC/WAVE) | sí | sí | SÍ | B | — |
| Delay: 60 tiempos, TYPE, PING-PONG, WOW/FLUTTER, PITCH/SPRAY, DRAW del tren de ecos | 60 tiempos; dibujo sí | 48 tiempos; dibujo sí (`tech_dly`) | SÍ | A | — |
| Reverb: 5 modelos | sí | sí, con animación por modelo | SÍ | A | — |
| Chorus o Phaser en el bus | sólo chorus | **CHORUS/PHASER** (`G_CTYPE`) | SÍ (ventaja QG) | B | — |
| Slicer en bus (GATE/STUT, 6 tracks + FX, 16 pasos CHR/SCL) | sí | sí | SÍ | B | — |
| **Página BUSES** (lente con DMIX/RSIZE/CDEPTH/SLDEP) | sí (`GR_BUSES`) | no | NO | B | P2 |
| **Página SENDS** por track con las 6 barras | sí (`GR_TRK`) | no (los envíos están en FX por track) | NO | B | P2 |
| EQ maestro paramétrico (PEAK/NOTCH/LOWSH/HIGHSH) | no | **sí, con curva** (`ui_tech.c tech_master_eq`) | SÍ (ventaja QG) | A | — |
| Ancho estéreo maestro (M/S) | no | **sí** (`G_MWIDTH`) | SÍ (ventaja QG) | B | — |
| MASTER con clipper + PUNCH + DUCK + REL | sí | sí | SÍ | A | — |
| DJ FILTER por track | sí (MIXER / LIVE MANO KNOB 4) | no | NO | B | P2 |
| **Página PUNCH** (ver y editar la lane de 64 pasos) | sí (`ui_lzfx.c`) | **la lane se graba, se guarda y se borra, pero no se ve** | **SIN UI** | A | **P0** |

## 6. Performance y capas

| Función | JIANT | QG | Estado QG | Utilidad | Prioridad |
|---|---|---|---|---|---|
| Layer FX (hold FX) con mapa de teclas | sí | sí, mapa dibujado sobre el teclado | SÍ | A | — |
| REPEAT 1/8 1/16 1/32 | sí | sí | SÍ | A | — |
| LPF / HPF momentáneos | sí (como sweeps) | sí | SÍ | B | — |
| 10 efectos MIDI (OCT±, 1/2 TEMPO, DEC±, STUTTER, ATK+, ARP, RANDOM) | sí, cuantizados al 1/16 | sí, cuantizados | SÍ | A | — |
| VIBRATO / DIVE | sí (fue STUTTER 1/16T) | **sí** (QG-1) | SÍ (ventaja QG) | B | — |
| GATE / SWEEP de master | sí (sweeps propios) | sí (C#5/D#5) | SÍ | B | — |
| 2ª banca (REVERSE, FREEZE, TAPE STOP, GLITCH, RETRIGGER) | sí, con knobs propios y 4 efectos más | **sí**, por pulsación larga de A#4 | **SÍ (oculto)** | A | **P0** (documentar/visible) |
| Latch del layer FX | sí | sí | SÍ | B | — |
| Lane punch-in grabable | sí | sí | SÍ | A | — |
| Lane punch-in **editable** | sí | no | **SIN UI** | A | **P0** |
| Macros M1..M4 latcheadas | sí | sí | SÍ | A | — |
| Dado de rutas de macro | sí (F3/G3) | sí | SÍ | B | — |
| **GLO > LIVE MANO** (board de modificadores en vivo por track) | sí (0.9.11) | no | NO | A | P2 |
| **GLO > LIVE RIELES** (6 secuencias como rieles, 4 knobs) | sí (0.9.11) | no | NO | B | P3 |
| Mutes por grupo DRUM-X | sí | sí (GLO teclas negras 7–10) | SÍ | A | — |
| Solo por track | sí | sí | SÍ | B | — |
| Undo (GLO + G4) | sí | sí | SÍ | A | — |

## 7. Secuenciador (contexto; no es el foco)

| Función | JIANT | QG | Estado QG |
|---|---|---|---|
| 64 pasos, polymeter por track, swing, gate | sí | sí | SÍ |
| Componentes de paso (ARP, TRIG, RAND, OCT, TONAL, BEND, ECHO, PULSE, JUMP, REV, FILL, KILL, WALK, CLIMB) | sí | sí (subconjunto: CP_WALK/CP_CLIMB sí) | SÍ |
| Página WHEN / TIMING / COMP 2 | sí (fusionadas en COMP/EVERY) | sí, como páginas separadas | SÍ |
| Página TRACK (PITCH, CHANCE, HUMAN, REC Q) | sí, 4 en una | repartidas en SHIFT / HUMAN / CLR AUTO | SÍ (parcial) |
| EUCLID | generador interno (`ui_tools.c tl_euclid`) expuesto como modo de página según el manual (V10.6) | sólo generador interno del dado de batería (`ui_tools.c:103`) | SÍ (interno) |
| Copy/paste de pasos | documentado en el manual (0.9.9-seq2h); no localizado por nombre en `seq.c`/`ui_input.c` en esta pasada | no encontrado en el código de QG | NO VERIFICADO |
| 4 playheads (FUGUE) | sí | sí | SÍ |

## 8. Motores

| Motor | JIANT | QG | Estado QG | Nota |
|---|---|---|---|---|
| ANALOG | 8 formas | **10 formas** (+TUBE +FOLD) | SÍ (ventaja QG) | |
| FM6 | 6 op, 32 alg, 3 páginas de operador | igual | SÍ | |
| PHASE | igual | igual + encabezado con DCW | SÍ | |
| LOFI | igual | igual | SÍ | |
| VOICE (formant) | + palabras/letra (`sing.c`, WORDS) | sólo formantes | SÍ (parcial) | |
| WHEEL | + página ROTOR | parámetros sin página | SÍ (parcial) | |
| NOISE | igual | igual | SÍ | |
| DRUM-X | igual | igual | SÍ | |
| ANIMAL | sí (0.8.3) | no | N/A / NO | excluido |
| STRING / ADD / MATH | no existen | no existen | NO | ver roadmap §5 |
| EXT (audio externo) | no (fuera de alcance declarado) | no | NO | sólo futuro |

## 9. Sistema y datos

| Función | JIANT | QG | Estado QG |
|---|---|---|---|
| 6 tracks / TRACKS 4 | sí (4/6) | sí (4/5/6) | SÍ |
| LOAD ECO/LITE/FULL | sí | sí | SÍ |
| 12 voces | 8 | **12** | SÍ (ventaja QG) |
| Curva de velocidad / aftertouch | sí, con calibración por tecla y VEL MIN | curva de velocidad (`voice.c vel_curve`, comentario QG-1 1.6 en `params.c:5`); aftertouch polifónico `P_AT` | SÍ |
| MIDI learn | sí, persistente | no | NO |
| Bluetooth MIDI / EXT SWARM | sí | no | NO |
| Diagnóstico (MONITOR) | no | **sí** (`GR_DIAG`) | SÍ (ventaja QG) |
| Backup/restore | sí | sí | SÍ |
