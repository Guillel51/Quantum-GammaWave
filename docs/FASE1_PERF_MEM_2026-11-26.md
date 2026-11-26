# FASE 1/6 — Rendimiento y memoria: qué se midió, qué se cambió, qué no se toca

Clases: **[AC791N]** build estático con el toolchain pi32v2 y el SDK; **[HOST]** pruebas y barridos en x86-64.
Ninguna cifra es una medición en una FM-1 física: no se flasheó ni se tocó la unidad.

## 1. Con qué se midió (y qué no se puede medir aquí)

| herramienta | qué da | clase | ¿disponible? |
| --- | --- | --- | --- |
| `tools/ram_report.py` | secciones del linker, símbolo por símbolo, por región | [AC791N] | sí (con `JIELI_TOOLCHAIN`) |
| `tests/target_budget.py` | instrucciones dentro de los bucles de cada función del audio, divides y llamadas, desde `build/felucca.dis` | [AC791N] | sí |
| `tools/dsp_bench.py` | todo lo anterior junto + ventanas de memoria | mixta | sí |
| `tests/regress` | 316 renders dorados, salud, voces, y CPU por muestra | [HOST] | **CPU no utilizable aquí**: no hay contador de kernel y el reloj del proceso tiene 10 ms de grano (p50 0.0 µs, max 10 000 µs) |
| `tests/audio_stress` | pico, DC, saltos, no finitos, p50/p99 por bloque | [HOST] | estable en lo que no es tiempo |
| valgrind/callgrind (`tests/cpu_vg.py`) | instrucciones exactas por muestra | [HOST] | **no instalado** |

Conclusión metodológica: en este entorno el **CPU solo se puede comparar por código generado para el AC791N**
(`target_budget.py`, exacto y repetible) y por la identidad de los 316 renders. El tiempo de host no sirve de
prueba y no se usa como tal en este documento.

## 2. Inventario de memoria [AC791N]

| región | origen | tamaño | usado | libre |
| --- | --- | --- | --- | --- |
| XIP | `0x02000120` | 583 612 | **578 856** | **4 756** (99,2 %) |
| POOL | `0x01C20000` | 327 680 | 303 328 | 24 352 |
| RAM (.data+.bss) | `0x01C08000` | 98 304 | 75 632 | 22 672 |
| LOWBUF | `0x01C00000` | 32 768 | 28 488 | 4 280 |
| NOINIT | `0x01C7C000` | 15 696 | 15 572 | 124 |
| RAMCODE | `0x01C70C00` | 23 552 | 3 940 | 19 612 |

Los mayores consumidores de RAM, con el archivo que los declara:

| B | símbolo | dónde | qué es |
| --- | --- | --- | --- |
| 10 584 | `trk` | `audio.c` | las seis pistas enteras (parámetros, ventanas de voces, secuenciador) |
| 7 296 | `eng_arena` | `eng_drum.c`/`voice.c` | estado del motor activo por pista (unión, 1 216 B × 6) |
| 6 192 | `WT_GEN` | `dsp.c` | tablas generadas al arrancar: lectura en RAM, no en XIP |
| 4 100 | `fm6_exp2_tab` | `fm6_core.c` | la curva exp2 de FM6, construida sin coma flotante |
| 2 050 | `SINE` | `drumx_voice.c` | seno de DRUM-X |
| 2 048 ×2 | `abuf`, `ua_ring` | `audio.c`, `usb.c` | bloque de DMA de la ALNK0 y anillo de USB audio |
| 2 048 | `WT_COEF` | `eng_wavetable.c` | 512 amplitudes Q30 expandidas de 64 B |
| 1 728 | `slot_arena` | `eng_additive.c` | estado del motor de cada ranura de voz (unión) |
| 1 656 | `vpool` | `voice.c` | las 18 ranuras de voz compartidas |

POOL (buffers grandes): `dly_buf` 131 072, `cv_buf` 59 520 (canvas), `rv_buf` 32 768, `sl_buf` 32 768,
`up_bank` 6 160, `comb_buf` 6 144, `reel` 4 992, `cho_buf` 4 096, `rpre_buf` 4 096.

