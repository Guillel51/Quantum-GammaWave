# FASE 2/9 — Identidad visual: ENV, FX, BUSES y SCL (v2.1.0)

Versión de este árbol: **v2.1.0** (`FELUCCA_VERSION`), identidad `FM-1_965`.
Clases: `[HOST]` x86-64 nativo, `[TARGET]` build AC791N (pi32v2 clang + AC79 SDK), `[STATIC]` inspección,
`[WASM]` Emscripten. **No hubo unidad FM-1 física**: nada de esto es una medición del instrumento, y ninguna
compuerta de instalación cambia de estado (`INSTALL BLOCKED`, ver `docs/VALIDATION_MATRIX.md`).

## 1. Qué se rediseñó y por qué

La versión anterior de estas páginas tenía una composición común: cuatro tarjetas de parámetros arriba y un
gráfico pequeño debajo, con dibujos que se diferenciaban por detalles. El pedido fue una transformación
visible, no un retoque de color. El rediseño cambia **la composición y el dibujo** de cada página, y deja
intactos los parámetros, la navegación, los cuatro knobs y el DSP.

| Página | Antes | Ahora |
|---|---|---|
| ENV | curva de 2 px con guías y cuatro etiquetas | la envolvente como **forma rellena** hasta el eje, con las cuatro esquinas marcadas (A, D, S, R) sobre la curva y el playhead de la voz más fuerte |
| DLY | seis barras sobre un eje (una fila de marcas) | **túnel de ecos**: rectángulos concéntricos, uno por repetición audible, más pequeños, más fríos y desplazados por SPREAD / PITCH / PING-PONG; el eco que suena va en acento |
| DLY 2 | seis barras de granos sobre una línea central | el mismo túnel, **uno por canal**, con PITCH inclinando cada lado y WIDTH separándolos |
| REVERB | cola de columnas de 1 px | la cola como **escalera de barras** (3 px + 1 de aire) con la cabeza de cada barra más clara: la pendiente, el largo con SIZE y el brillo con DAMP se leen de un vistazo |
| REVERB 2 | columnas de 1 px con wobble por columna | la misma escalera **partida en dos**, izquierda arriba y derecha abajo, con WIDE y FILT |
| CHORUS | cuatro sinusoides finas | **dos carriles grandes**, L y R, con la separación y el desfase de STEREO y la amplitud de DEPTH |
| BUSES | 2 × 3 bloques de 116 × 34 con dibujos de 16 px | **consola 3 × 2** de 76 × 52: cabecera con icono y lámpara, dibujo propio de cada efecto y el valor que importa en el pie; medidores de envío/retorno a la derecha |
| SCL (KEY / CHORD / PROG) | teclado con una franja de escala y otra de tónica | **cada grado tiene su color**: la tónica en acento, los demás recorriendo la paleta entre tema y acento, y el mismo color tiñe la cara de la tecla natural y la franja del accidental |

Los dibujos de BUSES no son genéricos: DELAY dibuja el túnel, REVERB la escalera, CHORUS los dos carriles,
SLICER las ocho compuertas con el paso actual en acento, PUNCH la transitoria contra la caída que DUCK le
hace y MASTER su medidor con la línea de pico y la lámpara de clip.

## 2. De dónde sale cada píxel (nada inventado)

- **DLY / DLY 2**: `song.g[G_DTIME]`, `G_DFDBK`, `G_DSPRY`, `G_DPIT`, `G_DPING`, `G_DTYPE` y `G_DMIX`. El número
  de marcos del túnel es `dly_echoes()`: cuenta las repeticiones que el feedback deja por encima del 1 % del
  golpe seco (tope 12), es decir el mismo número de ecos que el DSP produce, no una constante de dibujo.
- **REVERB / REVERB 2**: la envolvente de la cola es la misma que ya usaba la página, y su nivel sale de
  `fx_vu[5]` (el pico que el bus devuelve, publicado por `fx.c`); `G_RSIZE`, `G_RDAMP`, `G_RPRE`, `G_RTYPE`,
  `G_RFILT` y `G_RWIDE` siguen decidiendo largo, caída, pre-delay, modelo, tono y ancho.
