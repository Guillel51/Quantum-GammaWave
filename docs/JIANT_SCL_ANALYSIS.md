# Análisis de SCL — cómo lo presenta JIANT y cómo reproducirlo en QG (240×240)

**Fase:** 1/6 (auditoría) · **Fecha:** 12 de noviembre de 2026
**Prioridad:** central. Este documento es la referencia para toda la parte tonal de las fases siguientes.

---

## 1. El modelo de datos (idéntico en ambos proyectos)

Todo el sistema tonal de los dos firmwares descansa en **cuatro** parámetros por track y **una** tabla:

| Pieza | Dónde | Qué es |
|---|---|---|
| `SCALE_MASK[16]` | `seq.c:17-34` (idéntico byte a byte en JIANT y QG) | 16 máscaras de 12 bits: el bit *i* está puesto si la escala tiene la nota *i* semitonos sobre la tónica |
| `P_ROOT` (0..11) | `core.h`, `params.c` (`PD("ROOT", F_NOTE, 0, 11, 0)`) | la tónica de la escala |
| `P_SCALE` (0..15) | `N_SCALE` | cuál de las 16 |
| `P_QUANT` (0..3) | `N_QUANT` = OFF / SNAP / WHITE / SEQ | cómo se aplica la escala a lo que se toca |
| `P_TRANS` (−24..+24) | `N_*` | transposición en semitonos **de ese track** |
| `G_STRN` | global | ALL TR: desplaza las secuencias *n* grados de la escala (global, guardado con el proyecto) |

Las 16 escalas, con sus máscaras reales (`seq.c`):

| Índice | Nombre | Semitonos sobre la tónica | Notas (en C) |
|---|---|---|---|
| 0 | CHR | 0 1 2 3 4 5 6 7 8 9 10 11 | las 12 |
| 1 | MAJ | 0 2 4 5 7 9 11 | C D E F G A B |
| 2 | MIN | 0 2 3 5 7 8 10 | C D Eb F G Ab Bb |
| 3 | DOR | 0 2 3 5 7 9 10 | C D Eb F G A Bb |
| 4 | MIX | 0 2 4 5 7 9 10 | C D E F G A Bb |
| 5 | PEN | 0 2 4 7 9 | C D E G A |
| 6 | MPEN | 0 3 5 7 10 | C Eb F G Bb |
| 7 | HARM | 0 2 3 5 7 8 11 | C D Eb F G Ab B |
| 8 | PHRY | 0 1 3 5 7 8 10 | C Db Eb F G Ab Bb |
| 9 | LYD | 0 2 4 6 7 9 11 | C D E F# G A B |
| 10 | LOC | 0 1 3 5 6 8 10 | C Db Eb F Gb Ab Bb |
| 11 | MEL | 0 2 3 5 7 9 11 | C D Eb F G A B (ascendente) |
| 12 | BLUES | 0 3 5 6 7 10 | C Eb F Gb G Bb |
| 13 | WHOLE | 0 2 4 6 8 10 | C D E F# G# A# |
| 14 | DIMHW | 0 1 3 4 6 7 9 10 | C Db Eb E F# G A Bb |
| 15 | DIMWH | 0 2 3 5 6 8 9 11 | C D Eb F Gb Ab A B |

`scale_mask(t)` = `SCALE_MASK[clamp(t->p[P_SCALE], 0, 15)]` (`seq.c:149` QG / `:151` JIANT).

**Diferencia importante entre los proyectos:** `P_ROOT`, `P_SCALE` y `P_QUANT` son **globales de la canción**
en ambos ("one scale for the whole song", manual de QG §6 y de JIANT §6), aunque se editen desde la página de un
track. `P_TRANS` es por track. JIANT añadió en 0.8.3 que **ROOT y SCALE se guarden además por sección** (A–D), de
modo que una canción puede cambiar de tonalidad entre secciones; QG no lo tiene.

---

## 2. Cómo JIANT presenta la tonalidad — pantalla por pantalla

### 2.1 `SCL` (página del teclado) — `ui_lzpage.c:231-296`

Es una **lente**: ocupa el panel de 240×140 y tiene tres zonas.

