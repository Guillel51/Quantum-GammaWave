# QG — Plan de HOME y de navegación (240×240)

**Fase:** 1/6 (auditoría) · **Fecha:** 12 de noviembre de 2026

---

## 1. HOME en QG hoy (leído en el código)

| Pieza | Dónde | Comportamiento |
|---|---|---|
| Estado | `ui.home`, `ui.home_viz` (`ui.c`) | `home` = HOME normal; `home_viz` = visualizador a pantalla completa |
| Entrada | `go_home()` (`ui.c:799-810`), botón HOME | desde otra página: HOME; estando ya en HOME: **alterna `home_viz`** y dispara `home_pulse` |
| Salida | cualquier botón de familia (`open_family`), SELECT, menú | `open_family` limpia `home` y `home_viz` |
| Menú | HOME **sostenido** | abre el MENU (`ui_menu.c`) |
| KNOB 1..4 | `home_param()` (`ui.c:1403`) | los **cuatro parámetros principales del motor** (`ENGINES[e]->knob[0..3]`, normalmente `E1..E4`). Son los mismos valores que EDIT 1, editables desde HOME |
| Gráfico | `graph_ecosys()` / `graph_being()` / `graph_home_viz()` (`ui_being.c`) | tres bahías: curva de respuesta del filtro sobre espectro de 28 barras; "Hearth" (sol + luna + flares); 4 diales + tira T1–T6 |
| Rediseño | `ui_being.c` (91 KB), v2.7.4.1 | planetas reactivos, polvo, nebulosa, ribbon Wraith, forja |
| Diagnóstico | `GLO > MONITOR` (`GR_DIAG`, `ui_tech.c`) | CPU, voces, buses |

**Lo que HOME no puede hacer hoy:** ver el sonido de un track que no sea el seleccionado; ver qué mueve cada
macro; ver el detalle de un track sin ir a EDIT; llegar a ninguno de esos lugares sin cambiar de track primero.

---

## 2. La referencia JIANT, en lo que se puede reutilizar

JIANT convirtió HOME en una **cadena de 7 vistas en 3 grupos** (`ui_lens.c` `CE_HOME`/`CG_HOME`):

```
HOME ─ grupo ECO    : ECO · MACROS
     ─ grupo BEING  : BEING · ROUTES
     ─ grupo GEN    : GEN 1 · GEN 2 · GEN 3        (generador: fuera de alcance)
```

- Se navega con el **mismo mecanismo que las páginas**: HOME entra, HOME otra vez avanza al grupo siguiente,
  SELECT camina la cadena, el pie dice `HOME · ECO · ECO   1/7`.
- **ECO** = los 6 seres nadando (equivalente al `graph_ecosys` de QG).
- **BEING** = el ser del track grande en el centro, con sus 4 knobs de HOME dibujados como **órganos en las
  esquinas**, cada uno con su gauge, su nombre, su valor y una vena al cuerpo; el nombre del sonido debajo.
- **MACROS** = M1..M4 con su valor, en cards.
- **ROUTES** = el ser pequeño a la izquierda y **M1..M4 a la derecha con sus rutas dibujadas y rotuladas**
  (`T1 CUT+40`), contadas por `routes_count()`.
- En las skins BOTTOM/HUD los seres se dibujan **por debajo** del chrome (header, pestañas, knobs, breadcrumb).

Lo que QG **no debe copiar**: el estilo de los "beings" (es la identidad de JIANT), ni la cantidad de vistas, ni
el generador.

---

## 3. Propuesta de HOME para QG: tres vistas, un solo gesto nuevo

### 3.1 Las tres vistas

| Vista | Qué muestra | De dónde sale el contenido |
|---|---|---|
| **SHIP** (actual) | las tres bahías tal como están hoy | `graph_ecosys` — **sin tocar** |
| **TRACK** (nueva) | el track seleccionado en grande: nombre del motor y del sonido, medidor de nivel grande, los 4 parámetros de HOME con su nombre y valor alrededor, y el `graph_being` del motor en el centro | `graph_being()` ya existe (hoy se usa en EDIT); `home_param()` da los 4 valores |
| **ROUTES** (nueva) | M1..M4 y, por cada una, sus hasta 4 destinos con track, destino y cantidad (`T3 CUT +40`), más el total de rutas | `mod.c` (`P_M*SRC/DST/AMT`), `N_MSRC`/`N_MDST` ya tienen los nombres |

