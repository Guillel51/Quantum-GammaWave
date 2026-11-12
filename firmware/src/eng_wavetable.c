/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* WAVETABLE: a compact single-cycle wavetable oscillator with a morphing position (QG-1 3.0, phase 3).
 *
 * The table (tools/gen_tables.py, WT_DATA) holds eight single-cycle frames of 256 points: SINE, TRIANGLE,
 * SAW, SQUARE, PULSE 25 %, HOLLOW (odd harmonics, 1/k^3), METAL (1 3 5 7 9 11 13) and VOCAL (two formant
 * peaks). Every frame is a sum of sines of the cycle's harmonic, phase 0, normalised to the same peak, so a
 * morph between two neighbours is a level-steady interpolation and the frames line up at the cycle's start.
 *
 * MORPH (E1, MD_E1 in the matrix) is where the oscillator sits on that road: the two frames it falls between
 * and its fraction, both read per block, so an LFO or an envelope moves the timbre smoothly. The frames are
 * stored in three band limits (WT_LIMIT: 64, 16 and 5 harmonics) and the level follows the note, so the top
 * of the keyboard carries no aliasing: nothing above ~11 kHz is ever read into the wave.
 *
 * HARM (E8) tilts the spectrum at the output: a one-pole split at ~1 kHz whose high half is added or taken
 * away (0 = soft, 64 = as the frame is, 127 = thin and bright). It is continuous, so a sweep never steps.
 * SUB (E4) adds a sine an octave down. UNI (E3) a second copy a few cents away (DETUNE, E2): the pair beats,
 * the level is halved so it sits as the single one does. CUT / RES (E5, E6) are a 2-pole SVF low pass, as in
 * ANALOG: the parameters are the same ones, key-tracked, and ENV / LFO -> CUT still move it (m->cutoff).
 * DRIVE (E7) soft-clips the voice before the filter. A saturator fed by a sine and its sub (two frequencies
 * that do not line up in a half cycle) puts a DC offset on the waveform, so the voice ends in a DC blocker
 * (a one-pole high pass at 5 Hz, inaudible, 3 operations a sample): ten voices of a bass patch would
 * otherwise carry a few percent of a voice's full scale as offset and lose that headroom.
 *
 * Cost: per sample one 2-point interpolation per frame (two frames) plus the sub sine, the tilt and the
 * filter: no per-voice tables beyond the state in voice_t (three phases, three states). */
#define WT_L1_INC 33554432u       /* ~344 Hz: at most 64 harmonics (22 kHz) under it */
#define WT_L2_INC 134414336u      /* ~1.38 kHz: at most 16 above it, then 5 */

static const char *const N_WT_UNI[] = {"1 VOICE", "2 VOICES"};

/* a frame's point: linear interpolation between two of its 256 values */
static inline int32_t wt_pt(uint32_t l, uint32_t f, uint32_t ph)
{
    uint32_t i = ph >> 24, x = (ph >> 16) & 255u;
    int32_t a = WT_GET(l, f, i), b = WT_GET(l, f, (i + 1u) & 255u);
    return a + (((b - a) * (int32_t)x) >> 8);
}

/* the two frames MORPH falls between, and its fraction (Q8) */
static void wt_pos(int32_t morph, uint32_t *f0, uint32_t *f1, int32_t *fx)
{
    int32_t q = clamp(morph, 0, 127) * (WT_FRAMES - 1);     /* 0..889 */
    *f0 = (uint32_t)(q / 127);
    *f1 = *f0 + 1u < WT_FRAMES ? *f0 + 1u : *f0;
    *fx = (q % 127) * 256 / 127;
}

static void wt_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    /* (the filter and the tilt keep: engine_t.keep) */
}

static __attribute__((noinline)) void wt_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t lvl = m->inc < WT_L1_INC ? 0u : m->inc < WT_L2_INC ? 1u : 2u;
    uint32_t f0, f1, i, inc, inc2, ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t fx, uni = p[P_E2] != 0, sub = p[P_E3] * 258, tilt = (p[P_E7] - 64) * 384;
    int32_t cut = (p[P_E4] << 8) + m->cutoff + ((v->pitch16 - 60 * 16) * 4);
    int32_t drv = p[P_E6], drive = 32768 + drv * 512, ic1 = v->s[0], ic2 = v->s[1], lp = v->s[2];
    int32_t dcx = v->s[3], dcy = v->s[4];
    tsvf_t flt;
    if (n > CTL)
        n = CTL;
    wt_pos(p[P_E0], &f0, &f1, &fx);
    tsvf_coef(&flt, cut, p[P_E5]);
    inc = m->inc;
    inc2 = uni && p[P_E1] ? cents_inc(m->pitch16, p[P_E1], 0) : inc;
    for (i = 0; i < n; i++) {
        int32_t a = wt_pt(lvl, f0, ph0), b = wt_pt(lvl, f1, ph0), s;
        s = a + (((b - a) * fx) >> 8);
        if (uni) {
            int32_t c = wt_pt(lvl, f0, ph1), d = wt_pt(lvl, f1, ph1);
            s = (s + c + (((d - c) * fx) >> 8)) >> 1;
            ph1 += inc2;
        }
        ph0 += inc;
        if (sub) {
            s += (SINE[ph2 >> 22] * sub) >> 15;
            ph2 += inc >> 1;
        }
        lp += (s - lp) >> 6;                              /* the tilt's split, ~1 kHz */
        if (tilt)
            s += mulq15(tilt, s - lp);
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        s = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) * 2;
        {                                                 /* the DC blocker (~5 Hz) */
            int32_t y = s - dcx + (((dcy * 32744) + 16384) >> 15);   /* rounded: a floored
                                                          * multiply would bias the loop by
                                                          * ±1 LSB / (1 - R) = 685 */
            dcx = s;
            dcy = y;
            s = y;
        }
        out[i] += voice_amp(s, m, i) * 2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = lp;
    v->s[3] = dcx;
    v->s[4] = dcy;
}

