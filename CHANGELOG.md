## v1.8.7 — la tarjeta de presets: una pantalla que parpadeaba, y lo que se midió

Fase 1/9 del ciclo nuevo: una optimización medida, la UI repasada entera y el parpadeo de la pantalla transitoria
de presets, rediseñada como tarjeta Quantum y con pruebas que impiden que el defecto vuelva.

**El parpadeo, encontrado y medido.** La pantalla transitoria de presets (`graph_pop_plate`, 208 × 86 sobre el
panel) tenía **tres** intentos anteriores de arreglo (v1.11 dejó de reutilizar el lienzo de la página, v1.5.15 le
dio su propio lienzo de su tamaño, v1.7.20 la hizo cabalgar el blit del panel para no enviar dos veces las mismas
filas). El defecto que quedaba era otro: con la perilla quieta y la ventana de 1,5 s corriendo, **la tarjeta se
volvía a enviar en cada frame** — 17 888 px, treinta veces por segundo, de píxeles idénticos a los que ya estaban
en el cristal. Un panel que muestra cada escritura eso lo pinta como un parpadeo. Ahora la tarjeta tiene su propia
firma (`graph_pop_sig`: el sonido elegido, la lista, el filtro, la estrella, el estilo) y **se envía una sola vez**;
un cambio de página o cualquier invalidación del panel la vuelve a habilitar. Medido: `--regions` caso A, «el
pop-up: 17 888 px escritos», «un frame después y sin cambios: **0 px**» (antes: 17 888 en cada uno).

**Rediseño.** La transitoria decía el nombre y nada más. Ahora es una tarjeta: cuerpo con degradado (el bisel de
las cajas de operador de FM6 y de las filas de MENU), raíl de acento en la cabecera, y una cabecera con el motor y
la categoría del sonido (**ANALOG KEYS**), su nivel medido como barra (el mismo número que las filas del navegador)
y su sitio en la lista (**6/291**). Debajo, las cinco filas de siempre —dos arriba, la elegida encendida y con su
estrella si es favorita, dos abajo— en 13 px de paso. Misma caja, mismo sitio, un solo envío.

**Regresión.** `ui_flicker_test` tiene un cuarto caso, `browse_pop`: el navegador con la tarjeta arriba en **cada**
frame mientras la perilla gira ocho pasos, medido en los dos sentidos que el instrumento sabe (dentro del frame:
px escritos dos veces; entre frames: un elemento borrado y vuelto en un frame). 0, 0 y 0. Y `--regions` caso A
comprueba ahora, además, que un segundo frame con la tarjeta arriba y nada movido escribe **0 px**: el parpadeo no
puede volver sin que una prueba lo diga. Nueva escena `presets_pop` en el barrido (196 pantallas × 22 paletas).

**Repaso de la UI.** Barrido completo: **196 pantallas × 22 paletas**, 0 de lint, 0 tintas sobre su propio color,
0 medidas de alineación fuera de 1 px (76 sitios declarados), 0 textos cortados nuevos; **3 136 tomas de píxeles**
idénticas a la base (actualizada sólo por la escena nueva). `--idle`: 29 952 px escritos y 0 cambiados, el mismo
piso que v1.8.6 — la tarjeta no añade gasto en reposo. `ui_flicker_test`: las cuatro escenas, 0 hallazgos.

**Lo que se revisó y no se tocó.** Los defectos de las fases anteriores siguen cubiertos por sus pruebas y no se
reabrieron: vúmetro de FX (`tests/fx_meter_test.c`, 3 comprobaciones), solapes de HOME 4/5 (`--regions`), nombres
de preset sobre el MIXER (`--regions` caso A), parpadeo del navegador (`ui_flicker_test` caso `browse`), MORPH y
escenas (`tests/morph_test.c`, escenas `S_SCENES`).

**Numeración.** Progresiva por parche (1.8.6 → 1.8.7), `docs/VERSIONING.md`.

**Límite declarado.** Nada medido en la FM-1 física: son las mismas fuentes en el host x86-64. Los «Issues» de
GitHub no son consultables desde este entorno (sin navegador ni credenciales): el estado de los defectos
reportados se comprobó en el código y en las pruebas que los guardan, no en el rastreador.

