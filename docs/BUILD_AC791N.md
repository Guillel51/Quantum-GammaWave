# Building for the AC791N (and the emulator) from a clean machine

Everything here was run on a Linux x86_64 host with **no vendor account** and **no Docker**. The
steps are the same ones that produced `build/felucca.fwsc` at `07b5d94`:

| resource | value measured in that build |
|---|---:|
| image | 561 732 B |
| `.data + .bss` | 95 480 / 98 304 B |
| pool spare | 9 248 B |
| RAM code | 22 484 / 24 576 B |
| loader `ota.bin` | 8 007 B |
| package `.fwsc` | 611 163 B, identity `FM-1_982` |

`build/` is not tracked by git: the package is reproducible, not committed. Anything that checks
`build/` (for example `tests/emu_sync.py`) only passes on a machine that has just built here.

## 1. Firmware toolchain (pi32v2)

```sh
tools/get_toolchain.sh            # downloads JieLi's Linux toolchain into ~/.jieli/toolchain
export JIELI_TOOLCHAIN="$HOME/.jieli/toolchain"
```

`get_toolchain.sh` fetches `https://pkgman.jieliapp.com/s/linux-toolchain` and refuses to unpack
anything whose SHA-256 is not the one pinned in `tools/toolchain.sha256`. That URL serves the latest
build, so when JieLi replaces it upstream the script fails loudly, naming both hashes, instead of
building with a compiler nobody reviewed. The archive used here is `jieli-linux-toolchains-20260730.1`,
whose `pi32v2/bin/clang` reports **clang 4.0.1**.

`build.sh` requires `$JIELI_TOOLCHAIN/pi32v2/bin/clang` to be executable, otherwise it stops with
`no toolchain in …`. Mind the directory level: unpacking the archive one level deeper is the most
common mistake.

Toolchain **and** the three SDK blobs of §6 in one step, hashes verified and nothing written that does
not match them:

```sh
python3 tools/get_ac791n.py            # ~/.jieli/toolchain + ~/fw-AC79_AIoT_SDK, then the two exports
python3 tools/get_ac791n.py --check    # what is installed, what is missing; writes nothing
```

## 2. AC79 SDK

```sh
git clone --depth 1 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$HOME/fw-AC79_AIoT_SDK"
export AC79_SDK="$HOME/fw-AC79_AIoT_SDK"
```

About 1.1 GB. `build.sh` checks for `$AC79_SDK/cpu/wl82/tools/uboot.boot` and stops with
`no JieLi AC79 SDK in …` if it is missing. Neither the SDK nor the toolchain is vendored here.

Defaults if the variables are unset: `~/.jieli/toolchain` and `~/fw-AC79_AIoT_SDK`.

## 3. Build

```sh
ulimit -n 9000        # the link opens many objects at once; "Too many open files" is the usual failure
./build.sh            # tables -> firmware -> loader -> .fwsc -> manifest
```

Expected tail: `ok image …`, `ok RAM .data+.bss … of …`, `ok pool … spare …`,
`package build/felucca.fwsc … identity FM-1_982`, `manifest: v… at <commit>; 3 artefact(s) hashed`.

## 4. Emulator (WASM)

```sh
git clone --depth 1 https://github.com/emscripten-core/emsdk.git /tmp/emsdk
/tmp/emsdk/emsdk install 6.0.11 && /tmp/emsdk/emsdk activate 6.0.11
. /tmp/emsdk/emsdk_env.sh
sh web/emu/build.sh   # writes build/emu/{felucca.wasm,index.html,manifest.json}
```

`web/emu/build.sh` resolves `emcc` from `PATH`; there is no wrapper to install. The build at `07b5d94`
produced **1 050 545 B**, `sha256 169a4a67…`, and the same bytes at the previous commit because only a
test file changed — that is the reproducibility the emulator check relies on.

## 5. Did it work?

```sh
python3 tools/check_build_env.py     # what is installed, what is missing, and the versions found
python3 tools/engine_size.py --map   # XIP and .data+.bss per engine, against the build's own map
python3 tests/emu_sync.py            # page, manifest and WASM bytes agree (needs a fresh emu build)
python3 tests/manifest_test.py       # the manifest names this commit and its package
```

`check_build_env.py` never fails a host that lacks the tools: it reports and points at the step above,
so CI on a machine without the SDK stays honest instead of red.

