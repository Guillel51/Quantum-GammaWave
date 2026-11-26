#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Fixed-point lookup tables for FELUCCA (no float on the target).

FS = 44100 Hz, control rate = every CTL (32) samples. All curves map a
0..127 parameter value to the unit the DSP needs, plus a display table.
"""
import math
import sys
from pathlib import Path

FS = 44100
CTL = 32


def arr(name, ctype, vals, per=12):
    out = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


def bits3(name, vals, base):
    """(FASE 2/5) eight values a 24-bit group, three bits each: vals[i] - base is 0..7. The decoder reads the same
    bits with shifts (dsp.c sine_init, fm6_core.c fm6_exp2_init); no rounding, no loss."""
    assert all(base <= v <= base + 7 for v in vals), f"{name}: a value does not fit three bits"
    out, acc, nb = [], 0, 0
    for v in vals:
        acc = (acc << 3) | (v - base)
        nb += 3
        if nb == 24:
            out += [acc >> 16, (acc >> 8) & 255, acc & 255]
            acc, nb = 0, 0
    if nb:
        acc <<= 24 - nb
        out += [acc >> 16, (acc >> 8) & 255, acc & 255]
    return arr(name, "uint8_t", out, 12)



# WAVETABLE (src/eng_wavetable.c): eight single-cycle frames, 256 points, three band limits. Every frame
# is a sum of sines of the cycle's harmonic k (phase 0: all of them cross zero at the cycle's start, so a
# morph between two frames never jumps at phase 0), normalised to the same peak so a morph keeps its level.
# LIMIT is the highest harmonic each level carries: 64 (notes below ~344 Hz: 22 kHz), 16 (below ~1.4 kHz)
# and 5 (above). Choosing the level by the note keeps the top of the keyboard free of aliasing.
WT_LIMIT = (64, 16, 5)
WT_POINTS = 256


def wt_frame(f, k):
    """frame f's harmonic k: amplitude (sign included); the frames are the classic shapes plus two vocal ones"""
    if f == 0:                                             # SINE
        return 1.0 if k == 1 else 0.0
    if f == 1:                                             # TRIANGLE: odd, 1/k^2, alternating
        return ((-1) ** ((k - 1) // 2)) / (k * k) if k % 2 else 0.0
    if f == 2:                                             # SAW
        return 1.0 / k
    if f == 3:                                             # SQUARE: odd, 1/k
        return 1.0 / k if k % 2 else 0.0
    if f == 4:                                             # PULSE 25 %
        return 2.0 / (math.pi * k) * math.sin(math.pi * k * 0.25)
    if f == 5:                                             # HOLLOW: odd, 1/k^3 (a soft clarinet)
        return 1.0 / (k ** 3) if k % 2 else 0.0
    if f == 6:                                             # METAL: 1 3 5 7 9 11 13, a slow 1/k slope
        return 1.0 / (k ** 0.7) if k in (1, 3, 5, 7, 9, 11, 13) else 0.0
    # VOCAL: two formant peaks over a 1/k slope (a vowel around the 3rd and the 9th harmonic)
    return (0.12 + math.exp(-((k - 3) / 1.6) ** 2) + 0.6 * math.exp(-((k - 9) / 2.6) ** 2)) / k


def wt_tables():
    L = ["/* (FASE 1, FASE 6) all three band limits are rebuilt at boot (eng_wavetable.c, wt_gen_init) from\n"
         " * WT_COEF, the frames' harmonic amplitudes in Q30: 2 048 B of flash and 6 192 B of .bss instead of\n"
         " * 12 288 B of flash. The stored table the first two levels replaced came out bit for bit the same;\n"
         " * level 2 (LIMIT 5, every note above ~1.4 kHz) was the last one left in flash (FASE 6).\n"
         " * (FASE 1 de v1.5.22) every frame is a sum of sines, so w[WT_POINTS - i] = -w[i] exactly: the .bss\n"
         " * keeps ONE half cycle per frame (WT_PTS_H = 129 points, w[0] and w[128] = 0) and eng_wavetable.c's\n"
         " * wt_pts() reads the other half backwards and negated. 6 096 B of .bss the RAM budget gets back. */"]
    # the amplitudes: WT_COEF[(k - 1) * WT_FRAMES + frame], Q30; the boot synthesis sums k = 1..limit
    coef = []
    for k in range(1, WT_LIMIT[0] + 1):
        coef += [int(round(wt_frame(f, k) * (1 << 30))) for f in range(8)]
    # (FASE 2/5, 2026-11-25) 232 of the 512 amplitudes are 0 (a harmonic a frame does not carry): a 64-byte bit
    # map and the nonzero values (1 120 B) instead of 2 048 B. wt_coef_init (eng_wavetable.c) expands the 512 into
    # .bss once, before the first wt_gen_init pass; the samples are the same integers.
    mask = [0] * (len(coef) // 8)
    nz = []
    for i, v in enumerate(coef):
        if v:
            mask[i >> 3] |= 1 << (i & 7)
            nz.append(v)
    L += ["#define WT_COEF_N %d" % len(coef)]
    L += arr("WT_COEF_MASK", "uint8_t", mask, 16)
    L += arr("WT_COEF_NZ", "int32_t", nz, 8)
    L += ["#define WT_FRAMES 8", "#define WT_POINTS 256", "#define WT_LEVELS 3",
          "#define WT_COEF_H 64",
          "#define WT_PTS_H 129    /* points 0..128 of a frame: the cycle's first half, the second is -it */",
          "static int16_t WT_GEN[3 * WT_FRAMES * WT_PTS_H];   /* .bss: levels 0, 1 and 2, half a cycle each */",
          "/* level 0: 64 harmonics, level 1: 16, level 2: 5 (all generated) */",
          "#define WT_HALF(l) (WT_GEN + (l) * WT_FRAMES * WT_PTS_H)",
          "#define WT_GET(l, f, i) WT_HALF(l)[(f) * WT_PTS_H + (i)]", ""]
    return L


def main(path):
    L = ["/* generated by tools/gen_tables.py */", "#pragma once", "#include <stdint.h>",
         f"#define FS {FS}", f"#define CTL {CTL}", ""]
    # (1025 points: SINE[i + 1] needs no wrap: FM6's operators; the others mask)
    # (ETAPA 2) the sine is stored as its SECOND DIFFERENCE too: the 1 025 points step by at most 201 and the step
    # itself changes by at most 3, so an int8 per point carries the whole curve (1 025 B of the app slot instead
    # of 2 050) and dsp.c's sine_init rebuilds the same integers at boot with adds, into .bss. Every caller reads
    # the RAM copy: the hot path gets a RAM read, not an XIP one.
    sine = [int(round(32767 * math.sin(2 * math.pi * i / 1024))) for i in range(1025)]
    sd1 = [sine[i + 1] - sine[i] for i in range(1024)]
    sd2 = [sd1[i + 1] - sd1[i] for i in range(1023)]
    L += ["#define SINE_N 1025", "#define SINE_D2_N 1023",
          "#define SINE_V0 %d" % sine[0], "#define SINE_D1 %d" % sd1[0]]
    # (FASE 2/5) the 1 023 second differences are -3..3, three bits each with +3: 384 B instead of 1 023 B; sine_init
    # (dsp.c) reads them back with shifts and gets the same integers (and so the same 1 025 points).
    L += bits3("SINE_D2", sd2, -3)
    # phase increment for pitch in 1/16 semitone, MIDI 0..127
    inc = []
    for p in range(128 * 16):
        f = 440.0 * 2 ** ((p / 16 - 69) / 12)
        inc.append(min(int(f / FS * 2 ** 32), 0x7FFFFFFF))
    # stored as its top octave (1856..2047) shifted down: floor(floor(x) / 2^k) = floor(x / 2^k), so
    # pitch_inc(p) equals the 2048-entry table bit for bit (checked here); 7.4 KB less flash
    oct_ = inc[1856:2048]
    assert all(inc[q] == oct_[(q + 64) % 192] >> (10 - (q + 64) // 192) for q in range(2048))
    assert all((q + 64) * 21846 >> 22 == (q + 64) // 192 for q in range(2048))
    L += arr("PITCH_OCT", "uint32_t", oct_, 8)
    L += ["/* phase increment of pitch p in 1/16 semitone, 0..2047 (MIDI 0..127): 440 Hz * 2^((p / 16 - 69) / 12) */",
          "static inline __attribute__((always_inline)) uint32_t pitch_inc(uint32_t p)",
          "{",
          "    uint32_t i = p + 64u, o = (i * 21846u) >> 22;     /* i / 192: the octave above PITCH_OCT's - 10 */",
          "    return PITCH_OCT[i - o * 192u] >> (10u - o);",
          "}"]
    # times 1 ms .. 10 s (exponential), env in Q24 (1.0 = 1<<24)
    ms = [1.0 * (10000 ** (v / 127)) for v in range(128)]
    L += arr("TIME_MS_X10", "uint32_t", [int(round(m * 10)) for m in ms], 10)
    L += arr("ENV_LIN", "uint32_t", [max(1, int((1 << 24) * CTL / (m * FS / 1000))) for m in ms], 8)
    exp_k = []
    for m in ms:
        tau = m / 1000 / 4.6                       # ~99 % of the way after "m" ms
        exp_k.append(max(1, min(65535, int(65536 * (1 - math.exp(-CTL / (tau * FS)))))))
    L += arr("ENV_EXP", "uint16_t", exp_k)
    # LFO 0.05 .. 40 Hz, phase increment per control tick
    hz = [0.05 * (800 ** (v / 127)) for v in range(128)]
    # (FASE 1, 2026-11-16) 5 .. 4000 (0.05 .. 40 Hz x 100): a uint16 holds every entry, 256 B less XIP. The
    # value read is identical (the callers widen it), so params.c's display and the LFO rate do not change.
    # (this phase) CHORUS RATE, 0.05 .. 6.00 Hz: the bus's rate capped at the owner's 6 Hz, exponential so the
    # low end stays musical. Its own pair of tables (768 B) because the LFO's runs to 40 Hz and the chorus must
    # never get there, whichever byte a preset, a CC or a restored project hands it.
    ch = [0.05 * ((6.00 / 0.05) ** (v / 127)) for v in range(128)]
    L += arr("CH_HZ_X100", "uint16_t", [int(round(h * 100)) for h in ch], 10)
    L += arr("CH_INC", "uint32_t", [int(h * CTL / FS * 2 ** 32) for h in ch], 8)
    L += arr("LFO_HZ_X100", "uint16_t", [int(round(h * 100)) for h in hz], 10)
    L += arr("LFO_INC", "uint32_t", [int(h * CTL / FS * 2 ** 32) for h in hz], 8)
    # filter cutoff 30 Hz .. 16 kHz
    fc = [30.0 * ((16000 / 30) ** (v / 127)) for v in range(128)]
    L += arr("CUTOFF_HZ", "uint16_t", [int(round(f)) for f in fc])
    # master EQ cutoff: 20 Hz .. 20 kHz, the axis the MASTER EQ graph draws
    eq_fc = [20.0 * (1000.0 ** (v / 127)) for v in range(128)]
    L += arr("EQ_HZ", "uint16_t", [int(round(f)) for f in eq_fc])
    # master EQ gain law: A = 10^(dB / 40), sqrt(A) = 10^(dB / 80), Q12. The old
    # linear polynomial was 18 dB at +24 and clamped at -24: the graph and the
    # audio filter did not reach the gain the parameter asked for.
    eq_gain_db = range(-24, 25)
    L += arr("EQ_A_Q12", "uint16_t", [int(round(4096 * 10 ** (g / 40))) for g in eq_gain_db])
    L += arr("EQ_SQRT_A_Q12", "uint16_t", [int(round(4096 * 10 ** (g / 80))) for g in eq_gain_db])
    # trapezoidal (Simper) SVF: g = tan(pi fc / FS), Q12; stable at any cutoff/resonance
    L += arr("SVF_G", "uint16_t", [int(4096 * math.tan(math.pi * min(f, 0.45 * FS) / FS)) for f in fc])
    # level: 0 = off, else dB = (v - 112) / 2  (112 = 0 dB, 127 = +7.5 dB)
    db = [None] + [(v - 112) / 2 for v in range(1, 128)]
    L += arr("LEVEL_Q12", "uint16_t", [0] + [int(round(4096 * 10 ** (d / 20))) for d in db[1:]])
    L += arr("LEVEL_DB_X10", "int16_t", [-999] + [int(round(d * 10)) for d in db[1:]])
    # per-sample exponential decay for drum voices: 5 ms .. 4 s to -60 dB, Q16
    dms = [5.0 * (800 ** (v / 127)) for v in range(128)]
    L += arr("DECAY_K", "uint16_t", [min(65535, int(65536 * math.exp(-6.9 / (m / 1000 * FS)))) for m in dms])
    # soft clip curve for |x| in 0..2 (Q12 in, Q15 out): tanh
    L += arr("TANH_Q15", "int16_t", [int(32767 * math.tanh(i / 256 * 2)) for i in range(257)])
    L += fm6_tables()
    L += wt_tables()
    Path(path).write_text("\n".join(L) + "\n")


# FM6 (src/fm6_core.c, a C port of msfa, Apache-2.0): the tables msfa built with double math at start-up,
# precomputed here so the firmware needs no float. CTL-sample blocks at 44.1 kHz.
FM6_LFO_SOURCE = [   # msfa lfo.cc: the LFO rates 0..99 (Hz)
    0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
    1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
    3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
    4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
    6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
    7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
    9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
    16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
    25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
    38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084]


def fm6_tables():
    L = ["/* FM6 (fm6_core.c): msfa's start-up tables, precomputed */",
         f"#define FM6_LFO_UNIT {int(CTL * 25190424 / FS + 0.5)}     /* msfa Lfo::unit_ */",
         f"#define FM6_PENV_UNIT {int(CTL * (1 << 24) / (21.3 * FS) + 0.5)}    /* msfa PitchEnv::unit_ */",
         f"#define FM6_FREQ_R {int(round(2 ** 40 / FS))}u   /* 2^40 / FS: the Q30 mantissa -> a Q24 phase increment (fm6_freq) */",
         f"#define FM6_AMS_C0 {int(round(12.2 * math.log2(math.e) * 2 ** 24))}   /* log2(e^12.2), Q24 (the AMS curve) */",
         f"#define FM6_AMS_K {int(round(0.07 / 262144 * math.log2(math.e) * 2 ** 24 * 1024))}      /* d/dsensamp of it, Q24 / 1024 */"]
    # 2^(i / 1024) - 1, Q30: the mantissa of Exp2::lookup and Freqlut::lookup (linear between the points, as
    # msfa's: within 6e-8 of 2^x; a Q16 table drifted the partials of a ratio against each other)
    # (ETAPA 1) THE SAME POINTS, STORED AS A SECOND DIFFERENCE. 2^(i/1024) - 1 is smooth, so its second
    # difference is 491..983 for every one of the 1025 points and fits an int16: the table of 4100 B of XIP
    # becomes 2054 B, and fm6_core.c rebuilds the exact integers at boot with plain adds (fm6_exp2_init),
    # keeping them in .bss where a lookup is cheaper than an XIP read. Nothing about the sound moves: the
    # decode is integer-exact, and tests/fm6_exp2_test.c compares all 1025 points against the formula.
    exp2 = [int(round((1 << 30) * (2 ** (i / 1024) - 1))) for i in range(1025)]
    d1 = [exp2[i + 1] - exp2[i] for i in range(1024)]
    d2 = [d1[i + 1] - d1[i] for i in range(1023)]
    L += ["#define FM6_EXP2_V0 %du     /* 2^(0/1024) - 1, Q30 */" % exp2[0],
          "#define FM6_EXP2_D1 %du     /* the first step: the table is V0, V0 + D1, then the curve's d2 */" % d1[0],
          "#define FM6_EXP2_N %du" % len(exp2),
          "#define FM6_EXP2_D2_N %du" % len(d2),
          "#define FM6_EXP2_D2_V0 %d" % d2[0]]
    # (FASE 2/5) the 1 023 second differences are smooth (491..983): its own first difference is -3..4, three bits
    # each with +3 (384 B instead of 2 046 B). fm6_exp2_init (fm6_core.c) walks them with a shift and rebuilds the
    # same d2, so the exp2 curve and every FM6 render stay bit for bit the same.
    L += bits3("FM6_EXP2_D2", [d2[i] - d2[i - 1] for i in range(1, len(d2))], -3)
    lforatio = int(4437500000.0 * CTL / FS)                                  # msfa Lfo::lforatio_ (uint32)
    L += arr("FM6_LFO_DELTA", "uint32_t", [int(s * lforatio) for s in FM6_LFO_SOURCE], 8)
    # msfa note osc_freq: the DETUNE step of note mn, Q24 log2 (0.0209 * exp(-0.396 * lf) / 7 * lf, lf in octaves)
    base, step = 50857777, (1 << 24) // 12
    det = []
    for mn in range(128):
        lf = base + step * mn
        det.append(int(round(0.0209 * math.exp(-0.396 * (lf / (1 << 24))) / 7 * lf)))
    L += arr("FM6_DETUNE", "uint16_t", det, 16)
    # .. and its FINE: (1 << 24) / ln 2 * ln(1 + fine / 100), Q24 log2
    L += arr("FM6_FINE", "int32_t", [int(math.floor(24204406.323123 * math.log(1 + 0.01 * f) + 0.5)) for f in range(100)], 10)
    return L
    print(f"tables: {path}")


if __name__ == "__main__":
    main(sys.argv[1])
