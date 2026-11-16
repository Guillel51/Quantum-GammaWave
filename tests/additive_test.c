/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ADDITIVE engine test (src/eng_additive.c) on the host, through hostsim.c as regress.c.
 *   build/host/additive_test
 * 1. the profiles: SPECTRUM 0 (ORGAN) has no even harmonics; SPECTRUM 18 (SAW) has its 2nd at half the
 *    1st and its 3rd at a third, and a position between two profiles is a crossfade, not a jump.
 * 2. TILT tilts the spectrum: the 2..8 kHz energy over the fundamental rises at least 6 dB from 0 to 127
 *    and is monotonic in between (a continuous control).
 * 3. DECAY is spectral: at DECAY 90 the 4th partial has fallen a lot a second in while the fundamental
 *    is still within 3 dB (the way a struck body behaves).
 * 4. STRETCH makes the partials inharmonic: at 127 the 3rd partial sits more than 3 % above 3 f0.
 * 5. SUB adds the octave below; the level of a note does not depend on SPECTRUM (the normalisation).
 * 6. aliasing: the top notes carry nothing that is not a harmonic.
 * 7. health: every preset (10 notes, velocity 127) has no DC, no clipping beyond 2 x VOICE_FS a voice
 *    and ends silent; every EDIT parameter at its maximum, 8 notes, does not clip; a note from silence
 *    renders the same samples twice.
 * 8. cost: ns per sample per voice (8 notes held, the idle mix taken off); printed, fails above AD_COST_MAX.
 * 9. the descriptors: SPECTRUM is the first EDIT parameter (MD_E1) and the engine is listed. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 8192
#define AD_COST_MAX 900.0         /* host ns per sample and voice (well above ANALOG's; printed) */

static int bad;
static double y[NFFT * 2];        /* the samples of the note being measured (the FFT reads a window of it) */
static void check(const char *what, int ok)
{
    printf("additive: %-82s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t eng_additive(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_ADDITIVE)
            return e;
    return 0;
}

static void setup(const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_additive(), 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e ? e[i] : ENGINES[eng_additive()]->edit[i].def;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_ED_FLT] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_LEVEL] = 112;
    trk[0].p[P_X0] = trk[0].p[P_X1] = trk[0].p[P_X2] = trk[0].p[P_X3] = 127;   /* CHARACTER neutral */
    trk[0].p[P_MORPH] = 0;
}

/* the voices' sum of one note (0.1 s after the key) as doubles */
static void voice_render(const int16_t *e, uint32_t note, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, skip = FS / 10u / CTL;
    setup(e);
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < skip; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
}

/* the averaged power spectrum (Hann) of y */
static void spectrum(const double *y, uint32_t nf, double *ps)
{
    static double re[NFFT], im[NFFT];
    uint32_t f, i;
    memset(ps, 0, sizeof(double) * NFFT / 2);
    for (f = 0; f < nf; f++) {
        for (i = 0; i < NFFT; i++) {
            double w = y[f * NFFT + i];
            re[i] = w * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
            im[i] = 0;
        }
        fft(re, im, NFFT);
        for (i = 0; i < NFFT / 2; i++)
            ps[i] += re[i] * re[i] + im[i] * im[i];
    }
}

static double at(const double *ps, double f)
{
    uint32_t c = (uint32_t)(f / FS * NFFT), w = (uint32_t)(f * 0.012 / FS * NFFT), i;
    double m = 0;
    if (w < 3)
        w = 3;
    for (i = c > w ? c - w : 1; i <= c + w && i < NFFT / 2; i++)
        if (ps[i] > m)
            m = ps[i];
    return m;
}
static double aat(const double *ps, double f) { return sqrt(at(ps, f)); }
static double band(const double *ps, double f1, double f2)
{
    uint32_t a = (uint32_t)(f1 / FS * NFFT), b = (uint32_t)(f2 / FS * NFFT), i;
    double s = 0;
    for (i = a; i < b && i < NFFT / 2; i++)
        s += ps[i];
    return s;
}

/* a spectrum: the profile, at C3 (130.8 Hz), the tilt centred and no decay */
static const int16_t E_ORGAN[8] = {0, 64, 0, 0, 127, 0, 0, 0};
static const int16_t E_SAW[8] = {18, 64, 0, 0, 127, 0, 0, 0};
#define F0 130.81

/* one note (48) of these EDIT values, and its spectrum: every check below reads it this way */
static void spec_of(const int16_t *e, double *ps)
{
    voice_render(e, 48, y, NFFT * 2);
    spectrum(y, 2, ps);
}

