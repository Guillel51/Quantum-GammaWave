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