- **CHORUS**: `G_CRATE`, `G_CDEPTH`, `G_CSTER` y `G_CTYPE`; los dos carriles se mueven sólo si el motor tiene
  señal (`bg_energy()`), como el resto de los gráficos animados.
- **BUSES**: cada bloque lee los mismos `fx_vu` de su bus y los mismos `song.g` que la página de ese efecto;
  la lámpara se enciende con el retorno medido, y un bus apagado no dibuja actividad.
- **ENV**: los cuatro valores de la página (`P_ATK/DEC/SUS/REL`) y la voz más fuerte del motor, con la misma
  `ENV_K()` que usa el DSP para la caída exponencial.
- **SCL**: `mask` (la escala), `root`, el máscara de notas sonando y la selección; el color de grado es una
  mezcla de dos tokens de la paleta (`ux_mix`), así que GREY y MONO siguen siendo neutros.

## 3. Medidas

### 3.1 Memoria `[TARGET]` (build AC791N, `tools/mem_budget.py`, `tests/resources_test.py`)

| Ventana | v2.0.3 | v2.1.0 | libre |
|---|---:|---:|---:|
| Imagen (app slot) | 581 556 / 583 612 | **581 476 / 583 612** | **2 136** (suelo 2 048) |
| RAM `.data+.bss` | 92 652 / 98 304 | **92 636 / 98 304** | **5 668** (suelo 4 096) |
| LOWBUF | 31 648 / 32 768 | 31 648 / 32 768 | 1 120 |
| POOL | 312 124 / 327 680 | 312 124 / 327 680 | 15 556 |
| RAMTEXT | 3 792 / 24 576 | 3 792 / 24 576 | 20 784 |
| NOINIT | 15 572 / 15 696 | 15 572 / 15 696 | 124 |

La imagen **no creció**: el túnel, la escalera, los carriles y la consola entraron borrando lo que
reemplazaban (las seis barras del tren de ecos con su estado `dly_h`/`dly_b`/`dly_tick`, el brillo móvil de la
consola, los anillos de aire de las dos páginas de reverb y el wobble por columna de REVERB 2). El resultado
neto es **80 B menos de imagen y 16 B menos de RAM** que en v2.0.3, con más dibujo en pantalla.

### 3.2 CPU `[HOST]` x86-64, `tools/dsp_bench.py --compare`

| Medida | v2.0.3 | v2.1.0 | Δ |
|---|---:|---:|---:|
| Lazo principal, ns/frame | 2 800.0 | 3 145.0 | +12.3 % |
| `draw`, ns/frame | 1 646.7 | 1 748.3 | +6.2 % |
| `input`, ns/frame | 505.0 | 536.7 | +6.3 % |
| Mezcla 3 partes + batería, ns/muestra | 278.0 | 278.0 | 0.0 % |
| Mezcla 6 partes, ns/muestra | 323.0 | 327.6 | +1.4 % |

El camino de audio no cambia (ningún archivo de DSP fue tocado; `tests/regress` da los **311** renders
idénticos). El dibujo cuesta ~100 ns más por frame en host porque hay más píxeles que pintar; en el objetivo
sigue por debajo del presupuesto de frame del proyecto y las cifras de la ISR son ruido de host (van de
+63 % a −71 % entre dos corridas del mismo árbol).

### 3.3 Píxeles y lint `[HOST]`, `tests/ui_render` + `tests/ui_pixels.py`

- **166 de 3 129 frames cambiaron a propósito** (ENV, DLY, DLY 2, REVERB, REVERB 2, CHORUS, BUSES y las
  pantallas de SCL, en las 22 paletas); el resto sigue idéntico. `tests/ui_pixels.txt` se regeneró con el
  motivo en el commit.
- **Lint de alineación: 0 hallazgos** en 3 953 168 mediciones, 0 tintas sobre su propio color, 0 píxeles
  fuera de gris en GREY / MONO.

## 4. Lo que no se probó

- **No hubo unidad FM-1**: nada de esto demuestra comportamiento del instrumento. Las pantallas se
  verificaron en el render de host y en el emulador, que es donde el proyecto mide la UI.
- No se corrió `valgrind` (no está en el entorno): el perfilado de la UI queda como pendiente declarada.
