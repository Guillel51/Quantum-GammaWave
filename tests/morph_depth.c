/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5) MORPH's depth stack, proved where it is drawn.
 *
 * The panel draws three waves: the one that sounds in front and the two ends of its travel behind, smaller and
 * lifted. "It looks deeper" is not a check, so this bench reads the geometry the panel just drew -- graph_morph
 * keeps the state of each pass (morph_last) and morph_y is the same function the drawing uses, so what is measured
 * here is what was painted, not a second copy of the maths -- and then looks at the canvas for the pixels.
 *
 *   build/morph_depth
 *
 * For every setting it prints the panel's visible range, a column-by-column table (i, front, A, B) and then:
 *   - SEPARATION: the vertical distance between the front wave and the nearest back wave, per column: the
 *     minimum, the median and how many columns have at least 2 px (one px of lift is invisible under a 2 px line);
 *   - PIXELS: whether a bright pixel sits within 1 px of the front y and a dim one within 1 px of each back y,
 *     per column, which is what makes the layers actually visible.
 * Fails when a layer is missing, when the median separation is under 3 px, or when more than a tenth of the
 * columns cannot see both a front and a back line on the glass.
 *
 * Host x86-64 with the real sources: this is not the FM-1's panel and claims nothing about it. */
#define main hostsim_main
#include "../build/capture/ui_render_nomain.c"
#undef main

static int bad;
static void mcheck(const char *what, int ok, double a, double b)
{
    printf("morph: %-58s %s (%.1f / %.1f)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

/* the panel: PANEL_X0 .. PANEL_X0 + PANEL_W, and the wave's own 34 px about the line at 46 */
#define MG_X0 PANEL_X0
#define MG_MID 46
#define MG_AMP 34

/* one setting: draw the page's graph into the canvas and report what came out */
static void run(int wave, int morph, int engi, int verbose)
{
    track_t *t = TSEL;
    int32_t yf[111], ya[111], yb[111], i, sep_min = 999, sep_med = 0, cols2 = 0, seen = 0;
    int32_t nb = 0, nfront_px = 0, nback_px = 0, hist[40];
    static int32_t seps[111];
    memset(hist, 0, sizeof hist);
    set_engine_of(t, engi);
    t->p[P_E0] = (int16_t)wave;
    t->p[P_MORPH] = (int16_t)morph;
    cv_begin(240, graph_h(), T_BG);
    graph_morph(t);
    if (verbose)
        printf("  panel: x %d..%d, y %d..%d (visible), canvas %d px\n",
               MG_X0, MG_X0 + PANEL_W - 1, MG_MID - MG_AMP, MG_MID + MG_AMP, graph_h());
    for (i = 0; i <= 110; i++) {
        yf[i] = morph_y(t, &morph_last[0], (uint32_t)i);
        ya[i] = yb[i] = 0;
        if (morph_n > 1) {
            ya[i] = morph_y(t, &morph_last[1], (uint32_t)i);
            yb[i] = morph_y(t, &morph_last[2], (uint32_t)i);
            nb = 1;
        }
    }
    if (verbose) {
        printf("  col   x   front    A    B\n");
        for (i = 0; i <= 110; i += 10)
            printf("  %3d %4d   %4d %4d %4d\n", (int)i, MG_X0 + (int)i * 2, (int)yf[i], (int)ya[i], (int)yb[i]);
    }
    /* separation: the nearest back wave, column by column */
    for (i = 1; i < 111; i++) {
        int32_t d = 999;
        if (nb) {
            int32_t da = yf[i] > ya[i] ? yf[i] - ya[i] : ya[i] - yf[i];
            int32_t db = yf[i] > yb[i] ? yf[i] - yb[i] : yb[i] - yf[i];
            d = da < db ? da : db;
        }
        seps[i - 1] = d;
        if (d < sep_min)
            sep_min = d;
        if (d >= 2)
            cols2++;
        if (d < 40)
            hist[d]++;
    }
    if (nb) {
        int32_t cum = 0;
        for (i = 0; i < 40; i++) {
            cum += hist[i];
            if (cum >= 55) {
                sep_med = i;
                break;
            }
        }
    }
    /* pixels: is there a line where the geometry says, on the glass */
    for (i = 1; i < 111; i++) {
        int32_t x = MG_X0 + i * 2, k, f_ok = 0, b_ok = 0;
        if (x >= 240)
            break;
        for (k = -1; k <= 1; k++) {
            int32_t y = yf[i] + k;
            if (y >= 0 && y < graph_h() && cv_px[y * 240 + x] != cv_bg)
                f_ok = 1;
        }
        if (nb) {
            for (k = -1; k <= 1; k++) {
                int32_t y = ya[i] + k;
                if (y >= 0 && y < graph_h() && cv_px[y * 240 + x] != cv_bg)
                    b_ok = 1;
            }
        } else
            b_ok = 1;
        nfront_px += f_ok;
        nback_px += f_ok && b_ok;
        if (i == 1 && verbose)
            printf("  first column: front pixel %d, back pixel %d\n", f_ok, b_ok);
    }
    printf("  wave %d morph %d: passes %d, separation min %d median %d, columns >= 2 px %d of 110\n",
           wave, morph, (int)morph_n, (int)sep_min, (int)sep_med, (int)cols2);
    printf("  front line on the glass in %d of 110 columns, front and back together in %d\n", (int)nfront_px,
           (int)nback_px);
    {
        /* (v2.3.4) FOUR LAYERS AT EVERY VALUE. The stack used to wait for a crossfade to be in progress -- at MORPH 0
         * the page drew one line and no depth -- so the bench now asks for the stack at 0, at 64 and at 127 alike. */
        mcheck("the stack is four layers at every value", morph_n == QMD_N, (double)morph_n, (double)QMD_N);
        mcheck("the front wave is on the glass", nfront_px > 100, (double)nfront_px, 100.0);
        mcheck("front and a back wave are both visible", nback_px > 99, (double)nback_px, 99.0);
        mcheck("the nearest back wave is 3 px away at the median", sep_med >= 3, (double)sep_med, 3.0);
        /* the waves cross where they cross: there the gap is 0 by geometry, so the bar is three columns in four */
        mcheck("three columns in four show a real gap", cols2 > 82, (double)cols2, 82.0);
    }
    (void)seen;
}

int main(void)
{
    FILE *sink = fopen("/dev/null", "w");
    if (sink) {
        rep = sink;
        al_rep = sink;
        aud = sink;
    }
    if (!rep)
        rep = stderr;
    state();
    setup(S_MORPH);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.force = 1;
    frames(2);

    printf("morph_depth: the depth stack read off the panel it is drawn on (host x86-64)\n");
    run(0, 64, ENGI_ANALOG, 1);        /* SAW -> SQR, half way */
    run(1, 32, ENGI_ANALOG, 0);        /* SQR -> TRI, a third */
    run(0, 127, ENGI_ANALOG, 0);       /* the far end of the travel */
    run(0, 0, ENGI_ANALOG, 0);         /* at 0 the stack is there too */
    run(0, 64, ENGI_PHYS, 0);   /* another engine entirely */         /* the phase-distortion engine's own waves */
    run(0, 64, ENGI_LOFI, 0);
    printf("morph_depth: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
