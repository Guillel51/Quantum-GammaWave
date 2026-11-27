# FASE 2/8 — RAM, SRAM, POOL, LOWBUF y NOINIT: mapa real y estado de cada candidato (2026-11-27)

Clase de todas las cifras de §1 y §2: **[AC791N]**, `./build.sh` con el toolchain pi32v2 sobre el SDK AC79,
leídas del mapa del linker (`build/felucca.map`, `tools/mem_map.py`, `tools/mem_budget.py`). No hay ninguna
medida de unidad física aquí, y ninguna cifra se copió de un documento anterior.

## 1. Lo que se conservó de la fase anterior (comprobado, no supuesto)

- `undo_st[2]` **sigue en POOL** (`firmware/src/project.c`: `static project_store_t undo_st[2]
  __attribute__((section(".pool")));`), 7 680 B fuera de RAMTEXT. No se tocó.
- `tools/mem_budget.py` **sigue midiendo RAMTEXT como 0x5200** (el valor de `app.ld`), con la nota de la
  fase 2/6: la ventana de 0x6000 dejaba pasar el chequeo con 3 584 B que no existen. No se tocó.

## 2. Mapa medido (después del build de esta fase, idéntico al inicial: no se aplicó ningún cambio)

| ventana | dirección | usado | libre | suelo |
|---|---|---|---|---|
| `.data+.bss` (RAM) | 0x01C08000 | **86 072** / 98 304 (87,6 %) | 12 232 | 4 096 |
| LOWBUF | 0x01C00000 | **28 408** / 32 768 (86,7 %) | **4 360** | 1 024 |
| POOL | 0x01C20000 | **303 836** / 327 680 (92,7 %) | 16 676 (+7 168 reservados dentro) | 4 096 |
| RAMTEXT | — | **12 768** / 20 992 | 8 224 | 1 024 |
| NOINIT | 0x01C7C000 | **15 620** / 15 696 (**99,5 %**) | **76** | 0 |
| pila de usuario | 0x01C76900..0x01C7A000 | 14 080 con guardas | — | — |
| pila de excepción | 0x01C7A100..0x01C7C000 | 7 936 con guardas | — | — |

Relleno de alineación en las cinco ventanas: **0 B** (`mem_map.py`: 871 símbolos en la ventana de RAM, 11 en
LOWBUF, 40 en POOL, 9 en NOINIT; ningún hueco).

Consumidores, por ventana y de mayor a menor:

- **LOWBUF (28 408)**: `proj_scratch` 9 644, `sec_stage` 9 446, `st_buf` 3 840, `ota_stage.hdr` 944,
  `midi_sel_on` 1 024, `fm6_patch` 936, `ed_out` 600, `scope_snap` 512. Los dos grandes son el 67 %.
- **NOINIT (15 620)**: `proj_slot[4]` 15 360, `felucca_dbg` 76, `fm1_crash` 64, `cv_win` 48, `panel` 32,
  `settings` 16, `bootguard` 12, `fm1_mc_bar` 8.
- **POOL (303 836)**: `dly_buf` 131 072, `cv_buf` 59 520, `sl_buf` 32 768, `rv_buf` 32 768, `undo_st` 7 680,
  `up_bank` 6 160, `comb_buf` 6 144, `reel` 4 992, y 32 símbolos menores.
- **RAM (86 072)**: `trk` 10 584, `ramdata` 8 976, `eng_arena` 7 296 (6 × 1 216, la unión del motor activo por
  pista), `WT_GEN` 6 192, `fm6_exp2_tab` 4 100, `ANALOG_PRESETS` 2 482, `SINE` 2 050, `WT_COEF` 2 048,
  `ua_ring` 2 048, `abuf` 2 048.

## 3. Candidatos, uno por uno, con el motivo técnico

