# QG — Roadmap de implementación por entregas pequeñas

**Fase:** 1/6 (auditoría) · **Fecha:** 12 de noviembre de 2026
**Estado:** ninguna entrega de este documento está implementada. Es un plan, no un reporte de avance.

Documentos de los que sale este plan:

| Documento | Qué responde |
|---|---|
| `docs/JIANT_QG_COMPARISON.md` | qué es cada proyecto hoy, y por qué divergieron |
| `docs/JIANT_FEATURE_MATRIX.md` | función por función: JIANT sí/no, QG sí/no/oculto/sin UI, valor, prioridad |
| `docs/JIANT_SCL_ANALYSIS.md` | modelo tonal completo, cómo lo presenta JIANT, propuesta 240×240 para QG |
| `docs/QG_EDIT_EXPANSION.md` | 12 mejoras de EDIT con coste, riesgo y pruebas |
| `docs/QG_HOME_NAVIGATION_PLAN.md` | 3 vistas de HOME y 4 pasos de navegación |
| **este documento** | en qué orden, con qué puerta de verificación, y cuál va primero |

---

## 1. Criterios de secuenciación

Cada entrega de la secuencia cumple **los cinco**:

1. **Una sola cosa.** Se puede describir en una frase y verificarse en una sola sesión.
2. **Sin cambio de formato de proyecto ni de preset**, salvo que la entrega consista exactamente en eso y venga
   con su migración y su test de ida y vuelta. El formato es el activo más frágil del proyecto
   (`core.h:154-156`: los parámetros comunes nuevos van justo antes de `P_E0` y los stores mapean **por
   cantidad**).
3. **Sin tocar el DSP** en las entregas de UI, para que `tests/regress.c` (220 renders dorados) no se mueva. Si
   una entrega toca DSP, se dice, y se actualizan los dorados en el mismo commit con la explicación.
4. **Medible en host.** Toda entrega tiene al menos un test automático que falla si la entrega se rompe.
5. **Reversible.** Si empeora la sensación de uso, se puede sacar sin dejar residuos en el proyecto del usuario.

Regla de oro heredada del `AGENTS.md` de QG: **una compilación no es un test, un test no es un dispositivo.** Al
final de cada entrega se reporta qué clase de evidencia se obtuvo: `compilado`, `test de host`, `workerd/WASM`,
`presupuesto estático de target` o `medido en unidad`.

---

## 2. La secuencia

| # | Entrega | Documento | Tamaño | Depende de | Puerta de verificación |
|---|---|---|---|---|---|
| **D1** | **Arreglo de `chord_build` en escalas de 5/6/8 notas** | SCL §4.2, S1 | ~40 líneas en 1 archivo | — | test nuevo exhaustivo + `regress` |
| D2 | Página `SCL · CHORD` con diagrama por grado | SCL §4.2, S2 | ~1,4–2 KB flash | D1 | `ui_render` + `ui_test` |
| D3 | `VOICE 2` con ALLOC y DETUNE | EDIT E1 | ~40 B + docs | — | `ui_test` (qué escribe cada knob) |
| D4 | Documentar la 2ª banca del layer FX + card `BANK` | EDIT E3 | 0–60 B + docs | — | `perform_test` |
| D5 | Breadcrumb con grupo en el pie | HOME N1 | ~250 B | — | `ui_render` (texto no cortado) |
| D6 | Página `FX · PUNCH` (lane visible y editable) | EDIT E2 | 1,6–2,4 KB | D4 | `pfx_lane_test` nuevo + `project_test` |
| D7 | Vista `HOME · TRACK` | HOME H1 | ~1 KB, 1 B RAM | — | `ui_render` ×17 paletas |
| D8 | Vista `HOME · ROUTES` | HOME H2 | ~1,2 KB | D7 | `ui_test` (no escribe nada) |
| D9 | Grado seleccionable + tocar/fijar acorde + grabar al paso | SCL S3 | ~0,6 KB | D2 | `ui_test` + `regress` |
| D10 | Salto de grupo con doble toque en EDIT/SEQ | HOME N2 | ~700 B | D5 | `ui_test` (recorrido por familia) |
| D11 | Página `EDIT · FOCUS` | EDIT E4 | 1,2–1,8 KB | D5, D10 | `mod_test` + `ui_test` |
| D12 | Memoria de página por tipo de track en EDIT | HOME N4 | ~80 B, 2 B RAM | D10 | `ui_test` |
| D13 | Revisión de presupuesto en target y decisión sobre STRING/ADD | este doc §5 | medición | D1–D12 | `memory_map_test`, `target_budget` |