Lectura del inventario: **no hay duplicación que quitar**. Las dos "arenas" (`eng_arena`, `slot_arena`) son
uniones por pista/ranura, no copias; las tablas en RAM (`WT_GEN`, `WT_COEF`, `fm6_exp2_tab`, `SINE`) se
construyen una vez al arrancar para no pagar una lectura de XIP por muestra, y su coste en flash sería el mismo
o mayor que el que ahorran. Devolverlas a flash no ahorra memoria total y **gasta la región que está al 99,2 %**.

## 3. Asignación dinámica, fragmentación y recursos compartidos

- `malloc` / `calloc` / `realloc` / `free`: **ninguno** en `firmware/src` ni `firmware/hal`. Todo es estático o
  POOL; no hay fragmentación posible. (`pb_free` de `perform.c` es un índice en un array fijo, no el `free` de
  la libc.)
- Un solo `abuf` de DMA; un anillo de scope por canal; los estados de motor se comparten por unión y se limpian
  al cambiar de motor (`voice.c` `memset(eng_arena[...])`, `slot_arena[...]`).
- `beat_samples()` (fx.c) ya memoiza el cociente del tempo con sus tres entradas: el `div_samples()` que lo usa
  no divide si el tempo no cambió.

## 4. Rutas críticas y concurrencia

- **ALNK0 (audio)**: por media buffer, bloques de `CTL` muestras; `mix_block` es la ruta. Presupuesto del
  bloque: **725 µs** (32 muestras a 44,1 kHz).
- **TIMER5**: 10 kHz, anidado en el audio y por encima de él; hace teclado/LEDs y el contador de ms, y solo llama
  a `usb_poll`/`uart_midi_poll` cuando el audio no está renderizando. Coste estático: 1 252 instrucciones por
  tick, 1 divide (aceptable: es el escaneo que evita el parpadeo de los LEDs).
- **Dos núcleos**: `mc_split` / `mc_run` / `mc_wait` reparten pistas y buses; si el segundo núcleo no contesta,
  `mc_lost()` deja el instrumento en un núcleo con las pistas recuperadas, y `fx_bad` + `fx_trip` protegen la
  salida. `fx_insane()` revisa cada bloque que el retorno de los buses no se haya ido de rango.
- **Reparto por muestra (coste estático por función, [AC791N], `target_budget.py`)**:

| función | coste | divides | qué es |
| --- | --- | --- | --- |
| `analog_render` | 138 | 0 | ANALOG, una voz |
| `phase_render` / `_noise` | 91 / 111 | 0 | PHASE |
| `lofi_render` | 201 | 0 | LOFI |
| `formant_render` | 247 | 0 | VOICE formante |
| `wheel_render` | 164 (+1 llamada) | 0 | WHEEL, todos los parciales |
| `fm6_render` | 305 | 0 | FM6, seis operadores |
| `dx_run` | 396 | 1 (por bloque) | DRUM-X, un golpe |
| `noise_render` | 218 | 0 | NOISE |
| `mix_block` | 116 | 0 | buses, master, leveler |
| `BLOCK_PATH` | 34 835 | — | el bloque completo (los "cálculos de eventos" son por paso, no por muestra) |

Los bucles de `events_block` son de evento (paso, nota, latch): sus 104 llamadas internas se pagan al paso, no
por muestra, y por eso `BLOCK_PATH` estático no es un coste por muestra.

## 5. Lo que se cambió en esta fase

1. **`fx_trip()` (fx.c)**: un `for` de 65 536 iteraciones ponía a cero `dly_buf` (131 072 B de stores) y la línea
   siguiente ya hacía `memset(dly_buf, 0, sizeof dly_buf)`. Eliminado. Es la ruta de **fallo** del audio, la que
   tiene menos tiempo libre: se va un 131 072 B de trabajo de dentro del bloque.
2. **`ui_fxb.c` (FX > MASTER)**: `audio_out_peak(0) > audio_out_peak(1) ? audio_out_peak(0) : audio_out_peak(1)`
   llamaba hasta **cuatro** veces a una función que recorre las 256 muestras del anillo del DAC. Ahora una
   lectura por canal (dos recorridos), mismo valor.
3. **`ui_tech.c` (GLOBAL)**: `div_samples(2)` se llamaba dos veces por cuadro (es una división, vía
   `beat_samples()`); ahora una vez y se reusa para el compás y el paso.
