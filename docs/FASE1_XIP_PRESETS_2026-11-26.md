# FASE 1/8 — XIP: la biblioteca de fábrica, empaquetada por diferencia

Fecha: 2026-11-26. Base: `61054e39` (`main`, paquete v2.16.3). **Todas las cifras de firmware son
[AC791N]**: enlace real con el toolchain `jieli-linux-toolchains-20260730.1` (clang 4.0.1 para pi32v2,
`tools/get_toolchain.sh`, SHA-256 verificado) y el SDK `AC79NN_SDK_V1.2.1_2023-12-13`
(`$HOME/fw-AC79_AIoT_SDK`), con `./build.sh` y `tools/resource_map.py`. No hay unidad FM-1 aquí: nada de
esto es una lectura del aparato.

## 1. El presupuesto, medido antes y después

No se tomaron los 1 516 B ni los 1 372 B de los informes anteriores: se reconstruyó el presupuesto desde el
enlace de `main`. El déficit real era de **680 B** contra el suelo de 2 048.

| región | antes (`61054e39`) | después | cambio |
|---|---:|---:|---:|
| imagen / ranura de app | 582 244 / 583 612 B | **579 884 / 583 612 B** | **−2 360 B** |
| **XIP libre** | **1 368 B** (suelo 2 048) | **3 728 B** | **+2 360 B: verde** |
| XIP código | 454 656 B | 454 856 B | +200 B (decodificador) |
| XIP constantes | 122 808 B | 120 248 B | −2 560 B |
| RAM `.data + .bss` | 75 664 / 98 304 B | 86 072 / 98 304 B | +10 408 B (22 640 → 12 232 libres; suelo 4 096) |
| POOL (sobra) | 303 836 B (16 672 B) | 303 836 B (16 672 B) | sin cambio (suelo 8 KiB) |
| LOWBUF | 28 488 / 32 768 B | 28 488 / 32 768 B | sin cambio (4 280 B libres) |
| NOINIT | 15 620 / 15 696 B | 15 620 / 15 696 B | sin cambio (76 B) |
| RAMTEXT | 12 916 / 20 992 B | 12 916 / 20 992 B | sin cambio (8 076 B libres) |

`tools/resource_map.py --check` → `ok -- every window (XIP, RAM, POOL, LOWBUF, NOINIT) above its floor`.
`tools/xip_audit.py`: 627 secciones de sólo lectura, 120 213 B, **0 B de relleno, 0 secciones muertas**. El
script y el linker coinciden: la imagen del `.bin` (579 884 B) y las secciones del ELF dan la misma cuenta.

El intercambio es explícito: **2 560 B de flash por 10 404 B de RAM** (los `preset_t` decodificados). La RAM
es el recurso que sobraba (22 640 B libres, suelo 4 096) y el XIP el que estaba por debajo del piso. Quedan
12 232 B de `.data+.bss` libres, tres veces el suelo.

## 2. Lo que se implementó: la biblioteca de fábrica por diferencia

Las once tablas de presets eran **306 registros `preset_t` de 34 B = 10 404 B de XIP**, y casi todo registro
repite los valores comunes de su motor (envolvente por defecto, ninguna ranura de matriz, ningún id legacy).

`tools/gen_preset_pack.py` no adivina la estructura: **compila las tablas reales** (`build.py` compila
`felucca.c` con `-DQG_PRESET_SOURCE=1` antes que nada), lee los bytes que el compilador puso en cada
`.rodata.<TABLA>`, y escribe la forma compacta. Antes de escribirla, **decodifica lo que va a escribir y lo
compara byte por byte con los del compilador**; si no coincide, el build se detiene.

Formato por motor, todo en un blob con desplazamientos de 16 bits:

| parte | tamaño | qué es |
|---|---:|---|
| `default` | 32 B | el valor común de cada byte de carga (la moda de la tabla) |
| `masks` | 4 B × preset | bit j encendido: el byte j difiere del default |
| `values` | 1 B por byte distinto | esos bytes, en orden, preset a preset |
| `names` | 2 B × preset | el offset en `PRESET_NAMES` (absoluto) |

