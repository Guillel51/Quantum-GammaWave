/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Master EQ audit: every mode, every frequency/gain/Q corner, a loud input, a
 * silence, and a preset switch. It runs the same qg_biquad_* functions and the
 * same master_eq_update() as the firmware (through hostsim.c). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static int32_t l[CTL], r[CTL];
static void check(const char *what, int ok)
{
    printf("eq: %-70s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int32_t peak(const int32_t *x, uint32_t n)
{
    int32_t p = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t v = x[i] < 0 ? -x[i] : x[i];
        if (v > p) p = v;
    }
    return p;
}
static void fill(int32_t v)
{
    uint32_t i;
    for (i = 0; i < CTL; i++) l[i] = r[i] = v;
}
static void zeros(void)
{
    felucca_memset(l, 0, sizeof l);
    felucca_memset(r, 0, sizeof r);
}
static void set_mode(int mode, int freq, int gain, int q)
{
    qg_biquad_init(&master_eq);
    if (mode == 1) qg_biquad_set_peak(&master_eq, freq, gain, q);
    else if (mode == 2) qg_biquad_set_notch(&master_eq, freq, q);
    else if (mode == 3) qg_biquad_set_lowshelf(&master_eq, freq, gain);
    else if (mode == 4) qg_biquad_set_highshelf(&master_eq, freq, gain);
}
static int coefficients_ok(void)
{
    if (!master_eq.active) return 1;
    if (master_eq.b0 < -(1 << 29) || master_eq.b0 > (1 << 29) ||
        master_eq.b1 < -(1 << 29) || master_eq.b1 > (1 << 29) ||
        master_eq.b2 < -(1 << 29) || master_eq.b2 > (1 << 29))
        return 0;
    if (master_eq.a2 < -(16300 << 10) || master_eq.a2 > (16300 << 10)) return 0;
    if (master_eq.a1 < -((1 << 24) + master_eq.a2 - (1 << 12)) ||
        master_eq.a1 > (1 << 24) + master_eq.a2 - (1 << 12))
        return 0;
    return 1;
}

int main(void)
{
    static const int MODE[4] = {1, 2, 3, 4};
    static const int FREQ[6] = {0, 1, 32, 64, 96, 127};
    static const int GAIN[5] = {-24, -18, 0, 18, 24};
    static const int Q[4] = {0, 1, 64, 127};
    uint32_t m, f, g, q, b, i;
    int32_t worst_tail = 0, silent = 0, bad_coeff = 0;
    host_tracks_init();
    for (m = 0; m < 4u; m++)
        for (f = 0; f < 6u; f++)
            for (g = 0; g < 5u; g++)
                for (q = 0; q < 4u; q++) {
                    set_mode(MODE[m], FREQ[f], GAIN[g], Q[q]);
                    if (!coefficients_ok()) bad_coeff++;
                    /* a loud square (the state is driven hard, as by a +24 dB shelf) */
                    for (b = 0; b < 32u; b++) {
                        fill((b & 1u) ? 30000 : -30000);
                        qg_biquad_process(&master_eq, l, r, CTL);
                    }
                    /* silence: after the flush window the state must be gone */
                    for (b = 0; b < 48u; b++) {
                        zeros();
                        qg_biquad_process(&master_eq, l, r, CTL);
                    }
                    i = (uint32_t)peak(l, CTL);
                    if ((int32_t)i > worst_tail) worst_tail = (int32_t)i;
                    if (i != 0) silent++;
                    zeros();
                    qg_biquad_process(&master_eq, l, r, CTL);
                }
    printf("eq: swept %u corners; worst tail after 48 silent blocks %d; nonzero tails %d; bad coefficients %d\n",
           4u * 6u * 5u * 4u, worst_tail, silent, bad_coeff);
    check("all modes/freq/gain/Q: stable coefficients, no residual after silence",
          worst_tail == 0 && silent == 0 && bad_coeff == 0);

    /* (1.8.6) the response curve: a real frequency axis, a symmetric gain axis and a
     * stable sign at the extremes. fx_master_eq_hz_db uses the same coefficients as the
     * audio path; the old 12M + idx * 16M angle only covered 123..287 Hz. */
    {
        int32_t peak_boost, peak_cut, notch, low_lo, low_hi, high_lo, high_hi;
        int32_t eq_hz = EQ_HZ[64];
        song.g[G_EQMODE] = 1; song.g[G_EQFREQ] = 64; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
        peak_boost = fx_master_eq_hz_db(eq_hz);
        song.g[G_EQMODE] = 1; song.g[G_EQGAIN] = -24;
        peak_cut = fx_master_eq_hz_db(eq_hz);
        song.g[G_EQMODE] = 2; song.g[G_EQGAIN] = 0;
        notch = fx_master_eq_hz_db(eq_hz);
        song.g[G_EQMODE] = 3; song.g[G_EQGAIN] = 24;
        low_lo = fx_master_eq_hz_db(20); low_hi = fx_master_eq_hz_db(20000);
        song.g[G_EQMODE] = 4;
        high_lo = fx_master_eq_hz_db(20); high_hi = fx_master_eq_hz_db(20000);
        printf("eq:   (curve Q8 dB: peak +24 %d, peak -24 %d, notch %d, low 20/20k %d/%d, high 20/20k %d/%d)\n",
               peak_boost, peak_cut, notch, low_lo, low_hi, high_lo, high_hi);
        check("EQ curve: +24 dB peaks, -24 dB cuts, notch cuts, shelves tilt",
              peak_boost > 2000 && peak_cut < -2000 && notch < -2000 &&
              low_lo > low_hi + 2000 && high_hi > high_lo + 2000);
        check("EQ curve: 20 Hz and 20 kHz stay bounded and finite",
              peak_boost <= 24 * 256 && peak_boost >= -24 * 256 && low_lo <= 24 * 256 &&
              low_lo >= -24 * 256 && high_hi <= 24 * 256 && high_hi >= -24 * 256);
        song.g[G_EQMODE] = 0;
    }

    /* a poisoned delay line (a preset switched while the filter was ringing) */
    set_mode(3, 0, 24, 64);
    for (b = 0; b < 16u; b++) {
        fill((b & 1u) ? 30000 : -30000);
        qg_biquad_process(&master_eq, l, r, CTL);
    }
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    master_eq.s1_l = master_eq.s1_r = master_eq.s2_l = master_eq.s2_r = 200000;
    master_eq_update();                              /* the same values: the guard must clear it */
    for (b = 0; b < 40u; b++) {
        zeros();
        qg_biquad_process(&master_eq, l, r, CTL);
    }
    check("a poisoned state is cleared by master_eq_update", peak(l, CTL) == 0);

    /* preset switch: extreme low shelf -> bypass -> a normal peak; the output returns */
    host_tracks_init();
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    for (b = 0; b < 16u; b++) {
        fill((b & 1u) ? 30000 : -30000);
        qg_biquad_process(&master_eq, l, r, CTL);
    }
    song.g[G_EQMODE] = 0; master_eq_update();
    zeros(); qg_biquad_process(&master_eq, l, r, CTL);
    song.g[G_EQMODE] = 1; song.g[G_EQFREQ] = 64; song.g[G_EQGAIN] = 12; song.g[G_EQQ] = 64;
    master_eq_update();
    for (b = 0; b < 8u; b++) {
        for (i = 0; i < CTL; i++)
            l[i] = r[i] = (int32_t)(12000.0 * sin(2.0 * 3.141592653589793 * 440.0 * (b * CTL + i) / FS));
        qg_biquad_process(&master_eq, l, r, CTL);
    }
    check("preset recovery: extreme low shelf, bypass, then a peak passes audio",
          peak(l, CTL) > 1000);
    song.g[G_EQMODE] = 0; master_eq_update();

    /* rapid parameter changes: no coefficient can make the filter leave its bound */
    for (i = 0; i < 4000u; i++) {
        set_mode(1u + i % 4u, (int)(i * 37u % 128u), (int)(i * 11u % 49u) - 24, (int)(i * 53u % 128u));
        if (!coefficients_ok()) bad_coeff++;
        fill((i & 1u) ? 28000 : -28000);
        qg_biquad_process(&master_eq, l, r, CTL);
        if (peak(l, CTL) > 4 * (1 << 20)) bad_coeff++;
    }
    check("rapid frequency/gain/Q changes stay finite and bounded", bad_coeff == 0);
    printf("%s\n", bad ? "EQ SWEEP FAILED" : "eq sweep passed");
    return bad != 0;
}
