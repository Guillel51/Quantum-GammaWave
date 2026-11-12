# QG — Auditoría de motores y expansión de EDIT

**Fase:** 3/6 · **Fecha:** 12 de noviembre de 2026
**Fuente:** el código de `firmware/src` en `main` (v2.7.4.8). Cada lista de parámetros de abajo está copiada del
inicializador `engine_t` de su motor (`ENG_*.edit`, `engines.c`/`eng_*.c`), no deducida del manual.

## 0. Qué está verificado y qué no

| Afirmación | Clase de evidencia |
|---|---|
| Los 8 parámetros de cada motor, su formato, rango y valor por defecto | **Leído del código** (`ENG_ANALOG.edit`, …): exacto |
| Qué parámetros lee el DSP de verdad | **Leído del código** (`eng_*.c`, `voice.c`), con la línea citada |
| Páginas que los exponen | **Leído del código** (`params.c` `PAGES[]`, 84 páginas) |
| Coste en flash/RAM de una mejora nueva | **Estimación**, salvo el presupuesto global de `docs/MEMORY_BUDGET.md` §2.1 |
| CPU | **No medido**: no hay `JIELI_TOOLCHAIN` en este entorno, no hay build de target ni unidad |
| Que un cambio suene como se espera | **No medido**: sin build de target no hay `tests/regress.c` sobre el binario, sólo host |

Presupuesto de referencia (medido en `docs/MEMORY_BUDGET.md` §2.1, **no re-medido aquí**): imagen 531 204 B /
581 632 B (holgura 50 428 B); `.data+.bss` 96 088 / 98 304 (**holgura 2 216 B**); RAM de código 19 444 / 24 576;
spare del pool 9 248 B (piso 8 192).

Controles disponibles en el instrumento, iguales para todos los motores: **4 knobs** (uno por card), **27 teclas**,
**encoders y botones de sección** (EDIT/FX/SEQ/SCL/GLO/ENV/LFO/ARP/SAVE), **OCT±**, **MENU > LARGE**, y lo que
exista en `PAGES[]` para ese motor. La matriz de modulación tiene **4 ranuras** con 22 destinos, y hay **un LFO**
por track (`FAM_LFO`). No hay knob por parámetro: **todo lo que no esté en una página es inalcanzable**.

---

## 1. ANALOG — `eng_analog.c`

**Páginas:** `EDIT 1` (E0..E3), `EDIT 2` (E4..E7), `FILTER` (`TYPE CUT RES DRV`), `MORPH`, `VOICE`, `VOICE 2`.

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | WAVE | enum (10) | 0..9 | 0 | `analog_render`, `ft`/`wave` (`eng_analog.c:100,138`) |
| E1 | DETUNE | int | 0..127 | 10 | `cents_inc(...)` del osc 2 (`:101,148`) |
| E2 | MIX | % | 0..127 | 64 | `m2 = p[P_E2]*258`, `m1 = 32767-m2` (`:102`) |
| E3 | NOISE | % | 0..127 | 0 | `nz = p[P_E3]*200` (`:102`) |
| E4 | CUT | cutoff | 0..127 | 90 | `cut = (p[P_E4]<<8) + m->cutoff + …` (`:103`) |
| E5 | RES | % | 0..127 | 30 | `tsvf_coef(&flt, cut, p[P_E5])`, y `kd` (`:106,102`) |
| E6 | DRIVE | % | 0..127 | 0 | `drive = 32768 + p[P_E6]*512` (`:103`) |
| E7 | INT | semitonos | −24..24 | 0 | `cents_inc(m->pitch16 + p[P_E7]*16, …)` del osc 2 (`:101,150`) |

**Lo que el DSP hace y no se puede tocar:**

- **El seguimiento de teclado del filtro es una constante**: `cut += (v->pitch16 − 60·16) · 4` (`:103,142,199`).
  Era el knob **KTR** de JIANT 0.4 y QG lo reemplazó por **INT** (el comentario de `eng_analog.c:415` lo dice).
  Consecuencia audible: en los agudos el filtro no se abre lo que el oído espera, y no hay forma de graduarlo.