Se descarta por ahora una cuarta vista "MACROS" separada: sus valores ya están en la capa LFO latcheada y en
ROUTES; agregar una vista cuesta y no agrega información nueva.

### 3.2 El gesto

Hoy: HOME estando en HOME → `home_viz` (visualizador). Propuesta: HOME estando en HOME → ciclo
**SHIP → TRACK → ROUTES → VIZ → SHIP**.

- Es **la extensión natural de un gesto que ya existe**, no un gesto nuevo.
- No roba ningún botón ni ninguna combinación.
- El pie ya muestra `HOME` en la esquina (`ANALOG ACID ... HOME`): pasa a `HOME · SHIP 1/4`, `HOME · TRACK 2/4`, etc.
- Tres puntos de 2 px bajo el nombre de la página indican cuál vista (coste: 3 rectángulos).

**Coste del gesto:** un `uint8_t` de estado. No hay campo libre garantizado en `struct ui`: usar el mismo truco
que el resto del árbol — un campo existente con bits altos libres, o un `static uint8_t home_view` en `ui.c`
(0 B de `.bss` nuevo, es `.data` de 1 byte como `mac_latch`). Preferencia: **`static uint8_t home_view;`**.

### 3.3 Mockups (240×240, coordenadas reales)

**TRACK** — reutiliza el alto completo del panel (76..198) más la zona de cards:

```
y=0   ┌──────────────────────────────────────────────────────────┐
      │ 1 ▶ 124   T1 · ANALOG ACID            OCT+2   ▮▮▮  🔋  │  header (28 px, sin cambios)
y=28  ├──────────────────────────────────────────────────────────┤
      │                    ┌──────────────────┐                  │
      │   WAVE            │                  │           CUT    │  los 4 parámetros de HOME
      │   SAW             │   el ser del     │           355 Hz │  en las esquinas, con su
      │   ▁▁▁▁▁▁▁         │   motor (graph_   │        ▁▁▁▁▁▁    │  nombre, valor y barra
      │                   │   being), 80 px  │                  │
      │                   │                  │                  │
      │   RES             │   ANALOG / ACID  │          ATK     │  nombre del sonido debajo
      │   79 %            │   ▮▮▮▮▮▮▮▮  (nivel)│          1.0 ms  │
      │   ▁▁▁▁▁▁          └──────────────────┘          ▁▁▁▁    │
y=198 ├──────────────────────────────────────────────────────────┤
      │  ● ○ ○ ○   K1 WAVE  K2 CUT  K3 RES  K4 ATK               │  pistas de teclas (footer)
      │ ANALOG   ACID                              HOME · TRACK 2/4│
y=240 └──────────────────────────────────────────────────────────┘
```

**ROUTES** — 4 filas de 26 px en el panel:

```
y=76  ┌──────────────────────────────────────────────────────────┐
      │ M1  ──▶ T1 CUT +40    T2 DLY +18    T4 AMP -12    3 rutas│  fila de 24 px por macro
      │ M2  ──▶ T3 REV +55                                1 ruta │  macro a la izquierda,
      │ M3  ──▶ —                     (sin ruta)                │  destinos en texto,
      │ M4  ──▶ T1 PITCH +7   T1 VIB +30    T5 SHP +22    3 rutas│  contador a la derecha
      ├──────────────────────────────────────────────────────────┤
      │ K1 M1 VALOR   K2 M2 VALOR   K3 M3   K4 M4                │  los 4 knobs siguen siendo
      │ 64            12            0       100                  │  los de HOME (no las macros)
y=198 └──────────────────────────────────────────────────────────┘
```

Nota de diseño: en ROUTES **los knobs no se convierten en M1..M4**. QG ya tiene ese comportamiento en la capa
LFO latcheada, y duplicarlo en HOME crearía dos formas de mover lo mismo con reglas distintas. Si se quiere
mover macros desde HOME, se hace con LFO sostenido, como hoy.

### 3.4 Coste y riesgos

