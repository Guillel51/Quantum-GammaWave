# Building Quantum GammaWave (QG-1)

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `felucca.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow and fontTools: `pip3 install Pillow fonttools` (the UI font and icons are
  rasterised at build time)
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

  The archive is pinned by SHA-256 when `JIELI_TOOLCHAIN_SHA256` is set (`tools/get_toolchain.sh`
  prints the hash it got otherwise, so it can be reviewed and pinned); the SDK's three files are
  checked against `SDK_SHA256` in `tools/build.py`.

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).
- Emscripten (optional, `emcc`: the browser emulator, `web/emu/`).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 1.0` makes a release build: the package identity stays `FM-1_982` (Quantum GammaWave's own; Pocket GammaWave is FM-1_981,
the one The Firmware Hub knows; Felucca's per-release `FM-1_9XY` gave 1.1 `FM-1_911`, Felucca 1.1's),
and the version string `v1.0`; the package is `build/felucca-1.0.fwsc`, and
`build/release-1.0/` holds what a release ships: the package, the app
(`felucca-1.0-app.bin`), `SHA256SUMS`, `LICENSE`, `LICENSING.md` and
`LICENSES/` (the package contains Apache-2.0 SDK files, so the licence texts travel with it).

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`, `core.h` and `icons.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_CDC_DEFAULT` | 1 | `0`: the console is built in but left out of USB from boot (as MENU > USB SERIAL OFF) |
| `FELUCCA_UAC` | 1 | USB audio input (the master output, stereo, 44.1 or 48 kHz) |
| `FELUCCA_UAC_48K` | 1 | `0`: the USB audio input at 44.1 kHz only, as up to 1.0.5.2 (`firmware/src/usb.c`) |
| `FELUCCA_UART` | 1 | TRS MIDI IN |
| `FELUCCA_ICONS` | 1 | parameter icons on the knob cards |
| `FELUCCA_FM4` | 0 | the retired DIGITAL engine (4-operator FM) instead of its FM6 conversion |

`FELUCCA_USB_LAYOUT` (`0` to `3`, default `0`) picks other USB descriptor layouts for testing; see
`firmware/src/usb.c`.

The build checks the image's budgets and prints them (`tools/build.py`, `check`): the app slot (568 KiB of XIP
flash), RAM `.data` + `.bss` (96 KiB), the pool's headroom, the RAM code's room and its call reach. The RAM code
(flash driver, second core, hot audio) calls into the flash image and back with a ±4 MiB reach, so the **code** must
end within it, 8 KiB to spare. The linker places all code first and all constants (fonts, icons, tables, presets,
strings) after it (`firmware/app.ld`, `_ecode`), and constants are never called, so the reach is measured to the end
of the code (`XIP code … B, constants … B` in the notes); constants only use up the slot.

## Build identity (what produced these bytes)

Every build writes `build/manifest.json` **after** the artefacts (`tools/build_manifest.py`, called by
`tools/build.py` and refreshed by `web/emu/build.sh`). It records, together:

- the exact `commit` (`GIT_COMMIT`/`GITHUB_SHA` in CI, `git rev-parse HEAD` locally) and the
  uncommitted files (`dirty`, minus `build/`) — so a build from a modified tree says so;
- the `version` compiled into the firmware and the package `identity`;
- the build `config` (`FELUCCA_*`, `FELUCCA_USB_LAYOUT`), the `toolchain` (path, name and the clang
  binary's sha256) and the `sdk` (checkout commit and the sha256 of its three files);
- `artifacts`: sha256 and size of `felucca.bin`, `loader/ota.bin` and the package;
- `emulator`: the WASM's version, commit, sha256 and size, so the two cannot disagree.

```
python3 tools/build_manifest.py --verify                    # after a build
python3 tools/build_manifest.py --verify --require-clean \
        --expect-commit "$(git rev-parse HEAD)"             # what CI runs
```

`--verify` fails on any mismatch: a changed artefact, a manifest that names another commit, a dirty
build under `--require-clean`, a version that is not the firmware's, or a WASM built elsewhere. A
`--release` build refuses a dirty tree outright. The target build, the storage-less configuration and
the WASM parity check run in `.github/workflows/target.yml`; the host tests and the sanitizer runs in
`.github/workflows/c-cpp.yml`.

## Browser emulator

```
./build.sh
web/emu/build.sh
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

`web/emu/felucca_web.c` builds the firmware's sources as the host tests do, against a simulated FM-1, and
Emscripten compiles it to WebAssembly (`build/emu/felucca.wasm`, with `index.html`, `worklet.js` and the font in `fonts/`). The page
keeps the flash's storage sectors (projects, user presets, settings) in the browser. Not emulated: USB (audio,
MIDI out, the editor, updates) and TRS MIDI.

## Tests

```
tests/run_tests.sh
```

Runs the host tests and, with Node.js, the web page tests (and, with Emscripten too, the browser emulator's). Run it after `./build.sh`
(it uses `build/` and needs `AC79_SDK` set as for the build). The suites cover flash storage,
user presets, projects of every format, backup, the keys and knobs, MIDI (USB, TRS, clock,
control), USB audio, the update entry and loader, the command-line installer, the UI (the real
drawing code against stubs: every screen in every palette is rendered and checked for clipped or
overlapping text; PNGs land in `build/ui_new/`), every engine (DRUM, NOISE, FM6, the
DIGITAL conversion), the chord keys, the modulation matrix, the FX layer, the reverbs, the SLICER
and swing.

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). After an intended change of
the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1`
does the same for the cost files.

## Install

Use the web installer in Chrome or Edge:
<https://guillel51.github.io/Quantum-GammaWave/webapp/installer/>. It installs the released package (the file names keep
Felucca's, `felucca.bin` / `felucca.fwsc`: JIANT is built on it).

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/felucca.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

The page carries the build record (`build/manifest.json`) compiled into it and refuses to install
if the package it is given disagrees with that record in commit, version, product identity, size or
sha256 — and an unrecorded package is refused too. `make_site.py` enforces the same thing on the
machine that builds the site, so a mismatched site cannot be published in the first place. The check
runs before the first MIDI frame, so a package that fails it is never written to the device:

```
python3 tools/build_manifest.py --verify --require-clean
python3 web/make_site.py build/felucca.fwsc dev /tmp/felucca-site
cd /tmp/felucca-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

That record proves **integrity and provenance** — these bytes are the ones this commit produced —
not **authenticity**: without a signing key the page cannot tell a genuine Felucca build from one
rebuilt by someone else at the same commit, and it does not pretend to. The official FM-1 V15 file
is admitted only through its digest pinned in the source, and releases recorded before v1.9.0 have
no record, so they are no longer installable from the page.

When there is a key to sign with, `--build-manifest` puts the record's canonical SHA-256 inside the
Ed25519 signature (`build_sha256`), so authenticity and provenance are checked together: the
transport refuses a signed release that names no record, or another one.

```
python3 tools/fm1_sign.py sign build/felucca.fwsc --key /secure/qg1-release.pem --version 1.9.0 \
    --min 1.8.0 --keyid qg1-release-1 --build-manifest build/manifest.json
```

With `tools/fm1_keys.json` empty every package is refused (see `docs/SIGNING_CUSTODY.md`).

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
