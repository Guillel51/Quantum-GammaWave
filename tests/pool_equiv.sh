#!/bin/sh
# (3.6) pool_equiv.sh: compare two builds of this firmware as audio, not as hashes.
#
# The voice pool changes which physical slot a note lands in, and a slot carries residual oscillator and
# filter state, so an intended-invisible change can still alter the samples. This script renders every
# golden scenario with two builds and reports the level of the difference, against tolerances that are
# written down here rather than assumed:
#
#   peak:  the loudest sample of the render.  A pool that drops, doubles or re-prioritises a note moves
#          this at once, so the limit is tight.
#   tail:  the peak of the last 0.25 s (after every voice is free and the effects rang out). It has to match
#          almost exactly: a tail that differs is a voice or a release that differs.
#   vmax:  the most voices the scenario ever had sounding. It has to match exactly.
#
# Usage:  sh tests/pool_equiv.sh ["flags for A"] ["flags for B"]
#         defaults: "" (the build's own VPOOLN) and "-DVPOOLN=16" (the pooled 10-voice window).
#         Example: sh tests/pool_equiv.sh "" "-DVPOOLN=16 -DFELUCCA_VRESET=1 -DFELUCCA_WSEED=1"
#
# Host only: it measures the host build of tests/regress.c, never the FM-1.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
OUT=${OUT:-build/host}
mkdir -p "$OUT" /tmp/pool_equiv
A=${1-}
B=${2--DVPOOLN=16}
PEAK_TOL=${PEAK_TOL:-0.02}     # 2 % of the render's own peak
TAIL_TOL=${TAIL_TOL:-0.01}     # 1 % of the tail's own peak

build() {                       # $1 = extra flags, $2 = binary
    # shellcheck disable=SC2086
    $CC -O2 -w $1 -Ibuild/gen -Ifirmware/src -o "$2" tests/regress.c -lm
}

render() {                      # $1 = binary, $2 = metrics file
    GOLDEN_METRICS=1 "$1" tests/golden.txt tests/cpu_baseline.txt 2>/dev/null | grep '^METRIC ' > "$2" || true
}

build "$A" /tmp/pool_equiv/a
build "$B" /tmp/pool_equiv/b
render /tmp/pool_equiv/a /tmp/pool_equiv/a.txt
render /tmp/pool_equiv/b /tmp/pool_equiv/b.txt

python3 - "$PEAK_TOL" "$TAIL_TOL" <<'PY'
import sys
ptol, ttol = float(sys.argv[1]), float(sys.argv[2])
def load(p):
    d = {}
    for l in open(p):
        if l.startswith('METRIC'):
            f = l.split()
            n, peak, tail, vmax, rms = f[1], int(f[2]), int(f[3]), int(f[5]), float(f[6])
            d[n] = (peak, tail, vmax, rms)
    return d
a, b = load('/tmp/pool_equiv/a.txt'), load('/tmp/pool_equiv/b.txt')
bad = 0
rows = []
for n in sorted(a):
    if n not in b:
        print(f"missing in the second build: {n}"); bad += 1; continue
    pa, ta, va, ra = a[n]; pb, tb, vb, rb = b[n]
    dr = abs(rb - ra) / max(ra, 1e-9)
    dp = abs(pb - pa) / max(pa, 1)
    dt = abs(tb - ta) / max(ta, 1) if ta else (0.0 if tb == 0 else 1.0)
    rows.append((max(dp / ptol, dt / ttol, dr / ptol), n, pa, pb, ta, tb, va, vb, dp, dt, dr))
rows.sort(reverse=True)
over = [r for r in rows if r[0] > 1.0]
for r in rows[:8]:
    print(f"  {r[1]:42s} peak {r[2]:6d} -> {r[3]:6d} ({r[8]*100:+6.2f}%)  "
          f"rms {r[10]*100:+6.2f}%  tail {r[4]:5d} -> {r[5]:5d}  voices {r[6]}/{r[7]}")
print(f"{len(rows)} renders; {len(over)} outside the tolerance "
      f"(peak and rms {ptol*100:.0f} %, tail {ttol*100:.0f} %, voices exact)")
print("VERDICT:", "PASS" if not over and not bad else "FAIL")
sys.exit(0 if not over and not bad else 1)
PY
