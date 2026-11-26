# FASE 2/6 — Código, flash y recursos: la caché de ventanas entra en el presupuesto

Clases: **[AC791N]** build estático con el toolchain pi32v2 y el SDK; **[HOST]** barrido de UI y pruebas en x86-64.
Ninguna cifra es una medición en una FM-1 física: no se flasheó ni se tocó la unidad.

## 0. Los pendientes de la fase 1

| pendiente | estado |
| --- | --- |
| los tres cambios de la fase 1 (`fx_trip`, `audio_out_peak`, `div_samples`) | **en `main`**, verificados aquí con `git log` y el código (commit `7d767b16`) |
| la base de píxeles grabada con `git describe` | **resuelto** en la fase 1: se regeneró con la versión de `felucca.c`, la misma que compila `run_tests.sh` |
| `cloud_hot_test`, criterio del sostenimiento de CLOUD | **explicado, no relajado**: a `SIZE 127 / RATE 127` la cola mide 80,8 dB contra un inicio de 86,9 y el criterio pide "dentro de 6 dB" (80,9). Falla por 0,1 dB. Arreglarlo es tocar el algoritmo de CLOUD (calidad de audio) o subir el umbral; ninguna de las dos cosas es una optimización de recursos y las dos piden una medición de audio dedicada |
| `text_spacing_test.py`, par `Gl` del tamaño S | **explicado, no relajado**: −0,258 px contra un límite de 0,25. Es la métrica del glifo `G` de la fuente S; corregirla mueve píxeles de texto y obliga a regenerar la base de 3 305 cuadros y la puerta de alineación. No es un defecto de rendimiento ni de recursos |
| CPU del host no medible aquí | **explicado**: sin contador de kernel y sin valgrind; el reloj del proceso tiene 10 ms de grano. La evidencia de CPU del proyecto es el código generado para el AC791N y la identidad de los renders |

## 1. El tamaño, medido [AC791N]

| región | tamaño | usado | libre | uso |
| --- | --- | --- | --- | --- |
| XIP (imagen) | 583 612 | **582 136** | **1 476** | 99,7 % |
| — código | | 455 680 | | |
| — constantes | | 121 676 | | |
| POOL | 327 680 | 303 328 | 24 352 | 92,6 % |
| RAM (.data+.bss) | 98 304 | 75 632 | 22 672 | 76,9 % |
| LOWBUF | 32 768 | 28 488 | 4 280 | 86,9 % |
| NOINIT | 15 696 | 15 620 | **76** | 99,5 % |
| RAMCODE | 20 992 | 3 940 | 17 052 | 18,8 % |

Lo que hay dentro de la parte de sólo lectura, con `tools/xip_audit.py` (`docs/XIP_AUDIT.md`, regenerado en esta
fase):

| categoría | bytes | nota |
| --- | --- | --- |
| cadenas y varios ("other") | 61 908 | `.rodata.str1.1` son 25 562 B: nombres de preset, páginas, mensajes |
| curvas | 25 191 | tablas generadas (SINE, FM6 exp2 en RAM, `AI12`, `UAC_FIR`) |
| fuentes | 20 660 | S 12 px (7 293) + M 15 px (5 906) + L 18 px (4 270) + tablas |
| presets | 13 564 | 296 sonidos de fábrica en los diez motores |
| tablas de salto | 328 | |
| relleno / sin nombre | **0** | "DEAD sections: none. Every read-only byte is pointed at from the image." |

Las fuentes se regeneran en cada build (`tools/gen_aa_font.py`: S 95 glifos × 4 fases, M 95 × 2, L 41 × 2, con su
tabla Huffman y sus pares de kerning) y los presets son datos, no código: no hay ningún bloque de XIP que no esté
referenciado. **No queda relleno de alineación ni código muerto**: el barrido de funciones `static` sin ninguna
referencia da 0, y `xip_audit` no encuentra secciones huérfanas.

## 2. Lo que se enciende en esta fase: la caché de ventanas angostas

`gfx.c` tenía implementada desde la Fase 6 la caché de ventanas (`FELUCCA_WINCACHE`), **medida y apagada**: con la
bandera encendida el arnés medía 21 440 px por cuadro quieto contra 29 952, con todos los cuadros idénticos, pero
el enlace se pasaba de XIP por 2 752 B en aquel árbol.

Ahora entra. El árbol tiene el hueco, y el cambio queda así:

- **`gfx.c`**: `FELUCCA_WINCACHE` pasa de 0 a **1**. Con la bandera, la caché guarda el hash FNV-1a de las últimas
  cuatro ventanas angostas enviadas (las columnas de EDIT, las tiras del MIXER, los módulos de FX > BUSES, que la
  tabla de filas de 240 no cubre) y una ventana que se redibuja byte a byte no se manda. Una que cambió se manda
  entera. Un relleno, otro canvas, una banda ancha o el panel dormido la invalidan (`gfx_forget`).
- **`firmware/app.ld`**: RAMTEXT sube otros 2,5 KiB (`0x01C70C00` → `0x01C71600`) y su longitud baja lo mismo
  (`0x5C00` → `0x5200`). La bandera cuesta **3 288 B de XIP** y con ella el alcance de llamadas desde el código en
  RAM caía a 5 856 B de los 8 192 exigidos; subir la ventana lo devuelve a **8 416 B**. La ventana sigue
  conteniendo el código en RAM (3 940 B) y la cola fría `.ramdata` (16 656 B) en 20 992 B, y la banda de guarda no
  se movió: sigue empezando en `0x01C76800`.
