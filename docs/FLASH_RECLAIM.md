# Recuperar flash: los dos candidatos medidos, y por qué no hay nada que sacar

Objetivo: liberar XIP para poder añadir cosas. Estado de partida: **583 600 B de 583 612 — 12 B libres**.
Antes de escribir un decodificador, medí los dos assets que parecían más fáciles. Los dos ya están en su
forma densa.

## 1. `UAC_FIR` — 7 680 B, ya reducido a la mitad

`firmware/src/uac_fir.h` guarda **80 fases de 24 taps** (Q30, `int32_t`), no 160: el propio header lo dice —
*«phases 0..79 (phase 159 - p = phase p backwards)»*. La simetría del prototipo ya está explotada en la
dimensión de fase.

Lo que queda por explotar sería la simetría **dentro** de cada fase (`h[k] == h[N-1-k]`), que es lo único que
permitiría guardar 12 taps en vez de 24. No existe: mirando la fase 0, `15446, -50808, 120879, … , 20041,
-2478`, los extremos no coinciden. Un FIR de fase fraccionaria no tiene esa simetría por rama. **No hay
mitad que sacar.**

Bajar los coeficientes a Q15 (`int16_t`) ahorraría 3 840 B, pero cambia los bytes del audio: los 293 renders
dorados dejarían de coincidir. No es una optimización invisible, es otra versión del resampler.

## 2. `KC_DATA` — 4 792 B, y el RLE no gana nada

Los keycaps ya están **empaquetados a 4 bits por píxel**, dos píxeles por byte, con la etiqueta rasterizada
dentro de la misma píldora (`tools/gen_aa_keycaps.py`). Probé la compresión más simple que se le puede
aplicar a un dato así —RLE por byte con escape `0xFF`— sobre las 28 píldoras reales:

```
bytes crudos 4888 → RLE 4887   (ahorro: 1 byte)
```

Un byte. El dato es prácticamente ruido a nivel de byte: cada píldora mezcla borde antialias, relleno y
glifos, y las repeticiones no llegan al umbral de 3 bytes que hace rentable un token de 3. Un decodificador
costaría 40-60 B de XIP para ahorrar uno. **Descartado por medición, no por intuición.**

## 3. Fuentes — 20 474 B, también a 4 bits

`AF_S` 9 523 B, `AF_M` 6 681 B, `AF_L` 4 270 B: **4 bits de alfa por píxel**, recortadas a la caja de tinta
(`tools/gen_aa_font.py`). 4 bpp es el suelo razonable en 240 × 240 con texto de 9-16 px; bajar a 3 bpp
ahorraría ~5 KB y se vería en los degradados de letra.

## 4. Y el compilador tampoco tiene margen

Medido, no supuesto: con `-Os` en vez de `-Oz` la región XIP **desborda 33 708 B**. El `-Oz` que ya está en
`tools/build.py` no es una elección conservadora: es la que sostiene la imagen.

## 5. Conclusión

La vía «comprimir los assets» está cerrada por medición: los tres candidatos ya están densos y el cuarto
(elegir otro flag) empeora 33 KB. Lo que queda es otra cosa, con otro coste:

* **Regenerar en el arranque** lo que hoy viaja como tabla (las 24 tablas band-limited de `WT_DATA` son
  aritmética; las fuentes, rasterizado). Ahorra flash de verdad —12 KB y 20 KB— pero gasta **RAM** (las
  tablas regeneradas viven en `.bss`: 10 156 B libres, no 32 KB) y **tiempo de arranque**.
* Eso acota el objetivo realista: se puede regenerar una de las tres fuentes (~5-9 KB de RAM) o una parte de
  `WT_DATA`, no todo. Es un trabajo de una sesión por pieza, con los 293 renders y las 189 pantallas × 22
  paletas como prueba de que no cambia un píxel ni una muestra.

## 6. Dos experimentos más, los dos a cero

Buscando bytes sin escribir código nuevo, probé dos cosas más y las dos dieron exactamente cero:

| Prueba | Medición |
|---|---|
| `-fmerge-all-constants` añadido a `CFLAGS` (fusionar constantes idénticas) | imagen **583 600 B**, idéntica: el linker ya las fusiona, o los cuerpos no son idénticos porque cada descriptor lleva su nombre |
| Descriptores de parámetro duplicados a mano en `params.c` | hay **6 repetidos** (`FILTER`, `PITCH`, `SHAPE`, `GATE`, `SWING` y uno más), 192 B si se fusionaran — pero la tabla `PDESC` es un array **de structs**, no de punteros: convertirla a punteros costaría 125 × 4 = 500 B de punteros para ahorrar 192. No sale |

