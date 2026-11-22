/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LFO CURVE (v1.12.5): the phase bent before the wave sees it.
 *
 * The claim the knob makes is narrow and checkable: the cycle never runs backwards (the bend is monotone), every
 * wave keeps its range, the mapping is untouched at 0, and turning it changes WHEN in the cycle the wave is where
 * it was. What it does not prove: how any of it sounds on the unit -- this is host x86-64 with hostsim.c.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok, double a, double b)
{
    printf("lfo: %-64s %s (%.3f / %.3f)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

/* the first phase (of 1<<20 steps of the cycle) at which the wave of `wave` reaches up to `level` */
static uint32_t cross(int32_t curve, int32_t wave, int32_t level)
{
    uint32_t i;
    trk[0].p[P_LSHP] = (int16_t)curve;
    trk[0].p[P_LWAVE] = (int16_t)wave;
    for (i = 0; i < (1u << 20); i++) {
        int32_t v = lfo_wave(&trk[0], i << 12);
        if (v >= level)
            return i;
    }
    return 1u << 20;
}

int main(void)
{
    uint32_t i, back, out_of_range;
    int32_t w, cur;
    clock_t c0, c1;
    double ns0, ns1;
    volatile int32_t acc;

    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    trk_live = NPART;

    /* 1. MONOTONE: 4096 phases of every wave, the bent phase never goes back */
    for (w = 0, back = 0; w < 10; w++)
        for (cur = -64; cur <= 63; cur += 127) {
            uint32_t prev = 0;
            for (i = 0; i < 4096u; i++) {
                uint32_t ph = lfo_skew(i << 20, cur);
                if (ph < prev)
                    back++;
                prev = ph;
            }
        }
    check("the bent phase never runs backwards (all waves, both ends)", back == 0, (double)back, 0.0);

    /* 2. RANGE: no wave leaves its range with the curve at either end */
    for (w = 0, out_of_range = 0; w < 10; w++)
        for (cur = -64; cur <= 63; cur += 127) {
            trk[0].p[P_LSHP] = (int16_t)cur;
            trk[0].p[P_LWAVE] = (int16_t)w;
            for (i = 0; i < 1024u; i++) {
                int32_t v = lfo_wave(&trk[0], i << 22);
                if (v < -32768 || v > 32767)
                    out_of_range++;
            }
        }
    check("every wave stays in range with the curve at either end", out_of_range == 0, (double)out_of_range, 0.0);

    /* 3. IDENTITY at 0: the phase comes back exactly as it went in */
    for (i = 0, back = 0; i < 4096u; i++)
        if (lfo_skew(i << 20, 0) != (i << 20))
            back++;
    check("curve 0 leaves the phase alone, to the bit", back == 0, (double)back, 0.0);

    /* 4. DIRECTION: a rising wave reaches half its height earlier with +60 and later with -60 */
    {
        uint32_t c0 = cross(0, 1, 0), cp = cross(60, 1, 0), cm = cross(-60, 1, 0);
        check("+60 arrives earlier than the straight lean", cp < c0, (double)cp, (double)c0);
        check("-60 arrives later than the straight lean", cm > c0, (double)cm, (double)c0);
    }

    /* 5. NO STEP: the smooth waves, a cycle at a time, never jump more than the phase itself can */
    {
        int32_t worst = 0;
        for (w = 0; w < 10; w++) {
            int32_t prev = 0;
            if (w == 3 || w == 4 || w == 8)
                continue;                            /* SQR, S&H and PULSE are steps by definition */
            for (cur = -64; cur <= 63; cur += 63) {
                trk[0].p[P_LSHP] = (int16_t)cur;
                trk[0].p[P_LWAVE] = (int16_t)w;
                for (i = 0; i < 4096u; i++) {
                    int32_t v = lfo_wave(&trk[0], i << 20), d = v - prev;
                    if (i && (d < 0 ? -d : d) > worst)
                        worst = d < 0 ? -d : d;
                    prev = v;
                }
            }
        }
        printf("lfo: the biggest step a smooth wave takes in 1/4096 of a cycle: %d (of 65536)\n", worst);
        /* PLUK's head is steep by its own maths ((1 - x)^4 leaves at speed 4), and SQR / S&H / PULSE are steps
         * by definition: this is the smooth ones, with the curve, against the phase resolution itself */
        check("a smooth wave under the curve has no step of its own", worst < 600, (double)worst, 600.0);
    }

    /* 6. COST: a hundred thousand waves, curve off and at 60 */
    trk[0].p[P_LWAVE] = 2;                           /* SAW: the switch's cheapest branch, so the skew shows */
    trk[0].p[P_LSHP] = 0;
    acc = 0;
    c0 = clock();
    for (i = 0; i < 100000u; i++)
        acc += lfo_wave(&trk[0], i << 12);
    c1 = clock();
    ns0 = 1e9 * (double)(c1 - c0) / 100000.0 / (double)CLOCKS_PER_SEC;
    trk[0].p[P_LSHP] = 60;
    c0 = clock();
    for (i = 0; i < 100000u; i++)
        acc += lfo_wave(&trk[0], i << 12);
    c1 = clock();
    ns1 = 1e9 * (double)(c1 - c0) / 100000.0 / (double)CLOCKS_PER_SEC;
    printf("lfo: (acc %d)\n", (int)acc);
    printf("lfo: one wave costs %.1f ns with the curve off, %.1f with it (control rate: once a block a part)\n",
           ns0, ns1);
    check("the curve's cost is a control-rate cost", ns1 < ns0 + 60.0, ns1 - ns0, 60.0);

    printf("lfo: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