| Recurso | Estimación | Nota |
|---|---|---|
| Flash | **1,5–2,2 KB** (`graph_home_track` ~35 líneas, `graph_home_routes` ~45 líneas, el ciclo de vistas y los 3 puntos) | ambos usan primitivas ya existentes (`cv_rrect`, `cv_text_*`, `gw_plasma_bar`, `graph_being`) |
| RAM | **1 byte** (`home_view`) | |
| CPU | sólo al redibujar; ROUTES lee `trk[t].p[P_M*]`, sin recorrer nada por bloque | |
| Riesgo 1 | `ui.home_viz` deja de ser el segundo estado: hay código que asume `home_viz == 1` al volver (`ui.c:789-795`, `ui.c:1928-1934`, `ui_input.c:50`, `ui_draw.c:2015`). Revisar todos los usos antes de tocar el ciclo | |
| Riesgo 2 | El `ui_render` compara pantallas: HOME pasa de 1 pantalla a 4 (× 17 paletas × FLAT/LINE ⇒ ~136 capturas nuevas). Es trabajo de referencias, no de riesgo funcional | |
| Riesgo 3 | ROUTES debe rotular destinos con los nombres **reales** de `N_MDST` (22 destinos en QG, no 28 como JIANT) | |
| Prueba | `ui_render` (4 vistas × 17 paletas), `ui_test` (que el ciclo de vistas no cambie `song.sel` ni ningún parámetro), `particle_bounds_test.py` si SHIP se anima | capturas de referencia nuevas por vista |

---

## 4. Navegación: qué falta de verdad en QG

### 4.1 Estado actual, con números

| Familia | Páginas | Cómo se recorre hoy |
|---|---|---|
| ENV | 2 | ciclo |
| LFO | 5 | ciclo |
| FX | **16** | ciclo + 2 pulsaciones rápidas = salto de sección (`ui.c:714-748` `fx_head`/`open_family`) + pie con `DLY 2/2` |
| SCL | 3 | ciclo |
| EDIT | **25** (visible según motor) | ciclo, sin secciones |
| GLO | 3 (+MIXER) | ciclo con entrada fija al MIXER |
| SAVE | 5 | ciclo con entrada fija a USER |
| ARP | 2 | ciclo |
| SEQ | **17** | ciclo, con `page_visible` ocultando 2 |

El pie ya rotula la familia y su posición (`ui_draw.c`): p. ej. **`SCL 1/3`**, como se ve en la captura
`assets/screenshots/quantum_15_scales_chords.png`.

### 4.2 Lo que JIANT tiene y QG no

1. **Grupos con nombre por sección** (pestañas) — el usuario ve en qué grupo está y cuántas páginas tiene.
2. **Breadcrumb `SECCIÓN · GRUPO · página  n/N`** — una sola línea de texto que responde "dónde estoy".
3. **Memoria de página por tipo de track en EDIT** (synth / FM6 / DRUM) — hoy QG tiene una sola memoria para
   toda la familia, así que cambiar de un FM6 a un DRUM y volver deja una página que puede no existir.
4. **Salto de grupo con doble toque** — QG lo tiene sólo en FX.

### 4.3 Entrega propuesta, en tres pasos independientes

**N1 — Breadcrumb en el pie (barato, sin layout nuevo).**

- Contenido: `<SECCIÓN> · <GRUPO> · <página>  n/N`, sustituyendo el actual `FAMILIA n/N`.
- Requiere una tabla de grupos: `static const uint8_t PAGE_GRP[NPAGES]` (79 B de const en flash) con 0 = sin grupo.
- Coste: **~250 B de flash**, 0 RAM, 0 px de layout (usa la línea del pie que ya existe).
- Riesgo: el pie ya está ajustado al píxel por el lint de alineación; una cadena más larga puede recortarse.
  Mitigación: abreviaturas por grupo (tabla de 8 caracteres) y `...` sólo si no entra.
- Prueba: `ui_render` (0 hallazgos de texto cortado), `text_ref_test`/`text_spacing_test` si aplican.

**N2 — Grupos en EDIT, SEQ y FX (mismo mecanismo en las tres).**

- Tabla `PAGE_GRP` de N1 + un `open_group(fam, dir)` que salta a la primera página del grupo siguiente, atado al
  **doble toque** del botón de la familia (hoy sólo FX lo tiene).
