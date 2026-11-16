# FASE 7 — auditoría medida de los motores (2026-11-16)

Todo lo de esta página sale del árbol actual (`2c3bd84`) y de instrumentos que ya están en el repositorio.
No hay estimaciones ni "debería entrar": lo que no está medido, está dicho como no medido.

## 1. Tamaño real de cada motor

`nm --size-sort build/felucca.elf` sobre la compilación AC791N definitiva (unidad única, `-Oz`). El ELF es
`pi32v2`, leído con el `nm` del sistema: los tamaños son los símbolos del binario que se flashea.

| motor | `*_render` | tabla de presets | presets en la tabla |
|---|---:|---:|---:|
| ANALOG | 618 B | 2 448 B | 68 |
| FM6 | 1 714 B | 1 332 B | 37 |
| PHASE | 878 B | 1 152 B | 32 |
| LOFI | 1 462 B (+ 524 + 590 de las dos variantes) | 1 044 B | 29 |
| FORMANT | 1 730 B | 900 B | 25 |
| NOISE | 1 596 B | 792 B | 22 |
| WHEEL | 796 B | 792 B | 22 |
| WAVETABLE | 908 B | 792 B | 22 |
| DRUM-X | 732 B (+ `dx_run` 426 y `dx_run_sn` 276 de coste estático) | 648 B | 18 |
| DIGITAL (retirado) | 258 B | 288 B | 8 |

`preset_t` mide **36 B** (las once tablas son múltiplos exactos de 36: 2 448 = 68 × 36, 1 332 = 37 × 36, …), así
que la biblioteca son **275 presets de fábrica** sin contar los 8 del motor retirado.

## 2. Coste por bloque de cada render

`tests/target_budget.py` cuenta las instrucciones de los bucles de cada función en `build/felucca.dis`
((instrucciones del bucle) + 4 por nivel de anidamiento + 9 por división). Es una medida **estática**: dice
cuánto trabajo hay en el bucle, no cuántos ciclos corre en la unidad. Corrida de hoy: **todas las líneas en
verde, 0 % sobre la base**.

| motor | instrucciones | coste estático |
|---|---:|---:|
| analog_render | 228 | 138 |
| phase_render | 329 | 94 |
| lofi_render | 544 | 197 |
| formant_render | 571 | 238 |
| wheel_render | 276 | 257 |
| noise_render | 594 | 218 |
| fm6_render | 568 | **292** |
| drum_render | 231 | 10 (el trabajo está en `dx_run`: 426) |

El motor más caro por bloque es **FM6 (292)**, con FORMANT y WHEEL cerca. El presupuesto del bloque son
32 muestras a 44,1 kHz ≈ 725 µs para *todas* las voces de *todas* las pistas a la vez, y `tests/regress` es el
chequeo de CPU por bloque de la mezcla completa: hoy sin entradas fuera de presupuesto.

## 3. Auditoría de la biblioteca de presets

Mediana 22 presets por motor. **ANALOG tiene 68 (3,1× la mediana)** y FM6 37 (1,7×), mientras NOISE, WHEEL y
WAVETABLE están en 22 justos y DRUM-X en 18. Es exactamente la desproporción que pide mirar la fase: el motor
más viejo acumuló sonidos de todas las épocas.

Recuperación de XIP medida, si se recorta sólo lo redundante (mismos valores con otro nombre, variantes
mínimas):

| recorte | libera |
|---|---:|
| ANALOG 68 → 48 | 720 B |
| FM6 37 → 30 | 252 B |
| PHASE 32 → 28 | 144 B |
| **total** | **1 116 B** |

Eso llevaría la holgura de XIP de ~3 416 B a ~4 532 B. No lo ejecuté: recortar 27 sonidos exige escuchar y
comparar los 68 de ANALOG uno por uno, y borrar presets de fábrica es irreversible para quien los usa (los
proyectos guardan el id del preset). Queda como tarea con su método: listar, agrupar por parámetros idénticos o
casi idénticos, y descartar sólo los duplicados demostrables.

## 4. Veredicto sobre un motor nuevo

**No conviene incorporarlo todavía, y no por falta de ideas.** Los números:

- **XIP**: quedan ~3 416 B (580 196 B de imagen contra el tope de 583 612 B medido en Fase 6). Un motor aditivo
  de 6–8 parciales cuesta, por analogía con los existentes, entre 2,6 y 3,0 KB: el código de FM6 (1 714 B) o de
  FORMANT (1 730 B), sus 8 descriptores de parámetro, su entrada en `ENGINES[]` y una tabla de ~22 presets
  (792 B). Entraría con **menos de 400 B de margen**, que es exactamente el margen que la Fase 3 se comió con un
  cambio visual.
- **RAM**: 3 228 B libres de `.data+.bss` y 9 248 B de pool spare. Un motor polifónico de 8 parciales necesita
  estado por voz (8 amplitudes + 8 fases + su envolvente espectral): 8 voces × ~40 B = 320 B de `.bss` o del
  pool, más el `engine_t`. Entra, pero deja el pool por debajo del mínimo de 8 KiB que fija `AGENTS.md`.
- **CPU**: el motor más caro hoy es FM6 con coste estático 292. Un aditivo de 8 parciales hace 8 sumas de seno
  por muestra y por voz: 8 voces × 8 parciales = 64 osciladores por bloque, más que los 4 operadores de FM6 con
  su polifonía. **No hay hoy ningún instrumento que mida el coste por bloque de un motor aislado**: `regress` mide
  la mezcla completa y `target_budget` cuenta instrucciones estáticas. Sin ese instrumento, cualquier afirmación
  de que un aditivo entra en los 725 µs por bloque sería una corazonada — justo lo que esta fase pide no hacer.

**Decisión**: primero **reclamar los ~1,1 KB** de la tabla de arriba y **construir el banco de medida por motor**
(un runner de host que renderice N bloques de un solo motor a una sola voz y a la polifonía máxima y reporte
µs/bloque, junto a `regress`). Con esos dos datos, la decisión entre el motor aditivo-espectral y profundizar
EDIT/LFO deja de ser una opinión.

## 5. Qué queda pendiente, y por qué

- **Motor nuevo**: no implementado. Motivo arriba, con números.
- **Recorte de presets**: medido y cuantificado, no ejecutado (necesita escucha y comparación de los 68 de
  ANALOG; borrar ids de fábrica es destructivo para los proyectos guardados).
- **Banco de CPU por motor**: no existe; hay que escribirlo antes de decidir.
- **Publicación (Fase 5)**: sigue bloqueada. `emcc` no está en el entorno (el emulador WASM no se puede
  reconstruir) y `gh` no tiene sesión (`You are not logged into any GitHub hosts`), así que no hay tag, ni
  release v1.2.0, ni despliegue de Pages desde acá. El árbol sigue en `v1.1.0` en `felucca.c`.
