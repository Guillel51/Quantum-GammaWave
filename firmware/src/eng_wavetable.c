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
/* (FASE 3) the three phase transforms read 16 harmonics (a coarser frame), so they are alias-free up to ~689 Hz
 * (32 harmonics of the doubled read, or 32 samples of a held cycle, both at Nyquist) and fade out between ~459
 * and 689 Hz instead of folding anything back. */
#define WT_FX_HI 67108864u        /* 2^32 / 64: ~689 Hz */
#define WT_FX_LO 44739242u        /* ~459 Hz */

static const char *const N_WT_UNI[] = {"1 VOICE", "2 VOICES"};

/* (FASE 1) the two top band limits are rebuilt here instead of carried as flash: the frames' harmonic
 * amplitudes (WT_COEF, 2 048 B) and 8 192 B of .bss stand in for 8 192 B of stored wave. Once, before the
 * first note renders -- never inside a block, so a level change costs nothing at all at run time.
 * (FASE 6) the third limit (5 harmonics, every note above ~1.4 kHz) is built the same way: another
 * 4 096 B of .bss for the last 4 096 B of stored wave, so the whole wavetable is now 2 048 B of flash.
 * The stored table it replaces came out bit for bit identical (verified against the old WT_L5 before the
 * switch), so no render changed.
 * (FASE 1 de v1.5.22) each frame is a sum of sines of the cycle's harmonic, phase 0, so w[WT_POINTS - i] =
 * -w[i] holds: the .bss keeps one half cycle per frame (WT_PTS_H = 129 points) and wt_pts() reads the
 * second half backwards and negated. 6 096 B of .bss given back to the RAM budget (12 288 -> 6 192).
 * The round-half-up of the stored table left the two halves a LSB apart at some points; the wave now has
 * the exact odd symmetry its construction implies (measured: 1 LSB of +-32 000, 0.003 % of full scale). */
static uint8_t wt_ready;

/* (FASE 2/5, 2026-11-25) WT_COEF in .bss, 512 Q30 amplitudes expanded once from a 64-byte map and the 280 nonzero
 * values (1 184 B of XIP instead of 2 048 B). Same integers, same table, same render; the expansion is 512 mask
 * tests before the first wt_gen_init pass, never inside a block. */
int32_t WT_COEF[WT_COEF_N];
static void wt_coef_init(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < WT_COEF_N; i++)
        WT_COEF[i] = (WT_COEF_MASK[i >> 3] >> (i & 7u)) & 1u ? WT_COEF_NZ[n++] : 0;
}

/* a frame's point before its peak: the sum of that frame's sines (tools/gen_tables.py), Q15 */
static inline int32_t wt_raw(uint32_t f, uint32_t lim, uint32_t i)
{
    int64_t x = 0;
    uint32_t k;
    for (k = 1; k <= lim; k++)
        x += (int64_t)WT_COEF[(k - 1u) * WT_FRAMES + f] * SINE[((k * i) & (WT_POINTS - 1u)) * 4u];
    return (int32_t)(x >> 30);
}

/* (FASE 1) Two passes over each frame instead of a 1 024-byte scratch array in .bss: the peak first, then the
 * values normalised. The arithmetic is the same one, so the tables come out identical (measured: same hash);
 * the cost is paid once at boot, ~+0.17 ms on host, and it gives 1 024 B back to a .bss that had 924 B free */
static void wt_gen_init(void)
{
    uint32_t l, f, i;
    if (wt_ready)
        return;
    wt_coef_init();
    for (l = 0; l < WT_LEVELS; l++) {
        uint32_t lim = l == 0u ? 64u : l == 1u ? 16u : 5u;
        for (f = 0; f < WT_FRAMES; f++) {
            int32_t pk = 0;
            int16_t *dst = WT_HALF(l) + f * WT_PTS_H;
            for (i = 0; i < WT_PTS_H; i++) {           /* |w[WT_POINTS - i]| = |w[i]|: the half cycle holds
                                                        * the peak of the whole one */
                int32_t v = wt_raw(f, lim, i);
                if (v > pk) pk = v;
                else if (-v > pk) pk = -v;
            }
            if (!pk) pk = 1;
            for (i = 0; i < WT_PTS_H; i++)                 /* every frame normalised to the same peak: a morph
                                                            * keeps its level, as the stored ones did */
                dst[i] = (int16_t)(((int64_t)wt_raw(f, lim, i) * 32000 + pk / 2) / pk);
        }
    }
    wt_ready = 1;
}

