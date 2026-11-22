/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Six of the effects, measured from the outside instead of judged by ear: the CHORUS bus's PHASER and
 * FLANGER, the buses' routing, the master's mid/side WIDTH and low cut, and the master EQ's shelves and
 * peak. The complaints were that the phaser was weak and that the low shelf was aggressive; "weak" and
 * "aggressive" are numbers here -- a notch depth in dB, a correlation coefficient, a shelf's gain in dB --
 * so a claim can be checked and a regression caught.
 *
 * What this instrument found on the build it was written for:
 *   - the phaser is not weak: freezing the sweep and sweeping the tone shows a 23.6 dB comb inside
 *     200 Hz..6 kHz, and no step at all when DEPTH jumps (the mix ramps per sample);
 *   - M-WIDE 0 is exactly mono, 64 is untouched, 127 reaches correlation -0.59 at the level of 64
 *     (it used to arrive with +3.8 dB, which pushed the master's limiter: 5.0 compensates the
 *     widened pair's power, and a mono source stays mono);
 *   - the low shelf is monotone, reaches its -24 dB at the bottom and does not overshoot 0 above
 *     its corner, which is what "aggressive" would have looked like.
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
#undef FELUCCA_UAC                /* usb.c (through hostsim.c) was compiled with the input off; the probe
                                   * only needs the master's own tap, so the override is made explicit here */
#define FELUCCA_UAC 1
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

#include <math.h>
#include <stdarg.h>

#define PI2 6.283185307179586

static int bad;
/* variadic on purpose: these lines print dB, Q15 counts, Hz and integers, and a fixed (double, double)
 * signature made every "%d" read a double -- undefined, so the line printed 0 whatever the value was. */
static void check(const char *what, int ok, const char *fmt, ...)
{
    va_list ap;
    printf("%-58s %s", what, ok ? "ok  " : "FAIL");
    if (fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    printf("\n");
    if (!ok) bad++;
}

/* ---------------------------------------------------------------- PHASER ---
 * A 1 kHz tone through cho_phaser, the LFO driven a sample at a time as the block runs it. The
 * notches sweep past the tone, so the output envelope swings by however deep the notches are:
 * that swing is the effect. A phaser that "did little" reads as a swing of a decibel or two. */
/* The sweep frozen at 16 LFO positions, each run at 32 log-spaced tones from 200 Hz to 6 kHz: the chain's
 * gain at that position. Freezing it is what lets a narrow notch be seen at all -- smearing the moving
 * sweep into one measurement misses a notch that is sharp and brief, which is how the first version of
 * this file came to call the phaser 1.7 dB deep. */
static double phaser_gain_at(double hz, uint32_t p, int32_t dep)
{
    int i;
    double acc = 0;
    memset(&fx, 0, sizeof fx);
    for (i = 0; i < 8192; i++) {                   /* 4096 to settle, 4096 measured */
        int32_t x = (int32_t)(sin(PI2 * hz * i / FS) * 8000.0), y, sd = 0;
        y = cho_phaser(x, p, dep, &sd);
        if (i >= 4096) acc += (double)y * y;
    }
    return sqrt(acc / 4096.0);
}
/* One frozen sweep position, one DEPTH: the comb depth there over the 32 tones, and the gains it
 * spans (the peak is the chain's own level, the floor is the notch). */
static double phaser_comb_at(uint32_t ph, int32_t dep, double *lo_out, double *hi_out)
{
    double lo = 1e9, hi = 0;
    int f;
    for (f = 0; f < 32; f++) {
        double hz = 200.0 * pow(30.0, f / 31.0);   /* 200 Hz .. 6 kHz, log spaced */
        double g = phaser_gain_at(hz, ph, dep);
        if (g < lo) lo = g;
        if (g > hi) hi = g;
    }
    *lo_out = lo;
    *hi_out = hi;
    return lo > 1e-9 ? 20.0 * log10(hi / lo) : 200.0;
}

/* The sweep is frozen at 16 positions and the tone is swept instead: the deepest point of the comb
 * at that position, anywhere from 200 Hz to 6 kHz. Testing one tone across the sweep misses a null
 * that is narrow in frequency, and testing one sweep position can put the null between two tones;
 * this is the notch depth the ear actually hears on material with harmonics. */
static void probe_phaser(void)
{
    /* DEPTH as the knob delivers it: pdep = G_CDEPTH * 96, 0..127 -> 0..12192, and the coefficient the
     * sweep moves is a = 0.20 .. 0.95 at the top. The mix is dry + the full chain, so a notch cancels; a
     * half-dry mix could never beat 6 dB however the filter is built, which is why the depth is checked
     * over the whole knob and not at one point. */
    static const int KNOB[] = {13, 26, 38, 51, 64, 76, 89, 101, 114, 127};
    double lo_all = 1e9, hi_all = 0, deepest_worst = 200.0, deepest_best = 0, at_worst = 0;
    int32_t peak_delta = 0, prev = 0;
    int q, p, i;

    for (q = 0; q < (int)(sizeof KNOB / sizeof KNOB[0]); q++) {
        int32_t dep = KNOB[q] * 96;
        double deepest = 0, lo = 1e9, hi = 0;
        for (p = 0; p < 16; p++) {
            uint32_t ph = (uint32_t)((uint64_t)p << 28);   /* a turn is 2^32: 16 positions */
            double pl, pg, d = phaser_comb_at(ph, dep, &pl, &pg);
            if (d > deepest) deepest = d;
            if (pl < lo) lo = pl;
            if (pg > hi) hi = pg;
            if (pl < lo_all) lo_all = pl;
            if (pg > hi_all) hi_all = pg;
        }
        printf("       DEPTH %3d: deepest comb %5.1f dB, peak gain %.0f (Q15)\n", KNOB[q], deepest, hi);
        if (deepest < deepest_worst) { deepest_worst = deepest; at_worst = KNOB[q]; }
        if (deepest > deepest_best) deepest_best = deepest;
    }
    printf("       the comb runs %.1f .. %.1f dB over the DEPTH knob; gain %.0f .. %.0f (Q15)\n",
           deepest_worst, deepest_best, lo_all, hi_all);
    /* 20 dB is the gate: a phaser reads as "little intensity" at 6 dB -- which is also the most a half-dry
     * mix can reach, since a notch that cancels completely still leaves half the dry signal -- and it reads
     * as the effect from ~15 dB up. Checked at every DEPTH, not only at the top. */
    check("PHASER: the comb is >= 20 dB deep at every DEPTH (a 0.20..0.95)", deepest_worst >= 20.0,
          " %.1f dB (worst: DEPTH %d)", deepest_worst, (int)at_worst);
    check("PHASER: the chain stays bounded (no runaway feedback)", hi_all < 40000, " %.0f", hi_all);
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
          " %d", peak_delta);
}

/* -------------------------------------------------------------- FLANGER ---
 * (4.0) CHORUS > TYPE FLANGER: the same line read at 48 .. 302 samples, the output written back at 0.6.
 * The probe drives the line exactly as the block does (cho_buf's write, cho_flanger's tap) and freezes the
 * LFO; the gain across a log frequency sweep at each of eight positions is the comb the ear hears. Two
 * things have to be true for it to be a flanger and not a louder chorus: the comb must have depth, and
 * the notches must MOVE with the sweep. */
static double flanger_gain_at(double hz, uint32_t p)
{
    int i;
    double acc = 0;
    memset(&fx, 0, sizeof fx);
    memset(cho_buf, 0, sizeof cho_buf);
    for (i = 0; i < 8192; i++) {                   /* 4096 to settle, 4096 measured */
        int32_t x = (int32_t)(sin(PI2 * hz * i / FS) * 8000.0);
        uint32_t ci = fx.cho_w & (CHO_LEN - 1u);
        int32_t y;
        cho_buf[ci] = (int16_t)clamp((x >> 1) + mulq15(fx.flg_fb, 19661), -32768, 32767);
        y = cho_flanger(cho_buf, fx.cho_w, p, 127 * 512, &fx.flg_fb);
        fx.cho_w++;
        if (i >= 4096) acc += (double)y * y;
    }
    return sqrt(acc / 4096.0);
}

static void probe_flanger(void)
{
    double deepest = 0, hi_all = 0, first_notch = 0, last_notch = 0;
    int p, f;
    for (p = 0; p < 8; p++) {
        uint32_t ph = (uint32_t)((uint64_t)p << 29);   /* a turn is 2^32: 8 positions */
        double lo = 1e9, hi = 0, lo_hz = 0;
        for (f = 0; f < 48; f++) {
            double hz = 150.0 * pow(40.0, f / 47.0);   /* 150 Hz .. 6 kHz, log spaced */
            double g = flanger_gain_at(hz, ph);
            if (g < lo) { lo = g; lo_hz = hz; }
            if (g > hi) hi = g;
        }
        if (20.0 * log10(hi / (lo + 1e-9)) > deepest) deepest = 20.0 * log10(hi / (lo + 1e-9));
        if (hi > hi_all) hi_all = hi;
        if (p == 0) first_notch = lo_hz;
        if (p == 7) last_notch = lo_hz;
    }
    printf("       comb null at LFO 0: %.0f Hz; at LFO 7: %.0f Hz\n", first_notch, last_notch);
    printf("       deepest comb %.1f dB; peak gain %.0f (Q15)\n", deepest, hi_all);
    check("FLANGER: the comb has depth (>= 10 dB)", deepest >= 10.0, " %.1f dB", deepest);
    check("FLANGER: the comb stays bounded (0.6 feedback, no runaway)", hi_all < 40000, " %.0f", hi_all);
    /* the notches travel: a comb frozen in place is a static colour, not a flanger */
    check("FLANGER: the null moves with the sweep (>= 20 % between LFO 0 and LFO 7)",
          first_notch > 0 && (last_notch > first_notch ? last_notch / first_notch : first_notch / last_notch) >= 1.2,
          " %.0f -> %.0f Hz", first_notch, last_notch);
}

/* ---------------------------------------------------------------- ROUTE ---
 * (4.0) The ROUTING screen draws one path: a part's insert, its three sends, the three buses and the master.
 * This probe walks the buses themselves (fx_buses, the function the audio ISR calls) to pin the two facts the
 * screen claims: a send with audio in it raises that bus's own input meter and comes back out of it, and a bus
 * nothing was sent to returns nothing -- so a dark bar means no audio crossed it, not that the display hid it. */
static void probe_route(void)
{
    static int32_t ci[CTL], di[CTL], ri[CTL], wet[2 * CTL];
    int b, i;
    memset(&fx, 0, sizeof fx);
    memset(trk, 0, sizeof trk);
    song.g[G_CDEPTH] = 100;
    song.g[G_CTYPE] = 0;
    song.g[G_DMIX] = 64;
    song.g[G_RSIZE] = 64;
    song.g[G_RTYPE] = 0;
    for (b = 0; b < 40; b++) {                      /* ~40 blocks: the chorus line fills, the meters settle */
        for (i = 0; i < (int)CTL; i++) {
            double t = ((double)b * CTL + (double)i) / FS;
            ci[i] = (int32_t)(sin(PI2 * 440.0 * t) * 12000.0);   /* only the chorus bus is fed */
            di[i] = ri[i] = 0;
        }
        fx_buses(ci, di, ri, wet, CTL);
    }
    printf("       in %d / %d / %d; out %d / %d / %d\n", fx_vu[0], fx_vu[1], fx_vu[2], fx_vu[3], fx_vu[4], fx_vu[5]);
    check("ROUTE: a fed send raises its own input meter only", fx_vu[0] > 0 && !fx_vu[1] && !fx_vu[2], NULL);
    check("ROUTE: the bus it feeds returns audio", fx_vu[3] > 0, " %d", (int)fx_vu[3]);
    check("ROUTE: a bus nobody feeds returns nothing", !fx_vu[4] && !fx_vu[5], NULL);
}

/* ----------------------------------------------------------- M/S WIDTH ---
 * Two decorrelated sources, one hard left and one hard right, through master_out. At 64 the
 * width is bypassed; at 0 the sides are gone (mono); at 127 they are nearly doubled. The
 * correlation of the output says which of those actually happens. */
static double probe_width(int mwidth, double *lvl, int mono)
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
        r = mono ? l : (int32_t)((s2 >> 17) & 0x3fff) - 8192;   /* (5.0) a mono source: S is 0 */
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
    double c0, c64, c127, l0, l64, l127, lm64, lm127, cm127;
    c0 = probe_width(0, &l0, 0);
    c64 = probe_width(64, &l64, 0);
    c127 = probe_width(127, &l127, 0);
    probe_width(64, &lm64, 1);                        /* (5.0) on a mono source: only its level is used */
    cm127 = probe_width(127, &lm127, 1);
    printf("       correlation: width 0 %.3f, 64 %.3f, 127 %.3f (level %.0f / %.0f / %.0f)\n",
           c0, c64, c127, l0, l64, l127);
    check("M-WIDE 0 is mono (L and R identical)", c0 > 0.999, " r=%.4f", c0);
    check("M-WIDE 64 is the untouched pair", c64 < 0.1, " r=%.4f", c64);
    check("M-WIDE 127 is wider than 64", c127 < c64 - 0.05, " r=%.4f", c127);
    /* (5.0) wider is not louder: the pair keeps its power, so a turn of the knob cannot push the master's
     * limiter and pump the mix. Was +3.8 dB at 127 before the compensation. */
    check("M-WIDE 127 keeps the level of 64 (within 1 dB)", fabs(20.0 * log10(l127 / l64)) < 1.0,
          " %.1f dB", 20.0 * log10(l127 / l64));
    check("M-WIDE on a mono source: it stays mono, and to within 4 dB of its level", cm127 > 0.999 &&
          fabs(20.0 * log10(lm127 / lm64)) < 4.0, " r=%.4f %.1f dB", cm127, 20.0 * log10(lm127 / lm64));
}