- **El filtro es un SVF de 2 polos** con `TYPE` LP/HP/BP (+3 variantes): no hay paso-banda de 4 polos ni
  pendiente seleccionable. `tsvf_coef` es compartido (`dsp.c`), así que tocar la pendiente afecta a todos los
  motores que usan el mismo filtro.
- **El osc 2 sólo existe por DETUNE/INT**: es el mismo `wave`, no una segunda forma. `MIX` mezcla los dos.
- **La envolvente de filtro no tiene cantidad propia**: entra por la matriz (`QG_DST_CUT`), con 4 ranuras
  compartidas con todo lo demás.
- `AW_SAW3` fuerza el osc 2 al intervalo interno (`:148`), así que en esa forma `INT` no hace lo que dice.

**Limitaciones DSP:** 12 voces para todo el instrumento; un segundo oscilador por voz ya está; añadir un tercero
duplicaría el coste del bloque más caro sin RAM nueva (los osciladores son de fase, no de tabla).

**Candidato de mayor valor:** devolver **KTR** como parámetro real (`CUT` sigue siendo el centro; `KTR` gradúa
cuánto sigue al teclado, 0..127 con 64 = el 50 % de hoy). Coste real: 3 líneas en `eng_analog.c` (4 sitios que
calculan `cut`), 1 parámetro común nuevo + 1 página o una card reasignada. Ver §9 para el plan completo.

---

## 2. FM6 — `eng_fm6.c`

**Páginas:** `EDIT 1` (E0..E3), `EDIT 2` (E4..E7), `OPERATOR`, `OPERATOR 2`, `OP ENV`, `OP1..OP4 ENV`, `OP LEVEL`,
`FILTER`, `MORPH` (no: FM6 no está en MORPH), más los 32 algoritmos dibujados.

| # | Parámetro | Formato | Rango | Def | Nota |
|---|---|---|---|---|---|
| E0 | ALG | enum (33) | 0..32 | 0 | algoritmo; el gráfico lo dibuja |
| E1 | FB | int | −7..7 | 0 | realimentación del operador |
| E2 | MOD | ±% | −64..63 | 0 | índice de modulación (microsteps, `:431`) |
| E3 | RATIO | int | −16..16 | 0 | ratio del modulador |
| E4 | MEG | ±% | −64..63 | 0 | envolvente del modulador |
| E5 | VMOD | int | −7..7 | 0 | modulación por velocidad |
| E6 | DETUNE | % | 0..127 | 0 | |
| E7 | SLOT | int | 0..(OWN+BANK) | 0 | **no es un parámetro de sonido**: elige el patch |

**Lo que no se expone:** las 4 envolventes ADSR por operador **sí** están (`OP1..OP4 ENV`, `OP ENV`), pero
**no hay página de los 4 niveles a la vez con el algoritmo dibujado**: `OP LEVEL` es texto. Tampoco hay control
del *modo* de cada operador ni del `FP_TRNSP`/`fine` del patch (son del patch, no del track).

**Limitación DSP:** 6 operadores con 32 algoritmos; el coste ya está medido (`tests/target_budget.py`). Un
operador extra no cabe en el formato de patch (`FM6_PATCH`), y cambiarlo rompe presets.

**Candidato:** `OP LEVEL` con el chart (no toca DSP, sólo UI) — valor medio.

---

## 3. PHASE — `eng_phase.c`

**Páginas:** `EDIT 1`, `EDIT 2`, `MORPH`, `FILTER`, `VOICE`, `VOICE 2`.

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | WAVE | enum (8) | 0..7 | 0 | `phase_render` |
| E1 | WAVE2 | enum (9) | 0..8 | 0 | la segunda forma, para el morphing |
| E2 | DCW | % | 0..127 | 60 | `depth = (p[P_E2]<<8) + m->cutoff + env·p[P_E3]` (`:134`) |
| E3 | ENV | % | 0..127 | 64 | cantidad de envolvente sobre el filtro (`:134`) |
| E4 | DETUNE | int | 0..127 | 0 | |
| E5 | LINE | enum (2) | 0..1 | 0 | lineal / exponencial |
| E6 | SUB | % | 0..127 | 0 | suboscilador |
| E7 | FB | % | 0..127 | 0 | realimentación de fase |

