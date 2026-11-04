#!/usr/bin/env bash
# Split the long Quantum GammaWave suite into reproducible groups.
# Usage: tests/groups.sh ui|presets|web|emu|sync|target|security|stress|all
# Each group keeps its own exit status; no assertion or budget is changed.
set -eu
cd "$(dirname "$0")/.."

group=${1:-all}
mkdir -p build/uifix

run_six() {
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/six_track_test tests/six_track_test.c -lm
    ./build/uifix/six_track_test
}
run_ui() {
    cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/uifix/semi_test tests/semi_test.c -lm
    ./build/uifix/semi_test
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
    # Never let a failed built-site check fall through to the source-only check.
    if [ -d build/site ]; then
        python3 web/test_site.py build/site
    else
        python3 web/test_site.py
    fi
    python3 web/test_i18n.py
    python3 web/test_installer_i18n.py
    node web/test_web.mjs
}

run_sync() {
    python3 tests/emu_sync.py
}

run_emu() {
    if [ -f /tmp/emsdk/emsdk_env.sh ]; then
        . /tmp/emsdk/emsdk_env.sh
        sh web/emu/build.sh
    else
        echo "emu: emcc unavailable; testing the existing packaged WASM (not a rebuild)" >&2
        [ -f build/emu/felucca.wasm ] || return 1
    fi
    node web/emu/emu_test.mjs build/emu/felucca.wasm build/host/emu_native.f32
    run_sync
}

run_eq() {
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/eq_sweep_test tests/eq_sweep_test.c -lm
    ./build/uifix/eq_sweep_test
}

run_security() {
    python3 tests/install_test.py
    python3 tests/preflight_test.py
    python3 tests/gate_test.py
    node web/test_backup.mjs
    if [ -f build/felucca.fwsc ]; then node web/test_pkg_bounds.mjs build/felucca.fwsc; node web/test_pkg_faults.mjs build/felucca.fwsc; fi
    san="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow -fno-sanitize-recover=undefined -w"
    cc $san -Ibuild/gen -Ifirmware/src -o build/uifix/audio_san tests/audio_test.c -lm
    ASAN_OPTIONS=detect_leaks=0 ./build/uifix/audio_san
    cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -w -Ibuild/gen -Ifirmware/src -o build/uifix/fuzz_ed tests/fuzz_ed.c -lm
    cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -w -Ibuild/gen -Ifirmware/src -o build/uifix/fuzz_proj tests/fuzz_proj.c -lm
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/uifix/fuzz_ed 20000 7
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/uifix/fuzz_proj 5000 13
    # power-cut matrices: every persistent object's A/B write, and the OTA staging area
    cc -O1 -w -o build/uifix/storage_cut_test tests/storage_cut_test.c
    ./build/uifix/storage_cut_test
    if [ -f build/felucca.fwsc ]; then
        cc -O1 -w -o build/uifix/ota_cut_test tests/ota_cut_test.c
        ./build/uifix/ota_cut_test build/felucca.fwsc
    fi
    run_eq
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
    python3 tests/memory_map_test.py
}

case "$group" in
    ui) run_ui ;;
    six) run_six ;;
    presets) run_presets ;;
    web) run_web ;;
    emu) run_emu ;;
    sync) run_sync ;;
    security) run_security ;;
    stress) run_stress ;;
    target) run_target ;;
    all)
        run_ui
        run_presets
        run_web
        run_emu
        run_target
        run_security
        ;;
    *)
        echo "usage: $0 ui|presets|web|emu|sync|security|stress|target|all" >&2
        exit 2
        ;;
esac
