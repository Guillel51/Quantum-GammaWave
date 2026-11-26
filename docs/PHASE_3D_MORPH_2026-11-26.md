# Expansión 3D: MORPH de ANALOG con sus cuatro variantes — 2026-11-26

Clase de cada medición: **[HOST]** pruebas y renders en x86, **[AC791N]** build con el toolchain pi32v2 y el SDK.
No hay ninguna medición en una FM-1 física en esta pasada.

## 1. El caso concreto: PWM, SYNC, RING y SAW3

`graph_morph()` tiene dos caminos. El general (el MORPH que recorre una onda, y también PHASE, LOFI, PM y las
ondas sucias TUBE / FOLD) pasa por el estado `morph_st` y dibuja **cuatro capas**: la curva y tres copias
más planas, más altas y más oscuras, cada una con el color de la rampa térmica mezclado hacia el fondo
(`qz_back_ink`). El camino de las cuatro variantes especiales de ANALOG **no**: dibujaba una recta de esquina a
esquina —una rampa que sólo decía «MORPH está aquí»— y hacía `return` antes del bloque de profundidad. Eran las
únicas pantallas de MORPH sin onda y sin profundidad, y además la recta no tenía nada que ver con lo que el DSP
procesa.

Lo que hay ahora:

- **La forma de cada variante, la que hace `analog_ext()`** (`eng_analog.c`), a partir del MORPH de la página
  (sin inventar la modulación que el DSP suma encima — SHAPE, `mo_cur`):
  - **PWM**: el pulso con la ley de `an_pw()` (50 % … 99 % de ancho);
  - **SYNC**: el segundo oscilador reiniciado por cada ciclo del primero, con la relación `16 + MORPH·64/127`
    sobre 16 que pide `an_mext()`;
  - **RING**: el producto de los dos osciladores a esa misma relación;
  - **SAW3**: las tres sierras, el par exterior separado por DTUNE + MORPH (40 unidades por paso).
- **La pila de profundidad compartida**: las cuatro variantes entran por el mismo `morph_st` (`st.sp`) y se
  dibujan con `morph_pass()` en las mismas cuatro capas que el resto, así que no hay una segunda
  implementación del efecto.
- **La barra de recorrido y la etiqueta** que ya tenían las otras ondas: el marcador se pone donde está el
  MORPH (`f = MORPH·255/127`) y la etiqueta sigue siendo `PWM WIDTH` / `SYNC RATIO` / `RING RATIO` /
  `SAW3 SPREAD`.

## 2. Barrido, capturas y pruebas [HOST]

- Cuatro escenas nuevas en `tests/ui_render.c` (`S_MORPH_PWM`, `S_MORPH_SYNC`, `S_MORPH_RING`, `S_MORPH_SAW3`,
  con MORPH 70–90 y, en SAW3, DTUNE 20), con su nombre en `S_NAME`: el barrido pasa de **203 a 207 pantallas**
  × 22 paletas y ahora incluye las cuatro variantes en la base de píxeles.
- `tests/ui_render` (lint, alineación por tinta, coste, `--regions/--idle/--bands/--headers`): pasa; 0
  desalineaciones en 4 069 937 mediciones, los 176 hallazgos de siempre (las líneas de auto-test y el
  solapamiento de ABOUT con la cadena de versión larga).
- `tests/ui_pixels.py`: base regenerada, **3 305 cuadros** y todos idénticos en la verificación.
- `tests/morph_depth.c`: pasa (cuatro capas en todo valor, la onda frontal en el cristal, la de atrás visible,
  3 px de separación en la mediana) — el estado compartido no cambió de contrato.
- `ui_test`, `chorus_rate_test`, `control_map_test`, `phase2_test`: pasan.
- Capturas antes/después de las cuatro variantes en `build/ui_new/ppm/QUANTUM_morph_{pwm,sync,ring,saw3}.ppm`
  (y en las 21 paletas restantes), generadas por el barrido con el código real.

## 3. Build [AC791N]

La XIP creció con las formas nuevas (+500 B de código) y el chequeo de alcance de llamadas desde el código en
RAM quedó en 8 032 B de los 8 192 exigidos. Se subió **RAMTEXT 1 KiB** (`0x01C70800` → `0x01C70C00`) y se
bajó su longitud en la misma medida (`0x6000` → `0x5C00`), así que la banda de guarda en `0x01C76800` no se
movió: alcance **9 056 B**, código en RAM 3 940 B de 23 552.

| [AC791N] | valor |
| --- | --- |
| imagen | 578 936 B de 583 612 (4 676 libres) |
| XIP | 452 480 B de código + 121 676 B de constantes |
| RAM | 75 632 B de 98 304 (22 672 libres) |
| pool | 296 156 B, spare 24 352 B |
| LOWBUF | 28 488 B de 32 768 |
| RAM code | 3 940 B de 23 552; llamadas alcanzan 9 056 B |

`tests/memory_map_test.py` se salta en este entorno (no hay `objdump` de JieLi en el PATH); el mismo control
lo hace `./build.sh` sobre `felucca.syms` y pasa.

## 4. Inventario del resto de la fase, y qué queda pendiente

Se revisó el reparto real de las gráficas antes de tocar nada:

- **Motores con profundidad ya implementada** (cuatro capas con `qz_back_ink` / `heat_col`): MORPH (todas las
  ondas, ahora también las cuatro especiales), FILTER (respuesta con tres copias), LFO (onda y tres ecos), TILT
  (curvas de profundidad), CHORUS (nuevo en la pasada anterior), ENV, FM6 (algoritmo y envolventes), PHASE,
  LOFI, DRUM (colonia), ADDITIVE, WHEEL, HOME/visualizador (por su propio módulo `ui_being.c`, que dibuja
  paneles, trazas vivas y la cinta de carácter).
- **Pantallas planas por diseño** (una línea o una rejilla dicen más que una pila): piano roll, STEP, ARP, MSEQ,
  CHANCE, eventos de automatización, listas de SAVE/PRESETS y las páginas de menú. Ahí no se añadió 3D: una
  rejilla con notas se lee peor con capas detrás.
- **Lo que esta pasada NO hizo**, dicho como pendiente:
  - el rediseño pantalla por pantalla de las 207 escenas más allá de la ruta MORPH;
  - capas 3D nuevas en las páginas de FX que hoy son una sola curva (por ejemplo DLY) y en las de DRUM por
    sonido;
  - microanimaciones de respuesta al tocar cada encoder (hoy existe el resalte de tarjeta y el límite, no una
    reacción de la geometría);
  - toda medición física en la FM-1: no se flasheó ni se tocó la unidad.
