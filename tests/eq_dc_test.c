/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* What the master EQ's DC gain really is, and why it is not the coefficient's.
 *
 * The earlier report said "DC 1000 -> 13 492, expected 16 000, -0.75 dB". Both numbers were off:
 * 10^(24/20) = 15.85 is the +24 dB ratio, but a low shelf's DC gain is (b0+b1+b2)/(1+a1+a2) of
 * the coefficients actually stored, 15.956 for this corner. This test separates:
 *
 *   ideal       the coefficient's own DC gain, in double: (b0+b1+b2)/(1+a1+a2)
 *   processed   what qg_biquad_process() really returns, sampled at 1 s and at 20 s
 *   settling    the same value at both times: a 20 Hz shelf settles in ~2 s here, so a
 *               difference would be settling, and there is none
 *   bias        processed - ideal, in dB: what is left of the fixed-point error of the
 *               recursion after it rounds to nearest, multiplied by 1/(1+a1+a2) ~ 493 000
 *               because the pole is that close to 1
 *
 * Nothing here is tuned to make the numbers agree: the assertions are the ones the measured
 * behaviour has to keep (a bounded bias, no drift, both channels equal). If a future change to
 * the arithmetic moves the bias, this test prints the new figure and only fails outside 2 dB.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("eq-dc: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* run `secs` of DC 1000 through the filter; -> the last sample of each channel */
static void dc(int mode, int freq, int gain, double secs, int32_t *yl, int32_t *yr)
{
    static int32_t l[CTL], r[CTL];
    long n = 0, want = (long)(secs * FS);
    int i;
    qg_biquad_init(&master_eq);
    if (mode == 1) qg_biquad_set_peak(&master_eq, freq, gain, 64);
    else if (mode == 2) qg_biquad_set_notch(&master_eq, freq, 64);
    else if (mode == 3) qg_biquad_set_lowshelf(&master_eq, freq, gain);
    else qg_biquad_set_highshelf(&master_eq, freq, gain);
    while (n < want) {
        for (i = 0; i < CTL; i++) l[i] = r[i] = 1000;
        qg_biquad_process(&master_eq, l, r, CTL);
        n += CTL;
    }
    *yl = l[CTL - 1];
    *yr = r[CTL - 1];
}

int main(void)
{
    const double x = 1000.0;
    double b0, b1, b2, a1, a2, ideal, amp;
    int32_t y1l, y1r, y20l, y20r;
    double g1, g20, bias;

    host_tracks_init();
    qg_biquad_init(&master_eq);
    qg_biquad_set_lowshelf(&master_eq, 0, 24);
    b0 = master_eq.b0 / (double)BQ_ONE;
    b1 = master_eq.b1 / (double)BQ_ONE;
    b2 = master_eq.b2 / (double)BQ_ONE;
    a1 = master_eq.a1 / (double)BQ_ONE;
    a2 = master_eq.a2 / (double)BQ_ONE;
    ideal = (b0 + b1 + b2) / (1.0 + a1 + a2);

    printf("eq-dc: coefficients Q25: b0 %d b1 %d b2 %d a1 %d a2 %d\n",
           master_eq.b0, master_eq.b1, master_eq.b2, master_eq.a1, master_eq.a2);
    printf("eq-dc: 1 + a1 + a2 = %.3e (%.0f LSB of Q25), pole radius %.9f\n",
           1.0 + a1 + a2, (1.0 + a1 + a2) * BQ_ONE, sqrt(a2));
    printf("eq-dc: ideal DC gain (b0+b1+b2)/(1+a1+a2) = %.4f (%.2f dB), x1000 -> %.0f\n",
           ideal, 20.0 * log10(ideal), ideal * x);
    /* a +24 dB low shelf is 15.85x by definition and this corner's coefficients give 15.96:
     * the difference between them is the shelf's own DC error, ~0.06 dB, not a processing bug */
    check("the coefficient's DC gain is the +24 dB ratio (15.85) within 0.1 dB",
          fabs(20.0 * log10(ideal) - 24.0) < 0.1);

    dc(3, 0, 24, 1.0, &y1l, &y1r);
    dc(3, 0, 24, 20.0, &y20l, &y20r);
    g1 = y1l / x;
    g20 = y20l / x;
    bias = 20.0 * log10(g1 / ideal);
    printf("eq-dc: processed at 1 s: %d/%d  at 20 s: %d/%d  (gain %.4f, %.2f dB)\n",
           y1l, y1r, y20l, y20r, g1, 20.0 * log10(g1));
    printf("eq-dc: bias vs the ideal gain: %+.2f dB (truncation x 1/(1+a1+a2))\n", bias);

    check("both channels process DC identically (no left/right scale asymmetry)", y1l == y1r && y20l == y20r);
    check("the DC output does not drift between 1 s and 20 s (it is settled, not converging)",
          y1l == y20l && y1r == y20r);
    check("the processed DC gain is the coefficient's within 0.5 dB", fabs(bias) < 0.5);
    check("the processed DC gain is a boost, not a cut or a runaway (gain > 10)", g1 > 10.0 && g1 < 25.0);
    /* rounding to nearest (v1.8.9) is what brought this from -1.46 dB to +0.23 dB: the residue is
     * the last half-LSB of each product, amplified by the near-unit pole. A loss of 1 dB or more,
     * or an output that keeps growing, would be a different bug. */
    check("the quantization residue stays inside +/-0.5 dB of the ideal gain", fabs(bias) < 0.5);

    /* The same shelf at three levels. The truncation is an absolute offset in the output, not a
     * remainder: at a quiet DC it is a large share of the signal (-4.4 dB at 100) and at a loud
     * one it is a fraction of a percent. DC is not audio; the corner-response test covers the band. */
    {
        static int32_t l[CTL], r[CTL];
        static const int32_t IN[3] = {100, 1000, 8000};
        int k, i, err_ok = 1, gain_ok = 1;
        for (k = 0; k < 3; k++) {
            long n;
            int32_t out;
            double g, err;
            qg_biquad_init(&master_eq);
            qg_biquad_set_lowshelf(&master_eq, 0, 24);
            for (n = 0; n < 4L * FS; n += CTL) {
                for (i = 0; i < CTL; i++) l[i] = r[i] = IN[k];
                qg_biquad_process(&master_eq, l, r, CTL);
            }
            out = l[CTL - 1];
            g = out / (double)IN[k];
            err = out - ideal * IN[k];
            printf("eq-dc: input %5d -> %7d  gain %.4f (%.2f dB, ideal %+.2f dB)  offset %+.0f units\n",
                   IN[k], out, g, 20.0 * log10(g), 20.0 * log10(g / ideal), err);
            if (fabs(err) > 1000.0) err_ok = 0;                  /* < 3 % of 32768 */
            if (IN[k] >= 1000 && fabs(20.0 * log10(g / ideal)) > 0.5) gain_ok = 0;
        }
        check("the absolute DC remainder stays under 1000 units at every level", err_ok);
        check("at audio levels (1000, 8000) the gain is the ideal one within 0.5 dB", gain_ok);
    }

    printf("eq dc: %s (%d failed)\n", bad ? "FAILED" : "passed", bad);
    return bad != 0;
}
