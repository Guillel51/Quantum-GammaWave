#!/usr/bin/env bash
# Split the long Quantum GammaWave suite into reproducible groups.
# Usage: tests/groups.sh live|ui|presets|web|emu|sync|target|security|stress|browser|all
# (`live` reads the published site over HTTPS and holds it to this build's commit and SHA-256; it is not
#  part of `all`, and exits 2 when there is no route, which is "not run here" and never a pass.)
# Each group keeps its own exit status; no assertion or budget is changed.
set -eu
cd "$(dirname "$0")/.."

group=${1:-all}
mkdir -p build/uifix

run_six() {
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/six_track_test tests/six_track_test.c -lm
    ./build/uifix/six_track_test
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/six_track_live_test tests/six_track_live_test.c -lm
    ./build/uifix/six_track_live_test
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
    python3 tests/make_site_name_test.py
    node web/test_web.mjs
    node web/test_mid.mjs
}

run_browser() {
    # Chromium (playwright's build) loads build/emu as a page: the WASM the page receives must be the
    # manifest's, the console clean, and the worklet must render sound. Missing browser or system
    # libraries exit 2, which is "not run here", never a pass.
    if [ -d "$HOME/.cache/ms-playwright" ]; then
        LD_LIBRARY_PATH="$PWD/build/sysroot/usr/lib/x86_64-linux-gnu:$PWD/build/apt/root/usr/lib/x86_64-linux-gnu" \
            node web/emu/browser_test.mjs build/emu
    else
        echo "browser: playwright's Chromium is not installed (npx playwright install chromium)" >&2
        return 2
    fi
}

run_sync() {
    python3 tests/emu_sync.py
}

run_emu() {
    # emcc is wherever the environment says it is: PATH, $EMSDK, ~/emsdk, /opt/emsdk or /tmp/emsdk (the older
    # layout). tools/emu_env.sh shares the search with web/emu/build.sh and tests/run_tests.sh, and adds
    # emsdk's own directories because its emsdk_env.sh fails silently when sourced by dash.
    # (2.16) NO REBUILD IS NOT A PASS: this used to test whatever WASM was lying in build/emu and call that
    # green, which is a stale emulator reported as the current tree. It is a skip now (exit 2), the same
    # "not run here" the browser group uses.
    # shellcheck source=../tools/emu_env.sh
    . tools/emu_env.sh
    if emu_find_emcc; then
        sh web/emu/build.sh
    else
        echo "emu: emcc unavailable, so the WASM cannot be rebuilt from these sources: group SKIPPED" >&2
        return 2
    fi
    # the native reference the WASM is compared against bit for bit: tests/run_tests.sh builds it, and a
    # group run on a clean tree failed here on a missing file instead of on a wrong emulator. Build it
    # from this tree, so the parity claim is about these sources.
    mkdir -p build/host
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/emu_native web/emu/native_check.c -lm || return 1
    ./build/host/emu_native build/host/emu_native.f32 || return 1
    node web/emu/emu_test.mjs build/emu/felucca.wasm build/host/emu_native.f32
    run_sync
}

run_eq() {
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/eq_sweep_test tests/eq_sweep_test.c -lm
    ./build/uifix/eq_sweep_test
    # the same filters measured through qg_biquad_process(), i.e. the audio path the mixer
    # uses: a filter that is stable on paper but wrong at the corner cannot pass this one
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/eq_response_test tests/eq_response_test.c -lm
    ./build/uifix/eq_response_test
    # the four delay states one by one (a bad scale on one of them cost the right channel its EQ)
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/eq_state_test tests/eq_state_test.c -lm
    ./build/uifix/eq_state_test
    # what the DC gain really is: coefficients vs processing vs truncation
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/uifix/eq_dc_test tests/eq_dc_test.c -lm
    ./build/uifix/eq_dc_test
}