## v1.8.6 — la auditoría de cierre: presets, parpadeo, reposo y texto

Fase extra final, sin funciones nuevas: mirar todo lo que las tres fases tocaron y decir lo que se midió.

**Presets.** La biblioteca entera, leída y sonada: **293 presets, 0 en silencio, 0 pasados de nivel, 0
duplicados exactos, 0 nombres repetidos, 0 fuera de rango** (`tests/preset_audit_test.c`, que los pasa por el
motor de verdad). El censo (`tools/preset_audit.py`): PADS 68 · KEYS 58 · TEXTURES 51 · DRUMS 24 · BASS 21 ·
BELLS 21 · ORGANS 18 · LEADS 13 · PLUCKS 12 · ARPS 7; **291 de 293 llevan movimiento** en sus `x[]` y ninguno
se queda sin envíos. Ningún par cae por debajo de la distancia 0,035 (el más cercano es 808 SUB / TRAP 808 a
0,045) y ningún nombre se repite. No se reescribió ningún preset: la identidad de los clásicos es el activo, y
la auditoría no encontró nada que justificara tocarla. Lo que sí se comprueba es que las funciones nuevas no
rompen la carga: `upreset_test`, `preset_slots_test`, `preset_library_test`, `preset_change_order_test` y
`preset_categories_test` en verde.

**Parpadeo y reposo.** `ui_flicker_test` sobre las escenas que más se mueven, 8 pasos de encoder cada una:
**0 px borrados y reescritos, 0 px de banda perdidos, 0 px escritos dos veces**. Y el cuadro quieto:
`ui_render --idle` da **29 952 px escritos y 0 cambiados** en las escenas que no se mueven (tope 33 000: el
piso medido más un 10 %), el mismo número que antes de las tres fases.

**El barrido.** 195 pantallas × 22 paletas: **0 hallazgos de lint, 0 tintas sobre su propio color, 0 medidas
fuera de 1 px**, 3 120 tomas de `ui_pixels` idénticas a su base.

**Limpieza y texto.** La página SCENES separa su pie del borde de la placa; el manual en inglés vuelve a
envolver la línea de controladores que la fase 3 había estirado. `git status` limpio: no se generó ningún
archivo suelto que quedara en el árbol.

**La numeración, ahora progresiva.** De aquí en adelante una fase es **un parche** (1.8.N → 1.8.N+1) y el
minor sólo se mueve al cerrar una serie (`docs/VERSIONING.md`). **No hubo retroceso**: 1.8.3, 1.8.4 y 1.8.5
siguen publicadas, etiquetadas y en la lista del sitio; bajar la línea a 1.7.4 habría sido exactamente el
downgrade que se pidió evitar.

## v1.8.5 — QUANTUM MORPH: un extremo de otro track, y el controlador que lo mueve

**MORPH SCENES ya existía** (LFO > SCENES: cuatro estados del sonido de la pista, una pareja A/B y la posición
continua entre ellos, con slew de 3/127 por cuadro). Lo que faltaba era lo que la propia auditoría del proyecto tenía
abierto: *"MORPH con estados A/B persistentes — el formato de preset no tiene dos estados"*, y mover la posición
desde un controlador en vez de sólo la perilla.

**Un extremo puede ser otra pista (A < T1..T6 / B < T1..T6).** KNOB 4 de la página recorre ahora dieciséis acciones:
las dos capturas del buffer de edición, DIFF y doce `A < Tn` / `B < Tn` que copian *el sonido completo de otra pista*
—sus `p[]` y sus cuatro slots de matriz— dentro de la escena. La escena es una copia, no una referencia: editar esa
pista después no la cambia. Y como el proyecto ya guarda las seis pistas, una pareja hecha así **sobrevive a guardar
y a apagar** sin un byte de almacenamiento nuevo (la imagen crece 1 116 B; el registro de proyecto, que está lleno
hasta el último byte de su objeto de flash, no se toca).

