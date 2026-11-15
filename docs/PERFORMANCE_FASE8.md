# FASE 8 — la placa de cada módulo se pinta una vez

**Clase de la medición: host x86, `ui_render` compilado con `cc -O1`.** No es una medición del AC791N ni
del emulador: las cifras absolutas no se trasladan y solo los cocientes entre dos binarios construidos
igual tienen valor. La prueba principal de esta fase **no es el reloj**, es un contador exacto.

## El punto caliente, medido

`cv_rrect` rellenaba cada placa con tres rectángulos (cuerpo + banda izquierda + banda derecha) y después
`cv_plate_ground` / `cv_plate_cut_ground` pintaban la rampa **encima de esos mismos píxeles**, incluidos el
marco y las esquinas, que esas dos funciones vuelven a pintar con su propia mezcla. Cada placa se escribía
dos veces: en una tarjeta de 57 × 44 son 4466 píxeles rellenados para una superficie de 2508.

Para medirlo sin depender de la carga de la VM se añadió al arnés un contador **determinista** de píxeles
rellenados por frame (`GFX_HOOK_FILL` en `firmware/src/gfx.c`, no-op en el firmware; `tests/ui_render.c` lo
acumula y lo imprime en la columna `px filled` de la tabla de coste). Ese número es idéntico en cada corrida.

Antes del cambio, una pantalla de 57 600 píxeles rellenaba hasta 118 014.

## El cambio

Cuando la rampa va a correr (`c == T_SURF`, `!cv_plate_flat`, y el umbral de cada estilo), `cv_rrect` **no
hace el relleno base**: la rampa, el marco que dibujan `*_ground` y el bucle de esquinas cubren la placa
entera. Sin rampa (placas pequeñas, `cv_plate_flat`, estilos sin degradado) el relleno base sigue igual.

## Resultado

Píxeles rellenados por redibujado completo (exacto, una sola corrida basta):

| pantalla | antes | después | |
|---|---:|---:|---:|
| menu | 83 644 | 47 940 | **−42.7 %** |
| confirm_project | 52 572 | 28 700 | **−45.4 %** |
| fx | 151 253 | 106 257 | −29.7 % |
| mixer | 57 136 | 45 992 | −19.5 % |
| presets | 83 360 | 72 216 | −13.4 % |
| master | 83 259 | 72 115 | −13.4 % |
| drum | 83 722 | 72 578 | −13.3 % |
| reverb2 | 84 319 | 73 175 | −13.2 % |
| step | 93 299 | 82 155 | −11.9 % |
| roll_chords | 95 132 | 83 988 | −11.7 % |
| drumx | 103 709 | 92 565 | −10.7 % |
| home | 118 014 | 106 870 | −9.4 % |
| about_credits | 873 | 873 | ±0 (no tiene placas) |

Tiempo del mismo redibujado, **mínimo de cuatro corridas intercaladas** de los dos binarios (el host estaba
cargado durante la campaña: `home` llegó a 264 µs frente a sus 138 µs en una sesión tranquila, por eso solo se
usan los cocientes):

| pantalla | 2 pasadas | 1 pasada | |
|---|---:|---:|---:|
| TOTAL (17 pantallas) | 2275.3 µs | 2150.5 µs | **−5.5 %** |
| menu | 164.4 | 140.1 | −14.8 % |
| fx | 210.1 | 180.0 | −14.3 % |
| confirm_project | 125.8 | 110.5 | −12.2 % |
| home | 264.4 | 259.0 | −2.0 % |

**La imagen no cambia.** `diff -rq` de los 189 renders × 22 paletas contra el árbol sin el cambio: **0
ficheros distintos**. `ui_render` sale con 0: 0 hallazgos de lint, 0 tintas sobre su propio color, 0 píxeles
GREY / MONO fuera de gris, 3 400 410 mediciones de alineación con 0 desalineaciones. `theme_test`: todas las
paletas pasan, peor margen 4.08:1.

## Trampa de medición que quedó documentada

Con el contador escrito como función (`hk_fill_px`) el host medía **+16.6 %** de tiempo con el código sin
optimizar: una llamada por relleno, y MENU hace cientos de rellenos de una fila. Pasado a macro (`px_fill +=
…`) deja de verse en el reloj. Toda comparación de esta fase se hizo con **los dos binarios instrumentados
igual**: sin eso, el arnés mide su propio contador.

## Lo que esta campaña NO mide

- **AC791N** (`.bss`, ciclos reales, XIP): sin `JIELI_TOOLCHAIN`/`AC79_SDK` no hay build de destino. Lo que se
  afirma es un recuento de escrituras de píxel, que en el destino es un `for` de un `store` por píxel.
- **Emulador WASM**: sin `emcc`.
- **Audio**: el cambio es de dibujo; los goldens de audio (`tests/golden.txt`, 293) no se tocaron.
