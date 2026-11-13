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

Con `JIELI_TOOLCHAIN` (clang 4.0.1, `pi32v2`) y el SDK de la sección 6, `./build.sh` **compila todo
y llega al enlace**, y ahí el enlace **no cabe**:

```
ld: build/felucca.elf section `.hot_text` will not fit in region `XIP'
ld: region `XIP' overflowed by 1048 bytes
loader: image 9236 B at 0x1c0a800, bss end 0x1c0f910   (el loader sí enlaza)
```

Es un defecto **anterior a esta fase**, no causado por ella: revirtiendo los dos ficheros de UI de la
fase 2 el desborde es de **856 B**, y el último commit que enlazó (`07b5d94`) dejaba **19 832 B** de
margen (imagen 561 732 B, `XIP code 399 240 B, constants 139 260 B`). Desde entonces el árbol creció
en **4 304 B de código y 18 980 B de constantes** (`XIP code 403 544 B, constants 158 240 B`).

Lo que sí se pudo medir con el código real del chip (`ld --noinhibit-exec` + `objdump -d` →
`tests/target_budget.py`): **todos los presupuestos de ciclos pasan**, incluidos `mod_voice 19/18`,
`perf_block 254/253` y `rev_room 243/243`; y `loader/ota.bin` enlaza (9 236 B, 7 803 B empaquetado).

Salidas posibles, ninguna aplicada aquí porque las dos son decisiones del dueño:

1. **Recortar 1 048 B** de código/constantes (es un 0,2 % del `.text`): lo natural es empezar por las
   constantes, que son las que crecieron (tablas de salto de los `switch` nuevos y cadenas de las
   pantallas añadidas).
2. **Ampliar el slot declarado** `APP_SLOT`/`XIP LENGTH` en 1 056 B (`0x8DFBC` → `0x8E3DC`). El
   `flash.bin` mide `0x93000` y el bloque de aplicación acaba en `0x9225B`, así que hay 3 493 B
   libres antes de la partición siguiente y **ninguna partición se movería**; pero es un cambio del
   mapa de flash y no se ha hecho sin probarlo en una unidad.

Knobs probados y descartados con medidas (no ayudan en este compilador): `minsize` por función
(`tools/size_fns.py`, efecto 0 B: el módulo ya se compila a `-Oz`), `-fmerge-all-constants` (0 B),
`--gc-sections` + `-fdata-sections` (404 B, y arriesga secciones sólo referenciadas desde ensamblador),
`-inline-threshold=8` (+1 536 B) y `-inline-threshold=60` (+67 460 B).