- Coste en `.noinit`: 48 B (cuatro ventanas de 12 B). NOINIT queda con 76 B libres y `memory_map_test` pasa.

Medición [HOST], misma herramienta y mismo árbol menos la bandera:

| | px escritos por cuadro quieto | px cambiados | cuadros distintos de la base |
| --- | --- | --- | --- |
| `FELUCCA_WINCACHE=0` | 29 952 | 0 | 0 de 3 305 |
| `FELUCCA_WINCACHE=1` (ahora) | **21 440** | 0 | 0 de 3 305 |

**−28,4 % de píxeles por cuadro quieto** sin un solo píxel distinto: es tráfico de SPI y tiempo de panel que la
pantalla no necesita. El tope del arnés (`--idle`) es 33 000.

## 3. Qué se revisó y **no** se tocó, y por qué

- **Compresión de cadenas (`docs/STR_PACK.md`)**: descartada antes con medida. No se reintenta.
- **Duraciones por paso (`docs/FASE4_DURACION_EXPRESION_2026-11-26.md`)**: agotaba LOWBUF. No se reintenta.
- **Compresión de presets, `UAC_FIR` y descriptores (`docs/XIP_AUDIT_PRESETS_2026-11-26.md`)**: medido antes, no
  da ahorro sin pérdida. No se reintenta.
- **Funciones matemáticas del DSP**: las tablas (`SINE`, `WT_GEN`, `WT_COEF`, `fm6_exp2_tab`) ya están en RAM
  para no pagar una lectura de XIP por muestra, y el coste por función está en `docs/FASE1_PERF_MEM_2026-11-26.md`
  §4 sin divides por muestra. Cambiar la aritmética mueve los 316 renders dorados: no es una optimización de
  recursos.
- **Carga de presets e inicialización de motores**: `apply_preset_to` (ui.c) ya apaga las IRQ sólo para el flush
  corto, ya deja en paz los buses y la capa de performance de una pista silenciosa (issue #106) y no reconstruye
  ninguna tabla. No hay trabajo repetido que quitar sin cambiar el sonido.
- **Estructuras sobredimensionadas**: `eng_arena` y `slot_arena` son uniones por pista/ranura (no copias), y
  `vpool` guarda las 18 ranuras del pool real. No hay dos buffers con el mismo contenido.

## 4. WASM: bloqueado, no medido

No hay `emcc` en este entorno (`which emcc` vacío) y el repositorio no guarda un `.wasm` construido (sólo
`web/emu/worklet.js`, 6 973 B). **No se puede comparar el tamaño del WASM aquí**: hace falta Emscripten v6.0.11
(`sh web/emu/build.sh`). Se reporta como bloqueado; no se inventa una cifra.

## 5. Verificación

| prueba | resultado |
| --- | --- |
| `./build.sh` [AC791N] | imagen 582 136 B de 583 612 (1 476 libres); los cinco controles pasan (RAM, pool, alcance 8 416 B, registros, XIP) |
| barrido de UI, 207 pantallas × 22 paletas | **3 305 cuadros idénticos** a la base; **0 hallazgos de lint** con los recursos regenerados en este árbol (un build con los encabezados generados de la fase anterior reportaba 176; los cuadros son los mismos en los dos casos); **0 desalineaciones** en 4 069 937 mediciones |
| `ui_render --idle` | 21 440 px (tope 33 000) |
| `ui_render --regions` / `--bands` / `--headers` / `--announce` | pasan |
| `ui_test`, `ui_flicker_test`, `ui_nav_test`, `theme_test` | pasan |
| `tests/regress` [HOST] | **316 renders dorados idénticos**, 0 fallos de salud, 0 de voces (el audio no se toca) |
| `preset_library_test`, `preset_slots_test`, `preset_categories_test`, `preset_audit_test` | pasan: 296/296 categorías, 295 en sus ranuras, 298 presets sin silencios ni duplicados |
| `install_test.py`, `web/test_web.mjs`, `web/test_backup.mjs`, `web/test_pkg_bounds.mjs` | pasan |
| `panel_table_test.py` | pasa (tabla de la ST7789 intacta) |
| `memory_map_test.py` (con el toolchain) | pasa: RAM libre 22 672, pool 24 352, RAMTEXT 17 052, NOINIT 76 |
| `xip_audit.py --md docs/XIP_AUDIT.md` | regenerado; 0 secciones muertas |
| `target_budget.py` | todas las líneas del audio dentro de presupuesto (sin cambios de audio) |

## 6. El precio, dicho claro

XIP queda **al 99,7 %** (1 476 B libres) y NOINIT al 99,5 % (76 B). La caché se compró con ese hueco porque el
beneficio es medible y sin riesgo visual. Cualquier trabajo de las fases 3–6 que agregue código tiene que liberar
espacio antes; las tres fuentes evidentes —cadenas, fuentes, presets— ya se midieron y se descartaron en fases
anteriores con el número delante.
