/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ADDITIVE: a real additive engine, eight partials per voice, each with its own phase and its own
 * spectral envelope. QG-1 1.2.6.
 *
 * The eight engines before it are all "one oscillator, shaped": ANALOG and PHASE filter or wrap a
 * waveform, FM6 and WAVETABLE build the spectrum out of one phase, WHEEL is additive but fixed to the
 * nine tonewheel footages of an organ. This one sums eight sines whose harmonic numbers, amplitudes and
 * decay are the sound:
 *
 *   SPECTR (E0)    SPECTRUM: where the eight amplitudes sit on a road of eight profiles (ORGAN, SAW, SQUARE,
 *                  BELL, GLASS, REED, HOLLOW, METAL), interpolated per block like WAVETABLE's MORPH,
 *                  so the matrix's E1 and an LFO sweep the timbre instead of switching it. The
 *                  profiles themselves are 64 B of flash: eight harmonics, one byte each.
 *   TILT     (E1)  scales each partial by harmonic number (dark at 0, bright at 127): partial k
 *                  gets 1 + (TILT - 64) * k / 2 %-ish, clamped, so a sweep is continuous.
 *   DECAY    (E2)  spectral decay: partial k's amplitude falls by DECAY * k per block, so the upper
 *                  partials die first (the way a struck body does) while the fundamental holds.
 *                  This is what a filtered noise burst cannot do: it is per partial, not per voice.
 *   STRETCH  (E3)  inharmonicity: partial k is k * (1 + STRETCH * (k^2 - 1) / 8128), up to ~1.5x at
 *                  the eighth when fully stretched, so the same spectrum turns from organ to bell.
 *   CUT / RES / DRIVE / SUB: the same 2-pole filter, soft drive and octave-down sine as WAVETABLE and
 *                  ANALOG, so a sound from here sits in the same mix as the others.
 *
 * Partial amplitudes are the profile's, tilted and normalised to a total of 32767 (Q15), once per
 * block and part (add_block); each voice carries the eight phases (32 B) and the eight decay counters
 * (16 B) keyed by its pool slot, as WHEEL does. A partial whose gain is 0 costs one phase addition and
 * no table read, so a bell that has decayed to its fundamental costs 1/8 of its attack.
 *
 * Band limit: a partial is faded out between ~4.5 and 6 kHz and not rendered above it, rather than
 * folded down an octave (the tonewheel organ's trick): a folded partial would sit between the
 * harmonics, and this engine's whole point is that its spectrum is exactly its partials.
 *
 * Cost: per sample, eight table reads and multiply-adds (fewer once the top partials have decayed)
 * plus one 2-pole filter and a DC blocker; the profile, tilt, decay and stretch run once per block.
 * Measurement: tests/eng_bench (host x86-64, read with the other nine), tests/additive_test.c. */
#define AD_NP 8
#define AD_LO 438304000u          /* ~4.5 kHz: from here up a partial is faded out (band limiting) */
#define AD_HI 584366000u          /* ~6 kHz: at and above it a partial is not rendered at all */

/* the eight profiles, one byte per harmonic, in the order SPECTRUM walks them. Each is a family the
 * other engines cannot make on their own: odd-only drawbars, a 1/k saw, a 1/k odd square, an
 * inharmonic bell (with STRETCH), a bright glass, a formant reed, a hollow third, a metal cluster. */
static const uint8_t AD_PROF[AD_NP][AD_NP] = {
    {127, 0, 66, 0, 34, 0, 18, 0},        /* ORGAN */
    {127, 64, 42, 32, 25, 21, 18, 16},    /* SAW */
    {127, 0, 42, 0, 25, 0, 18, 0},        /* SQUARE */
    {127, 0, 26, 0, 62, 34, 0, 46},       /* BELL */
    {127, 18, 48, 74, 92, 60, 34, 22},    /* GLASS */
    {127, 40, 88, 30, 64, 20, 38, 12},    /* REED */
    {127, 0, 96, 0, 0, 0, 22, 0},         /* HOLLOW */
    {127, 96, 74, 110, 52, 88, 40, 66},   /* METAL */
};

typedef struct {                  /* per part, once per block (add_block) */
    int32_t g[AD_NP];             /* the partial gains, Q15, normalised to 32767 in total */
    uint32_t eph;                 /* (CHARACTER) EVOLVE: the slow sine that walks the spectrum */
} ad_trk_t;

typedef struct {                  /* per voice, keyed by pool slot (as WHEEL's drw_v): 68 B */
    uint32_t ph[AD_NP];           /* 32 B: one phase a partial (STRETCH makes them inharmonic) */
    uint16_t dk[AD_NP];           /* 16 B: the decay left, Q16 (65535 = the profile as SPECTRUM has it) */
    int16_t gp[AD_NP];            /* 16 B: each partial's gain at the end of the last block (ramp start) */
    uint32_t sub;                 /* .. the octave-down sine (SUB) */
} ad_vc_t;

static ad_trk_t ad_t[NPART];
static ad_vc_t ad_v[VPOOLN];      /* one entry per pool slot: it follows the voice */

static uint32_t ad_part(const track_t *t) { return (uint32_t)(t - trk); }   /* NTRK == NPART */

/* once per block and part: where the morph falls between two profiles, the tilt, and the eight gains
 * normalised to 32767. Nothing here runs per sample. */
static void add_block(track_t *t)
{
    ad_trk_t *T = &ad_t[ad_part(t)];
    const int16_t *p = t->p;
    int32_t pr = clamp(p[P_E0], 0, 127) * (AD_NP - 1);   /* 0..889 */
    int32_t tilt = p[P_E1] - 64, ev = xamt(p, 0), sc[AD_NP], sum = 0;
    uint32_t f0, f1, k, scale;
    int32_t fx;
    if (ev) {                                             /* (CHARACTER) EVOLVE: SPECTRUM drifts, slowly */
        T->eph += 0x00200000u;                            /* ~1.4 s a cycle */
        pr = clamp(pr + ((sine_i(T->eph) * ev) >> 14), 0, 127 * (AD_NP - 1));
    }
    f0 = (uint32_t)(pr / 127);
    f1 = f0 + 1u < AD_NP ? f0 + 1u : f0;
    fx = (pr % 127) * 256 / 127;                          /* Q8 between the two profiles */
    for (k = 0; k < AD_NP; k++) {
        int32_t a = AD_PROF[f0][k] * 258, b = AD_PROF[f1][k] * 258;   /* 0..32766, Q15 */
        int32_t v = a + (((b - a) * fx) >> 8);
        if (tilt) {                                   /* the tilt: partial k a factor (1 + tilt * k / 32) */
            int32_t q = 1024 + tilt * (int32_t)(k + 1u) * 16;
            v = (v * (q < 96 ? 96 : q > 8192 ? 8192 : q)) >> 10;
        }
        sc[k] = v;
        sum += v;
    }
    /* normalised to a total of 32767 (Q15): a spectrum's level does not jump when SPECTRUM moves, and
     * one partial alone is as loud as eight */
    scale = sum > 0 ? (32767u << 8) / (uint32_t)sum : 0u;
    for (k = 0; k < AD_NP; k++)
        T->g[k] = (int32_t)(((uint32_t)sc[k] * scale) >> 8);
}

static void add_note_on(track_t *t, voice_t *v)
{
    ad_vc_t *V = &ad_v[VSLOT(v)];
    uint32_t k;
    (void)t;
    for (k = 0; k < AD_NP; k++) {
        V->ph[k] = 0;                                 /* every partial starts at the cycle's start */
        V->dk[k] = 65535;
        V->gp[k] = 0;
    }
    V->sub = 0;
}

static __attribute__((noinline)) void add_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const ad_trk_t *T = &ad_t[ad_part(t)];
    ad_vc_t *V = &ad_v[VSLOT(v)];
    const int16_t *p = t->p;
    int32_t acc[CTL], ic1 = v->s[0], ic2 = v->s[1], dcx = v->s[2], dcy = v->s[3];
    int32_t cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) * 4);
    int32_t drv = p[P_E6], drive = 32768 + drv * 512;
    int32_t sub = p[P_E7] * 258, stretch = clamp(p[P_E3], 0, 127), dec = clamp(p[P_E2], 0, 127);
    uint32_t i, k, sph = V->sub;
    tsvf_t flt;
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++)
        acc[i] = 0;
    tsvf_coef(&flt, cut, p[P_E5]);
    for (k = 0; k < AD_NP; k++) {
        uint32_t inc = m->inc, ph = V->ph[k];
        uint32_t d = V->dk[k];
        int32_t g1, g0;
        /* the harmonic number: k + 1, stretched up as STRETCH asks (k^2 - 1 grows faster than k, so the
         * upper partials go sharp together, a bell's signature) */
        if (k) {
            /* (k + 1) times the note, stretched: rq is that harmonic number in 1/1024ths, plus up to
             * ~0.5 of it more by the eighth partial at STRETCH 127 */
            uint32_t rq = (k + 1u) * 1024u +
                          (uint32_t)(((uint32_t)(k + 1u) * (k + 1u) - 1u) * (uint32_t)stretch * 48u) / 127u;
            inc = (uint32_t)(((uint64_t)inc * rq) >> 10);    /* rq / 1024: 1 .. 8.5 */
        }
        /* the decay: the fundamental (k = 0) never decays, and harmonic k + 1 loses DECAY * k / 4 of
         * 32768 per block, so the upper partials die first while the fundamental holds. At DECAY 127 the
         * eighth is down ~70 dB in a second; at 0 nothing decays (an organ). Measured in
         * tests/additive_test: at DECAY 90, harmonic 4 at -27.6 dB and harmonic 1 at -0.1 dB after 0.8 s. */
        d -= (d * (uint32_t)((dec * (int32_t)k) >> 2)) >> 15;   /* (DECAY * k / 4) / 32768 a block */
        V->dk[k] = (uint16_t)d;
        g1 = mulq15(T->g[k], (int32_t)(d >> 1));       /* Q15: the profile times the decay left */
        /* band limiting: a partial between 4.5 and 6 kHz fades out (the slope, once a block), and one
         * above 6 kHz is not rendered. Folding it down an octave, as the tonewheel organ's top does,
         * would put it between the harmonics: this engine sums sines, so it can simply stop in time. */
        if (inc >= AD_HI)
            g1 = 0;
        else if (inc > AD_LO)
            g1 = (int32_t)(((uint32_t)g1 * ((AD_HI - inc) >> 19)) / 280u);
        g0 = V->gp[k];                                 /* the block's first sample: the last block's */
        V->gp[k] = (int16_t)g1;
        if (!g0 && !g1) {
            V->ph[k] = ph + inc * n;
            continue;
        }
        if (g0 == g1) {
            for (i = 0; i < n; i++) {
                ph += inc;
                acc[i] += (SINE[ph >> 22] * g1) >> 15;
            }
        } else {                                      /* a gain ramp over the block: no step at its edge */
            int32_t gq = g0 << 5, dg = g1 - g0;
            for (i = 0; i < n; i++) {
                ph += inc;
                gq += dg;
                acc[i] += (SINE[ph >> 22] * (gq >> 5)) >> 15;
            }
        }
        V->ph[k] = ph;
    }
    if (sub) {                                        /* SUB: a sine an octave down */
        uint32_t inc = m->inc >> 1;
        for (i = 0; i < n; i++) {
            sph += inc;
            acc[i] += (SINE[sph >> 22] * sub) >> 15;
        }
    }
    V->sub = sph;
    for (i = 0; i < n; i++) {
        int32_t s = acc[i];
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) * 2;
        {                                             /* the DC blocker (~5 Hz), rounded as WAVETABLE's */
            int32_t y = s - dcx + dc_pole(dcy);
            dcx = s;
            dcy = y;
            s = y;
        }
        out[i] += voice_amp(s, m, i);
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = dcx;
    v->s[3] = dcy;
}