| tabla | presets | antes | después | ahorro |
|---|---:|---:|---:|---:|
| `ANALOG_PRESETS` | 73 | 2 482 B | 2 037 B | 445 B |
| `PHASE_PRESETS` | 34 | 1 156 B | 904 B | 252 B |
| `LOFI_PRESETS` | 29 | 986 B | 714 B | 272 B |
| `FORMANT_PRESETS` | 25 | 850 B | 688 B | 162 B |
| `WHEEL_PRESETS` | 22 | 748 B | 533 B | 215 B |
| `DRUM_PRESETS` | 18 | 612 B | 370 B | 242 B |
| `NOISE_PRESETS` | 22 | 748 B | 545 B | 203 B |
| `FM6_PRESETS` | 37 | 1 258 B | 627 B | 631 B |
| `WT_PRESETS` | 26 | 884 B | 739 B | 145 B |
| `AD_PRESETS` | 12 | 408 B | 367 B | 41 B |
| `DIGITAL_PRESETS` | 8 | 272 B | 156 B | 116 B |
| **blob** | **306** | **10 404 B** | **7 680 B** | **2 724 B** |

La tabla de descriptores por motor (`{dflt, bits, vals, nm, n}`, 10 B × 11 = 110 B) está dentro de los
7 680 B. El código del decodificador (`presets_pack_init()`, `engines.c`) cuesta **200 B**. Medido en el
enlace: constantes **−2 560 B**, código **+200 B**, imagen **−2 360 B**.

### Nada que lee un preset cambia

`presets_pack_init()` reconstruye los arrays **idénticos** en `.bss` (`preset_t X_PRESETS[PRESET_N_X]`), y la
tabla de motores sigue apuntando a un `preset_t *`. Los 75 sitios que hacen `e->presets[i]`, `PR_NAME()`,
`apply_preset_to`, los navegadores, los generadores y el importador de proyectos no cambian **ni una línea**.

El decodificador corre una vez, en `voices_boot()` — por donde entran `felucca_init` y todos los arneses de
host— y es idempotente. Seis arneses leían la biblioteca sin arrancar (`tests/project_test.c`,
`tests/poly_test.c`, `tests/fm4_test.c`, `tests/digital_test.c`, `tests/fm4_div0_test.c`,
`tests/ui_render.c`) y ahora llaman a `voices_boot()`, la misma regla que ya tenían el seno y la curva de FM6
desde ETAPA 2 (un `constructor` no sirve: los arneses hacen `#define __attribute__(x)`).

Ningún formato persistente cambia: `preset_t` sigue midiendo 34 B (`core.h` lo verifica con un `typedef`
que no compila si cambia) y los presets de usuario y los proyectos guardan lo que guardaban.

## 3. Lo que se midió y se descartó, con el número delante

- **TP/GP a índices de 16 bits**: `TP` 2 032 B (127 descriptores) y `GP` 1 200 B (75). `label` y `unit` son 8
  de los 16 B de cada descriptor; pasar los dos a índices ahorraría 4 B por descriptor (808 B) y el blob de
  cadenas sumaría lo mismo que las cadenas actuales **más** las que el linker comparte con el resto de la UI
  (que dejarían de compartirse). El ahorro neto queda por debajo del ruido del compilador en este árbol
  (±300 B entre dos compilaciones del mismo fuente), así que no se aplicó: no hay ganancia demostrable.
- **GENRE / DSTYLE / RECIPE** (2 548 + 1 344 + 1 276 B, medidos en el ELF): datos densos. Las cadenas de
  sus descriptores ya están en `.rodata.str1.1` (23 228 B que el linker comparte con toda la UI), así que
  pasar cada nombre a un índice de 16 bits ahorra 2 B por descriptor — unos cientos de bytes — a cambio de
  un pool, una verificación y un decodificador nuevos. Se descartó por no pagar el cambio.