| candidato | ahorro | región | veredicto y por qué |
|---|---|---|---|
| `proj_slot[4]` → 2 plazas | 7 680 | NOINIT | **Rechazado.** No es una caché: son las cuatro plazas que la canción y los respaldos direccionan. `proj_obj(slot) = OBJ_PROJECT0 + (slot & 3)` y `OBJ_SONG1 + (song-1)*4 + slot`: cada canción ocupa **cuatro** objetos en flash, y `chain_config.row[i].slot` guarda el índice. Bajarlo a dos cambia el mapa de objetos en flash, deja ilegibles las canciones y las cadenas que apuntan a 2/3, y `editor_backup` devuelve punteros a `proj_slot` por índice (ids 2..5). Es una función, no un desperdicio. |
| `proj_scratch` fuera del camino (pack/import en streaming) | ~9 640 | LOWBUF | **No aplicado en esta pasada.** El diseño seguro existe y está descrito abajo, pero es un refactor del camino de proyecto: `proj_pack`/`proj_import` contra el estado vivo, con FUN7..FUNB, FUN1..FUN6, autosave, guardado con la canción sonando y los respaldos. Sin esa batería no se toca. |
| `sec_stage` compartido con `proj_scratch` | 9 446 | LOWBUF | **Imposible.** `sec_stage` lo lee el ISR de audio mientras el main loop lo escribe (se publica con `RING_PUBLISH()` + `ready`); compartirlo con un guardado corrompería la canción en reproducción. |
| `proj_wire_u` (3 840) eliminado | 3 840 | POOL | **Imposible.** Ya es el scratch compartido del main loop: staging de guardado/carga, del índice de canciones y de los respaldos del editor (`ED_BK_RAW`). Quitarlo obliga a inventar otro buffer. |
| `ota_stage.hdr` 1 024 → 944 | **80** | LOWBUF | **Aplicado.** El bloque de actualización leía dos mitades de 512 B en un buffer de 1 KiB; el encabezado del UFW son 0x40 B y hasta once entradas de 0x50, o sea 944 B exactos. La segunda lectura pide ahora lo que queda (432 B) y `_Static_assert(0x40 + 11 × 0x50 == 944)` sostiene el límite: un formato con más entradas no compila. **LOWBUF 28 488 → 28 408 B, 80 B netos**, medido en el mapa. |
| `dly_buf`, `sl_buf`, `rv_buf`, `cv_buf` | hasta 65 536 | POOL | **Rechazado.** Son el TIME máximo del delay, las líneas indexadas por máscara y el canvas completo del gráfico: recortarlos recorta funciones y dibujo, no desperdicio. |
| `up_bank` (6 160) bajo demanda | 6 160 | RAM/POOL | **Rechazado.** Es el banco de voces FM6 en RAM para el note-on; sacarlo pide leer flash desde el ISR. |
| Pilas (14 080 + 7 936) | — | — | **No tocado.** El margen lo fija el linker con guardas; no hay medición en el AC791N que autorice recortarlas (la marca de agua de `tests/stack_watermark` es del ABI del host). |
| `eng_arena` (6 × 1 216) | — | RAM | **No es desperdicio.** Es la unión del estado del motor activo por pista, dimensionada al motor más grande; el `memset` la limpia al cambiar de motor. |

## 4. El diseño del streaming, para la pasada que lo ejecute

`proj_scratch` (9 644 B) sólo existe para pasar de un registro empaquetado (3 840 B) al estado vivo y
viceversa. El camino seguro, sin leer datos incompletos:

1. **Validar antes de escribir.** El registro lleva `magic`, `size` y `sum` (FNV-1a sobre todo el registro):
   se comprueban sobre los 3 840 B **antes** de tocar `trk`, `song` ni `chain`. Un registro corrupto no
   escribe nada. `chain_valid`/`motion_valid` se comprueban sobre el estado vivo ya decodificado, y el
   guardado de la sección anterior sigue disponible para volver atrás si falla.
2. **Import por secciones**: globales, luego cada pista (parámetros, pasos, matriz, patch FM6), luego el kit
   DX, MORPH y FUGUE. Cada sección escribe sólo sus campos, así que un fallo deja el estado anterior en el
   resto — y se detecta antes de empezar por el `sum`.
3. **Export por secciones**: `proj_pack_live` produce exactamente los mismos bytes leyendo el estado vivo con
   las mismas expresiones que hoy usa `project_capture` (que ya es una copia campo a campo), escribiendo el
   registro por bloques en `proj_wire_u`, que ya es el staging de 3 840 B.
4. **Pruebas obligatorias antes de aterrizarlo**: guardar/cargar en FUN7..FUNB y FUN1..FUN6, autosave,
   guardado con la canción sonando y con el layer vivo, respaldo/restauración del editor, undo/redo, seis
   pistas y todos los motores. La suite actual cubre casi todo eso (`project_test`, `backup_test`,
   `song_chain_test`, `regress`), y es la puerta para el cambio.

Con eso, `proj_scratch` desaparece y LOWBUF pasa de 4 280 B libres a ~13 900 B: **9 640 B netos**, no un
traslado.

## 5. Estado de la fase

La única recuperación aterrizada es la del encabezado del OTA: **80 B netos de LOWBUF**, medidos
(28 488 → 28 408 B usados, 4 280 → 4 360 libres) y verificados con `tests/ota_test` contra el `.fwsc` de esta
compilación. Es real, es segura y es pequeña: no llega a los ~9 640 B que da el streaming de `proj_scratch`
ni a los 7 680 B de `proj_slot`. El mapa está medido y cada candidato tiene su
motivo técnico escrito, pero la condición de cierre de la fase —al menos una mejora real, segura y medida—
**sólo se cumple en su mínimo**: los dos candidatos grandes (`proj_scratch` y `proj_slot`) son exactamente los dos
que exigen la batería completa de proyecto/canción, y aterrizarlos sin ella sería lo que la fase prohíbe.

Riesgo vivo que conviene atender primero, aunque no sea recuperación: **NOINIT está al 99,5 % (76 B
libres)**. Cualquier campo nuevo en `.noinit` rompe el enlace. La salida segura es llevar `proj_slot` a POOL
(16 676 B libres), que **no** recupera SRAM y por eso no se cuenta como ahorro; se propone como medida de
seguridad independiente.