run_security() {
    python3 tests/install_test.py
    python3 tests/preflight_test.py
    # what M-Flasher itself will check before it erases anything, on the committed release
    if [ -f tests/fixtures/felucca-v1.9.0.fwsc ]; then python3 tests/mflasher_accept_test.py; fi
    python3 tests/gate_test.py
    python3 tests/sign_test.py
    python3 tests/key_custody_test.py
    node web/test_backup.mjs
    python3 tests/restore_test.py
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
    # the variables win; the /tmp paths are the old CI layout and stay as the fallback, so a machine that
    # exports the toolchain (AGENTS.md §6) does not have to copy it to /tmp just to run this group
    tl=${JIELI_TOOLCHAIN:-/tmp/jieli/toolchain}
    sdk=${AC79_SDK:-/tmp/ac79}
    if [ ! -d "$tl" ] || [ ! -d "$sdk" ]; then
        echo "target: JieLi toolchain or AC79 SDK missing (JIELI_TOOLCHAIN=$tl AC79_SDK=$sdk)" >&2
        return 1
    fi
    JIELI_TOOLCHAIN=$tl AC79_SDK=$sdk python3 tools/build.py
    python3 tests/target_budget.py
    python3 tests/memory_map_test.py
}

# (FASE 8) THE PUBLISHED SITE, FROM OUTSIDE THE TREE. Every other group reads the checkout; a Pages
# deploy that failed, a cache serving the previous artefact or a versions.json from another run all look
# fine from inside it. This reads the live origin and holds it to THIS build: the commit the package was
# built from, the package's SHA-256, and the emulator record the site ships -- a 404 on the package, or a
# hash that is not this release's, is a failure here.
# Offline is exit 2 ("not run here"), never a pass; once the origin answers, site_check.py's own rule
# applies unchanged: a site whose files cannot be read is not a site that can be trusted.
run_live() {
    ver=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
    rec="release/build-manifest-${ver}.json"
    pkg="release/felucca-${ver}.fwsc"
    if [ ! -f "$rec" ] || [ ! -f "$pkg" ]; then
        echo "live: ${rec} or ${pkg} is missing: run tools/make_release.py --sync first" >&2
        exit 2
    fi
    commit=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["commit"])' "$rec")
    sha=$(sha256sum "$pkg" | cut -d' ' -f1)
    emu=$(python3 -c 'import json,sys; print((json.load(open(sys.argv[1])) or {}).get("commit", ""))' \
        site/emulator/manifest.json 2>/dev/null || true)
    url=${SITE_URL:-https://guillel51.github.io/Quantum-GammaWave/}
    if ! python3 - "$url" <<'PY'
import sys, urllib.error, urllib.request
url = sys.argv[1]
try:
    urllib.request.urlopen(url, timeout=15).read(1)
except urllib.error.HTTPError:
    sys.exit(0)                      # it answered (a 404 included): site_check.py has to judge that
except Exception:
    sys.exit(3)                      # no DNS, no route, no TLS: not this machine's failure to prove
PY
    then
        echo "live: no route to $url (exit 2: not run here)" >&2
        exit 2
    fi
    echo "live: $url held to build $commit, package $sha ($ver)"
    # shellcheck disable=SC2086
    python3 tools/site_check.py --url "$url" --expect-commit "$commit" --expect-sha256 "$sha" \
        ${emu:+--expect-emu-commit "$emu"}
}

case "$group" in
    live) run_live ;;
    ui) run_ui ;;
    six) run_six ;;
    presets) run_presets ;;
    web) run_web ;;
    emu) run_emu ;;
    sync) run_sync ;;
    browser) run_browser ;;
    security) run_security ;;
    stress) run_stress ;;
    target) run_target ;;
    eq) run_eq ;;
    all)
        run_ui
        run_presets
        run_web
        run_emu
        run_eq
        run_target
        run_security
        run_browser
        ;;
    *)
        echo "usage: $0 live|ui|presets|web|emu|sync|eq|security|stress|target|browser|all" >&2
        exit 2
        ;;
esac
