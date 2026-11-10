/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Three of the effects, measured from the outside instead of judged by ear: the CHORUS bus's
 * PHASER, the master's mid/side WIDTH, and the master EQ's shelves. The complaints were that the
 * phaser was weak and that the low shelf was aggressive; "weak" and "aggressive" are numbers here
 * -- a notch depth in dB, a correlation coefficient, a shelf's gain in dB -- so a claim can be
 * checked and a regression caught.
 *
 * What this instrument found on the build it was written for:
 *   - the phaser is not weak: freezing the sweep and sweeping the tone shows a 23.6 dB comb inside
 *     200 Hz..6 kHz, and no step at all when DEPTH jumps (the mix ramps per sample);
 *   - M-WIDE 0 is exactly mono, 64 is untouched, 127 reaches correlation -0.59 with +3.8 dB;
 *   - the low shelf is monotone, reaches its -24 dB at the bottom and does not overshoot 0 above
 *     its corner, which is what "aggressive" would have looked like.
 * The first version of this file measured a single tone across the sweep, and that version said
 * the phaser was 1.7 dB deep -- the nulls were narrow in frequency and fell between the tones.
 * A measurement that says "weak" has to be checked before it is believed, too.
 *
 *   tests/fx_probe
 *
 * Every figure below is a host measurement on x86 through the real code paths (the same
 * cho_phaser, master_out and coefficient builders the firmware runs), not a model of them.
 * It proves the arithmetic and the DSP, not the FM-1. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return host_dma_advanced && host_half_reads++ > 0u; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

#include <math.h>

#define PI2 6.283185307179586

static int bad;
static void check(const char *what, int ok, const char *fmt, double a, double b)
{
    printf("%-58s %s", what, ok ? "ok  " : "FAIL");
    if (fmt) printf(fmt, a, b);
    printf("\n");
    if (!ok) bad++;
}

/* ---------------------------------------------------------------- PHASER ---
 * A 1 kHz tone through cho_phaser, the LFO driven a sample at a time as the block runs it. The
 * notches sweep past the tone, so the output envelope swings by however deep the notches are:
 * that swing is the effect. A phaser that "did little" reads as a swing of a decibel or two. */
/* The sweep frozen at each of 32 LFO positions, the tone run to steady state at each: the gain of
 * the chain at that position. The swing of those gains is the notch depth the ear hears; smearing
 * the moving sweep into one measurement would miss a notch that is sharp and brief. */
static double phaser_gain_at(double hz, uint32_t p)
{
    int i;
    double acc = 0;
    memset(&fx, 0, sizeof fx);
    for (i = 0; i < 8192; i++) {                   /* 4096 to settle, 4096 measured */
        int32_t x = (int32_t)(sin(PI2 * hz * i / FS) * 8000.0), y, sd = 0;
        y = cho_phaser(x, p, 12192, &sd);
        if (i >= 4096) acc += (double)y * y;
    }
    return sqrt(acc / 4096.0);
}

/* The sweep is frozen at 16 positions and the tone is swept instead: the deepest point of the comb
 * at that position, anywhere from 200 Hz to 6 kHz. Testing one tone across the sweep misses a null
 * that is narrow in frequency, and testing one sweep position can put the null between two tones;
 * this is the notch depth the ear actually hears on material with harmonics. */
static void probe_phaser(void)
{
    double lo_all = 1e9, hi_all = 0, deepest = 0, at = 0;
    int32_t peak_delta = 0, prev = 0;
    int p, f, i;

    for (p = 0; p < 16; p++) {
        uint32_t ph = (uint32_t)((uint64_t)p << 28);   /* a turn is 2^32: 16 positions */
        double lo = 1e9, hi = 0;
        for (f = 0; f < 32; f++) {
            double hz = 200.0 * pow(30.0, f / 31.0);   /* 200 Hz .. 6 kHz, log spaced */
            double g = phaser_gain_at(hz, ph);
            if (g < lo) lo = g;
            if (g > hi) hi = g;
        }
        if (20.0 * log10(hi / lo) > deepest) { deepest = 20.0 * log10(hi / lo); at = (double)p; }
        if (lo < lo_all) lo_all = lo;
        if (hi > hi_all) hi_all = hi;
    }
    printf("       deepest comb %.1f dB at LFO position %d; overall gain %.0f .. %.0f (Q15)\n",
           deepest, (int)at, lo_all, hi_all);
    check("PHASER: the comb has depth somewhere in 200 Hz..6 kHz (>= 20 dB)", deepest >= 20.0, " %.1f dB", deepest, 0);
    check("PHASER: the chain stays bounded (no runaway feedback)", hi_all < 40000, " %.0f", hi_all, 0);
    /* and a step in DEPTH must not step the output: the mix ramps per sample */
    memset(&fx, 0, sizeof fx);
    for (i = 0; i < (int)(FS / 2); i++) {
        int32_t x = (int32_t)(sin(PI2 * 1000.0 * i / FS) * 8000.0), y, sd = 0;
        uint32_t p = (uint32_t)((double)i * 1.4 * 4294967296.0 / FS);   /* ~1.4 Hz, as the LFO runs */
        int32_t d;
        y = cho_phaser(x, p, i < (int)FS / 4 ? 0 : 12192, &sd);   /* DEPTH jumps at a quarter second */
        d = y - prev;
        if (d < 0) d = -d;
        if (d > peak_delta) peak_delta = d;
        prev = y;
    }
    check("PHASER: no click when DEPTH jumps (max sample step, Q15)", peak_delta < 5000,
          " %d", (double)peak_delta, 0);
}

