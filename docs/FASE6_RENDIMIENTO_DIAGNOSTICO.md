# Fase 6 — Home, visualizer y animaciones: la medición primero

Clase de todas las cifras de este documento: **`[HOST]`** x86-64, las fuentes reales del firmware compiladas
con `cc -O2`, en un bench que no toca el audio. **No hay unidad FM-1 en este entorno**: aquí no se puede
reproducir un tirón del panel, y nada de lo que sigue se presenta como si lo fuera.

## 1. La herramienta: `tests/frame_bench` mide ahora las cinco pantallas de HOME

`tests/frame_bench.c` medía el lazo con la pantalla quieta. Le añadí una tabla que, para cada una de las cinco
pantallas de HOME, mide **dos** fotogramas distintos:

- `force`: `ui.force = 1` en cada llamada, es decir la página entera pintada (bandas, cuerpo y gráfico). Es el
  coste de una transición o de un cambio de página.
- `settled`: la misma pantalla sin forzar nada, o sea lo que cuesta *estar viva*: solo lo que de verdad cambia.

El cociente entre las dos dice cuánto está ahorrando el sistema de firmas (`ui.head_sig`, `ui.foot_sig`,
`ui.graph_sig`, `ui.roll[].sig`) y, sobre todo, **qué pantalla no lo está aprovechando**.

## 2. Los números `[HOST]` (ns de CPU por fotograma, 400 iteraciones)

| Pantalla de HOME | `force` | `settled` | cociente |
|---|---:|---:|---:|
| Main | 247 740 | 70 120 | ×3,5 |
| **Visualizer** | **370 500** | **245 840** | **×1,5** |
| Overview | 148 800 | 14 960 | ×9,9 |
| Mixer | 137 920 | 12 980 | ×10,6 |
| Performance | 156 920 | 15 400 | ×10,2 |

*(host x86-64: ~1/2000 de lo que cuesta un bloque de audio de 725 µs, así que estos números no se comparan con
el presupuesto del AC791N; lo que sí se compara es una pantalla con otra, que es lo que aquí se busca.)*

Y el lazo completo, con la pantalla ya asentada:

| Pieza | `[HOST]` ns/llamada |
|---|---:|
| `ui_input()` | 512 |
| `ui_leds()` | 182 |
| `ui_draw()` | 1 020 |
| lazo entero | 1 700 |
| `autosave_sig()` (cada 250 ms) | 3 547 |
| `autosave_poll()` por fotograma | 300 |

## 3. El diagnóstico

1. **El Visualizer es el punto negro, y no por animarse: por no cachear nada.** Cuesta **245 840 ns quieto**,
   dieciséis veces lo que Overview (14 960), Mixer (12 980) o Performance (15 400). El cociente ×1,5 lo dice
   todo: forzar el fotograma solo cuesta 1,5 veces más que no forzarlo, porque **se repinta entero de todos
   modos**. En las otras tres pantallas el cociente es ×10: ahí el sistema de firmas está haciendo su trabajo.
2. **Main es el segundo**: 70 120 ns asentado (×4,6 respecto a las otras secundarias). También anima su bahía
   cada fotograma.
3. **Lo que ya está bien y no hay que tocar**: no hay FFT en el camino de audio (el visualizer usa
   `scope_buf`, una instantánea de la forma de onda, y 16 bandas de 8 muestras); el análisis es una sola pasada
   por fotograma y sus dos pasadas de dibujo comparten la misma medición (`bars[]`, `peak_decay[]`); el
   `scope_snap` ya se reutiliza; el autosave tiene su tick de 250 ms y su firma, y el `poll` por fotograma
   cuesta 300 ns.

## 4. Por qué no hay un parche en esta versión

El arreglo correcto del Visualizer no es bajar la calidad ni recortar la animación: es **darle la misma
arquitectura que ya usan las otras cuatro pantallas** —firmas para las capas estáticas (placa, título, rejilla,
suelo del espectro) y repintado solo de las dinámicas (traza, barras, peacks, campo profundo)— y que el campo
profundo (`gw_viz_deep`) tenga su propio compás, porque es lo único que cambia sin que cambie el audio.

Eso toca `graph_home_viz` entero (dos pasadas de 240 × 88, `cv_oy` por pasada, `gw_viz_deep`, `gw_embers`,
`peak_decay`), y una reescritura así **no se valida con el bench**: se valida comparando el fotograma dibujado
contra el baseline de píxeles en las 22 paletas, comprobando que no aparece *flicker* en la costura de las dos
pasadas (el report de v1.11 fue exactamente eso) y midiendo el bench antes/después. Con el presupuesto de esta
pasada no llegaba a hacerlo con esa garantía, y publicar un visualizer a medio cachear sería peor que el
actual: parpadeos en una pantalla que hoy no los tiene.

## 5. Lo que queda medido y listo para la siguiente pasada

- La tabla de las cinco pantallas está en el repo y se puede volver a correr: `build/frame_bench 400`.
- El objetivo es concreto: bajar `settled` del Visualizer de 245 840 a la banda de las otras secundarias
  (13 000–16 000), es decir **×16**, sin tocar un solo elemento visual.
- El mismo molde sirve para Main (70 120 → ~20 000).