static const preset_t AD_PRESETS[] = {
    /* name, {SPECTRUM, TILT, DECAY, STRETCH, CUT, RES, DRIVE, SUB}, {A D S R}, fenv, mono */
    {"ADDTV ORGAN", {0, 64, 0, 0, 108, 10, 0, 0}, {0, 64, 127, 45}, 0, 0, FX(0, 26, 8, 28), PAT(5),
     {XP(P_LRATE, 18), XP(P_LWAVE, 1), XP(P_LD_AMP, 6)}},
    {"GLASS BELL", {55, 78, 78, 42, 118, 14, 0, 0}, {0, 72, 24, 66}, 8, 0, FX(0, 30, 26, 62), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 14), XP(P_LEVEL, 118)}},
    {"CRYSTAL PLUCK", {44, 90, 56, 20, 110, 26, 22, 28}, {0, 76, 0, 40}, 18, 0, FX(8, 20, 40, 34), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 22)}},
    {"SPECTRAL PAD", {14, 54, 8, 12, 82, 24, 4, 12}, {52, 88, 120, 92}, 0, 0, FX(0, 52, 32, 84), PAT(5),
     {XP(P_LRATE, 11), XP(P_LWAVE, 6), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 30), XP(P_LD_FLT, 14)}},
    {"TINE EP", {24, 66, 58, 0, 100, 8, 2, 0}, {0, 66, 40, 44}, 20, 0, FX(0, 16, 22, 32), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 16)}},
    {"GONG DEEP", {62, 40, 48, 96, 92, 18, 10, 24}, {0, 84, 12, 96}, 6, 0, FX(6, 34, 30, 88), PAT(7),
     {XP(P_LRATE, -1), XP(P_LWAVE, 6), XP(P_LD_FLT, 16)}},
    {"HARMONIC BASS", {1, 44, 26, 0, 58, 30, 26, 92}, {0, 58, 112, 30}, 0, 1, FX(22, 0, 0, 8), PAT(2),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 20)}},
    {"METAL CLUSTER", {127, 96, 64, 74, 114, 40, 40, 26}, {2, 70, 30, 58}, 22, 0, FX(24, 12, 28, 44), PAT(1),
     {XP(P_LRATE, 44), XP(P_LWAVE, 4), XP(P_M1SRC, 7), XP(P_M1DST, 3), XP(P_M1AMT, 10)}},
    /* (1.3.4) the two sounds the bank asked for: odd partials through a soft knee make a reed a reed (SPECTR low,
     * TILT up), and STRETCH holds a choir's partials apart while the slow LFO opens its filter */
    {"REED HORN", {8, 92, 20, 30, 76, 44, 18, 20}, {18, 80, 100, 50}, 35, 1, FX(0, 18, 24, 30), PAT(6),
     {XP(P_LRATE, 26), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 12)}},
    {"AIR CHOIR", {26, 30, 10, 74, 96, 12, 0, 0}, {70, 92, 116, 96}, -20, 0, FX(0, 58, 36, 80), PAT(4),
     {XP(P_LRATE, 6), XP(P_LWAVE, 6), XP(P_LD_AMP, 8), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 26)}},
};