- Grupos propuestos para EDIT (según motor, como JIANT): `SOUND` (EDIT 1/2) · `TONE` (FILTER, MORPH, RETRIG,
  VOICE, VOICE 2) · `KIT` (KIT 3, X-MOD, STEREO) · `SOUND 1..4` (DRUM) · `OPS` (FM6: OPERATOR, OP ENV, OPERATOR 2).
- Grupos para SEQ: `PATT` (STEP, PATTERN, STRETCH, PLAYHEADS) · `TRACK` (SHIFT, HUMAN) · `BAR` (NOTE, TIMING) ·
  `STEP` (COMP, COMP 2, WHEN) · `AUTO` (AUTOMATION, AUTO LIST, MUTATE).
- Coste: **~700 B de flash** (lógica) + 0 RAM.
- Riesgo: cambiar el significado del doble toque en familias donde hoy no hace nada. Es aditivo, no rompe nada.
- Prueba: `ui_test` con un recorrido por cada familia comprobando que el doble toque visita exactamente los
  grupos en orden y vuelve al primero.

**N3 — Pestañas dibujadas (sólo si N1 y N2 se validan).**

- Una tira de 7 px bajo el header con los nombres de los grupos y un punto por página.
- **Cuesta alto del panel**: 122 → 115 px, o del header, o de las cards (44 → 40). Cualquiera de las tres
  obliga a repasar `ui_render` completo y las 4 vistas de HOME.
- Coste: **~1,2 KB de flash**; RAM 0.
- Recomendación: **hacerla sólo para EDIT y SEQ** (con `LK_LABEL`, que ya reduce el panel), y **no** en HOME
  (donde los puntos de 2 px bastan).

**N4 — Memoria de página por tipo de track en EDIT.**

- Hoy: `ui.fam_last[FAM_EDIT]` (1 byte). Propuesta: 3 bytes indexados por `edit_kind()` (synth / FM6 / DRUM).
- Coste: **2 B de `.bss`** (hay 2 216 B de holgura; es trivial) + ~80 B de código.
- Riesgo: el struct `ui` está alineado y algunos campos se guardan en settings; `fam_last` no se guarda. Verificar.
- Prueba: `ui_test` — cambiar EDIT en un FM6 a la página OPERATOR, ir a un DRUM, volver al FM6 y comprobar que
  vuelve a OPERATOR.

### 4.4 Lo que **no** recomiendo portar

| Idea de JIANT | Por qué no |
|---|---|
| Cadena de páginas **sin wrap** con tope en la primera y la última | QG usa wrap y sus usuarios lo conocen; el cambio es puramente de gusto y rompe memoria muscular |
| 4 skins completos (AIR/COLORS/BOTTOM/HUD) | QG tiene FLAT/LINE + paletas y un rediseño reciente aprobado; agregar skins multiplica por 4 el costo de cada página nueva en `ui_render` |
| Lentes por sección (`ui_lzfx.c`, `ui_lzedit.c`, `ui_lzpage.c`) con un panel propio por sección | es un rediseño visual completo de 4 archivos; contradice "no rediseñar el emulador/HOME" y no es lo que falta funcionalmente |
| SELECT con destino fijo por familia (`nav_jump`) | QG usa SELECT para caminar páginas y para el generador en HOME; reasignarlo rompe el manual y el reflejo |

---

## 5. Resumen

| Entrega | Contenido | Valor | Coste flash | RAM | Prioridad |
|---|---|---|---|---|---|
| H1 | Vista HOME · TRACK (ser + 4 parámetros) | A | ~1 KB | 1 B | P1 |
| H2 | Vista HOME · ROUTES (M1..M4 y sus destinos) | A | ~1,2 KB | 0 | P1 |
| N1 | Breadcrumb con grupo en el pie | A | ~250 B | 0 | P1 |
| N2 | Salto de grupo con doble toque en EDIT/SEQ/FX | B | ~700 B | 0 | P2 |
| N4 | Memoria de página por tipo de track en EDIT | B | ~80 B | 2 B | P2 |
| N3 | Pestañas dibujadas en EDIT y SEQ | B | ~1,2 KB | 0 | P3 |