static void profiles(void)
{
    static double ps[NFFT / 2];
    double h1, h2, h4;
    spec_of(E_ORGAN, ps);
    h1 = aat(ps, F0); h2 = aat(ps, 2 * F0); h4 = aat(ps, 4 * F0);
    check("SPECTRUM 0 (ORGAN) is odd only: the 2nd and 4th under 4 % of the 1st",
          h1 > 0 && h2 < h1 * 4e-2 && h4 < h1 * 4e-2);
    spec_of(E_SAW, ps);
    h1 = aat(ps, F0); h2 = aat(ps, 2 * F0);
    {
        double h3 = aat(ps, 3 * F0), r2 = h2 / h1, r3 = h3 / h1;
        char what[176];
        snprintf(what, sizeof what, "SPECTRUM 18 (SAW) is 1/k: the 2nd at %.2f of the 1st (0.5 +-25 %%), the 3rd at %.2f",
                 r2, r3);
        check(what, r2 > 0.37 && r2 < 0.63 && r3 > 0.22 && r3 < 0.45);
    }
    /* between two profiles: half way between ORGAN and SAW the 2nd sits between their amplitudes */
    {
        int16_t e[8];
        double rA, rM, rB;
        memcpy(e, E_SAW, sizeof e);
        e[0] = 0;
        spec_of(e, ps);
        rA = aat(ps, 2 * F0) / aat(ps, F0);
        e[0] = 9;                                /* between ORGAN (0) and SAW (1): pr = 63, half way */
        spec_of(e, ps);
        rM = aat(ps, 2 * F0) / aat(ps, F0);
        e[0] = 18;
        spec_of(e, ps);
        rB = aat(ps, 2 * F0) / aat(ps, F0);
        check("a SPECTRUM position between two profiles is a crossfade (the 2nd between theirs)",
              rM > rA * 1.2 && rM < rB * 0.9);
    }
}

static void tilt(void)
{
    static double ps[NFFT / 2];
    int16_t e[8];
    double r0, r64, r127;
    memcpy(e, E_SAW, sizeof e);
    e[2] = 0;
    e[1] = 0;
    spec_of(e, ps);
    r0 = band(ps, 4 * F0, 9 * F0) / at(ps, F0);           /* harmonics 4..8 over the fundamental */
    e[1] = 64;
    spec_of(e, ps);
    r64 = band(ps, 4 * F0, 9 * F0) / at(ps, F0);
    e[1] = 127;
    spec_of(e, ps);
    r127 = band(ps, 4 * F0, 9 * F0) / at(ps, F0);
    {
        char what[176];
        snprintf(what, sizeof what, "TILT: harmonics 4..8 over the 1st %.1f dB up from 0 to 127, monotonic through 64",
                 10 * log10((r127 + 1e-30) / (r0 + 1e-30)));
        check(what, 10 * log10(r127 / r0) > 6 && r0 < r64 && r64 < r127);
    }
}

static void decay(void)
{
    int16_t e[8];
    const double c4 = 261.63;                          /* C4: the 4th partial at 1046.5 Hz */
    double early4, late4, early1, late1;
    memcpy(e, E_SAW, sizeof e);
    e[2] = 90;
    setup(e);
    {
        static int32_t b[CTL];
        static double ps[NFFT / 2];
        uint32_t i, j;
        trk_note_on(&trk[0], 60, 100);
        for (i = 0; i < 4u; i++) track_render(&trk[0], b, CTL);
        for (i = 0; i < 2u * NFFT / CTL; i++) {
            track_render(&trk[0], b, CTL);
            for (j = 0; j < CTL; j++) y[i * CTL + j] = b[j] / 32768.0;
        }
        spectrum(y, 2, ps);
        early1 = aat(ps, c4);
        early4 = aat(ps, 4 * c4);
        for (i = 0; i < (uint32_t)(0.8 * FS) / CTL; i++) track_render(&trk[0], b, CTL);
        for (i = 0; i < 2u * NFFT / CTL; i++) {
            track_render(&trk[0], b, CTL);
            for (j = 0; j < CTL; j++) y[i * CTL + j] = b[j] / 32768.0;
        }
        spectrum(y, 2, ps);
        late1 = aat(ps, c4);
        late4 = aat(ps, 4 * c4);
    }
    {
        char what[190];
        double d4 = 20 * log10((late4 + 1e-30) / (early4 + 1e-30)), d1 = 20 * log10((late1 + 1e-30) / (early1 + 1e-30));
        snprintf(what, sizeof what, "DECAY 90 is spectral: the 4th at %.1f dB and the 1st at %.1f dB after 0.8 s "
                 "(-20 dB and -3 dB or better)", d4, d1);
        check(what, d4 < -20 && d1 > -3.0);
    }
}

