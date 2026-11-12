# QG — Motores nuevos: STRING, ADD, MATH

**Fase:** 4/6 · **Estado:** estudio cerrado, **ninguno implementado todavía**. Este documento es la decisión
técnica que la fase pide *antes* de escribir el primero: qué cuesta cada uno en la AC791N, qué RAM hay de
verdad y por qué el orden STRING → ADD → MATH no se puede empezar por STRING sin una decisión de memoria.

## 1. Lo que hay hoy, medido

| Dato | Valor | Clase |
|---|---|---|
| Motores | `NENGINES 14` con **6 huecos reservados** (`ENG_GONE` en 1, 4, 6, 8, 9, 13) y la regla "append-only" | leído en `core.h:96`, `engines.c:45` |
| Motores visibles | `NENG_SHOWN = NENGINES - !FELUCCA_FM4 - 5` → 8 en FM-1 | leído |
| Voces | `NVOICE 12` por parte, **una** bolsa compartida por las seis (`VPOOLN 18` con su slot inerte) | leído en `core.h:58,71` |
| Bloques | 32 muestras a 44,1 kHz ≈ 725 µs por bloque en target; en host `HALF_FRAMES 128` | `AGENTS.md` §8, `core.h:91` |
| `.data+.bss` | 96 088 / 98 304 → **2 216 B libres** | medido (build AC791N, en `docs/MEMORY_BUDGET.md`) |
| Bolsa de audio | 311 240 B, spare `POOL_END - pool_top` **9 248 B**, piso 8 KiB → **1 056 B de margen** | medido |
| Flash | imagen 531 204 / 581 632 → 50 428 B libres | medido |
| FPU | pi32v2 con FPU de simple precisión | `AGENTS.md` §1 |

Un motor nuevo no es sólo DSP: exige entrada en `ENGINES[]`, **presets** (la librería de sonidos y el editor los
cuentan), `knob[4]`, `page_title[2]`, `note_on`/`render`, su dibujo de EDIT (o `GR_NONE` + texto), y su lugar en
`ENGINE_ORDER`. Los tres que siguen se **añaden** en 14, 15 y 16: ningún índice existente se mueve, así que
proyectos, presets de usuario y el protocolo del editor siguen resolviendo a lo que resolvían.

## 2. STRING — Karplus-Strong

**Algoritmo.** Una línea de retardo de *L = FS/f* muestras con un filtro de un polo (el "amortiguador") y la
excitación como contenido inicial. Enteros: `y[n] = (a·y[n−L] + b·y[n−L−1]) >> k`, con la afinación por
interpolación lineal en la lectura (la tensión/brightness es `b/a`, y la amortiguación el polo).

**Controles que la fase pide** (todos caben en los 8 `P_E*`): `EXCIT` (ruido/impulso y su color), `TENSION`
(polo del filtro: brillo vivo), `DAMP` (pérdida por vuelta), `DECAY` (la cola total), `BRIGHT` (filtro de la
excitación), `RES` (un paso-banda resonante en serie) y `TUNE` (±24 st). Es exactamente el espacio de JIANT PHYS.

**El problema es la RAM, y es aritmética, no opinión.** La línea tiene que durar un periodo entero:

| Nota más grave | Muestras (8 bits) | 12 voces | 8 voces | 4 voces |
|---|---|---|---|---|
| C2 (65,4 Hz) | 674 | 8 088 B | 5 392 B | 2 696 B |
| E2 guitarra (82,4 Hz) | 535 | 6 420 B | 4 280 B | 2 140 B |
| A2 (110 Hz) | 401 | 4 812 B | 3 208 B | 1 604 B |

Contra **2 216 B libres en `.bss`** y **1 056 B de margen en la bolsa**, ninguna fila de 8 o 12 voces cabe sin
mover algo. Con 4 voces y piso E2 (2 140 B) sí cabe en `.bss`, pero cuatro voces no son un motor de cuerdas
polifónico; son un juguete.

**La decisión que hace falta** (una de estas tres, en este orden de preferencia):

1. **Sacar las líneas de la bolsa de audio**: declarar el bloque KS como el tramo que sigue a `pool_top` y hacer
   que `tests/target_budget.py` lo cuente contra `POOL_END`. Con 12 voces y piso E2 son 6 420 B: **no cabe** con
   el piso de 8 KiB tal cual (1 056 B de margen), así que habría que bajar el piso a 2 KiB, que es una decisión
   del dueño, no del agente.
2. **8 voces y piso A2** (3 208 B, la mitad del margen de la bolsa): cabe en la bolsa sin tocar el piso, pero el
   motor no baja del La2, y eso en un groovebox con bajos es un recorte audible.
3. **Recortar RAM en otro sitio** (el banco de voces de FM6, `fm6_vbank.c`, o el pool de 18 slots a 13 si la
   política de voces lo permite) y dedicar lo liberado a las cuerdas. Es la única que da 12 voces con piso C2.