## 6. El SDK sin descargar 1,1 GB: sólo hacen falta tres ficheros

`tools/build.py` no compila ni enlaza contra el SDK. Del SDK sólo usa **tres blobs** del SPL, y los
fija por SHA-256 (`SDK_SHA256`, versión probada `AC79NN_SDK_V1.2.1_2023-12-13`):

| ruta en el SDK | sha256 (fijado en `build.py`) | tamaño |
|---|---|---:|
| `cpu/wl82/tools/uboot.boot` | `4e3b4c22…76ded4` | 14 536 B |
| `cpu/wl82/tools/cfg_tool.bin` | `27657995…010d1b` | 383 B |
| `cpu/wl82/tools/cfg/eq_cfg_hw.bin` | `41167491…80ea7d` | 655 B |

Basta con crearse ese árbol mínimo (`~15 KiB` en vez de 1,1 GB) trayendo los tres ficheros del tag
correcto de gitee y comprobando el hash antes de usarlos:

```sh
REF=AC79NN_SDK_V1.2.1_2023-12-13
T="$HOME/fw-AC79_AIoT_SDK/cpu/wl82/tools"; mkdir -p "$T/cfg"
curl -fL -o "$T/uboot.boot"        "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$REF/cpu/wl82/tools/uboot.boot"
curl -fL -o "$T/cfg_tool.bin"      "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$REF/cpu/wl82/tools/cfg_tool.bin"
curl -fL -o "$T/cfg/eq_cfg_hw.bin" "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$REF/cpu/wl82/tools/cfg/eq_cfg_hw.bin"
sha256sum "$T/uboot.boot" "$T/cfg_tool.bin" "$T/cfg/eq_cfg_hw.bin"
export AC79_SDK="$HOME/fw-AC79_AIoT_SDK"
```

`./build.sh` sólo comprueba `$AC79_SDK/cpu/wl82/tools/uboot.boot`, y `build.py` verifica los tres
hashes al empaquetar: si el tag sirve otros bytes, el build se detiene antes de escribir el `.fwsc`.
El clon completo sigue siendo válido, pero para compilar es tiempo y disco que no se usan.

Todo lo anterior es lo que hace `python3 tools/get_ac791n.py --sdk </ruta>`: trae los tres ficheros del
tag `AC79NN_SDK_V1.2.1_2023-12-13`, compara el SHA-256 contra el que `tools/build.py` fija en
`SDK_SHA256` y **no escribe** lo que no coincide (dice el hash que llegó y el que se esperaba).

## 7. Estado del build en este árbol (medido, 2026-11-13)

Con `JIELI_TOOLCHAIN` (clang 4.0.1, `pi32v2`) y el SDK de la sección 6, `./build.sh` compila todo,
enlaza y **empaqueta el `.fwsc`**:

```
ok      image 581276 B; RAM .data+.bss 87764 B of 98304; pool 305096 B, spare 15392 B
ok      XIP code 403804 B, constants 157156 B
ok      RAM code 19552 B of 24576; calls reach 56708 B past the code's end
package build/felucca.fwsc  610959 B, identity FM-1_965   (XIP 581564 B: 288 B de margen)
```

### 7.1 Los 1 048 B que sobraban

El 07b5d94 dejaba 19 832 B de margen (imagen 561 732, `XIP code 399 240 B, constants 139 260 B`).
Desde ahí el árbol creció en **4 304 B de código y 18 980 B de constantes** por las pantallas de las
fases 2 y 3 (`XIP code 403 544 B, constants 158 240 B`, 1 048 B por encima de la región). No era grasa:
con `-fdata-sections` + `--gc-sections` sólo había **404 B** de secciones sin referencia, y ni
`minsize` ni `-fmerge-all-constants` movían un byte.

El recorte salió de un dato que la propia fuente arrastraba: la tabla de kerning de la cara S
(`AF_S_KERN` + `AF_S_KD`) son **3 366 B** — 1 122 pares a 3 B cada uno (clave + delta en 1/16 px) — para
correcciones que la rejilla de 12 px casi nunca muestra. Con `gen_aa_font.py --kern-min 4` — el flag
que el generador ya traía para esto — quedan **592 pares** y se van **1 590 B de constantes**
(`constants 158 240 → 156 904`). Los bitmaps de los glifos **no cambian**: el `.h` regenerado sólo
difiere en las dos tablas y en el `nk` del `aafont_t`.

