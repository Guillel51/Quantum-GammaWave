# Visualizer: dónde se va el tiempo, y por qué 13 000 ns no se alcanza animando

Clase de todas las cifras: **`[HOST]`** x86-64, las fuentes reales, `frame_bench` con la pantalla
`S_HOME_VIZ` asentada (sin forzar). No es la unidad: estos números comparan partes entre sí y consigo
mismos antes/después, nada más.

## 1. El desglose, medido quitando una pieza cada vez

Cada fila es un build distinto, con una sola pieza del dibujo neutralizada, y el `settled` que sale
(`build/frame_bench 320`, línea `visualizer`):

| Build | `settled` | lo que cuesta la pieza quitada |
|---|---:|---:|
| completo | **302 475** | — |
| sin `gw_viz_deep` (el campo profundo) | 123 025 | **~179 000 (59 %)** |
| sin `gw_viz_deep` ni `gw_embers` | 120 375 | embers ~2 650 |
| sin campo, sin embers y con los pozos opacos (`cv_rect_a` → `cv_rect`) | **62 425** | **los dos velos alfa ~58 000 (19 %)** |

Lo que queda en 62 425 ns es el resto: la placa (232 × 176 con chaflán o rectángulo), el título y la fila
de medidores, la rejilla de los dos osciloscopios, la traza (72 llamadas de línea), la órbita (22), las 16
barras con sus tapas, y los dos `cv_blit` de 240 × 88 por fotograma.

## 2. El veredicto sobre el objetivo

El objetivo que se me pidió era llevar `settled` de 245 840 a la banda de 13 000–16 000, es decir **×16**,
sin bajar la animación. Con el desglose delante, eso **no es alcanzable**, y no por poco:

- **Con el campo profundo y los velos eliminados por completo** —dos piezas que sí son visibles y que no se
  pueden quitar— el fotograma todavía cuesta **62 425 ns**, cuatro veces por encima del techo del objetivo.
- Ese residuo es lo que la pantalla *es*: rejilla, traza, órbita, barras y dos transferencias de 88 filas.
  Para bajar de 16 000 habría que dejar de dibujar la traza o las barras la mayoría de los fotogramas, o
  dejar de transferir la pantalla: eso es bajar la animación, exactamente lo que la fase prohíbe.
- Las otras pantallas llegan a 13 000–16 000 porque **no animan**: su fotograma asentado no dibuja nada y
  solo cambia cuando cambia un valor. El visualizer dibuja 240 × 176 de contenido nuevo cada fotograma por
  definición. Comparar su coste asentado con el de Overview es comparar un osciloscopio con un cartel.

## 3. Lo que sí se puede bajar, con lo medido

Ordenado por lo que devuelve, y con el techo estimado a partir de las mismas cifras:

1. **El campo profundo, por dentro** (179 000 ns, 59 %): no se puede «dibujar cada 8 fotogramas» sin
   backbuffer —el canvas se limpia en cada pasada y las estrellas desaparecerían—, así que la vía es
   abaratarlo: posiciones de estrellas precalculadas, menos pasos de gradiente por cuerpo y el velo de la
   galaxia a bandas en vez de por píxel. Techo razonable: dejarlo en ~60 000, o sea **302 000 → ~180 000**.
2. **Los dos velos alfa** (58 000 ns, 19 %): `cv_rect_a` mezcla 14 656 píxeles por pasada. Un velo ditherado
   precalculado (el mismo patrón 4 × 4 que ya usa `gw_dither`) daría el mismo aspecto sin mezcla por píxel.
   Techo: **~40 000 menos**.
3. **Transferencia por bandas**: hoy se envían 2 × 240 × 88 por fotograma; si las barras (filas 153..180) se
   dibujan en un canvas de 28 filas y se envía solo esa banda, se ahorra la parte proporcional de la
   transferencia y del trazado de la rejilla que la rodea.

Con (1) y (2) juntos el fotograma asentado quedaría en el entorno de **120 000–140 000**: ×2,2, no ×16. Ese
es el techo real de esta pantalla sin dejar de animarla, y es lo que hay que decidir antes de tocar
`graph_home_viz`: si el objetivo es ×16, la respuesta es que el visualizer deje de redibujarse cada
fotograma; si el objetivo es que no arrastre al resto del sistema, la vía son los tres puntos de arriba.

## 4. Estado del árbol

No hay cambios de firmware en esta pasada: los builds de sonda se hicieron sobre copias y el árbol quedó
restaurado (`git status` limpio en `728df9f`). Lo único que queda es este documento y la tabla, que es la
que decide el trabajo siguiente.