**CC 78 mueve la posición, CC 79 y CC 80 capturan.** El controlador entra por la misma variable que la perilla, así
que la posición sigue pasando por el slew: entre dos bloques de audio nunca llega más de un paso de 127, venga de
donde venga. Los valores fuera de 0..127 se recortan, no dan la vuelta. La captura de un CC es la de la pista del
canal que lo manda.

**La página muestra lo que hace.** LFO > SCENES dibuja el raíl de A a B: la parte recorrida rellena, el marcador de
la posición, los números de escena en los extremos (color de tema para A, acento para B; grises cuando ese extremo
está vacío) y una fila por extremo con su número y `STORED` o `KNOB 4: STORE`. El pie dice lo que un par hace y qué
controladores lo mueven. Un extremo vacío no dibuja relleno y el pie avisa: nada de un deslizador que no mueve nada.

**Lo que no cambia.** Ni motores, ni voces, ni efectos, ni la escala de la posición; los tres tests de MORPH siguen
en verde (21 casos audibles, 6 transiciones sin click, la página y su interpolación) más once comprobaciones nuevas
de esta versión: la copia desde otra pista, que no es una referencia, el recorrido 0 → 127 → 0, el slew del
controlador, el recorte en los extremos y las dos capturas por CC.

## v1.8.4 — FX: el vumetro de DIST y la escala que lo tenía clavado

**El vúmetro del FX pegado arriba con la distorsión al 1 %.** No era la distorsión: era la escala. Las cuatro
tarjetas de FX (DIST, CHORUS, DELAY, REVERB) recortaban su nivel a **7000**, una escala que venía de cuando el
envolvente de la pista era un pico de 0..7000; `fx.c` mide desde v1.9 el `|x|` de la muestra de la pista en **Q15
(0..32767)**. Una nota normal mide **17 638** —252 % de 7000—, así que la barra de DIST llegaba al tope con la
distorsión a cero y el dueño lo leyó como "subir la distorsión un 1 % clava el vúmetro". Comprobado con una sonda
sobre el motor (`DIST 0 → vu 17 638`, `DIST 1 → 16 836`, `DIST 64 → 15 441`): el nivel ni sube con la distorsión.

Corregido a **32767**, la misma escala que los medidores del MIXER y el VU de MASTER; el punto de clip pasa de 6800 a
32 700 (el mismo umbral que el VU de MASTER). La conversión vive ahora en `fx_meter_target()` (core.h) para que la
página y `tests/fx_meter_test.c` no puedan volver a separarse: tres comprobaciones nuevas fijan 0, 17 638 (54 %) y
32 767 (tope, y más allá también tope).

**Análisis del motor, medido.** `tests/eng_bench` da el coste por motor con 8 teclas (host x86-64, por bloque de
32 muestras, sobre el silencio de 5,5 µs): ANALOG 2,7 µs · LOFI 3,1 · DRUM 4,1 · FM6 5,9 · PHASE 6,3 · VOICE 6,5 ·
NOISE 6,6 · ADDTV 6,8 · WHEEL 7,4 · WTABLE 7,5. Seis pistas con el motor más caro suman ~50 µs de los 725 del
presupuesto. `tests/fx_bench` (best of 3): todas las vías alimentadas 135,2 ns/muestra, seco 134,1, flanger 157,9,
**reposo con las vías armadas 119,4** y seis pistas con cuatro notas cada una 203,3. El perfil (`gprof`) del bloque
pone `fx_buses` al 28 %, `mix_block` al 16 %, `track_render` al 12 %, `rev_room` al 9 %.

**Lo que no se cambió, y por qué.** Ninguna optimización del DSP se quedó en esta versión: para tocar el camino de
audio la barra es que los 311 renders golden queden idénticos, y los candidatos que se midieron (saltar el bloque
de vías cuando su entrada y su cola son exactamente cero, fusionar los pases de medición de la reverberación) mueven
posiciones de línea o la fase del LFO del chorus, que sí cambian el sonido al volver la señal. Quedan anotados con su
coste en `docs/OPT_DSP_2026-11-23.md` para que el siguiente intento empiece por ahí en vez de repetir el recorrido.