Los 112 B que faltaban los puso `-Wl,--gc-sections` en el enlace (`--gc` = 144 B aquí). Es seguro en
este árbol porque el `app.ld` ya hace `KEEP` de todo lo que se referencia desde ensamblador
(`.text.crt0`, `.ram_text`, `.c1_text`, `.hot_text`) y porque se comprobó que la tabla de símbolos del
`.text` es **idéntica** con y sin la opción: 649 símbolos en los dos casos, ninguno fuera.

### 7.2 Lo que costó, medido

- `tests/text_spacing_test.py build/gen/ui_fonts.h`: **ok**. La peor pareja de la cara S queda a
  **0,239 px** de su sitio (el test falla por encima de 0,25 px) y la M no se toca (0,044 px). Con
  `--kern-min 5` sí falla (`'xe' +0.38 px`), así que 4 es el límite honesto.
- `regress`: **285 renders dorados, 0 cambiados**, 0 fallos de salud, CPU dentro de presupuesto.
- `ui_test`: all passed. Barrido 191 pantallas × 22 paletas: **0 hallazgos de lint y 0 px de
  desalineación** en 3 155 200 medidas (516 textos recortados con puntos suspensivos en vez de 514:
  el ancho de una cadena cambia como mucho una fracción de píxel).
- `tests/target_budget.py`: pasa todo, con los mismos números que antes del recorte
  (`perf_block 254/253`, `rev_room 243/243`, `DRUM 1153/1153`): el código generado no cambió.

### 7.3 Knobs probados y descartados (medidos, no supuestos)

`minsize` por función (`tools/size_fns.py`, 0 B: el módulo ya va a `-Oz`), `-fmerge-all-constants`
(0 B), `-falign-functions=2` (**+648 B**: las funciones ya van empaquetadas al byte),
`-fshort-enums` (**+648 B**), `--icf=all` (este `ld` no lo tiene), `-inline-threshold=8` (+1 536 B) y
`-inline-threshold=60` (+67 460 B). `-fdata-sections` + `--gc-sections` sólo dan 404 B y no hacen
falta una vez recortado el kerning.

Queda **1 048 B de deuda pagada y 288 B de margen**: el mapa de flash no se ha tocado (sigue
`APP_SLOT 0x8DFBC`). El hueco libre real antes de la partición siguiente no era 3 493 B sino **2 774 B**
(la región empaquetada terminaba en `0x8E52A` de `0x8F000`); en la fase 7 el slot creció **2 048 B** hasta
`0x8E7BC`, la región acaba en `0x8ED2B` con 725 B de holgura y `FLASH_SIZE` no se movió.


## 8. Cuánto mide cada motor: XIP y `.data+.bss` desde el propio build

`./build.sh` escribe `build/felucca.syms` (la tabla de símbolos del ELF), `build/felucca.map` (el mapa
del enlazador, `-Map`) y ejecuta al final `tools/engine_size.py --strict`: si un motor o la imagen no
entran en el presupuesto, el build **falla** en vez de dejar el número para después. Releer la medición
no pide toolchain ni trabajo manual — basta el árbol ya construido:

```sh
python3 tools/engine_size.py            # tabla por motor
python3 tools/engine_size.py --map      # + verificación cruzada contra el mapa del enlazador
python3 tools/engine_size.py --json     # la misma tabla, para scripts
```

**Por qué por símbolos y no por el `.map`:** el firmware enlaza como un solo objeto *unity*, así que el
mapa no dice a qué motor pertenece un `.text.<fn>`. La tabla de símbolos sí, porque cada función tiene su
propia sección (`-ffunction-sections`) y su tamaño. El mapa se usa para lo que sí puede probar: que la
misma función mide lo mismo en los dos artefactos (**88 funciones de motor, 0 discrepancias** en el build
de v1.3.1).

La atribución sale de las fuentes (`firmware/src/eng_*.c`, definiciones a columna 0), no de una lista a
mano: un nombre que declaren **dos** motores no se atribuye a ninguno y cae en `(rest)`. Cuenta como XIP
lo que vive en `.text`; lo que el enlazador copia a RAM (`.ram_text`, `.c1_text`, `.hot_text`) se informa
aparte, en su propia columna, para que no se confunda con flash; `.data` y `.bss` van en las suyas.

### 8.2 La auditoría de la memoria viva (v1.3.1, medido sobre `build/felucca.syms`)

