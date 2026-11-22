## v1.7.21 — El visualizer cuesta la mitad: la mezcla de share fijo es una tabla

**Medido primero.** Un banco en el host (`tres pasadas de 300 cuadros, `-O1`, `UI_NO_DRIFT`, reloj fijo`) puso el
visualizer de HOME en **371,5 µs por cuadro** (mediana de 7 corridas) contra 88 µs de HOME/eco, 19 del MIXER y 3 del
navegador de presets: el visualizer era, por lejos, el cuadro más caro del instrumento. El desglose por partes
(apagando una y midiendo) dejó el velo translúcido del pozo del osciloscopio -- `cv_rect_a(12,63,144,72,T_BG,60)` y
`cv_rect_a(164,64,64,67,...)`, 14 656 px por cuadro -- con 117 µs: un tercio del cuadro para bajar el brillo de un
fondo.

**La causa, contada en operaciones.** Cada uno de esos píxeles pasaba por `ux_mix`: dos desempaquetados de color,
tres multiplicaciones, tres divisiones por 100 y tres saturaciones. **19 052 mezclas por cuadro** en el visualizer,
14 656 de ellas del velo.

**La corrección, que no cambia un píxel.** Cuando el share es el mismo para todo el rectángulo, mezclar es un mapa
`dst -> out` por canal que cabe en el ancho del canal: `cv_rect_a` arma tres tablas (32 + 64 + 32 entradas) con la
misma expresión y el mismo redondeo que `ux_mix` y aplica tres búsquedas por píxel. El recorte se hace una vez por
rectángulo en lugar de una vez por punto. El camino MONO conserva el original (un canal, un tono), así que GREY y
MONO no cambian de comportamiento.

**Números (host x86-64, `-O1`, 300 cuadros, mediana de 7):** visualizer **371,5 -> 187,6 µs/cuadro (-49 %)**;
mezclas por cuadro **19 052 -> 4 396 (-77 %)**. Las otras pantallas no se mueven (HOME/eco 88 µs, MIXER 19 µs,
presets 3 µs): el velo era el único sitio con una mezcla de área grande.

**La prueba que lo sostiene.** `tests/blend_budget_test.c` (en `tests/run_tests.sh`) cuenta las mezclas de un cuadro
con un contador que `gfx.c` lleva solo bajo `-DGFX_MIX_COUNT` -- un **conteo**, no un tiempo, así que no puede
fallar por un host ocupado. Tope 6 000 para el visualizer, 200 para las demás pantallas. Verificado al revés:
repuesta la implementación anterior, el test falla (19 052 > 6 000); con las tablas, pasa.

**Nada visual cambió.** `ui_pixels`: 3 088 cuadros idénticos al baseline (regenerado solo por la cadena de versión
del splash). `--idle`: 29 952 px escritos, 0 cambiados (igual que en 1.7.20). `--bands`: cada paso cuesta las
filas que cambian, 0 filas no enviadas que estuvieran mal. `regress`: 311 renders de audio, 0 cambiados.

