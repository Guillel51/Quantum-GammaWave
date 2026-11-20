/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3) The part filter's two new types, measured through the real path.
 *
 * NOTCH is LP + HP and SHELF is x + LP, both taken from the same state-variable filter the other
 * types run (voice.c tf_run / eng_analog.c svf_mode). A formula that is right on paper can still be
 * wired to the wrong output or lose the resonance, so this test feeds a sine through track_filter()
 * -- what a part's block goes through -- and reads the gain in dB:
 *
 *   LP     passes the band below the cutoff and rolls off above it      (the reference)
 *   NOTCH  takes the cutoff out and passes on both sides of it
 *   SHELF  a low shelf: the bass up, the top where it was (x + LP)
 *
 * The test never assumes where the cutoff landed: it sweeps, and asks each type for its shape.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NF 12
static int bad;
static void check(const char *what, int ok)
{
    printf("ftype: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* RMS gain in dB of the part filter of type ft at hz, cutoff 72 (mid), resonance 40 */
static double gain_db(uint32_t ft, double hz, int32_t res)
{
    static int32_t buf[CTL];
    const double amp = 3000.0, w = 2.0 * 3.141592653589793 * hz / (double)FS;
    track_t *t = &trk[0];
    double in = 0, out = 0;
    long n = 0;
    int b, i, period = (int)((double)FS / hz + 0.5), prime, measure;
    if (period < 8)
        period = 8;
    prime = period * 40 / CTL + 16;
    measure = prime / 2 + 16;
    t->p[P_FTYPE] = (int16_t)ft;
    t->p[P_FCUT] = 72;
    t->p[P_FRES] = (int16_t)res;
    t->p[P_LD_FLT] = t->p[P_ED_FLT] = 0;
    t->lfo_val = 0;
    t->lfo_fade = 32767;
    tf_on[0] = 0;                                        /* from no filter: no crossfade, the type as set */
    tf_cmb[0] = 0;
    tf_xb[0] = 0;
    tf_ft[0] = (uint8_t)ft;
    tf_ic1[0] = tf_ic2[0] = 0;
    for (b = 0; b < prime + measure; b++) {
        for (i = 0; i < CTL; i++)
            buf[i] = (int32_t)(amp * sin(w * (double)(n + i)));
        n += CTL;
        track_filter(t, buf, CTL, 0, 0, 0);
        if (b >= prime) {
            for (i = 0; i < CTL; i++) {
                double x = amp * sin(w * (double)(n - CTL + i));
                in += x * x;
                out += (double)buf[i] * (double)buf[i];
            }
        }
    }
    return in > 0 && out > 0 ? 10.0 * log10(out / in) : -200.0;
}

/* 12 points from 80 Hz to 6.4 kHz, log spaced: the sweep the shapes are read off */
static double hz_at(int i)
{
    return 80.0 * pow(6400.0 / 80.0, (double)i / (double)(NF - 1));
}

int main(void)
{
    double lp[NF], nt[NF], pk[NF], hp[NF], lo, hi, at80, at6k;
    int i, imin = 0;

    for (i = 0; i < NF; i++) {
        lp[i] = gain_db(FT_LP, hz_at(i), 40);
        hp[i] = gain_db(FT_HP, hz_at(i), 40);
        nt[i] = gain_db(FT_NOTCH, hz_at(i), 40);
        pk[i] = gain_db(FT_SHELF, hz_at(i), 40);
        if (nt[i] < nt[imin])
            imin = i;
    }
    /* the harness itself: the reference types behave as their names say */
    check("LP passes the bottom of the sweep and rolls off the top", lp[0] > -3.0 && lp[NF - 1] < -12.0);
    check("HP is the other way round", hp[0] < -12.0 && hp[NF - 1] > -3.0);

    /* NOTCH: a hole, on both sides of it the signal passes */
    lo = 1e9;
    hi = -1e9;
    for (i = 0; i < NF; i++) {
        if (nt[i] < lo) lo = nt[i];
        if (nt[i] > hi) hi = nt[i];
    }
    at80 = nt[0];
    at6k = nt[NF - 1];
    check("NOTCH: the deepest point of the sweep is a hole of at least 9 dB",
          imin > 0 && imin < NF - 1 && lo < -9.0);
    check("NOTCH: both ends of the sweep pass (within 3 dB of unity)", at80 > -3.0 && at6k > -3.0);
    check("NOTCH: the hole is between the two ends, not at either of them", nt[imin] < at80 - 6.0 && nt[imin] < at6k - 6.0);

    /* SHELF: a low shelf -- the bass up, the top where the lowpass would have taken it away */
    check("SHELF: the bass is up by at least 4 dB", pk[0] > 4.0);
    check("SHELF: the top stays where it was (within 1.5 dB of unity)", fabs(pk[NF - 1]) < 1.5);
    check("SHELF: it is not the lowpass (at the top LP is 20 dB down and SHELF is not)",
          pk[NF - 1] > lp[NF - 1] + 20.0);
    check("SHELF: its bass is above what LP gives there", pk[0] > lp[0] + 4.0);

    /* the new types are not the old ones wearing a name */
    check("NOTCH is not BP: it passes the band BP rejects", nt[0] > lp[0] - 3.0 && nt[NF - 1] > -3.0);
    check("the filter with no resonance still has its notch (RES 0)",
          gain_db(FT_NOTCH, hz_at(NF / 2), 0) < gain_db(FT_LP, hz_at(NF / 2), 0) - 3.0);

    /* no type blows up: a full-scale sine at high resonance stays in range */
    {
        double worst = -1e9;
        for (i = 0; i < NF; i++) {
            double g = gain_db(FT_SHELF, hz_at(i), 127);
            if (g > worst) worst = g;
        }
        check("SHELF at full resonance stays finite and under 40 dB", worst < 40.0 && worst > -200.0);
    }
    printf("%s\n", bad ? "FILTER TYPES TEST FAILED" : "filter types test passed");
    return bad != 0;
}
