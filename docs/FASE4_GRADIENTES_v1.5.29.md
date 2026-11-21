# FASE 4 — degradados, jerarquía y legibilidad en 240 × 240

La Fase 4 pedía algo visual más allá de la barra del fader. Esto cambia **la primitiva** de la que cuelgan
todas las pantallas —el degradado del panel y el del suelo— en vez de retocar páginas sueltas, y lo mide:
capturas antes/después del propio firmware (no mockups), lint en cero y la alineación en cero.

**Clase de cada medida de este documento:** `[HOST]` = render del firmware en x86 (`tests/ui_render.c`, los
mismos `.c` que corre la FM-1). No hay unidad física en este entorno: nada de aquí dice cómo se ve en el panel.

## 1. Lo que se cambió, y dónde

Todo está en `firmware/src/gfx.c`, en tres funciones que **toda** pantalla usa. Ninguna página cambió sus
llamadas, así que las 191 pantallas × 22 paletas se movieron juntas y ninguna puede quedar con el degradado
viejo.

| primitiva | antes | después |
|---|---|---|
| `plate_ramp_at` (rampa del panel) | lineal `hi → lo` | **cuadrática**: la mitad del panel queda cerca del pie oscuro y la luz se concentra arriba |
| `cv_plate_ground` / `cv_plate_rows` (cara de cada tarjeta, lista, módulo) | `hi = mix(c, TEXT, 7 %)`, `lo = mix(c, BG, 58 %)` | `hi = 4 %`, `lo = **74 %**` (más oscuro abajo, menos lavado arriba) |
| `cv_plate_cut_ground` (módulos con esquina cortada: FX > BUSES) | `hi = 14 %`, `lo = 58 %` | `hi = 8 %`, `lo = 74 %` |
| `ux_ground_rows` / `ux_ground_at` (suelo de HOME, FX > BUSES, FX > master, MOD…) | rampa lineal, acento al 10–45 % | **cuadrática**, rampa `× 4/5` y **acento a la mitad**: el suelo se lee como fondo, no como color |

Razón de cada número: el acento (cian/ámbar según paleta) iba del 10 al 45 % en el suelo y competía con el
contenido —el espectro y la curva de HOME peleaban contra un lavado de teal—; a la mitad, el suelo se hunde y
lo que está encima gana contraste. El pie del panel al 74 % hacia `T_BG` y la curva cuadrática hacen el resto:
el degradado existe, pero pasa de ser una mancha a ser **una superficie con luz arriba**.

`ux_ground_at` (el color que se usa para mezclar un texto que se apoya en el suelo) lleva **exactamente** la
misma curva y el mismo recorte de acento: si no, cada glifo quedaría con un halo de un color que no está
debajo, que es justo lo que el lint llama *tinta sobre su propio tono*.

## 2. Jerarquía, legibilidad y feedback: lo revisado

Sobre las 191 pantallas (capturas completas en `build/ui_group/ppm`, informe en `build/ui_group/report.txt`):

- **Jerarquía** — etiqueta (MID), valor (THEME), unidad (MID), control activo (ACCENT). Se mantiene: el
  cambio de degradado **la refuerza** sin tocar un solo color de tinta, porque baja el fondo y no sube el texto.
- **Legibilidad** — el valor y la unidad no cambian de color; lo que cambia es lo que hay debajo. El
  contraste de un texto MID contra el suelo sube en todas las filas donde el suelo bajó (ver §3).
- **Feedback** — el anillo y el punto del control activo siguen en ACCENT puro sobre un panel más oscuro: el
  único elemento que grita en la pantalla es el que tiene el foco, y ahora grita más porque el fondo calla.
- **Textos cortados** — 456 textos libres se eliden (10 distintos), exactamente los mismos que antes del
  cambio: el degradado no toca métricas de texto.
- **Nada desaparece** — 0 tintas sobre su propio color, 0 píxeles fuera de gris/neutro en GREY y MONO: el
  gris sigue siendo R = G = B en cada mezcla (las tres rampas son `ux_mix`, que satura por canal).

## 3. Medición `[HOST]`: antes / después

Píxeles distintos entre el render de antes y el de después (240 × 240 = 57 600), y el máximo cambio por canal:

| pantalla | píxeles distintos | Δ máx. por canal |
|---|---|---|
| HOME | 6 261 | 17 |
| MIXER | 20 430 | 16 |
| FX > BUSES | 15 440 | 58 |
| MASTER | 24 111 | 17 |
| MOD > LFO DEST | 26 194 | 17 |
| PRESETS | 22 632 | 17 |

Barrido vertical dentro del panel de LFO DEST (x = 120, de y = 90 a 210, paso 20), que es el degradado del
suelo sin contenido encima:

```
antes    (24,24,24) (24,32,32) (32,40,41) (32,48,49) (41,60,65) (41,68,74)
después  (24,24,24) (24,24,24) (24,32,32) (32,40,41) (32,52,57) (41,68,74)
```

La fila 2 ya no sube: el tercio superior del panel se queda en el tono del fondo y la rampa se gasta en el
último tercio. Es la diferencia entre un panel «pintado» y un panel que se lee como superficie.

## 4. Verificación ejecutada

| prueba | resultado |
|---|---|
| `tests/groups.sh ui` (191 pantallas × 22 paletas + barrido de páginas y valores) | **exit 0**: 0 hallazgos de lint, 456 textos elididos (los mismos), 0 tintas sobre su color, 0 GREY/MONO fuera de gris |
| alineación de la misma pasada | **0 de 3 785 110** medidas, off by 1 px o más |
| degradados de antes/después | las 6 capturas de `docs/shots/before` y `docs/shots/after`, del render del firmware |
| `tests/regress` (audio) | **306 golden renders, 0 cambiados**: un cambio de UI no toca una muestra |
| CPU del host (`regress`) | **no es un PASS**: en este entorno *todos* los trabajos miden 24–26 % más lentos que su baseline (máquina compartida), y la puerta compara tiempo de pared. Se informa como limitación, no como verde |

## 5. Riesgos y pendientes

- **El panel real no se miró.** Todo esto es `[HOST]`; la FM-1 sigue sin conectar (AGENTS.md §3: nada se le
  escribe). Un contraste medido en x86 y una gamma de panel real no son la misma cosa.
- **Cambio global, no por pantalla.** Es la ventaja (nada puede quedar atrás) y el riesgo (una página cuyo
  diseño dependía del lavado de acento perdió profundidad). El barrido de las 22 paletas no muestra ninguna
  que se rompa, y las capturas de las seis pantallas están en el repositorio para revisarlas.
- **Siguiente paso natural**: el mismo tratamiento para las tarjetas que usan `cv_plate_flat` (las que se
  pintan planas a propósito, p. ej. teclas y chips de 3 px), donde un degradado no se resuelve a esta altura.

Capturas: `docs/shots/before/` y `docs/shots/after/` (`home`, `mixer`, `fx_buses`, `master`, `lfo_dest`,
`presets`), paleta GAMMA, 2× para verse en pantalla.
