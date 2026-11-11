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