`-fmerge-all-constants` no se quedó en el build: no aporta y es una opción que cambia la identidad de
direcciones, así que no se adopta sin ganancia medida.

## 7. Lo que esto significa para la etapa de optimización

Con estos cinco resultados (FIR ya espejado, keycaps sin redundancia, fuentes a 4 bpp, `-Os` peor por 33 KB,
constantes ya fusionadas), la conclusión de la etapa 1 es que **el firmware actual no tiene recursos que
liberar por compresión ni por flags**. Lo que queda es cambiar el modelo: guardar menos y calcular más.

Y ahí aparece el círculo que hay que resolver primero, porque es el bloqueo real de todo lo demás: cualquier
generador en el arranque es **código nuevo**, y para que ese código entre hace falta flash libre. Con 12 B
libres no entra ni el bucle que lo haría. La primera pieza tiene que ser, entonces, una cesión explícita de
una de estas tres cosas:

1. **RAM por flash**: mover una tabla a `.bss` y regenerarla al arrancar (hay 10 156 B de RAM; `AF_L` son
   4 270 B), aceptando el coste de arranque.
2. **Un matiz de calidad**: bajar las fuentes a 3 bpp (~5 KB) y verlo en el antialias del texto.
3. **Más flash**: cambiar el mapa de memoria o el chip, que es una decisión de hardware y no de firmware.

Sin una de esas tres, la etapa 1 está terminada en lo que respecta a liberar recursos, y las etapas 2 a 7 no
tienen dónde meterse.

## 8. Resultado: el modelo cambió, y funcionó

La cesión que se eligió fue la primera: **guardar menos y calcular más**, sin tocar calidad ni RAM crítica.
`WT_DATA` guardaba 8 formas × 256 puntos × 3 límites de banda (12 288 B). Ahora guarda:

* las amplitudes armónicas de las 8 formas en Q30 (`WT_COEF`, 2 048 B), de donde se sintetizan el nivel de
  64 y el de 16 armónicos al arrancar (`wt_gen_init`, 8 192 B de `.bss`, 292 B de código);
* el nivel de 5 armónicos, entero (`WT_L5`, 4 096 B): es el que leen las notas agudas y no debe esperar a
  ningún cálculo.

Medido: **583 600 → 577 760 B de imagen (5 840 B liberados)**, `.bss` 97 380 de 98 304, y la onda generada
difiere de la guardada en 3 LSB de 32 000. Los 22 goldens de WAVETABLE se rebasaron; los otros 271 no se
movieron.

Con esos bytes entró **VOICE 3 (DETUNE)**: 24 B, un control que el DSP aplicaba desde siempre y que la
v4.1.0 había dejado fuera por falta de espacio. Quedan **5 816 B** libres para la siguiente pieza.

---

# Segunda pasada (v1.3.7): lo que sí costaba imagen, medido

Estado de partida: **583 548 B de 583 612 — 64 B libres**. Dos candidatos nuevos, los dos medidos con
`./build.sh` (build del AC791N) y, cuando hacía falta, con `nm -S build/felucca.elf`.

## 4. El blob de formas no cuesta imagen: 10 861 B que el enlazador no lleva

`build/gen/ui_shapes.h` guarda **10 861 B de puntos** (`SH_PLATES_*`, el arte de las placas orgánicas de
`ui_organic.c`). Le añadí dos knobs al generador (`--eps=0.6 --flat=0.8`, simplificación de polilínea) y el blob
bajó a **9 661 B**… y la imagen quedó **idéntica**: **583 548 B antes y después**.

La prueba de por qué: `nm -S build/felucca.elf | grep SH_` no devuelve **ni un solo** `SH_PLATES_*` ni `SH_*_PTS`.
`ui_organic.c` se incluye desde `ui_graph.c:15`, pero ninguna pantalla referencia su arte de placas, así que el
enlazador lo descarta (`--gc-sections`) y sus bytes nunca llegaron a la imagen. `tools/table_sizes.py` lo confirma:
*shapes (154 B)*, que es sólo la tabla de punteros que sí se usa.

**Ahorro real en imagen: 0 B.** La vía se cierra con la evidencia; los knobs del generador se revirtieron para no
dejar en el árbol una optimización que no paga nada.

