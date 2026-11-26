# Fase 2/4 — RAM, SRAM, POOL, LOWBUF y recursos internos (2026-11-26)

Clases de medida: **[AC791N]** símbolos y ventanas del ELF enlazado por el toolchain pi32v2;
**[HOST x86-64]** pruebas de pila; **[leído]** código, sin ejecutarlo. Sin unidad FM-1.

El mapa completo se regenera con el build delante:

```bash
JIELI_TOOLCHAIN=$HOME/.jieli/toolchain ./build.sh
python3 tools/mem_map.py --top 8
```

## 1. Mapa real (commit `8444cc8c`, ELF de `./build.sh`)

| ventana | base | usado / total | libre |
|---|---|---|---|
| `.data+.bss` (presupuesto SDK) | `0x01C08000` | **75 664 / 98 304 B (77,0 %)** | **22 640 B** |
| LOWBUF | `0x01C00000` | **28 488 / 32 768 B (86,9 %)** | **4 280 B** |
| POOL | `0x01C20000` | **296 156 / 327 680 B (90,4 %)** | **24 356 B** (+ 7 168 B de pilas de core 1 dentro de la ventana) |
| NOINIT | `0x01C7C000` | **15 620 / 15 696 B (99,5 %)** | **76 B** |
| pila de usuario | `0x01C76900` | 14 080 B, guardas de 256 B | — |
| pila de excepción | `0x01C7A100` | 7 936 B, guardas de 256 B | — |

No hay relleno de alineación en ninguna ventana: **0 B** de hueco entre símbolos en las cuatro
(39 símbolos en POOL, 859 en `.data+.bss`, 11 en LOWBUF, 9 en NOINIT). Lo que ocupa, ocupa.

### Mayores consumidores

`POOL`
: `dly_buf` 131 072 B (línea de delay, 1,49 s a 41,7 kHz, la usa `delay_samples()` hasta `DLY_LEN−8`),
`cv_buf` 59 520 B (canvas de 240×248), `sl_buf` 32 768 B, `rv_buf` 32 768 B, `up_bank` 6 160 B,
`comb_buf` 6 144 B (6 pistas × 1 024 B), `reel` 4 992 B (32 tomas).

`.data+.bss`
: `trk` 10 584 B (6 × 1 764 B de estado vivo por pista: matriz, punch-in, secuenciador, glide),
`eng_arena` 7 296 B (6 × 1 216 B, unión por pista del motor activo), `WT_GEN` 6 192 B (medio ciclo
por frame, reconstruido en el primer note-on), `fm6_exp2_tab` 4 100 B y `SINE` 2 050 B (tablas en RAM
a propósito: lectura de RAM en vez de XIP), `WT_COEF` 2 048 B (expandido al arrancar).

`LOWBUF`
: `proj_scratch` 9 644 B (`project_t` de trabajo: captura → `proj_pack`, o `proj_import` →
`project_restore_runtime`), `sec_stage` 9 446 B (sección de canción que el main loop decodifica y
el ISR intercambia), `st_buf` 3 840 B (exactamente `sizeof(project_store_t)`).

`NOINIT`
: `proj_slot[4]` 15 360 B (4 × `project_store_t` de 3 840 B), `felucca_dbg` 76 B, `fm1_crash` 64 B,
`cv_win`, `panel`, `settings`.

## 2. Qué se puede recuperar, con ciclo de vida medido

