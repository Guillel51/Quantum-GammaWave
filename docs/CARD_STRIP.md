# La franja de una tarjeta: qué repinta un dígito que rueda

## El síntoma

Cinco comprobaciones de `tests/ui_test.c` que llevaban tiempo en rojo tenían la misma forma: la pantalla de un
cuadro animado no era idéntica al render estático forzado de la misma pantalla.

```
ui: roll: the roll's last frame is pixel-identical to the static render                       FAIL
ui: roll: header BPM 120 -> 121 and a card 10 -> 11: only the strip changes, ...               FAIL
ui: roll: a change mid-roll retargets (from the last value, restarted) and ends static         FAIL
ui: #63 soak: 150000 random frames ... cards = a forced redraw                                FAIL
```

Más dos de la disposición LARGE, que comprobaban la misma suposición por otra vía (un píxel de la tarjeta tenía
que ser exactamente `T_SURF`).

El volcado de diferencias lo dijo en la primera pasada: **716 píxeles distintos en un rectángulo de 55 × 14**,
justo dentro de la franja del valor de la tarjeta 4 (`CARD_X(3) = 180`), y el color de la pantalla estática era
`T_SURF` plano donde la animada tenía un tono mezclado más oscuro.

## La causa

Una tarjeta no se pinta de un color: `cv_rrect` la rellena con `cv_plate_ground`, un **degradado de una fila por
píxel** (de un poco de `T_TEXT` arriba a un poco de `T_BG` abajo). La franja del valor, que es lo único que se
redibuja mientras un dígito rueda, se limpiaba a **`T_SURF` plano** y luego repintaba sólo su borde:

```c
cv_begin(COL_W, ROLL_H, T_SURF);      /* flat */
cv_oy = -ROLL_Y;
card_edge(COL_W, COL_H);
```

Al terminar el roll, la franja quedaba con el color plano y las filas de arriba y de abajo con el degradado: una
banda más clara, exactamente lo que el test y el soak de 150 000 cuadros contaban como tarjeta obsoleta.

No era la optimización de la Fase 8 (`cv_rrect` salta el relleno base cuando el degradado cubre la placa):
revertirla en un worktree aparte deja los mismos fallos. El degradado es de la Fase 3, y la franja nunca lo
acompañó.

## El arreglo

`gfx.c` gana `cv_plate_rows()`: las filas de una placa, con **las mismas bandas y los mismos colores** que
`cv_plate_ground` (mismo `n`, mismo `bh`, mismo `plate_ramp_at`, así que comparte su caché). `ui_draw.c` limpia la
franja con las filas que le tocan de la tarjeta en vez de con `T_SURF` plano:

```c
cv_plate_rows(0, ROLL_Y, ROLL_Y + ROLL_H, COL_W, COL_H, T_SURF);
card_edge(COL_W, COL_H);
```

Lo que queda fuera de la franja lo recorta el canvas, así que un cuadro de roll es, píxel a píxel, el render
estático.

## Las comprobaciones que hubo que corregir, y por qué no se relajaron

Tres aserciones medían "hay tarjeta/panel acá" como "este píxel es exactamente `T_SURF`", que era falso desde que
la placa es un degradado. Ahora miden lo mismo que dicen: el píxel **no es el fondo del hueco** (`T_BG`), y en el
caso del hueco se sigue exigiendo que antes de LARGE sí fuera `T_BG`. La de dirección del roll medía "tinta"
contando píxeles distintos de un único `T_SURF`; ahora el fondo de referencia es el color más común **de esa
fila**, que es lo que la fila tiene de fondo con degradado. Ninguna aserción se volvió más laxa en lo que
comprueba: siguen exigiendo que el último cuadro sea idéntico al estático, que sólo cambie la franja, que no
queden tarjetas viejas y que la disposición LARGE ponga tarjeta donde estaba el hueco.

## Evidencia

Clase de medición: **host**, binarios nativos de esta VM (no emulador, no AC791N, no unidad física).

- `tests/ui_test.c` completo, build por defecto: **0 FAIL, `ui: all passed`** (antes: 7 FAIL).
- `tests/ui_test.c` con `-DFELUCCA_FM4=1`: **0 FAIL, `ui: all passed`**.
- El soak de 150 000 cuadros no imprime "old rolls / stale cards" y compara más de 500 momentos en quietud.
