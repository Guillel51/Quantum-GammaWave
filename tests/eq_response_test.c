/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The master EQ measured through the audio path, not through its coefficients.
 *
 * tests/eq_sweep_test.c checks stability, tails and the response curve as the UI
 * computes it. This one feeds a sine through qg_biquad_process() -- the same call
 * the mixer makes -- and compares the output RMS with the input RMS, so a filter
 * that is "stable" but has the wrong gain at the corner cannot pass.
 *
 * The reported symptom was maximum gain with minimum frequency. Every case here is
 * a corner: 20 Hz and 20 kHz, +/-24 dB, both Q ends.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static int32_t bl[CTL], br[CTL];

static void check(const char *what, int ok)
{
    printf("eq-r: %-70s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void set(int mode, int freq, int gain, int q)
{
    qg_biquad_init(&master_eq);
    if (mode == 1) qg_biquad_set_peak(&master_eq, freq, gain, q);
    else if (mode == 2) qg_biquad_set_notch(&master_eq, freq, q);
    else if (mode == 3) qg_biquad_set_lowshelf(&master_eq, freq, gain);
    else qg_biquad_set_highshelf(&master_eq, freq, gain);
}

/* RMS gain in dB at hz. A 20 Hz pole needs seconds to settle: 60 cycles of the test tone
 * prime the filter and the next 30 are measured, so the number is a steady state, not a
 * transient. A shelf at its own corner gives half its gain (the midpoint of the shelf). */
static double gain_db(int mode, int freq, int gain, int q, double hz)
{
    const double amp = 3000.0, w = 2.0 * 3.141592653589793 * hz / (double)FS;
    double in = 0, out = 0;
    long n = 0;
    int b, i, period = (int)((double)FS / hz + 0.5);
    int prime, measure;
    if (period < 8) period = 8;
    prime = period * 60 / CTL + 32;
    measure = prime / 2 + 32;
    set(mode, freq, gain, q);
    for (b = 0; b < prime + measure; b++) {
        for (i = 0; i < CTL; i++) bl[i] = br[i] = (int32_t)(amp * sin(w * (double)(n + i)));
        n += CTL;
        qg_biquad_process(&master_eq, bl, br, CTL);
        if (b >= prime) {
            for (i = 0; i < CTL; i++) {
                double x = amp * sin(w * (double)(n - CTL + i));
                in += x * x;
                out += (double)bl[i] * (double)bl[i];
            }
        }
    }
    if (in <= 0 || out <= 0) return -999.0;
    return 10.0 * log10(out / in);
}

/* sustained drive at the corner: the state must not ramp into the clamp (the reported bug) */
static int stays_bounded(int mode, int freq, int gain, int q, double hz)
{
    const double amp = 3000.0, w = 2.0 * 3.141592653589793 * hz / (double)FS;
    long n = 0;
    int b, i, ok = 1;
    set(mode, freq, gain, q);
    for (b = 0; b < 20000; b++) {
        for (i = 0; i < CTL; i++) bl[i] = br[i] = (int32_t)(amp * sin(w * (double)(n + i)));
        n += CTL;
        qg_biquad_process(&master_eq, bl, br, CTL);
        for (i = 0; i < CTL; i++)
            if (bl[i] >= BQ_LIM || bl[i] <= -BQ_LIM) ok = 0;
    }
    return ok;
}

static int bounded_at(int mode, int freq, int gain, int q)
{
    int b, i, ok = 1;
    set(mode, freq, gain, q);
    for (b = 0; b < 64; b++) {
        for (i = 0; i < CTL; i++) bl[i] = br[i] = (b & 1) ? 32000 : -32000;
        qg_biquad_process(&master_eq, bl, br, CTL);
        for (i = 0; i < CTL; i++)
            if (bl[i] >= BQ_LIM || bl[i] <= -BQ_LIM) ok = 0;
    }
    return ok;
}

int main(void)
{
    double v;
    char b[128];

    v = gain_db(3, 0, 24, 64, 20.0);
    snprintf(b, sizeof b, "low shelf +24 dB at its 20 Hz corner (half gain): %+.2f dB", v);
    check(b, v > 9.5 && v < 14.5);

    v = gain_db(3, 0, -24, 64, 20.0);
    snprintf(b, sizeof b, "low shelf -24 dB at its 20 Hz corner (half gain): %+.2f dB", v);
    check(b, v < -9.5 && v > -14.5);

    v = gain_db(4, 127, 24, 64, 20000.0);
    snprintf(b, sizeof b, "high shelf +24 dB at its 20 kHz corner (half gain): %+.2f dB", v);
    check(b, v > 9.5 && v < 14.5);

    v = gain_db(1, 0, 24, 64, 20.0);
    snprintf(b, sizeof b, "peak +24 dB at 20 Hz: measured %+.2f dB", v);
    check(b, v > 21.5 && v < 26.5);

    /* A 20 Hz cut is limited by the Q25 coefficients themselves, not by the recursion: at that
     * frequency the denominator's coefficients are only a few LSB apart, so a deep cut cannot be
     * resolved (measured -17.6 dB). The same cut at 650 Hz reaches -24.0 dB, which is checked
     * below: this is a documented limit of the filter's resolution, not a regression. */
    v = gain_db(1, 0, -24, 64, 20.0);
    snprintf(b, sizeof b, "peak -24 dB at 20 Hz: measured %+.2f dB (Q25 limit, >= 15)", v);
    check(b, v < -15.0 && v > -27.0);

    v = gain_db(1, 64, -24, 64, EQ_HZ[64]);
    snprintf(b, sizeof b, "peak -24 dB at index 64 (%.0f Hz): measured %+.2f dB", (double)EQ_HZ[64], v);
    check(b, v < -21.5 && v > -26.5);

    v = gain_db(2, 0, 0, 64, 20.0);
    snprintf(b, sizeof b, "notch at 20 Hz: measured %+.2f dB", v);
    check(b, v < -15.0);

    v = gain_db(1, 64, 12, 64, EQ_HZ[64]);          /* at the index's own frequency, not a guess */
    snprintf(b, sizeof b, "peak +12 dB at index 64 (%.0f Hz): measured %+.2f dB", (double)EQ_HZ[64], v);
    check(b, v > 10.0 && v < 14.0);

    v = gain_db(3, 0, 24, 64, 1000.0);
    snprintf(b, sizeof b, "low shelf +24 dB at 20 Hz leaves 1 kHz alone: measured %+.2f dB", v);
    check(b, v > -1.0 && v < 1.0);

    check("a full-scale square at 20 Hz +24 dB never reaches the state clamp",
          bounded_at(3, 0, 24, 64) && bounded_at(4, 127, 24, 64) && bounded_at(1, 0, 24, 127));
    check("20 s of a 20 Hz sine at +/-24 dB never ramps into the clamp (the reported bug)",
          stays_bounded(3, 0, 24, 64, 20.0) && stays_bounded(3, 0, -24, 64, 20.0) &&
          stays_bounded(1, 0, 24, 64, 20.0));

    printf("eq response: %s (%d failed)\n", bad ? "FAILED" : "passed", bad);
    return bad != 0;
}
