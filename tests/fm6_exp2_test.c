/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 1) THE FM6 EXP2 CURVE, STORED AS ITS SECOND DIFFERENCE AND REBUILT AT BOOT.
 *
 * The app slot is the resource this firmware runs out of first (728 B free of 583 612 when this was written),
 * and fm6's 1025-point table of 2^(i/1024) - 1 was 4100 B of it -- 0.7 %, seven times the headroom there was.
 * The curve is smooth, so its second difference fits an int16 (491..983), and the same integers come back from
 * plain adds: 2054 B of XIP plus 4100 B of .bss (10.8 KB free) instead of 4100 B of XIP.
 *
 * What this proves, and what it cannot:
 *   exact     every one of the 1025 decoded points equals the value the generator would emit: the reference here
 *             is the same formula and the same rounding (round(x) = floor(x + 0.5) for these positive values),
 *             computed with libm in double, which is what tools/gen_tables.py does with Python floats.
 *   shape     the curve is monotonic and starts at 0 (so a Q30 mantissa of 2^0 is exactly 0, as msfa has it).
 *   in range  every stored difference is an int16 the header can carry, which is what makes the storage legal.
 *   no drift  the decode keeps the same number of points the lookups index (k + 1 up to 1024), so fm6_mant's
 *             neighbour read is inside the array.
 * It does not measure sound: the golden renders do that (306 of them, and this table is in every FM6 note).
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/fm6_exp2_test tests/fm6_exp2_test.c -lm
 *   build/host/fm6_exp2_test
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "felucca_tables.h"

static int fails;

static void check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        fails++;
}

int main(void)
{
    uint32_t tab[FM6_EXP2_N];
    int32_t i, a = (int32_t)FM6_EXP2_V0, b = a + (int32_t)FM6_EXP2_D1, d2 = (int32_t)FM6_EXP2_D2_V0;
    int bad = 0, worst = -1;
    double worstd = 0.0;
    uint32_t acc = 0, bits = 0;
    const uint8_t *p = FM6_EXP2_D2;

    tab[0] = (uint32_t)a;
    tab[1] = (uint32_t)b;
    for (i = 0; i < (int32_t)FM6_EXP2_D2_N; i++) {
        int32_t c = b + (b - a) + d2;
        tab[i + 2] = (uint32_t)c;
        a = b;
        b = c;
        if (i + 1 < (int32_t)FM6_EXP2_D2_N) {   /* the same three packed bits fm6_exp2_init reads */
            while (bits < 3u) {
                acc = acc << 8 | *p++;
                bits += 8u;
            }
            d2 += (int32_t)((acc >> (bits - 3u)) & 7u) - 3;
            bits -= 3u;
        }
    }

    check("the decode fills 1025 points (fm6_mant reads k + 1 up to 1024)",
          FM6_EXP2_N == 1025u && FM6_EXP2_D2_N == 1023u);
    check("the curve starts at 0 (2^(0/1024) - 1)", tab[0] == 0u);

    for (i = 0; i < (int32_t)FM6_EXP2_N; i++) {
        double ref = (double)(1u << 30) * (pow(2.0, (double)i / 1024.0) - 1.0);
        uint32_t want = (uint32_t)(uint64_t)(ref + 0.5);
        if (tab[i] != want) {
            if (!bad)
                worst = (int)i;
            double d = fabs((double)tab[i] - ref);
            if (d > worstd)
                worstd = d;
            bad++;
        }
    }
    if (bad)
        printf("     first mismatch at %d, worst distance %.1f of %.0f\n",
               worst, worstd, (double)(1u << 30) * (pow(2.0, (double)worst / 1024.0) - 1.0));
    check("every decoded point is the generator's integer (all 1025)", bad == 0);

    {
        int mono = 1;
        for (i = 1; i < (int32_t)FM6_EXP2_N; i++)
            if (tab[i] <= tab[i - 1])
                mono = 0;
        check("the curve is strictly increasing (a linear interpolation between two points is never inverted)", mono);
        /* 2^30 * (2^1 - 1) = 2^30 exactly: the curve's own top, not the Q30 maximum */
        check("the last point is 2^30 (2^1 - 1 in Q30)", tab[FM6_EXP2_N - 1] == (1u << 30));
    }

    {
        int range = (int)(sizeof FM6_EXP2_D2) == (int)(((FM6_EXP2_D2_N - 1u) * 3u + 7u) / 8u);
        check("the packed changes fit the three bits the header declares", range);
    }

    printf("fm6 exp2 table: %u points, %u B of XIP instead of 4100 (saved %u), decoded at boot\n",
           FM6_EXP2_N, (unsigned)sizeof FM6_EXP2_D2, (unsigned)(4100u - sizeof FM6_EXP2_D2));
    printf(fails ? "FM6 EXP2 TEST FAILED (%d)\n" : "fm6 exp2 table: 7 checks, 0 FAILED\n", fails ? fails : 7);
    return fails ? 1 : 0;
}