**CPU**: ~2 lecturas + 1 escritura + 1 interpolación por muestra y voz → ~12–16 ciclos → 44 100 × 12 × 14 ≈
7,4 M ciclos/s ≈ **2–4 %** de un núcleo a 200 MHz. Barato: no es la CPU lo que lo frena.

**Flash**: ~700–900 B de código, sin tablas. **Licencias**: Karplus-Strong 1983 es un algoritmo publicado;
se implementa desde la descripción, sin portar código de nadie. Sin dependencias.

**Lo que no se hará**: llamarlo STRING y sonar a un banco de samples (eso era SAMPLE, retirado), ni bajar de 12
voces a 4 y llamarlo polifónico.

## 3. ADD — aditiva

**Cómo.** Una tabla de seno de 2 048 puntos (Q15, 4 KB en **flash**, `const`) y N parciales por voz leídos a
`k·f`. La amplitud de cada parcial es un perfil (1/k, impar, dibujado, o el que digan los parámetros) y su
evolución una envolvente por parcial o una global con "tilt".

**Controles**: `PARTIALS` (1..16), `TILT` (la pendiente del perfil), `ODD` (sólo impares / todos), `EVOLVE`
(cuánto se mueve el perfil en el tiempo), `PHASE` (dispersión inicial), `DETUNE`, `FOLD`. La fase pide
"armónicos, amplitud de parciales, envolventes, mezcla y evolución tímbrica": todos caben.

**RAM**: 16 fases × 2 B por voz = 32 B, × 12 voces = **384 B** → cabe en los 2 216 B libres, con sitio para el
estado de la envolvente. **Flash**: tabla 4 KB + ~1 KB de código.

**CPU**: el más caro de los tres. 8 parciales × 12 voces × 44 100 = 4,2 M lecturas de tabla por segundo; con
~8–10 ciclos por parcial (índice, fase, envolvente, suma, todo entero) son 34–42 M ciclos/s ≈ **17–21 %** de un
núcleo. Con 16 parciales se va al 35–40 %: el límite es `PARTIALS` y hay que medirlo en target antes de fijar su
máximo (por eso este motor va **después** de STRING: primero una medida real de CPU con la tabla en marcha).

**Licencias**: ninguna. Tabla generada por `tools/build.py` como las demás.

## 4. MATH — bytebeat / floatbeat

**Cómo.** Un contador por voz y una **lista corta de fórmulas propias** (`(t>>k)*…`, `t*(t>>k)`, XOR/shift
anidados), cada una con 2–3 parámetros que el usuario gira: `FORM` (la fórmula), `RATE` (el reloj del contador,
que es la "velocidad" del patrón), `MASK`, `BITS`, `FOLD`, `MIX`, y una `TUNE` que reescala el contador por nota
(así una fórmula sigue siendo tonal). La fase pide "variación tímbrica mediante algoritmos acotados y
eficientes": eso es exactamente esto.

**RAM**: contador + fase + parámetros ≈ 16 B por voz → **192 B** para 12 voces. **Flash**: ~600 B. **CPU**: una
fórmula por muestra y voz, media docena de operaciones → ~20 ciclos → 10,6 M ciclos/s ≈ **5 %**.

**floatbeat**: la AC791N tiene FPU de simple precisión, así que es *viable*, pero en el camino caliente y por
voz es 3–6× más caro que entero y el resultado no es más musical. Se ofrece **después** del bytebeat entero, y
sólo si la medida de CPU del entero deja margen.

**Licencias**: el bytebeat es una práctica de fórmulas publicadas; se escriben **fórmulas propias** en vez de
copiar el repertorio conocido, y así no hay atribución que arrastrar ni duda de licencia.

## 5. Orden y entregas

| # | Entrega | Qué incluye | Por qué así |
|---|---|---|---|
| 1 | **Decisión de memoria** | una de las tres opciones de §2, con el número y con el dueño del piso de 8 KiB de acuerdo | sin ella STRING nace cojo |
| 2 | **STRING** | `eng_string.c`, 8 presets, 2 páginas EDIT, `note_on`/`render`, test de audio (afinación a ±3 cents en 3 octavas, decaimiento monótono, silencio al final, sin denormales), presupuesto de CPU y RAM | es el que la fase pide primero |
| 3 | **ADD** | `eng_add.c`, tabla de seno, 8 presets, 2 páginas EDIT, test de espectro (los parciales están donde dicen, la fundamental no se dobla) | necesita la CPU medida de STRING |
| 4 | **MATH** | `eng_math.c`, 10–12 fórmulas propias, 8 presets, 2 páginas EDIT, test de rango (nunca satura el bus sin `DRIVE`) | el más barato, y cierra la fase |

Cada entrega: `NENGINES`/`ENGINE_ORDER` (append), su entrada en `ENGINES[]`, presets en la librería, el editor
(`web/editor.html` y `site/webapp/editor/index.html` llevan la tabla de motores), `docs/MANUAL*.md`, y las
pruebas de audio de `tests/` con sus dorados. Ninguna toca el mapa de flash ni el bootloader.