**Nota:** PHASE es el motor con **envolvente de filtro propia** (E3), el único además de NOISE con seguimiento
de teclado en el cutoff (vía `m->cutoff`). **No hay nada obvio sin exponer**: de los 8 motores es el que está más
completo en su espacio de parámetros.

**Candidato:** ninguno de DSP. Sólo el gráfico (`graph_phase` ya existe).

---

## 4. LOFI — `eng_lofi.c`

**Páginas:** `EDIT 1`, `EDIT 2`, `MORPH`, `FILTER`, `VOICE`, `VOICE 2`, y sus dos gráficos (`EDIT_LOFI`,
`EDIT_LOFI_FREE`).

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | CHIP | enum (5) | 0..4 | 0 | el conversor (bits y reloj) |
| E1 | WAVE | enum (7) | 0..6 | 0 | `N_RWAVE` |
| E2 | DUTY | int | 0..127 | 64 | ancho de pulso (`DUTY/8`) |
| E3 | CRUSH | % | 0..127 | 0 | peldaños del crusher |
| E4 | CUT | cutoff | 0..127 | 110 | **no**: `lofi_render` usa `p[P_FCUT]` (`:480`), la página FILTER |
| E5 | VIB | % | 0..127 | 0 | vibrato |
| E6 | BEND | % | 0..127 | 0 | deformación de la afinación |
| E7 | MASK | % | 0..127 | 0 | máscara de bits |

**Hallazgo:** `EDIT 2` de LOFI muestra **CUT** como E4, pero el DSP lee **`P_FCUT`** (la de la página FILTER).
Son dos parámetros distintos con la misma etiqueta: girar E4 en `EDIT 2` **no cambia el filtro** (el descriptor
de `lofi_desc` documenta que ahí vive otra cosa). Es una inconsistencia de etiqueta, no un bug de sonido, y hay
que decidirla antes de tocar LOFI: o el DSP lee E4, o la etiqueta cambia.

**Limitación DSP:** el conversor y el crusher son por bloque; el filtro es el SVF compartido, sin seguimiento
de teclado ni envolvente propia.

**Candidato:** key tracking del filtro de LOFI (hoy fijo a 0) — real, pequeño (`:480`, una suma), pero sólo
tiene sentido si antes se resuelve la ambigüedad E4/`P_FCUT`.

---

## 5. VOICE (formant) — `eng_formant.c`

**Páginas:** `EDIT 1`, `EDIT 2`, `FILTER`, `VOICE`, `VOICE 2`.

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | VOWEL | int | 0..127 | 0 | `vowel_at(v0 + m->cutoff, fa, ba)` (`:121`) |
| E1 | VOWEL2 | int | 0..127 | 64 | la segunda vocal del tracto |
| E2 | TALK | tiempo | 0..127 | 0 | velocidad del recorrido entre vocales |
| E3 | SHIFT | semitonos | −12..12 | 0 | |
| E4 | BUZZ | % | 0..127 | 64 | fuente glotal |
| E5 | BREATH | % | 0..127 | 10 | ruido de aliento |
| E6 | Q | % | 0..127 | 64 | agudeza de los formantes |
| E7 | RAND | % | 0..127 | 0 | variación aleatoria del tracto |

**Limitación DSP:** dos formantes (F1/F2) y una tabla de vocales; no hay F3 ni tracto por sexo/edad. Añadir F3
implica una tabla y un filtro más por voz: coste alto para 12 voces.

**Candidato:** ninguno pequeño. Es el motor mejor cubierto de los ocho en proporción a lo que su DSP hace.

---

## 6. WHEEL (drawbar) — `eng_wheel.c`

