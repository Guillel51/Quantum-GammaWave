# FASE FX LATCH — renovación de la capa de performance

Fecha: 2026-11-26. Alcance: `firmware/src/perform.c` (estado y DSP de FX LATCH) y `firmware/src/ui_layer.c`
(`layer_fx`, `layer_cards`, `draw_layer`). Capturas: `docs/shots/before/` y `docs/shots/after/`.

## 1. Qué era FX LATCH

FX LATCH no es una página `GR_*`: es la capa `LAYER_FX` que aparece con el botón FX mantenido. Su estado vive en
`perform.c` (`perf_held`, `perf_latched`, `perf_latch_on`, `perf_kill`, `perf_k[4]`, los tres cabezales `pb[]` y el
banco `pfx_bank`/`pfx_tgt`) y su dibujo en `ui_layer.c`. La asignación de cada tecla sale de `perf_key()` (seq.c),
la misma función que usa la ISR, así que una etiqueta no puede separarse del DSP.

Antes de esta fase la capa tenía dos problemas estructurales:

1. **Se dibujaba en dos bandas con una superficie propia de 234 px** (`cv_rrect` sobre `T_SURF`), 58 filas para dos
   tarjetas de sólo texto y 122 para el teclado, con `fx_text_y0/y1` decidiendo qué banda escribía cada letra.
2. **No tenía la fila de tarjetas del resto del instrumento.** La v2.0 había recortado cinco tarjetas a dos
   (LATCH y DEPTH) y las dibujaba a mano dentro del mapa: sin iconos, sin barras y con el mismo texto que el pie.

## 2. Errores corregidos

- **`perf_buf_state()` devolvía un valor y el mapa otro.** La primera versión del readout de los cabezales
  devolvía `pb[i].mode` (BM_REV = 2, BM_FRZ = 3 …) en el campo de estado que la interfaz comparaba con 2: un REV
  capturando se habría pintado como "leyendo". El estado es ahora `pb[i].state ? 2 : 1`, el mismo valor que
  devolvía `perf_buf_state()`.
- **`perf_buf_select()` recorría dos veces lo mismo.** Dos bucles idénticos (uno para `PF_R8..PF_R32` y otro para
  `PF_REV..PF_END`) conciliaban los efectos de buffer con los tres cabezales: ahora un solo recorrido con
  `PF_BIT(e) & PF_BUF`, que es el conjunto exacto porque los dos rangos son disjuntos.
- **`perf_count()` comparaba seis constantes por bit** para excluir las seis mutes. Ahora una máscara
  (`PF_EFF_MASK`) y un recorrido de bits: mismo resultado, menos código en la ruta de la tecla.
- **La superficie del mapa se construía dos veces** (una por banda) y el texto podía dibujarse en la banda
  equivocada. Con una sola banda desaparecen `fx_text_y0/fx_text_y1` y el doble `cv_begin`/`cv_blit`.
- **Las teclas se dibujaban con dos cuerpos duplicados** (uno blanco y uno negro) y cada una buscaba su índice
  recorriendo las veintisiete. Ahora hay un solo cuerpo (`fx_key`) y el índice es el bucle.

## 3. El rediseño

- **Superficie de 240 × 122 px hasta los bordes**, con `ux_ground_rows` (la misma primitiva de HOME y FX > BUSES,
  que camina la rampa fila a fila: escalones de 1–4 px, 16 bits reales), borde superior iluminado, pie en sombra y
  una columna de 1 px en x = 0 y x = 239. El pie de la capa continúa la misma rampa, así que el suelo llega a la
  última fila de la pantalla.
- **Fila posterior**: la silueta del teclado 6 px por encima y 4 px a la derecha, al 12 % de tinta, antes de las
  teclas. Es lo que dice que las teclas están sobre algo.
- **Teclas con volumen**: borde superior iluminado, pie en sombra, tecla retenida con tapa de acento y marco, tecla
  bloqueada (FX LATCH) con tapa, tecla no disponible con cuerpo igual y etiqueta atenuada. Teclas de 108 px (antes
  118 en la banda única, 146 con la franja intermedia), negras de 50 px y etiqueta vertical centrada.
- **Cuatro tarjetas estándar** (`layer_cards(LAYER_FX)`), con el chaflán, el icono y la barra del resto del
  instrumento y **valores reales**: LATCH (`perf_latch_on`, con `OCT+` / `HOLD` como unidad), DEPTH
  (`perf_k[3]`, 100 − corte), FILTER (`pf.lt`/`pf.ht` y el corte `pf.lc`/`pf.hc` en la barra) y BANK
  (`pfx_bank`, con el destino `pfx_tgt`). Sustituyen a las dos tarjetas dibujadas a mano y son las gráficas que
  reaccionan al DSP: la barra de FILTER se mueve con el filtro real mientras LPF/HPF están retenidos o KNOB 1 gira.

## 4. Presupuesto medido

| | antes (HEAD) | ahora | Δ |
|---|---|---|---|
| imagen | 582 136 B (1 476 libres) | **579 332 B (4 280 libres)** | −2 804 |
| código XIP | 455 680 B | **452 912 B** | −2 768 |
| constantes | 121 676 B | **121 640 B** | −36 |
| alcance de llamadas desde RAM | 8 416 B | **11 184 B** (mínimo 8 192) | +2 760 |
| RAM `.data+.bss` | 75 632 B | 75 664 B | +32 |
| POOL / LOWBUF | 296 156 / 28 488 B | 296 156 / 28 488 B | = |

El rediseño **no cuesta flash: lo libera**. La intervención que lo paga es dejar de dibujar a mano lo que el
instrumento ya sabe dibujar (la fila de tarjetas) y dejar de construir dos veces la superficie del mapa.

## 5. Pruebas

- `./build.sh`: los cinco controles pasan (RAM, pool, alcance ≥ 8 KiB, registros, XIP).
- `ui_render` (207 pantallas × 22 paletas, 3 305 cuadros): **0 hallazgos de lint, 0 tintas sobre su propio color,
  4 073 426 medidas de alineación con 0 desviaciones ≥ 1 px, 0 píxeles GREY/MONO fuera de gris**.
- `ui_pixels.py --update`: **80 cuadros cambiados, todos de la capa FX LATCH** (`layer_lock_fx` y las escenas
  `perform_*` en sus paletas); `*_fx.ppm` y `*_fx_buses.ppm` quedan idénticos, así que ninguna otra pantalla se movió.
- `perform_test`: pasa, incluidos LATCH, cambio de banco y liberación desde el otro banco.
- `ui_test`: pasa entero. `regress`: 316 renders dorados idénticos, 0 notas, 0 voces y 0 fallos de salud;
  1 fallo de presupuesto de CPU es el host (201 % más lento), no el código.

## 6. Límites, dichos como son

- **No hay unidad FM-1 en este entorno**: nada de esto está verificado en hardware, ni el audio ni el panel.
- La CPU del host no es medible aquí (sin contador de kernel, reloj de 10 ms); la evidencia de CPU es el código
  generado para AC791N, no una medida.
- **La barra de progreso por tecla de los cabezales de captura se cotizó y se dejó fuera.** Con el mapa en una
  sola banda el teclado ya no tiene la fila baja donde iba, y añadirla costaba ~250 B de flash para un dato que el
  estado de la tecla (acento y tapa) ya insinúa. Las gráficas del DSP son las barras de las tarjetas, que son
  valores reales del motor. Queda anotado aquí, no para otra fase: es una decisión de presupuesto, no un pendiente.
- WASM no se puede medir: no hay `emcc` y el repositorio no guarda `.wasm`.