Después de D13, y sólo con números reales del build de target en mano, se decide el orden de los proyectos
grandes (VOICE con palabras, STRING, ADD, MATH, progresiones). **No se comprometen antes de medir.**

### Por qué este orden y no otro

- **D1 primero** porque es la única entrega que cambia *lo que suena* de un modo verificable sin hardware, y
  porque D2 y D9 muestran acordes: si el motor de acordes está mal, la pantalla nueva documenta el error.
- **D2 antes de D3** porque el diagrama de acordes es la mejora más visible de todo el informe y no necesita
  ningún parámetro nuevo.
- **D3 y D4 son de una línea**: se pueden colar en la misma sesión que D2 si el presupuesto de revisión lo
  permite, pero se cuentan como entregas separadas para que cada commit diga una cosa.
- **D6 después de D4** porque la 2ª banca tiene efectos que la lane puede escribir (nibbles 13/14/15); conviene
  primero que el usuario sepa que existen.
- **D11 (FOCUS) al final de la UI** porque es la que más lógica de entrada nueva tiene y se beneficia del
  breadcrumb y de los grupos ya probados.
- **Nada de formato** en D1–D12. Las tres ideas que lo requieren (ROTOR depth/dust, VOICE con palabras,
  progresiones, key por sección) quedan fuera de la secuencia corta y necesitan su propia fase con migración.

---

## 3. La primera implementación: **D1 — `chord_build` en escalas de 5, 6 y 8 notas**

### 3.1 Por qué es la de mayor valor

1. **Es la única que arregla un error audible hoy.** En QG, con `MPEN` (pentatónica menor) y CHORD en `TRIAD`,
   una tecla en C produce **C · F · Bb**: no es una tríada, es una cuarta y una séptima. Lo mismo en `PEN`,
   `BLUES`, `WHOLE`, `DIMHW` y `DIMWH` — 6 de las 16 escalas. JIANT lo corrigió en 0.8.3 con la regla "el
   intervalo que el acorde pide donde la escala lo tiene, si no el más cercano que ofrece".
2. **No toca la interfaz.** Cero riesgo de layout, cero capturas nuevas, cero cambios de manual (salvo la nota
   de comportamiento).
3. **No cambia el formato.** Los acordes grabados son notas MIDI (`step_t.note[4]`), no grados: un proyecto
   guardado antes suena igual que antes si no se toca CHORD, y los pasos ya grabados no se reescriben.
4. **Es verificable de forma exhaustiva en host.** 16 escalas × 12 raíces × 11 modos × 4 voicings = 8 448
   combinaciones, con una invariante fuerte y objetiva: **toda nota del acorde pertenece a la escala**. No
   requiere oído ni hardware.
5. **Habilita D2 y D9.** El diagrama de acordes y el "tocar el acorde del grado" sólo tienen sentido si el
   acorde es el correcto.

### 3.2 Implementación de referencia

JIANT `firmware/src/seq.c:242-320` (0.8.3). Es la idea que se porta, **re-implementada sobre el `seq.c` de QG**;
no se copia el archivo (los dos árboles han divergido: QG tiene 12 voces, `KB_CH`, y su propio `SCALE_MASK`
idéntico pero su propio entorno de compilación y de lint).

### 3.3 Qué se toca exactamente

