## v1.8.0 — La matriz de modulación enseña lo que aplica, y la etiqueta que se cortaba

**La matriz ya existía y no se duplicó.** Ocho slots por pista (cuatro en parámetros, cuatro en `track_t.mx`),
diecisiete fuentes (LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, M1..M4, STEP, NOISE, PAT, BAR), treinta y cuatro
destinos más el banco X1..X4 de cada motor y los dos del master, profundidad bipolar de -100 a +100, guardado en
proyecto y en presets de usuario, y una vista de mapa de destinos. Todo eso queda como estaba: esta entrega no
añade una segunda matriz al lado de la que hay.

**Lo que cambió es lo que la pantalla dice.** La barra de actividad de cada fila dibujaba el valor *de la fuente*:
con profundidad -50 o +10 se movía igual, así que la única lectura posible era "la LFO se mueve", no "esta ruta
está moviendo algo". Ahora dibuja **el producto que el motor aplica**, `fuente x profundidad`, con signo:

- el centro es "sin modulación" para toda fuente, bipolar o no -- una fuente unipolar con AMT -100 ya no parece
  apagada;
- el lado lo decide la polaridad de la ruta, y el color también (acento arriba, medio abajo);
- una ruta de poca profundidad dibuja una barra corta, así que la profundidad se ve sin leer el número.

El producto vive en `mod_contrib()` (`mod.c`), que es exactamente el que usan `mod_block_slot` y `mod_voice_add`
(AMT 64 = 100 %): la pantalla no puede mostrar una ruta hacia un lado mientras el motor la mueve al otro.

**Seis comprobaciones nuevas** en `tests/mod_test.c`: AMT 64 no aplica nada; la polaridad invierte el signo y es
simétrica; doble profundidad, doble cantidad; una fuente unipolar con AMT -100 sigue moviendo; la profundidad
completa es la fuente misma y los extremos quedan dentro de Q15 sin desbordar; una fuente vacía no aporta nada.

**Y un texto que se cortaba.** La columna del PROYECTO en la página de canciones decía `PROJ` en un ancho que no
lo admite y el barrido lo daba como elipsis: ahora dice `PRJ`. Es una de las tres elipsis que informa el audit
(las otras dos son avisos largos con "…" a propósito).