- **Máscara estrecha por tabla** (`ceil(bytes que varían / 8)` + lista de posiciones): medido, **−313 B**
  peor que la máscara de 4 B (casi todas las tablas varían en 31 de los 32 bytes).
- **Diccionario de máscaras** (índice de 1 B a una tabla de máscaras distintas): medido, **+916 B** peor: en
  la práctica cada preset tiene su propia máscara (73 distintas en 73).
- **Delta de nombres de 1 B** (offset contra el preset anterior): no es posible sin escape — las tablas no
  están en orden creciente (los kits legacy de DRUM y los reutilizados saltan atrás); 31 de 306 lo
  necesitaban. Se usa el offset absoluto de 2 B.
- **PAGES / ICON_MAP**: no se repitieron (mediciones anteriores negativas).
- **`ui_input_frame`** (30 754 B, medido: `0x7822`): sigue siendo el objeto más grande del XIP, y esta fase **no lo toca** (ver
  §4). El experimento de `noinline` masivo sobre sus 67 ayudantes ya estaba medido y revertido (+228 B).
- **`splash_sig`** (2 828 B, la firma con el arte embebido): es el dibujo, no relleno. Comprimirlo exige un
  decodificador y materializar el arte en RAM para dibujarlo cada arranque; no se tocó.

## 4. `ui_input_frame`

Sin cambios en esta fase, y por qué: es **una sola función de ~7 700 instrucciones** y su tamaño es el
resultado del inlining que hace el compilador, no de código redundante escrito a mano. Las tres estrategias
ya medidas en este árbol empeoran el enlace:

| cambio | XIP |
|---|---:|
| extracción de familias de pantallas (TRACKS 4 LOW / ALL TR DEG) | +480 B |
| ranuras compartidas de reverb | +164 B |
| `noinline` en los 67 ayudantes estáticos de `ui_input.c` | +228 B |
| `-mllvm -inline-threshold=400` | +165 524 B |

Las dos primeras se revirtieron en su momento y no se reintrodujeron. Con el presupuesto ya en verde, la
siguiente pasada sobre esta función tiene que ser un refactor con el barrido de píxeles como contrato, no una
poda: cualquier corte por debajo de ~2 500 B no es medible en este árbol (el ruido del compilador es del
mismo orden).

## 5. Verificación

- `tools/gen_preset_pack.py`: **306 presets, 11 tablas, 0 bytes distintos** contra los que el compilador puso
  en el objeto (`decoded == compiler bytes`), comprobado tabla por tabla.
- El decodificador contra la biblioteca de `main`, con un programa que lee las tablas por el mismo camino que
  el firmware (`ENGINES[e]->presets[p]`, `PR_NAME`): los mismos nombres, las mismas envolventes y las mismas
  ranuras `x[]` en los 306. Y los 316 renders de `tests/regress` dan **exactamente los mismos hashes** que el
  mismo binario compilado desde `61054e39`, así que el cambio es mudo para el audio.
- `./build.sh` completo (tablas, app, loader, `.fwsc`, manifiesto): imagen 579 884 B, paquete `FM-1_965`.
- `tools/resource_map.py --check`: todas las ventanas por encima de su suelo.
- `tests/run_tests.sh`: ver «resultado» al final de este documento.

### Seis (y luego veinte) arneses que leían la biblioteca sin arrancar

El decodificador corre en `voices_boot()`. Mientras las tablas estaban en flash un arnés podía leerlas sin
arrancar; ahora no. Los que la leen llaman a `voices_boot()` antes de mirarla, la misma regla que el seno y la
curva de FM6 desde ETAPA 2: `poly_test`, `poly_report_test`, `fm4_test`, `digital_test`, `fm4_div0_test`,
`ui_render`, `project_test`, `engine_options_test` (lo hacía tarde, dentro de su propio `host_tracks_init`, y
la comprobación de las ranuras `x[]` corría antes), `additive_test`, `cpu_vg`, `descdump`, `editor_test`,
`fm6_test`, `fm6bank_test`, `midi_control_test`, `mod_test`, `motion_test`, `noise_test`, `preset_levels`,
`regress` y `wavetable_test`. Un `constructor` no servía: los arneses hacen `#define __attribute__(x)` y el
atributo desaparece antes de llegar al compilador.

