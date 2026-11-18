# Performance baseline — v4.0.0 (2026-11-15)
> **Nota (15-11-2026):** esta página es la línea base de v4.0.0. La referencia vigente para v4.1.0, con sus clases de medición y las cifras que sí se re-midieron en host, es `docs/PERFORMANCE_STATUS_v4.1.0.md`.

Every number here says which class of measurement produced it. Nothing on this page was measured on a
physical FM-1.

## 1. Where the budget stands

| Resource | Value | Limit | Headroom | Class |
|---|---|---|---|---|
| Firmware image | 583 600 B | XIP 583 612 B (`app.ld`) | **12 B** | AC791N build (`./build.sh`) |
| XIP code | 412 976 B | — | — | AC791N build |
| XIP constants | 150 232 B | — | — | AC791N build |
| RAM `.data+.bss` | 88 148 B | 98 304 B | 10 156 B | AC791N build |
| Pool (`dly_buf`, `cv_buf`, `sl_buf`, `cloud_buf`, …) | 305 096 B used | — | 15 392 B | AC791N build |
| RAM code (`.ram_text`+`.c1_text`+`.hot_text`) | 19 596 B | 24 576 B | 4 980 B | AC791N build |
| Package `.fwsc` | 610 959 B | 1 MiB part | — | `build/felucca.fwsc` |
| Loader | 7 803 B | — | — | AC791N build |

The image is at 99.998 % of the XIP region: **12 bytes free**. That single fact decides this phase.

## 2. Audio ISR and engines

`python3 tests/target_budget.py` disassembles the AC791N objects (`build/felucca.dis`) and counts the
instructions inside each function's loops, weighting a divide by 5 and nesting by 8. Class: **instruction
count from the target build**, not a timing.

| Function | Loop cost | Budget | Δ |
|---|---|---|---|
| `fm1_timer5_irq` (the whole audio ISR, aggregate) | 26 923 | 27 338 | −2 % |
| `DRUM` (path) | 1 153 | 1 153 | 0 % |
| `slicer_bus` | 799 | 801 | −0.2 % |
| `uac_tap48` | 465 | 504 | −8 % |
| `dx_run` | 426 | 426 | 0 % |
| `fm6_render` | 292 | 322 | −9 % |
| `formant_render` | 238 | 229 | **+4 %** |
| `noise_render` | 219 | 201 | **+9 % (AT THE CEILING, limit +10 %)** |
| `fm1_alnk0_irq` | 291 | 270 | +8 % |

41 functions measured, none over its limit, 41 with **0 divides in their loops** except the aggregate ISR
(1). `wheel_block` is already 63 % under its budget; `uac_tap48` 8 % under.

## 3. Two optimisations measured and withdrawn

Both were implemented, measured and **not shipped**. They are recorded so nobody repeats them.

### 3.1 `ux_mix` without the rounding division

`ux_mix()` rounded three channels per call with `(d ± 50) / 100`. Replaced by `(n * 20972) >> 21`, exact for
every `n ≤ 43 599` (verified value by value):

* **Host render sweep** (`tests/ui_render.c` built with `-O1`): 79.41 s → 26.5 s, 3.0×, frames byte-identical.
* **On the target**: `pi32v2` **has hardware divide** (`r0 = r0 / r10 (u)` in `build/felucca.dis`), so the
  compiler was never calling a division helper. The change costs **16 B of XIP** (the link goes from 12 B free
  to 4 B over) and reduces no target instruction.
* **Conclusion**: the 3× was an artefact of the `-O1` host harness, not a property of the firmware. Withdrawn;
  the earlier "3×" wording in the v4.0.0 notes is corrected in `CHANGELOG.md`.

### 3.2 `heat_col()` as a 257-entry table

Correct and bit-identical, and it removes a per-point mix, but it costs **244 B of XIP** (measured by the
link failure), so it cannot land while 12 B are free.

## 4. What the flash budget implies

Any change that adds code or constants must first free as much. The honest options, in order of size
available, are all data rather than functions:

| Item | Size | Where |
|---|---|---|
| `WT_DATA` (wavetables) | 12 288 B | rodata |
| `AF_M_DATA` (Medium font bitmap) | 16 921 B | rodata |
| `AF_S_DATA` (Small font bitmap) | 9 523 B | rodata |
| JIANT line art (`ui_shapes.h`) | 10 861 B | rodata |
| `UAC_FIR` | 7 680 B | rodata |
| `KC_DATA` (keycap pills) | 4 792 B | rodata |

Each of them is a generated asset: the way to free flash without losing a pixel is to make the *bootstrap*
reproduce them at run time (the arithmetic is already in the generators) or to store them with a decoder
that is cheaper than the bytes it saves. That is a project of its own; none of it was attempted here.

## 5. Not measured

* **CPU on the unit**: no board, so no cycles per sample, no per-core load, no worst-case dropout count.
  `CPU_SPREAD=1 build/host/regress` gives that distribution on the host, not on the MCU.
* **Physical audio**: no unit, no clicks, no voice-stealing behaviour under load on hardware.
* **Render cost on the target**: the render figures in §3 are host x86 wall clock.

## Visualizador de HOME con pozos semitransparentes (FASE EXTRA)

`graph_home_viz` deja de tapar el campo profundo con dos placas opacas: `cv_rect_a()` mezcla un velo sobre los
14 656 píxeles de los dos pozos. Medido con `tests/ui_render.c` (`cc -O1`, host x86; sólo las proporciones
sirven en el aparato):

| Versión del velo | Visualizador HIGH | LOW | XIP |
|---|---|---|---|
| sin velo (placas opacas, línea base de 1.4.4.x) | 213,3 µs | 98,2 µs | — |
| `ux_mix()` por píxel (**la que quedó**) | 363,4 µs | 255,1 µs | +80 B |
| tablas de 128 entradas construidas con `ux_mix()` | 247,5 µs | 133,1 µs | +224 B |

Las tablas dan bit a bit el mismo resultado y son ~115 µs más rápidas, pero con la línea 1.4.7 del otro chat
(la espera de la DMA del panel) el XIP no las admite: la imagen con tablas se pasaba por 152 B, y con la fila
de menú VIZ VEIL también. Quedó la de `ux_mix()`: +70 % en el cuadro del visualizador y 48 B libres de imagen
(583 564 de 583 612).