static const preset_t WT_PRESETS[] = {
    /* name, {MORPH, DETUNE, UNI, SUB, CUT, RES, DRIVE, HARM}, {A D S R}, fenv, mono */
    {"GLASS PAD", {18, 22, 1, 10, 92, 10, 0, 76}, {25, 70, 110, 75}, 0, 0, FX(0, 48, 42, 72), PAT(5),
     {XP(P_LRATE, 16), XP(P_LWAVE, 6), XP(P_LD_FLT, 12), XP(P_LD_AMP, 6)}},
    {"MORPH LEAD", {54, 12, 1, 0, 104, 24, 18, 80}, {3, 60, 110, 35}, 0, 1, FX(20, 22, 26, 26), PAT(4),
     {XP(P_LRATE, 40), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 18)}},
    {"SINE KEY", {0, 0, 0, 0, 118, 0, 0, 40}, {2, 55, 100, 40}, 0, 0, FX(0, 12, 14, 24), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 20)}},
    {"VOCAL AAH", {127, 8, 1, 4, 88, 30, 6, 66}, {35, 80, 127, 70}, 0, 0, FX(0, 36, 32, 64), PAT(5),
     {XP(P_LRATE, 18), XP(P_LWAVE, 0), XP(P_LD_FLT, 12)}},
    {"METAL BELL", {98, 0, 0, 0, 116, 12, 0, 92}, {0, 50, 40, 55}, 0, 0, FX(0, 24, 22, 40), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 20)}},
    {"HOLLOW WOOD", {72, 6, 0, 0, 96, 44, 10, 30}, {0, 60, 70, 45}, 0, 0, FX(10, 14, 16, 28), PAT(2),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 24)}},
    {"BRASS SWELL", {36, 18, 1, 0, 78, 20, 30, 88}, {45, 75, 127, 60}, 0, 0, FX(26, 32, 22, 48), PAT(6),
     {XP(P_LRATE, 20), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 16)}},
    {"SUB BASS", {10, 0, 0, 90, 60, 26, 8, 20}, {0, 55, 120, 35}, 0, 1, FX(20, 0, 0, 10), PAT(2),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 18)}},
    {"PULSE ORGAN", {78, 14, 1, 0, 100, 8, 0, 58}, {0, 64, 127, 40}, 0, 0, FX(0, 32, 20, 34), PAT(5),
     {XP(P_LRATE, 18), XP(P_LWAVE, 1), XP(P_LD_AMP, 6)}},
    {"QUANTUM MASS", {44, 26, 1, 30, 66, 36, 22, 84}, {55, 90, 127, 95}, 0, 0, FX(30, 52, 46, 88), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 6), XP(P_LD_FLT, 18), XP(P_LD_AMP, 8)}},
    /* (6.0) the remaster's new sounds: one per family, each with its own motion and sends */
    {"VELVET MORPH", {40, 26, 1, 8, 86, 12, 10, 70}, {46, 88, 106, 88}, 8, 0, FX(0, 58, 30, 76), PAT(5),
     {XP(P_LRATE, 15), XP(P_LWAVE, 6), XP(P_LD_FLT, 14), XP(P_LD_AMP, 6)}},  /* B: pad */
    {"SPECTRAL REED", {58, 14, 1, 0, 100, 26, 16, 62}, {6, 80, 84, 56}, 20, 1, FX(14, 24, 36, 40), PAT(4),
     {XP(P_LRATE, 36), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 7), XP(P_M1DST, 11), XP(P_M1AMT, 8)}},  /* A: reed lead */
    {"CHROME PLUCK", {84, 10, 0, 0, 110, 30, 20, 88}, {0, 78, 0, 52}, 26, 0, FX(10, 18, 48, 32), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 24)}},  /* PLUCK */
    {"BARRO Y LUZ", {28, 32, 1, 12, 74, 18, 6, 54}, {70, 92, 112, 98}, 6, 0, FX(0, 62, 34, 84), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 6), XP(P_LD_FLT, 16), XP(P_M1SRC, 7), XP(P_M1DST, 2), XP(P_M1AMT, 8)}},  /* G: pad */
};

static const engine_t ENG_WAVETABLE = {
    .name = "WTABLE",                                      /* six characters: the engine field fits six */
    .page_title = {"TABLE", "TONE"},
    .edit = {
        {"MORPH", F_PCT, 0, 127, 0, 0, 0},               /* MD_E1: the matrix's first engine destination */
        {"DETUNE", F_PCT, 0, 127, 20, 0, 0},
        {"UNI", F_ENUM, 0, 1, 1, N_WT_UNI, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_PCT, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 8, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = WT_PRESETS,
    .npresets = NELEM(WT_PRESETS),
    .note_on = wt_note_on,
    .render = wt_render,
    .knob = {P_E0, P_E7, P_E4, P_E2},
    .keep = 0x1f,                                        /* the filter's two states, the tilt, the DC blocker */
};
