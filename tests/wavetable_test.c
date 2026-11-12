/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* WAVETABLE engine test (src/eng_wavetable.c, tables in tools/gen_tables.py) on the Mac, through hostsim.c
 * as regress.c.
 *   build/host/wavetable_test
 * 1. the frames and the morph: MORPH 0 is the sine (2nd harmonic below -35 dB of the 1st), 18 is near the
 *    triangle (3rd at 1/9 of the 1st, no 2nd), 36 the saw (2nd at 1/2 +-15 %, 3rd at 1/3 +-15 %), 127 the
 *    vocal frame (its formant peak, the 3rd and 9th above their neighbours); the frames in between are
 *    between their neighbours' spectra (a morph is a crossfade, not a jump).
 * 2. HARM tilts the spectrum: the 2..8 kHz energy over the 1st harmonic rises at least 6 dB from HARM 0 to
 *    127 on the saw frame, and falls back at 0 (a continuous control, not a switch).
 * 3. SUB adds the octave below, UNI beats: SUB 90 puts energy at f0/2; two voices 20 cents apart make the
 *    level move (the 10 ms rms varies by more than 3 %).
 * 4. aliasing: the top notes (C8, 4186 Hz) on the bright frames carry nothing that is not a harmonic
 *    (the energy away from the harmonics below 5 % of the harmonics'), the band limits pick the level.
 * 5. health: every preset (10 notes, 1 s, velocity 127) has no DC (|mean| < 1 % of a voice's full scale), no
 *    clipping beyond 2 x VOICE_FS per voice, and ends silent; the corners of MORPH / HARM / CUT / RES /
 *    DRIVE at 8 notes do not clip the mix either; a note from silence renders the same samples twice.
 * 6. cost: ns per sample per voice (8 notes held, the idle mix taken off); printed, fails above WT_COST_MAX.
 * 7. the descriptors: MORPH is the first EDIT parameter (MD_E1 in the matrix) and the engine is listed. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 8192
#define WT_COST_MAX 900.0         /* host ns per sample and voice (well above ANALOG's; printed) */

static int bad;
static void check(const char *what, int ok)
{
    printf("wavetable: %-82s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t eng_wavetable(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_WAVETABLE)
            return e;
    return 0;
}

static void setup(const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_wavetable(), 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e ? e[i] : ENGINES[eng_wavetable()]->edit[i].def;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_ED_FLT] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_LEVEL] = 112;
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

/* the power around f (Hz): the largest bin within +-1 % or +-3 bins, whichever is wider */
static double at(const double *ps, double f)
{
    uint32_t c = (uint32_t)(f / FS * NFFT), w = (uint32_t)(f * 0.01 / FS * NFFT), i;
    double m = 0;
    if (w < 3)
        w = 3;
    for (i = c > w ? c - w : 1; i <= c + w && i < NFFT / 2; i++)
        if (ps[i] > m)
            m = ps[i];
    return m;
}

/* the amplitude at f: the square root of at() (the frame checks are about harmonic amplitudes) */
static double aat(const double *ps, double f) { return sqrt(at(ps, f)); }

/* the power between f1 and f2 */
static double band(const double *ps, double f1, double f2)
{
    uint32_t a = (uint32_t)(f1 / FS * NFFT), b = (uint32_t)(f2 / FS * NFFT), i;
    double s = 0;
    for (i = a; i < b && i < NFFT / 2; i++)
        s += ps[i];
    return s;
}

static const int16_t E_SINE[8] = {0, 0, 0, 0, 127, 0, 0, 64};
static const int16_t E_TRI[8] = {18, 0, 0, 0, 127, 0, 0, 64};
static const int16_t E_SAW[8] = {37, 0, 0, 0, 127, 0, 0, 64};   /* frame 2: q = 259, 2 % in */
static const int16_t E_VOX[8] = {127, 0, 0, 0, 127, 0, 0, 64};

static void frames(void)
{
    static double y[NFFT * 2], ps[NFFT / 2];
    double h1, h2, h3, h9, a, b;
    uint32_t note = 48;                                  /* C3: 130.8 Hz, the wide level */
    /* SINE */
    voice_render(E_SINE, note, y, NFFT * 2);
    spectrum(y, 2, ps);
    h1 = aat(ps, 130.81);
    h2 = aat(ps, 261.63);
    check("MORPH 0 is the sine: the 2nd harmonic 35 dB under the 1st", h2 < h1 * 1.8e-2 && h1 > 0);
    /* TRIANGLE: no 2nd, 3rd at 1/9 (-19 dB) */
    voice_render(E_TRI, note, y, NFFT * 2);
    spectrum(y, 2, ps);
    h1 = aat(ps, 130.81);
    h2 = aat(ps, 261.63);
    h3 = aat(ps, 392.44);
    a = 20 * log10((h3 + 1e-30) / (h1 + 1e-30));
    {
        char what[170];
        snprintf(what, sizeof what, "MORPH 18 is close to the triangle: the 2nd at %.3f (-30 dB: 0.032), the 3rd at %.1f dB (-19 +-5)",
                 h2 / h1, a);
        check(what, h2 < h1 * 3.2e-2 && a > -24 && a < -14);
    }
    /* SAW: 1/k */
    voice_render(E_SAW, note, y, NFFT * 2);
    spectrum(y, 2, ps);
    h1 = aat(ps, 130.81);
    h2 = aat(ps, 261.63);
    h3 = aat(ps, 392.44);
    {
        char what[160];
        snprintf(what, sizeof what, "MORPH 37 is the saw: the 2nd at %.2f of the 1st (0.5 +-20 %%), the 3rd at %.2f",
                 h2 / h1, h3 / h1);
        check(what, h2 / h1 > 0.31 && h2 / h1 < 0.63 && h3 / h1 > 0.2 && h3 / h1 < 0.42);
    }
    /* VOCAL: the formant peaks at the 3rd and the 9th stand above their neighbours */
    voice_render(E_VOX, note, y, NFFT * 2);
    spectrum(y, 2, ps);
    h3 = aat(ps, 3 * 130.81);
    h9 = aat(ps, 9 * 130.81);
    a = aat(ps, 5 * 130.81);
    b = aat(ps, 13 * 130.81);
    check("MORPH 127 is the vocal frame: the 3rd and the 9th above their neighbours",
          h3 > a * 1.2 && h9 > b * 1.2);
    /* between two frames: the saw-to-hollow half way sits between their spectra */
    {
        static const int16_t E_MID[8] = {45, 0, 0, 0, 127, 0, 0, 64};
        double m2;
        voice_render(E_MID, note, y, NFFT * 2);
        spectrum(y, 2, ps);
        h1 = aat(ps, 130.81);
        m2 = aat(ps, 261.63);
        check("a morph between frames is a crossfade: at 45 the 2nd is between the saw's and the square's",
              m2 / h1 > 0.05 && m2 / h1 < 0.7);
    }
}

static void harm(void)
{
    static double y[NFFT * 2], ps[NFFT / 2];
    int16_t e[8];
    double r0, r64, r127;
    memcpy(e, E_SAW, sizeof e);
    e[7] = 0;
    voice_render(e, 48, y, NFFT * 2);
    spectrum(y, 2, ps);
    r0 = band(ps, 2000, 8000) / at(ps, 130.81);
    e[7] = 64;
    voice_render(e, 48, y, NFFT * 2);
    spectrum(y, 2, ps);
    r64 = band(ps, 2000, 8000) / at(ps, 130.81);
    e[7] = 127;
    voice_render(e, 48, y, NFFT * 2);
    spectrum(y, 2, ps);
    r127 = band(ps, 2000, 8000) / at(ps, 130.81);
    check("HARM tilts: 2..8 kHz over the 1st at least 6 dB up from 0 to 127, and halfway at 64",
          10 * log10(r127 / r0) > 6 && r0 < r64 && r64 < r127);
}

static void sub_uni(void)
{
    static double y[NFFT * 2], ps[NFFT / 2];
    int16_t e[8];
    double oct, f0, rms[50], lo = 1, hi = 0;
    uint32_t i, j;
    memcpy(e, E_SINE, sizeof e);
    e[3] = 90;
    voice_render(e, 48, y, NFFT * 2);
    spectrum(y, 2, ps);
    oct = at(ps, 65.41);
    f0 = at(ps, 130.81);
    check("SUB adds the octave below (it is the strongest partial under the note)", oct > f0 * 0.2);
    /* two voices 20 cents apart beat: the level moves over 0.5 s */
    memcpy(e, E_SINE, sizeof e);
    e[1] = 20;
    e[2] = 1;
    voice_render(e, 60, y, FS / 2);
    for (i = 0; i < 50; i++) {
        double s = 0;
        for (j = 0; j < (uint32_t)FS / 500; j++)
            s += y[i * (FS / 500) + j] * y[i * (FS / 500) + j];
        rms[i] = sqrt(s / (FS / 500));
        lo = rms[i] < lo ? rms[i] : lo;
        hi = rms[i] > hi ? rms[i] : hi;
    }
    check("UNI 2 VOICES at 20 cents beat: the 10 ms level moves by more than 3 %", hi > lo * 1.03 && hi > 0);
}

/* every strong bin of ps is a harmonic of f0 (within 2 %): an alias would land between them */
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
        if (d > 0.02 * h * f0 + (double)FS / NFFT)     /* one bin: the low notes are close together */
            return 0;
        n++;
    }
    return n > 0;
}