El `.data+.bss` está en 95 012 B de 98 304 tras devolver 1 536 B en `cdc_out` (1.3.1); el margen real es el
pool (9 248 B). Los símbolos más grandes, medidos con `nm` sobre el build AC791N:

| símbolo | bytes | qué es | ¿se puede recuperar? |
|---|---:|---|---|
| `WT_GEN` | 6 192 | banco de wavetables: 3 niveles de banda × 8 frames × **media** onda (129 puntos, `int16`); la otra mitad es su negación al revés (`wt_pts`), porque cada frame es una suma de senos y es impar (v1.5.22) | ya recortado a la mitad en v1.5.22 (12 288 → 6 192 B, `docs/FASE1_OPTIMIZACION_v1.5.22.md`): lo que queda es la tabla, no grasa |
| `trk` | 10 560 | las seis pistas (6 × 1 760: 64 pasos de 17 B + 672 de lo demás) | **no, medido y descartado (1.3.1.1)**: empaquetar `step_t` a 15 B devuelve 1 024 B de `.bss` pero cuesta ~4,4 KB de XIP (cada escritura a un bit-field es leer, enmascarar, o, escribir) y el enlace revienta `region XIP overflowed by 3776 bytes` |
| `fm6_note` | 7 104 | 6 pistas × voces FM6 | auditar la estructura; mover constantes a XIP |
| `drum_kit` | 5 376 | estado vivo de las 8 líneas por pista | no: es estado de voz |
| `st_buf` | 3 840 | payload de un sector de flash (4 KiB − 256 de cabecera) | no sin cambiar el layout de flash |
| `upf` | 3 728 | índice del banco de FM6, persistente | no: se guarda en flash |
| `gen_keep` | 2 176 | UNDO de GEN (2 × 64 pasos), sobrevive entre frames | no: es estado |
| `ua_ring` | 2 048 | anillo de audio USB (512 frames, 10,7 ms) | **no**: 512 es el mínimo. `usb.c` lo exige con `_Static_assert(UA_HI + HALF_FRAMES + UA_MAXF < UA_N)` = 128 + 128 + 46 = 302 marcos, y el índice va con máscara, así que tiene que ser potencia de dos: 256 no cumple |
| `hc_tab` | 1 548 | 3 LUT de Huffman (una por fuente) | no se comparten: cada fuente tiene su código |

**La unión de búferes que se propuso en la revisión no es válida**: `gen_keep`, `upf` y `st_buf` tienen vidas
que se solapan (UNDO de GEN, índice en flash y cuerpo de objeto devuelto por `editor_backup`). Lo que sí es
seguro y está hecho es dimensionar los anillos por lo que de verdad transportan.

**El recurso escaso es la imagen, no la RAM** (1.3.1.1, y sigue válido en 1.3.2). Con la imagen en 582 928 B
quedaban **684 B** de XIP; el árbol de la 1.3.2, con el arreglo de HOME, mide **582 864 B y deja 748 B** (ver
§8.3), con 3 292 B de `.bss` y 9 248 B de pool: cualquier cambio que cambie RAM por código va en la dirección
equivocada, y cualquiera que gaste imagen tiene que pagarla antes.

### 8.3 La auditoría de la 1.3.2: qué se midió y qué se descartó

Cada candidato se construyó con el toolchain y el SDK fijados y se midió contra el mismo árbol, con la suite
de host y los 293 renders dorados como condición. Los tres que tocaban código o flags no compran nada en la
región que aprieta, así que **no quedaron en el árbol** (el código sólo conserva el comentario que lo dice):

| candidato | qué ahorra | qué cuesta | veredicto |
|---|---|---|---|
| memorizar la máscara de portadoras de FM6 (`fm6_carriers`) en una palabra | 5 vueltas de 6 pasos por nota y bloque | **+48 B de XIP** | descartado: el presupuesto no ve la vuelta, y la imagen es el recurso |
| caché de «última fuente» delante de `hc_lut()` | 1–2 comparaciones de puntero por glifo | **+16 B de XIP** | descartado por lo mismo: el texto no gasta un frame en esa búsqueda |
| `-fdata-sections` (para que `--gc-sections` recoja datos sin referencia) | — | imagen **582 828 B frente a 582 832 B** | descartado: −4 B es relleno de sección, no datos muertos (coincide con la medición de la FASE 1) |

