#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Felucca in the browser: the firmware's sources as the host tests build them (web/emu/felucca_web.c), compiled
# to WebAssembly with Emscripten, plus the page. After X0X's web/emu/build.sh (charlesvestal/fm1-x0x, GPL-3.0).
#   web/emu/build.sh   ->  build/emu/{index.html, worklet.js, felucca.wasm, fonts/}
# Run after one ./build.sh (build/gen: the tables, the fonts, the CC0 samples).
# -ffp-contract=off: no fused multiply-adds, as the host build. EMU_OPT replaces -O2, EMU_CFLAGS adds flags
# (EMU_CFLAGS=-msimd128: let the compiler vectorise; every browser with AudioWorklet has wasm SIMD).
# The page shows the version (FELUCCA_VERSION); EMU_PREVIEW=1 adds "(preview)" for an unreleased build.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: run ./build.sh once first (build/gen)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "emu: no emcc (Emscripten)"; exit 1; }
# One version everywhere: .github/workflows/target.yml installs EMU_EMCC and docs/BUILD_AC791N.md
# documents it. Another compiler usually still works, so this warns instead of refusing, but the
# parity check compares bit for bit and a silent version drift is what makes it hard to trust.
EMU_EMCC="${EMU_EMCC:-6.0.11}"
have="$(emcc --version 2>/dev/null | sed -n '1s/.*) \([0-9][0-9.]*\).*/\1/p')"
[ -n "$have" ] && [ "$have" != "$EMU_EMCC" ] && \
    echo "emu: emcc $have, the pinned version is $EMU_EMCC (docs/BUILD_AC791N.md)"
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
mkdir -p build/emu/fonts
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c
# the commit that goes in the page and the manifest is the tree the WASM was compiled from, not the
# deploy's: on a normal CI checkout they are the same, but the stable channel builds this in a worktree
# at the signed release's commit and GITHUB_SHA there is main's (measured on the live site: the root
# served an emulator labelled with main while versions.json named the release, and site_check failed).
EMU_COMMIT=$(git rev-parse --short=12 HEAD 2>/dev/null || printf '%s' "${GITHUB_SHA}" | cut -c1-12)
EMU_PREVIEW="${EMU_PREVIEW:-0}" EMU_COMMIT_OVERRIDE="$EMU_COMMIT" python3 tools/package_emu.py
# (v1.9.0) one manifest for the tree: refresh it now that the WASM and its own manifest exist, so the
# firmware, the package and the emulator cannot describe different commits. Needs a ./build.sh first.
[ -f build/manifest.json ] && python3 tools/build_manifest.py
cp web/emu/worklet.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