**Páginas:** `EDIT 1`, `EDIT 2`, `FILTER`, `VOICE`, `VOICE 2`.

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | REG | enum (16) | 0..15 | 4 | registro de drawbars |
| E1 | SUB | int | −8..8 | 0 | drawbar 16' |
| E2 | BODY | int | −8..8 | 0 | drawbars graves |
| E3 | TOP | int | −8..8 | 0 | drawbars agudos |
| E4 | PERC | enum (7) | 0..6 | 0 | percusión |
| E5 | CLICK | % | 0..127 | 40 | click de tecla |
| E6 | DRIVE | % | 0..127 | 0 | saturación |
| E7 | ROTOR | enum (3) | 0..2 | 1 | rotor (leslie) |

**Lo que no se expone:** el rotor **no tiene velocidad ni profundidad**: es un enum de 3 posiciones. JIANT tiene
el mismo enum. La velocidad del rotor es fija en el DSP.

**Candidato:** ROTOR con velocidad separada (un parámetro más) — DSP pequeño (un coeficiente), pero es sonido
de nicho y ya hay `LFO > DEST` para el vibrato.

---

## 7. NOISE — `eng_noise.c`

**Páginas:** `EDIT 1`, `EDIT 2`, `FILTER`, `VOICE`, `VOICE 2`.

| # | Parámetro | Formato | Rango | Def | Lo lee |
|---|---|---|---|---|---|
| E0 | MODE | enum (4) | 0..3 | 0 | blanco / rosa / LFSR / meta |
| E1 | COLOR | % | 0..127 | 64 | inclinación espectral |
| E2 | FREQ | cutoff | 0..127 | 80 | `cut = (p[P_E2]<<8) + …` (`:123`) |
| E3 | PEAK | % | 0..127 | 30 | resonancia del SVF |
| E4 | TRK | % | 0..127 | 64 | **seguimiento de teclado, real**: `+ ((pitch−60·16)·p[P_E4]·151)>>10` (`:123`) |
| E5 | DENSE | % | 0..127 | 0 | densidad (LFSR/meta) |
| E6 | DRIFT | % | 0..127 | 0 | deriva |
| E7 | CRUSH | % | 0..127 | 0 | reloj del crusher |

**Hallazgo:** NOISE es el único motor con **TRK graduable**. Ese es exactamente el control que le falta a ANALOG,
y su fórmula (`p[P_E4]·151 >> 10` aplicada a `pitch16 − 60·16`) es la referencia para implementarlo allí: es
código del propio proyecto, ya probado en producción, no una idea nueva.

**Candidato:** ninguno: el espacio está completo.

---

## 8. DRUM-X — `eng_drum.c`

**Páginas:** `EDIT 1` (KIT, los 8 del kit entero), `EDIT 2` (KIT 2), `KIT 3`, `X-MOD`, `SOUND 1..4` (por sonido, con
`MORPH TUNE TONE DECAY NOISE FM ENV FOLD DRIVE`), `LEVELS`, `LEVELS 2`, `STEREO`, `FILTER`.

| # | Parámetro (kit) | Def | Lo lee |
|---|---|---|---|
| E0 | MORPH | 64 | el motor de cada sonido |
| E1 | TUNE | 64 | afinación del kit |
| E2 | TONE | 64 | brillo |
| E3 | DECAY | 64 | decaimiento |
| E4 | NOISE | 64 | mezcla de ruido |
| E5 | FM ENV | 0 | FM con la envolvente de pitch |
| E6 | FOLD | 0 | wavefolder |
| E7 | DRIVE | 0 | saturación |

**Limitación DSP:** 8 sonidos por kit, 12 kits + 2 legacy; cada sonido es un modelo fijo (no un motor
paramétrico completo). `KIT 3` (BP/FEEDBK) y `X-MOD` (LFO sobre MORPH/RAND) ya cubren lo que JIANT tenía.

**Candidato:** ninguno pequeño que no toque el formato de kit.

---

## 9. RESUMEN: qué falta de verdad y en qué orden