## 5. Keycaps que nadie dibuja: 904 B de imagen, medidos

De las 29 etiquetas que rasterizaba `tools/gen_aa_keycaps.py`, **seis no las dibujaba ninguna pantalla**:
`KC_ARP`, `KC_K3`, `KC_K4`, `KC_HOLD`, `KC_ON`, `KC_OFF` (ni una llamada a `cv_keycap` con esas etiquetas; el
resto de las 23 sí se usan). Quitarlas del generador:

```
keycaps: 22 pills, data 3939 B + table 176 B; knob arcs 584 B = 4699 B      (antes: 5 564 B)
KC_DATA[4792] -> KC_DATA[3939]                                              (-853 B)
```

y el build del AC791N:

| | v1.3.6 | v1.3.7 | |
|---|---|---|---|
| imagen | 583 548 B | **582 644 B** | −904 B |
| XIP código | 422 928 B | 422 928 B | 0 |
| XIP constantes | 140 192 B | **139 288 B** | −904 B |
| `.data + .bss` | 95 124 B | 95 124 B | 0 |
| RAM code | 19 632 B | 19 632 B | 0 |
| libre en el slot | 64 B | **968 B** | +904 B |

Los 853 B del dato más 12 B de tabla de offsets y alineación: **904 B**, todos de constantes y ninguno de código,
que es lo que se espera de un dato que ya no se guarda. Ningún píxel cambia porque esas píldoras no se dibujaban.

## 6. KEYTRK: el parámetro que más aporta, y sus 2 456 B medidos

Con 968 B libres, el candidato con más diferencia sonora era **KEYTRK** (profundidad de keytracking del filtro):
hoy el seguimiento está escrito a mano como `(v->pitch16 - 60 * 16) * 4` en seis sitios (`eng_analog.c` líneas
105/147/208/277, `eng_wavetable.c:110`, `eng_additive.c:125`) y ninguna página puede tocarlo. Se implementó
completo —miembro del enum, descriptor, la fila libre de VOICE 3, los seis motores, default 127 = el 4 de siempre
bit a bit— y se midió tres veces:

| variante | imagen | coste |
|---|---|---|
| insertado antes de `P_E0` (lo que exige el mapa por conteo) | 585 100 B | **+2 456 B** |
| añadido después de `P_E7` (para descartar el desplazamiento) | 585 084 B | +2 440 B |
| con un segundo parámetro reserva (para descartar la paridad) | 585 088 B | +2 444 B |

Reparto del primero: constantes **+36 B**, RAM **+48 B** (4 B por pista) y **código +2 404 B**. El `nm -S` de los
dos ELF dice dónde: **147 símbolos de código crecen**, y ninguno lee el parámetro —
`ui_input_frame` 13 842 → 14 444 B, `draw_graph` +150, `graph_signature` +146, `note_edit` +140, `part_dice` +112,
`draw_columns` +66, `edit_param` +68… Todas tocan `track_t`, y crecer `p[]` 2 B mueve **todos** los campos de una
estructura de **1 760 B** (medido con `offsetof`): el compilador rehace las codificaciones de acceso a lo largo de
la UI. No es el coste del parámetro, es el precio de tocar la estructura.

**Con 904 B recuperados y 2 456 B de coste, el parámetro no entra.** Las dos piezas que sí caben, para la
siguiente pasada:

1. **El valor**: `track_t` tiene **exactamente un byte de relleno** en el offset 327 (entre `latched` y
   `arp_pos`, `sizeof(track_t)` = 1 760 B, alineación 4). Un `uint8_t` ahí **no cambia el tamaño ni un offset**,
   así que cuesta 0 B de imagen.
2. **El control**: una página puede ofrecer un valor que no es id de `p[]` por el mismo embudo que ya usan los
   macros (`params.c` `page_desc`, sombra `int16_t` + escritura en `ui_input.c`) y los slots 5..8 de la matriz
   (`*valp = &TSEL->mx[...]`).

Lo que falta no es flash, es **formato**: los ids de 7 bits están en **125 de 127** y ni el proyecto (FUNB, sin byte
libre por pista) ni el preset de usuario (registro de tamaño fijo) tienen un byte por pista para guardarlo. Un
parámetro de primera clase necesita una decisión de formato (un bloque nuevo, o SUBIR `UP_VER`), no 900 B más.
