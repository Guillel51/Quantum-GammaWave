# Solar Heat UI — lo que se hizo en esta pasada (6.4)

Documento de entrega de esta pasada. Declara lo implementado, lo verificado y lo que queda abierto, sin presentar
como terminado lo que no se tocó.

## 1. FX > BUSES: la distribución estaba rota y se rehízo

El barrido de lint de la UI lo medía: **318 hallazgos** en la pantalla, todos del mismo tipo, textos fuera de su
celda — los títulos de cada bloque (`DELAY`, `REVERB`, `CHORUS`, `SLICER`, `PUNCH`, `MASTER`) se dibujaban por encima
del rectángulo interior del bloque, y en ocho celdas de 28 px los nombres completos de la cadena se pisaban entre sí
(`TRACIBUSES`, `PUNCHMASTER`).

Cambios en `firmware/src/ui_fxb.c`:

- **La cadena en una fila.** Ocho celdas de 28 px con tres letras (TRK, BUS, DLY, REV, CHO, SLI, PCH, MST), la
  seleccionada en acento y el nombre completo de la seleccionada abajo a la derecha. Antes eran dos filas de cuatro
  con nombres completos, que se leían como dos menús en lugar de un recorrido.
- **Cada bloque es un panel con su propio suelo.** Un `cv_rrect` exterior y otro interior que contiene título y
  dibujo; el título ya no puede salir de la celda y el lint lo confirma. Se retiró la tira de cabecera que se salía
  del rectángulo y el número que competía con el título en el borde derecho — los cuatro controles de arriba ya
  muestran los valores del eslabón seleccionado.
- **Cada efecto conserva su dibujo propio**: repeticiones del delay separadas por TIME y atenuadas por FEEDBK, cola
  de reverb cuya longitud es SIZE y cuya altura es el pico wet, tres ondas del chorus con fase viva y barrido por
  DEPTH, ocho compuertas del slicer con el paso actual encendido, transiente de PUNCH contra el pico del master más la
  muesca de DUCK, y la etapa de ganancia con el pico de salida y la lámpara de clip. Nada de eso es nuevo DSP: todo
  sale de los ids que el motor ya toca y de `fx_vu`, así que un bus en silencio no anima nada.
- **El master queda siempre enmarcado** en la cadena (es la salida de todos los buses), con el bloque de los knobs en
  acento.

Resultado del barrido: **190 pantallas × 22 paletas, 0 hallazgos de lint, 0 desalineaciones** (antes: 318 sólo en
esta pantalla).

## 2. HOME: Vía Láctea (pasada anterior, 6.3)

El cielo del *bay* tenía dos «nebulosas» de doce píxeles; ahora lo cruza una banda galáctica con núcleo, bulbo, halo,
polvo y un único seno lento para el brillo. Detalle en `docs/QG_UI_VISUAL_REDESIGN.md` §1.

## 3. Corrección de una regresión introducida por mí

El commit `19b5414` puso un gráfico por motor en EDIT 1. El barrido de la UI lo rechazó: la comprobación que exige
32 gráficos FM6 en el recorrido LARGE bajó a **0**, porque EDIT 1 dejó de dibujar el gráfico de operadores que ese
recorrido mide. Se revirtió el gancho (EDIT 1 vuelve a `GR_NONE`) y se eliminó `ui_enginegraph.c`. `SEQ > TOOLS`
del mismo commit se conserva y sigue verde. Lo que el gráfico por motor quería aportar queda pendiente, y ahora se
sabe que su sitio no puede ser EDIT 1 sin conservar el gráfico de operadores.

## 4. Estado de lo demás que pediste

No se tocó en esta pasada, y conviene decirlo antes de que lo busques:

- **SCALE / CHORD**: el rediseño de 3 páginas de `436de58` sigue en pie (círculo de doce notas, teclado, grados,
  carriles de voces). No se le añadieron degradados, partículas ni animaciones nuevas.
- **Identidad Solar Heat global** (degradados, brillos, partículas de fondo, transiciones): no se aplicó a todas las
  pantallas.
- **Planetas**: siguen siendo cinco cuerpos con cinco reacciones distintas al tacto más la estrella; no se añadieron
  superficies nuevas (cráteres, relieves).
- **Visualizador**: no recibió la banda galáctica ni cambios de partículas.
- **MIXER, PERFORMANCE, MOD MATRIX, PRESET BROWSER, motores**: sin cambios visuales en esta pasada.
- **Arranque**: no se investigó la pausa previa a la animación final.

## 5. Verificación de esta pasada

| Comprobación | Clase | Resultado |
|---|---|---|
| `ui_test` completo | host x86 | 0 FAIL |
| Barrido `ui_render` + `tests/ui_render.py` | host x86, 190 escenas × 22 paletas | 0 hallazgos, 0 desalineaciones |
| Compilación AC791N | no ejecutada en esta pasada | — |
| Unidad física FM-1 | no ejecutada | — |

Las capturas de este documento son render de host en la paleta GREY: no son hardware, y no demuestran nada sobre el
panel real.