```
┌────────────────────────────────────────────────────────────┐
│  ●●●  las 12 notas en círculo, C arriba, en sentido horario │   radio 42 px, centro (62,58)
│      · la escala ENCENDIDA y UNIDA por líneas               │
│      · ROOT en color de acento, el resto en color de tema   │
│      · fuera de la escala: nodo pequeño, color "raise"      │
│      · el nombre de la nota DENTRO de cada nodo encendido    │
│   en el centro:  "C" grande  +  "MIN" debajo                 │
│                                          ┌───────────────┐  │
│                                          │ QUANT   OFF   │  │  KNOB 3
│                                          │ ALL TR   +0   │  │  KNOB 4
│                                          └───────────────┘  │
│  ▁▂▁ ▂▁▂ ▁▂▁ ▂▁▂  teclado de 2 octavas desde C             │   24 teclas de 9 px, y 112/122
│  (blancas bajas 12 px, negras altas 10 px; la escala en     │
│   color de tema, la tónica en acento, el resto apagado)      │
└────────────────────────────────────────────────────────────┘
```

Detalles exactos del código, útiles para portar:

- Posiciones del círculo: `a = i*65536/12 + 0xC000`, `px = 62 + cos(a)*42/32768`, `py = 58 + sin(a)*42/32768`.
  (Trigonometría propia `og_cos`/`og_sin`, sin libm.)
- Se dibuja **primero** el polígono (las líneas entre notas consecutivas de la escala, cerrando el círculo) y
  **después** los nodos, para que las líneas queden debajo.
- El nodo de la tónica se dibuja siempre con `T_ACCENT`; las demás notas de la escala con `T_THEME`; las que no
  están, con `T_RAISE` y radio 4 en vez de 7.
- Debajo, 24 rectángulos de 9 px: `deg = (i + 24 - root) % 12`; si `deg == 0` acento, si está en la escala tema,
  si no, `T_LINE` (negra) o `T_RAISE` (blanca). Esto es **el teclado físico dibujado**, no un piano genérico.
- KNOB 3 y 4 se dibujan como "label arriba, valor abajo" a la derecha (`key_val()`).

**Por qué el círculo importa:** en 240×140 no cabe un piano de 3 octavas legible, pero sí un reloj de 12 nodos.
El círculo hace visible en un golpe de vista *qué notas tiene la escala* y *dónde está la tónica*, que es
exactamente lo que un piano de 7 teclas no puede mostrar sin mirar dos octavas.

### 2.2 `CHORD` (página de acordes) — `ui_lzpage.c:297-330`

Ésta es la pantalla que QG **no tiene**, y la que responde a la pregunta "todas las notas que componen cada
acorde" sin que el usuario toque nada.

```
┌────────────────────────────────────────────────────────────┐
│ TRIAD · CLOSE                                              │   encabezado: modo · voicing
│ ┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐                        │   7 columnas de 27 px + 2 de aire
│ │   ││   ││   ││   ││   ││   ││   │  ← 7 tarjetas de 27×100 │   x = 14 + d*31
│ │ ▬ ││ ▬ ││ ▬ ││ ▬ ││ ▬ ││ ▬ ││ ▬ │  ← una barra de 3 px    │   por nota del acorde, a altura
│ │ ▬ ││ ▬ ││   ││ ▬ ││   ││ ▬ ││   │     real: y = 112 -      │   (nota-48)*3, recortado 0..31
│ │ ▬ ││   ││   ││   ││ ▬ ││   ││ ▬ │     la raíz (la más      │   grave) en acento
│ │ I ││ II││III││IV ││ V ││VI ││VII│  ← el grado romano       │   el grado I en tema, el resto dim
│ └───┘└───┘└───┘└───┘└───┘└───┘└───┘                        │
└────────────────────────────────────────────────────────────┘
```

Cómo se calcula lo que se ve (`graph_chord`):

1. `deg[]` = los semitonos de la escala, en orden ascendente, hasta 7. (En una escala de 5 notas sólo se ven
   I..V; en las de 8, I..VII.)
2. Para cada grado *d* se llama **a la misma función que usa el motor de audio**:
   `chord_build(t, 48 + root + deg[d], nt)`. Nota 48 = C3 como base de dibujo.
3. Se dibuja una barra de 23×3 px por cada nota devuelta, en `y = 112 - clamp(nt[k]-48, 0, 31)*3`. Es decir:
   **la altura de la barra es la nota**, no un adorno. Dos acordes distintos se ven distintos.
