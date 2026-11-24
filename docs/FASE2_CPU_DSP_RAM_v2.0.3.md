# FASE 2/9 — CPU, DSP y RAM (v2.0.3)

Versión de este árbol: **v2.0.3** (`FELUCCA_VERSION`), identidad `FM-1_965`.
Clases: `[HOST]` x86-64 nativo, `[TARGET]` build AC791N (pi32v2 clang + AC79 SDK), `[STATIC]` inspección,
`[WASM]` Emscripten. **No hubo unidad FM-1 física**: nada de esto es una medición del instrumento, y ninguna
compuerta de instalación cambia de estado.

## 1. Punto de partida (medido, no supuesto)

`[TARGET]` `tests/target_budget.py` y `tools/mem_budget.py` sobre el build de v2.0.2:

| Ventana | usado / tamaño | libre |
|---|---:|---:|
| Imagen (app slot) | 581 556 / 583 612 | 2 056 |
| RAM `.data+.bss` | 92 652 / 98 304 | 5 652 |
| LOWBUF | 31 648 / 32 768 | 1 120 |
| POOL | 312 124 / 327 680 | 15 556 |
| RAMTEXT | 3 792 / 24 576 | 20 784 |
| NOINIT | 15 572 / 15 696 | 124 |

`[TARGET]` coste estático de los lazos: `BLOCK_PATH` **33 433** (presupuesto 45 704, −27 %),
`DRUM_PATH` **1 155** (techo 1 153, dentro del 10 % aceptado), `events_block` 1 796 instrucciones con
**11 divides**, `fm1_timer5_irq` 1 285 instrucciones / 41 llamadas.

`[HOST]` `tests/regress` (cc -O2, mínimo de varias ventanas): `6parts_full` **320 ns/muestra**,
`3parts_full_drums` **353 ns/muestra**, `idle_drums` **105 ns/muestra**. Un bloque de 32 muestras cuesta
~10 µs del presupuesto de 725 µs.

## 2. Lo que se entregó

### 2.1 `tools/dsp_bench.py` — el arnés reproducible que faltaba

Un comando mide todo lo medible sin unidad y lo compara contra una línea base:

```bash
python3 tools/dsp_bench.py --record     # tests/bench_baseline.json
python3 tools/dsp_bench.py --compare    # +25 % host, +3 % target, golden distinto = fallo
```

Cubre: CPU/muestra de las cuatro escenas de mezcla y el veredicto de los 311 golden renders `[HOST]`;
ns/tick de la TIMER5 `[HOST]`; ns/frame, ns/firma y ns/sondeo del lazo principal `[HOST]`;
instrucciones y divides por función de los lazos de render, las cinco ventanas de memoria y el tamaño de
la imagen `[TARGET]`; y el margen de pila `[HOST]`. Lo que el entorno no puede medir se informa como
ausente, nunca se rellena con una estimación.

### 2.2 `tests/stack_watermark.c` — el margen de pila, por primera vez

El clang del AC791N rechaza `-fstack-usage` (`unknown argument`), así que el repositorio no tenía ningún
número de pila. Ahora cada escena corre en un hilo cuyo stack **es** un buffer asignado y pintado con
`0xA5`, y se lee la frontera exacta que el C escribió (no hay que adivinar direcciones debajo del SP del
hilo principal: esas páginas no están mapeadas).

| Escena | Marca | Clase |
|---|---:|---|
| Mezcla, 6 pistas, los cuatro envíos abiertos | **1 783 B** | `[HOST]` |
| Captura + empaquetado + decodificación de proyecto | < 256 B | `[HOST]` |
| 40 frames de UI (input, LEDs, draw) | **4 815 B** | `[HOST]` |

Contra las ventanas de `app.ld` — **13.75 KiB** de pila de usuario y **7.75 KiB** de excepción, con guardas
de 256 B — y un presupuesto de 24 KiB en este test. La cadena profunda es la de la UI, no la de audio, y
ninguna de las tres escenas recursa por la ruta de audio o de proyecto. El test corre en la suite: si un
cambio hunde más una cadena, falla.

### 2.3 Un defecto de la fase anterior, confirmado y cerrado

La firma del autoguardado ya no se calcula dos veces por sondeo (v2.0.2) y el arnés lo mide: `frame_bench`
da `sig` ≈ 4–5 µs y `poll` ≈ 300–420 ns por frame. Nada que corregir.

## 3. Auditado y rechazado, con la medición delante

| Candidato | Medición | Veredicto |
|---|---|---|
| Compartir `proj_scratch` (9 452 B) con `sec_stage` (9 398 B) | un guardado durante la reproducción tendría que parar la cadena (1/16 perdido = cambio audible) | **no demostrable**, igual que en la serie anterior |
| Mover `gen_keep` (2 176 B) a LOWBUF | LOWBUF tiene 1 120 B libres; y no es disjunto de una sesión de backup del editor (varios callbacks) | **no cabe / no demostrable** |
| Mover `upf` (3 728 B) fuera de `.bss` | es el objeto de una sesión BACKUP_GET que vive entre callbacks; POOL sólo tiene 192 B sobre su suelo de 8 KiB | **no cabe** |
| Recortar la contabilidad de `fm1_in_stat` en la ISR de 10 kHz | el tick completo mide **25.7–35.2 ns** (`isr_bench`, host) = 0.03 % de sus 100 µs; la contabilidad es una fracción de eso | **no vale el riesgo** en la ISR más caliente |
| Recíprocos de DRUM-X en el sitio de llamada | ya medido en la serie anterior: el lazo crece al duplicar el cuerpo; `DRUM_PATH` está a 2 instrucciones de su techo | **rechazado** |
| Bypass por señal de los buses de FX | `fx.c` ya corta por bus (`zero CPU cost when bypassed`, `truly bypassed: no stale tail`); no hay señal silenciosa que ahorrar | **ya está** |
| Tablas: `WT_GEN` (6 192 B), `SINE`/`fm6_exp2_tab` | ya generadas a media/una tabla de deltas desde fases anteriores; en XIP costarían 7 199 B más que el slot | **ya está** |
| Perfilar la UI con callgrind | no hay valgrind en este entorno | **bloqueado**, se informa |

## 4. Pruebas

- `tools/dsp_bench.py --record` + `--compare` (línea base nueva en `tests/bench_baseline.json`).
- `tests/stack_watermark`: 1 783 / < 256 / 4 815 B, dentro del presupuesto.
- `tests/regress`: 311 golden renders, **0 cambiados**, presupuesto de CPU y de voces en verde.
- `tests/ui_pixels`: 3 129 frames, 0 cambios tras el salto de versión; `ui_render`: 0 lint, 0 desalineaciones.
- `./build.sh` y `./build.sh --release 2.0.3` `[TARGET]`; suite completa del host y emulador \(= native\).