/* ------------------------------------------------------------------- LOWCUT ---
 * (5.0) The master's low cut, as a tone at two frequencies through master_out: 1 kHz must come out at the
 * level it went in (within 2 dB) in every profile, and 60 Hz must be the one that goes away -- a high-pass
 * that silenced the midrange would look exactly like a broken cut. This is the "HPF silences the audio"
 * complaint measured instead of argued. */
static double probe_lowcut(int profile, double hz)
{
    uint32_t i;
    double acc = 0, phase = 0, inc = PI2 * hz / FS;
    fx_lowcut = (uint8_t)profile;
    dce_l = dce_r = 0;
    lim_env = 0;
    lev_cur = 32768;
    lev_dg = 0;
    for (i = 0; i < 40000; i++) {
        int32_t l = (int32_t)(6000.0 * sin(phase)), r = l;
        phase += inc;
        if (phase > PI2) phase -= PI2;
        if (i < 20000) continue;                        /* the filter's own settling, not measured */
        master_out(&l, &r);
        acc += (double)l * l;
    }
    fx_lowcut = 0;
    return sqrt(acc / 20000.0);
}

static void probe_lowcut_all(void)
{
    double tone0, tone1, tone2, low0, low1, low2;
    tone0 = probe_lowcut(0, 1000.0);
    tone1 = probe_lowcut(1, 1000.0);
    tone2 = probe_lowcut(2, 1000.0);
    low0 = probe_lowcut(0, 60.0);
    low1 = probe_lowcut(1, 60.0);
    low2 = probe_lowcut(2, 60.0);
    printf("       1 kHz: off %.0f, LOWCUT %.0f, BASS+ %.0f (rms)\n", tone0, tone1, tone2);
    printf("       60 Hz: off %.0f, LOWCUT %.0f, BASS+ %.0f (rms)\n", low0, low1, low2);
    check("LOWCUT and BASS+ leave 1 kHz where it was (within 2 dB)",
          fabs(20.0 * log10(tone1 / tone0)) < 2.0 && fabs(20.0 * log10(tone2 / tone0)) < 2.0,
          " %.1f / %.1f dB", 20.0 * log10(tone1 / tone0), 20.0 * log10(tone2 / tone0));
    /* both are high-passes -- BASS+ (a small speaker) uses the wider step, sh 5 against 6, so its corner is
     * the higher one and its own bass shelf puts some of the 60 Hz back: what must hold is that the cut is
     * there, that LOWCUT is the deeper of the two, and that neither comes near silencing anything */
    check("both take the 60 Hz tone down, LOWCUT the deeper one, neither near silence",
          20.0 * log10(low1 / low0) < -6.0 && 20.0 * log10(low2 / low0) < -2.0 &&
          low1 < low2 && 20.0 * log10(low1 / low0) > -20.0,
          " %.1f / %.1f dB", 20.0 * log10(low1 / low0), 20.0 * log10(low2 / low0));
}