4. La nota más grave de cada acorde va en acento; las demás en tema.
5. Si `P_CHRD == 0` el encabezado dice `CHORD OFF · ONE NOTE A KEY` y las tarjetas muestran un solo tono por grado.

**Esto es lo más valioso de todo el SCL de JIANT**: el usuario ve, antes de tocar, qué acorde va a sonar en cada
grado de su escala, con sus notas exactas. Y como llaman a `chord_build`, **no puede mentir**: es el mismo código
que suena.

### 2.3 Grados e intervalos

- **Grado**: índice dentro de las notas de la escala (0 = tónica). En escalas de 7 notas, grado 0..6 = I..VII.
- **Intervalo en grados**: `DIA[5][4]` de `chord_build` (`seq.c:232`):

  | Modo | Grados que apila | Resultado típico en MAJ |
  |---|---|---|
  | `CD_TRI` (TRIAD) | 0, 2, 4 | 1 · 3 · 5 (tríada diatónica) |
  | `CD_SEV` (7TH) | 0, 2, 4, 6 | 1 · 3 · 5 · 7 |
  | `CD_ADD9` (ADD9) | 0, 2, 4, 8 | 1 · 3 · 5 · 9 |
  | `CD_SUS2` (SUS2) | 0, 1, 4 | 1 · 2 · 5 |
  | `CD_SUS4` (SUS4) | 0, 3, 4 | 1 · 4 · 5 |

  El "grado +2" significa "la nota dos posiciones más arriba **en la escala**", no un tono. Por eso en una escala
  de 7 notas sale la tríada correcta de cada grado (C, Dm, Em, F, G, Am, Bº en C mayor).
- **Intervalos en semitonos** (las formas "en cualquier tecla", `SHP[6][4]`):

  | Modo | Semitonos |
  |---|---|
  | MAJ | 0 4 7 |
  | MIN | 0 3 7 |
  | 7 | 0 4 7 10 |
  | MAJ7 | 0 4 7 11 |
  | MIN7 | 0 3 7 10 |
  | POWER | 0 7 12 |

### 2.4 El arreglo de 0.8.3 (lo que QG no tiene)

Con la regla "cada dos notas de la escala", una escala de 5 notas produce apilamientos que no son acordes:
en Cm pentatónica (C Eb F G Bb), la "tríada" sobre C sale **C F Bb** (una cuarta y una séptima, no una tríada).

JIANT lo corrigió con 30 líneas (`seq.c:297-316`):

```c
static int32_t chord_pick(uint32_t mask, int32_t rel, const int8_t *p, uint32_t n)
{   /* el primer intervalo de la lista p que la escala tiene sobre la raíz */
    for (i = 0; i < n; i++) if ((mask >> ((rel + p[i]) % 12)) & 1u) return p[i];
    return -1;
}
/* T3: {4,3,5,2}  F5: {7,8,6}  F5M: {7,6,8}  S7: {10,11,9}  S7D: {9,10}  S9: {14,15}  S2: {2,1}  S4: {5,6} */
```

Reglas exactas:

1. La **tercera** es 4 (mayor) si la escala la tiene; si no, 3 (menor); si no, 5 (cuarta: el acorde pasa a
   suspendido); si no, 2 (segunda).
2. La **quinta** es 7 (justa); si no, si la tercera fue menor → 8 (disminuida), si fue mayor → 6 (aumentada).
3. La **séptima** es 10 (menor); si no 11 (mayor); si no 9 (sexta). Caso especial: tercera menor + quinta
   disminuida → prueba 9 antes que 10.
4. La **novena** es 14; si no 15.
5. Si la tercera no existe en absoluto, el acorde queda sólo con raíz y quinta (poder).
6. **Toda nota del acorde pertenece a la escala.** Es la invariante que hace que "todo esté en tonalidad".

Resultado declarado en el manual de JIANT: en Cm pentatónica salen Cm, Eb, Fsus4, Eb/G, Bbsus4; con 7TH sobre F,
**F7sus4**; la escala de tonos enteros da acordes aumentados; las disminuidas dan disminuidos y séptimas de
dominante.

### 2.5 Inversiones y voicings

**No hay un parámetro de inversión por acorde** (no existe "1ª inversión"). Hay tres voicings globales del track
(`P_CVOI`) que *producen* inversiones:

| Voicing | Qué hace el código | Efecto musical |
|---|---|---|
| `CV_CLOSE` (0) | nada: el acorde tal como se construyó | posición fundamental |
| `CV_LEAD` (1) | cada tono se baja/sube de octava hasta quedar a ≤6 semitonos de la **media del acorde anterior** de ese track (`chord_last[NTRK]`, `chord_nlast`) | conducción de voces: inversiones que minimizan el salto |
| `CV_OPEN` (2) | el segundo tono sube una octava (`nt[1] += 12`) | acorde abierto |
| `CV_BASS` (3) | en un acorde de 4 notas se descarta la quinta (`nt[2] = nt[3]`) y se añade la raíz una octava abajo | bajo + acorde |

Después de aplicar el voicing, el código **ordena de grave a agudo** y elimina unísonos empujando cada nota
repetida una octava arriba (`nt[i] <= nt[i-1] → nt[i] = nt[i-1] + 12`), recorta a 0..127 y guarda el resultado
como "el último acorde" para el próximo LEAD.

### 2.6 Progresiones armónicas

**Ninguno de los dos firmwares tiene "progresiones" como objeto.** No hay una lista de grados, ni un
sequenciador de acordes, ni un campo de progresión en el proyecto. Lo que hay es:

- **En ejecución:** el usuario toca acordes con CHORD activo; cada tecla es el grado de su nota. Una progresión
  es *lo que el usuario toca*, y se graba al secuenciador como los acordes resultantes (una nota de acorde por
  paso, hasta 4 notas por paso — `step_t.note[4]`). Eso es lo que después se reproduce.
- **En el generador de canciones (fuera de alcance):** JIANT usa "familias de armonías" y QG usa su propio
  generador; los dos eligen progresiones por dentro. Ninguno expone la progresión como dato editable.

Conclusión honesta para el roadmap: si se quiere **progresiones como entidad** (I–V–vi–IV guardable y
transponible), eso es una función nueva que **no existe en JIANT** y hay que diseñarla desde cero en QG. Ver
`QG_EDIT_EXPANSION.md` §6 como propuesta opcional (P2).

### 2.7 La cadena completa: tonalidad → escala → acorde → notas

```
ROOT (global) ─┐
SCALE (global) ─┴→ scale_mask(t) ──→ deg[] = semitonos de la escala
                                        │
tecla / nota MIDI (n) ──────────────────┤
                                        ├→ chord_build(t, n, out)  ── si P_CHRD == 0: out = {n}
P_CHRD (modo)  ─────────────────────────┤       usa DIA[] (grados) o SHP[] (semitonos)
P_CVOI (voicing) ───────────────────────┤       aplica OPEN / LEAD / BASS
P_CSTR (strum) ─────────────────────────┘       ordena, quita unísonos, recorta
                                        │
                                        ├→ keys_strum(): primera nota ya, el resto cada chord_gap()
                                        │                (P_CSTR * FS/2000 ≈ 0.5 ms por paso → hasta ~63 ms)
                                        ├→ key_next() → trk_note_on(): cada nota del acorde es una nota normal
                                        └→ grabación: rec_note() por nota, en el paso donde bajó la tecla
```

Consecuencias que el usuario ve:

- **QUANT** actúa *antes* del acorde: con SNAP, la tecla se corrige a la escala y el acorde se construye sobre la
  nota corregida.
- Una tecla **fuera de la escala** se corrige hacia abajo (`while (!(mask & …)) r--;`) hasta la nota de la escala
  más cercana *por debajo*. Con CHR se usa la mayor de ROOT.
- Si el track está en MONO, LEGATO o UNISON, o el motor es de un solo disparo (DRUM), `chord_build` devuelve
  **sólo la raíz**. Es una decisión explícita en la primera línea de la función.
- El ARP arpegia las notas del acorde: el acorde entra al arp como varias teclas.

---

## 3. Lo que QG ya tiene hoy (para no rehacer nada)