| Archivo | Cambio | Líneas estimadas |
|---|---|---|
| `firmware/src/seq.c` | `chord_pick()` nueva + rama `else` de `chord_build` cuando la escala no tiene 7 notas | +38 / −3 (se reemplaza el bloque `for (i = 0; i < 4u && DIA[…]…)` por el `if (c == 7u) … else …`) |
| `tests/chord_scale_test.c` | **nuevo**: el test exhaustivo | +120 |
| `tests/run_tests.sh` | una línea para compilar y correr el test nuevo | +2 |
| `docs/MANUAL.md`, `docs/MANUAL.es.md` | 4 líneas en la sección CHORD explicando que en escalas de 5/6/8 notas cada tono es el intervalo pedido o el más cercano de la escala | +8 |

Nada más. No se agregan parámetros, ni páginas, ni estado.

### 3.4 El algoritmo, en concreto

```c
/* el primer intervalo de la lista p (n de ellos) que la escala tiene sobre la raíz rel semitonos sobre ROOT */
static int32_t chord_pick(uint32_t mask, int32_t rel, const int8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if ((mask >> (uint32_t)((rel + p[i]) % 12)) & 1u)
            return p[i];
    return -1;
}
```

Tablas (8 listas, 22 bytes en total, `static const int8_t`):

| Lista | Contenido | Significado |
|---|---|---|
| `T3` | 4, 3, 5, 2 | tercera mayor, menor, suspendida 4, suspendida 2 |
| `F5` | 7, 8, 6 | quinta justa, disminuida, aumentada |
| `F5M` | 7, 6, 8 | (cuando la tercera fue menor: la quinta justa primero, después la bemol) |
| `S7` | 10, 11, 9 | séptima menor, mayor, sexta |
| `S7D` | 9, 10 | (tercera menor + quinta disminuida: sexta antes que séptima) |
| `S9` | 14, 15 | novena menor, mayor |
| `S2` | 2, 1 | segunda mayor, menor (para `SUS2`) |
| `S4` | 5, 6 | cuarta justa, aumentada (para `SUS4`) |

Secuencia (idéntica a la referencia, con los nombres de QG):

```
th = (mode == CD_SUS2) ? pick(S2) : (mode == CD_SUS4) ? pick(S4) : -1
if (th < 0) th = pick(T3)
fi = (th == 3) ? pick(F5M) : pick(F5)
out: raíz
     + th  (si th >= 0)
     + fi  (si fi >= 0)
     + séptima/novena según mode
```

Y después, **sin cambios**, la cola que QG ya tiene: `CV_OPEN`, `CV_LEAD`, `CV_BASS`, orden ascendente, quita
de unísonos, recorte a 0..127.

### 3.5 Riesgo de regresión, analizado

| Camino | ¿Cambia? | Por qué |
|---|---|---|
| Escalas de 7 notas (MAJ MIN DOR MIX HARM PHRY LYD LOC MEL) con CHORD | **no** | la rama `c == 7u` es la misma que hoy |
| CHR (`0xFFF`) | **no** | la máscara se sustituye por MAJ antes de contar (`if ((mask & 0xFFF) == 0xFFF) mask = SCALE_MASK[1]`), así que `c == 7` |
| Formas en cualquier tecla (MAJ MIN 7 MAJ7 MIN7 POWER) | **no** | van por `SHP[]`, antes de la rama diatónica |
| PEN MPEN BLUES WHOLE DIMHW DIMWH **con CHORD OFF** | **no** | `chord_build` devuelve la raíz sola en la primera línea si `!mode` |
| PEN MPEN BLUES WHOLE DIMHW DIMWH **con CHORD ON** | **sí, a propósito** | es el arreglo |
| Renders dorados de `tests/regress.c` | **pueden** cambiar si algún preset dorado usa una de esas 6 escalas con CHORD ON | hay que comprobarlo: si cambia, se documenta en el commit y se actualiza el dorado; si no cambia, se deja constancia de que no cambió |

Ese último punto es la única incógnita y se resuelve en la primera hora de la entrega, antes de escribir el
resto: correr `regress` con el cambio aplicado y ver la lista.

### 3.6 Pruebas exigidas

**Test nuevo `tests/chord_scale_test.c`** (host, sin hardware):

1. **Invariante de tonalidad** — para 16 escalas × 12 raíces × 11 modos × 4 voicings: toda nota devuelta, módulo
   12 relativo a la raíz, tiene su bit puesto en `SCALE_MASK[escala]`. Un solo fallo imprime
   `escala/raíz/modo/voicing` y la nota culpable.
