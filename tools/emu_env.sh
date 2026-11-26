#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Find Emscripten for the emulator build, the host suite and the dev server. Sourced, not run:
#   . tools/emu_env.sh && emu_find_emcc
#
# Why this exists (2.16): `emcc` is not on the PATH in this project's own setup -- the SDK lives in
# $HOME/emsdk and emsdk_env.sh is what puts it there -- so every caller grew its own copy of the search,
# and each copy had the same hole: emsdk's emsdk_env.sh cannot work out its own directory when it is
# sourced by dash (`sh`) from outside the SDK ("Error: unable to determine 'emsdk' directory"), so the
# sourcing failed silently under `sh` and the suite reported "no emcc" on a machine that has Emscripten
# 6.0.11 installed. The paths emsdk_env.sh would add are added directly as well, which needs nothing
# from the SDK's shell heuristics: upstream/emscripten holds emcc and upstream/bin the LLVM tools.
#
# Returns 0 when emcc is reachable afterwards, 1 when it is not. It never downloads anything.

emu_find_emcc() {
    command -v emcc >/dev/null 2>&1 && return 0
    for sdk in "${EMSDK:-}" "$HOME/emsdk" "/opt/emsdk" "/tmp/emsdk"; do
        [ -n "$sdk" ] || continue
        if [ -x "$sdk/upstream/emscripten/emcc" ]; then
            PATH="$sdk/upstream/emscripten:$sdk/upstream/bin:$PATH"
            export PATH
            command -v emcc >/dev/null 2>&1 && return 0
        fi
        if [ -f "$sdk/emsdk_env.sh" ]; then
            # shellcheck disable=SC1090
            . "$sdk/emsdk_env.sh" >/dev/null 2>&1 || true
            command -v emcc >/dev/null 2>&1 && return 0
        fi
    done
    return 1
}

# the version emcc reports (empty when there is none): compared against the pin in web/emu/build.sh
emu_emcc_version() {
    emcc --version 2>/dev/null | sed -n '1s/.*) \([0-9][0-9.]*\).*/\1/p'
}
