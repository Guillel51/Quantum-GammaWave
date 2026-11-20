# Fase 4 — segunda optimización de renderizado (v1.5.25)

La Fase 1 optimizó **recursos** (memoria, XIP, `.data+.bss`). Esta fase optimiza **trabajo de dibujo**: qué se
escribe en el panel, cuándo y por qué. La regla de la fase es la del pedido: nada de bajar la tasa de refresco, de
apagar animaciones ni de esconder partes de la interfaz — lo que se gana es trabajo que **no hacía falta**.

**No hay FM-1 en este entorno.** Todo lo de este documento es `[HOST]` (arnés de render sobre x86) o `[AC791N]`
(tamaño de imagen del build real). Nada de aquí afirma nada sobre el panel, el audio ni el DAC de la unidad.

---

## 1. Primero el instrumento: qué medía el arnés y qué no

`tests/ui_render.c --idle` es la medida del piso: dibuja una pantalla quieta y cuenta los píxeles que el firmware
manda al panel. Tres defectos del arnés hacían que ese número no fuera el que el instrumento gasta:

1. **Un solo frame quieto, y el primero.** El arnés hacía `rframe()` (un frame forzado) y después **un** frame
   quieto. Ese primer frame quieto todavía paga lo que dejó el cambio de estado anterior (la firma del panel, la
   tabla de filas): mide un transitorio, no el estado estacionario. Ahora hace **tres** frames quietos.
2. **"El menor de los tres".** Con tres frames siempre hay dos de la misma paridad de `ui.frame`, y el mínimo
   salía 0 para una página que se repintaba **cada dos frames** (la firma de las páginas `GR_NONE` incluía
   `ui.frame / 2`). El mínimo escondía justo lo que la fase busca. Ahora se **suman** los tres y se divide por
   tres: 0 para una página quieta, y para un parpadeo exactamente los frames en que se movió.
3. **El reloj quieto.** `fm1_ms` no avanzaba entre frames quietos, así que una página a mitad de una transición
   temporizada nunca terminaba de asentarse y se contaba como repintado permanente. Ahora avanza 33 ms (30 fps)
   entre frames: lo que se mide es la página ya asentada, que es lo que la unidad pasa segundo a segundo.

Con el instrumento corregido, **el árbol de v1.5.24 mide 38 272 px por frame quieto** (antes, con el arnés viejo,
109 056). Este build mide **29 952**: **−22 %**.

## 2. Lo que se encontró, y las dos causas

Con la traza de bandas (`UI_BANDS_DEBUG=1`) y la de rellenos, los dos peores casos eran las páginas de operador
de FM6 (`op_env`, `op_level`): **18 176 px por frame** en una pantalla que no cambiaba **ni un píxel**.

La causa no era "dibuja menos", era **que el panel recibía lo que ya tenía**:

### 2.1 Un relleno olvidaba la tabla entera

`gfx.c` guarda un hash por **fila del panel** de lo que el panel tiene, y sólo manda las filas cuyas palabras
difieren. Un relleno (`lcd_fill`) invalida esos hashes… pero `lcd_gen++` decía solamente *"hubo un relleno"* y
`gfx` tiraba **toda** la tabla. En las páginas de operador, entre las cuatro tarjetas hay cuatro franjas negras de
3 × 56 px (13 × 56 = 728 px de relleno): esas franjas hacían que el panel recibiera **el cuerpo entero** otra vez,
18 176 px por frame para volver a mandar lo mismo.

Ahora `lcd.c` registra **qué filas** tocó un relleno (`lcd_fill_y0`/`lcd_fill_y1`) y **qué rectángulos**
(`lcd_fill_x0/x1/ya/yb`, hasta ocho por frame), y `gfx.c` (`cv_fill_rows`) olvida exactamente esas filas — y de las
ventanas angostas, sólo las que el relleno pisa de verdad. Coste: 8 rectángulos y dos enteros en `.bss`.

### 2.2 Lo que se midió y **no** entró: las ventanas angostas (XIP)

La tabla de filas es una fila por **fila del panel** y sólo servía bandas de 240 de ancho: las cuatro columnas de
las páginas de EDIT (4 × 57 × 56 = **12 768 px**), las tiras del MIXER y los módulos de FX > BUSES se reenviaban
enteras cada frame.

Se implementó una tabla de ventanas (`cv_side`: hash por fila atado al rectángulo, con las tiradas que cambian y la
entrada tirada por cualquier dibujo que pise el rectángulo) y **se midió**: `op_env` / `op_level` bajaban de
**14 016 a 9 760 px** por frame y el total de 29 952 a **21 440** (−44 % frente a v1.5.24), con los 3 056 frames
del barrido idénticos salvo los 13 de la barra nueva.