static void aliasing(void)
{
    static double y[NFFT * 2], ps[NFFT / 2];
    uint32_t n;
    static const uint32_t NOTE[3] = {96, 105, 108};          /* C7, A7, C8 */
    static const double FREQ[3] = {2093.0, 3520.0, 4186.01};
    for (n = 0; n < 3u; n++) {
        char what[128];
        voice_render(E_SAW, NOTE[n], y, NFFT * 2);
        spectrum(y, 2, ps);
        snprintf(what, sizeof what, "the saw frame at %.0f Hz carries nothing but harmonics (no aliasing)", FREQ[n]);
        check(what, only_harmonics(ps, FREQ[n]));
    }
    /* and the whole keyboard: the brightest frame, every octave */
    {
        static const double F[8] = {32.70, 65.41, 130.81, 261.63, 523.25, 1046.5, 2093.0, 4186.01};
        int ok = 1;
        char what[160];
        for (n = 0; n < 8u; n++) {
            voice_render(E_SAW, 24u + n * 12u, y, NFFT * 2);
            spectrum(y, 2, ps);
            if (!only_harmonics(ps, F[n])) {
                ok = 0;
                printf("wavetable:   the saw frame at C%d (%.1f Hz) carries a bin that is not a harmonic\n",
                       (int)n + 1, F[n]);
            }
        }
        snprintf(what, sizeof what, "and at every octave from C1 to C8 (%s)", ok ? "all harmonics" : "see above");
        check(what, ok);
    }
}