- Página **SCL** con ROOT / SCALE / QUANT / TRANS (`params.c`, `{"SCL", FAM_SCL, SC_TRACK, GR_SCALE, …}`).
- **`graph_scale`** (`ui_graph.c:691-790`, rediseñado en QG-1 1.7/3.x): piano de 7 blancas (29 px) y 5 negras
  (18 px) desde x=12, con franja de 2 px en la tapa de cada tecla (acento en la tónica, tema en la escala),
  nombre de nota en las blancas, y **barra de acento en la tecla cuando esa nota está sonando** (`live`).
  En el pie del panel: `TRANS ±n` y `CHORD ON/OFF`.
- Página **CHORD** con `P_CHRD` / `P_CVOI` / `P_CSTR`, y **todo `chord_build` funcionando** (con la regla antigua
  de "cada dos notas de la escala").
- Página **SCL 2** con ALL TR.
- LEDs del teclado físico marcando la escala (`PREF_SCALE_LEDS`, `MI_SCLLED`).
- STRUM, LEAD, +BASS y el registro del acorde al secuenciador: todo igual que JIANT.

Lo que falta: **(a)** la construcción correcta en escalas de 5/6/8 notas; **(b)** la vista de acordes por grado;
**(c)** opcionalmente el círculo de 12 notas; **(d)** opcionalmente key por sección.

---

## 4. Propuesta para QG: cómo reproducir y ampliar la experiencia en 240×240

### 4.1 Geometría disponible en QG (medida en el código, `ui.c:176-189`)

| Zona | y | alto | uso |
|---|---|---|---|
| Header | 0 | 28 | play, BPM, track, octava, medidor, batería |
| Cards (KNOB 1–4) | 28 | 44 | nombre + valor + barra, 4 columnas |
| Panel | **76** | **122** | el gráfico de la página (`Y_GRAPH`, `H_GRAPH`) |
| Footer | 202 | 38 | pistas de teclas + nombre de página `SCL 1/3` |
| LARGE | 136 | 62 | con MENU > LARGE el panel se achica a 62 px |

El panel útil para gráficos es **232 × 122** (x 4..236). Cualquier vista nueva debe leerse ahí **y** sobrevivir a
`LARGE` (62 px), o declarar que no se dibuja en LARGE (como ya hacen las páginas con `LK_LABEL`).

### 4.2 Pantalla nueva A — `SCL · CHORD` con diagrama por grado (la que más valor tiene)

Es un port casi directo del `graph_chord` de JIANT, con la geometría de QG.

```
y=76  ┌──────────────────────────────────────────────────────────┐
      │ TRIAD · CLOSE                          KEY C MIN   ◀ 1/3 │  línea 1 (9 px): modo · voicing · root/scale
      ├──────────────────────────────────────────────────────────┤
      │  ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐                            │  7 columnas de 28 px, 4 de aire: x 8..228
      │  │  ││  ││  ││  ││  ││  ││  │                            │  tarjeta 28×84, y 90..174
      │  │▬ ││▬ ││▬ ││▬ ││▬ ││▬ ││▬ │  barras de 4 px de alto,   │  y = 172 - (nota-45)*4  (32 semitonos)
      │  │▬ ││▬ ││  ││▬ ││▬ ││▬ ││  │  a altura real de la nota  │  la nota más grave en acento
      │  │▬ ││  ││  ││  ││  ││  ││▬ │                            │
      │  │I ││II││II││IV││V ││VI││VI│  grado romano              │  I en tema, el resto dim
      │  └──┘└──┘└──┘└──┘└──┘└──┘└──┘                            │
      ├──────────────────────────────────────────────────────────┤
      │ B2 D#3 G3   ◀ K1 GRADO   K2 MODO   K3 VOICING   K4 STRUM │  pie del panel: las notas del grado elegido
y=198 └──────────────────────────────────────────────────────────┘
```

Decisiones de diseño para QG:

1. **Base y rango.** JIANT dibuja desde C3 (48) con 32 semitonos de ventana. En 84 px de alto, con 4 px por
   semitono caben 21 semitonos. Propuesta: ventana de **21 semitonos desde C3** y, si un acorde se sale, se
   recorta y se marca el borde con una línea de 1 px (no se deforma la escala de alturas).
2. **Columnas.** 7 columnas de 28 px con 4 de aire = 228 px, cabe en 232. En escalas de 5 notas se dibujan 5
   columnas más anchas (44 px) para no dejar hueco; en las de 8, 8 de 27.