static void stretch(void)
{
    static double ps[NFFT / 2];
    int16_t e[8];
    double at3, off, f3 = 3 * F0;
    memcpy(e, E_SAW, sizeof e);
    e[3] = 127;
    spec_of(e, ps);
    at3 = aat(ps, f3);
    off = aat(ps, f3 * 1.125);                     /* where STRETCH 127 puts it: 3.375 f0, not 3 */
    {
        char what[176];
        snprintf(what, sizeof what, "STRETCH 127 moves the 3rd partial to 3.375 f0 (%.1f there, %.2f at 3 f0)",
                 off, at3);
        check(what, off > at3 * 4.0);
    }
    /* and the level does not depend on SPECTRUM (the profile normalisation) */
    {
        double a, b2;
        int16_t f[8];
        memcpy(f, E_ORGAN, sizeof f);
        spec_of(f, ps);
        a = aat(ps, F0);
        memcpy(f, E_SAW, sizeof f);
        spec_of(f, ps);
        b2 = aat(ps, F0);
        check("the normalisation keeps the level across profiles (the fundamental within 6 dB)",
              a > 0 && b2 > 0 && fabs(20 * log10(b2 / a)) < 6.0);
    }
}

static void sub(void)
{
    static double ps[NFFT / 2];
    int16_t e[8];
    double oct, f0;
    memcpy(e, E_ORGAN, sizeof e);
    e[7] = 90;
    spec_of(e, ps);
    oct = at(ps, F0 / 2);
    f0 = at(ps, F0);
    check("SUB adds the octave below (it is the strongest partial under the note)", oct > f0 * 0.2);
}

/* (CHARACTER) EVOLVE is inverted like the first two slots are: 127 still, down from there the spectrum walks.
 * The polarity is not cosmetic -- at the wrong default a sound nobody touched would drift as it plays. */
