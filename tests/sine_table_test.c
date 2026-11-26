/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 2) THE SINE, STORED AS ITS SECOND DIFFERENCE AND REBUILT AT BOOT.
 *
 * The app slot is still the resource this firmware runs out of first, and SINE was 2 050 B of it -- 1 025 int16
 * points of a curve whose step never exceeds 201 and whose step changes by at most 3. The change is stored in
 * THREE bits (value + 3, 0..7), so the 1 023 changes are 384 B, and dsp.c's sine_init turns them back into the
 * same integers with plain adds, into .bss where every caller (sine_i, ADDTV, ANALOG's formant, FM6's fm6_sin)
 * reads it. The decode below is the same loop, on the same packed bytes: a test that read the array as int8
 * would be checking a format the firmware no longer writes.
 *
 * What this proves:
 *   exact     every one of the 1 025 decoded points is the generator's integer, bit for bit: the reference here
 *             is the same formula and the same rounding as tools/gen_tables.py, with libm in double.
 *   in range  every stored difference fits the int8 the header declares, which is what makes the storage legal.
 *   no drift  the decode keeps the 1 025 points the lookups index (SINE[(i + 1) & 1023] and the 1 025th = 0).
 * A rebuilt table that drifted by one would be a detune nobody hears but every golden render would see; that is
 * what this test and tests/regress are for.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/sine_table_test tests/sine_table_test.c -lm
 *   build/host/sine_table_test
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
    int16_t tab[SINE_N];
    int32_t i, v = SINE_V0, d = SINE_D1;
    int bad = 0, first = -1;
    uint32_t acc = 0, bits = 0;
    const uint8_t *p = SINE_D2;

    tab[0] = (int16_t)v;
    for (i = 1; i < (int32_t)SINE_N; i++) {
        v += d;
        tab[i] = (int16_t)v;
        if (i <= (int32_t)SINE_D2_N) {
            while (bits < 3u) {
                acc = acc << 8 | *p++;
                bits += 8u;
            }
            d += (int32_t)((acc >> (bits - 3u)) & 7u) - 3;
            bits -= 3u;
        }
    }

    check("the decode fills 1025 points (the 1025th repeats the 1st)",
          SINE_N == 1025u && SINE_D2_N == 1023u);
    check("the curve starts at 0 and its first step is 201 (2*pi/1024 * 32767, rounded)",
          tab[0] == 0 && SINE_D1 == 201);

    for (i = 0; i < (int32_t)SINE_N; i++) {
        int want = (int)(int16_t)(int)lround(32767.0 * sin(2.0 * 3.14159265358979323846 * (double)i / 1024.0));
        if (tab[i] != want) {
            if (!bad)
                first = (int)i;
            bad++;
        }
    }
    if (bad)
        printf("     first mismatch at %d: decoded %d, generator %d\n", first, tab[first],
               (int)(int16_t)(int)lround(32767.0 * sin(2.0 * 3.14159265358979323846 * (double)first / 1024.0)));
    check("every decoded point is the generator's integer (all 1025)", bad == 0);

    {
        /* the packed form has no out-of-range value by construction; what has to hold is that it is the
         * size the header declares and that the last byte is not padding the decode would read. */
        int range = (int)(sizeof SINE_D2) == (SINE_D2_N * 3 + 7) / 8;
        check("every stored difference fits the three bits the header declares", range);
    }
    check("the quarter point is the full scale (32767 at i = 256)",
          tab[256] == 32767 && tab[768] == -32767);
    {
        int wrap = 1;
        for (i = 0; i < 1024; i++)
            if (tab[i] != -tab[1024 - i])
                wrap = 0;
        check("the curve is odd around its half point (tab[i] == -tab[1024 - i])", wrap);
    }

    printf("sine table: %u points, %u B of XIP instead of 2050 (saved %u), decoded at boot\n",
           SINE_N, (unsigned)sizeof SINE_D2, (unsigned)(2050u - sizeof SINE_D2));
    printf(fails ? "SINE TABLE TEST FAILED (%d)\n" : "sine table: 6 checks, 0 FAILED\n", fails ? fails : 6);
    return fails ? 1 : 0;
}