/* ----------------------------------------------------------- M/S WIDTH ---
 * Two decorrelated sources, one hard left and one hard right, through master_out. At 64 the
 * width is bypassed; at 0 the sides are gone (mono); at 127 they are nearly doubled. The
 * correlation of the output says which of those actually happens. */
static double probe_width(int mwidth, double *lvl)
{
    double sl = 0, sr = 0, slr = 0;
    int i;
    uint32_t s1 = 12345u, s2 = 99991u;
    song.g[G_MWIDTH] = (int16_t)mwidth;
    dc_l = dc_r = 0;
    dce_l = dce_r = 0;
    lim_env = 0;
    lev_cur = 32768;
    lev_dg = 0;
    for (i = 0; i < 20000; i++) {
        int32_t l, r;
        s1 = s1 * 1103515245u + 12345u;
        s2 = s2 * 1103515245u + 12345u;
        l = (int32_t)((s1 >> 17) & 0x3fff) - 8192;
        r = (int32_t)((s2 >> 17) & 0x3fff) - 8192;
        master_out(&l, &r);
        sl += (double)l * l;
        sr += (double)r * r;
        slr += (double)l * r;
    }
    *lvl = sqrt(sl / 20000.0);
    return slr / sqrt(sl * sr + 1e-9);
}

static void probe_mwidth(void)
{
    double c0, c64, c127, l0, l64, l127;
    c0 = probe_width(0, &l0);
    c64 = probe_width(64, &l64);
    c127 = probe_width(127, &l127);
    printf("       correlation: width 0 %.3f, 64 %.3f, 127 %.3f (level %.0f / %.0f / %.0f)\n",
           c0, c64, c127, l0, l64, l127);
    check("M-WIDE 0 is mono (L and R identical)", c0 > 0.999, " r=%.4f", c0, 0);
    check("M-WIDE 64 is the untouched pair", c64 < 0.1, " r=%.4f", c64, 0);
    check("M-WIDE 127 is wider than 64", c127 < c64 - 0.05, " r=%.4f", c127, 0);
    check("M-WIDE keeps the level sane (127 within +-6 dB of 64)", fabs(20.0 * log10(l127 / l64)) < 6.0,
          " %.1f dB", 20.0 * log10(l127 / l64), 0);
}

/* ------------------------------------------------------------------ EQ ---
 * The master EQ's own curve, from the same coefficient builders the audio path uses. A low
 * shelf at -24 dB should fall by about 24 dB below its corner and stay flat two octaves above
 * it; if the shelf is "aggressive" that is visible as a corner that cuts well above its
 * frequency, or as a shelf that never flattens. */
/* dB of the master EQ at a real frequency: fx_master_eq_hz_db, the same curve the graph draws */
static double eq_db(double hz) { return (double)fx_master_eq_hz_db((int32_t)hz) / 256.0; }

static void probe_eq(void)
{
    static const double HZ[7] = {30, 60, 120, 250, 1000, 4000, 16000};
    double low_db, top_db, peak_db, worst = 0;
    int i;
    song.g[G_EQMODE] = 3;                          /* LOW SH */
    song.g[G_EQFREQ] = 48;
    song.g[G_EQGAIN] = -24;
    song.g[G_EQQ] = 64;
    printf("       LOW SH -24 dB curve:");
    for (i = 0; i < 7; i++)
        printf(" %g:%+.1f", HZ[i], eq_db(HZ[i]));
    printf("\n");
    low_db = eq_db(30);
    top_db = eq_db(4000);
    /* a low shelf must not dip on the way up, and must not overshoot 0 above the corner: a bump in
     * the mids is what a shelf reads as when its Q is wrong */
    {
        double prev = -99;
        for (i = 20; i <= 20000; i = i * 3 / 2) {
            double d = eq_db((double)i);
            if (d < prev - 0.3) worst = -99;        /* went down: not a shelf */
            if (d > 0.5) worst = -98;               /* overshoot above unity */
            prev = d;
        }
    }
    check("EQ LOW SH -24: the bottom is cut near -24 dB", low_db < -18.0 && low_db > -30.0, " %.1f dB", low_db, 0);
    check("EQ LOW SH: rises monotonically and never overshoots 0", worst > -97.0, " bad=%g", worst, 0);
    check("EQ LOW SH: it does flatten out (4 kHz within 2 dB of 0)", fabs(top_db) < 2.0, " %.1f dB", top_db, 0);

    song.g[G_EQGAIN] = 24;
    low_db = eq_db(30);
    check("EQ LOW SH +24: the bottom is lifted near +24 dB", low_db > 18.0 && low_db < 30.0, " %.1f dB", low_db, 0);

    song.g[G_EQMODE] = 1;                          /* PEAK: the curve must peak at its own gain */
    song.g[G_EQFREQ] = 64;
    song.g[G_EQGAIN] = 12;
    peak_db = -99;
    {
        double at = 0;
        for (i = 20; i <= 20000; i = i * 11 / 10) {
            double d = eq_db((double)i);
            if (d > peak_db) { peak_db = d; at = i; }
        }
        printf("       PEAK +12: max %+.1f dB at %.0f Hz (20 Hz %+.1f, 16 kHz %+.1f)\n", peak_db, at, eq_db(20), eq_db(16000));
    }
    check("EQ PEAK +12 reaches its gain at the center", peak_db > 10.0 && peak_db < 13.0, " %.1f dB", peak_db, 0);
    song.g[G_EQMODE] = 0;
}

int main(void)
{
    memset(trk, 0, sizeof trk);
    song.g[G_MWIDTH] = 64;
    printf("fx_probe: host x86, %.0f Hz\n\n", (double)FS);
    probe_phaser();
    probe_mwidth();
    probe_eq();
    printf("\n%s\n", bad ? "FX PROBE FAILED" : "fx probe passed");
    return bad ? 1 : 0;
}