Lo que sí se corrigió es la herramienta que informa: `tools/ram_report.py` decía **21 252 B libres de XIP**
(sumaba sólo `.text`, sin las copias de carga de la RAM code y `.data`) y **3 328 B libres de RAM** (sin el
hueco de alineación de 36 B entre `.data` y `.bss`). Ahora las dos filas salen de los mismos símbolos que
comprueba `tools/build.py` y coinciden al byte: XIP **582 864 / 748**, RAM **95 012 / 3 292**, pool
**318 432 / 9 248**, LOWBUF **26 724 / 1 948**, RAM code **19 676 / 4 900**, NOINIT **15 572 / 124**.
Detalle y tabla en `docs/MEMORY_BUDGET.md` §9. Medido: `step_t` de 17 → 15 B (−768 B en `trk`,
−256 B en `gen_keep`) compila y pasa la suite de host entera, pero el enlace del AC791N lo rechaza
(`section .hot_text will not fit in region XIP`, `region XIP overflowed by 3776 bytes`); el mismo árbol sin el
cambio enlaza en 582 928 B como siempre.

**Los dos anillos de 2 048 B que quedan no se pueden tocar sin cambiar comportamiento** (1.3.1.1):

- `ua_ring` es el anillo del audio USB: 512 frames es el mínimo que deja el invariante del propio `usb.c`
  (banda 80..128 + un render de 128 + un paquete de 46 = 302 marcos, y el anillo tiene que ser potencia de
  dos porque se indexa con máscara). La banda no es una estimación: está medida en unidad con carga del
  65 % de CPU (ver el comentario de `UA_LO`/`UA_HI`).
- `abuf` es el doble búfer del DMA de audio (2 × `HALF_FRAMES` × `int32`). La mitad es el bloque que el
  codec consume entre interrupciones, y de `HALF_FRAMES` cuelgan `CPU_AVG`, `SHED_TICKS` y los umbrales de
  LOAD (65 % / 60 %, ECO 50 % / 45 %): reducirlo duplica la tasa de interrupción y recalibra el medidor de
  carga. Es un cambio de temporización que se prueba en la FM-1, no en el host.
- **Compartir un anillo entre CDC y UAC es incorrecto**: son dos endpoints del mismo dispositivo compuesto
  y coexisten (la consola abierta mientras suena el audio). `ep3tx` (EP3) sale de `cdc_out` y `ua_ring`
  alimenta EP4; los dos se atienden en el mismo camino de interrupción USB, y un solo anillo mezclaría
  texto con muestras. `cdc_out` sí bajó de 2 048 a 512 B en 1.3.1 porque su cola es una línea de
  diagnóstico.

### 8.1 El build de v1.3.1 (2026-11-16, medido en AC791N)

Con el toolchain de §1 y los tres blobs de §6, `./build.sh` termina en:

```
ok      image 582928 B; RAM .data+.bss 95012 B of 98304; pool 311256 B, spare 9248 B
ok      XIP code 422372 B, constants 140084 B
ok      RAM code 19676 B of 24576
package build/felucca-1.3.1.fwsc  610959 B, identity FM-1_965   (684 B de margen en el slot)
```

| motor | XIP | `.data` | `.bss` | símbolos | mayor símbolo |
|---|---:|---:|---:|---:|---|
| FM6 | 6 856 | 0 | 8 966 | 29 | `fm6_note` 7 104 B |
| DRUM | 4 282 | 4 | 6 891 | 40 | `drum_kit` 5 376 B |
| LOFI | 8 934 | 0 | 0 | 35 | `lofi_float` 2 336 B |
| ANALOG | 6 656 | 0 | 0 | 18 | `ANALOG_PRESETS` 2 448 B |
| WHEEL | 3 184 | 0 | 2 232 | 18 | `drw_v` 1 656 B |
| PHASE | 3 926 | 0 | 0 | 13 | `PHASE_PRESETS` 1 152 B |
| VOICE | 3 509 | 4 | 0 | 12 | `formant_render` 1 730 B |
| NOISE | 3 332 | 0 | 0 | 15 | `noise_render` 1 596 B |
| **ADDTV** | **1 832** | **0** | **1 440** | 10 | `ad_v` 1 224 B |
| WTABLE | 2 310 | 0 | 4 | 10 | `wt_render` 908 B |
| DIGITAL | 0 | 0 | 0 | 0 | no compilado (`FELUCCA_FM4=0`) |
| (resto: UI, DSP, secuenciador, tablas) | 489 945 | 753 | 75 928 | 2 852 | — |
| **total** | **534 766** | **761** | **95 461** | **3 052** | — |