3. **El grado elegido** (`knob 1`) se resalta con el fondo `T_TINT` (el mismo que usa JIANT para `d == 0`) y sus
   notas se escriben en el pie del panel, como texto ("B2 D#3 G3") — así el usuario lee las notas *con nombre*,
   que es lo que un diagrama de barras no dice.
4. **Tocar el acorde.** `OCT+` en esta página toca el acorde del grado elegido mientras se mantiene; `SET` lo
   fija como acorde latcheado (para escuchar progresiones sin teclado externo). Esto **no existe en JIANT** y es
   barato: es una llamada a `chord_build` + `trk_note_on` desde la UI, el mismo camino que `key_next`.
5. **KNOB 2/3/4** = los tres parámetros reales (`P_CHRD`, `P_CVOI`, `P_CSTR`), no cursores. El cursor es KNOB 1.
   Así la pantalla es "lo que suena" y no un editor de mentira.

**Coste estimado** (a confirmar en Fase 3 con build real):

| Recurso | Estimación | Base |
|---|---|---|
| Flash de código | 1,4–2,0 KB | ~40 líneas de dibujo con `cv_rect`/`cv_text_on` + 2 tablas de 7 bytes; comparar con `graph_scale` (~100 líneas ≈ 2,5 KB de código XIP) |
| RAM | **0 B de `.bss`** si el grado elegido se guarda en un `static uint8_t` de la UI (como `mod_ui_slot`, `ms_ui_step`) | `params.c` ya usa ese patrón para los cursores de página |
| CPU | sólo en el redibujo de la página; sin trabajo por bloque de audio | no toca el ISR |
| Riesgo | el lint de `ui_render` rechaza texto fuera de su celda: hay que declarar cada barra y cada texto con su caja | `tests/ui_render.c` |
| Prueba | `ui_render` (17 paletas × FLAT/LINE, 0 hallazgos), `ui_test` (navegación a la página), y un test nuevo de `chord_build` con las 16 escalas × 12 raíces × 11 modos: toda nota pertenece a la escala (invariante de 0.8.3) | ver §4.5 |

### 4.3 Pantalla nueva B — el círculo de 12 notas

Dónde ponerlo sin quitarle sitio al piano: **no en `SCL`**, donde el piano ya funciona y está pulido, sino como
**segunda mitad de la página `SCL`**: el panel de 122 px se parte en `círculo de 56 px de radio a la izquierda
(x 6..124)` y `piano compacto de 2 octavas a la derecha (x 128..232)`. El círculo aporta lo que el piano no:
relación entre tónica y grados, y la escala "unida" como forma.

```
┌───────────────────────────────┬─────────────────────────┐
│        ●  ●                    │  C  C# D  D# E F F# …   │
│    ●        ●   ← escala unida │  ▓  ░  ▓  ░  ▓ ▓ ░  …   │  22 teclas de 4,5 px
│   ●    C    ●     ROOT acento  │  ▓  ░  ▓  ░  ▓ ▓ ░      │  (2 octavas) con franja
│    ●   MIN  ●     escala tema  │                         │
│        ●  ●                    │  QUANT OFF   ALL TR +0  │
└───────────────────────────────┴─────────────────────────┘
```

Alternativa si se prefiere una sola vista: **el círculo sustituye al piano sólo cuando `P_QUANT != 0`**, porque
es cuando importa ver qué notas entran. No lo recomiendo: introduce un cambio de layout por estado, que es
justo lo que el lint de QG castiga.

**Valor:** medio (B). **Prioridad:** P2. **Coste:** ~1 KB flash, 0 RAM, sin riesgo de audio.

### 4.4 Controles físicos — el mapa completo propuesto

QG tiene 14 botones (FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+), 7 encoders
(SELECT ALGORITHM PRESETS + KNOB 1..4) y 27 teclas.

| Gesto | En `SCL` (piano) | En `SCL · CHORD` (nuevo) |
|---|---|---|
| `SCL` (toque) | vuelve a la página | siguiente página de la familia |
| `SCL` sostenido | capa: cualquier tecla fija ROOT; KNOB 1..4 = ROOT SCALE QUANT TRANS | igual, y **SET fija el grado como acorde latcheado** |
| `KNOB 1` | ROOT | **grado** (I..VII) |
| `KNOB 2` | SCALE | modo de acorde |
| `KNOB 3` | QUANT | voicing |
| `KNOB 4` | TRANS | strum |
| `SELECT` | camina páginas de SCL | camina páginas de SCL |
| `OCT-` / `OCT+` | octava del teclado | octava de la base de dibujo; `OCT+` sostenido **toca el acorde** |
| teclas | tocan notas (con QUANT), dibujan en vivo | tocan acordes del grado de cada tecla |
| `EDIT` | — | si el cursor está en un grado, **añade el acorde al paso del secuenciador** (mismo camino que `rec_note`) |

