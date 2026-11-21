#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Stress pass over the storage, project, robustness and MIDI paths with ASan/UBSan, each binary run
# N times (default 3). Simulated flash only: no board, no physical write. Run from the repo root after
# the tables exist (python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()").
set -u
N=${1:-3}
OUT=build/stress
CC=${CC:-cc}
mkdir -p "$OUT" || exit 1
fails=0
for t in project_test persistence_test robust_test midi_control_test; do
    if ! $CC -O1 -g -w -fsanitize=address,undefined -fno-omit-frame-pointer \
        -Ibuild/gen -Ifirmware/src -o "$OUT/$t" "tests/$t.c" -lm; then
        echo "stress: $t did not build"; fails=$((fails+1)); continue
    fi
    i=0
    while [ "$i" -lt "$N" ]; do
        if ! ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$OUT/$t" >"$OUT/$t.$i.log" 2>&1; then
            echo "stress: $t failed on run $i (log $OUT/$t.$i.log)"; fails=$((fails+1)); break
        fi
        i=$((i+1))
    done
    [ "$i" -eq "$N" ] && echo "stress: $t $N runs ok"
done
# long golden pass: the whole library and every mix again, twice
if ! $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm; then
    echo "stress: regress did not build"; exit 1
fi
i=0
while [ "$i" -lt 2 ]; do
    # (ETAPA 7) no ns baseline inside the stress loop: this script is the load. The CPU budget is a wall-clock
    # measurement of an idle host (renice-sensitive, +-25 %) and a pass that runs while four sanitizer binaries
    # are still warm cannot measure it -- it reported an idle scene over budget that three standalone runs put at
    # 0. The host/target runs check the budget; here the pass is for crashes, ASan and identical goldens.
    if ! CPU_NS_BASE=/dev/null "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt >"$OUT/regress.$i.log" 2>&1; then
        echo "stress: regress failed on run $i"; fails=$((fails+1)); break
    fi
    i=$((i+1))
done
[ "$i" -eq 2 ] && echo "stress: regress 2 full passes ok"
[ "$fails" -eq 0 ] || exit 1
echo "stress: all ok"