static void evolve_render(int16_t x0, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k;
    setup(0);
    trk[0].p[P_X0] = x0;
    trk_note_on(&trk[0], 60, 100);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

static void evolve(void)
{
    static double a[FS / 2], b[FS / 2], c[FS / 2];
    uint32_t i, n = FS / 2u;
    double still = 0, drift = 0;
    char what[128];
    evolve_render(127, a, n);
    evolve_render(127, b, n);
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            still = 1.0;
    evolve_render(0, c, n);
    for (i = 0; i < n; i++)
        if (fabs(c[i] - a[i]) > drift)
            drift = fabs(c[i] - a[i]);
    check("EVOLVE at its default (127) is still: the same sound twice, sample for sample", !still);
    snprintf(what, sizeof what, "EVOLVE down from it walks the spectrum (difference %.3f of full scale)", drift);
    check(what, drift > 0.02);
}

static int only_harmonics(const double *ps, double f0)
{
    double mx = 0;
    uint32_t i, n = 0;
    for (i = 1; i < NFFT / 2; i++)
        if (ps[i] > mx)
            mx = ps[i];
    if (mx <= 0)
        return 0;
    for (i = 2; i + 2 < NFFT / 2; i++) {
        double f, h, d;
        if (ps[i] < mx * 0.01 || ps[i] < ps[i - 1] || ps[i] < ps[i + 1])
            continue;
        f = (double)i * FS / NFFT;
        h = floor(f / f0 + 0.5);
        d = fabs(f - h * f0);
        if (d > 0.02 * h * f0 + (double)FS / NFFT)
            return 0;
        n++;
    }
    return n > 0;
}

static void aliasing(void)
{
    static double ps[NFFT / 2];
    uint32_t n;
    static const uint32_t NOTE[3] = {96, 105, 108};
    static const double FREQ[3] = {2093.0, 3520.0, 4186.01};
    int ok = 1;
    for (n = 0; n < 3u; n++) {
        voice_render(E_SAW, NOTE[n], y, NFFT * 2);
        spectrum(y, 2, ps);
        if (!only_harmonics(ps, FREQ[n])) {
            ok = 0;
            printf("additive:   %0.f Hz carries a bin that is not a harmonic\n", FREQ[n]);
        }
    }
    check("C7, A7 and C8 carry nothing but harmonics (the fold keeps the top below 6 kHz)", ok);
}

static void health(void)
{
    static int32_t b[CTL];
    uint32_t pi, i, k, nclip = 0, ndc = 0, nsil = 0;
    for (pi = 0; pi < ENGINES[eng_additive()]->npresets; pi++) {
        double sum = 0;
        int32_t peak = 0;
        uint32_t cnt = 0;
        int16_t e[8];
        for (i = 0; i < 8u; i++)
            e[i] = ENGINES[eng_additive()]->presets[pi].e[i];
        setup(e);
        for (i = 0; i < 10u; i++)
            trk_note_on(&trk[0], 60u + i * 2u, 127);
        for (i = 0; i < FS / 4; i += CTL)
            track_render(&trk[0], b, CTL);
        for (i = 0; i < 2u * FS; i += CTL) {
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++) {
                int32_t a = b[k] < 0 ? -b[k] : b[k];
                peak = a > peak ? a : peak;
                sum += b[k];
                cnt++;
            }
        }
        if (peak > 20 * VOICE_FS)
            nclip++;
        if (cnt && fabs(sum / cnt) > 0.02 * VOICE_FS) {
            ndc++;
            printf("additive:   %s: mean %.0f (%.2f %% of a voice's full scale)\n",
                   ENGINES[eng_additive()]->presets[pi].name, sum / cnt, 100 * fabs(sum / cnt) / VOICE_FS);
        }
        for (i = 0; i < 10u; i++)
            trk_note_off(&trk[0], 60u + i * 2u);
        for (i = 0; i < FS / 2; i += CTL)
            track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL; k++)
            if (b[k])
                break;
        if (k < CTL)
            nsil++;
    }
    {
        char what[200];
        snprintf(what, sizeof what, "all %u presets, 10 notes at velocity 127: %u clipping, %u with DC, %u not "
                 "silent after the release", ENGINES[eng_additive()]->npresets, nclip, ndc, nsil);
        check(what, nclip == 0 && ndc == 0 && nsil == 0);
    }
    {
        int16_t C[8];
        uint32_t c;
        int32_t peak = 0;
        for (c = 0; c < 8u; c++)
            C[c] = (int16_t)ENGINES[eng_additive()]->edit[c].max;
        setup(C);
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], 40u + i * 3u, 127);
        for (i = 0; i < FS / 4; i += CTL) {
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++) {
                int32_t a = b[k] < 0 ? -b[k] : b[k];
                peak = a > peak ? a : peak;
            }
        }
        {
            char what[190];
            snprintf(what, sizeof what, "every EDIT parameter at its maximum, 8 notes: %.2f x VOICE_FS a voice",
                     (double)peak / (double)VOICE_FS);
            check(what, peak < 22 * VOICE_FS * 8 / 10);
        }
    }
    {
        static double y1[NFFT], y2[NFFT];
        voice_render(E_SAW, 60, y1, NFFT);
        voice_render(E_SAW, 60, y2, NFFT);
        check("a note from silence renders the same samples again", !memcmp(y1, y2, sizeof y1));
    }
}

static void cost(void)
{
    static int32_t b[CTL];
    struct timespec t0, t1;
    uint32_t i, k, reps = 60;
    double solo, both;
    setup(0);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < reps; i++)
        for (k = 0; k < FS / CTL; k++)
            track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    both = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    setup(0);
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 48u + i * 2u, 100);
    for (i = 0; i < 4u; i++)
        track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < reps; i++)
        for (k = 0; k < FS / CTL; k++)
            track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    solo = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    {
        double per = (solo - both) / (reps * (double)FS * 8.0);
        printf("additive: %.0f ns per sample and voice on this host\n", per);
        check("cost within the host budget (printed above)", per < AD_COST_MAX);
    }
}

int main(void)
{
    uint32_t e = eng_additive();
    printf("== ADDTV (engine %u, %u presets)\n", e, ENGINES[e]->npresets);
    check("listed (eng_ok) and reachable with the PRESETS knob", eng_ok(e) && eng_rank(e) < NENG_SHOWN);
    check("EDIT 1 is SPECTR (SPECTRUM) first (MD_E1: the matrix can move the spectrum)",
          strcmp(ENGINES[e]->edit[0].label, "SPECTR") == 0 && ENGINES[e]->edit[0].max == 127);
    check("the CHARACTER page has EVOLVE (the slow walk of SPECTRUM) and 8 partials",
          ENGINES[e]->ch_n == 1 && ENGINES[e]->ch && strcmp(ENGINES[e]->ch[0].label, "EVOLVE") == 0);
    check("the engine is in the factory library with at least 8 sounds", engine_library(ENGINES[e]) >= 8);
    profiles();
    tilt();
    decay();
    stretch();
    sub();
    evolve();
    aliasing();
    health();
    cost();
    printf(bad ? "ADDITIVE: %d FAILED\n" : "ADDITIVE: all checks passed\n", bad);
    return bad != 0;
}
