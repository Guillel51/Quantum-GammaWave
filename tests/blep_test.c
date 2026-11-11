/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The polyBLEP residual's one-compare fast path is exactly the reference, for every input.
 *
 * dsp.c blep() used to test the two wrap windows and fall through to `return 0`; the fast path
 * returns 0 first, before the shift and the division, because almost every sample is outside
 * both windows. The saving is only real if the function is the same function: this test compares
 * the firmware's own blep() against the reference formulation over the edges of both windows
 * (ph at 0, inc - 1, inc, inc + 1, ~inc - 1, ~inc, ~inc + 1, the ends of uint32), every
 * increment that can occur (0, 1, the sub-1 Hz incs whose d is 0, the audio range, and the
 * increments past Nyquist where the two windows overlap), a dense sweep of the phase for each,
 * and 10 000 000 pseudorandom pairs from a fixed seed.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o blep_test tests/blep_test.c -lm
 *   ./blep_test
 */
#include <stdio.h>
#include <stdint.h>

#include "felucca_tables.h"
#include "core.h"                                   /* CTL, vmod_t: what dsp.c is written against */
#include "dsp.c"

static int bad;
static long checked;
static uint32_t rng = 2463534242u;                    /* xorshift32, fixed seed: reproducible */

static uint32_t next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* dsp.c blep() before the fast path: the wrap windows are tested first, then the residual */
static int32_t blep_ref(uint32_t ph, uint32_t inc)
{
    uint32_t d = inc >> 15;
    int32_t x;
    if (!d)
        return 0;
    if (ph < inc) {
        x = (int32_t)(ph / d);                       /* 0..32767 */
        return x + x - ((x * x) >> 15) - 32768;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        x = -(int32_t)((0xFFFFFFFFu - ph) / d);       /* -32767..0 */
        return ((x * x) >> 15) + x + x + 32768;
    }
    return 0;
}

static void same(uint32_t ph, uint32_t inc)
{
    int32_t a = blep_ref(ph, inc), b = blep(ph, inc);
    checked++;
    if (a != b && bad < 10)
        printf("blep: ph %u inc %u: reference %d, fast path %d\n", ph, inc, a, b);
    bad += a != b;
}

int main(void)
{
    static const uint32_t incs[] = {           /* 0 / sub-d: d = 0; the audio range; past Nyquist */
        0u, 1u, 2u, 100u, 32766u, 32767u, 32768u, 32769u,
        1u << 20, 1u << 24, 0x04000000u, 0x08000000u, 0x10000000u, 0x20000000u,
        0x3FFFFFFFu, 0x40000000u, 0x55555555u, 0x7FFFFFFEu, 0x7FFFFFFFu,
        0x80000000u, 0x80000001u, 0xC0000000u, 0xFFFFFFFEu, 0xFFFFFFFFu
    };
    size_t k;
    int i;
    for (k = 0; k < sizeof incs / sizeof incs[0]; k++) {
        uint32_t inc = incs[k], e[10];
        e[0] = 0u; e[1] = 1u; e[2] = inc - 1u; e[3] = inc; e[4] = inc + 1u;
        e[5] = ~inc - 1u; e[6] = ~inc; e[7] = ~inc + 1u;
        e[8] = 0x80000000u; e[9] = 0xFFFFFFFFu;
        for (i = 0; i < 10; i++) {
            same(e[i], inc);
            same(e[i] - 1u, inc);
            same(e[i] + 1u, inc);
        }
        {   /* a dense sweep of the phase around the whole accumulator for this increment */
            uint32_t ph = 0u, step = 7919u;              /* prime: hits both windows at every inc */
            for (i = 0; i < 200000; i++) {
                same(ph, inc);
                ph += step;
            }
        }
    }
    for (i = 0; i < 10000000; i++)
        same(next(), next());
    printf("blep: %ld pairs, %d differ from the reference\n", checked, bad);
    return bad != 0;
}
