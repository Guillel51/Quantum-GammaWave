# Etapa 4/8 — una capacidad de síntesis nueva: PHASE WAVE 8, SYNC

Clases: **`[HOST]`** = x86 (`tests/pd_sync_test.c`, `tests/regress`); **`[AC791N]`** = el linker del destino
(`build/felucca.bin`, log de `./build.sh`). Nada `[HW]`.

## 1. Qué se agregó de verdad

**`PHASE > WAVE = SYNC`**: la novena onda del motor de distorsión de fase. El motor lee un ciclo de `-cos` a través
de una fase doblada; SYNC hace otra cosa: la portadora corre a una **relación que fija DCW (1× … 8×, en octavos,
continua)** y el ciclo maestro la **reinicia** en cada período, así que el cruce cae dentro del período y deja el
borde que da el timbre sync: en 1× es la senoide pura, en 8× es duro y metálico, y todo el recorrido es continuo.

Ninguna otra onda del motor hace eso: las ocho existentes (SAW, SQR, PLS, DSIN, SPLS y las tres RESONANCE con
ventana de sierra, triángulo y trapecio) doblan la fase; SYNC la **trunca contra el maestro**. ANALOG tiene un SYNC
de otra familia (reinicia la sierra del oscilador 2); esto es un sincronismo dentro del motor de fase.

### Controles e integración

- **Knob existente**: `WAVE` (EDIT) pasó de 0..7 a **0..8**, con el nombre `SYNC` en la lista del engine
  (`N_PD_WAVE`), que es de donde la UI dibuja las opciones. **DCW** es la relación. No se agregó ningún parámetro
  nuevo: no hay duplicados con otro nombre ni campos nuevos en el formato de proyecto o preset.
- **Presets/MIDI/automatización**: viaja como cualquier valor de WAVE (int8 en `preset_t`, CC del motor y matriz
  de modulación ya lo alcanzan). Guardado y carga no cambian de formato: valores viejos 0..7 suenan igual que antes.
- **Compatibilidad**: la fase se enmascara ahora a **15** en vez de 7 y se limita a 8, así que un valor guardado
  fuera de rango toca la última onda en vez de caer en una rama indefinida.
- **Preset de demostración**: **PHASE > SYNC CATHEDRAL** (WAVE 8, DCW 76 ≈ 2.4×, brillo lento por LFO y envíos de
  chorus/reverb). La biblioteca pasa de 286 a **287** sonidos; no se reemplazó ni se borró ninguno.

### Coste `[AC791N]`

| | antes | después | Δ |
|---|---|---|---|
| imagen (app slot) | 579 876 B | **579 972 B** | **+96 B** |
| libre en el slot | 3 736 B | **3 640 B** | −96 B |
| RAM / pool / `.lowbuf` / `.noinit` | — | sin cambio | 0 |

`tests/target_budget.py`: exit 0, ningún lazo por encima del +10 % (la onda nueva es un `case` del mismo lazo, no
un lazo nuevo).

## 2. Prueba nueva

`tests/pd_sync_test.c` (3 checks):

- **nueva**: una nota A4 con las **nueve** ondas da **nueve hashes distintos** — SYNC no es ninguna de las otras
  ocho.
- **tocable**: SYNC en DCW 8 / 64 / 120 da **tres sonidos distintos**: el knob mueve la relación.
- **sana**: ninguna muestra no finita.

Lo que no prueba: cómo suena. Eso lo dicen el preset y el oído; el test sólo dice «distinto».

## 3. Verificación ejecutada

- `tests/regress`: **307 golden renders, 0 cambiados, 1 nuevo** (el preset agregado), 0 fallos de salud,
  0 de voz/routing, 0 entradas de CPU fuera de presupuesto, 0 crashes. Dos corridas idénticas.
- `tests/preset_library_test.py`: **287** sonidos (PHASE 34), 10 kits DRUM y los alias legacy intactos.
- `tests/preset_categories_test.py`: 287/287, una categoría primaria cada uno.
- `tests/groups.sh presets` y `groups.sh web`: 0 (export MIDI 64 checks, 0 fallos).
- `tests/resources_test.py`: 13/13 con todos los pisos; `tests/target_budget.py`: exit 0.
- Build de destino completo (`.fwsc` 610 933 B, identidad FM-1_965).

## 4. Lo que se evaluó y **no** se hizo, con razón

La etapa pedía también un motor nuevo y capacidades en cada motor. El muro es el mismo de las etapas 1–3 y ahora
está medido al byte:

- **Un motor nuevo**: un `engine_t` completo (render, envolventes, EDIT, presets, categorías) no baja de varios
  KB de XIP; el slot tiene **3 640 B**. No entra sin quitar algo, y no hay nada muerto que quitar (el mapa de
  recursos no muestra objetos sin uso).
- **Parámetros nuevos en cualquier motor**: cada uno suma a `track_t` (RAM: 4 692 B libres), al formato de
  proyecto (compatibilidad hacia atrás) y a la UI. Ninguno cabe sin rediseñar el almacenamiento primero.
- **PHASE LINE = FOLD**: ya estaba diseñado y medido en la Fase 6 (+96 B de XIP) y descartado porque +13
  instrucciones por muestra rompían el presupuesto de DSP; sigue fuera por la misma razón medida.
- **LOFI / NOISE / FM6 / DRUM-X / WHEEL**: se revisaron sus tablas de knobs; todas tienen su rango completo
  ocupado (LOFI 0..5 oleadas, NOISE 0..N colores, FM6 algoritmos y feedback ya presentes). Agregar una opción
  nueva exige ampliar un enum y **su** código de render: es la misma cuenta de XIP que SYNC, pero sin un hueco
  tan barato como el de la onda 8 (96 B).

## 5. Pendientes para la Etapa 5

1. **Liberar XIP** es el requisito de todo lo demás: con ~3.6 KB sólo entran capacidades del tamaño de SYNC.
   Candidatos medidos: los 7 680 B de `UAC_FIR` (sin simetría ni filas repetidas, no compresible sin pérdida),
   las tablas de iconos (ya a nibble), y el código de UI (`draw_graph` 15 700 B, `draw_columns` 13 636 B), donde
   sí hay margen para medir código común antes de tocar nada.
2. **Segunda onda nueva** del mismo tipo (p. ej. SYNC con diente de sierra en vez de seno, u otro ratio
   logarítmico) cuesta ~100 B más y ya tiene su test.
3. **Un motor nuevo** sólo será posible cuando el slot tenga ~10 KB libres o cuando el motor pueda vivir en
   `.hot_text`/RAMTEXT con presupuesto propio.
