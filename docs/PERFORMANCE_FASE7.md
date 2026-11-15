# FASE 7 — segunda campaña de optimización (host)

Qué se midió, cómo se midió y qué salió. **Clase de la medición: host x86, `ui_render` compilado con
`cc -O1`, reloj de proceso.** No es una medición del AC791N ni del emulador: las cifras absolutas no se
trasladan, y solo los cocientes entre dos binarios construidos igual tienen valor.

Método: el mismo árbol, dos commits, misma versión del compilador y mismas banderas. La base es `c0105f9`
(lo que `main` tenía antes de la campaña visual). Cada binario se corrió **cuatro veces** y se tomó el
**mínimo por pantalla**: este host es una VM compartida y una corrida sola varía hasta ±15 %.

```
base   : git worktree add build/base c0105f9; build.generate(); cc -O1 ... tests/ui_render.c
después: cc -O1 ... tests/ui_render.c            (árbol actual)
```

## Resultado

Suma del "full redraw" de las 17 pantallas de la tabla de coste del informe:

| | base | después | Δ |
|---|---|---|---|
| suma (µs, host) | 2 350.1 | 2 385.0 | **+1.5 %** |

Y las pantallas que más se movieron:

| pantalla | base | después | Δ |
|---|---|---|---|
| `home` | 138.6 | 137.9 | −0.5 % |
| `edit_analog` | 133.8 | 135.8 | +1.5 % |
| `env` | 133.1 | 133.4 | +0.2 % |
| `fx` | 131.2 | 129.0 | −1.7 % |
| `roll_chords` | 131.0 | 127.9 | −2.4 % |
| `about_credits` | 228.7 | 222.1 | −2.9 % |
| `mixer` | 153.0 | 156.8 | +2.5 % |
| `confirm_project` | 103.1 | 109.4 | +6.1 % |
| `menu` | 125.9 | 139.3 | **+10.6 %** |

## Qué se optimizó en el código nuevo

1. **La rampa de placa se calculaba en cada tarjeta.** Una pantalla dibuja de cuatro a ocho placas de la
   misma altura y los mismos colores, y cada una volvía a mezclar su rampa: 21 `ux_mix()` por tarjeta, tres
   canales con saturación cada uno, para una lista de colores que no cambia dentro del frame. Ahora hay una
   caché por `(filas, hi, lo)` (`plate_cache`, `gfx.c`) que se recalcula solo cuando alguno cambia.
2. **La misma caché para la rampa del panel** (`ramp_cache`, `ux_ground_ramp`): la página de gráfica mezcla
   toda su altura una vez por frame.
3. **Placas cortas con menos tramos.** Una fila de lista mide 16–18 px: a un tramo cada 2 filas son ocho
   rellenos donde antes había uno, y MENU dibuja siete seguidas. Bajo 24 px de alto la rampa va en cuatro
   bandas (misma ley de 2 px en una tarjeta), que es lo que la vista resuelve a esa altura.
4. **Huella en `.bss` de las cachés**: 64 entradas × 2 B + cabecera por caché = **~276 B** en total, dicho
   explícitamente porque el presupuesto de `.data+.bss` es de 96 KiB.

Antes de las tres primeras, la campaña visual costaba **+4.9 %** en la suma (y `edit_analog` +24 %,
`env` +26 %, `pattern` +20 %). Con ellas, `edit_analog` queda en +1.5 %, `env` +0.2 % y `pattern` +0.5 %.

## Lo que queda por encima y por qué

- **`menu` +10.6 % (13 µs host)** y **`confirm_project` +6.1 % (6 µs host)**: son las filas de lista y los
  diálogos, que ahora llevan rampa propia. Es tinta real, no trabajo repetido: la reduje a cuatro bandas y no
  la quito porque es justo lo que se pidió (un cuadro con degradado en vez de una barra muerta). En el
  dispositivo, con el factor de escala de este proyecto, es una fracción de milisegundo por frame de MENU.
- **`roll_chords` y `about_credits` mejoran** porque abandonaron el tramado 50 % (`gw_dither` pixel a pixel)
  por bandas sólidas.

## Lo que esta campaña NO mide

- **RAM/SRAM/XIP del AC791N, `.bss` final y huella del binario**: sin `JIELI_TOOLCHAIN`/`AC79_SDK` no hay
  build de destino. Lo único declarado es lo que ocupan las cachés en el código fuente (~276 B).
- **CPU de audio, ISR, comunicación entre núcleos**: son `tests/isr_bench.c` y `tests/fx_bench.c` en el
  destino; aquí no se corrieron porque el cambio de esta campaña es de dibujo, no de DSP. Los goldens de
  audio siguen intactos.
- **Emulador WASM**: sin `emcc`.
- **Asignaciones dinámicas**: comprobado por búsqueda en el árbol que **no hay `malloc`/`calloc`/`realloc`
  en el firmware** (solo en `libc.c`, el envoltorio), así que no hay fragmentación que medir.