| # | Mejora | Motores | DSP real | Formato | Coste est. | Riesgo | Veredicto |
|---|---|---|---|---|---|---|---|
| **A1** | **Key tracking del filtro (KTR) en ANALOG** | ANALOG | sí (4 sumas) | 1 parámetro común nuevo (compatible: `core.h:263-273`) | flash ~90 B + 1 página; RAM 0 | bajo | **elegida** |
| A2 | Página `VOICE 2` con ALLOC + DETUNE | todos | no (expone DSP que ya existe y hoy es inalcanzable) | 0 (amplía la página) | ~40 B | bajo | 2.ª entrega |
| A3 | Resolver CUT de LOFI (`EDIT 2` E4 vs `P_FCUT`) | LOFI | sí (una línea) | 0 | ~10 B | medio (cambia el sonido de presets) | 3.ª |
| A4 | `OP LEVEL` de FM6 con el chart | FM6 | no | 0 | ~1,2 KB | bajo | 4.ª |
| A5 | ROTOR con velocidad | WHEEL | sí | 1 parámetro | ~80 B | bajo | después |
| A6 | Resto de la lista de `QG_EDIT_EXPANSION.md` (E2, E4, E5, E7, …) | — | — | — | — | — | ya priorizado allí |

### A1 en detalle (la entrega elegida)

**Qué hace.** `KTR` (0..127, 64 por defecto = el 50 % de la constante de hoy) gradúa el seguimiento de teclado
del filtro de ANALOG: `cut += ((v->pitch16 − 60·16) · KTR · 151) >> 10`, la misma forma que NOISE ya usa
(`eng_noise.c:123`). A 0 el filtro no sigue al teclado; a 64 suena como hoy; a 127 sigue el doble.

**Por qué es la de mayor valor.** Es el único hueco de DSP de un motor *audible en todas las notas*: hoy un
acorde abierto en ANALOG se apaga en los agudos y el usuario no tiene control. Además es la primera pieza del
"EDIT completo" (OSCILLATOR/FILTER) que pide la fase, y la que menos código toca por unidad de sonido ganado.

**Archivos.**

| Archivo | Cambio |
|---|---|
| `firmware/src/core.h` | `P_KTR` justo **antes** de `P_E0` (la regla del propio archivo, `:153`) |
| `firmware/src/params.c` | descriptor `PD("KTR", F_PCT, 0, 127, 64)` y la card en la página `FILTER` de ANALOG (`{P_FTYPE, P_E4, P_E5, P_KTR}` o una página `FILTER 2` si se quiere conservar DRV) |
| `firmware/src/eng_analog.c` | los 4 sitios que calculan `cut` (`:103`, `:142`, `:199`, `:268`) y `kd` |
| `web/editor.html`, `site/webapp/editor/index.html` | la tabla de parámetros por índice (los dos árboles) |
| `docs/MANUAL.md`, `docs/MANUAL.es.md` | la fila del parámetro |

**Compatibilidad.** `core.h:263-273` guarda `np = P_COUNT` con cada store y reconstruye: los primeros `np−8`
son los comunes en orden y los últimos 8 son `P_E0..P_E7`. Un parámetro común nuevo entra **antes** de `P_E0`,
así que todo proyecto y preset anterior carga con el valor por defecto (64) y suena igual. **Esto hay que
probarlo**, no suponerlo: `tests/project_test.c` y `tests/upreset_test.c` tienen los casos de ida y vuelta.

**Pruebas exigidas.**

1. `tests/filter_ktr_test.c` (nuevo): KTR 0 no cambia el corte con la nota; KTR 64 es bit-idéntico al render de
   hoy (la constante exacta); KTR 127 sigue el doble; el mismo test en los 4 caminos de `analog_render`.
2. `tests/regress.c`: los 220 dorados **pueden** cambiar sólo si un preset factory usa ANALOG y no está en KTR 64
   — el defecto tiene que ser exactamente la constante de hoy para que no cambie ninguno. Se comprueba, no se
   ajusta el dorado.
3. `tests/project_test.c` / `upreset_test.c`: un proyecto de `np` viejo carga y KTR vale 64.
4. `ui_render`: la página nueva en 17 paletas, lint y alineación en 0.
5. `tests/target_budget.py` y `tests/memory_map_test.py`: el presupuesto de flash y la holgura de `.bss`.

**Lo que esta fase NO hace.** No toca el mapa de flash, ni el bootloader, ni el instalador. No añade motores.
No cambia la firma de `engine_t` ni el formato de patch de FM6.
