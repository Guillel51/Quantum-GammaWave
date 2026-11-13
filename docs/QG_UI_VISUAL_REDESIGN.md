# Rediseño visual de la UI — estado de esta pasada (6.3)

Este documento dice lo que se cambió en esta pasada, con qué evidencia, y lo que sigue pendiente. No afirma que el
rediseño integral esté terminado: sólo una parte se implementó aquí.

## 1. Lo que se implementó: la Vía Láctea de HOME

El cielo del *bay* de HOME (88 × 52 px, dentro del panel de la izquierda) tenía dos «bandas de nebulosa» de doce
píxeles cada una, dibujadas como puntos sueltos. A 240 × 240 eso se lee como seis motas, no como una galaxia, y el
fondo no daba la profundidad contra la que están afinados los planetas.

Ahora cruza el bay una banda galáctica real, en `firmware/src/ui_being.c`:

- **Núcleo, halo y polvo.** Treinta y seis columnas (`MW_X`, `MW_Y`, `MW_T`) trazan la línea central, que se
  engrosa en un bulbo donde giran los brazos; cada columna pinta núcleo, halo a ±1 px, polvo a ±2/±3 px y, una de
  cada cuatro, una franja de polvo que corta la banda.
- **Un solo brillo para todo el cielo.** La luminiscencia sale de un único `bg_sin()` de reloj lento, así que la
  banda respira entera en vez de parpadear píxel a píxel. La forma no depende del reloj: no puede vibrar entre
  fotogramas.
- **Coste acotado.** Tablas estáticas y un `sys_pset()` por entrada (≈ 110 píxeles por fotograma en el caso peor),
  sin trigonometría por píxel y sin asignaciones. Se retiraron los dos bucles de nebulosa anteriores.
- **Las estrellas de la banda.** Una estrella fija que cae dentro de la banda ya no se dibuja encima: la luz de la
  banda la representa, lo que evita el «doble punto» que se veía antes.

Antes y después, render de host en la paleta GREY (no es hardware):

`.shots/home_before.png` → `.shots/home_after.png` (el panel del bay, abajo a la izquierda).

## 2. Lo que ya existía y no se duplicó

- **Planetas de HOME**: cinco cuerpos con cinco mecanismos de reacción distintos al tacto (`PK_SPIN`, `PK_SWELL`,
  `PK_SLING`, `PK_RIPPLE`, `PK_FROST`), más la estrella, que responde a su vez y hace reaccionar a todos los cuerpos
  a la vez. La excitación viene de las notas reales de los pares de pistas 1/4, 2/5, 3/6. No es «un planeta animado»:
  es uno por cuerpo.
- **Visualizador**: analizador de espectro de 16 bandas con picos que decaen, medido del audio real.
- **MIXER y HOME 5/9**: medidores por pista y estado de mute/actividad.

## 3. Lo que queda pendiente de este encargo

- Vía Láctea en el **visualizador** (hoy tiene su propio fondo, distinto del bay de HOME).
- Detalle de superficie por planeta (cráteres, relieves) más allá de las bandas y el terminador actuales.
- Revisión de densidad del MIXER y de las pantallas nuevas (FX BUSES, SCL, MOD MATRIX, PRESET BROWSER) contra las
  referencias de JIANT.
- Capturas «antes/después» de todas las pantallas y barrido visual completo.

## 4. Verificación de esta pasada

- `ui_test` (host x86): sin FAIL.
- `ui_render`: 196 escenas renderizadas en 22 paletas; `tests/ui_render.py` sin hallazgos de lint ni desalineación.
- No se compiló AC791N ni se midió en unidad física en esta pasada; el coste añadido es un bucle de 36 iteraciones
  con tablas constantes.