/* ------------------------------------------------------------------ EQ ---
 * The master EQ as the mixer applies it, not as its coefficients describe it. A low shelf at -24 dB
 * should fall by about 24 dB below its corner and stay flat two octaves above it; if the shelf is
 * "aggressive" that is visible as a corner that cuts well above its frequency, or as a shelf that
 * never flattens.
 *
 * Reading fx_master_eq_hz_db (what the graph draws) is not enough: it builds a filter of its own and
 * cannot see what the audio path actually holds. That is how a wrong shelf went unnoticed -- the curve
 * was right and the sound was not. This feeds a tone through master_eq, the struct master_begin()
 * updates and qg_biquad_process() runs, and compares RMS out with RMS in. */
static int32_t eq_bl[CTL], eq_br[CTL];
static double eq_db(double hz)
{
    const double amp = 6000.0, w = PI2 * hz / FS;
    double in = 0, out = 0;
    long n = 0;
    int b, i, per = (int)(FS / hz + 0.5), prime, measure;
    if (per < 8) per = 8;
    master_eq_update();                       /* the same call master_begin() makes every block */
    qg_biquad_clear_state(&master_eq);
    prime = per * 60 / CTL + 32;              /* 60 cycles to settle a 20 Hz pole, then 30 measured */
    measure = prime / 2 + 32;
    for (b = 0; b < prime + measure; b++) {
        for (i = 0; i < CTL; i++) {
            int32_t x = (int32_t)(amp * sin(w * (double)(n + i)));
            eq_bl[i] = eq_br[i] = x;
        }
        n += CTL;
        qg_biquad_process(&master_eq, eq_bl, eq_br, CTL);
        if (b >= prime) {
            for (i = 0; i < CTL; i++) {
                double x = amp * sin(w * (double)(n - CTL + i));
                in += x * x;
                out += (double)eq_bl[i] * (double)eq_bl[i];
            }
        }
    }
    if (in <= 0 || out <= 0)
        return -999.0;
    return 10.0 * log10(out / in);
}