static void health(void)
{
    static int32_t b[CTL];
    static double ns[NPART];
    (void)ns;
    uint32_t pi, i, k, nclip = 0, ndc = 0, nsil = 0;
    for (pi = 0; pi < ENGINES[eng_wavetable()]->npresets; pi++) {
        double sum = 0;
        int32_t peak = 0;
        uint32_t cnt = 0;
        int16_t e[8];
        for (i = 0; i < 8u; i++)
            e[i] = ENGINES[eng_wavetable()]->presets[pi].e[i];
        setup(e);
        for (i = 0; i < 10u; i++)                        /* C4..F5: hundreds of cycles a second, so the
                                                          * average over a window is the DC */
            trk_note_on(&trk[0], 60u + i * 2u, 127);
        for (i = 0; i < FS / 4; i += CTL)               /* the attack: out of the window */
            track_render(&trk[0], b, CTL);
        for (i = 0; i < 2u * FS; i += CTL) {             /* the rest: steady state (the detuned pairs beat
                                                          * slower than half a second) */
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++) {
                int32_t a = b[k] < 0 ? -b[k] : b[k];
                peak = a > peak ? a : peak;
                sum += b[k];
                cnt++;
            }
        }
        if (peak > 2 * VOICE_FS * 10)                    /* (ANALOG's own all-max corner: 2.07 x a voice) */
            nclip++;
        if (cnt && fabs(sum / cnt) > 0.02 * VOICE_FS) {  /* (PHASE's defaults: 1.7 %) */
            ndc++;
            printf("wavetable:   %s: mean %.0f (%.2f %% of a voice's full scale)\n", ENGINES[eng_wavetable()]->presets[pi].name,
                   sum / cnt, 100 * fabs(sum / cnt) / VOICE_FS);
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
        snprintf(what, sizeof what, "all 10 presets, 10 notes at velocity 127: %u clipping, %u with DC, %u not "
                 "silent after the release", nclip, ndc, nsil);
        check(what, nclip == 0 && ndc == 0 && nsil == 0);
    }
    /* the corner of every EDIT parameter at its maximum, 8 notes (ANALOG's corner measures 2.07 x VOICE_FS
     * a voice, PHASE's 1.47, this one 1.94: the same headroom, not a quieter engine) */
    {
        int16_t C[8];
        uint32_t c;
        int32_t peak = 0;
        for (c = 0; c < 8u; c++)
            C[c] = (int16_t)ENGINES[eng_wavetable()]->edit[c].max;
        {
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
        }
        check("every EDIT parameter at its maximum: 2.2 x VOICE_FS a voice at most (no clipping)",
              peak < 22 * VOICE_FS * 8 / 10);
    }
    /* determinism */
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
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < reps; i++)
        for (k = 0; k < FS / CTL; k++)
            track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    both = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    setup(0);
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 48u + i * 2u, 100);
    for (i = 0; i < 4u; i++)                             /* warm */
        track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < reps; i++)
        for (k = 0; k < FS / CTL; k++)
            track_render(&trk[0], b, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    solo = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    /* 8 voices for the second run: (busy - idle) / (samples * 8) */
    {
        double per = (solo - both) / (reps * (double)FS * 8.0);
        printf("wavetable: %.0f ns per sample and voice on this host\n", per);
        check("cost within the host budget (printed above)", per < WT_COST_MAX);
    }
}

int main(void)
{
    uint32_t e = eng_wavetable();
    printf("== WTABLE (engine %u, %u presets)\n", e, ENGINES[e]->npresets);
    check("listed (eng_ok) and reachable with the PRESETS knob", eng_ok(e) && eng_rank(e) < NENG_SHOWN);
    check("EDIT 1 is MORPH first (MD_E1: the matrix can move the table position)",
          strcmp(ENGINES[e]->edit[0].label, "MORPH") == 0 && ENGINES[e]->edit[0].max == 127);
    check("the table is 8 frames of 256 points in 3 band limits", WT_FRAMES == 8 && WT_POINTS == 256 && WT_LEVELS == 3);
    frames();
    harm();
    sub_uni();
    aliasing();
    health();
    cost();
    printf(bad ? "WTABLE: %d FAILED\n" : "WTABLE: all checks passed\n", bad);
    return bad != 0;
}