## v1.8.3 — HOME: el MASTER del MIXER y la batería cargando

**MIXER, sección MASTER (HOME 4/5): los porcentajes se leían encima de los vumetros.** Las barras L y R iban de
x 82 a 214 (132 px) y su porcentaje está alineado a la derecha en x 226: con tres cifras y el signo el número empieza
hacia x 202, así que doce píxeles de barra quedaban debajo del texto en cada fila, y la barra de LEVEL (desde x 72)
llegaba a dos. Ahora las tres barras comparten columna (x 82, 112 px: 194 como máximo) y los números conservan su
franja de 196 a 228. Nada se oculta ni se acorta: los vumetros siguen midiendo lo mismo, el porcentaje se lee entero
y LEVEL conserva su fila.

**La batería cargando era un borrón.** Con USB conectado el icono dibujaba tres celdas y les recortaba tres muescas
de fondo para insinuar el rayo: a 20 x 13 las muescas dejaban las celdas como un bloque con mordiscos y el rayo no se
leía. Ahora, con corriente externa, no se dibujan celdas: el cuerpo mantiene su marco y el rayo se dibuja en tinta
— cabeza arriba a la derecha, caída en diagonal, pie abajo a la izquierda —, tres trazos de 3 a 6 px que se leen a
la distancia del panel. Los cuatro estados de nivel no cambian.

**FPS de HOME: perfilado hecho, un candidato probado y descartado por su propio test.** Con `gprof` sobre HOME 1/5
el cuadro costaba ~90-105 µs (host x86-64) y el 36 % de las muestras estaba en `ux_mix`; el 17 % en los cuatro diales
de la bahía HEARTH. Se les dio firma propia para no repintarlos cuando no cambiaban y el dibujo del panel bajó de
102,7 a 78,8 µs (mediana de cinco series) — pero `ui_flicker_test` cazó el fallo de fondo: `draw_graph` empieza cada
panel con un `cv_begin` que **limpia el lienzo**, así que un widget que se salta su dibujo deja un agujero de 375 px
en la bahía. El cambio se revirtió entero y no hay ganancia de FPS que declarar en esta versión. Se deja escrito en
el código y en `docs/OPT_HOME_2026-11-23.md` para que el siguiente intento no repita el camino: para cachear dentro
del panel hace falta un lienzo persistente, no una firma.

## v1.8.2 — Cierre de ciclo: lo que la web decía y el firmware ya no

**Números públicos contra números medidos.** `tests/preset_library_test.py` cuenta **291 sonidos de fábrica** en
diez motores; la portada decía 283, el `fmhub.json` 287 y el README anunciaba `v1.7.0`. Los tres se corrigen a lo
que el código tiene (291 sonidos, versión 1.8.2). Ninguno de esos números se tocó de la tabla de presets: lo que
estaba mal era la copia.

**Un pendiente que ya estaba resuelto.** La desincronización del punto de ENV con el ADSR dibujado se cerró en
v1.12: el punto se coloca con `env_x_of`, que recorre la misma recurrencia que la curva muestra a muestra en vez de
invertir una recta. La matriz lo daba por abierto; ahora dice dónde y cómo se cerró.

**Auditoría de seguridad (sólo lectura, nada debilitado).** `sign_test` (firma válida, alterada, clave desconocida,
downgrade), `key_custody_test` (la clave nunca se sobreescribe, PKCS#8), `install_test` con sus compuertas,
`preflight_test` sobre el paquete real y `test_pkg_bounds`: todos en verde. No hay claves, tokens ni `.env` en el
repositorio; el único `BEGIN PRIVATE KEY` del árbol es una aserción de test sobre una clave que él mismo genera.

**Limpieza.** Sin artefactos de build, `.o`, `.pyc`, `.log` ni respaldos versionados; sin `TODO`/`FIXME` en el
firmware; `build/` y los worktrees `wt/` no están en el repo. Se conservan las auditorías históricas de `docs/`
como registro, marcando cuál es el estado actual.