| candidato | bytes | por qué NO se tocó en esta fase |
|---|---|---|
| `dly_buf` | 131 072 | Se direcciona hasta el final: `delay_samples()` llega a `DLY_LEN−8` y `DLY_EDGE` cubre el Hermite y el wow. Recortarlo acorta el TIME máximo (1,49 s → menos): es una función, no un desperdicio. |
| `cv_buf` | 59 520 | Canvas único de 240×248; el barrido de UI ya lo mide paleta por paleta. |
| `sl_buf`, `rv_buf` | 65 536 | Líneas de slicer y reverb, ambas indexadas por máscara sobre su longitud completa. |
| `trk` | 10 584 | Estado vivo por pista, sin copia de patrón: no hay duplicación que quitar (el patrón vive en `song`/`proj`). |
| `eng_arena` | 7 296 | **Ya es una unión por pista** del motor activo (`core.h eng_arena_t`, 1 216 B). Es la arquitectura que esta fase buscaría, ya aplicada. |
| `WT_GEN` | 6 192 | Ya reducido a medio ciclo con simetría impar (−6 096 B en su día) y reconstruido en RAM al primer note-on. |
| `proj_scratch` | 9 644 | Es el snapshot de trabajo de guardar/cargar/importar: el camino E/S necesita un `project_t` validado *antes* de tocar el estado vivo. Eliminarlo es un refactor del formato de proyecto, no un recorte. |
| `sec_stage` | 9 446 | **No se puede unir con `proj_scratch`**: `sec_stage` lo lee el ISR mientras el main loop lo escribe; compartir memoria con un guardado (main loop) corrompería la canción en reproducción. |
| `proj_slot[4]` | 15 360 | 4 proyectos en NOINIT (sobreviven a un reset). Bajarlo a 1 plaza es quitar una función; moverlo a `.bss`/POOL pierde la semántica de NOINIT. |
| `felucca_dbg` | 76 | Diagnóstico del ISR que hoy sobrevive a un reset, igual que `fm1_crash`. Moverlo daría 76 B de NOINIT y consumiría 76 B de `.bss`: traslado, no recuperación. |

Ninguna de las ventanas tiene relleno, ni símbolos muertos: `xip_audit.py` y el mapa coinciden en que no
hay `.bss` sin referencias. **No se aplicó ningún cambio en esta fase**: todas las opciones medidas son
o una función (delay máximo, plazas de proyecto, diagnóstico tras reset) o un refactor del camino de
proyecto/canción que exige su propia batería de guardado, carga e importación. Recortar sin eso sería
exactamente lo que la fase prohíbe.

## 3. Pruebas ejecutadas

- **[AC791N]** `./build.sh` → `.data+.bss` 75 664/98 304 B (22 640 libres), LOWBUF 28 488/32 768,
  POOL 296 156/327 680, NOINIT 15 620/15 696; imagen 583 608/583 612 B; las cuatro `ASSERT` del linker
  script (LOWBUF, NOINIT, XIP, POOL) pasan.
- **[HOST x86-64]** `tests/stack_watermark`: escena más profunda **4 799 B** (40 cuadros de UI) de un
  presupuesto de 24 576 B; mix de 6 pistas 1 751 B; captura+pack+unpack de proyecto 0 B. Las pilas del
  destino son 13,75 KiB / 7,75 KiB con guardas: el margen lo fija el linker, no esta prueba.
- `python3 tests/memory_map_test.py` → **omitida** en este entorno: necesita el `objdump` de JieLi, que
  no está en el toolchain instalado. No se contó como pasada.
- `python3 tools/mem_map.py` (nuevo) reproduce la tabla de §1 desde el ELF, con consumidores y huecos.

## 4. Propuesta para la siguiente pasada (con su riesgo)

1. **Guardar/cargar sin `project_t` de trabajo** (~9 600 B de LOWBUF): `proj_pack` leyendo el estado vivo
   con las mismas funciones que usa `project_capture`, y `proj_import` validando sobre la marcha en vez
   de decodificar a un snapshot. Riesgo: formato FUNB/FUNC y compatibilidad con los stores antiguos
   (`PROJ_STORE_V7/V8/V9/VA`) — necesita los tests de importación de cada versión y una prueba de guardado
   con la canción sonando.
2. **`proj_slot` a 2 plazas + carga desde flash** (7 680 B de NOINIT): hoy NOINIT está al 99,5 % y
   cualquier campo nuevo rompe el enlace. Riesgo: cambia cuántos proyectos sobreviven a un reset.

Ninguna de las dos se aplicó aquí porque exigen verificar guardado, carga, edición y cambio de preset
bajo reproducción, y eso es una fase en sí misma, no un ajuste.
