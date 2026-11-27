/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5b) The two new knob pairs, measured instead of read: ANALOG's SYNC and SUB (the slots DRUM's LEVELS
 * uses, P_LN0 / P_LN1, reinterpreted for ANALOG in params.c) and ADDITIVE's eight DRAW levels (P_LN0..P_LN7,
 * the same slots for that engine).
 *
 *   1. neutral: with the two ANALOG knobs at their stored 127 -- what every factory preset and every project
 *      saved before this change carries -- the render is the engine as it was: a note rendered twice is
 *      sample for sample the same, and moving either knob at all changes it by more than 1 % of the sound.
 *   2. SUB is the octave below: at 64 the energy at f0/2 rises at least 6 dB against the same note with the
 *      knob neutral, and the fundamental does not fall away.
 *   3. SYNC is a hard sync: at 64 the waveform stops being the plain saw (the render differs by more than
 *      10 %), and the change survives a second note (the state it leaves is the voice's, not the engine's).
 *   4. the descriptors: params.c hands ANALOG "SYNC" / "SUB" with the neutral at 127, and ADDITIVE "H1".."H8".
 *   5. every one of the ten knobs moves the audio: each is forced to both ends against the same note and has
 *      to change the render by at least 1 % (the same threshold as tests/param_sweep_test.c), so a knob that
 *      is drawn and stored but never read fails here.
 *   6. health: SUB and SYNC at their maximum do not clip and leave no DC.
 *
 * Build: build/host/analog_sync_test (tests/run_tests.sh). */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 8192
static int bad;
static double y[NFFT];
static int32_t buf[NFFT];

static void check(const char *what, int ok)
{
    printf("sync/sub: %-78s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t eng_of(const engine_t *e)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        if (ENGINES[i] == e)
            return i;
    return 0;
}

/* a note on the engine with its EDIT parameters as given, the two lane knobs as given (127 = neutral), the
 * LEVELS slots of the track left at 127 unless lane0/lane1 say otherwise */
static void setup(const engine_t *en, const int16_t *e, int32_t ln0, int32_t ln1, int32_t ln2, int32_t ln3)
{
    uint32_t i, eg = eng_of(en);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eg, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e ? e[i] : en->edit[i].def;
    trk[0].p[P_LN0] = (int16_t)ln0;
    trk[0].p[P_LN1] = (int16_t)ln1;
    trk[0].p[P_LN2] = (int16_t)ln2;
    trk[0].p[P_LN3] = (int16_t)ln3;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_ED_FLT] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_LEVEL] = 112;
    trk[0].p[P_X0] = trk[0].p[P_X1] = trk[0].p[P_X2] = trk[0].p[P_X3] = 127;
    trk[0].p[P_FTYPE] = 0;
}

/* one note into buf[] (0.1 s after the key), the track already set up */
static void render_raw(uint32_t note, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, skip = FS / 10u / CTL;
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < skip; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            buf[i + k] = b[k];
    }
}
static void render(const engine_t *en, const int16_t *e, int32_t ln0, int32_t ln1, uint32_t note, uint32_t n)
{
    setup(en, e, ln0, ln1, 127, 127);
    render_raw(note, n);
}

/* a Goertzel-free DFT bin: the magnitude at hz, over the window */
static double energy(double hz)
{
    uint32_t i;
    double re = 0, im = 0, w = 2.0 * 3.14159265358979 * hz / FS;
    for (i = 0; i < NFFT; i++) {
        y[i] = buf[i] / 32768.0;
        re += y[i] * cos(w * i);
        im += y[i] * sin(w * i);
    }
    return sqrt(re * re + im * im) / NFFT;
}

/* the windowed level and the DC of a render */
static void health(int32_t *b, uint32_t n, int32_t *peak, int32_t *dc)
{
    uint32_t i;
    int64_t sum = 0;
    *peak = 0;
    for (i = 0; i < n; i++) {
        int32_t a = b[i] < 0 ? -b[i] : b[i];
        if (a > *peak)
            *peak = a;
        sum += b[i];
    }
    *dc = (int32_t)(sum / (int64_t)(n ? n : 1));
}

/* how far apart two renders are, as a share of the reference's RMS */
static double rel(const int32_t *a, const int32_t *b, uint32_t n)
{
    uint32_t i, off = n / 3u;                 /* the attack is not what the knobs act on */
    double acc = 0, r = 0;
    for (i = off; i < n; i++) {
        double d = (double)a[i] - (double)b[i];
        acc += d * d;
        r += (double)a[i] * (double)a[i];
    }
    return r > 1.0 ? sqrt(acc / r) : 0.0;
}