static void probe_eq(void)
{
    static const double HZ[7] = {30, 60, 120, 250, 1000, 4000, 16000};
    double low_db, top_db, peak_db;
    int i, shelf_ok;
    song.g[G_EQMODE] = 3;                          /* LOW SH */
    song.g[G_EQFREQ] = 48;
    song.g[G_EQGAIN] = -24;
    song.g[G_EQQ] = 64;
    printf("       LOW SH -24 dB curve (through the audio path):");
    for (i = 0; i < 7; i++)
        printf(" %g:%+.1f", HZ[i], eq_db(HZ[i]));
    printf("\n");
    low_db = eq_db(30);
    top_db = eq_db(4000);
    /* a low shelf must not dip on the way up, and must not overshoot 0 above the corner: a bump in
     * the mids is what a shelf reads as when its Q is wrong */
    {
        int monotone = 1, overshoot = 1;
        double prev = -99;
        for (i = 20; i <= 20000; i = i * 3 / 2) {
            double d = eq_db((double)i);
            if (d < prev - 0.3) monotone = 0;       /* went down: not a shelf */
            if (d > 0.5) overshoot = 0;             /* above unity somewhere in the band */
            prev = d;
        }
        shelf_ok = monotone && overshoot;
    }
    check("EQ LOW SH -24: the bottom is cut near -24 dB", low_db < -18.0 && low_db > -30.0, " %.1f dB", low_db);
    check("EQ LOW SH: rises monotonically and never overshoots 0", shelf_ok, NULL);
    check("EQ LOW SH: it does flatten out (4 kHz within 2 dB of 0)", fabs(top_db) < 2.0, " %.1f dB", top_db);
    /* (v1.7.0) and it is still the LOW SH after a profile change: the LOWCUT / BASS+ shelf was written
     * into master_eq by qg_fx_update_eq(), so picking a profile replaced the EQ (and eq_last_* kept
     * saying it was applied, so nothing rebuilt it). Sound and graph disagreed; this is that */
    {
        double at30 = eq_db(30), lowcut, bass;
        qg_fx_update_eq(1);                       /* MENU > LOWCUT, as the panel and the menu call it */
        lowcut = eq_db(30);
        qg_fx_update_eq(2);                       /* MENU > BASS+ */
        bass = eq_db(30);
        qg_fx_update_eq(0);
        check("EQ LOW SH survives a LOWCUT / BASS+ profile change", fabs(lowcut - at30) < 1.0 && fabs(bass - at30) < 1.0,
              " %.1f / %.1f dB", lowcut, bass);
    }

    song.g[G_EQGAIN] = 24;
    low_db = eq_db(30);
    check("EQ LOW SH +24: the bottom is lifted near +24 dB", low_db > 18.0 && low_db < 30.0, " %.1f dB", low_db);

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
    check("EQ PEAK +12 reaches its gain at the center", peak_db > 10.0 && peak_db < 13.0, " %.1f dB", peak_db);
    song.g[G_EQMODE] = 0;
}

int main(void)
{
    memset(trk, 0, sizeof trk);
    song.g[G_MWIDTH] = 64;
    /* SINE lives in .bss and voices_boot() decodes it with sine_init(); a harness that skips that boot reads
     * sin = 0 for every angle above 0.1 rad. This probe ran without it, so the phaser's and the flanger's LFO
     * never moved and the EQ's cosines collapsed to 1: three "failures" that were the instrument, not the DSP.
     * The other EQ tests already call it for this reason (eq_response_test.c). */
    sine_init();
    printf("fx_probe: host x86, %.0f Hz\n\n", (double)FS);
    probe_phaser();
    probe_flanger();
    probe_route();
    probe_mwidth();
    probe_lowcut_all();
    probe_eq();
    printf("\n%s\n", bad ? "FX PROBE FAILED" : "fx probe passed");
    return bad ? 1 : 0;
}