Nada de esto agrega parámetros al proyecto: sólo usa `P_CHRD`/`P_CVOI`/`P_CSTR` y estado de UI.

### 4.5 Cambios de datos necesarios

| Cambio | Necesario | Detalle |
|---|---|---|
| Arreglo de `chord_build` para escalas de 5/6/8 notas | **sí** | +30 líneas en `seq.c`, 6 tablas `static const int8_t` de 2–4 bytes y una función de 4 líneas. **No cambia el formato de proyecto**: los acordes grabados son notas, no grados |
| Estado de cursor del grado | sí, mínimo | `static uint8_t chord_ui_deg;` en `params.c` (junto a `mod_ui_slot`, `ms_ui_step`, `sl_ui_step`) o un campo de `page_over`; **0 B de `.bss`** (ya hay precedente) |
| Key por sección (0.8.3) | opcional | requiere guardar ROOT+SCALE en el bloque de la sección: hay 33 bytes libres en `PROJ_PFX_OFF` ya usados por la lane; requiere un campo nuevo ⇒ **cambio de formato** ⇒ P2 y con test de compatibilidad |
| Progresiones como entidad | no ahora | ver `QG_EDIT_EXPANSION.md` §6 |

### 4.6 Pruebas exigibles a la entrega de SCL

1. **Test nuevo `tests/chord_scale_test.c`** (host): para las 16 escalas × 12 raíces × 11 modos × 4 voicings:
   - toda nota devuelta por `chord_build` pertenece a la máscara de la escala (la invariante de 0.8.3);
   - `n` entre 1 y 4 (o 5 con +BASS), ordenado ascendente, sin unísonos, todas en 0..127;
   - en escalas de 7 notas y `CD_TRI`, la tríada coincide con la diatónica (comparar contra una tabla de
     referencia escrita a mano para C mayor: C, Dm, Em, F, G, Am, Bº);
   - en MONO/LEGATO/UNISON/DRUM devuelve 1 nota (regresión de la primera línea).
2. **`tests/regress.c`**: los renders dorados **no** deben cambiar (el arreglo sólo afecta a escalas ≠ 7 notas;
   verificar que ningún preset dorado usa PEN/MPEN/BLUES/WHOLE/DIM\* con CHORD on; si alguno lo hace, el cambio de
   dorado se documenta en el commit).
3. **`tests/ui_render.c`**: la página nueva en 17 paletas × FLAT/LINE, 0 lint y 0 alineación; y una captura de
   referencia en `build/ui_new`.
4. **`tests/ui_test.c`**: navegación SCL → SCL 2 → CHORD → vuelta; que KNOB 1 no escriba en el proyecto.
5. **`docs/MANUAL.md` + `MANUAL.es.md`**: tabla de páginas de SCL actualizada (hoy lista `SCL 2` y `CHORD` sin
   vista).

---

## 5. Resumen de la propuesta SCL

| Entrega | Contenido | Valor | Coste | Prioridad |
|---|---|---|---|---|
| S1 | Arreglo de `chord_build` para escalas de 5/6/8 notas (port de la idea 0.8.3, re-implementada sobre QG) | **A** | ~40 líneas, 0 RAM | **P0** |
| S2 | Página `SCL · CHORD` con diagrama por grado + notas con nombre en el pie | **A** | 1,4–2,0 KB flash, 0 RAM | **P0** |
| S3 | Grado seleccionable con KNOB 1, tocar/fijar acorde con OCT+/SET, grabar al paso con EDIT | B | ~0,6 KB flash | P1 |
| S4 | Círculo de 12 notas junto al piano en `SCL` | B | ~1 KB flash | P2 |
| S5 | ROOT y SCALE por sección (cambio de formato) | A | formato + tests de compat | P2 |
| S6 | Progresiones como entidad (nuevo, no existe en JIANT) | A | diseño nuevo | P3 |