`RAM: .data+.bss 95012 B of 98304 B (3292 B free)` e `image: 582928 B of 583612 B (684 B free)`.

El motor ADDITIVE entra con **1 832 B de XIP y 1 440 B de `.bss`**: el `.bss` es exactamente el
`sizeof` de sus estructuras (1 224 B de voces + 216 B de partes) y el XIP queda por debajo de la
estimación hecha en host (~2,9 KB), que es lo que se esperaba de `-Oz` sobre pi32v2. El `.fwsc` mide
610 959 B en v1.2.5, en v1.2.6, en v1.2.7 y en v1.3.0 porque el contenedor llena un tamaño fijo por slot: la identidad de un
paquete es su SHA-256 y su `identity`, no su longitud.

`tests/engine_size_test.py` (host, sin toolchain) fija el camino: qué símbolo pertenece a qué motor,
XIP contra RAM, la verificación con el mapa, el código de salida de `--strict` y la forma del `--json`.

### 8.4 La 1.3.2 final: la escena de FX entra, el SLICER por paso no

**Lo que entra.** La escena de FX por pista (los cuatro envíos de la página FX y el tipo de DIST, cuatro ranuras por
pista) cuesta **456 B** de imagen y 120 B de `.bss`: el build de `d4b5842` enlaza en **583 320 B de 583 612** (292 B
de margen), con `.data + .bss` en **95 124 B de 98 304**, pool **311 256 B con 9 248 B de sobra**, RAM code
**19 632 B de 24 576**, LOWBUF **26 724 de 28 672** y NOINIT **15 572 de 15 696**. Es el mismo paquete de **610 959 B**
con identidad **FM-1_965**: el contenedor llena un tamaño fijo por slot.

**Lo que no entra, medido.** El SLICER con DEPTH y MODE por paso (rama `sl-step`) está escrito y verde en host, y el
enlace del AC791N lo rechaza: `region XIP overflowed by 816 bytes` con el gráfico de la página y **496 B** sin él,
contra **292 B** libres. Por símbolos, en el mismo build: `graph_slstep` **310 B**, `proj_unpack_c` **+120**,
`page_desc` **+92**, `proj_pack` **+84**, `graph_signature` **+84**, `project_capture` **+64**,
`project_restore_runtime` **+48**, `slicer_on` **+34**, `sl_enter` **+32**, `slicer_bus` **+28**, `system_init` **+20**
y ~88 B de cadenas y descriptores.

Detrás del slot no hay colchón: la región empaquetada acaba en `0x8ED2B` y la partición siguiente empieza en
`0x8F000`, o sea **725 B** en total. Las salidas son tres: comprimir datos (el blob de `build/gen/ui_shapes.h` son
10 861 B de puntos, y ya se hizo algo así con el kerning de la cara S: 1 590 B con `--kern-min 4`), crecer `APP_SLOT`
comiendo ese margen (cambia el bloque `app_area_head` del paquete y hay que resincronizar instalador, emulador y
sitio), o recortar la función. Ninguna es de una línea, así que el feature espera.

### 8.5 La 1.3.3: el arreglo de la escena de FX, 32 B

Guardar la escena de FX con OCT+ en su página también movía la octava, porque el `case B_OCTDN/B_OCTUP` de la
octava seguía resolviendo el botón al soltarlo. La página FX sale ahora antes de ese caso (como ya salía con
`LAYER_FX`), y el arreglo cuesta **32 B**: la imagen queda en **583 352 B de 583 612** (**260 B** de margen),
`.data + .bss` en **95 124 B de 98 304**, pool con **9 248 B** de sobra y RAM code en **19 632 B de 24 576**.
La ranura fantasma `fx_scn_pick`, que nadie escribía ni leía, se fue. Con **260 B** libres el SLICER por paso
(**496 B** sin su gráfico) sigue sin caber, y por eso su rama queda esperando flash, no RAM.

### 8.6 La 1.3.5: la tinta translúcida del visualizador, 96 B