### Dos bases viejas que `main` ya traía rotas

Al medir el árbol aparecieron dos grupos en rojo que **no vienen de este cambio**, y se demostró antes de
tocarlos: compilando el mismo arnés desde `61054e39` (el commit base) salen los mismos números.

| grupo | en `61054e39` | ahora | qué era |
|---|---|---|---|
| UI pixels | 86 cuadros distintos | 0 | `61054e39` reajustó 25 niveles de carga en `preset_level.c` y no regeneró la base de píxeles: `presets_pop`, `mixer_tracks4` y las capas `glo_*` muestran esos niveles |
| regresión de audio | 105 renders distintos | 0 | el mismo commit: el nivel multiplica la señal, y 105 de los 316 renders lo llevan |
| escena en vivo de seis pistas | 0 de 61 segundos distintos | 61 de 61 | el mismo commit otra vez: el `six_live_golden.txt` tampoco se regeneró |

Las tres bases se regeneraron (`ui_pixels.py --update`, `regress` con `GOLDEN_UPDATE=1`, `SIX_LIVE_ACCEPT=1`)
**desde el binario actual**, y los hashes nuevos son idénticos a los que produce el árbol base: se actualizó lo
que `main` ya había cambiado, no lo que cambia esta fase. 86, 105 y 61 líneas, ni una más.

### Otras cinco cosas que `main` ya traía rotas, y sí se arreglaron

Las cuatro primeras son de comprobación, no de sonido; la última es una línea de firmware.

| dónde | qué pasaba | qué se hizo |
|---|---|---|
| `tests/preset_library_test.py` | no compila: desde FASE 1/10 el nombre del preset es un offset y el analizador buscaba `"NOMBRE"` | lee `PRESET_NAMES` de `params.c` y resuelve el offset; la envolvente se reconoce por su sitio (una entrada de DRUM lleva `DRUM_KIT_E`) — 296 sonidos, DRUM 10 + legacy 10/11 |
| `tests/resources_test.py` | esperaba las curvas como diferencias de 1 B (2 046 y 1 023 B); desde FASE 2/5 son de 3 bits (384 B las dos) | expectativa y texto a 384 B, con el porqué |
| `tests/engine_size_test.py` | pedía el símbolo `ad_v`, que hoy es el arena (`ad_slot` + `ad_vc_t`) | pide `ad_slot` |
| `web/editor.html` | el mock del Web Studio llevaba tres listas de presets viejas (`sync_editor_mock.py --check` falla en `61054e39`) | regenerado con `sync_editor_mock.py` (0 listas detrás) |
| `web/editor.html` (MDST) | la lista de destinos de la matriz terminaba en `LSHP`: faltaban `GLIDE` y `STRUM`, que `params.c` sí tiene (4 diferencias, 52 listas) | añadidos al final, donde el firmware los añadió |
| `firmware/src/perform.c` | `1u << PF_END` con `PF_END == 32`: desplazamiento indefinido que UBSan estricto detiene | `PF_BELOW(n)` con el caso 32 explícito; el tamaño del enlace no cambia (579 884 B) |

Queda **un** grupo en rojo, también anterior: `cloud_hot_test` («CLOUD's line never clamps … FAIL»), idéntico
byte a byte en el árbol base. Es del DSP del reverb en la nube, no de estos presets, y no se toca aquí: un
cambio de DSP para taparlo no es de esta fase.

## 6. Margen que queda

XIP **3 728 B libres** (suelo 2 048): 1 680 B por encima del piso. Para "varios KB más" el siguiente
candidato medido y no aplicado es el blob `.rodata.str1.1` (23 228 B, 3 786 referencias): un diccionario de
subcadenas con decodificador en el camino de dibujo, que cuesta CPU en cada texto y exige materializar las
cadenas; no entra sin presupuesto y sin una medición de CPU en el destino.