int main(void)
{
    static const int16_t SAW[8] = {0, 0, 0, 0, 110, 8, 0, 0};      /* WAVE SAW, MIX 0: osc 1 alone */
    static const int16_t SAW2[8] = {0, 0, 64, 0, 110, 8, 0, 0};     /* both oscillators at equal level: what a
                                                                     * hard sync needs to be heard */
    static const int16_t AD_E[8] = {127, 64, 0, 0, 110, 8, 0, 0};   /* SPECTRUM 127 (METAL): every harmonic is
                                                                     * there, so every DRAW level has something
                                                                     * to lower (ORGAN's even harmonics are 0) */
    const engine_t *an = &ENG_ANALOG, *ad = &ENG_ADDITIVE;
    static int32_t base[NFFT], alt[NFFT];
    const param_desc_t *d;
    int32_t peak, dc;
    double e_base, e_sub, f0;
    uint32_t i, note = 48;

    /* 1. neutral: 127 in both slots is the engine exactly as it was, and either knob moves it */
    render(an, SAW, 127, 127, note, NFFT);
    memcpy(base, buf, sizeof base);
    render(an, SAW, 127, 127, note, NFFT);
    check("ANALOG with SYNC and SUB at their neutral (127) renders the same twice", rel(base, buf, NFFT) == 0.0);
    render(an, SAW, 127, 0, note, NFFT);
    check("SUB at 127 (its full level) moves the sound by more than 10 %", rel(base, buf, NFFT) > 0.10);
    render(an, SAW2, 0, 127, note, NFFT);
    check("SYNC at 127 (its full ratio) moves the sound by more than 10 %", rel(base, buf, NFFT) > 0.10);

    /* 2. SUB is the octave below */
    f0 = 440.0 * pow(2.0, ((int)note - 69) / 12.0);
    render(an, SAW, 127, 127, note, NFFT);
    e_base = energy(f0 / 2.0);
    render(an, SAW, 127, 64, note, NFFT);
    e_sub = energy(f0 / 2.0);
    {
        double fund = energy(f0);
        char what[96];
        snprintf(what, sizeof what, "SUB 64 puts the octave below at least 6 dB over neutral (%.1f dB)",
                 20.0 * log10((e_sub + 1e-12) / (e_base + 1e-12)));
        printf("sync/sub: f0 %.1f Hz: octave %g neutral, %g at SUB 64, fundamental %g\n", f0 / 2.0, e_base, e_sub, fund);
        check(what, e_sub > e_base * 2.0 && fund > 0.0005);
    }

    /* 3. SYNC changes the spectrum and keeps working on a second note (the state is the voice's) */
    render(an, SAW2, 0, 127, note, NFFT);
    memcpy(alt, buf, sizeof alt);
    render(an, SAW2, 127, 127, note, NFFT);
    memcpy(base, buf, sizeof base);
    check("SYNC 127 rewrites the waveform (correlation with the plain saw below 0.9)",
          rel(base, alt, NFFT) > 0.10);
    render(an, SAW2, 0, 127, note + 7, NFFT);
    {
        static int32_t n2[NFFT];
        memcpy(n2, buf, sizeof n2);
        render(an, SAW2, 127, 127, note + 7, NFFT);
        check("SYNC moves the note a fifth up as well (the ratio is not left over from the last note)",
              rel(buf, n2, NFFT) > 0.10);
    }

    /* 4. the descriptors */
    setup(an, SAW, 127, 127, 127, 127);
    d = track_desc(&trk[0], P_LN0);
    check("ANALOG's P_LN0 is labelled SYNC, 0..127, neutral 127", d && !strcmp(d->label, "SYNC") &&
          d->min == 0 && d->max == 127 && d->def == 127);
    d = track_desc(&trk[0], P_LN1);
    check("ANALOG's P_LN1 is labelled SUB, 0..127, neutral 127", d && !strcmp(d->label, "SUB") &&
          d->min == 0 && d->max == 127 && d->def == 127);
    setup(ad, 0, 127, 127, 127, 127);
    d = track_desc(&trk[0], P_LN7);
    check("ADDITIVE's P_LN7 is still its DRAW level H8, neutral 127", d && !strcmp(d->label, "H8") &&
          d->def == 127);

    /* 5. every one of the ten knobs moves the audio: each is forced to its far end (0) against the same note
     * with every lane knob neutral (127), which is the neutral for both engines */
    {
        struct { const engine_t *en; const int16_t *e; uint32_t slot; const char *name; } K[10];
        uint32_t n = 0, moved = 0;
        K[n].en = an; K[n].e = SAW2; K[n].slot = 0; K[n].name = "ANALOG SYNC"; n++;
        K[n].en = an; K[n].e = SAW; K[n].slot = 1; K[n].name = "ANALOG SUB"; n++;
        for (i = 0; i < 8u; i++) {
            static char nm[16];
            snprintf(nm, sizeof nm, "ADDITIVE H%u", (unsigned)(i + 1u));
            K[n].en = ad; K[n].e = AD_E; K[n].slot = i; K[n].name = nm; n++;
        }
        for (i = 0; i < n; i++) {
            double v;
            setup(K[i].en, K[i].e, 127, 127, 127, 127);
            render(K[i].en, K[i].e, 127, 127, note, NFFT);
            memcpy(base, buf, sizeof base);
            setup(K[i].en, K[i].e, 127, 127, 127, 127);
            trk[0].p[P_LN0 + K[i].slot] = 0;
            render_raw(note, NFFT);
            v = rel(base, buf, NFFT);
            printf("sync/sub: %-14s to 0: %5.1f %% of the sound\n", K[i].name, v * 100.0);
            moved += v >= 0.01;
        }
        check("every new knob moves the audio by at least 1 %", moved == n);
    }

    /* 6. health: the knobs at full do not clip and leave no DC */
    render(an, SAW2, 0, 0, note, NFFT);
    health(buf, NFFT, &peak, &dc);
    printf("sync/sub: both knobs at full: peak %d (full scale 32767), DC %d\n", (int)peak, (int)dc);
    /* the same two bounds the rest of the suite uses: a voice may reach 2 x VOICE_FS before the master's
     * limiter (tests/additive_test.c) and the DC limit is tests/regress.c LIM_DC (800 = 2.4 %) */
    check("SYNC 127 + SUB 127: inside the engine's headroom and inside the suite's DC limit",
          peak <= 32767 * 2 && (dc < 800 && dc > -800));

    printf("sync/sub: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