/* two frames' points at phase ph: linear interpolation between two of the cycle's 256 values, from the half
 * the table keeps (w[WT_POINTS - i] = -w[i]). The second half is the first one walked backwards and negated:
 * one index calculation for both frames of a morph. */
static inline int32_t wt_pts(uint32_t l, uint32_t f0, uint32_t f1, uint32_t ph, int32_t *b1)
{
    const int16_t *h0 = WT_HALF(l) + f0 * WT_PTS_H, *h1 = WT_HALF(l) + f1 * WT_PTS_H;
    uint32_t i = ph >> 24, x = (ph >> 16) & 255u;
    if (i & 0x80u) {                           /* the second half */
        uint32_t n = WT_POINTS - i;            /* 1..128 */
        int32_t a = h0[n], c = h1[n];
        *b1 = -(c + (((h1[n - 1u] - c) * (int32_t)x) >> 8));
        return -(a + (((h0[n - 1u] - a) * (int32_t)x) >> 8));
    }
    {
        int32_t a = h0[i], c = h1[i];
        *b1 = c + (((h1[i + 1u] - c) * (int32_t)x) >> 8);
        return a + (((h0[i + 1u] - a) * (int32_t)x) >> 8);
    }
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
    wt_gen_init();                                 /* (FASE 1, FASE 6) once, on the first note:
                                                    * 12 288 B of .bss instead of 12 288 B of flash */
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
    int32_t wpa = xamt(p, 0), posa = xamt(p, 1), folda = p[P_MORPH];   /* (FASE 2) CHARACTER */
    int32_t pph = v->s[5], psm = v->s[6], morph = p[P_E0];
    /* (FASE 3) PHASE: which shape WARP's depth asks for. SINE (0) is the warp the engine has always had, bit for
     * bit; KNEE bends the read point asymmetrically, HOLD holds it for 32 steps of the cycle, X2 reads two
     * cycles and crossfades. The three read a coarser frame (16 harmonics) and fade out near the top of it. */
    uint32_t mode = (uint32_t)clamp(p[P_LRTRG], 0, 3);
    int32_t fxamt = wpa;
    if (mode && fxamt) {
        if (m->inc >= WT_FX_HI)
            fxamt = 0;
        else if (m->inc > WT_FX_LO)
            fxamt = fxamt * (int32_t)((WT_FX_HI - m->inc) / ((WT_FX_HI - WT_FX_LO) / 128u)) >> 7;
    }
    if (mode && fxamt) {
        if (lvl == 0u)                                 /* the transforms' images need the 16-harmonic frame */
            lvl = 1u;
    } else {
        mode = 0u;                                     /* no depth (or above the fade): the plain engine */
    }
    tsvf_t flt;
    if (n > CTL)
        n = CTL;
    if (posa) {                                       /* (FASE 2) POSMOD: MORPH itself, walked by a slow sine */
        pph += 0x00400000;
        psm += (((sine_i((uint32_t)pph) * posa) >> 10) - psm) >> 3;
        morph = clamp(morph + (psm >> 8), 0, 127);
    }
    wt_pos(morph, &f0, &f1, &fx);
    tsvf_coef(&flt, cut, p[P_E5]);
    inc = m->inc;
    inc2 = uni && p[P_E1] ? cents_inc(m->pitch16, p[P_E1], 0) : inc;
    for (i = 0; i < n; i++) {
        uint32_t wph = ph0;
        int32_t b, a, s;
        if (mode == 0u) {
            if (wpa)
                wph = ph0 + (uint32_t)(sine_i(ph0) * (wpa * 258));   /* (FASE 2) WARP: the read point bends, at
                                                                      * most a quarter of a cycle */
        } else if (fxamt) {
            if (mode == 1u) {                                      /* (FASE 3) KNEE: sin + half sin(2x): an
                                                                    * asymmetric bend, ~1/4 cycle at full depth */
                int32_t kn = sine_i(ph0) + (sine_i(ph0 * 2u) >> 1);
                wph = ph0 + (uint32_t)(kn * (fxamt * 172));
            } else if (mode == 2u) {                               /* HOLD: 32 steps of the cycle, blended */
                uint32_t q = (ph0 + 0x04000000u) & 0xF8000000u;
                wph = ph0 + (uint32_t)(((int32_t)(q - ph0) * fxamt) >> 7);
            }
        }
        if (mode == 3u) {                                          /* X2: two cycles, crossfaded in */
            int32_t d, c = wt_pts(lvl, f0, f1, ph0, &d);
            a = c + (((d - c) * fx) >> 8);
            c = wt_pts(lvl, f0, f1, ph0 * 2u, &d);
            b = c + (((d - c) * fx) >> 8);
            s = a + (((b - a) * fxamt) >> 7);
        } else {
            a = wt_pts(lvl, f0, f1, wph, &b);
            s = a + (((b - a) * fx) >> 8);
        }
        if (uni) {
            int32_t u, d, c = wt_pts(lvl, f0, f1, ph1, &d);
            u = c + (((d - c) * fx) >> 8);
            if (mode == 3u) {
                c = wt_pts(lvl, f0, f1, ph1 * 2u, &d);
                c += (((d - c) * fx) >> 8);
                u += ((c - u) * fxamt) >> 7;
            }
            s = (s + u) >> 1;
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
            int32_t y = s - dcx + dc_pole(dcy);   /* rounded: a floored
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
    v->s[5] = pph;
    v->s[6] = psm;
    if (folda)                                        /* (FASE 2) FOLD: a symmetric fold after the filter */
        for (i = 0; i < n; i++)
            out[i] = softclip((out[i] * (4096 + folda * 24)) >> 12);
}

static const preset_t WT_PRESETS[] = {
    /* name, {MORPH, DETUNE, UNI, SUB, CUT, RES, DRIVE, HARM}, {A D S R}, fenv, mono */
    {1225, {18, 22, 1, 10, 92, 10, 0, 76}, {25, 70, 110, 75}, 0, 0, FX(0, 48, 42, 72), PAT(5),
     {XP(P_LRATE, 16), XP(P_LWAVE, 6), XP(P_LD_FLT, 12), XP(P_LD_AMP, 6)}},
    {2599, {54, 12, 1, 0, 104, 24, 18, 80}, {3, 60, 110, 35}, 0, 1, FX(20, 22, 26, 26), PAT(4),
     {XP(P_LRATE, 40), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 18)}},
    {178, {0, 0, 0, 0, 118, 0, 0, 40}, {2, 55, 100, 40}, 0, 0, FX(0, 12, 14, 24), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 20), XP(P_HARM, 10)}},
    {2610, {127, 8, 1, 4, 88, 30, 6, 66}, {35, 80, 127, 70}, 0, 0, FX(0, 36, 32, 64), PAT(5),
     {XP(P_LRATE, 18), XP(P_LWAVE, 0), XP(P_LD_FLT, 12)}},
    {2620, {98, 0, 0, 0, 116, 12, 0, 92}, {0, 50, 40, 55}, 0, 0, FX(0, 24, 22, 40), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 20), XP(P_HARM, 24)}},
    {2631, {72, 6, 0, 0, 96, 44, 10, 30}, {0, 60, 70, 45}, 0, 0, FX(10, 14, 16, 28), PAT(2),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 24), XP(P_LEVEL, 119), XP(P_ELOOP, 2)}},
    {2643, {36, 18, 1, 0, 78, 20, 30, 88}, {45, 75, 127, 60}, 0, 0, FX(26, 32, 22, 48), PAT(6),
     {XP(P_LRATE, 20), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 16)}},
    {192, {10, 0, 0, 90, 60, 26, 8, 20}, {0, 55, 120, 35}, 0, 1, FX(20, 0, 0, 10), PAT(2),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 18), XP(P_HARM, -16)}},
    {2655, {78, 14, 1, 0, 100, 8, 0, 58}, {0, 64, 127, 40}, 0, 0, FX(0, 32, 20, 34), PAT(5),
     {XP(P_LRATE, 18), XP(P_LWAVE, 1), XP(P_LD_AMP, 6)}},
    {2667, {44, 26, 1, 30, 66, 36, 22, 84}, {55, 90, 127, 95}, 0, 0, FX(30, 52, 46, 88), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 6), XP(P_LD_FLT, 18), XP(P_LD_AMP, 8)}},
    /* (6.0) the remaster's new sounds: one per family, each with its own motion and sends */
    {2680, {40, 26, 1, 8, 86, 12, 10, 70}, {46, 88, 106, 88}, 8, 0, FX(0, 58, 30, 76), PAT(5),
     {XP(P_LRATE, 15), XP(P_LWAVE, 6), XP(P_LD_FLT, 14), XP(P_LD_AMP, 6), XP(P_LSHP, 20)}},  /* B: pad */
    {2693, {58, 14, 1, 0, 100, 26, 16, 62}, {6, 80, 84, 56}, 20, 1, FX(14, 24, 36, 40), PAT(4),
     {XP(P_LRATE, 36), XP(P_LWAVE, 1), XP(P_LD_FLT, 10), XP(P_M1SRC, 7), XP(P_M1DST, 11), XP(P_M1AMT, 8)}},  /* A: reed lead */
    {2707, {84, 10, 0, 0, 110, 30, 20, 88}, {0, 78, 0, 52}, 26, 0, FX(10, 18, 48, 32), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 24), XP(P_LEVEL, 125), XP(P_ELOOP, 2)}},  /* PLUCK */
    {2720, {28, 32, 1, 12, 74, 18, 6, 54}, {70, 92, 112, 98}, 6, 0, FX(0, 62, 34, 84), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 6), XP(P_LD_FLT, 16), XP(P_M1SRC, 7), XP(P_M1DST, 2), XP(P_M1AMT, 8)}},  /* G: pad */
    /* (3.0) the morph family: each one moves MORPH itself (the matrix's E1) or lets POSMOD walk it, so the
     * table is heard as a road and not as fourteen still frames; the LFO stays on the filter where noted */
    {2732, {64, 18, 1, 6, 88, 20, 0, 68}, {32, 82, 112, 86}, 0, 0, FX(0, 50, 40, 82), PAT(5),
     {XP(P_LRATE, 12), XP(P_LWAVE, 6), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 38), XP(P_LD_FLT, 20)}},
    {2741, {74, 8, 0, 0, 104, 18, 8, 88}, {4, 70, 96, 42}, 12, 1, FX(16, 28, 34, 44), PAT(4),
     {XP(P_LRATE, 52), XP(P_LWAVE, 1), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 46), XP(P_LD_FLT, 10)}},
    {2748, {22, 20, 1, 4, 90, 14, 0, 72}, {30, 78, 112, 84}, 0, 0, FX(0, 60, 38, 80), PAT(5),
     {XP(P_LRATE, 14), XP(P_LWAVE, 6), XP(P_X1, 26), XP(P_LD_FLT, 14), XP(P_LD_AMP, 8), XP(P_LEVEL, 108)}},
    {2760, {112, 10, 1, 2, 86, 26, 4, 60}, {42, 84, 120, 80}, 0, 0, FX(0, 46, 36, 76), PAT(5),
     {XP(P_LRATE, 10), XP(P_LWAVE, 0), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 14), XP(P_X1, 34)}},
    {2771, {48, 6, 0, 0, 108, 22, 16, 90}, {8, 66, 100, 48}, 16, 1, FX(22, 30, 30, 40), PAT(4),
     {XP(P_LRATE, 30), XP(P_LWAVE, 4), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 42), XP(P_X0, 26)}},
    {2782, {12, 0, 0, 84, 56, 24, 8, 16}, {0, 52, 120, 30}, 0, 1, FX(8, 0, 0, 12), PAT(2),
     {XP(P_LRATE, 8), XP(P_LWAVE, 0), XP(P_M1SRC, 1), XP(P_M1DST, 12), XP(P_M1AMT, 10), XP(P_X1, 8)}},
    /* (FASE 5) Two families the bank was missing, both built from what the engine already does: a tine
     * electric piano (the frame near HOLLOW, the filter env a fast attack then a body, no sub) and a
     * crystal bell (the METAL frame, a short filter env and a long tail, sent to the reverb). Neither
     * adds DSP: MORPH picks the frame, HARM the tilt, CUT/RES the body. */
    {2792, {34, 6, 0, 0, 96, 8, 4, 72}, {0, 62, 44, 40}, 22, 0, FX(0, 18, 20, 30), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 18), XP(P_LEVEL, 112), XP(P_HARM, 12)}},
    {2806, {104, 0, 0, 0, 112, 20, 0, 100}, {0, 70, 18, 62}, 10, 0, FX(0, 24, 26, 52), PAT(3),
     {XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 16), XP(P_LEVEL, 116), XP(P_HARM, 20)}},
    {2818, {44, 26, 1, 14, 86, 20, 18, 58}, {28, 78, 102, 84}, 10, 0, FX(24, 50, 42, 72), PAT(5),
     {XP(P_LRATE, 9), XP(P_LWAVE, 7), XP(P_LD_FLT, 20), XP(P_M1SRC, 4), XP(P_M1DST, 1), XP(P_M1AMT, 14)}},  /* (v1.5.19) the table morphing under the cut */
    /* (FASE 3) PHASE's three transforms, one preset each: they add nothing to a sound until WARP asks for it,
     * and the preset writes the mode in the retired slot (P_LRTRG) and WARP's depth on CHARACTER 1. */
    {2831, {58, 8, 1, 0, 100, 26, 12, 72}, {6, 74, 96, 56}, 18, 1, FX(14, 24, 30, 36), PAT(4),
     {XP(P_LRTRG, 1), XP(P_X0, 20), XP(P_X1, 8), XP(P_LRATE, 28), XP(P_LD_FLT, 12), XP(P_M1AMT, 8)}},
    {2841, {104, 0, 0, 0, 112, 20, 0, 100}, {0, 70, 30, 64}, 10, 0, FX(0, 22, 26, 54), PAT(3),
     {XP(P_LRTRG, 2), XP(P_X0, 30), XP(P_X1, 18), XP(P_M1SRC, 3), XP(P_M1DST, 2), XP(P_M1AMT, 14)}},
    {2852, {30, 12, 1, 0, 108, 16, 8, 82}, {0, 60, 120, 40}, 0, 1, FX(18, 18, 24, 26), PAT(2),
     {XP(P_LRTRG, 3), XP(P_X0, 36), XP(P_X1, 20), XP(P_LRATE, 22), XP(P_LD_FLT, 10), XP(P_M1AMT, 10)}},
};

static const param_desc_t WT_CH[3] = {
    {"WARP", F_PCTINV, 0, 127, 127, 0, 0}, {"POSMOD", F_PCTINV, 0, 127, 127, 0, 0},
    {"FOLD", F_PCT, 0, 127, 0, 0, 0},
};

static const engine_t ENG_WAVETABLE = {
    .name = "WTABLE",                                      /* six characters: the engine field fits six */
    .page_title = {"TABLE", "TONE", "CHAR"},
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
    .ch = WT_CH,
    .ch_n = 3,
    .note_on = wt_note_on,
    .render = wt_render,
    .knob = {P_E0, P_E7, P_E4, P_E2},
    .keep = 0x1f,                                        /* the filter's two states, the tilt, the DC blocker */
};