**No entra en esta versión por presupuesto de XIP.** La imagen de v1.5.24 dejaba **1 272 B** libres; con las dos
cachés el `ld` del pi32v2 rechazó el enlace: *region `XIP' overflowed by 2 960 bytes* (`[AC791N]`). La tabla de
ventanas cuesta ~3,9 KB de código (el hash por fila de cada ventana más la invalidación por rectángulo) — más de lo
que queda. Se queda fuera y el árbol conserva la ganancia que sí cabe: **29 952 px** (−22 %). Queda medido y escrito
para la fase en la que se recupere XIP (o para la variante que reutilice `cv_row_hash` con una clave de 32 bits en
lugar de la estructura por ventana).

### 2.3 Medición, escena por escena `[HOST]`

| escena | v1.5.24 | v1.5.25 |
|---|---|---|
| `op_env` (EDIT 1, FM6: el gráfico del operador) | 18 176 px/frame | **14 016** |
| `op_level` (EDIT 1, FM6: los niveles) | 18 176 px/frame | **14 016** |
| resto de las 191 pantallas × 22 paletas | 1 440 – 2 800 | igual o menos |
| **total de las escenas quietas** | **38 272** | **29 952** |

El tope del test baja de 120 000 a **33 000** (el piso medido +10 %): una regresión de esta clase vuelve a fallar
el arnés en vez de pasar desapercibida.

### 2.4 Dos reglas que la caché obligó a escribir

Una caché de filas sólo es correcta si **todo** lo que cambia el panel se lo dice. Estas dos no estaban:

- **Un panel que se durmió no tiene encima la foto que tenía.** `SLPIN`/`DISPOFF` dejan la pantalla a oscuras y la UI
  redibuja la página antes de `DISPON` (`ui.force`); con la tabla de filas intacta, una fila "que no había cambiado"
  no se mandaba a un panel que ya no la tenía. Ahora `lcd_power()` y `lcd_wake_now()` olvidan **el panel entero**
  (`lcd_panel_forget`), que es exactamente lo que ese `ui.force` paga. Lo encontró `tests/ui_test.c` (SCREEN OFF:
  "the frame before DISPON redrew it all"), no una revisión a ojo.
- **Quien vacía su framebuffer tiene que decirlo.** `gfx_forget()` es el contrato del arnés y de cualquier relleno
  de pantalla completa; `tests/ratchet_test.c` vaciaba `host_screen` sin llamarlo y con la caché vieja no se notaba
  (cualquier relleno invalidaba la tabla entera). Ahora lo llama en sus cinco vaciados: la tabla de filas describe
  el panel, y un panel vaciado por el arnés es un panel que no tiene nada.

## 3. La barra fina junto a los vúmetros de FX

En HOME > FX cada tarjeta tiene un vúmetro de plasma de 16 × 43 px que dice **lo que el bus devuelve**. No decía
nada de **lo que se le pide**: un bus silencioso es una barra plana con cualquier fader, así que "por qué no se oye
el CHORUS" no tenía respuesta en pantalla (el fader era sólo el número, y de los 5 glifos de "100 %" el `%` ya
rozaba la columna del vúmetro).

Ahora hay una **barra de fader** de 2 px, de la misma altura que el vúmetro y 4 px a su izquierda, en el hueco que
la línea del valor deja libre entre `cx + 29` y `mx − 1`:

- la ranura es `T_BG` y tiene un labio `T_LINE` arriba: se ve el **lugar** aunque el valor sea 0;
- el relleno entra desde el pie en **seis bandas** de un degradado `T_THEME → T_ACCENT` (oscuro abajo, acento
  arriba), así que la cabeza de la barra es el nivel;
- el valor es el mismo `t->p[P_DIST + i]` que el número (`0..127` MIDI, mostrado como %), no una invención;
- coste: seis rellenos de 2 px por tarjeta, sólo en esa página (los vúmetros de al lado ya se redibujan).

`[HOST]`: los 13 frames `*_fx.ppm` de la paleta cambian; **ningún otro** de los 3 056 frames del barrido cambia.

## 4. Auditoría de pantallas (la que hace el arnés)

El barrido completo de esta versión, `[HOST]`:

- **191 pantallas × 22 paletas** + el barrido de páginas y valores, **0 hallazgos de lint** (superposiciones, texto
  fuera de su caja, tintas sobre su propio color), **0 píxeles GREY/MONO fuera de gris/neutro**;
- **3 785 084 medidas de alineación** en 74 lugares, **0 fuera por 1 px o más**;
- `--regions`: 872 regiones distintas, todas dentro de las dos columnas de la selección, **0 en el resto**; el
  frame que deja contra un redibujo entero, **0 px distintos**;
- `--bands`: cada paso cuesta las filas que cambian, y el frame queda exacto;
- `--headers`: ningún encabezado cambia un píxel con la pantalla quieta, en FLAT y LINE, con ANIM y sin él;
- `--announce`: lo que se dice es lo que la pantalla tiene (Screen Reader, #105).

Y la comprobación que importa para las dos cachés nuevas: **de los 3 056 frames, sólo cambian los 13 `*_fx`**
(los de la barra nueva). Todo el resto sale **byte a byte igual** que en v1.5.24, que es la prueba de que las
tablas de filas y de ventanas no se olvidan de nada que el panel tenga que ver.

## 5. Issues

- **#105 (Screen Reader).** Cerrado por evidencia, no por existencia de tests: la Fase 3 dejó `ann_*` y el modo
  `--announce` recorre las pantallas comparando lo **dicho** contra lo **pintado** (y comprobando que sin lector no
  se dice nada). En esta versión sigue en verde.
- **#106 (pendiente de unidad).** Sigue **abierto**: pide medidas del panel y del DAC de la FM-1 física
  (`docs/FASE2_ISSUES_106_107.md` §1). No hay unidad en este entorno y no se marca cerrado.

## 6. Lo que no se tocó, y por qué

- **DSP y audio:** ni una línea. Esta fase es de dibujo; el audio no cambia y no se afirma nada de él.
- **Animaciones y tasa de refresco:** intactas por diseño. Todo lo que se ganó es trabajo repetido que no producía
  un píxel distinto.
- **Formato de proyecto, presets y parámetros:** sin cambios; los presets de v1.5.24 cargan igual.
- **Coste `[AC791N]`:** imagen y `.data+.bss` en la tabla de `docs/VALIDATION_MATRIX.md` (fila de esta fase).
  Las dos tablas nuevas son de `.bss` (no ocupan XIP por datos), y el código que las usa es de decenas de bytes.