2. **Forma** — `n` en 1..5; estrictamente ascendente; sin unísonos; todas en 0..127.
3. **Tabla de referencia escrita a mano** para C mayor (TRIAD): C, Dm, Em, F, G, Am, Bº — con las notas exactas.
   Si el código y la tabla discrepan, falla el test, no la tabla.
4. **Tabla de referencia para Cm pentatónica** (el caso del informe de JIANT): Cm, Eb, Fsus4, Eb/G, Bbsus4; y
   `7TH` sobre F = F7sus4.
5. **Excepciones** — MONO, LEGATO, UNISON y un motor de un solo disparo (DRUM) devuelven **1** nota.
6. **Tecla fuera de la escala** — con MPEN y una tecla en D, la raíz se corrige hacia abajo a C y el acorde es el
   de C.

**Comandos, y su estado esperado:**

```bash
NO_PACKAGE=1 ./tests/run_tests.sh      # incluye el test nuevo; exit 0
tests/regress.c                        # dorados: idénticos o lista justificada
tests/ui_render.c                      # debe quedar idéntico (no se toca dibujo)
```

**Evidencia que se reporta al cerrar D1:** *test de host exhaustivo (8 448 combinaciones) + regresión de renders
dorados*. **No** "probado en hardware".

### 3.7 Criterio de aceptación (definición de terminado)

- [ ] `tests/chord_scale_test.c` existe, falla contra el árbol anterior y pasa con el cambio (se comprueba
      revirtiendo el bloque un momento, para no escribir un test que no prueba nada).
- [ ] `NO_PACKAGE=1 ./tests/run_tests.sh` exit 0.
- [ ] `regress` con 0 renders cambiados, **o** los cambiados listados con su justificación en el mensaje del
      commit.
- [ ] `ui_render` idéntico (la entrega no toca dibujo).
- [ ] `docs/MANUAL.md` y `docs/MANUAL.es.md` actualizados.
- [ ] Commit que dice **por qué** (el acorde de una pentatónica era un apilamiento, no un acorde), con la
      evidencia de arriba.
- [ ] Informe honesto: "verificado en host; no medido en unidad".

### 3.8 Rollback

Revertir un solo commit. No hay estado persistido, ni formato, ni migración: el único efecto es que los acordes
de 6 escalas vuelven a ser apilamientos.

---

## 4. Protocolo de verificación común a todas las entregas

