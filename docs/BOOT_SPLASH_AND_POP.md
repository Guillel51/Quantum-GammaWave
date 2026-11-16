# Arranque: el splash y el pop del encendido (1.0.2)

Lo que se revisó, lo que se encontró y lo que se cambió en el arranque del instrumento. Se actualiza con la
release: si algo de acá deja de ser cierto, el archivo cambia en el mismo commit que el código.

## 1. El splash es el mismo código en los dos lados

`draw_splash()` y `splash_sig()` viven en `firmware/src/ui_draw.c` y las dos rutas de arranque las llaman:

| | firmware (aparato, `main.c`) | emulador (`web/emu/felucca_web.c`) |
| --- | --- | --- |
| arte, textos, colores | `draw_splash()` | `draw_splash()` |
| animación | `splash_write(ms)` desde `spl_t0_us` (fotograma a fotograma) | `splash_write()` cada 20 ms |
| líneas | `SPLASH_LINES` = `{FELUCCA_VERSION, "QUANTUM GAMMAWAVE // QG-1"}` | la misma constante |

No hay dos dibujos: hay un dibujo. Lo que **sí** difería era la decisión de mostrarlo. El aparato lee
MENU > STARTUP SPLASH (`ui_splash`: 0 FULL, 1 SHORT, 2 OFF) y el emulador tenía `spl_anim = 1` fijo, así que
con el ajuste en SHORT u OFF el aparato y el navegador mostraban cosas distintas. Desde la 1.0.2 el emulador
lee el mismo ajuste y usa el mismo lapso (`SPL_SHORT` o `SPL_WRITE + SPL_HOLD`, 0 con OFF), así que los dos
coinciden en los tres modos.

Arte, textos y tiempos están verificados en **host**: `tests/ui_render.c` recorre la línea de tiempo del splash
(425 pasos de 4 ms) y la pinta en las 22 paletas; `build/uiA/ppm/*_splash.ppm` es el fotograma que se compara
contra la captura del navegador. Nada de esto es una medición sobre el FM-1 físico.

## 2. El pop al encender

**Causa.** La etapa de salida del códec (`pc6`) se desmuteaba **dos veces** en `fm1_audio_init`
(`firmware/hal/fm1_audio.h`): la primera con ALNK0 todavía deshabilitado, la segunda 5 ms después de
habilitarlo. O sea que el altavoz se abría sobre un DAC que no había terminado de asentarse y sobre un FIFO
que el motor todavía no había llenado: un escalón al altavoz, el pop.

**Arreglo.** El mute se mantiene durante todo el bring-up; el motor arranca sobre el buffer ya en cero y la
salida se desmutea recién cuando el DAC lleva 25 ms corriendo en silencio. El orden es el mismo que ya usaba el
encendido del reloj y del panel: primero el silencio, después la señal.

**Qué está medido y qué no.** El cambio es de orden y de tiempo en el bring-up del códec: compila en el build
AC791N (imagen 583 540 B, XIP 418 740 B) y los 293 renders dorados siguen idénticos, pero **el pop no se puede
escuchar ni medir acá**: no hay unidad conectada y no se escribe en ninguna. Lo que queda verificado es la
secuencia (una sola apertura del mute, después de que el DAC corre sobre ceros), no el resultado acústico.

## 3. El presupuesto del target, re-basado con su razón

`tests/target_budget.txt` tenía tres líneas en rojo desde `ce2833c` (LINE = PM): `phase_render` 98/84 (+17 %),
`phase_render_noise` 117/103 (+14 %) y `analog_ext` 174/157 (+11 %). La 1.0.2 saca tres ramas por muestra
(la PM plegada en la suma de fase con índice cero, la mezcla decidida una vez por bloque, los dos desbordes de
fase como valor) y deja 94, 113 y 174; el resto es la función aprobada. El re-basado se hizo con
`BUDGET_UPDATE=1` y queda escrito en el archivo por qué.

Clase de esas cifras: **cuenta estática de las instrucciones de los lazos en el código del build AC791N**
(`build/felucca.dis`), no ciclos ni carga del aparato. El chequeo primario de CPU por bloque
(`tests/regress`) da **0 entradas sobre presupuesto**, y los **293 renders dorados quedaron idénticos**, que es
lo que prueba que esta pasada no cambió el sonido.