4. **`tests/ui_pixels.txt`**: la base de píxeles se había grabado con la cadena de `git describe`, y
   `tests/run_tests.sh` compila con la versión de `firmware/src/felucca.c` (`v2.12.0`): la puerta de píxeles
   **no podía pasar en un árbol limpio** (48 cuadros de ABOUT y SPLASH, exactamente el texto de la versión).
   Regenerada con la receta del propio suite: 3 305 cuadros y verificación idéntica.

## 6. Antes / después, medido

| [AC791N] | antes | después |
| --- | --- | --- |
| imagen | 578 936 B | **578 856 B** (−80) |
| XIP código | 452 480 B | **452 392 B** (−88) |
| XIP constantes | 121 676 B | 121 684 B (+8) |
| RAM .data+.bss | 75 632 B | 75 632 B (=) |
| POOL | 303 328 / 24 352 libre | igual |
| LOWBUF / NOINIT | 28 488 / 15 572 | igual |
| `BLOCK_PATH` estático | 34 835 | **34 835** (=) |
| alcance de llamadas desde RAM code | 9 056 B | 9 144 B |

Lo que se elimina, en unidades del código y no del reloj: 131 072 B de stores en la ruta de fallo del audio;
hasta 512 lecturas del anillo del DAC por cuadro en FX > MASTER; una división por cuadro en GLOBAL. El coste
por muestra de los motores **no se movió** (tabla de §4 idéntica), que es lo que se busca cuando no se cambia
el sonido.

## 7. Verificación

| prueba | resultado |
| --- | --- |
| `tests/regress` (316 renders dorados, salud, voces, enrutado) | **316 idénticos, 0 cambiados**, 0 fallos de salud, 0 de voces |
| `target_budget.py` | todas las líneas dentro de presupuesto (BLOCK_PATH +4 % del presupuesto, no del código) |
| `ui_pixels.py` | **3 305 cuadros idénticos** a la base regenerada |
| `ui_render` (lint, alineación, `--regions/--idle/--bands/--headers`) | 176 hallazgos de siempre, **0 desalineaciones** en 4 069 937 mediciones |
| `ui_test`, `morph_depth` | pasan |
| `audio_stress 8` (6 motores, 6 pistas, todos los buses) | pasa: pico 0,300/0,300, 0 muestras no finitas, 0 saltos > 0,5 |
| `poly_stress_test`, `poly_voices_test` | pasan (sin voces colgadas, sin notas denegadas) |
| `reverb_test` (incluye el camino de `fx_trip`) | pasa |
| `chorus_rate_test`, `control_map_test`, `phase2_test`, `dly_echo_test`, `six_track_live_test` | pasan |
| `./build.sh` (AC791N) | pasa los cinco controles (RAM, pool, alcance, registros, XIP) |

## 8. Pendientes que esta fase encontró y **no** arregla

- **`tests/cloud_hot_test.c`, criterio del sostenimiento**: a `SIZE 127`, `RATE 127`, la cola mide 80,8 dB
  contra un inicio de 86,9 dB y la prueba pide "dentro de 6 dB" (80,9): falla por **0,1 dB**. Arreglarlo
  significa tocar el algoritmo de CLOUD (calidad) o subir el umbral (relajar la prueba): ninguna de las dos
  cosas entra en una fase de rendimiento sin una medición de audio dedicada. Queda diagnosticado, no tocado.
- **`tests/text_spacing_test.py`**: el par `Gl` del tamaño S (12 px) da −0,258 px de error de hueco contra un
  límite de 0,25. Es una métrica del glifo `G` de la fuente S, no del motor; corregirla mueve píxeles de texto y
  exige regenerar la base de 3 305 cuadros y la puerta de alineación. Diagnóstico, no cambio.
- **Tiempo de CPU medible en este entorno**: sin contador de kernel ni valgrind, y con el reloj del proceso a
  10 ms, ninguna cifra de ns del host es una prueba. La evidencia de CPU de esta fase es el código generado para
  el AC791N (§4, §6) y la identidad de los 316 renders. Para una medición de ciclos hace falta la unidad o un
  `perf`/`callgrind` que aquí no existe.
