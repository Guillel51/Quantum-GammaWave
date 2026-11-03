#!/usr/bin/env bash
# Split the long Quantum GammaWave suite into reproducible groups.
# Usage: tests/groups.sh ui|presets|web|emu|target|all
# Each group keeps its own exit status; no assertion or budget is changed.
set -eu
cd "$(dirname "$0")/.."

group=${1:-all}
mkdir -p build/uifix

run_ui() {
    cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/uifix/ui_test tests/ui_test.c -lm
    ./build/uifix/ui_test
    ver=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
    cc -O1 -w "-DFELUCCA_VERSION=\"$ver\"" -Ibuild/gen -Ifirmware/src -Itests \
        -o build/uifix/ui_render tests/ui_render.c -lm
    rm -rf build/ui_group build/ui_group_slot
    mkdir -p build/ui_group/ppm build/ui_group_slot/ppm
    ./build/uifix/ui_render build/ui_group build/ui_group_slot
}

run_presets() {
    python3 tools/gen_preset_categories.py
    python3 tests/preset_categories_test.py
}

run_web() {
    node web/test_web.mjs
}

run_emu() {
    if [ ! -f /tmp/emsdk/emsdk_env.sh ]; then
        echo "emu: /tmp/emsdk/emsdk_env.sh missing" >&2
        return 1
    fi
    . /tmp/emsdk/emsdk_env.sh
    sh web/emu/build.sh
    node web/emu/emu_test.mjs build/emu/felucca.wasm build/host/emu_native.f32
}

run_security() {
    python3 tests/install_test.py
    node web/test_backup.mjs
    if [ -f build/felucca.fwsc ]; then node web/test_pkg_bounds.mjs build/felucca.fwsc; fi
    san="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow -fno-sanitize-recover=undefined -w"
    cc $san -Ibuild/gen -Ifirmware/src -o build/uifix/audio_san tests/audio_test.c -lm
    ASAN_OPTIONS=detect_leaks=0 ./build/uifix/audio_san
}

run_stress() {
    tests/stress_run.sh "${2:-1}"
}

run_target() {
    if [ ! -d /tmp/jieli/toolchain ] || [ ! -d /tmp/ac79 ]; then
        echo "target: JieLi toolchain or AC79 SDK missing" >&2
        return 1
    fi
    JIELI_TOOLCHAIN=/tmp/jieli/toolchain AC79_SDK=/tmp/ac79 python3 tools/build.py
    python3 tests/target_budget.py
}

case "$group" in
    ui) run_ui ;;
    presets) run_presets ;;
    web) run_web ;;
    emu) run_emu ;;
    security) run_security ;;
    stress) run_stress ;;
    target) run_target ;;
    all)
        run_ui
        run_presets
        run_web
        run_emu
        run_security
        run_target
        ;;
    *)
        echo "usage: $0 ui|presets|web|emu|security|stress|target|all" >&2
        exit 2
        ;;
esac