/* the UI's bars (ui_graph.c graph_additive): the gain each partial has in this part's last block. The
 * engine owns its state, so the graph asks instead of reaching into ad_t. */
int32_t ad_partial(const track_t *t, uint32_t k) { return k < AD_NP ? ad_t[ad_part(t)].g[k] : 0; }

static const param_desc_t AD_CH[1] = {
    {"EVOLVE", F_PCT, 0, 127, 127, 0, 0},            /* (CHARACTER) a slow walk of SPECTRUM, inverted as the
                                                      * first two slots are: 127 still, down from there drifts */
};

static const engine_t ENG_ADDITIVE = {
    .name = "ADDTV",                                 /* six characters: the engine field fits six */
    .page_title = {"PARTIAL", "TONE", "CHAR"},
    .edit = {
        {"SPECTR", F_PCT, 0, 127, 0, 0, 0},          /* MD_E1: the matrix's first engine destination */
        {"TILT", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 0, 0, 0},
        {"STRETCH", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_PCT, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 8, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = AD_PRESETS,
    .npresets = NELEM(AD_PRESETS),
    .ch = AD_CH,
    .ch_n = 1,
    .note_on = add_note_on,
    .render = add_render,
    .knob = {P_E0, P_E1, P_E4, P_E7},
    .block = add_block,
    .keep = 0x0f,                                    /* the filter's and the blocker's state */
};
