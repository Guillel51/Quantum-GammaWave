# Fase 2/3 — Renovación visual de Quantum GammaWave (v2.3.16)

Sin rediseño: la identidad, los componentes, las animaciones, los degradados y la profundidad se conservan. Lo
que cambia es **cuánto pesan las dos zonas** y **lo que estaba mal dibujado**. Sin unidad FM-1: todo lo de este
documento es `[HOST]` (el banco de renders, `tests/ui_render.c`) salvo donde diga `[TARGET]` (el build de AC791N).

## 1. Los dos ajustes, en los tokens

Se hicieron una vez, en `palette_set()` (`firmware/src/gfx.c`), porque de esa función salen todos los colores de
la interfaz: `T_BG`, `T_SURF`, `T_TEXT`, `T_THEME`, `T_ACCENT` y cada tinta derivada de ellos (`mid`, `dim`, `line`,
`sel`, `tint`, `raise`, `key`, `lane`, `grid`). Ninguna pantalla tiene colores propios que se hayan quedado atrás:
la paleta se aplica y las 203 pantallas, en las 22 paletas, se redibujan con ella.

| Token | Antes | Ahora | Por qué |
|---|---|---|---|
| fondo (paleta oscura) | `bg` | `bg −16 %` de luma | negros y azules profundos definidos |
| superficie | `surf −10 %` | `surf −5 %` | el escalón fondo → placa **crece** en vez de cerrarse |
| tema | `theme` | `theme` 18 % más lejos de su luma | cian, azul, violeta y turquesa con más presencia |
| acento | `accent` | `accent` 18 % más lejos de su luma | los elementos destacados y sus degradados |

Tres números, `UI_DEEP_BG_PCT`, `UI_DEEP_SURF_PCT`, `UI_VIVID_PCT`, y dos reglas:

* **GREY** conserva cada tinta sobre la línea de grises (`R = B`, `G = 2 R`): `ux_sat` no se le aplica y el
  oscurecido usa `ux_shade`, que ya respeta esa línea. `tests/ui_test.c` recorre la pantalla entera de GREY.
* **MONO** se queda en negro y blanco puros; **PAPER** y las claras no se oscurecen (su papel es el color).

El matiz no se toca: `ux_sat` mueve cada canal alrededor de la luma de la propia tinta, así que el tono de la
paleta queda donde estaba. No entra ningún color nuevo.

## 2. Lo que estaba mal dibujado (y se arregló)

`ui_render` venía saliendo 1 con **4 267** hallazgos desde antes de la fase anterior, y la matriz lo tenía como
`REPORTED`. No eran cosméticos:

| Familia | Cuántos | Qué era | Cómo quedó |
|---|---|---|---|
| `FM6 part through a box` | 483 | la ruta de un operador arrancaba en el borde de la caja, o sea dentro de la sombra de 2 px que la caja proyecta: el cable nacía **debajo de la sombra** | arranca 2 px más abajo (`y + FM_BH + 2`), por debajo de la sombra |
| `FM6 part through a box` | (ídem) | el codo se dibujaba a `y − 3` y el punto de 3 × 3 a `y − 2`/`y − 3` rozaban la sombra de la fila de arriba | el codo va a `y − 3` (la única fila libre) y el punto de fusión a `y − 2`, donde su fila superior no toca la sombra |
| `FM6 part touches a box` | 21 | la realimentación salía por el lado derecho aunque hubiera una caja al lado | el lado se **elige**: se prueba el rectángulo que necesita la realimentación contra cada caja (sombra incluida) y se usa el libre |
| `spills out of its cell` | 112 | las etiquetas de DX IMAGE (BD SD CP CH OH TM RS CB) se dibujaban 1 px **por encima** de su placa | entran en su celda (`CAP_IN(S, 13)`) |

El mensaje del lint, además, imprimía dos veces el mismo píxel: ahora imprime el rectángulo de la caja, que es lo
que hace falta para arreglar un roce.

## 3. Lo que se midió

| Comprobación | Resultado | Clase |
|---|---|---|
| `ui_render` (lint, 203 pantallas × 22 paletas, 260 charts de FM6) | **0 hallazgos** (antes 4 267); sólo las 4 líneas del auto-test | host |
| Alineación por tinta | 0 desvíos de 1 px en 3 967 423 medidas, 76 lugares | host |
| GREY / MONO | 0 píxeles fuera de gris / neutro | host |
| Tintas sobre su propio color | 0 | host |
| Baseline de píxeles | 3 241 cuadros regenerados; la puerta los compara byte a byte | host |
| Coste de dibujo | sin cambios (mismas rutinas, mismos píxeles de texto) | host |
| Imagen | 580 736 B de 583 612 (2 876 libres, suelo 2 048) | target |
| RAM | `.data+.bss` 93 556 B de 98 304 (4 748 libres) | target |

## 4. Capturas comparativas

`build/check2/*_cmp.png`: antes (v2.3.15) a la izquierda, después (v2.3.16) a la derecha, 2× para que se lean.
Revisadas: HOME (y sus cinco vistas), Visualizer, Overview, Mixer, Performance, FX, SCL TUNE, LFO, ENV, DX IMAGE,
Presets, Menu, Splash, y las paletas GREY, MONO, PAPER y NIGHT para comprobar que no se movieron.

## 5. Lo que queda

* **Sin unidad FM-1**: nada de esto es una medición de hardware.
* El lint no ve lo que no está registrado: un dibujo que no use `cv_*`/`GFX_HOOK_*` no entra en su alcance.
