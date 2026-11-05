/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* clamp_sym(v, k) is the same function as clamp(v, -k, k), for every int32 v.
 *
 * The SVF (dsp.c tsvf_lp) clamps its two states this way twice per sample and voice, so the
 * formulation is on the hot path of every ANALOG, NOISE and DRUM-X voice. The one-compare form
 * is only worth it if it is exactly the old one: this test compares the firmware's own
 * functions, not a copy of them, over the whole neighbourhood of the bound, the two extremes of
 * int32 (where a signed v + k would overflow, and the unsigned sum does not), a dense sweep of
 * 5 000 001 values, and 20 000 000 pseudorandom ones from a fixed seed. It also checks the
 * bounds themselves: +-k are returned unchanged, +-(k + 1) saturate.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o clamp_sym_test tests/clamp_sym_test.c -lm
 *   ./clamp_sym_test
 */
#include <stdio.h>
#include <stdint.h>

#include "felucca_tables.h"
#include "core.h"                                   /* CTL, vmod_t: what dsp.c is written against */
#include "dsp.c"

static int bad;
static uint32_t rng = 2463534242u;                    /* xorshift32, fixed seed: reproducible */

static uint32_t next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void same(int32_t v, int32_t k)
{
    int32_t a = clamp(v, -k, k), b = clamp_sym(v, k);
    if (a != b && bad < 10)
        printf("clamp_sym: %d at k=%d: clamp %d, clamp_sym %d\n", v, k, a, b);
    bad += a != b;
}

static void check(const char *w, int ok)
{
    printf("clamp_sym: %-62s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(void)
{
    const int32_t K = 150000;                          /* the SVF's bound */
    int32_t v, k;
    uint32_t i;

    for (v = -K - 4; v <= K + 4; v++)
        same(v, K);
    check("every value around +-150000, the bound included, is identical", bad == 0);

    for (i = 0; i < 200000u; i++)                      /* int64 counter: v++ past INT32_MAX would be UB */
        same((int32_t)((int64_t)INT32_MIN + (int64_t)i), K);
    for (i = 0; i < 200000u; i++)
        same((int32_t)((int64_t)INT32_MAX - (int64_t)i), K);
    check("both ends of int32 saturate the same way (no signed overflow)", bad == 0);

    for (v = -2500000; v <= 2500000; v++)
        same(v, K);
    check("5 000 001 values around the bound, dense", bad == 0);

    for (i = 0; i < 20000000u; i++)
        same((int32_t)next(), K);
    check("20 000 000 pseudorandom int32 values", bad == 0);

    for (k = 1; k < 300000; k += 997)                  /* other bounds: same identity, no wrap */
        for (i = 0; i < 4000u; i++) {
            int32_t x = (int32_t)next() % (4 * k) - 2 * k;
            same(x, k);
            same(x ^ 0x7FFFFFFF, k);
        }
    check("other bounds (1, 998, ... 298 990): the identity holds", bad == 0);

    {   /* the bounds themselves: +-k pass, +-(k+1) saturate, and the sign picks the bound */
        int ok = clamp_sym(K, K) == K && clamp_sym(-K, K) == -K && clamp_sym(K + 1, K) == K &&
                 clamp_sym(-K - 1, K) == -K && clamp_sym(K - 1, K) == K - 1 && clamp_sym(0, K) == 0;
        check("+-k unchanged, +-(k+1) saturate, 0 unchanged", ok);
    }
    {   /* no value escapes the bound, which is what keeps the state products in 32 bits */
        int ok = 1;
        for (i = 0; i < 2000000u; i++) {
            int32_t y = clamp_sym((int32_t)next(), K);
            if (y > K || y < -K)
                ok = 0;
        }
        check("2 000 000 random states stay inside +-150000", ok);
    }

    printf(bad ? "clamp_sym: %d FAILED\n" : "clamp_sym: all passed\n", bad);
    return bad != 0;
}