Los trazados del visualizador se mezclan ahora con lo que ya hay en el framebuffer en vez de taparlo: `cv_ink`
(porcentaje de tinta, 100 por defecto) lo lee solo `cv_pset`, y `cv_line_a(x0,y0,x1,y1,c,pct,thick)` lo pone, recorre
con el `cv_line_t` que ya existía y lo devuelve a 100. La escalera es `CV_A_SIGNAL 84`, `CV_A_CURVE 70`, `CV_A_WAVE 74`,
`CV_A_MOD 56` y se aplica en cuatro puntos de `ui_being.c`. **96 B** de imagen y 0 B de RAM: la imagen queda en
**583 548 B de 583 612** (**64 B** de margen), `.data + .bss` en **95 124 B de 98 304**, pool con **9 248 B** de sobra,
RAM code **19 632 B de 24 576**, LOWBUF **26 724 de 28 672** y NOINIT **15 572 de 15 696**; el paquete sigue siendo de
**610 959 B** con identidad **FM-1_965**, porque el contenedor llena un tamaño fijo por slot.

Duplicar el Bresenham para no tocar `cv_ink` fue el primer intento y el enlazador lo rechazó: `region XIP overflowed
by 96 bytes`. Con **64 B** libres, cualquier cosa que sea una fila nueva de página (**92 B** solo en descriptores,
§8.4) o un parámetro de motor no entra, y por eso esta fase no añade controles: la mejora es de dibujo, no de tabla.

### 8.7 La 1.3.6: los avisos a cero, y el árbol sin gastar un byte

Los seis avisos que el target arrastraba (`ui_seqtools.c` escribiendo pasos por un accesor `const`, `eng_phase.c`
pasando el estado del ruido como `uint32_t *` a `noise32(int32_t *)`, `nt[12]` sin leer en `ui_scl.c`, dos locales
muertos en `ui_being.c` y uno en `main.c`) se arreglaron sin cambiar comportamiento: **0 warnings, 0 errors**. La pila
del manejador de la progresión recupera **12 B** y el resto de los locales no costaba imagen, así que la imagen queda
igual: **583 548 B de 583 612** (**64 B** de margen), `.data + .bss` **95 124 B de 98 304** (3 180 B libres), pool
**311 256 B con 9 248 B de sobra**, RAM code **19 632 B de 24 576**, XIP **563 120 B**, LOWBUF **26 724 de 28 672** y
NOINIT **15 572 de 15 696**. El paquete sigue en **610 959 B**, identidad **FM-1_965**.

Los 303 renders dorados no se movieron y el presupuesto de CPU del regresor quedó en **0 entradas por encima** de 20
notas medidas: la limpieza es de código, no de sonido. Con 64 B libres, esta fase **no** añade parámetros, FX ni
presets — y lo dice en lugar de insinuarlo.

### 8.8 La 1.3.7: 904 B de vuelta, y el precio real de un parámetro nuevo

La fase 1.3.7 no añade sonido: quita del generador **seis píldoras de keycap que ninguna pantalla dibujaba**
(`KC_ARP`, `KC_K3`, `KC_K4`, `KC_HOLD`, `KC_ON`, `KC_OFF`) y deja el mapa de teclas en **4 699 B** en vez de 5 564.
El build del AC791N lo paga con la cuenta exacta:

```
XIP: code 422928 B + constants 139288 B = 562216 B from 0x2000120
RAM: .data+.bss 95124 B of 98304 B (3180 B free)
image: 582644 B of 583612 B (968 B free in the app slot)
RAM code 19632 B of 24576
```

**583 548 → 582 644 B**: 904 B, todos de constantes, con el código en el mismo byte. Y el parámetro que esos bytes
debían pagar (KEYTRK) se midió en **+2 456 B** de código repartidos en 147 funciones, así que no entra: la razón
completa, con los tres builds de control, está en `docs/FLASH_RECLAIM.md` §6.

### 8.6 La 1.3.9: sólo dibujo, 128 B de imagen

La 1.3.9 no toca el motor ni la RAM: es la pasada de UI del §9.5 de `MEMORY_BUDGET.md`. El enlace da
**582 772 B de 583 612** (**840 B** de margen), `.data + .bss` en **95 124 B de 98 304**, pool **311 256 B con
9 248 B de sobra**, y el paquete sigue en **610 959 B** con identidad **FM-1_965**. Son **+128 B** sobre la 1.3.8
de `d2fe9d2` (582 644 B), o sea 80 B de `.text` y 48 B de constantes.
