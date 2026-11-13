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