| Clase de evidencia | Cómo se obtiene | Qué prueba |
|---|---|---|
| Compilado | `python3 -c "…build.generate()"` y `gcc -fsyntax-only -I firmware/hal -I build/gen -I firmware/src firmware/src/felucca.c` | el compilador aceptó el código |
| Test de host | `NO_PACKAGE=1 ./tests/run_tests.sh`, `tests/groups.sh` para partir el tiempo | comportamiento en x86, con DMA y flash simulados |
| Regresión de audio | `tests/regress.c` + `tests/golden.txt` | ningún render cambió, o cuáles y por qué |
| Presupuesto estático de target | `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | instrucciones del bucle de render en el pi32v2 |
| Memoria | `python3 tests/memory_map_test.py` | `.data+.bss`, RAM code, pool spare |
| UI | `tests/ui_render.c` (17 paletas × FLAT/LINE, lint + alineación) y `tests/ui_test.c` | ninguna página nueva rompe el layout ni la navegación |
| Formato (sólo D de formato) | `tests/project_test.c`, `tests/upreset_test.c` | ida y vuelta de todos los formatos |
| Web/editor | `node web/test_web.mjs`, `node web/test_pkg_bounds.mjs`, `python3 tests/install_test.py` | el protocolo y el paquete siguen coherentes |
| Emulador (WASM) | `web/emu/emu_test.mjs` | el WASM arranca y suena igual que el nativo |
| Unidad física | — | **no disponible en esta fase**; cualquier entrega de DSP puro queda pendiente de esa medición |

**Ninguna entrega de este roadmap toca el DSP salvo D1 (que es lógica de construcción de acordes, no de audio),
así que no hay ninguna que requiera la unidad física para cerrarse.** Las que sí lo requieren (STRING, ADD, MATH,
VOICE con palabras) están fuera de la secuencia corta precisamente por eso.

---

## 5. STRING, ADD, MATH y EXT — viabilidad con números

Estos cuatro son **motores nuevos**, no páginas. Ninguno existe en JIANT (excepto la idea de EXT, que su spec
declara fuera de alcance), así que no hay "implementación de referencia": hay que diseñarlos, y las cifras de
abajo son estimaciones para decidir si vale la pena medirlas, no compromisos.

### 5.1 Restricciones reales del hardware, ya conocidas

| Recurso | Holgura medida (v1.9.0, `docs/MEMORY_BUDGET.md` §2.1) | Consecuencia |
|---|---|---|
| Imagen de app | 50 428 B | cabe código nuevo: **la flash no es el problema** |
| `.data+.bss` | **2 216 B** | cualquier motor con estado por voz compite acá |
| RAM code | 5 132 B | código caliente en RAM, escaso |
| Pool spare | 1 056 B sobre el piso de 8 KiB | buffers grandes sólo si se reutiliza algo |
| Render | ≤ 522 instrucciones por bloque de 32 muestras | 725 µs por bloque a 44.1 kHz: **el techo real es CPU, no memoria** |

### 5.2 STRING (cuerda pulsada, Karplus-Strong extendido)

- **Diseño**: una línea de delay por voz con filtro de lazo (KS clásico) + excitación (ruido o impulso filtrado)
  + control de amortiguación y brillo.
- **RAM**: una línea de KS necesita al menos un período de la nota más grave. A 44.1 kHz, C2 (65 Hz) = 678
  muestras. Con 6 voces simultáneas de nota libre: ~4 KB en `int16` — **no entra** en los 2 216 B.
  Tres caminos:
  1. **Una línea por track, con repitch** (como un sampler mono): 6 × 678 × 2 B = 8,1 KB si se limita a C2;
     4,1 KB si se limita a C3 (135 Hz, 327 muestras). Reutilizando el `sl_buf` del slicer (32 KB, compartido con
     `perform.c`) **cuesta 0 B nuevos**, con la condición de tomar el buffer sólo si el slicer está OFF —
     precedente exacto: `perform.c` ya presta `sl_buf` (`sl_lent`, `slicer.c:69`).
  2. **Decimado**: KS a 22.05 kHz baja todo a la mitad (2 KB) con pérdida en el ataque.
  3. **Línea corta + alargamiento por grano**: 1 024 muestras para todas las notas, con artefactos en las graves.
- **Flash**: ~1,5–2,5 KB de código.
- **CPU**: 1 multiplicación + 1 suma + 1 lectura por muestra por voz (baratísimo), más la excitación al note-on.
  Con 6 voces es de los motores más baratos que existen.
- **Riesgo principal**: la pelea por `sl_buf` con el slicer y con la 2ª banca del layer FX (que ya usa el mismo
  buffer para REV/FRZ/TAPE/GLCH). **Hay que decidir una política de propiedad del buffer antes de escribir una
  línea.**
- **Veredicto**: **viable**, con línea por track y repitch. Es el más prometedor de los tres. Requiere medir en
  target el peor caso con el slicer activo.

### 5.3 ADD (aditivo por tablas de armónicos)

- **Diseño**: N parciales por nota, cada uno un seno de tabla (ya existe `sine_i`, `bg_sin`) o una tabla de
  armónicos por nota, con envolvente por parcial o una común.
- **CPU**: N lecturas de tabla + N multiplicaciones + N sumas por muestra. Con N = 12 y 6 voces son 72
  multiplicaciones por muestra, contra un presupuesto de 522 instrucciones por 32 muestras (≈16 instrucciones por
  muestra). **No entra con 12 parciales.** Con N = 8 y una voz por track (6 en total, 48 multiplicaciones) sigue
  siendo dudoso. Alternativa: tabla precalculada de un ciclo completo del timbre por nota (más flash, 1 lectura
  por muestra), que es la forma barata y la que recomendaría si se hace.
- **RAM**: 0 si se usan tablas en flash; 2–4 KB si los parciales se calculan por voz.
- **Flash**: 1–2 KB de código + tablas (una tabla de 256 muestras × 8 bits = 256 B por timbre).
- **Veredicto**: **viable sólo con tabla precalculada y pocos timbres**. Como motor "de verdad" (parciales
  calculados, muchos parciales) **no entra en CPU**. Requiere medición previa.

### 5.4 MATH (operadores lógicos sobre señal)

- **Precedente real**: LOFI `BYTE` ya es bytebeat (`eng_lofi.c`, `N_BYTE`, `N_FLOAT`). Un motor MATH sería un
  bytebeat sin la parte de emulación de chip, con más operadores y sin tabla de chips.
- **Coste**: prácticamente el de LOFI (que ya está medido y en el presupuesto). ~1–2 KB de código, 0 RAM nueva.
- **Riesgo**: **solapamiento conceptual con LOFI**. Si el usuario no distingue "LOFI" de "MATH", se agregó un
  motor sin agregar una voz. Antes de hacerlo, conviene preguntar: ¿qué timbre tiene MATH que LOFI no puede
  hacer? Si la respuesta es "más operadores y otra curva", probablemente sea mejor **ampliar LOFI** que sumar un
  motor.
- **Veredicto**: **el más barato de los tres y el de menor valor incremental**. No lo priorizaría.

### 5.5 EXT (audio externo por los FX) — sólo posibilidad futura

- Lo que haría falta: una ruta de audio entrante (USB audio isócrono o ADC) que entre a un bus y pase por los
  efectos. En la FM-1, **no hay evidencia en ninguno de los dos repositorios de que exista esa ruta**: JIANT lo
  declara explícitamente fuera de alcance ("sin evidencia de que sea posible", `FELUCCA-TONIC-SPEC.md`), y QG no
  la menciona.
- Lo único documentado en QG relacionado es el **audio USB de salida** (`uac_fir.h`, `usb.c`) y el ring de
  reproducción, no una entrada.
- **Veredicto**: **no es una entrega de firmware; es una investigación de hardware** (¿el AC791N de la FM-1 tiene
  I2S/ADC libre y accesible? ¿el codec permite una entrada?). Sin respuesta a eso, cualquier plan es invento.
  Queda registrado como posibilidad futura y **fuera** de toda secuencia.

---

## 6. Riesgos del proceso

| Riesgo | Cómo se manifiesta | Mitigación |
|---|---|---|
| El árbol de QG ya no compila por otra razón | `build.sh`/`-fsyntax-only` falla antes del cambio | correr el chequeo **antes** de tocar nada y anotar el estado de base |
| Los renders dorados se mueven y se "aceptan" sin mirar | `regress` pasa tras regenerar `golden.txt` | regenerar sólo con la lista de qué cambió y por qué, en el mensaje del commit |
| El lint de UI rechaza texto nuevo | `ui_render` con hallazgos de texto cortado o fuera de celda | toda cadena nueva pasa por `text_ink`/`af_*` y se prueba en la paleta más ancha y en la más angosta |
| Presupuesto de RAM cruzado sin darse cuenta | `.data+.bss` > 98 304 B | `memory_map_test.py` en cada entrega que agregue estado |
| Copiar código de JIANT tal cual | el árbol no compila (divergencia de 1 000+ commits) | re-implementar; portar sólo la idea y la tabla; acreditar en el commit |
| Declarar "probado" algo que sólo compiló | el `AGENTS.md` lo prohíbe expresamente | cada cierre de entrega nombra la clase de evidencia |
| Cambiar el formato "de paso" | proyectos viejos abren mal | ninguna entrega de D1–D12 toca `core.h` ni `project.c` |

---

## 7. Resumen ejecutivo

1. **Auditoría cerrada con seis documentos.** El hallazgo central: QG no está atrás de JIANT en capacidad de
   síntesis —está a la par o adelante en casi todos los motores, y adelante en master—; está atrás en **tres
   cosas concretas y baratas**: la construcción de acordes en escalas de 5/6/8 notas, la **vista** de acordes, y
   la **exposición** de cosas que ya tiene (ALLOC/DETUNE, punch-in lane, 2ª banca de FX, rutas de macros).
2. **La primera implementación es D1**: arreglar `chord_build` para las 6 escalas no heptatónicas. 40 líneas,
   un archivo, sin formato, sin UI, verificable de forma exhaustiva en host con una invariante objetiva, y
   habilita la entrega más visible (D2, la pantalla de acordes).
3. **Después: la pantalla de acordes (D2)**, que es la pieza de SCL que QG no tiene y la que convierte el sistema
   tonal en algo que se ve.
4. **Lo que no está en la secuencia corta** — STRING, ADD, MATH, EXT, VOICE con palabras, progresiones, ROTOR
   depth/dust, key por sección — no está por falta de valor, sino porque cada uno pide o **medición en target**
   (CPU/RAM) o **cambio de formato** con migración. Se deciden en D13, con números.

---

## 8. Estado de las fases (actualizado en la fase 4)

| Fase | Entrega | Estado | Evidencia |
|---|---|---|---|
| 1 | Auditoría JIANT vs QG (6 documentos) | **cerrada** | `docs/JIANT_QG_COMPARISON.md`, `JIANT_FEATURE_MATRIX.md`, `JIANT_SCL_ANALYSIS.md`, `QG_EDIT_EXPANSION.md`, `QG_HOME_NAVIGATION_PLAN.md`, este documento |
| 2 | **D1** `chord_build` en escalas de 5/6/8 notas | **cerrada** | `tests/chord_scale_test.c`: INVARIANT 8 448/8 448, 360 acordes de 7 notas idénticos al recorrido viejo |
| 2 | **D2** SCL como herramienta: SCALE, CHORD, VOICING, INTERVAL, PROG, KEYS | **cerrada** | `firmware/src/ui_scl.c`; `ui_render` 0 lint / 0 alineación; `ui_test` verde |
| 3 | Auditoría de los 8 motores y diseño sonoro | **cerrada (estudio)** | `docs/QG_ENGINE_AUDIT.md`: parámetros leídos del código, huecos reales, A1 (KTR en ANALOG) elegida y especificada |
| 2 | Fase 2: banco CHARACTER por motor, LFO SHAPE, ENV VEL, destinos X1..X4 | **cerrada (host)** | `docs/QG_PHASE2.md`; `tests/phase2_test.c` verde; 285 goldens sin cambio; `ui_render` 189 pantallas, 0 lint |
| 3 | Fase 3: pantalla ARP con teclado, notas activas y recorrido reales; deuda de pruebas de Fase 2 | **parcial (host)** | `docs/QG_PHASE3.md`; `ui_test` all passed; `ui_render` 189 pantallas, 0 lint; 285 goldens sin cambio |
| 3 | **A1** KTR como parámetro real de ANALOG | **cerrada (host)** | Fase 2, CHARACTER `KTR+`; `phase2_test` verde; la medida de CPU/audio del target sigue pendiente |
| 4 | HOME multipantalla: ciclo + OVERVIEW + SYSTEM | **cerrada** | `docs/QG_HOME_VIEWS.md`; `test_home_views` verde; `ui_render` 187 pantallas, 0 lint, 0 alineación |
| 4 | PERFORMANCE, MIXER, MODULATION, SOUND ENGINE | pendiente | especificadas en `QG_HOME_VIEWS.md` §5 |
| 4 | STRING, ADD, MATH | **estudio cerrado; ninguno implementado** | `docs/QG_NEW_ENGINES.md`: RAM medida, CPU estimada, decisión de memoria pendiente |

**Bloqueos vigentes:** sin `JIELI_TOOLCHAIN`/`AC79_SDK` no hay build AC791N, ni `target_budget.py`, ni medida
real de CPU/RAM/flash; sin `emcc` no hay WASM del emulador. Todo coste de este documento es estimación salvo
donde dice "medido", y lo medido viene de `docs/MEMORY_BUDGET.md` (build AC791N previo), no de este entorno.
