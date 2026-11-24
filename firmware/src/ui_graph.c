/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;
#include "ui_organic.c"
#include "rowb.h"                              /* JIANT FM's organic line art (drawn by code) */
#include "ui_fm6op.c"                                /* (0.6.10, from Felucca 1.5) EDIT > OPERATOR, OP ENV, OPERATOR 2 */
static uint16_t heat_col(int32_t v);
/* GAMMA / WRAITH / JIANT (thermal): a curve's colour at canvas row y, hot at the top (y0) and cold at the bottom (y1); else c */
static uint16_t heat_y(int32_t y, int32_t y0, int32_t y1, uint16_t c)
{
    return ux.chamfer && y1 > y0 ? heat_col(24 + (y1 - y) * 232 / (y1 - y0)) : c;
}
/* GAMMA / WRAITH / JIANT: a hit or note as hot as it plays (velocity x chance; an accent white hot); else ACCENT or c */
static uint16_t step_heat(const step_t *st, int acc, uint16_t c)
{
    int32_t v = (st->vel ? st->vel : 100) * (int32_t)step_chance(st) / 100;
    if (!ux.chamfer)
        return acc ? T_ACCENT : c;
    return heat_col(acc ? 236 : 40 + v * 160 / 127);
}
#include "ui_being.c"                                /* (JIANT 0.5) the beings: the engine pages' creatures */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
#define ENV_K(n) (32768 - 150733 / (n))            /* the exp(-4.6 / n) step: the curve's own factor, one place */
/* (v1.12) WHERE THE VOICE IS ON A DRAWN EXPONENTIAL, IN THE CURVE'S OWN TERMS. The playhead used to place itself by
 * inverting a straight line -- (1000 - lvl) * n / (from - to) -- while the curve it sits on decays by ENV_K every
 * sample: with the default decay the dot ran up to thirteen pixels ahead of the ink, and on a release the same.
 * This walks the same recurrence the drawing loop walks, sample by sample, and interpolates between the two samples
 * the level falls between, so the dot is on the curve by construction. `from` > `to`, both 0..1000, n >= 1. */
static int32_t env_x_of(int32_t lvl, int32_t n, int32_t from, int32_t to)
{
    int32_t i, e = 32768, k = ENV_K(n), prev = from, px = 0, ok = 0;
    if (lvl >= from)
        return 0;                                  /* still at the section's first sample */
    if (lvl <= to)
        return n;                                  /* past its last */
    for (i = 1; i <= n; i++) {
        int32_t lv;
        e = (e * k) >> 15;
        lv = to + ((from - to) * e >> 15);
        if (lv <= lvl) {                           /* between sample i-1 (prev) and i (lv) */
            int32_t d = prev - lv;
            px = (i - 1) * 100 + (d > 0 ? (prev - lvl) * 100 / d : 0);   /* samples and hundredths, one unit */
            ok = 1;
            break;
        }
        prev = lv;
    }
    if (!ok)
        px = n * 100;
    return px < 0 ? 0 : (px > n * 100 ? n * 100 : px);   /* hundredths of a sample: the caller divides */
}

/* (v2.0.1) THE FX PAGES KEEP ui_tech.c's GRAPHS. For a while these six pages were redirected to a set of static
 * drawings added here, which hid the technical graphs (tech_dly, tech_rev, tech_chorus, tech_master) that already
 * answered the same knobs with animation. They are back in charge: the pages are GR_NONE again, ui_tech.c draws
 * them from the page id, and tests/ui_test's "the rings follow the bus's return" is the gate that caught it. */
/* (v2.2) QUANTUM DEPTH. The ENV screen asked for the one thing a flat curve cannot say: that these shapes stand in
 * space, one behind another. A real 3D engine is out of the question on this part (no matrix unit worth the flash,
 * and every curve would cost a transform a point), so this is the old trick, done once and reused: a layer is the
 * SAME curve drawn smaller, lifted and shifted a step for every layer behind it, mixed towards the palette's cool end,
 * and the front one is drawn 2 px with a lit floor under it while the ones behind are 1 px and dimmer. It is pure 2D
 * geometry -- scale, shear, lift, colour -- so the cost of a layer is the cost of a polyline.
 *
 * Two rules keep it honest: the levels come from a function the page passes in (never from a timer), and the buffer is
 * one static array of samples, filled per layer and never allocated. The page asks for the curve once and gets the
 * whole stack; nothing here knows what an envelope, an LFO or a chorus is. */
#define QZ_N 81                                     /* samples a layer: 2.7 px apart across the 220 px panel --
                                                     * twice the 41 the stack had, so the arm of an attack is an
                                                     * arm and not a staircase; not more, because at 111 the
                                                     * compiler unrolls these loops and the image grows 1 776 B */
#define QZ_LAYERS 6                                 /* (v2.3.4) the most layers a page has asked for */
static int16_t qz_v[QZ_N];                          /* one layer's levels, 0..1000 (82 B, shared by every page) */
typedef struct {
    int32_t x0, y0, w, h;                           /* the FRONT layer's box: x0,y0 its left foot, w x h */
    int32_t n;                                      /* layers, 1..QZ_LAYERS */
    int32_t dx, dy;                                 /* what a layer recedes: this many px right, this many up */
    int32_t sx, sy;                                 /* and this many per-mille smaller (60 = 6 %) */
    uint16_t front, back;                           /* the colour ramp: the front is the page's own ink */
    uint16_t glow;                                  /* the front layer's floor, 0 for none */
} qz_t;
/* the level of a curve, 0..1000, at u = 0..1000 across the layer: k is which layer (0 = front) */
typedef int32_t (*qz_lvl_t)(void *ctx, int32_t k, int32_t u);
/* (v2.3.3) THE STACK GOES DARK, NOT COOL. A layer behind is the same curve one step further away, and distance on a
 * 16-bit panel reads as a darker line, not as a bluer one: each repetition mixes further towards the page's own
 * ground -- T_BG, so MONO stays black and PAPER fades towards its paper instead of towards white-on-white -- with
 * the step set so the second layer is already clearly behind and the fifth is barely there. */
static uint16_t qz_ink(const qz_t *q, int32_t k)
{
    int32_t pct;
    (void)q->back;
    if (k <= 0 || q->n < 2)
        return q->front;
    pct = 58 - (k - 1) * 44 / (q->n - 1);           /* 58 % down to 14 % over however deep the stack is */
    return ux_mix(T_BG, q->front, pct < 14 ? 14 : pct);
}
/* (v2.3.3) A LAYER FURTHER BACK IS DRAWN DARKER, NOT MERELY THINNER. MORPH and TILT keep the pass state of the two
 * layers they stand behind the front one, and each of them spans a fraction of the panel (`sy`): that fraction is the
 * depth it sits at, so the ink fades in the same order the layers recede -- the near one still reads, the far one is
 * a shade above the page's ground. */
static uint16_t qz_back_ink(uint16_t ink, int32_t sy)
{
    int32_t pct = 46 + (sy - 700) * 40 / 300;       /* sy 760 -> 54 %, 880 -> 70 %, 1000 -> 86 % */
    return ux_mix(T_BG, ink, pct < 18 ? 18 : (pct > 94 ? 94 : pct));
}
static __attribute__((noinline)) void qz_set(qz_t *q, int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t n, uint16_t front, uint16_t back)
{
    q->x0 = x0;
    q->y0 = y0;
    q->w = w < 8 ? 8 : w;
    q->h = h < 8 ? 8 : h;
    q->n = n < 1 ? 1 : (n > QZ_LAYERS ? QZ_LAYERS : n);
    /* (v2.3.3) MORE DEPTH, AND INSIDE THE PANEL. Each layer steps a sixteenth of the panel right and a fourteenth of
     * its height up, and shrinks 9.5 %: the shrink lowers the layer's top by more than the lift raises it, so the
     * four back curves stay inside the box instead of walking out through its top edge (the first cut lifted a
     * quarter of the height a layer and the back two were cut off at the frame). */
    q->dx = q->w / 16;                              /* the shear: each layer steps right ... */
    q->dy = q->h / 14;                              /* ... and up, so the stack has a vanishing point */
    q->sx = 95;                                     /* and shrinks 9.5 % a layer: the depth is real geometry */
    q->sy = 95;
    q->front = front;
    q->back = back;
    q->glow = ux_mix(T_BG, front, 24);
}
static __attribute__((noinline)) void qz_curve(const qz_t *q, qz_lvl_t fn, void *ctx)
{
    int32_t k, i;
    for (k = q->n - 1; k >= 0; k--) {                /* back to front, so the near layer is never covered */
        int32_t sw = q->w * 1000 / (1000 + k * q->sx);
        int32_t sh = q->h * 1000 / (1000 + k * q->sy);
        int32_t ox = q->x0 + k * q->dx, oy = q->y0 + (q->h - sh) - k * q->dy;
        uint16_t ink = qz_ink(q, k);
        int32_t px = 0, py = 0;
        for (i = 0; i < QZ_N; i++) {
            int32_t v = fn(ctx, k, i * 1000 / (QZ_N - 1));
            qz_v[i] = (int16_t)clamp(v, 0, 1000);
        }
        for (i = 0; i < QZ_N; i++) {                /* the under-glow: what the front curve stands on */
            int32_t x = ox + i * sw / (QZ_N - 1);
            int32_t y = oy + sh - qz_v[i] * sh / 1000;
            if (k == 0 && q->glow)                   /* the front curve's floor: two rows of light, not a fill */
                cv_rect(x, y + 1, sw / (QZ_N - 1) + 1, 2, q->glow);
            if (i) {
                cv_line_t(px, py, x, y, ink, k ? 1 : 2);
            }
            px = x;
            py = y;
        }
    }
}
/* exp(-4.6 u) in nine steps, Q15: the envelope's own decay (the DSP falls to 1 % over a stage, ENV_K is built from
 * the same constant), sampled instead of walked a pixel at a time. */
static const uint16_t ENV_E[9] = {32768, 18448, 10386, 5846, 3291, 1853, 1043, 587, 330};
static int32_t env_fall(int32_t t1000)              /* 0..1000 across a stage -> Q15 of exp(-4.6 t) */
{
    int32_t f = clamp(t1000, 0, 1000) * 8 / 1000, r = clamp(t1000, 0, 1000) * 8 % 1000;
    if (f >= 8)
        return ENV_E[8];
    return ENV_E[f] + (ENV_E[f + 1] - ENV_E[f]) * r / 1000;
}

/* one ADSR per layer, in the page's own pixels and % (built in graph_adsr, read by env_lvl) */
typedef struct { int32_t a[QZ_LAYERS], d[QZ_LAYERS], r[QZ_LAYERS], t[QZ_LAYERS], s[QZ_LAYERS]; } env_ctx_t;
static __attribute__((noinline)) int32_t env_lvl(void *ctx, int32_t k, int32_t u)
{
    const env_ctx_t *e = (const env_ctx_t *)ctx;
    int32_t a = e->a[k], d = e->d[k], t = e->t[k], r = e->r[k], sus = e->s[k];
    int32_t tot = a + d + t + r, x = u * tot / 1000;
    if (tot <= 0)
        return 0;
    if (x < a)
        return a > 0 ? 1000 * x / a : 1000;
    x -= a;
    if (x < d)
        return sus + (1000 - sus) * env_fall(d > 0 ? x * 1000 / d : 1000) / 32768;
    x -= d;
    if (x < t)
        return sus;
    x -= t;
    return r > 0 ? sus * env_fall(clamp(x * 1000 / r, 0, 1000)) / 32768 : 0;
}
static __attribute__((noinline)) void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
    const voice_t *bv;                                                  /* the loudest voice, found once */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    /* (v2.2) THE ENVELOPE STANDS IN SPACE. One curve says the shape; a stack of them says where it came from. The
     * four values this page edits are kept in a five-deep ring, and the ring is what the depth engine draws: the
     * current shape in front, full size and 2 px, and the four shapes the user had before it receding behind it --
     * smaller, lifted, sheared and mixed towards the palette's cool end. Nothing is invented: every layer is a real
     * ADSR the page really held, and the front one is the one the DSP is playing. The ring only shifts when a knob
     * changes, so a still page draws a still stack. */
    {
        /* (v2.2) THE INSTRUMENT'S ENVELOPES, STANDING IN SPACE. The page's own envelope is drawn in front -- full
         * size, 2 px, with the lit floor under it -- and behind it stand the other tracks' amplitude envelopes, one
         * layer each, smaller, lifted and mixed towards the palette's cool end. It is the same four values read from
         * six tracks: nothing is a timer and nothing is invented, and a knob on any track changes its own layer. */
        /* (v2.3.3) THE SAME ENVELOPE, FIVE TIMES, GETTING FURTHER AND DARKER. The stack used to be four real ADSRs --
         * this track's in front and three OTHERS behind -- so turning a knob moved only the front layer and the rest
         * sat frozen, which read as a broken picture rather than as depth. Every layer is this page's own four
         * values now: the knob moves the whole stack together, and what separates the layers is distance (shear,
         * lift, size and the darker ink qz_ink gives each one), not data. The front layer is still exactly the ADSR
         * the DSP is playing. */
        static env_ctx_t ec;                        /* 100 B of .bss, filled every draw: no allocation */
        int32_t k;
        qz_t q;
        for (k = 0; k < QZ_LAYERS; k++) {
            ec.a[k] = a;
            ec.d[k] = d;
            ec.r[k] = r;
            ec.s[k] = sus;
            ec.t[k] = x3 - x1 - d > 0 ? x3 - x1 - d : 1;    /* the flat part, the same width in every layer */
        }
        qz_set(&q, x0, top, x3 + r - x0, bot - top, QZ_LAYERS, heat_y((bot + top) / 2, top, bot, c), T_BG);
        qz_curve(&q, env_lvl, &ec);
    }
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    (void)px;
    (void)py;
    /* (v2.0.1) THE LOUDEST VOICE IS FOUND ONCE: the playhead and the stage's accent both need it, and the page used
     * to walk the twelve voices twice for the same answer. */
    {
        const voice_t *b0 = 0;
        for (i = 0; i < NVOICE; i++)
            if (V(t, i).stage && V(t, i).stage < 4u && (!b0 || V(t, i).env > b0->env))
                b0 = &V(t, i);
        bv = b0;
    }
    if (pg->id[0] == P_ATK) {                           /* (JIANT 0.5) the track's envelope now: the loudest voice's */
        int32_t lvl, x;                                 /* level as a dot on the curve, a line down to the axis */
        if (bv) {
            lvl = (int32_t)(((int64_t)bv->env * 1000) >> 24);
            if (bv->stage == 1u)                       /* the attack is drawn straight: level over its length */
                x = x0 + a * lvl / 1000;
            else if (bv->stage == 2u)                  /* the decay and the release are the curve's own, not a line */
                x = x1 + (sus < 1000 ? env_x_of(lvl, d, 1000, sus) / 100 : 0);
            else
                x = x3 + (sus > 0 ? env_x_of(lvl, r, sus, 0) / 100 : r);
            cv_rect(x, EGY(lvl), 1, bot + 2 - EGY(lvl), T_MID);
            cv_rrect(x - 3, EGY(lvl) - 3, 7, 7, 3, T_TEXT, T_SURF);
        }
    }
    /* (v2.1) THE FOUR CORNERS. The stage's start and end points are on the curve as small lit pips, so the shape has
     * joints and not only a silhouette: the attack's peak, the decay's arrival, the release's start and its foot. */
    {
        int32_t cx4[4], cy4[4], q;
        cx4[0] = x1; cy4[0] = top;
        cx4[1] = x1 + d; cy4[1] = EGY(sus);
        cx4[2] = x3; cy4[2] = EGY(sus);
        cx4[3] = x3 + r; cy4[3] = bot;
        for (q = 0; q < 4; q++)
            cv_rrect(cx4[q] - 2, cy4[q] - 2, 5, 5, 2, q == 1 || q == 2 ? T_ACCENT : T_TEXT, T_SURF);
    }
    /* (v1.12.8) THE STAGES ARE ON THE CURVE. The page drew the shape and nothing else: the four segments of the
     * envelope, which is the one thing the cards cannot say at a glance, had no mark at all. A quiet guide stands
     * where each segment ends and begins, its letter sits under its own segment (accent on the stage the loudest
     * voice is in now), and the level now is a % beside the live stage's name on the top row. Everything is read
     * from the same a, d, r and sus the curve was built from, so a label cannot name a segment that is not there. */
    {
        static const char *const SN[4] = {"ATK", "DEC", "SUS", "REL"};
        int32_t g, seg_live = -1, mid[4];
        /* every label is three glyphs of AF_S (21 px): measured once, as a number, instead of six text_w calls */
        mid[0] = (x0 + x1) / 2 - 10;                         /* centred on its own segment, and never past it */
        mid[1] = (x1 + x3) / 2 - 10;
        mid[2] = x3 - 25;                                    /* SUS ends on the release's own boundary */
        mid[3] = x3 + 4;                                     /* REL starts after it */
        if (mid[0] < 12)
            mid[0] = 12;
        if (mid[1] + 21 > mid[2] - 4)                        /* no room between DEC and SUS: the guide stays */
            mid[1] = -1000;
        if (mid[2] < x1 + 4)
            mid[2] = -1000;
        if (mid[3] + 21 > 228) {
            mid[3] = 207;
            if (mid[3] < x3 + 2)
                mid[3] = -1000;
        }
        if (bv && pg->id[0] == P_ATK)
            seg_live = (int32_t)bv->stage - 1;
        cv_rect(x1, top, 1, bot - top, T_LINE);          /* a quiet guide, not a box: the curve stays the subject */
        cv_rect(x3, top, 1, bot - top, T_LINE);
        if (graph_ht - (bot + 3) >= 8)                 /* the strip has no room under the curve: guides only */
            for (g = 0; g < 4; g++)
                if (mid[g] > -1000)
                    cv_text_on(mid[g], bot + 3, &AF_S, SN[g], g == seg_live ? T_ACCENT : T_DIM, T_SURF);
    }
#undef EGY
}

/* LFO: the wave as it runs (JIANT 0.5): the phase now at the playhead (a quarter in), two cycles across, scrolling
 * at the LFO's own rate; behind the playhead dimmer (what it played), the level now a dot, its depth after FADE */
/* (v1.12.7) TILT: THE KNOB AS A PICTURE. The page had one knob and an empty card: a tilt is a shelf, and a shelf is
 * the one response that can be drawn honestly without an FFT -- flat at the bottom, flat at the top, a smooth bend
 * at the pivot, and the top's own height is 20 log10(k/64) of what the DSP does. Left of the pivot the line IS the
 * 0 dB axis (DC gain is exactly 1 by construction, not by approximation), right of it the plateau the knob sets, and
 * the bend is drawn as the straight chord between them: an asymptotic sketch, labelled as such by its shape, not a
 * measured response. The dot is where the top sits; the knob moves it. Cheap: one rectangle a column, a few
 * multiplies, nothing per sample. */
/* (FASE 5) one pass of TILT's response for a given knob position. `amt` -64..63 is the same value the DSP uses, so
 * the three passes are three real settings and not three copies of one: the ends of the knob's travel behind, the
 * one it is on in front. lift/scale/ink are the depth. */
static void tilt_pass(uint16_t c, int32_t amt, int32_t sy, int32_t dy, int32_t th, int back)
{
    int32_t x, cy = graph_ht / 2 + dy, a = 38 * graph_ht / 100 * sy / 1000;
    int32_t k = 64 + (clamp(amt, -64, 63) * 3) / 4;   /* 16..111: the same gain the DSP uses */
    int32_t xp = PANEL_X0 + PANEL_W / 2, band = PANEL_W / 6;
    int32_t right = cy - ((k - 64) * a) / 64;         /* the plateau, in the graph's own px */
    for (x = 0; x < PANEL_W; x++) {
        int32_t d = x - (xp - PANEL_X0), y;
        if (d <= -band)
            y = cy;                                   /* the body: untouched, exactly */
        else if (d >= band)
            y = right;                                /* the top: the knob's own plateau */
        else
            y = cy + ((right - cy) * (d + band)) / (2 * band);
        cv_rect(PANEL_X0 + x, y, 1, th, back ? qz_back_ink(heat_y(y, cy - a, cy + a, c), sy)
                                              : heat_y(y, cy - a, cy + a, c));
    }
    if (!back)
        cv_rrect(xp - band + 2, right - 3, 7, 7, 3, T_TEXT, T_SURF);   /* the top's level, where the knob puts it */
}
/* (v2.3.4) THE FILTER PAGE DRAWS ITS RESPONSE, IN DEPTH. It had no graph at all -- four knobs and an empty panel --
 * so the one page that is entirely about a shape showed nothing. The curve is the part's own filter, read from the
 * page's own ids (ANALOG keeps CUT/RES in E4/E5, every other engine in FCUT/FRES), drawn four times: the curve and
 * three copies flatter, higher and darker behind it, the same depth MORPH and TILT have. */
static int32_t flt_mag(int32_t type, int32_t u, int32_t uc, int32_t q, int32_t dr)
{
    int32_t d = u - uc, dabs = d < 0 ? -d : d, roll = d > 0 ? d : 0, g, bump;
    int32_t lp = 1000 * 1000 / (1000 + roll * roll / 220);
    switch (type) {
    case 1:                                          /* HP: the mirror of LP */
        g = 1000 - lp;
        break;
    case 2:                                          /* BP: one peak at the cutoff, nothing either side */
        g = lp * (1200 - lp) / 320;
        break;
    case 3:                                          /* COMB: the notch train, one every 160 units */
        g = 520 + 480 * (((u + uc) / 160) % 2 ? 1 : -1);
        break;
    case 4: {                                        /* VOWEL: three formants, fixed, as a voice has them */
        static const int16_t VF[3] = {250, 520, 780};
        int32_t k, sum = 0;
        for (k = 0; k < 3; k++) {
            int32_t dd = u - VF[k];
            dd = dd < 0 ? -dd : dd;
            if (dd < 95)
                sum += (95 - dd) * 380 / 95;
        }
        g = 170 + sum;
        break;
    }
    case 5:                                          /* SCREAM: a spike at the cutoff on a slope, no tail */
        g = lp * 3 / 4 + (dabs < 150 ? 880 - dabs * 5 : 0);
        break;
    case 6:                                          /* NOTCH: one hole where the cutoff is */
        g = 1000 - (dabs < 210 ? 820 - dabs * 3 : 0);
        break;
    case 7:                                          /* SHELF: flat, and a step down past the corner */
        g = 400 + 600 * lp / 1000;
        break;
    default:                                         /* LP */
        g = lp;
        break;
    }
    bump = 150 - dabs;                               /* RESONANCE: at the cutoff and nowhere else */
    if (bump > 0)
        g += (q * bump / 150) * 5 / 10;
    g += dr * 140 / 127;                             /* DRIVE: the hotter the part, the higher the curve */
    return clamp(g, 0, 1180);
}
static void filter_pass(int32_t type, int32_t uc, int32_t q, int32_t dr, int32_t sy, int32_t dy, int32_t th, int back)
{
    int32_t x, cy = graph_ht * 42 / 100 + dy, top = graph_ht * 12 / 100, bot = graph_ht * 82 / 100;
    int32_t px = 0, py = 0;
    for (x = 0; x < PANEL_W; x++) {
        int32_t u = x * 1000 / (PANEL_W - 1);
        int32_t g = flt_mag(type, u, uc, q, dr) * sy / 1000;
        int32_t y = bot - g * (bot - top) / 1180;
        y = clamp(y, top, bot);
        if (x) {
            uint16_t ink = heat_y(y, top, bot, T_THEME);
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, back ? qz_back_ink(ink, sy) : ink, th);
        }
        px = PANEL_X0 + x;
        py = y;
    }
    (void)cy;
    (void)px;
}
static __attribute__((noinline)) void graph_filter(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t type = (int32_t)t->p[pg->id[0]], cut = (int32_t)t->p[pg->id[1]], res = (int32_t)t->p[pg->id[2]], k;
    int32_t uc = 40 + clamp(cut, 0, 127) * 920 / 127;   /* 4 % .. 96 % of the sweep */
    int32_t q = clamp(res, 0, 127), dr = clamp((int32_t)t->p[pg->id[3]], 0, 127);
    cv_rect(PANEL_X0, graph_ht * 42 / 100, PANEL_W, 1, T_RAISE);
    for (k = 3; k >= 1; k--)
        filter_pass(type, uc, q, dr, 1000 - k * 80, -k * 4, 1, k);
    filter_pass(type, uc, q, dr, 1000, 0, 2, 0);
    (void)c;
}
static __attribute__((noinline)) void graph_tilt(const track_t *t, uint16_t c)
{
    int32_t cy = graph_ht / 2, a = 38 * graph_ht / 100, amt = t->p[P_HARM];
    int32_t xp = PANEL_X0 + PANEL_W / 2;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);       /* 0 dB */
    cv_rect(xp, cy - a - 4, 1, 2 * a + 8, T_MID);     /* the pivot: ~880 Hz, where the bend sits */
    /* (FASE 5) the two ends of TILT's travel behind, the setting it is on in front: the page says how far the
     * knob has gone, not only where it is. At the ends themselves the same curve is already in front, so the
     * pass equal to the current one is skipped and nothing is drawn twice. */
    /* (v2.3.4) THE SAME DEPTH MORPH HAS: the curve TILT is set to, four times, each one flatter, higher and darker
     * than the last. It used to put the two ENDS of the knob's travel behind (and skipped the near one at the ends),
     * which left a single line at either extreme and layers that did not move with the knob. */
    {
        int32_t k;
        for (k = 3; k >= 1; k--)
            tilt_pass(c, amt, 1000 - k * 80, -k * 5, 1, k);
        tilt_pass(c, amt, 1000, 0, 1, 0);
    }
}

/* (v2.2) THE LFO IN DEPTH. The page drew one wave; a modulation is a wave AND where it has been, which is what the
 * depth engine is for: three layers of the same wave, each a step of phase further back, receding up and to the
 * right in the palette's cool end, with the wave of now in front at 2 px. The playhead and the level dot stay on the
 * front one, so the animation is unchanged -- what the engine adds is the third dimension it was always drawing flat.
 * WAVE, RATE, DEPTH, FADE, PHASE and SYNC are the same values the DSP runs: a layer is a phase step, not a timer. */
typedef struct { const track_t *t; uint32_t ph, span; } lfo_ctx_t;
static int32_t lfo_lvl(void *ctx, int32_t k, int32_t u)
{
    const lfo_ctx_t *c = (const lfo_ctx_t *)ctx;
    uint32_t px = c->ph + (uint32_t)u * c->span + (uint32_t)k * (c->span / 3u);
    int32_t w;
    if (c->t->p[P_LWAVE] == 4)
        w = ((int32_t)((((px >> 29) + 1u) * 2654435761u) >> 16) - 32768);
    else
        w = lfo_wave((track_t *)c->t, px);
    return 500 + w * 500 / 32768;
}
static __attribute__((noinline)) void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, g, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, xp = PANEL_W / 4;
    uint32_t span = 0xFFFFFFFFu / (PANEL_W / 2u), ph = t->lfo_ph - (uint32_t)xp * span;
    int32_t now = cy - mulq15(t->lfo_val, t->lfo_fade) * a / 32768;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (g = 3; g >= 0; g--) {                       /* (v2.3.4) four layers: the wave and three behind it */
        /* (v2.3.3) the echo goes DARK, not cool, and stands further back: a layer one step behind is the same wave
         * earlier in its cycle, lifted a sixteenth of the panel and mixed further towards the page's ground, so the
         * three read as one wave receding instead of three strands of one colour.
         * (v2.3.4) S&H IS NOT PHASE-SHIFTED BEHIND. Stepping a sample-and-hold wave back by a fraction of its cycle
         * gives a DIFFERENT random value, so the four layers read as four unrelated staircases crossing each other --
         * the illegible screen of the report. The echoed layers keep the front's phase there: the same steps, lifted
         * and darker, which is what depth looks like on a staircase. */
        int32_t sh = t->p[P_LWAVE] == 4;
        /* a staircase lifted a sixth of the panel per layer becomes four staircases in the same picture; the S&H echo
         * sits a couple of pixels above the front instead and lets the darker ink do the depth */
        int32_t lift = g * (sh ? graph_ht / 40 + 1 : graph_ht / 16 + 2);
        uint16_t gi = g ? ux_mix(T_BG, heat_y(cy, cy - a, cy + a, c), 56 - (g - 1) * 14)
                        : heat_y(cy, cy - a, cy + a, c);
        py = cy - lift;
        for (x = 0; x < PANEL_W; x++) {              /* (lfo_wave only reads the track) */
            uint32_t px = ph + (uint32_t)x * span + (uint32_t)(sh ? 0 : g) * (span * (uint32_t)xp / 2u);
            int32_t y = cy - lift - lfo_wave((track_t *)t, px) * a / 32768;
            if (sh)                                  /* S&H: a value per cycle part, held */
                y = cy - lift - ((int32_t)((((px >> 29) + 1u) * 2654435761u) >> 16) - 32768) * a / 32768;
            if (x)
                cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y,
                          g ? gi : (x < xp ? ux_mix(T_SURF, gi, 45) : gi), g ? 1 : 2);
            py = y;
        }
    }
    cv_rect(PANEL_X0 + xp, cy - a - 4, 1, 2 * a + 8, T_MID);   /* the playhead */
    cv_rrect(PANEL_X0 + xp - 3, now - 3, 7, 7, 3, T_TEXT, T_SURF);
}

/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* PATTERN page: the 64 steps as 4 rows of 16 bars (an accent: the accent colour, a tie: a lower bar,
 * empty: a stub), the cursor and the playhead under them */
/* (ETAPA 6) CHANCE: the whole pattern's probability at a glance, not just the cursor's step.
 * The page used to be the piano roll with a die drawn on the knob, so 16 steps' chances -- the one
 * thing the page is about -- were invisible: the card was mostly empty and a step's probability could
 * only be read one at a time. This draws the bank of 16 as columns measured from the bottom, the
 * ratchet's parts as pips above the baseline, a 25/50/75/100 % guide, the cursor framed and the
 * step the transport is on marked under it. Colour says the chance: T_THEME at 100 %, warmer as it
 * falls (heat_col 0..200), so the risky steps read at a glance without a legend. */
static __attribute__((noinline)) void graph_chance(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ((uint32_t)ui.cursor / 16u) * 16u, play = seq_src(t, t->seq_idx);
    int32_t gh = graph_ht < 40 ? 40 : graph_ht;    /* (a short card -- MENU > LARGE's strip -- never makes a
                                                    * negative height: the bars just live in less room) */
    int32_t yb = gh - 10, hmax = gh - 18, x0 = 28, pitch = 12, w = 10, g;
    cv_rect(x0 - 4, yb, 196, 1, T_MID);                        /* the floor every bar stands on */
    for (g = 0; g < 4; g++)                                     /* the beats: five separators, 4 steps apart */
        cv_rect(x0 - 3 + g * 4 * pitch, yb - hmax, 1, hmax + 3, g ? T_RAISE : T_MID);
    for (g = 1; g <= 4; g++) {                                  /* the guides: 25, 50, 75 and 100 % */
        int32_t y = yb - hmax * g / 4;
        cv_rect(x0 - 4, y, 196, 1, T_RAISE);
        cv_rect(x0 - 10, y, 5, 1, g == 4 ? T_MID : T_DIM);      /* the tick, wider at the ceiling */
    }
    for (i = 0; i < 16u; i++) {
        int32_t x = x0 + (int32_t)i * pitch, ch, h;
        uint32_t st_i = base + i;
        const step_t *st = st_i < NSTEP ? &seq_steps(t)[st_i] : 0;
        if (!st || st_i >= len) {                               /* past the pattern's length: a stub */
            cv_rect(x + 3, yb - 2, 4, 2, T_RAISE);
            continue;
        }
        ch = (int32_t)step_chance(st);
        h = ch * hmax / 100;
        cv_rect(x, yb - hmax, w, hmax, T_SURF);                 /* the well the bar fills */
        if (h) {
            cv_rrect(x, yb - h, w, h, h > 6 ? 2 : 0, ch >= 100 ? T_THEME : heat_col((uint32_t)ch * 2u), T_SURF);
            if (h > 3)
                cv_rect(x + 1, yb - h + 1, w - 2, 1, ux_mix(ch >= 100 ? T_THEME : heat_col((uint32_t)ch * 2u), T_TEXT, 40));
        }
        if (st->time == ST_TIE)                                 /* a tie still offers no die roll */
            cv_rect(x, yb - 1, w, 1, T_MID);
        if (st->time == ST_NOTE && (st->n || st->hit)) {        /* RATCH x1..x4 as pips under the bar */
            uint32_t r = step_ratchet(st), k;
            for (k = 0; k < r && k < 4u; k++)
                cv_rect(x + 1 + (int32_t)k * 2, yb + 1, 1, 3, T_MID);
        }
        if (st_i == (uint32_t)ui.cursor)                        /* the cursor: a 1 px frame, as SCL's keys */
            cv_frame(x - 1, yb - hmax - 1, w + 2, hmax + 2, T_TEXT);
        else if (song.playing && st_i == play)                  /* the step the transport is on */
            cv_rect(x, yb + 5, w, 2, T_TEXT);
    }
}

static __attribute__((noinline)) void graph_steps(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    int32_t sm = graph_ht < 100;                     /* MENU > LARGE's strip: rows 14 px apart, bars 10 px */
    int32_t bh = sm ? 10 : 14, pitch = sm ? 14 : 24, y0 = sm ? 2 : 4;
    for (i = 0; i < NSTEP; i++) {
        int32_t x = bar_x(i), y = y0 + (int32_t)(i / 16u) * pitch;
        const step_t *st = &seq_steps(t)[i];
        if (i >= len)
            continue;
        if (i % 16u == 0u && i + 16u <= len)
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "step bars' row centred");
        if (step_on(st))
            cv_rrect(x, y, 9, bh, 2, step_heat(st, st->flags & SF_ACCENT, c), T_SURF);
        else if (st->time == ST_TIE)
            cv_rrect(x, y + bh - 6, 9, 6, 1, T_MID, T_SURF);
        else
            cv_rrect(x, y + bh - 3, 9, 3, 1, T_RAISE, T_SURF);
        if (i == ui.cursor)
            cv_rect(x, y + bh + 2 - sm, 9, 2, T_ACCENT);
        else if (song.playing && i == seq_src(t, t->seq_idx))
            cv_rect(x, y + bh + 2 - sm, 9, 2, T_TEXT);
    }
}

/* (0.6.39) SEQ > COMP: the cursor's 16 steps as on PATTERN, each with its mark under it (an ARP component: heat
 * orange, TRIG: violet, a CHANCE under 100 %: MID, a RATCH: THEME); below, the cursor step's TYPE drawn: an ARP's
 * notes as a ladder in its ORDER (the one playing ACCENT), TRIG's loops as boxes (the ones it plays filled, the loop
 * now framed), CHANCE as a bar, RATCH as its parts */
static uint16_t comp_mark(const step_t *st)
{
    uint32_t c = st->comp ? st->comp : st->comp2;      /* (0.6.82: the second, when it is the only one) */
    if (cp_isarp(c)) return heat_col(200);
    if (c == CP_TRIG) return heat_col(90);
    if (c > CP_TRIG) return heat_col(20 + (c - CP_RAND) * 22u);   /* (0.6.42) the others */
    if (step_chance(st) < 100u) return T_MID;
    if (step_ratchet(st) > 1u) return T_THEME;
    return 0;
}
static __attribute__((noinline)) void graph_comp(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], v = comp_view(), ct;
    const step_t *cs = &t->step[ui.cursor];
    int32_t y0 = 4, bh = 14, x, w;
    char b[32];
    uint32_t cl = (uint32_t)clamp(t->p[P_CLEN], 0, NSTEP), cnow = cl && song.playing ? (t->cpos + cl - 1u) % cl : 0xFFu;
    for (i = ui.bank * 16u; i < ui.bank * 16u + 16u && i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        uint16_t mk;
        x = bar_x(i);
        if (cl && i < cl && i == cnow) cv_rect(x, y0 + bh + 12, 9, 2, T_MID);   /* (0.6.47) C.LEN: the lane playing */
        if (i >= len) {                                 /* (past LEN: only C.LEN's components) */
            if (i < cl && (mk = comp_mark(st)) != 0) cv_rect(x, y0 + bh + 3, 9, 3, mk);
            continue;
        }
        if (step_on(st)) cv_rrect(x, y0, 9, bh, 2, step_heat(st, st->flags & SF_ACCENT, c), T_SURF);
        else if (st->time == ST_TIE) cv_rrect(x, y0 + bh - 6, 9, 6, 1, T_MID, T_SURF);
        else cv_rrect(x, y0 + bh - 3, 9, 3, 1, T_RAISE, T_SURF);
        if ((mk = comp_mark(st)) != 0 && (!cl || i < cl)) cv_rect(x, y0 + bh + 3, 9, 3, mk);
        if (st->comp == CP_JUMP && CP_JTO(st->carg) / 16u == ui.bank)   /* (0.6.42) JUMP: a line to the step it names */
            cv_line(x + 4, y0 + bh + 4, bar_x(CP_JTO(st->carg)) + 4, y0 + bh + 4, mk);
        if (st->comp && st->comp2)                      /* (0.6.82) two components: a second mark */
            cv_rect(x, y0 + bh + 6, 9, 1, comp_mark(&(step_t){.comp = st->comp2}));
        if (i == ui.cursor) cv_rect(x, y0 + bh + 8, 9, 2, T_ACCENT);
        else if (song.playing && i == seq_src(t, t->seq_idx)) cv_rect(x, y0 + bh + 8, 9, 2, T_TEXT);
    }
    if (comp_slot() == 2u) {                            /* (0.6.82) WHEN: the two, the loop now */
        static const char *const WN[4] = {"ALWAYS", "ON 1:2", "ON 2:2", "SKIPPED"};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            uint32_t c = k ? cs->comp2 : cs->comp;
            str_cpy(b, k ? "B " : "A ", sizeof b);
            str_cpy(b + 2, c ? COMP_NAMES[COMP_ARPN + c - CP_ARPN] : "-", 12);
            if (c) { str_cpy(b + str_len(b), "  ", 3); str_cpy(b + str_len(b), WN[CW_OF(cs, k)], 9); }
            cv_text(PANEL_X0 + 2, 40 + (int32_t)k * 16, &AF_S, b, c ? T_MID : T_DIM);
        }
        cv_text(PANEL_X0 + 2, 80, &AF_S, "A SILENCING ONE (TRIG, FILL) WINS", T_DIM);
        if (song.playing) {
            str_cpy(b, "LOOP ", sizeof b);
            fmt_int(b + 5, (int32_t)t->loops + 1);
            cv_text_r(228, 40, &AF_S, b, t->loops & 1u ? T_THEME : T_ACCENT, T_SURF);
        }
        return;
    }
    ct = comp_type(cs);
    str_cpy(b, v >= COMP_ARPN && v <= COMP_ARPS ? "ARP " : "", sizeof b);
    str_cpy(b + str_len(b), v ? COMP_NAMES[v] : "NO COMPONENT  K2: TYPE", sizeof b - 8u);
    cv_text(PANEL_X0 + 2, 36, &AF_S, b, v ? T_MID : T_DIM);
    if (cl) {                                           /* (0.6.47) the lane's own length (SHIFT > C.LEN) */
        char lb[12] = {'C', '.', 'L', 'E', 'N', ' ', 0};
        fmt_int(lb + 6, (int32_t)cl);
        cv_text_r(228, 36, &AF_S, lb, T_ACCENT, T_SURF);
    }
    if (v >= COMP_ARPN && v <= COMP_ARPS) {             /* the ladder: one cycle (up to 16 notes), as it plays */
        step_t sh = *cs;
        uint32_t n, lo = 127u, hi = 0u, now = 0xFFu;
        sh.carg = (uint8_t)comp_arg(cs);
        uint8_t nt[16];
        if (drum_track(t)) {                            /* (0.6.82) a kit: its sounds in turn, or tuned up the scale */
            cv_text(PANEL_X0 + 2, 60, &AF_S, ct == CP_ARPS ? "THE HITS UP THE SCALE" : "THE SOUNDS IN TURN", T_DIM);
            return;
        }
        if (!cs->n) { cv_text(PANEL_X0 + 2, 60, &AF_S, "A STEP WITH NOTES PLAYS IT", T_DIM); return; }
        if (CP_ORD(sh.carg) == 3u) sh.carg = (uint8_t)(sh.carg & ~3u);   /* (RND: drawn UP, the order is the dice's) */
        n = CP_ORD(sh.carg) == 2u ? 2u * 16u : 16u;
        for (i = 0; i < 16u && i < n; i++) {
            nt[i] = (uint8_t)cp_arp_note(t, &sh, ct, sh.carg, i);
            if (nt[i] < lo) lo = nt[i];
            if (nt[i] > hi) hi = nt[i];
        }
        n = i;
        if (song.playing && t->arp_from == ui.cursor + 1u && t->arp_k)
            now = (uint32_t)(t->arp_k - 1u) % n;
        w = PANEL_W / (int32_t)n;
        for (i = 0; i < n; i++) {
            int32_t hgt = 8 + (hi > lo ? (int32_t)(nt[i] - lo) * 40 / (int32_t)(hi - lo) : 20);
            cv_rrect(PANEL_X0 + (int32_t)i * w + 1, 98 - hgt, w - 2, hgt, 2,
                     i == now ? T_ACCENT : heat_col(60 + (uint32_t)(nt[i] - lo) * 150u / (hi > lo ? hi - lo : 1u)), T_SURF);
        }
    } else if (v == COMP_TRIG) {                        /* the loops: a of b filled, the loop now framed */
        const uint8_t *cd = CP_TRIGS[comp_arg(cs) % CP_NTRIG];
        uint32_t nb = cd[0] ? cd[1] : 4u, cur = song.playing ? (cd[0] ? t->loops % nb : (t->loops < 3u ? t->loops : 3u)) : 0xFFu;
        w = 24;
        for (i = 0; i < nb; i++) {
            int on = cd[0] ? i == cd[0] - 1u : (cd[1] == 1u ? i == 0u : i > 0u);
            x = PANEL_X0 + (PANEL_W - (int32_t)nb * (w + 4)) / 2 + (int32_t)i * (w + 4);
            if (i == cur) cv_rrect(x - 2, 58, w + 4, w + 4, 3, T_ACCENT, T_SURF);
            cv_rrect(x, 60, w, w, 3, on ? heat_col(90) : T_RAISE, i == cur ? T_ACCENT : T_SURF);
        }
    } else if (v > COMP_TRIG) {                         /* (0.6.42) what it does, in a line, and its picture */
        static const char *const SAY[COMP_N - COMP_RAND] = {
            "THE NOTE AT RANDOM IN THE SCALE", "AN OCTAVE UP OR DOWN", "THE REST OF THE LOOP MOVED",
            "THE NOTE GLIDES OVER THE STEP", "REPEATS, EACH SOFTER", "AGAIN ON THE EMPTY STEPS AFTER",
            "THEN TO ANOTHER STEP", "THEN BACK TO STEP 1", "ONLY WHILE [FX] IS HELD", "CUTS WHAT THE TRACK SOUNDS"};
        int32_t a = (int32_t)comp_arg(cs), cx = PANEL_X0 + PANEL_W / 2;
        cv_text(PANEL_X0 + 2, 50, &AF_S, SAY[v - COMP_RAND], T_DIM);
        if (v == COMP_RAND || v == COMP_TONAL || v == COMP_OCT) {   /* the span it moves in, the note in the middle */
            int32_t r = v == COMP_RAND ? a * 4 : v == COMP_TONAL ? (a - 7) * 4 : (a % 6u == 1u || a % 6u == 3u ? -24 : 24);
            int32_t lo = v == COMP_RAND ? -r : r < 0 ? r : 0, hi = v == COMP_RAND ? r : r > 0 ? r : 0;
            cv_rrect(cx - 10, 82 - hi, 20, hi - lo + 4, 3, T_RAISE, T_SURF);
            cv_rrect(cx - 10, 82, 20, 4, 1, heat_col(200), T_SURF);
            if (v != COMP_RAND) cv_rrect(cx - 10, 82 - r, 20, 4, 1, T_ACCENT, T_SURF);
        } else if (v == COMP_BEND) {                    /* the glide */
            int32_t d = (a - 12) * 2;
            cv_line_t(PANEL_X0 + 20, 82, PANEL_X0 + PANEL_W - 20, 82 - d, heat_col(200), 3);
        } else if (v == COMP_ECHO) {                    /* the repeats fading */
            uint32_t h = 2u + (uint32_t)a % 7u, dv = CT_DIVS[CT_DIV(cs) % CT_NDIV];
            if (dv && h > dv) h = dv;                    /* (0.6.82) DIV: as many as fit, that far apart */
            w = (PANEL_W - 8) / (int32_t)(dv ? dv : h);
            for (i = 0; i < h; i++) {
                int32_t hh = 34 * (int32_t)(h - i) / (int32_t)h;
                cv_rrect(PANEL_X0 + 4 + (int32_t)i * w, 98 - hh, w - 6, hh, 3, heat_col(200 - i * 160u / h), T_SURF);
            }
        }
    } else if (v == COMP_CHANCE) {
        cv_rrect(PANEL_X0 + 2, 64, PANEL_W - 4, 12, 3, T_RAISE, T_SURF);
        cv_rrect(PANEL_X0 + 2, 64, (PANEL_W - 4) * (int32_t)step_chance(cs) / 100 + 1, 12, 3, heat_col(160), T_SURF);
    } else if (v == COMP_RATCH) {
        uint32_t r = step_ratchet(cs);
        w = (PANEL_W - 8) / (int32_t)r;
        for (i = 0; i < r; i++) cv_rrect(PANEL_X0 + 4 + (int32_t)i * w, 60, w - 6, 30, 3, heat_col(120 + i * 30u), T_SURF);
    }
}

/* (0.6.43) SEQ > MUTATE: a row a track, the next 32 bars: SOUND's changes an orange mark on top, PATTERN's a violet one
 * under it; the bar now ACCENT; the selected track's row lit; ORBIT / DRIFT at the right; FROZEN over it all */
static __attribute__((noinline)) void graph_mutate(void)
{
    uint32_t k, b, now = song.playing ? bar_n : 0u;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t s = MUT_BARS[(uint32_t)clamp(t->p[P_MSND], 0, 6)], q = MUT_BARS[(uint32_t)clamp(t->p[P_MPAT], 0, 6)];
        int32_t y = 4 + (int32_t)k * 16, on = (s || q) && k < trk_live;
        char nm[3] = {'T', (char)('1' + k), 0};
        cv_text(PANEL_X0, y + 1, &AF_S, nm, k == song.sel ? T_THEME : on ? T_MID : T_DIM);
        for (b = 0; b < 32u; b++) {
            uint32_t bar = now + 1u + b;
            int32_t x = PANEL_X0 + 22 + (int32_t)b * 5;
            cv_rect(x, y + 6, 4, 1, T_RAISE);
            if (on && s && bar % s == 0u) cv_rect(x, y + 1, 4, 4, mut_freeze ? T_DIM : heat_col(190));
            if (on && q && bar % q == 0u) cv_rect(x, y + 8, 4, 4, mut_freeze ? T_DIM : heat_col(90));
        }
        if (on) cv_text_r(PANEL_X0 + PANEL_W, y + 1, &AF_S, t->p[P_MMODE] ? "DRIFT" : "ORBIT", T_DIM, T_SURF);
    }
    cv_rect(PANEL_X0 + 22, 2, 4, 96, ux_mix(T_SURF, T_ACCENT, 25));   /* (the next bar's column) */
    if (mut_freeze) cv_text(PANEL_X0 + 70, 98, &AF_S, "FROZEN  [OCT-]", T_ACCENT);
}

/* (0.6.45) EDIT > MORPH: a cycle of the wave as it morphs (from WAVE towards the next ones), heat coloured by its
 * height; under it the two waves it is between */
/* (0.6.48) EDIT > IMAGE (a DRUM track): the eight sounds from left to right as IMAGE places them (as wide as DRV,
 * DIST and the FILTER let it play: dx_img_k), a row each, the kick on top, on a line from the middle */
static __attribute__((noinline)) void graph_dximg(const track_t *t)
{
    static const char *const AB[DV_NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    int32_t img = clamp(t->p[P_DXIMG], 0, 127), k = img ? dx_img_k(t) : 0, cx = PANEL_X0 + PANEL_W / 2;
    uint32_t l;
    cv_rect(cx, 0, 1, 96, T_RAISE);
    for (l = 0; l < DV_NLANE; l++) {
        int32_t p = img ? dx_img_pan(l, img) * k >> 8 : 0, y = (int32_t)l * 12;
        int32_t x = cx + p * (PANEL_W / 2 - 14) / 64;
        uint16_t c = k < 256 && img ? T_MID : heat_col(60u + l * 22u);   /* (narrowed by DRV DIST FILTER: grey) */
        cv_rect(p < 0 ? x : cx, y + 6, p < 0 ? cx - x : x - cx, 1, c);
        cv_rrect(x - 13, y, 26, 12, 2, c, T_SURF);
        cv_text_in(x - 13, y + CAP_IN(S, 12), 26, &AF_S, AB[l], T_INK, c);
    }
}

/* (FASE 5) what MORPH's panel is about to draw, worked out once: pos/f the wave, a0/a1 its two ends, and the
 * state each engine's wave needs. graph_morph fills it and draws from it; tests/morph_depth.c fills the same one
 * and reads the geometry straight out of the same function, so the bench measures what the panel paints and not a
 * second copy of the maths. */
typedef struct {
    int32_t pos, f, sy, dy;
    uint32_t a0, a1, e;
    int dirt, pmw;
    pd_t pa, pb;
    an_dirt_t dt;
} morph_st;
#define QMD_N 4                   /* (v2.3.4) the layers MORPH draws: the curve and three copies behind it */
static morph_st morph_last[QMD_N];  /* the passes of the last draw: front first (see graph_morph) */
static int32_t morph_n;           /* how many were drawn */
/* (FASE 5) the y of one column (0..110) of the wave this state describes: the height scaled by sy (per-mille of
 * the panel's 34 px) about the line at 46, lifted by dy. Invisible: no colour, no canvas. */
static int32_t morph_y(const track_t *t, const morph_st *st, uint32_t i)
{
    const uint32_t e = st->e;
    uint32_t ph = (i % 110u) * (0xFFFFFFFFu / 110u), inc = 0xFFFFFFFFu / 220u;
    int32_t s;
    if (ENGINES[e] == &ENG_PHASE) {
        s = pd_wave(&st->pa, ph >> 16);
        if (st->f) s += ((pd_wave(&st->pb, ph >> 16) - s) * st->f) >> 8;
    } else if (ENGINES[e] == &ENG_LOFI) {
        s = lofi_mto(st->a0, ph, 0x80000000u, WRAM[((uint32_t)t->p[P_E2] >> 3) & 15u], 0, 1u);
        if (st->f) s += ((lofi_mto(st->a1, ph, 0x80000000u, WRAM[((uint32_t)t->p[P_E2] >> 3) & 15u], 0, 1u) - s) * st->f) >> 8;
    } else if (st->pmw) {
        int32_t mo = osc_saw(ph * 2u, inc);
        s = osc_sine(ph + (uint32_t)mo * (uint32_t)(16384u + (uint32_t)t->p[P_MORPH] * 128u));
    } else if (st->dirt)
        s = an_dirt((uint32_t)t->p[P_E0], ph, &st->dt);
    else
        s = an_morph(st->pos, ph, inc);
    return 46 + st->dy - s * 34 * st->sy / (32768 * 1000);
}
/* (FASE 5) one pass drawn: the same y morph_y gives, joined up. `back`: a wave of the travel, drawn dim. */
static void morph_pass(const track_t *t, const morph_st *st, int32_t th, int back)
{
    int32_t px = 0, py = 0;
    uint32_t i;
    for (i = 0; i <= 110u; i++) {
        uint32_t ph = (uint32_t)(i % 110u) * (0xFFFFFFFFu / 110u);
        int32_t x = PANEL_X0 + (int32_t)i * 2, y = morph_y(t, st, i);
        (void)ph;
        if (i)
            cv_line_t(px, py, x, y, back ? qz_back_ink(heat_col(80 + (uint32_t)(46 - y + 34) * 2u), st->sy)
                                         : heat_col(80 + (uint32_t)(46 - y + 34) * 2u), th);
        px = x;
        py = y;
    }
}

static __attribute__((noinline)) void graph_morph(const track_t *t)
{
    const int16_t *p = t->p;
    uint32_t e = t->eng_req % NENGINES, i, a0 = 0, a1 = 0;
    int32_t px = 0, py = 0, pos, f;
    const char *n0 = "", *n1 = "";
    char b[24];
    pd_t pa, pb;
    an_dirt_t dt = {0, 0, 0, 0, 0};
    int dirt = 0, pmw = 0;
    if (ENGINES[e] == &ENG_PHASE) {
        uint32_t dcw = (uint32_t)(p[P_E2] << 8) * 65535u / (127u << 8);
        pos = (int32_t)(p[P_E0] & 7) * 256 + p[P_MORPH] * 768 / 127;
        dcw = (dcw * 56000u) >> 16;
        a0 = ((uint32_t)pos >> 8) & 7u; a1 = (a0 + 1u) & 7u;
        pd_setup(&pa, a0, dcw); pd_setup(&pb, a1, dcw);
        n0 = N_PD_WAVE[a0]; n1 = N_PD_WAVE[a1];
    } else if (ENGINES[e] == &ENG_LOFI) {
        pos = clamp(p[P_E1] * 256 + p[P_MORPH] * 768 / 127, 0, RW_WRAM * 256);
        a0 = (uint32_t)pos >> 8; a1 = a0 + 1u < RW_BYTE ? a0 + 1u : a0;
        n0 = N_RWAVE[a0]; n1 = N_RWAVE[a1];
    } else if (p[P_E0] == AW_PM) {                    /* (FASE 2) PM: a sine whose phase osc 2 bends, MORPH the index */
        pmw = 1;
        pos = 0;
        n0 = n1 = N_ANALOG_WAVE[AW_PM];
    } else if (p[P_E0] >= AW_TUBE) {                  /* (1.0-GW) TUBE FOLD: the sine as MORPH makes it dirty */
        an_dirt_set(&dt, p[P_MORPH] << 8);
        dirt = 1;
        pos = 0;
        n0 = n1 = N_ANALOG_WAVE[(uint32_t)p[P_E0] % NELEM(N_ANALOG_WAVE)];
    } else {
        if (p[P_E0] >= 4) {                             /* (0.6.81) PWM SYNC RING SAW3: the WAVE, what MORPH moves */
            static const char *const WHAT[4] = {"PWM  WIDTH", "SYNC  RATIO", "RING  RATIO", "SAW3  SPREAD"};
            str_cpy(b, WHAT[(p[P_E0] - 4) & 3], sizeof b);
            cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
            for (i = 0; i <= 110u; i++) {
                int32_t x = PANEL_X0 + (int32_t)i * 2, y = 46 - (int32_t)((i * 34u * (uint32_t)p[P_MORPH]) / (110u * 127u)) + 17;
                if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
                px = x; py = y;
            }
            cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
            return;
        }
        pos = clamp(p[P_E0] * 256 + p[P_MORPH] * 768 / 127, 0, 1536);
        a0 = ((uint32_t)pos >> 8) & 3u; a1 = (a0 + 1u) & 3u;
        n0 = N_ANALOG_WAVE[a0]; n1 = N_ANALOG_WAVE[a1];
    }
    f = pos & 255;
    cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
    {
        morph_st st;
        st.pos = pos; st.f = f; st.a0 = a0; st.a1 = a1; st.e = e;
        st.dirt = dirt; st.pmw = pmw; st.dt = dt;
        st.pa = pa; st.pb = pb;
        /* the state of each pass, kept for tests/morph_depth.c: it reads the geometry the panel just drew from,
         * instead of working the maths out a second time and drifting from it */
        /* (v2.3.4) THE DEPTH IS THERE AT EVERY VALUE, AND IT FOLLOWS THE KNOB. The stack used to be the two ENDS of
         * the crossfade, drawn only while the knob sat between them: at MORPH 0 there was one line and no depth at
         * all, and turning the knob moved the front while the two ends stayed where they were. The four layers are
         * the same curve at four depths now -- flatter and higher the further back, each in a darker ink -- so the
         * stack exists at 0, at 127 and everywhere between, and all of it moves when the knob turns. */
        st.pos = pos; st.f = f; st.sy = 1000; st.dy = 0;
        {
            int32_t k;
            morph_n = QMD_N;
            for (k = 0; k < QMD_N; k++) {
                morph_last[k] = st;
                morph_last[k].sy = 1000 - k * 80;    /* 6.4 % flatter a layer */
                morph_last[k].dy = -k * 5;           /* and 5 px higher */
            }
            for (k = QMD_N - 1; k >= 1; k--)
                morph_pass(t, &morph_last[k], 1, k);
            morph_pass(t, &morph_last[0], 2, 0);
        }
    }
    if (0)
    for (i = 0; i <= 110u; i++) {
        uint32_t ph = (uint32_t)(i % 110u) * (0xFFFFFFFFu / 110u), inc = 0xFFFFFFFFu / 220u;
        int32_t s, x = PANEL_X0 + (int32_t)i * 2, y;
        if (ENGINES[e] == &ENG_PHASE) {
            s = pd_wave(&pa, ph >> 16);
            if (f) s += ((pd_wave(&pb, ph >> 16) - s) * f) >> 8;
        } else if (ENGINES[e] == &ENG_LOFI) {
            s = lofi_mto(a0, ph, 0x80000000u, WRAM[((uint32_t)p[P_E2] >> 3) & 15u], 0, 1u);
            if (f) s += ((lofi_mto(a1, ph, 0x80000000u, WRAM[((uint32_t)p[P_E2] >> 3) & 15u], 0, 1u) - s) * f) >> 8;
        } else if (pmw) {
            int32_t mo = osc_saw(ph * 2u, inc);       /* osc 2, twice osc 1: a harmonically rich modulator */
            s = osc_sine(ph + (uint32_t)mo * (uint32_t)(16384u + (uint32_t)p[P_MORPH] * 128u));
        } else if (dirt)
            s = an_dirt((uint32_t)p[P_E0], ph, &dt);
        else
            s = an_morph(pos, ph, inc);
        y = 46 - s * 34 / 32768;
        if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
        px = x; py = y;
    }
    str_cpy(b, n0, sizeof b);
    if (f && a1 != a0) { str_cpy(b + str_len(b), " > ", 4); str_cpy(b + str_len(b), n1, 8); }
    cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
    /* (3.4) WHERE the morph is, not only what it sounds like: the bottom of the panel is the travel from A to
     * B and the marker is the position the knob is on, so the panel answers the question the page is about --
     * how far between the two waves this sound sits -- without another number to read. */
    cv_rect(PANEL_X0 + 1, 82, PANEL_W - 2, 5, T_RAISE);
    if (f)
        cv_rect(PANEL_X0 + 1, 82, (PANEL_W - 2) * f / 255, 5, ux_mix(T_RAISE, T_ACCENT, 120));
    cv_rect(PANEL_X0 + (PANEL_W - 3) * f / 255, 79, 2, 11, T_ACCENT);
}

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_on[k] && kb_trk[k] == song.sel) {
            uint32_t i;
            for (i = 0; i < kb_on[k]; i++) {            /* (0.6.81: a chord's every note) */
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_snd[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
        }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || ANIM_SNAP();
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c, hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c, hits);
        else if (acc)
            pr_split(x, y, w, PR_RH, c == T_ACCENT ? c : T_TEXT, hits);
        else
            pr_split(x, y + 1, w, PR_RH - 2, c, hits);
    }
}
static __attribute__((noinline)) void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (si == ui.cursor)
            cv_frame(x, PR_Y0 - 2, PR_CW + 1, ybot - PR_Y0 + 4, T_TEXT);
        if (song.playing && si == seq_src(t, t->seq_idx))
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        col = s == ui.cursor || si == ui.cursor ? T_ACCENT : step_heat(st, 0, c);   /* (JIANT: as hot as it plays) */
        if (s == ui.cursor && ui.pr_focus && ui.pr_focus <= st->n && cur_page()->id[0] == 4u) {   /* (0.6.81) SEQ >
                                                         * NOTE: the chord dimmed, the note in focus lit */
            step_t one = *st;
            int32_t yy = -10000;
            pr_bars(st, s == si ? x + 2 : x - 1, s == si ? 9 : 12, T_MID, 0, &y, s == si ? step_ratchet(st) : 1u);
            one.n = 1; one.note[0] = st->note[ui.pr_focus - 1u]; one.hit = 0;
            pr_bars(&one, s == si ? x + 2 : x - 1, s == si ? 9 : 12, T_ACCENT, 1, &yy, s == si ? step_ratchet(st) : 1u);
        } else if (s == si) {                         /* (0.6.88: its TIMING off the grid, shown off it) */
            int32_t mo = ST_MICRO(st) * PR_CW / 16;
            pr_bars(st, x + 2 + mo < PR_X0 ? PR_X0 : x + 2 + mo, 9, col, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        }
        else                                          /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
    if (ui.entry_open && !ui.ent_arp && ui.ent_nn && ui.ent_len > 1u) {   /* (0.6.81) a chord held, KNOB 1 its length:
                                                         * the length to be written, outlined from the cursor on */
        uint32_t k, j;
        for (k = 0; k < ui.ent_len && k < len; k++) {
            uint32_t si = (ui.cursor + k) % len;
            int32_t x;
            if (si < base || si >= base + ncol) continue;
            x = PR_X0 + (int32_t)(si - base) * PR_CW;
            for (j = 0; j < ui.ent_nn; j++) {
                int32_t y = pr_row_y(ui.ent_notes[j]);
                if (y < PR_Y0 || y >= ybot) continue;
                cv_frame(k ? x - 1 : x + 2, y, k ? 13 : 10, PR_RH, T_ACCENT);
            }
        }
    }
    if (song.g[G_STRN]) {                             /* (0.6.45) ALL TR (SCL 2): the notes sound moved, shown */
        char b[16];
        str_cpy(b, "ALL TR ", sizeof b);
        b[7] = song.g[G_STRN] < 0 ? '-' : '+'; b[8] = 0;
        fmt_int(b + 8, song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
        cv_text_r(PANEL_X0 + PANEL_W, 0, &AF_S, b, T_ACCENT, T_SURF);
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing */
static __attribute__((noinline)) void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t y0 = 6;
    for (i = 0; i < 16u && base + i < len; i++)     /* a parameter lock: a mark over the step */
        if ((locks >> (base + i)) & 1u)
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, 14, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + 2, 0, y + 12, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y - 1, &AF_S, drum_lane_abbr(t, l), sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + 2, 10, 10, step_heat(st, step_accents(st) & b, c), step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rrect(x, y + 2, 10, 10, 2, step_heat(st, step_accents(st) & b, c), row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank)
        cv_rect(40 + (int32_t)(seq_src(t, t->seq_idx) % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* (QG-1 1.7) SCL: the scale on a real keyboard, in the 16-bit chrome HOME already speaks.
 * Every key is a bevel -- a flat face, one lighter edge, one darker edge -- where the page
 * used to paint flat rectangles, and the card uses its whole height: the footer line carries
 * the two values this page owns (TRANS, CHORD), which is what used to leave the lower half of
 * the card empty (measured: 60 px of nothing under the keys).
 *   - seven white keys, 30 px wide and 1 px apart, from x 12 to 228, y 26 to 100;
 *   - five black keys 18 px wide and 46 tall, centred on the seam after their white key, so
 *     every accidental sits where a piano puts it;
 *   - in the scale the face is the track's colour and the root the accent; out of the scale the
 *     key stays recessed in the card. (3.2) the naturals name their note and nothing else: the
 *     degree digit that sat beside it read as a key number, not as a scale degree;
 *   - the title (ROOT + SCALE, QUANTIZED) sits above the keys, never on one.
 * Every figure read here is the parameter the engine plays. */
static __attribute__((noinline)) void graph_scale(const track_t *t, uint16_t c)
{
    /* (v2.0.1) THE SCL TABLES AND THE LIVE MASK ARE THE ONES THE SCL PAGES USE: this page had its own copies of the
     * three note tables and its own scan of the voices, which is the same information ui_scl.c already keeps. */
    uint32_t i, mask = scale_mask(t), root = (uint32_t)t->p[P_ROOT] % 12u, live = scl_live_mask(t);
    /* (3.1) the piano, measured: 7 white keys 29 px wide with a 2 px gap (12..228), 60 px tall,
     * five black keys 18 x 38 centred on their seam. Both are rounded bevels with a 1 px contour,
     * a light grey face for the naturals and a darker one for the accidentals, so the layout reads
     * as a keyboard on every theme instead of a wall of one colour. The scale is shown by a 2 px
     * top strip (ROOT: accent, other degrees: theme), not by filling whole keys orange and cyan:
     * that combination was the one the owner called unattractive, and it fought every palette. */
    const int32_t x0 = 12, kw = 29, kg = 2, y0 = 25, kh = 60, bw = 18, bh = 38;
    char sb[24];

    if (ux.chamfer && !ux.style)
        gw_oct_panel(8, 2, 224, 118, 5, T_BG, T_LINE, 1, c);
    else
        cv_rrect(8, 2, 224, 118, 4, T_SURF, T_BG);

    /* the title line, above the keys */
    str_cpy(sb, N_NOTE[root], sizeof sb);
    str_cpy(sb + str_len(sb), " ", 2);
    str_cpy(sb + str_len(sb), N_SCALE[clamp(t->p[P_SCALE], 0, 17)], sizeof sb - str_len(sb));
    cv_text_on(12, 7, &AF_S, sb, T_THEME, T_SURF);
    if (t->p[P_QUANT])
        cv_text_r(228, 7, &AF_S, "QUANTIZED", T_ACCENT, T_SURF);

    /* the seven naturals */
    for (i = 0; i < 7u; i++) {
        uint32_t note = SCL_WNOTE[i], deg = (note + 12u - root) % 12u;
        int32_t kx = x0 + (int32_t)i * (kw + kg);
        int in = (mask >> deg) & 1u, is_root = in && deg == 0u;
        int sound = (live >> note) & 1u;
        uint16_t face = in ? T_KEYW : ux_mix(T_KEYW, T_BG, 24);
        uint16_t brd = ux_mix(face, T_BG, 58);
        uint16_t strip = is_root ? T_ACCENT : in ? T_THEME : 0;
        if (sound)
            face = ux_mix(face, T_THEME, 34);
        scl_pkey(kx, y0, kw, kh, face, brd);
        if (strip)
            cv_rect(kx + 2, y0 + 1, kw - 4, 2, strip);
        cv_rect(kx + kw - 1, y0 + 3, 1, kh - 6, ux_mix(face, T_BG, 32));
        /* (3.2) the naturals carry their NOTE NAME only: the degree digit ("1 C", "3 D") sat
         * beside it and read as a key number the player could press. The scale is already
         * said twice -- the strip on top and the face -- and the accidentals never had it. */
        cv_text_r(kx + kw - 4, y0 + kh - 15, &AF_S, N_NOTE[note], in ? T_INK : T_DIM, face);
        if (in) {                                    /* (2.8) the degree, at the top of the key: beside the
                                                      * name it read as a key number, stacked it does not */
            uint32_t rk = 0, kk;
            char db[4];
            for (kk = 0; kk < deg; kk++)
                rk += (mask >> kk) & 1u;
            fmt_int(db, (int32_t)rk + 1);
            /* centred in the key's exposed middle: an accidental overhangs 8 px of it on either side,
             * so the free band is the 12 px in the middle -- which is where a two-digit degree fits */
            cv_text_in(kx, y0 + 6, kw, &AF_S, db, is_root ? T_ACCENT : T_MID, face);
        }
        if (sound)                                    /* the note is sounding: one clear bar */
            cv_rect(kx + 2, y0 + kh - 4, kw - 4, 2, T_ACCENT);
    }
    /* the five accidentals, over the seam they belong to */
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - root) % 12u;
        int32_t kx;
        int in, is_root, sound;
        uint16_t face, ink, strip;
        if (!SCL_BLACK[i])
            continue;
        kx = x0 + (int32_t)SCL_WKEY[i] * (kw + kg) + kw + kg / 2 - bw / 2;
        in = (mask >> deg) & 1u;
        is_root = in && deg == 0u;
        sound = (live >> i) & 1u;
        face = sound ? ux_mix(T_KEYB, T_THEME, 46) : T_KEYB;
        ink = in ? T_TEXT : T_MID;
        strip = is_root ? T_ACCENT : in ? T_THEME : 0;
        scl_pkey(kx, y0, bw, bh, face, ux_mix(T_KEYB, T_TEXT, 24));
        if (strip)
            cv_rect(kx + 2, y0 + bh - 3, bw - 4, 2, strip);
        cv_rect(kx, y0 + bh, bw, 2, T_BG);            /* its shadow on the naturals */
        GFX_HOOK_ALIGN(kx, y0, kx + bw, y0 + bh, AL_HV, "SCL accidental name in its key");
        cv_text_in(kx, y0 + CAP_IN(S, bh), bw, &AF_S, N_NOTE[i], ink, face);
        if (sound)
            cv_rect(kx + 2, y0 + bh - 3, bw - 4, 2, T_TEXT);
    }

    /* the footer: what this page's two other values are */
    cv_rect(12, 89, 216, 1, T_LINE);
    {
        char fb[24], tb[8];
        fmt_int(tb, t->p[P_TRANS]);
        str_cpy(fb, "TRANS ", sizeof fb);
        str_cpy(fb + str_len(fb), t->p[P_TRANS] > 0 ? "+" : "", 2);
        str_cpy(fb + str_len(fb), tb, sizeof fb - str_len(fb));
        cv_text_on(12, 91, &AF_S, fb, T_MID, T_SURF);
        cv_text_r(228, 91, &AF_S, t->p[P_CHRD] ? "CHORD ON" : "CHORD OFF",
                  t->p[P_CHRD] ? T_ACCENT : T_DIM, T_SURF);
    }
}
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
/* (QG-1) FX: four send channels and the three master modules that act on them, each in its own box.
 * (v1.12) What changed and why:
 *   - the channel names are whole ("CHORUS", "DELAY", "REVERB", not the CHOR/REV the 53 px card could
 *     not hold next to a 12 px icon): the name is its own line now, the icon sits over the value, and
 *     the slim meter keeps the right-hand column, so nothing is abbreviated and nothing is clipped;
 *   - the value is AF_S, one size down from the AF_M it was: four 2-digit numbers plus their meters fit
 *     without touching the card's edge on any palette or style;
 *   - the single "MASTER: EQ:BYP SLICER:OFF WIDTH:100%" ribbon at the bottom is gone. It packed three
 *     unrelated readouts into one 16 px strip (the "empty square" under the meters) and WIDTH is the
 *     STEREO M/S page's own control. In its place, three independent modules, each with its box, its
 *     icon and its real value: EQ (song.g[G_EQMODE], G_EQGAIN), DEPTH (song.g[G_SLDEP], the slicer's
 *     gate) and SLICER (song.g[G_SLMODE], lit while sl.idx is running). No animation, no invented
 *     number: every figure is read from the same global the DSP reads. */
/* (1.3.1) THE MODULE BARS' BALLISTICS: the meters' own law (immediate attack, a quarter of the way down per
 * frame) without the peak hold, which a value does not have. It lives here because these three bars are the
 * only values in the firmware drawn as a fill that has to follow a knob in real time. */
static void fx_ease(int32_t *st, int32_t target)
{
    if (*st < 0)
        *st = target;                                  /* first frame after a page change: no sweep in */
    else if (target >= *st)
        *st = target;
    else
        *st -= (*st - target + 3) >> 2;
}

/* (FASE 4) THE FADER BESIDE THE METER -- what the card is SET TO, next to what the bus is DOING. The FX page
 * shows four sends and its meters, and the meters only ever answered the second question: a bus that is silent
 * is a flat bar whatever the fader says, so "why is CHORUS not audible" had no answer on screen (the fader was
 * only a number, 2 px of which the % sign of "100 %" already used). This is a 2 px slot, the meter's own height,
 * 4 px to its left: dark when nothing is asked for, and filled from the foot with a ramp that goes from the
 * theme's dark to the accent as it climbs, so the head of the bar is where the level is. Two pixels wide on
 * purpose: it reads as a fader's slot, it does not compete with the 12 px plasma meter 4 px away, and six solid
 * bands are the whole cost (a ramp per pixel would be 43 fills a card a frame). */
static void fx_fader_bar(int32_t x, int32_t y, int32_t h, int32_t pct)
{
    int32_t rows = clamp(pct, 0, 100) * h / 100, j, step;
    cv_rect(x, y, 2, h, T_BG);                     /* the slot */
    cv_rect(x, y, 2, 1, T_LINE);                   /* and its lip: the bar is a place, even empty */
    if (rows <= 0)
        return;
    step = rows > 6 ? (rows + 5) / 6 : 1;
    for (j = 0; j < rows; j += step) {
        int32_t n = j + step > rows ? rows - j : step;
        cv_rect(x, y + h - j - n, 2, n,
                ux_mix(T_THEME, T_ACCENT, 15 + j * 85 / rows));   /* dark at the foot, the accent at the head */
    }
}

static __attribute__((noinline)) void graph_fx(const track_t *t, uint16_t c)
{
    static const char *const FX_LABELS[4] = {"DIST", "CHORUS", "DELAY", "REVERB"};
    static const char *const DIST_TYPES[6] = {"SOFT", "TAPE", "HRTH", "FLUX", "FOLD", "CRSH"};   /* (FASE 5) CRSH: appended with the model */
    static const uint16_t FX_ICON[4] = {ICON_DIST, ICON_CHORUS, ICON_DELAY, ICON_REVERB};
    /* (4.0) one ballistics for every bar in the firmware: fx_meter_step (core.h), the helper the FX meter test
     * pins -- an immediate attack, a quarter of the way down per frame, a 30-frame hold. The FX page had grown
     * its own one-pixel release here while that helper sat unused outside its test; now a bus's bar and HOME's
     * mixer's read the same real peaks (fx_vu / trk[].vu) and step them the same way. */
    static uint8_t vu[4], hold[4], hold_t[4];
    static int32_t meter_trk = -1;
    int32_t pk = t->vu < 0 ? -t->vu : t->vu;   /* the track's post-FX envelope (fx.c mix_part) */
    /* DIST is an insert on the selected track: when it is off (P_DIST == 0) track_dist() leaves the
     * signal untouched and its meter must stay dark, instead of following the dry track the way it
     * used to. The other three buses already read fx.c's real measured peaks, which fall to zero
     * when their send (or their mix) is zero. */
    if (t->p[P_DIST] == 0) pk = 0;
    int32_t bus[4] = {pk, fx_vu[3], fx_vu[4], fx_vu[5]};   /* DIST, chorus, delay, reverb returns */
    int32_t send[4] = {pk, fx_vu[0], fx_vu[1], fx_vu[2]};  /* what each bus actually receives */
    uint32_t i;
    if (meter_trk != (int32_t)(t - trk)) {           /* the meters belong to one track */
        memset(vu, 0, sizeof vu);
        memset(hold, 0, sizeof hold);
        memset(hold_t, 0, sizeof hold_t);
        meter_trk = (int32_t)(t - trk);
    }

    /* (1.3) THE PANEL USES ITS WHOLE HEIGHT, and every meter sits inside its card. The four cards were 62 px tall
     * inside a plate that stopped at row 68, and the three modules filled a 28 px strip under them: rows 99..121
     * were empty while the page's meters were the narrowest on the instrument. The cards are 80 tall now (4..84),
     * the plate follows them, the modules are 30 tall and live in the rows the page was wasting (88..118), and
     * mx is cx + 39: the old track started at cx + 43 and was 12 px wide on a 53 px card, so all four bars hung
     * two pixels past the card's edge -- the "corridos" the owner saw. 39 + 12 + 3 = 54, the card's own width. */
    cv_rrect(3, 2, 234, 79, 6, T_SURF, T_BG);
    /* (1.2.6) THE PANEL IS THE CONSOLE'S GRADIENT, like FX > BUSES' (fxb_grad): the plate's own ramp is a light
     * bevel, and on this page the owner read it as a flat box next to a screen that is a gradient. ux_ground_rows
     * walks the accent from 10 to 45 % over the panel's height, solid, so the page's light comes from below. */
    ux_ground_rows(5, 3, 230, 77, 22, 62, 10, 45, 3, 81, 100);
    for (i = 0; i < 4u; i++) {
        int32_t cx = 6 + (int32_t)i * 57;
        int32_t val = clamp(t->p[P_DIST + i], 0, 127);
        /* (1.3.1) THE METER STARTS UNDER THE NAME, with air between them: "CHORUS" and "REVERB" are six
         * glyphs of AF_S and their line runs to cx + 39, right over the meter's column (cx + 35), so the bar
         * used to cut through the last two letters. The name is one row higher (ink 8 .. 16, its line ends on
         * 17) and the meter opens on 22: six rows of air, and the bar is still 16 x 43 (22 .. 64). */
        int32_t mx = cx + 35, my = 22, mh = 43;
        /* (v1.8.4) THE METER'S FULL SCALE IS THE SIGNAL'S, 32767. These bars were clamped to 7000, a scale
         * from when the track's envelope was a 0..7000 peak; fx.c has measured |x| of the part's own sample
         * (Q15, 0..32767) since v1.9, so ANY note -- 17 638 was the probe's plain one, 252 % of 7000 -- put
         * DIST at the top before a hand touched it, and the owner read it as "1 % of distortion pegs the
         * meter". It was the scale, not the drive. The four cards, their clip dots and the send ticks now use
         * the same 32767 the mixer's meters and the MASTER VU use. */
        int32_t lvl = clamp(bus[i], 0, 32767);        /* the return, not the fader */
        int32_t inpx = clamp(send[i], 0, 32767) * mh / 32767;
        char num[8];

        cv_rrect(cx, 4, 54, 75, 5, T_SURF, T_BG);
        ux_ground_rows(cx + 2, 5, 50, 73, 22, 62, 10, 45, 5, 78, 100);   /* (1.2.6) its own gradient */
        /* (1.2.6) its texts sit on the gradient now, not on a flat T_SURF: the background they are blended over is
         * the ramp's own colour at that row, or the letters would be drawn on a tone they are not lying on. */
        cv_text_on(cx + 3, 5, &AF_S, FX_LABELS[i], T_TEXT, ux_ground_at(0, 73, 10, 45));   /* the whole name, on its own line */
        /* (1.3) the value owns the card's left column and the icon sits under it: with the meter 16 px wide the
         * value could not stay right-aligned to it without running into the icon (the old pair was 12 px apart). */
        cv_icon_on(cx + 3, 33, 12, FX_ICON[i], T_MID, ux_ground_at(28, 73, 10, 45));
        /* (3.1) the four sends are MIDI 0..127 inside (presets, automation, the DSP's own maths keep
         * that range) and 0..100 % on screen: the conversion happens here, once, for the label. */
        fmt_int(num, (val * 100 + 63) / 127);
        str_cpy(num + str_len(num), "%", sizeof num - str_len(num));
        /* (3.2) right-aligned to the meter's left edge, not drawn from a fixed x: "100 %" is one
         * glyph wider than "99 %" and the old fixed x put its % over the meter's column (the
         * owner's overflow). 2 px of air, whatever the value, on every palette and style. */
        cv_text_on(cx + 3, 18, &AF_S, num, val ? T_TEXT : T_DIM, ux_ground_at(13, 73, 10, 45));
        /* (1.3) the type / SEND label moved down with the card: it was at y 40 on a 62 px card and is at 71 on an
         * 80 px one, so the meter gets the rows between the value and it instead of the label splitting them. */
        cv_text_on(cx + 3, 66, &AF_S, i == 0 ? DIST_TYPES[(uint32_t)t->p[P_DTYPE] % 6u] : "SEND",
                   i == 0 ? T_ACCENT : T_DIM, ux_ground_at(61, 73, 10, 45));
        if (lvl >= 32700)                             /* (3.6) this bus is at full scale: it clips now */
            cv_rect(cx + 47, 6, 3, 3, T_ACCENT);
        else if (lvl > 64)                            /* (3.6) this bus is producing sound now: its own
                                                       * return, not the track's envelope (which lit all
                                                       * four cards together whatever was running) */
            cv_rect(cx + 47, 6, 3, 3, T_THEME);
        else
            cv_rect(cx + 47, 6, 3, 3, T_LINE);

        /* (3.6) The meter is an instrument, not a decoration. fx.c measures each bus's peak once a
         * block (fx_vu) while the UI redraws at ~30 fps: drawing that raw peak made the bar jump with
         * whichever block the frame happened to land on, and the peak-hold arrays were never written at
         * all -- vu[] and hold[] stayed 0, so the trail, the hold line and the input tick were dead code
         * drawing nothing, with the same bar drawn three times at three different lengths. Now vu[i]
         * follows the real level with an immediate attack and a ~4-frame release, hold[i] keeps its
         * highest point for 30 frames and then falls a pixel a frame, and one thin tick marks what the
         * bus actually receives. Every figure is the DSP's own. */
        cv_rect(mx, my, 16, mh, T_BG);                /* the track: 16 of the card's 54 px, 3 px from its edge */
        {   /* (3.7) HOME's own meter: the segmented plasma bar, with the same attack/decay ballistics */
            int32_t target = fx_meter_target(lvl);
            uint16_t heat0 = i == 0 ? 60u : (uint16_t)(96u + (uint32_t)i * 24u);
            gw_plasma_bar(mx + 2, my, 12, mh, 8, fx_meter_step(&vu[i], &hold[i], &hold_t[i], target, 255), heat0);
        }
        /* (FASE 4) and the fader's own setting beside the meter, in the 5 px the value's line leaves free
         * between cx + 29 (where "100 %" ends) and mx - 1: the two bars now answer both questions at a
         * glance -- what is asked for (thin, left) and what is coming back (plasma, right). */
        fx_fader_bar(mx - 5, my, mh, (val * 100 + 63) / 127);
        if (i && inpx > 0)                            /* what the bus receives, over the return */
            cv_rect(mx + 6, my + mh - inpx, 4, 1, T_MID);
        if (lvl >= 32700)                             /* full scale: the head of the meter clips */
            cv_rect(mx + 2, my, 12, 2, T_ACCENT);
    }

    /* (1.3.1) THE THREE MASTER MODULES ARE INSTRUMENTS: each one now shows the thing it measures and moves
     * with it. They are 76 x 39 at 82 .. 120 (the rows the page used to leave empty), with the value on its
     * own line and a 68 x 9 bar under it.
     *   - EQ is bipolar: the bar's centre is 0 dB, the fill runs from the centre to the gain's position and
     *     the head sits there. Flat and bypassed read at a glance (no fill, head on the centre, DIM).
     *   - DEPTH keeps a quarter scale over the fill, so 25 / 50 / 75 % are readable, and its head marks the
     *     exact gate.
     *   - SLICER lights the 16 steps of the pattern it will actually gate (sl_pattern) and the running one
     *     stands a pixel taller in the accent colour, so the page and the audio agree frame by frame.
     * All three ease with the meters' ballistics (fx_ease: immediate attack, a quarter down per frame), so a
     * knob turned fast does not tear the bar and a value that stops still settles in a couple of frames. */
    {
        static const int32_t MX3[3] = {3, 82, 161};
        static const uint16_t IC[3] = {ICON_TONE, ICON_LEVEL, ICON_STEPS};
        static const char *const NM[3] = {"EQ", "DEPTH", "SLICER"};
        static int32_t bar[3] = {-1, -1, -1};         /* the three fills, 0..68 px, eased */
        uint32_t m;
        char vb[12];
        for (m = 0; m < 3u; m++) {
            /* (FASE 2/8) THE SEVEN BOXES ARE ONE FAMILY. The three modules were a different ground: a 3 px corner
             * and a ramp of their own (14..50 over T_SURF, 6..32 over the accent, 37 rows) beside the four
             * cards' 5 px corner and 22..62 / 10..45 -- and two of their texts asked ux_ground_at for the
             * accent band (10..45) while their body had been filled with (6..32), so the icon and the name sat
             * on a tone that was not under them (a halo the layout lint cannot see). Same corner, same ramp,
             * same direction; only the height differs, and the row asked of ux_ground_at is the module's own. */
            cv_rrect(MX3[m], 82, 76, 39, 5, T_SURF, T_BG);
            ux_ground_rows(MX3[m] + 2, 83, 72, 37, 22, 62, 10, 45, 83, 119, 100);   /* (1.2.6) the bottom row too */
            cv_icon_on(MX3[m] + 4, 83, 12, IC[m], T_MID, ux_ground_at(0, 37, 10, 45));
            cv_text_on(MX3[m] + 19, 83, &AF_S, NM[m], T_DIM, ux_ground_at(0, 37, 10, 45));
            cv_rect(MX3[m] + 4, 108, 68, 9, T_BG);        /* the bar's track, under the value */
        }
        /* EQ: its mode (N_EQMODE) and where the gain sits, from the centre out */
        {
            uint32_t em = (uint32_t)clamp(song.g[G_EQMODE], 0, 4);
            int32_t gdb = (int32_t)clamp(song.g[G_EQGAIN], -24, 24);
            int32_t px = (gdb + 24) * 62 / 48 + (gdb >= 0 ? 3 : 0);   /* -24 .. +24 dB, marker 4 wide */
            fx_ease(&bar[0], px);
            if (em) {
                int32_t c = 34;                        /* the centre: 0 dB */
                str_cpy(vb, N_EQMODE[em], sizeof vb);
                cv_text_on(MX3[0] + 4, 94, &AF_S, vb, T_THEME, ux_ground_at(11, 37, 10, 45));
                if (bar[0] >= c)
                    cv_rect(MX3[0] + 4 + c, 110, bar[0] - c, 5, T_THEME);   /* boost: centre -> right */
                else
                    cv_rect(MX3[0] + 4 + bar[0], 110, c - bar[0], 5, T_THEME);   /* cut: left -> centre */
                cv_rect(MX3[0] + 4 + bar[0], 108, 4, 9, T_ACCENT);   /* where the gain sits */
            } else {
                cv_text_on(MX3[0] + 4, 94, &AF_S, "BYP", T_DIM, ux_ground_at(11, 37, 10, 45));
                cv_rect(MX3[0] + 4 + 32, 110, 2, 5, T_DIM);  /* flat and bypassed: the centre only */
            }
            cv_rect(MX3[0] + 4 + 34, 108, 1, 9, T_LINE);      /* the 0 dB line, over the fill */
        }
        /* DEPTH: the slicer's gate (song.g[G_SLDEP], 0..127), filled with its quarter scale */
        {
            int32_t d = clamp(song.g[G_SLDEP], 0, 127), w = d * 68 / 127, q;
            fx_ease(&bar[1], w);
            fmt_int(vb, d * 100 / 127);
            str_cpy(vb + str_len(vb), "%", 2);
            cv_text_on(MX3[1] + 4, 94, &AF_S, vb, d ? T_TEXT : T_DIM, ux_ground_at(11, 37, 10, 45));
            if (bar[1] > 0)
                cv_rect(MX3[1] + 4, 110, bar[1], 5, song.g[G_SLMODE] ? T_THEME : T_DIM);
            if (bar[1] > 0)                            /* the head, and where it is going */
                cv_rect(MX3[1] + 2 + bar[1], 108, 2, 9, d ? T_ACCENT : T_DIM);
            for (q = 1; q < 4; q++)                    /* 25 / 50 / 75 % over the fill */
                cv_rect(MX3[1] + 4 + q * 17, 110, 1, 5, T_BG);
        }
        /* SLICER: OFF / GATE / STUT, the pattern it will gate and the step running now (slicer.c sl.idx) */
        {
            uint32_t sm = (uint32_t)clamp(song.g[G_SLMODE], 0, 2), k, pat = sl_pattern();
            str_cpy(vb, N_SLCR[sm], sizeof vb);
            cv_text_on(MX3[2] + 4, 94, &AF_S, vb, sm ? T_THEME : T_DIM, ux_ground_at(11, 37, 10, 45));
            for (k = 0; k < 16u; k++) {
                int32_t sx = MX3[2] + 4 + (int32_t)k * 4;
                int on = sm && ((pat >> k) & 1u), now = sm && k == (sl.idx & 15u);
                if (now)                               /* the step the audio is on: taller, accent */
                    cv_rect(sx, 107, 3, 10, T_ACCENT);
                else if (on)
                    cv_rect(sx, 109, 3, 8, T_THEME);
                else
                    cv_rect(sx, 111, 3, 6, T_RAISE);
            }
        }
    }
}
/* SLICER and SL BUS pages (JIANT 0.6.3: one, on a bus): the pattern's 16 steps, a 'x' step a full bar; a '.' step: GATE
 * a bar as high as it stays open (DEPTH), STUT hatched (it repeats the last 'x'); the step playing underlined. Under
 * them what goes in: T1..T6 and FX, lit ON. Grey when OFF */
static __attribute__((noinline)) void graph_slicer(uint16_t c)
{
    uint32_t i, pat = sl_pattern(), mode = (uint32_t)song.g[G_SLMODE], cur = sl.idx;
    int32_t open = 52 - song.g[G_SLDEP] * 52 / 127;     /* px a closed GATE step keeps */
    uint16_t col = mode == SL_OFF ? T_DIM : c;
    if (ux.chamfer && !ux.style) {
        /* QUANTUM GAMMAWAVE // QG-1: 4-Track Analog Audio Waveform Tape Deck (y = 4..69) */
        int32_t lane, x, head_x = 18 + (int32_t)(cur & 15u) * 13;
        gw_oct_panel(8, 4, 224, 65, 4, T_BG, T_LINE, 1, T_THEME);
        /* Top Yellow Tape Loop Bracket */
        cv_rect(52, 6, 136, 2, bg_col(228));
        cv_rect(52, 6, 2, 5, bg_col(228));
        cv_rect(186, 6, 2, 5, bg_col(228));
        /* 4 Continuous Scrolling Analog Audio Waveform Lanes (T1 DUB, T2 PLAY, T3 PLAY, T4 PLAY) */
        for (lane = 0; lane < 4; lane++) {
            int32_t cy = 17 + lane * 14, on = song.g[G_SLT1 + lane] != 0;
            uint16_t lc = !on ? T_DIM : lane == 0 ? bg_col(176) : lane == 1 ? T_ACCENT : lane == 2 ? bg_col(224) : bg_col(112);
            cv_rect(12, cy, 216, 1, T_GRID);
            /* Left Track Dub/Play LED */
            cv_rect(13, cy - 2, 4, 5, lane == 0 ? bg_col(184) : on ? T_ACCENT : T_RAISE);
            for (x = 20; x < 226; x += 2) {
                uint32_t step = (uint32_t)((x - 20) * 16 / 206);
                int32_t gate = ((pat >> step) & 1u) ? 5 : (open * 5 / 52);
                uint32_t sidx = (scope_w - 226u + (uint32_t)x + (uint32_t)lane * 37u) & (SCOPE_N - 1u);
                int32_t env = ((scope_buf[sidx] < 0 ? -scope_buf[sidx] : scope_buf[sidx]) * gate / 9000) +
                              (((bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u) < 0
                                    ? -bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u)
                                    : bg_sin((uint32_t)(x * (1100 + lane * 420)) + ui.frame * 900u)) * gate) >> 15);
                env = clamp(env, 1, 5);
                cv_rect(x, cy - env, 1, 2 * env + 1, lc);
            }
        }
        /* Vertical White-Hot Analog Tape Head */
        cv_rect(head_x, 6, 2, 61, T_TEXT);
        cv_rect(head_x - 2, 5, 6, 3, T_THEME);
        cv_rect(head_x - 2, 65, 6, 3, T_THEME);
    } else {
        for (i = 0; i < 16u; i++) {
            int32_t x = bar_x(i), y;
            if ((pat >> i) & 1u) {
                cv_rrect(x, 8, 9, 54, 2, mode == SL_OFF ? T_RAISE : col, T_SURF);
            } else if (mode == SL_STUT) {
                for (y = 8; y < 62; y += 4)
                    cv_rect(x, y, 9, 1, col);
            } else {
                cv_rrect(x, 59, 9, 3, 1, T_RAISE, T_SURF);
                if (open > 3)
                    cv_rrect(x, 62 - open, 9, open, 2, T_RAISE, T_SURF);
            }
            if (mode != SL_OFF && i == cur)
                cv_rect(x, 65, 9, 2, T_ACCENT);
        }
    }
    for (i = 0; i < NTRK + 1u; i++) {                   /* T1..T6, FX: in or not */
        int32_t x = 8 + 32 * (int32_t)i, on = song.g[G_SLT1 + i] != 0;
        uint16_t fill = on ? (mode == SL_OFF ? T_MID : c) : T_RAISE, ink = on ? T_INK : T_DIM;
        char b[3] = {(char)('1' + i), 0, 0};
        if (i == NTRK) { b[0] = 'F'; b[1] = 'X'; }
        cv_rrect(x, 74, 26, 16, 4, fill, T_SURF);
        GFX_HOOK_ALIGN(x, 74, x + 26, 90, AL_HV, "SLICER bus chip label centred");
        cv_text_in(x, 74 + CAP_IN(S, 16), 26, &AF_S, b, ink, fill);
    }
}
/* (FASE 3) ARP: the keys the player is holding, the note the arpeggiator is sounding and the route it
 * walks, read from the sequencer's own state (track_t's held/arp_note/arp_ch/arp_idx/arp_walk) and the
 * parameters the engine plays. Nothing here is a decorative pattern: the keyboard lights the keys that
 * are down, the route strip lists the notes the arp built (sorted or as played, over OCT octaves) and
 * marks the place arp_pick took this step, and the three meters are GATE, SWING and PROB as stored.
 * A page with the arp off says so and draws no route. */
static void arp_key(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t face, uint16_t brd)
{
    /* (v1.12.11) A KEY WITH A LIGHT ON IT. The face was one flat colour and a two-line contour, so the fourteen
     * naturals read as grey rectangles and the accidentals as five loose chips. Three bands from the lit top to
     * the shaded body give each key the same ramp SCL's keybed uses, and the bottom and right edges keep the
     * contour: the keybed now reads as one instrument instead of fourteen buttons. */
    int32_t k, n = h >= 12 ? 3 : 2, bh = h / n;
    for (k = 0; k < n; k++)
        cv_rect(x, y + k * bh, w, k == n - 1 ? h - bh * (n - 1) : bh, ux_mix(face, brd, 30 - k * 15));
    cv_rect(x, y + h - 1, w, 1, brd);
    cv_rect(x + w - 1, y, 1, h, brd);
}
/* (FASE 3) the three read-only meters under ARP's route: label and value on one row, a 3 px bar two rows under
 * them. The label was T_DIM on SURF and the bar 5 px tall starting at y + 14, which put its foot on the footer's
 * own row: the panel's last line read as text crossed by three bars. The label is T_MID now, the value T_TEXT,
 * and the bar is 3 px at y + 13, clear of both. */
static void arp_meter(int32_t x, int32_t y, const char *lab, int32_t v, int32_t max, uint16_t c)
{
    char b[8];
    int32_t pct = (v * 100 + max / 2) / max, w = v * 50 / max;
    cv_text(x, y, &AF_S, lab, T_MID);
    fmt_int(b, pct);
    str_cpy(b + str_len(b), "%", sizeof b - str_len(b));
    cv_text_r(x + 62, y, &AF_S, b, T_TEXT, T_SURF);
    cv_rect(x, y + 13, 50, 3, T_RAISE);
    if (w > 0)
        cv_rect(x, y + 13, w < 2 ? 2 : w, 3, c);
}
/* the place in the route this step took: the same formulas arp_pick uses, with arp_idx having already
 * advanced one past the pick. RND cannot be known before it is drawn (a sentinel), CHORD takes all. */
static uint32_t arp_route_now(const track_t *t, uint32_t len)
{
    uint32_t m = (uint32_t)clamp(t->p[P_AMODE], 0, 15), i = t->arp_idx ? t->arp_idx - 1u : 0u;
    if (!m || !len)
        return 0xFFFFFFFFu;
    if (m == AM_RND)
        return 0xFFFFFFFEu;                       /* the next pick is not knowable: no place is marked */
    if (m == AM_WALK)
        return (uint32_t)t->arp_walk < len ? t->arp_walk : len - 1u;
    if (m == AM_CHORD)
        return 0u;
    return arp_step_index(m, i, len);
}
static __attribute__((noinline)) void graph_arp(const track_t *t, uint16_t c)
{
    static const uint8_t WN[7] = {0, 2, 4, 5, 7, 9, 11};          /* C D E F G A B */
    static const uint8_t BK[5] = {1, 3, 6, 8, 10};                  /* the five accidentals */
    static const uint8_t BW[5] = {0, 1, 3, 4, 5};                   /* the white key before each */
    uint32_t i, j, o, m = (uint32_t)clamp(t->p[P_AMODE], 0, 15), oct = (uint32_t)clamp(t->p[P_AOCT], 1, 4);
    uint32_t len = 0, list[64], cnt, cur, start = 0, nh = t->nheld;
    int32_t gate = clamp(t->p[P_AGATE], 1, 127), swing = clamp(t->p[P_ASWING], 0, 100);
    int32_t prob = clamp(t->p[P_APROB], 0, 127), base, lo = 128, hi = -1;
    char b[24], q[10];

    for (i = 0; i < nh; i++) {
        list[len++] = t->held[i];
        if (t->held[i] < lo) lo = t->held[i];
        if ((int32_t)t->held[i] > hi) hi = t->held[i];
    }
    cnt = len;
    len = 0;                                    /* the list is rebuilt from its held notes, as arp_step does */
    if (!t->p[P_AORDER])
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                uint32_t x = list[j]; list[j] = list[j - 1]; list[j - 1] = x;
            }
    for (o = 0; o < oct; o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (m == AM_REPEAT) { list[0] = nh ? t->held[nh - 1u] : 0u; len = 1; }
    if (m == AM_UP8 && len && len < 64u)
        list[len++] = (uint32_t)clamp((int32_t)list[0] + 12 * (int32_t)oct, 0, 127);
    if (m == AM_TRNS || !m) len = 0;
    cur = arp_route_now(t, len);

    /* header: the mode and whether the keys are being held or latched */
    str_cpy(b, "ARP  ", sizeof b);
    str_cpy(b + str_len(b), N_AMODE[m], sizeof b - str_len(b));
    if (t->p[P_AHOLD])
        str_cpy(q, "LATCH", sizeof q);
    else {
        fmt_int(q, (int32_t)nh);
        str_cpy(q + str_len(q), nh == 1u ? " KEY" : " KEYS", sizeof q - str_len(q));
    }
    scl_head(b, q, t->p[P_AHOLD] ? T_ACCENT : nh ? T_MID : T_DIM);
    cv_rect(12, 17, 216, 1, T_LINE);

    /* the keyboard: two octaves around the keys held, so what is down is always on it */
    base = lo < 128u ? (int32_t)(lo / 12u) * 12 : 48;
    if (hi >= 0 && hi > base + 23)
        base = (int32_t)(hi / 12u) * 12 - 12;
    base = clamp(base, 0, 104);
    for (o = 0; o < 2u; o++)
        for (i = 0; i < 7u; i++) {
            int32_t note = base + (int32_t)(12u * o) + WN[i], x = 14 + (int32_t)(7u * o + i) * 15;
            uint32_t k;
            int down = 0, snd = note == (int32_t)t->arp_note;
            for (k = 0; k < nh; k++)
                if (t->held[k] == note) down = 1;
            for (k = 0; k < 3u; k++)
                if (t->arp_ch[k] && t->arp_ch[k] == note) snd = 1;
            arp_key(x, 20, 14, 39, snd ? T_ACCENT : down ? T_THEME : ux_mix(T_KEYW, T_BG, 20),
                    snd ? T_TEXT : down ? T_THEME : ux_mix(T_KEYW, T_BG, 58));
            /* (FASE 3) the name on the key's foot, in the ink the face takes: T_DIM on a light key left the
             * letters as grey on grey. Every one of the fourteen white keys is named, held or not. */
            cv_text_in(x, 46, 14, &AF_S, N_NOTE[(uint32_t)note % 12u],
                       snd ? T_INK : ux_mix(T_KEYW, T_BG, down ? 96 : 88),
                       snd ? T_ACCENT : down ? T_THEME : ux_mix(T_KEYW, T_BG, 20));
        }
    for (o = 0; o < 2u; o++)
        for (i = 0; i < 5u; i++) {
            int32_t note = base + (int32_t)(12u * o) + BK[i];
            int32_t x = 14 + (int32_t)(7u * o + BW[i] + 1u) * 15 - 5;
            uint32_t k;
            int down = 0, snd = note == (int32_t)t->arp_note;
            for (k = 0; k < nh; k++)
                if (t->held[k] == note) down = 1;
            for (k = 0; k < 3u; k++)
                if (t->arp_ch[k] && t->arp_ch[k] == note) snd = 1;
            arp_key(x, 20, 11, 22, snd ? T_ACCENT : down ? T_THEME : T_KEYB,
                    snd ? T_TEXT : down ? T_THEME : ux_mix(T_KEYB, T_TEXT, 26));
            /* (FASE 3) AND THE FIVE ACCIDENTALS ARE NAMED. A 9 px key has no room for "C#" on one line, so the
             * letter sits on the key's head and its sharp under it: the two rows of the small face fit the 25 px
             * key, and both of them are in the ink that reads on the key's own face. */
            {
                uint16_t kc = snd ? T_INK : down ? T_THEME : ux_mix(T_KEYB, T_TEXT, 78);
                char nn[2] = {N_NOTE[(uint32_t)note % 12u][0], 0};
                /* (v1.12.11) 11 px wide and two rows 1 px apart from the key's edges: the letter and its sharp
                 * used to sit 10 px below it on a 9 px face with a blank row in between, which read as two
                 * unrelated glyphs floating over the naturals. */
                cv_text_in(x, 20, 11, &AF_S, nn, kc, snd ? T_ACCENT : down ? T_THEME : T_KEYB);
                cv_text_in(x, 30, 11, &AF_S, "#", kc, snd ? T_ACCENT : down ? T_THEME : T_KEYB);   /* 1 px clear of the letter's last row */
            }
        }

    /* the route: the notes the arp built, in the order the list holds them, and the one playing now */
    cv_text(14, 59, &AF_S, "ROUTE", T_MID);
    if (t->arp_note) {                                  /* (FASE 8) what it is sounding right now, as note names */
        char s[24];
        uint32_t k;
        s[0] = 0;
        str_cpy(s, N_NOTE[t->arp_note % 12u], sizeof s);
        fmt_int(s + str_len(s), (int32_t)t->arp_note / 12 - 1);
        for (k = 0; k < 3u; k++)
            if (t->arp_ch[k] && str_len(s) < sizeof s - 4u) {
                str_cpy(s + str_len(s), " ", sizeof s - str_len(s));
                str_cpy(s + str_len(s), N_NOTE[t->arp_ch[k] % 12u], sizeof s - str_len(s));
                fmt_int(s + str_len(s), (int32_t)t->arp_ch[k] / 12 - 1);
            }
        cv_text(56, 59, &AF_S, s, T_ACCENT);
    }
    if (len && m == AM_CHORD)
        cv_text_r(226, 59, &AF_S, "ALL", T_MID, T_SURF);
    else if (len && m == AM_RND)
        cv_text_r(226, 59, &AF_S, "RND", T_MID, T_SURF);
    else if (len) {
        fmt_int(q, (int32_t)(cur < len ? cur + 1u : 0u));
        str_cpy(q + str_len(q), "/", sizeof q - str_len(q));
        fmt_int(q + str_len(q), (int32_t)len);
        cv_text_r(226, 59, &AF_S, q, T_MID, T_SURF);
    } else
        cv_text_r(226, 59, &AF_S, m == AM_TRNS ? "TRNS" : m ? "NO KEYS" : "OFF", T_DIM, T_SURF);
    if (len) {
        if (cur < len && cur >= 8u)
            start = cur - 7u;
        for (i = 0; i < 8u && start + i < len; i++) {
            uint32_t idx = start + i, note = list[idx];
            int on = m == AM_CHORD || idx == cur;
            int32_t x = 10 + (int32_t)i * 25, cy = 74;
            char n[6];
            str_cpy(n, N_NOTE[note % 12u], sizeof n);
            fmt_int(n + str_len(n), (int32_t)note / 12 - 1);
            cv_rrect(x, cy, 24, 13, 2, on ? T_ACCENT : T_RAISE, T_SURF);
            /* (FASE 3) what the synth is sounding THIS step, not only the arp's place in the list: the note and
             * the three voices of a chord are marked on their chips, so the route and the keyboard tell the same
             * story at the same instant. */
            if (note == (uint32_t)t->arp_note || note == (uint32_t)t->arp_ch[0] ||
                note == (uint32_t)t->arp_ch[1] || note == (uint32_t)t->arp_ch[2]) {
                cv_rect(x, cy, 24, 1, T_TEXT);
                cv_rect(x, cy + 12, 24, 1, T_TEXT);
                cv_rect(x, cy, 1, 13, T_TEXT);
                cv_rect(x + 23, cy, 1, 13, T_TEXT);
            }
            cv_text_in(x, cy + CAP_IN(S, 13), 24, &AF_S, n, on ? T_INK : T_MID, on ? T_ACCENT : T_RAISE);
        }
        /* (FASE 8) the clock, right above the notes: the slot of the step being played fills as its time runs and the
         * tick is where GATE closes the note. arp_pos and arp_off are the sequencer's own counters (samples into the
         * step and to the note-off), so this moves with the clock, swing included, and stops when it stops */
        {
            int32_t per = (int32_t)div_samples((uint32_t)clamp(t->p[P_ARATE], 0, 9));
            int32_t pos, gw, g;
            if (per < 1)
                per = 1;
            pos = t->arp_pos > (uint32_t)per ? per : (int32_t)t->arp_pos;
            gw = pos * 24 / per;
            for (i = 0; i < 8u && start + i < len; i++) {
                int32_t x = 10 + (int32_t)i * 25;
                cv_rect(x, 71, 24, 2, T_RAISE);
                if (m == AM_CHORD || start + i == cur) {
                    if (gw > 0)
                        cv_rect(x, 71, gw, 2, T_ACCENT);
                    if (t->arp_off) {
                        g = (pos + (int32_t)t->arp_off) * 24 / per;
                        if (g < 24)
                            cv_rect(x + g, 71, 1, 4, T_TEXT);
                    }
                }
            }
        }
    }
    arp_meter(14, 88, "GATE", gate, 127, c);
    arp_meter(88, 88, "SWING", swing, 100, c);
    arp_meter(162, 88, "PROB", prob, 127, c);

    /* footer: the values the two ARP pages own, in the words the descriptors use */
    str_cpy(b, "RATE ", sizeof b);
    str_cpy(b + str_len(b), N_DIV[(uint32_t)clamp(t->p[P_ARATE], 0, 9)], sizeof b - str_len(b));
    cv_text(14, 106, &AF_S, b, T_MID);
    str_cpy(b, "OCT x", sizeof b);
    fmt_int(b + str_len(b), (int32_t)oct);
    cv_text(96, 106, &AF_S, b, T_MID);
    str_cpy(b, "ORDER ", sizeof b);
    str_cpy(b + str_len(b), N_ORDER[t->p[P_AORDER] ? 1 : 0], sizeof b - str_len(b));
    cv_text_r(226, 106, &AF_S, b, T_MID, T_SURF);
}

/* SL PITCH (JIANT 0.6.3): the repeats' 16 pitch steps as bars from the middle (+-24 semitones), past LEN dim, the one
 * KNOB 2 edits underlined, the one playing (STUT running) lit */
static __attribute__((noinline)) void graph_slpitch(uint16_t c)
{
    uint32_t i, len = (uint32_t)clamp(song.g[G_SLPLEN], 1, 16), now = song.g[G_SLMODE] == SL_STUT ? sl.idx % len : 0xFFu;
    int32_t y0 = 46, hmax = 38;
    cv_rect(10, y0, 220, 1, T_RAISE);
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i), v = song.g[G_SLP0 + i], h = v * hmax / 24;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : ux.chamfer ?
                       heat_col(128 + v * 5) : c;
        if (h > 0)
            cv_rrect(x, y0 - h, 9, h, 2, col, T_SURF);
        else if (h < 0)
            cv_rrect(x, y0 + 1, 9, -h, 2, col, T_SURF);
        else
            cv_rect(x, y0 - 1, 9, 3, col);
        if (i == sl_ui_step)
            cv_rect(x, y0 + hmax + 6, 9, 2, T_TEXT);
    }
}
/* (QG-1) GLO MONITOR: real system diagnostics, core load, active voices, DSP & buses.
 * Only displays authentic measurements; displays "N/A" if counter is zero or uninstrumented.
 * (QG-1 v1.5) seven rows now: how the parts split between the cores (mc_mask_last, from fx.c mc_split) and how long
 * core 1's last job really took (hal/fm1_multicore.h fm1_mc_job_us, stamped by core 1 itself around the job and
 * published before `done`), plus the late blocks and the voices shed for them (audio.c load_diag). Every figure has
 * a writer; none is a constant. */
static __attribute__((noinline)) void graph_diag(uint16_t c)
{
    uint32_t i, n1 = 0, n0, step = 0, slen = 1, sounding, release, fading, peak, steals, denied, cap;
    char b[64];                    /* (v1.5.13) the longest row (POLYPHONY with every counter up) is 55 */
    if (ui.force)                                   /* (FASE 2, #107) the peak is since this screen was opened */
        voice_stats_reset();
    voice_stats(&sounding, &release, &fading, &peak, &steals, &denied);
    for (i = 0; i < NPART; i++)                     /* the parts core 1 took in the last block */
        n1 += (mc_mask_last >> i) & 1u;
    if (n1 > trk_live) n1 = trk_live;
    n0 = trk_live - n1;

    if (ux.chamfer && !ux.style) {
        gw_oct_panel(8, 4, 224, 102, 4, T_BG, T_LINE, 1, c);
    } else {
        cv_rrect(8, 4, 224, 102, 4, T_SURF, T_BG);
    }

    /* Row 1: MULTICORE STATUS. mc_status() already spells the whole state ("CORES 2", "CORES 1 (FAULT)"), the
     * fallback and the fault included, so the caption must not repeat it */
    cv_text(16, 8, &AF_S, "MULTICORE", T_MID);
    if (fx_trips) {                                 /* the buses reset out of range (fx.c fx_trip): a fault */
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_str(&r, mc_status());
        rowb_str(&r, " TRIP ");
        rowb_int(&r, (int32_t)fx_trips);
        cv_text_r(224, 8, &AF_S, b, T_ACCENT, T_SURF);
    } else
        cv_text_r(224, 8, &AF_S, mc_status(), T_THEME, T_SURF);

    /* Row 2: the audio's own load (audio.c song.cpu_q8, averaged) and the worst block since power-on.
     * (3.0) With nothing measured -- the host and the browser emulator do not advance the tick inside the
     * render -- it says N/A: a 0 % there was a measurement that never happened. */
    cv_text(16, 22, &AF_S, "CPU LOAD", T_MID);
    if (load_diag.valid) {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_int(&r, (int32_t)(song.cpu_q8 * 100u / 256u));
        rowb_str(&r, "% (PK ");
        rowb_int(&r, (int32_t)load_diag.peak);
        rowb_str(&r, "%)");
        if (load_diag.shed) {                       /* (v1.5.23, FASE 2) the overload guard's own count: how many
                                                     * voices it took since power-on. It was only in MENU > ABOUT,
                                                     * so the screen that shows the load could not say what the load
                                                     * had cost. */
            rowb_str(&r, " SHD ");
            rowb_int(&r, (int32_t)load_diag.shed);
        }
    } else {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_str(&r, "N/A");
    }
    cv_text_r(224, 22, &AF_S, b, !load_diag.valid ? T_MID : song.cpu_q8 > 200u ? T_ACCENT : T_TEXT, T_SURF);

    /* Row 3: HOW THE PARTS SPLIT, and how long core 1's last job took (its own stamp on the shared TIMER4) */
    cv_text(16, 36, &AF_S, "CORE SPLIT", T_MID);
    {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_str(&r, "C0 ");
        rowb_int(&r, (int32_t)n0);
        rowb_str(&r, " C1 ");
        rowb_int(&r, (int32_t)n1);
#if FELUCCA_DUAL_CORE                              /* the figure only exists with a second core built in */
        if (mc_live()) {                            /* .. and once core 1 has actually run a job */
            rowb_str(&r, " ");
            rowb_int(&r, (int32_t)fm1_mc_job_us());
            rowb_str(&r, "us");
        }
#endif
        if (mc_fx_tk) {                             /* the buses' own block ticks (fx.c fx_run) */
            rowb_str(&r, " ");
            rowb_int(&r, (int32_t)mc_fx_tk);
            rowb_str(&r, " TK");
        } else
            rowb_str(&r, " N/A TK");
    }
    cv_text_r(224, 36, &AF_S, b, n1 ? T_THEME : T_TEXT, T_SURF);

    /* Row 4: POLYPHONY, told apart: what holds a note, what is ringing out, what the allocator is fading, the peak
     * since the screen was opened, and how many voices it has taken. (FASE 2, #107: the old figure was a single
     * count sampled at the UI's frame, so a release read as a voice and a peak between two frames was invisible.) */
    cv_text(16, 50, &AF_S, "POLYPHONY", T_MID);
    cap = voice_budget();                           /* (v1.5.13) the cap in force, not the pool's size: LOAD ECO
                                                     * shares 6 voices between the parts and this row printed 12
                                                     * anyway, which is the figure a player compares a chord to */
    {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_int(&r, (int32_t)sounding);
        rowb_str(&r, " / ");
        rowb_int(&r, (int32_t)cap);
        rowb_str(&r, " VOICES");
        if (release) {
            rowb_str(&r, " REL ");
            rowb_int(&r, (int32_t)release);
        }
        if (fading) {
            rowb_str(&r, " FADE ");
            rowb_int(&r, (int32_t)fading);
        }
        if (peak) {
            rowb_str(&r, " PK ");
            rowb_int(&r, (int32_t)peak);
        }
        if (steals) {
            rowb_str(&r, " STL ");
            rowb_int(&r, (int32_t)steals);
        }
        if (denied) {                               /* a note that found no voice at all: never a silent steal */
            rowb_str(&r, " DEN ");
            rowb_int(&r, (int32_t)denied);
        }
    }
    cv_text_r(224, 50, &AF_S, b, sounding + 1u >= cap ? T_ACCENT : T_TEXT, T_SURF);

    /* Row 5: ACTIVE TRACKS T1..T6 VOICE BLOCKS (a muted track reads dim, a sounding one accent). (v1.5.23) The block
     * was one colour for "any voice", so a part holding a chord and a part whose chord had already been let go read
     * the same. Now the block is accent while a key or the sequencer holds a voice and theme-coloured when only its
     * release tails are left, with a bar under it as long as the tails still ringing: the two things the POLYPHONY row
     * counts for the whole instrument, per part. */
    for (i = 0; i < 6u; i++) {
        int32_t tx = 16 + (int32_t)i * 35;
        uint32_t v, tg = 0, tr = 0;
        char tb[4] = {'T', (char)('1' + i), 0, 0};
        int32_t live = i < trk_live;
        if (live)
            for (v = 0; v < NVOICE; v++) {
                const voice_t *tvv = &V(&trk[i], v);
                if (!tvv->active)
                    continue;
                if (tvv->gate && tvv->stage != 4u)
                    tg++;
                else
                    tr++;
            }
        cv_text(tx, 64, &AF_S, tb, live ? T_DIM : T_LINE);
        cv_rrect(tx + 16, 64, 14, 10, 2,
                 tg ? T_ACCENT : tr ? T_THEME : live && !trk[i].p[P_MUTE] ? T_RAISE : T_LINE, T_SURF);
        if (tr)
            cv_rect(tx + 16, 75, (int32_t)(tr > 14u ? 14u : tr), 1, T_MID);
    }

    /* Row 6: the sequencer's real position (seq.c seq_src: the step the selected part plays now) */
    cv_text(16, 78, &AF_S, "SEQ", T_MID);
    slen = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    step = seq_src(TSEL, TSEL->seq_idx);
    {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        rowb_str(&r, song.playing ? "PLAY " : "STOP ");
        rowb_int(&r, (int32_t)(step < slen ? step + 1u : 0u));
        rowb_str(&r, "/");
        rowb_int(&r, (int32_t)slen);
        rowb_str(&r, "  ");
        rowb_int(&r, bar_n + (song.playing ? 1u : 0u));   /* (the bar it is on now) */
        rowb_str(&r, " BAR");
    }
    cv_text_r(224, 78, &AF_S, b, song.playing ? T_ACCENT : T_TEXT, T_SURF);

    /* Row 7: the three FX returns (fx.c fx_vu[3..5], the buses' measured peaks) and the blocks that missed
     * their deadline with the voices shed for them. A bus that has never returned anything is a dash, not a 0. */
    cv_text(16, 92, &AF_S, "FX RET", T_MID);
    {
        rowb_t r;
        rowb_init(&r, b, sizeof b);
        for (i = 0; i < 3u; i++) {                  /* chorus, delay, reverb: what each bus returns */
            int32_t v = clamp((int32_t)fx_vu[3 + i], 0, 32767);
            rowb_str(&r, i ? " " : "");
            if (!v)
                rowb_str(&r, "-");
            else
                rowb_int(&r, v * 99 / 32767);       /* 0..99: the return's own peak */
        }
        rowb_str(&r, "  LATE ");
        rowb_int(&r, (int32_t)load_diag.late);
        rowb_str(&r, "/");
        rowb_int(&r, (int32_t)load_diag.shed);
    }
    cv_text_r(224, 92, &AF_S, b, load_diag.late || load_diag.shed ? T_ACCENT : T_TEXT, T_SURF);
}

/* (FASE 8) MACROS page: the project's four macros (mod.c macro_v) -- the matrix's sources M1..M4 -- as a value
 * bar each, with the destinations the SELECTED track's slots drive with them: the matrix connection, shown where
 * the macro is edited. KNOB 1..4 set M1..M4 (params.c G_MAC1.., ui_input.c). A macro no slot reads says so. */
/* (1.0) MACROS: the page now speaks the cards' language. Every macro is a plate with its own ramp, the M1..M4
 * in a pill (as a card's K), the value in the 15 px face, the bar a rounded gauge in the palette's heat colour,
 * the destinations as chips -- and, while its knob is being turned, the row's frame takes the accent and a pip
 * travels the gauge: the same "this is the one you touched" the cards use, and only while hot (the UI redraws
 * every frame then, so the animation costs nothing when the page is still). */
static __attribute__((noinline)) void graph_macros(uint16_t c)
{
    uint32_t i, k, tr = (uint32_t)song.sel;
    const char *L[4] = {"M1", "M2", "M3", "M4"};
    for (i = 0; i < 4u; i++) {
        int32_t y = 2 + (int32_t)i * 29, v = (int32_t)macro_get(i), used = 0;
        int32_t hot = ui.hot_t && ui.hot_col == i, gw = 78, gx = 78;   /* (clear of a three-digit value) */
        const char *r[2] = {0, 0};
        char vb[8];
        for (k = 0; k < NMSLOT && used < 2; k++) {      /* what this track drives with the macro */
            if ((uint32_t)mslot(&trk[tr], k, 0) != MS_M1 + i || !mslot(&trk[tr], k, 1))
                continue;
            r[used++] = mod_dst_name(&trk[tr], mslot(&trk[tr], k, 1));
        }
        cv_rrect(4, y, 232, 27, 3, T_SURF, T_BG);       /* the row's plate (its own ramp); touched, its pill and
                                                          * its gauge take the accent (hot, below) */
        cv_rrect(10, y + 7, 25, 13, 3, used ? (hot ? T_ACCENT : T_KEY) : T_RAISE, T_SURF);
        cv_text_in(10, y + 7 + CAP_IN(S, 13), 25, &AF_S, L[i], used ? T_INK : T_MID,
                   used ? (hot ? T_ACCENT : T_KEY) : T_RAISE);
        fmt_int(vb, v);
        cv_text_on(41, y + 5, &AF_M, vb, used ? c : T_DIM, T_SURF);       /* the value, in the cards' face */
        cv_rrect(gx, y + 10, gw, 7, 1, T_GRID, T_SURF);                  /* the gauge: a track and a fill */
        if (v) {
            int32_t fw = v * gw / 127;
            cv_rrect(gx, y + 10, fw < 3 ? 3 : fw, 7, 1,
                     hot ? T_ACCENT : used ? (ux.chamfer ? heat_col(v * 1000 / 127 * 256 / 1000) : T_THEME) : T_MID,
                     T_SURF);   /* a card's gauge colour: heat on the chamfered palettes, the theme on the others */
        }
        if (!used)
            cv_text_on(162, y + 8, &AF_S, "NO ROUTES", T_DIM, T_SURF);
        for (k = 0; k < (uint32_t)used && k < 2u; k++) {                  /* the destinations, as chips */
            int32_t w = text_w(&AF_S, r[k]) + 8, cx = 162 + (int32_t)k * 36;
            if (cx + w > 232)
                break;
            cv_rrect(cx, y + 7, w, 13, 3, hot ? T_RAISE : T_KEY, T_SURF);
            cv_text_on(cx + 4, y + 8, &AF_S, r[k], T_INK, hot ? T_RAISE : T_KEY);
        }
    }
}

/* (v1.8.5) QUANTUM MORPH (LFO > SCENES): the rail from A to B, the share walked filled, the position as a
 * marker and a percentage, and what each end holds -- its scene number and whether it was captured (an empty
 * end draws no fill: the pair is not armed and turning MORPH would do nothing). Read only: the four knobs under
 * the panel are the page's, and the fill's colour follows HEAT when the palette is chamfered, as the MACROS
 * gauges do. Cheap by construction: a handful of rectangles and two numbers a frame, no per-id loop. */
static __attribute__((noinline)) void graph_scenes(void)
{
    int32_t x0 = PANEL_X0 + 4, x1 = PANEL_X0 + PANEL_W - 4, w = x1 - x0;
    int32_t pos = clamp(msc_pos, 0, 127), mx = x0 + w * pos / 127, k;
    uint32_t hv[2];
    char b[8];
    hv[0] = (msc_have >> msc_a) & 1u;
    hv[1] = (msc_have >> msc_b) & 1u;
    cv_text_on(PANEL_X0, 1, &AF_S, "QUANTUM MORPH", msc_on ? T_MID : T_DIM, T_SURF);
    fmt_int(b, pos * 100 / 127);
    cv_text_r(x1, 1, &AF_S, "%", msc_on ? T_MID : T_DIM, T_SURF);
    cv_text_r(x1 - text_w(&AF_S, "%") - 2, 1, &AF_S, b, msc_on ? T_MID : T_DIM, T_SURF);
    cv_rrect(x0, 26, w, 14, 3, T_GRID, T_SURF);                       /* the rail */
    if (msc_on && pos > 0) {
        int32_t fw = mx - x0;
        cv_rrect(x0, 26, fw < 3 ? 3 : fw, 14, 3,
                 ux.chamfer ? heat_col(90u + (uint32_t)pos) : T_THEME, T_SURF);
    }
    for (k = 0; k < 2; k++) {                                         /* the two stops, their scene numbers */
        int32_t sx = k ? x1 - 13 : x0, c = (int32_t)hv[k];
        fmt_int(b, (int32_t)(k ? msc_b : msc_a) + 1);
        cv_rrect(sx, 21, 13, 24, 2, c ? (k ? T_ACCENT : T_THEME) : T_RAISE, T_SURF);
    }
    cv_rect(mx - 1, 18, 3, 30, msc_on ? T_ACCENT : T_DIM);            /* the position */
    for (k = 0; k < 2; k++) {                                         /* what each end holds */
        int32_t y = 54 + k * 24, c = (int32_t)hv[k];
        cv_rrect(PANEL_X0, y, 232, 20, 3, T_SURF, T_BG);
        cv_text_on(PANEL_X0 + 6, y + 4, &AF_M, k ? "B" : "A", c ? T_MID : T_DIM, T_SURF);
        fmt_int(b, (int32_t)(k ? msc_b : msc_a) + 1);
        cv_text_on(PANEL_X0 + 26, y + 5, &AF_S, "SCENE", T_DIM, T_SURF);
        cv_text_on(PANEL_X0 + 82, y + 4, &AF_M, b, c ? T_MID : T_DIM, T_SURF);
        cv_text_on(PANEL_X0 + 100, y + 5, &AF_S, c ? "STORED" : "KNOB 4: STORE", c ? T_KEY : T_DIM, T_SURF);
    }
    /* the footline: what a pair does, and the controllers that move it -- the page documents itself */
    cv_text_on(PANEL_X0, 101, &AF_S, msc_on ? "CC 78: MORPH   CC 79/80: STORE" : "STORE BOTH ENDS TO MORPH",
               msc_on ? T_DIM : T_MID, T_SURF);
}

/* (FASE 8) MOD page: the matrix's eight slots as rows "1 LFO > CUT +50" and, on the right, the value the slot's
 * source has now -- a bar from the middle for a bipolar source (LFO, KEY, RAND, NOISE: its side is the polarity of
 * the moment) and from the left for the others. The amount's own sign (param_format, F_BIPCT) is the assignment's
 * polarity, always shown. The slot KNOB 2..4 edit is selected; a slot that does nothing (SRC, DST or AMT at 0) is
 * dim and has no bar.
 * MOD MAP is the same page in map mode: the matrix at a glance, as the editor's map reads it -- only the assigned
 * slots, drawn by the same row, and a destination two or more of them drive carries its count in a chip: the
 * shared destinations, where one value is moved twice. Read only and with no bars: MOD is where the slots are
 * edited. */
static __attribute__((noinline)) void graph_mod(const track_t *t, uint16_t c, int map)
{
    uint32_t k, j, i = 0, n;
    for (k = 0; k < NMSLOT; k++) {
        int32_t s = mslot(t, k, 0), d = mslot(t, k, 1), a = mslot(t, k, 2);
        int32_t on = s && d && a, sel = !map && k == mod_ui_slot, y;
        int32_t hot = sel && ui.hot_t;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM;
        char b[10], nb[3];
        const char *unit;
        if (map && !on)
            continue;                                                    /* a slot that does nothing is not a route */
        y = 2 + (int32_t)(map ? i : k) * 15;
        cv_rrect(4, y, 232, 13, 3, sel ? T_THEME : T_SURF, T_BG);         /* every slot its own plate */
        if (!sel && !map)                                                 /* a tab, so a slot reads as one; inside
                                                                           * the plate's edge, clear of the digit */
            cv_rect(7, y + 3, 2, 7, on ? T_THEME : T_DIM);
        nb[0] = (char)('1' + k);
        nb[1] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V, "mod row text centred up/down");
        cv_text_on(13, y - 1, &AF_S, nb, sel ? T_INK : on ? T_THEME : T_MID, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V, "mod row text centred up/down");
        cv_text_on(22, y - 1, &AF_S, N_MSRC[clamp(s, 0, MS_N - 1)], col, bg);
        cv_line(58, y + 6, 64, y + 6, col);              /* an arrow */
        cv_line(61, y + 3, 64, y + 6, col);
        cv_line(61, y + 9, 64, y + 6, col);
        cv_text_on(70, y - 1, &AF_S, mod_dst_name(t, d), col, bg);
        param_format(&TP[P_M1AMT], a, b, &unit);         /* F_BIPCT: "+50" / "-50", the polarity */
        cv_text_r(152, y - 1, &AF_S, b, on ? (sel ? T_INK : a > 0 ? T_ACCENT : T_MID) : T_DIM, bg);
        if (map) {                                       /* shared: how many routes reach this destination */
            for (n = 0, j = 0; j < NMSLOT; j++)
                n += mslot(t, j, 0) && mslot(t, j, 1) && mslot(t, j, 2) && mslot(t, j, 1) == d;
            if (n > 1u) {
                b[0] = 'x';
                b[1] = (char)('0' + n);
                b[2] = 0;
                cv_rrect(196, y, 24, 13, 3, T_KEY, T_SURF);
                cv_text_on(200, y - 1, &AF_S, b, T_INK, T_KEY);
            }
            i++;
        } else if (on) {                                 /* (v1.8.0) WHAT THE ROUTE APPLIES, not the source alone:
                                                          * the bar is source x depth, signed, so a route with AMT at
                                                          * -50 moves left for the same source a +50 one moves right
                                                          * with, and a shallow route draws a short bar. The centre
                                                          * is "no modulation" for every source, bipolar or not: a
                                                          * unipolar source at AMT -100 must not read as "nothing"
                                                          * because it cannot go below zero. */
            int32_t v = mod_contrib(mod_src_live(t, k), a), x0 = 160, x1 = 230, yc = y + 6;
            int32_t xm = (x0 + x1) / 2, px = xm + v * (xm - x0) / 32767;
            cv_rrect(x0, yc, x1 - x0, 1, 0, sel ? T_INK : T_GRID, bg);
            cv_rect(xm, y + 4, 1, 5, sel ? T_INK : T_GRID);          /* the zero, so the sign reads */
            if (v) {
                cv_rrect(px < xm ? px : xm, yc, px > xm ? px - xm : xm - px, 1, 0,
                         sel ? T_INK : v > 0 ? T_ACCENT : T_MID, bg);
                cv_rect(px, y + 3, 1, 7, hot ? T_TEXT : sel ? T_INK : v > 0 ? T_ACCENT : T_MID);   /* the pip */
            }
        }
        (void)unit;
    }
    if (map) {                                       /* (ETAPA 6) where the routes land: the destination each
                                                      * assigned slot drives, with its slots' amounts added,
                                                      * as a bipolar bar under the rows -- the map's other half,
                                                      * the one the chip only counted. Read only. */
        uint32_t d, r = 0, y = 2 + i * 15 + 6, used = 0, lim = (uint32_t)(graph_ht > 40 ? graph_ht - 14 : 0);
        uint32_t ndst = (uint32_t)(sizeof N_MDST / sizeof N_MDST[0]);   /* a count: N_MDST alone is the
                                                                         * array, and -w never complains */
        for (d = 1; d < ndst && used < 4u; d++) {
            int32_t sum = 0, k2;
            char nm[12];
            for (k2 = 0; k2 < NMSLOT; k2++)
                if (mslot(t, k2, 0) && mslot(t, k2, 1) == (int32_t)d && mslot(t, k2, 2))
                    sum += mslot(t, k2, 2);
            if (!sum)
                continue;
            char vb[6];
            if ((uint32_t)y + 13u > lim)                 /* the card's end: never past it */
                break;
            used++;
            cv_rrect(4, y, 232, 13, 3, T_SURF, T_BG);
            str_cpy(nm, N_MDST[d], sizeof nm);
            cv_text_on(12, y - 1, &AF_S, nm, c, T_SURF);
            {   /* the bar: the track from the middle, the amount -252 .. 252 as the slots add up */
                int32_t xm = 84, x1 = 192, px = xm + clamp(sum, -252, 252) * (x1 - xm) / 252;
                cv_rect(xm - 30, y + 4, x1 - xm + 60, 5, T_RAISE);
                cv_rect(px < xm ? px : xm, y + 4, px > xm ? px - xm : xm - px, 5, sum > 0 ? T_ACCENT : T_MID);
                cv_rect(xm, y + 3, 1, 7, T_TEXT);        /* the zero */
            }
            vb[0] = sum < 0 ? '-' : '+';
            fmt_int(vb + 1, sum < 0 ? -sum : sum);
            cv_text_r(230, y - 1, &AF_S, vb, sum > 0 ? T_ACCENT : sum < 0 ? T_MID : T_DIM, T_SURF);
            y += 15;
            r++;
        }
        if (!used)                                   /* nothing assigned: the map says so, in the band */
            cv_text_on(12, 2 + i * 15 + 8, &AF_S, "NO ROUTES", T_DIM, T_SURF);
        (void)r;
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}



/* (3.4) LOFI's EDIT pages: the chip's own panel. The two pages used to fall back to the generic being (WAVE 1 /
 * WAVE 2 / MIXER), which says nothing about this engine. What is drawn here is its identity, from the
 * parameters the DSP reads and from nothing else: the trace the track is producing, the converter and the
 * waveform it is set to (their names come from the engine's own EDIT descriptors, so a new waveform needs no
 * change here), the pulse width on its period, the crusher's staircase, and a T.CLK timeline that shows what
 * NOTE and FREE do -- a note restarts the formula, or the formula keeps running and the note joins it where it
 * is (eng_lofi.c lofi_note_on, voice.c's retrigger). */
static __attribute__((noinline)) void graph_lofi(const track_t *t, uint16_t c)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    int32_t chip = clamp(t->p[P_E0], 0, 4), wv = clamp(t->p[P_E1], 0, 6);
    int32_t duty = clamp(t->p[P_E2], 0, 127), crush = clamp(t->p[P_E3], 0, 127);
    int32_t fr = clamp(t->p[P_LRTRG], 0, 1), i, x, px = 18, py = 102;

    gw_oct_panel(8, 2, 224, 68, 4, T_BG, T_LINE, 1, c);
    cv_text_on(14, 4, &AF_S, "LOFI CHIP", T_THEME, T_BG);
    if (e->edit[0].names && e->edit[0].names[chip])
        cv_text_r(170, 4, &AF_S, e->edit[0].names[chip], T_ACCENT, T_BG);
    if (e->edit[1].names && e->edit[1].names[wv])
        cv_text_r(228, 4, &AF_S, e->edit[1].names[wv], T_MID, T_BG);
    cv_rect(12, 20, 216, 1, T_LINE);
    cv_rect(12, 33, 216, 1, T_GRID);
    gw_live_trace(14, 212, 33, 12, 148);             /* the master trace: the sound this track is making */
    cv_rect(12, 49, 216, 1, T_LINE);

    /* the pulse width, on its own period */
    cv_rect(14, 53, 96, 7, T_RAISE);
    if (duty)
        cv_rect(14, 53, duty * 96 / 127, 7, c);
    /* the converter's staircase: five steps, lit as far as CRUSH keeps them */
    for (i = 0; i < 5; i++) {
        int32_t bh = 2 + i * 2;
        cv_rect(150 + i * 9, 60 - bh, 7, bh, crush * 5 / 128 > i ? c : T_RAISE);
    }

    gw_oct_panel(8, 74, 224, 44, 4, T_BG, T_LINE, fr, T_ACCENT);
    cv_text_on(14, 76, &AF_S, "T.CLK", T_THEME, T_BG);
    cv_text_r(226, 76, &AF_S, fr ? "FREE" : "NOTE", fr ? T_ACCENT : T_MID, T_BG);
    /* five notes across the timeline, at the spacing the formula's period has */
    for (i = 0; i < 5; i++)
        cv_rect(18 + (int32_t)i * 43, 91, 1, 11, T_LINE);
    for (x = 18; x <= 230; x++) {                     /* NOTE: the phase restarts at each note; FREE: runs on */
        int32_t y = fr ? 102 - (x - 18) * 11 / 212 : 102 - ((x - 18) % 43) * 11 / 43;
        if (x > 18)
            cv_line(px, py, x, y, fr ? T_ACCENT : c);
        px = x;
        py = y;
    }
    cv_text_on(14, 103, &AF_S, fr ? "runs on, note joins" : "each note restarts", T_DIM, T_BG);
}

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
/* (3.6) PHASE's own screen. Its EDIT 1 and EDIT 2 pages fell through to the generic being, so the one
 * engine whose whole character IS a bent phase had no picture of it. This is that picture and nothing
 * invented: a phase ramp (the phase the oscillator walks, x) bent at the knee DCW puts it, drawn from the
 * track's own P_E0..P_E7 -- the values eng_phase.c plays -- plus the figures the page does not carry.
 * Moving DCW moves the knee and moving the page's WAVE moves what the header says, because those are the
 * parameters; no state is kept here and no audio is read. */
static __attribute__((noinline)) void graph_phase(const track_t *t, uint16_t c)
{
    int32_t w = (int32_t)(t->p[P_E0] & 7), w2 = clamp(t->p[P_E1], 0, 8);
    int32_t dcw = clamp(t->p[P_E2], 0, 127), env = clamp(t->p[P_E3], 0, 127);
    int32_t dtn = clamp(t->p[P_E4], 0, 127), line = t->p[P_E5] & 1;
    int32_t sub = clamp(t->p[P_E6], 0, 127), fb = clamp(t->p[P_E7], 0, 127);
    int32_t x0 = PANEL_X0 + 2, x1 = 228, y0 = 26, y1 = 66;
    int32_t knee = x0 + (x1 - x0) * (96 + dcw) / 224;   /* DCW 0: the knee mid, 127: at the right */
    char b[40], h[40];

    cv_text(PANEL_X0 + 2, 3, &AF_S, "PHASE", T_TEXT);   /* (the right header carries the rest) */
    fmt_int(b, w);
    str_cpy(h, "WAVE ", sizeof h);
    str_cpy(h + str_len(h), b, sizeof h - str_len(h));
    fmt_int(b, w2);
    str_cpy(h + str_len(h), line ? "  RING  W2 " : "  MIX  W2 ", sizeof h - str_len(h));
    str_cpy(h + str_len(h), b, sizeof h - str_len(h));
    fmt_int(b, dtn);
    str_cpy(h + str_len(h), "  DT ", sizeof h - str_len(h));
    str_cpy(h + str_len(h), b, sizeof h - str_len(h));
    fmt_int(b, dcw);
    str_cpy(h + str_len(h), "  DCW ", sizeof h - str_len(h));
    str_cpy(h + str_len(h), b, sizeof h - str_len(h));
    cv_text_r(228, 3, &AF_S, h, T_MID, T_SURF);

    cv_rect(x0, y1, x1 - x0, 1, T_RAISE);                /* the axes */
    cv_rect(x0, y0, 1, y1 - y0 + 1, T_RAISE);
    cv_line_t(x0, y1, knee, y0, c, 2);                   /* the phase, bent at the knee */
    cv_line_t(knee, y0, x1, y1, ux_mix(c, T_MID, 80), 2);
    cv_rect(knee - 1, y0 - 4, 3, 3, T_ACCENT);           /* DCW's knee */

    {   /* the figures the page has no column for: ENV, DT, SUB, FB (EDIT 1 and EDIT 2 together) */
        static const char *const NM[4] = {"ENV", "DT ", "SUB", "FB "};
        int32_t v[4], k;
        v[0] = env; v[1] = dtn; v[2] = sub; v[3] = fb;
        for (k = 0; k < 4; k++) {
            int32_t yy = 68 + k * 10;
            cv_text(PANEL_X0 + 2, yy, &AF_S, NM[k], T_DIM);
            cv_rect(PANEL_X0 + 40, yy + 3, 110, 3, T_RAISE);
            if (v[k] > 0)
                cv_rect(PANEL_X0 + 40, yy + 3, v[k] * 110 / 127, 3, c);
            fmt_int(b, v[k] * 100 / 127);                /* percent, as the page's columns read */
            cv_text_r(228, yy, &AF_S, b, v[k] ? T_TEXT : T_DIM, T_SURF);
        }
    }
    if (w2 > 0) {                                       /* W2 alternates with WAVE: mark it on the ramp */
        int32_t x = x0 + (x1 - x0) * w2 / 9;
        cv_rect(x - 1, y0 - 2, 3, 3, ux_mix(c, T_ACCENT, 120));
        cv_text(PANEL_X0 + 2, 112, &AF_S, "W2", T_DIM);
        fmt_int(b, w2);
        cv_text(PANEL_X0 + 24, 112, &AF_S, b, T_TEXT);
        cv_text(PANEL_X0 + 40, 112, &AF_S, line == 1 ? "RING" : line == 2 ? "PM" : "MIX", T_DIM);
    }
}

/* (1.2.6) ADDITIVE's EDIT pages: the eight partials as bars, the spectrum the engine is playing, in the
 * same language as WHEEL's drawbars. Each bar is one harmonic, its height the gain add_block normalised
 * (a partial the profile leaves out shows as a stub), and the whole row lights in the accent while the
 * knob that moves the spectrum is being turned. */
static __attribute__((noinline)) void graph_additive(const track_t *t, uint16_t c)
{
    static const char *const names[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    uint32_t k;
    int hot = ui.hot_t && (cur_page()->id[0] == P_E0 || cur_page()->id[0] == P_E1);
    for (k = 0; k < 8u; k++) {
        int32_t x = 10 + (int32_t)k * 28, h = ad_partial(t, k) * 64 / 32767;
        cv_rrect(x + 9, 4, 4, 72, 2, T_RAISE, T_SURF);
        if (h)
            cv_rrect(x + 4, 76 - h, 14, h, 3, hot ? T_ACCENT : c, T_SURF);
        else
            cv_rrect(x + 4, 74, 14, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        GFX_HOOK_ALIGN(x + 4, 0, x + 18, 0, AL_H, "additive partial under its bar");
        cv_text_in(x + 4, 82, 14, &AF_S, names[k], T_MID, T_SURF);
    }
}
static __attribute__((noinline)) void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level = drw_level(t->p, k);
        if (t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        int hot = ui.hot_t && cur_page()->id[0] == P_E0 &&
                  (ui.hot_col == 0u || ui.hot_col == (k < 2u ? 1u : k < 4u ? 2u : 3u));
        cv_rrect(x + 7, 2, 4, 73, 2, T_RAISE, T_SURF);
        if (level) cv_rrect(x + 3, 75 - level * 8, 12, level * 8, 3, hot ? T_ACCENT : c, T_SURF);
        else cv_rrect(x + 3, 73, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        GFX_HOOK_ALIGN(x + 7, 0, x + 11, 0, AL_H, "wheel footage under its drawbar");
        cv_text_in(x + 3, 82, 12, &AF_S, names[k], T_MID, T_SURF);   /* under the bar, by its ink */
    }
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers. */
static __attribute__((noinline)) void graph_fm6(const track_t *t, uint16_t c, uint32_t sel)   /* sel < 6: the operator picked (ui_fm6op.c) */
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[11] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (!t->p[P_E0])                                 /* (0.6.46) ALG on PAT: the patch's number, and that it is the patch's */
        str_cpy(b + 6, " PAT", 5);
    if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 9u; k++)                          /* (1.7) the chart's reticle: a 16-bit grid, 24 px steps */
        cv_rect(24 + (int32_t)k * 23, 20, 1, 82, T_GRID);
    cv_rect(PANEL_X0, 20, PANEL_W, 1, T_LINE);

    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = k == sel ? T_ACCENT : fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        gw_bevel(x[k] - 10, y[k], 21, FM_BH, fill, ux_mix(fill, T_TEXT, 40), ux_mix(fill, T_BG, 42));
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, n, fill_c || k == sel ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    if (sel < 6u) {                                  /* the operator picked: "OP3 x1.50" */
        char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
        fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
        cv_text_r(228, 4 - GOY, &AF_S, fq, T_TEXT, T_SURF);
    } else if (fm6_bank_slot(fm6_slot[(t - trk) % NTRK]) >= 0) {   /* (0.6.11) SLOT B1..B32: the bank voice's name */
        char nm[11];
        for (k = 0; k < 10u; k++)
            nm[k] = (char)pt[FP_NAME + k];
        for (nm[10] = 0; k && nm[k - 1] == ' '; k--)
            nm[k - 1] = 0;
        cv_text_r(228, 4 - GOY, &AF_S, nm, T_TEXT, T_SURF);
    }
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

/* OP ENV (ui_fm6op.c): the picked operator's envelope as the patch has it: from L4 to L1 at R1, L2 at R2, L3 at R3, held
 * there while the key is down (a RAISE mark where it is let go), back to L4 at R4. A stage's width grows as its rate
 * falls (4..48 px: 99 is a step, 0 the longest), the levels 0..99 linearly (dB). The curve THEME, the stage KNOB 3 / 4
 * edit (STAGE) ACCENT with a dot at its target; "OP3" top left, the stage's rate and level top right */
/* (v2.3.5) THE OPERATOR'S ENVELOPE, IN THE SAME DEPTH. OP ENV was the last EDIT page that draws a CURVE and had no
 * stack: five segments, one 2 px polyline, flat. The same six points are drawn four times now -- each layer flatter
 * about the baseline, lifted, and darker -- the geometry and the ramp MORPH, TILT and the filter response use, so the
 * page reads as one instrument with the rest. The stage picked stays on the front line (the accent), where it was. */
static void fm6env_pass(const int32_t *x, const int32_t *y, int32_t bot, int32_t sy, int32_t dy, int32_t th, uint16_t ink)
{
    int32_t k;
    for (k = 0; k < 5; k++) {
        int32_t ya = bot + dy - (bot - y[k]) * sy / 1000;
        int32_t yb = bot + dy - (bot - y[k + 1]) * sy / 1000;
        cv_line_t(x[k], ya, x[k + 1], yb, ink, th);
    }
}
static __attribute__((noinline)) void graph_fm6env(const track_t *t, uint16_t c)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    int32_t top = 24, bot = 88, x[6], y[6], k;
    uint32_t sg = fop_stg & 3u;
    char b[24] = {'O', 'P', (char)('1' + fop_op % 6u), 0};
#define FEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define FEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    x[0] = 12;
    y[0] = FEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + FEGW(op[FP_R1 + k]);
        y[k + 1] = FEGY(op[FP_L1 + k]);
    }
    x[5] = 228;
    x[4] = x[5] - FEGW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    for (k = 1; k < 4; k++)                          /* (1.7) the level guides, a quarter of the scale each */
        cv_rect(PANEL_X0, top + k * (bot - top) / 4, PANEL_W, 1, T_GRID);
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_rect(x[4], top, 1, bot - top + 1, T_RAISE);   /* the key let go */
    {
        int32_t sl;
        for (sl = 3; sl >= 1; sl--)
            fm6env_pass(x, y, bot, 1000 - sl * 80, -sl * 4, 1, qz_back_ink(c, 1000 - sl * 80));
        for (k = 0; k < 5; k++)                      /* (3: the hold, no stage of its own) */
            cv_line_t(x[k], y[k], x[k + 1], y[k + 1], k != 3 && (uint32_t)(k < 3 ? k : 3) == sg ? T_ACCENT : c, 2);
    }
    k = sg < 3u ? (int32_t)sg + 1 : 5;
    fm_dot(x[k], y[k], T_ACCENT);
#undef FEGY
#undef FEGW
    cv_text(10, 4 - GOY, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4 - GOY, &AF_S, b, T_TEXT, T_SURF);
}

#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static __attribute__((noinline)) void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave, PATTERN's steps,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_STEPS, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_STEPS ? SK_STEPS : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] == P_E0 ? 0u : pg->id[0] == P_X0 ? 2u : 1u] : 0;   /* EDIT: the engine's */
    if (pg->fam == FAM_FX && !page_over) {              /* (0.6.24) FX: the page within its section */
        uint32_t h = ui.page;
        while (h > 0u && !fx_head(h))
            h--;
        for (i = h; i < NPAGES && PAGES[i].fam == FAM_FX && (i == h || !fx_head(i)); i++)
            if (page_visible(i)) {
                n++;
                if (i == ui.page)
                    k = n;
            }
        pt = str_eq(PAGES[h].title, "FX") ? pg->title : PAGES[h].title;   /* (the section's name: "DLY 2/2", "SLICER
                                                         * 4/4"; 0.6.81: TRACK's its pages' own, "AUTO PAN 3/3" was
                                                         * "FX 3/3") */
    } else
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            n++;
            if (i == ui.page)
                k = n;
        }
    str_cpy(ti, pt ? pt : grid_on() ? "GRID" : pg->title, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t)) {  /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
/* the thermal ramp (GAMMA / WRAITH / JIANT): a value 0..256 as heat:
 * - GAMMA: Cherenkov blue -> violet -> furnace crimson -> Bakelite orange -> amber-gold -> yellow -> core white
 * - WRAITH: Void indigo -> ultraviolet -> specimen crimson -> rose plasma -> peach -> spectral mint -> core white
 * - JIANT: cyan (cold) .. blue .. violet .. red .. orange .. yellow .. white */
static uint16_t heat_col(int32_t v)
{
    static const uint16_t H[7]  = {0x069Fu, 0x59FFu, 0xB19Cu, 0xF146u, 0xFBC0u, 0xFEA0u, 0xFFFFu};
    static const uint16_t HG[7] = {0x131Au, 0x4116u, 0xA8E4u, 0xEA20u, 0xFC80u, 0xFFE6u, 0xFFFFu};
    static const uint16_t HW[7] = {0x2092u, 0x611Au, 0xB8EEu, 0xF9CAu, 0xFD94u, 0x7FEFu, 0xFFFFu};
    static const uint16_t HQ[7] = {0x1047u, 0x2ADBu, 0x06FFu, 0xD959u, 0xFCA2u, 0xFF51u, 0xFFFFu};   /* QUANTUM: vacuum indigo, blue, cyan, magenta, amber, gold, white */
    static const uint16_t HS[7] = {0x2082u, 0x7940u, 0xC260u, 0xE980u, 0xFD60u, 0xFFE0u, 0xFFFFu};   /* SOLARIS: deep forge ember, warm copper, flame orange, sun gold, white */
    static const uint16_t HC[7] = {0x1848u, 0x03FFu, 0x067Fu, 0xB8BFu, 0xF972u, 0xFF7Du, 0xFFFFu};   /* CYBERPUNK: neon dark blue, electric cyan, laser blue, violet, neon pink, white */
    static const uint16_t HN[7] = {0x08A6u, 0x0BF3u, 0x1E59u, 0x47B0u, 0x8772u, 0xFEEAu, 0xFFFFu};   /* NORDIC: polar deep teal, arctic ice, aurora green, mint, pale gold, white */
    /* (FASE 2/5) THE SKINS' RAMPS. A skin that only recoloured the chrome left every curve, meter and planet
     * on the previous palette's ramp (they all fell through to H, the thermal one), so NEBULA read as
     * quantum-with-a-violet-border and MINIMAL still had orange physics. Each skin carries its own ramp now:
     * cold at 0, white at 256, the same seven points the other palettes use, so nothing that reads heat had
     * to change. */
    static const uint16_t HNB[7] = {0x0845u, 0x294Fu, 0x5299u, 0x8B3Eu, 0xCBDCu, 0xFD5Bu, 0xFFFFu};  /* NEBULA: deep indigo, blue, violet, orchid, lilac, white */
    static const uint16_t HPP[7] = {0x1802u, 0x5806u, 0xA84Au, 0xE14Fu, 0xFB67u, 0xFDEBu, 0xFFFDu};  /* PLASMA: black cherry, wine, magenta, hot pink, coral, cream */
    static const uint16_t HOG[7] = {0x00A1u, 0x11E3u, 0x2B66u, 0x5D48u, 0xAE8Au, 0xF72Fu, 0xFFFEu};  /* ORGANIC: moss, fern, leaf, lime, honey, white */
    static const uint16_t HMN[7] = {0x10A2u, 0x39E7u, 0x6B6Eu, 0xA514u, 0xCE7Au, 0xEF7Eu, 0xFFFFu};  /* MINIMAL: graphite to paper */
    static const uint16_t HRT[7] = {0x1060u, 0x4121u, 0x8A21u, 0xD3C2u, 0xFDA6u, 0xFF12u, 0xFFFEu};  /* RETRO: ember, amber, phosphor gold */
    const uint16_t *R = settings.palette == UI_GAMMA_INDEX ? HG :
                        settings.palette == UI_WRAITH_INDEX ? HW :
                        settings.palette == UI_QUANTUM_INDEX ? HQ :
                        settings.palette == UI_SOLARIS_INDEX ? HS :
                        settings.palette == UI_CYBER_INDEX ? HC :
                        settings.palette == UI_NORDIC_INDEX ? HN :
                        settings.palette == UI_NEBULA_INDEX ? HNB :
                        settings.palette == UI_PLASMA_INDEX ? HPP :
                        settings.palette == UI_ORGANIC_INDEX ? HOG :
                        settings.palette == UI_MINIMAL_INDEX ? HMN :
                        settings.palette == UI_RETRO_INDEX ? HRT :
                        settings.palette == UI_JIANT_INDEX ? H : 0;   /* (3.5) THERMAL keeps the camera's own ramp:
                                                                       * a thermal page whose physics came from its
                                                                       * five chrome tokens read as black on black */
    /* (3.5) EVERY THEME HAS ITS OWN RAMP. A palette with no hand-tuned table above (GREY, GREEN, ROSE, PAPER,
     * HI-CON, NIGHT, MONO, THERMAL) fell through to H, the thermal one: its curves, meters and planets were the
     * same colours as GAMMA's, which is what made HOME's five worlds look alike whichever theme was chosen. The
     * ramp is now derived from the palette's own tokens -- cold at 0, the theme and the accent in the middle,
     * the text at 256 -- so a theme that only names five colours still gets a full ramp of its own. */
    uint16_t own[7];
    int32_t k;
    if (!R) {
        own[0] = ux_mix(T_BG, T_SURF, 60);
        own[1] = ux_mix(T_BG, T_THEME, 70);
        own[2] = ux_mix(T_THEME, T_ACCENT, 45);
        own[3] = ux_mix(T_THEME, T_ACCENT, 85);
        own[4] = ux_mix(T_ACCENT, T_TEXT, 35);
        own[5] = ux_mix(T_ACCENT, T_TEXT, 72);
        own[6] = T_TEXT;
        R = own;
    }
    v = clamp(v, 0, 256);
    if (!ux.chamfer)
        return v < 128 ? ux_mix(T_SURF, T_ACCENT, v * 100 / 128) : ux_mix(T_ACCENT, T_TEXT, (v - 128) * 100 / 128);
    k = v * 6 / 257;
    return ux_mix(R[k], R[k + 1], (v * 6 - k * 256) * 100 / 256);
}

/* DRUM (DRUM-X): the eight sounds as columns, each as high and as hot as its hit now (a muted one crossed); the MORPH
 * A .. B above. sel: the sound edited (EDIT > SOUND), its name in the theme colour; -1 none */
static __attribute__((noinline)) void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

/* (0.6.52) SEQ > FUGUE: the steps (their notes as heat) with the sequencer's playhead, then a lane each for heads 2..4:
 * where it is, its RATE DIR INTERVAL; the head edited lit */
static __attribute__((noinline)) void graph_fugue(const track_t *t)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, k, ti = song.sel % NTRK;
    int32_t w = PANEL_W / (int32_t)len, x0 = PANEL_X0 + (PANEL_W - w * (int32_t)len) / 2;
    if (w < 2) w = 2;
    for (i = 0; i < len; i++) {
        const step_t *st = &seq_steps(t)[i];
        int on = step_on(st);
        cv_rect(x0 + (int32_t)i * w, on ? 4 : 14, w - (w > 3 ? 1 : 0), on ? 14 : 4, on ? step_heat(st, st->flags & SF_ACCENT, ACC) : T_RAISE);
        if (song.playing && i == seq_src(t, t->seq_idx)) cv_rect(x0 + (int32_t)i * w, 20, w, 2, T_TEXT);
    }
    for (k = 0; k < FG_HEADS; k++) {
        const fg_set_t *f = &fg_set[ti][k];
        const fg_head_t *h = &fg_head[ti][k];
        int32_t y = 28 + (int32_t)k * 23;
        uint16_t c = f->rate ? heat_col(70u + k * 60u) : T_DIM;
        char b[24] = {(char)('2' + k), ' ', 0};
        if (k == fg_ui % FG_HEADS) cv_rrect(PANEL_X0, y - 2, PANEL_W, 22, 3, T_RAISE, T_SURF);
        cv_rect(x0, y + 17, w * (int32_t)len, 1, T_MID);
        if (f->rate && song.playing && h->idx < len)
            cv_rect(x0 + (int32_t)h->idx * w, y + 14, w, 6, c);
        str_cpy(b + 2, N_FGRATE[f->rate % FG_NRATE], sizeof b - 2u);
        if (f->rate) {
            uint32_t n = str_len(b);
            b[n] = ' '; b[n + 1u] = 0;
            str_cpy(b + n + 1u, N_FGDIR[f->dir & 3u], sizeof b - n - 1u);
            n = str_len(b);
            b[n] = ' ';
            b[n + 1u] = f->ivl > 0 ? '+' : 0;
            fmt_int(b + n + 1u + (f->ivl > 0), f->ivl);
        }
        cv_text(PANEL_X0 + 4, y, &AF_S, b, f->rate ? T_TEXT : T_DIM);
    }
}

/* (0.6.51) the page's name big (page_flash). (0.6.61) In the header, not over the cards or the graph (covering them
 * lost the sense of what a turn did): for PF_MS the header shows the page's name (M) and, smaller at the right, the
 * track and its engine; on HOME (a track picked there) the track's engine big and "HOME" small. Then the header comes
 * back (ui.force). (0.6.64) Over the footer instead, which says the same: the header (load, battery) stays */
static void page_flash_draw(void)
{
    static uint32_t drawn;
    uint32_t e = fm1_ms - pf_ms, ph, dim;
    char nm[20], sub[20] = {'T', 0, 0};
    const page_t *pg = cur_page();
    const char *pt = pg->scope == SC_ENGINE ? ENGINES[TSEL->eng_req % NENGINES]->page_title[pg->id[0] == P_E0 ? 0u : pg->id[0] == P_X0 ? 2u : 1u] : 0;
    const char *en = ENGINES[TSEL->eng_req % NENGINES]->name;
    if (e >= PF_MS) {
        pf_on = 0;
        ui.foot_sig ^= 1u;                           /* (0.6.64) the footer, whole again: that alone redrawn */
        return;
    }
    dim = e > PF_MS - 120u;
    ph = (pf_ms << 1 | dim) + 1u;                     /* (a new flash or its dimming; else every 4th frame: cheap) */
    if (!ui.force && ph == drawn && (ui.frame & 3u))
        return;
    drawn = ph;
    sub[1] = (char)('1' + song.sel % NTRK);
    sub[2] = ' ';
    if (ui.home) {                                   /* HOME: the engine big, "T2 HOME" small */
        str_cpy(nm, en, sizeof nm);
        str_cpy(sub + 3, "HOME", sizeof sub - 3u);
    } else {
        str_cpy(nm, pt && pt[0] != '-' ? pt : pg->title, sizeof nm);
        str_cpy(sub + 3, en, sizeof sub - 3u);
    }
    cv_begin(240, H_FOOT, T_BG);                     /* (0.6.64) at the bottom: the footer says the same */
    cv_bg = T_BG;
    cv_text(8, (H_FOOT - (int32_t)AF_M.h) / 2, &AF_M, nm, dim ? T_MID : T_TEXT);
    cv_text_r(232, (H_FOOT - (int32_t)AF_S.h) / 2, &AF_S, sub, dim ? T_DIM : T_ACCENT, T_BG);
    cv_blit(0, Y_FOOT);
}

/* (0.6.61) the panels' cytoplasm: a few granules drifting behind the graph, as HOME's, so the screen breathes. LOAD
 * FULL and ANIM ON only, not on HOME (its own), the lists and the dense grids (the roll, the steps); (0.6.64) no
 * redraw of their own: they move when the panel redraws anyway (a knob, the LFO, the envelope), so they cost nothing */
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 0                                /* (tests/ui_test.c: 1, its frames compared pixel by pixel) */
#endif
static int drift_on(void)
{
    uint32_t g = cur_page()->graph;
    if (UI_NO_DRIFT)
        return 0;
    return !ui.home && !sys_load && !(ui_prefs & PREF_ANIM_OFF) && strip_kind() == SK_NONE && g != GR_ROLL && g != GR_SHAPE &&
           g != GR_STEPS && g != GR_CHANCE && g != GR_BROWSE && g != GR_USER && g != GR_SLOTS && g != GR_EVENTS &&
           g != GR_SONG && g != GR_TOOLS && g != GR_PATS;
}
static __attribute__((noinline)) void graph_drift(void)
{
    uint32_t k, gh = graph_h();
    for (k = 0; k < 14u; k++) {
        uint32_t h = k * 2654435761u + 0x9E37u;
        int32_t gx = 10 + (int32_t)((h >> 8) % 220u + ui.frame * (1u + (h >> 28)) / 8u) % 220;
        int32_t gy = 4 + (int32_t)(((h >> 16) % (gh - 8u)) + (uint32_t)(sine_i((ui.frame * 200u + h) << 16) >> 13)) % (int32_t)(gh - 8u);
        cv_rect(gx, gy, 1 + (int32_t)(h >> 31), 1 + (int32_t)(h >> 31), ux_mix(T_SURF, T_THEME, 18 + (int32_t)(h >> 29) * 3));
    }
}
static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home) {
        /* (3.5) every frame a body answers a touch -- and (7.0) the four knobs' values are part of the
         * signature, so turning KNOB 1 on HOME repaints the bay on the very next frame instead of waiting for
         * something else to change. The bay's cutoff is the card's own parameter (home_param), so the picture
         * and the card can never disagree. */
        uint32_t knobs = 0;
        for (i = 0; i < 4u; i++) {
            int16_t *vp;
            home_param(i, &vp);
            knobs = knobs * 31u + (uint32_t)(uint16_t)*vp;
        }
        return h ^ (ui.frame / 2u) ^ gw_planet_sig() ^ knobs * 2654435761u;
    }
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP || pg->graph == GR_MOTION) h ^= ui.frame / 2u;
    if (pg->graph == GR_FX) {                        /* real bus peaks; the ambient granules move every frame */
        h ^= (uint32_t)(t->peak < 0 ? -t->peak : t->peak) * 2654435761u ^ (uint32_t)bg_energy() * 40503u;
        if (drift_on())                                /* (1.8.6) was ui.frame / 16: the FX granules ran at ~3.75 fps */
            h ^= ui.frame * 1299709u;
        for (i = 0; i < 6u; i++)
            h = (h ^ (uint32_t)fx_vu[i]) * 16777619u;
    }
    if (pg->graph == GR_FX_BUSES) {                  /* (6.1) the console: the buses' real peaks, moving while
                                                      * there is signal and still when there is none */
        for (i = 0; i < 8u; i++)
            h = (h ^ (uint32_t)fx_vu[i]) * 16777619u;
        if (fx_vu[0] | fx_vu[1] | fx_vu[2] | fx_vu[3] | fx_vu[4] | fx_vu[5] | fx_vu[6] | fx_vu[7])
            h ^= ui.frame * 1299709u;
        h ^= (uint32_t)(fxb_sel() + 1) * 40503u;
    }
    if (pg->graph == GR_LFO)                         /* (JIANT 0.5) the LFO runs: its phase, its level */
        h ^= (t->lfo_ph >> 23) * 2654435761u ^ (uint32_t)(t->lfo_val * t->lfo_fade >> 22);
    if (pg->graph == GR_ADSR)                        /* the envelope's dot: every voice's stage and level */
        for (i = 0; i < NVOICE; i++)
            h = (h ^ ((uint32_t)V(t, i).stage << 24 | (uint32_t)V(t, i).env >> 16)) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_MSEQ)                        /* (JIANT) the step picked, the one playing, the level now */
        h ^= (ms_ui_step + 1u) * 40503u + (song.playing ? ui.frame * 2654435761u : 0u);
    if (pg->graph == GR_SONG) {
        for (uint32_t r = 0; r < CHAIN_ROWS; r++)
            h = (h ^ (song_idx.scene[song_cur][r].on * (r + 1u))) * 16777619u;
        h ^= ui.song_row * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u + chain.pass * 15485863u : 0u;
        h ^= SONG_END(&chain_config) * 2246822519u;      /* (0.6.22: the END) */
        for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (24u + i);
        h += graph_pname_sig;
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if ((pg->graph == GR_SLCR || pg->graph == GR_SLPIT) && song.g[G_SLMODE])   /* the SLICER's step playing */
        h ^= (sl.idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLCR || pg->graph == GR_SLPIT)   /* (its globals: the page's knobs and the bus) */
        for (i = 0; i < G_SL_N; i++)
            h = (h ^ (uint32_t)song.g[G_SLMODE + i]) * 16777619u;
    if (pg->graph == GR_SLPIT)
        h ^= (sl_ui_step + 1u) * 40503u;
    if (pg->graph == GR_TOOLS)                       /* (0.6.12) the action picked, the song's row */
        h ^= ui.act * 7919u + ui.song_row * 104729u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot; the project, the music's section, the song) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig + song_cur * 2654435761u + (uint32_t)(live.cur + 1) * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
    }
    if (pg->graph == GR_MOTION) {                    /* (FASE 5/9) the counts, the parameter drawn, the grid, the mode,
                                                      * and the records themselves: the movement is what it shows */
        h ^= motion_count(t) * 131u + motion_enabled(t) + ui.mo_id * 4093u + motion_res_steps(trk_index(t)) * 1021u;
        if (!song.playing) h ^= 0x5EEDu;             /* (the step sounding, while it plays) */
        for (i = 0; i < motion.count; i++)
            h = (h ^ ((uint32_t)motion.event[i].place << 24 | (uint32_t)motion.event[i].param << 16 |
                      (uint16_t)motion.event[i].value)) * 16777619u;
    }
    if (pg->graph == GR_EVENTS) {                    /* AUTO LIST: the rows, the one selected, + ADD LOCK's place */
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx);
        for (i = 0; i < n; i++)
            h = (h ^ ((uint32_t)motion.event[idx[i]].place << 24 | (uint32_t)motion.event[idx[i]].param << 16 |
                      (uint16_t)motion.event[idx[i]].value)) * 16777619u;
        h = (h ^ (n + 1u) * 40503u ^ ev_row(n) * 2654435761u ^ ((uint32_t)ui.ev_step << 8 | ui.ev_id) * 104729u) * 16777619u;
    }
    if (pg->graph == GR_CHANCE) h ^= ui.cursor * 40503u + step_chance(&t->step[ui.cursor]);
    if (pg->graph == GR_FUGUE) {                     /* (0.6.52) the heads, their settings, where they are */
        uint32_t ti = song.sel % NTRK, k;
        h = (h ^ (steps_hash(t) + (song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu) * 31u + fg_ui * 7u)) * 16777619u;
        for (k = 0; k < FG_HEADS; k++)
            h = (h ^ ((uint32_t)fg_set[ti][k].rate | (uint32_t)fg_set[ti][k].dir << 4 | (uint32_t)(uint8_t)fg_set[ti][k].ivl << 8 |
                      (song.playing ? (uint32_t)fg_head[ti][k].idx << 16 : 0u))) * 16777619u;
    }
    if (pg->graph == GR_DXIMG)                       /* (0.6.48) IMAGE and how wide it plays */
        h = (h ^ ((uint32_t)t->p[P_DXIMG] << 16 | (uint32_t)dx_img_k(t))) * 16777619u;
    if (pg->graph == GR_MORPH)                       /* (0.6.45) the wave, MORPH */
        h = (h ^ ((uint32_t)t->p[P_MORPH] << 16 | (uint32_t)t->p[P_E0] << 8 | (uint32_t)t->p[P_E1] |
                  (uint32_t)t->p[P_E2] << 24)) * 16777619u;
    if (pg->graph == GR_MUTATE) {                    /* (0.6.43) the bar, the settings, FREEZE */
        h = (h ^ (song.playing ? bar_n : 0u) * 2654435761u ^ mut_freeze * 977u) * 16777619u;
        for (i = 0; i < NTRK; i++)
            h = (h ^ ((uint32_t)trk[i].p[P_MSND] | (uint32_t)trk[i].p[P_MPAT] << 4 | (uint32_t)trk[i].p[P_MMODE] << 8)) * 16777619u;
    }
    if (pg->graph == GR_COMP) {                      /* (0.6.39) the steps, their components, the arpeggio and the loop */
        const step_t *cs = &t->step[ui.cursor];
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
        h = (h ^ (steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.bank * 613u)) * 16777619u;
        h = (h ^ ((uint32_t)t->p[P_CLEN] << 8 | (song.playing ? t->cpos : 0xFFu))) * 16777619u;   /* (0.6.47) C.LEN */
        for (i = ui.bank * 16u; i < ui.bank * 16u + 16u && i < NSTEP; i++)
            h = (h ^ ((uint32_t)t->step[i].comp << 8 | t->step[i].carg | (uint32_t)t->step[i].probability << 16 |
                      (uint32_t)t->step[i].flags << 24 | (uint32_t)t->step[i].comp2 << 4)) * 16777619u;
        h = (h ^ (comp_view() * 977u + cs->carg + (song.playing ? t->arp_k * 131u + t->loops * 7u + t->arp_from : 0u))) *
            16777619u;
        h = (h ^ ((uint32_t)cs->carg2 | (uint32_t)cs->cwhen << 8 | (uint32_t)cs->ctime << 16 | comp_slot() << 24)) *
            16777619u;                                  /* (0.6.82) COMP 2, WHEN */
    }
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || ENGINES[t->eng_req % NENGINES] == &ENG_ADDITIVE ||
                                   t->eng_req % NENGINES == ENGI_FM6 ||
                                   (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_DRUM)
        h ^= ui.frame * 2654435761u;                 /* DRUM-X: the specimen moves every frame */
    if (pg->graph == GR_DXSND) h ^= ui.frame * 2654435761u ^ dx_sig();
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u;
    if (fop_page(pg->graph))                         /* (0.6.77) OPERATOR, OP ENV (track pages): the patch, the operator */
        h = (h ^ ((fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u ^ (uint32_t)fop_op << 8 ^ fop_stg)) * 16777619u;
    if (pg->graph == GR_STEPS || pg->graph == GR_ROLL || pg->graph == GR_CHANCE || pg->graph == GR_SHAPE) {
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
        if (pg->graph != GR_STEPS && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        if (pg->graph != GR_STEPS) {                 /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (pg->graph != GR_STEPS && !drum_track(t)) {   /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
            h = (h ^ (ui.pr_focus | (uint32_t)pg->id[0] << 4 | (ui.entry_open ? (uint32_t)ui.ent_len << 8 : 0u) |
                      (uint32_t)ui.ent_nn << 16)) * 16777619u;   /* (0.6.81: NOTE's focus, the length being held) */
        }
    }
    if (pg->graph == GR_DIAG) {                     /* (QG-1) GLO > MONITOR: the live numbers it draws */
        uint32_t av = 0, k;
        for (k = 0; k < trk_live; k++) {
            uint32_t v;
            for (v = 0; v < NVOICE; v++)
                av = av + (V(&trk[k], v).active != 0);
        }
        h = (h ^ song.cpu_q8 * 3329u ^ load_diag.peak * 1931u ^ load_diag.late * 2671u ^
             load_diag.shed * 6151u ^ (load_diag.valid ? 1u : 0u) * 7919u) * 16777619u;   /* (3.0) N/A -> % */
        h = (h ^ mc_fx_tk * 40503u ^ fx_trips * 7919u ^ trk_live * 104729u ^ av * 131u) * 16777619u;
        /* (QG-1 v1.5) the rows added on top of those: the part split, core 1's job and the multicore state.
         * Without these the page kept the figure it first drew: mc_status() and CORE SPLIT never moved, because
         * nothing in the old hash changed when they did (that is the "data that does not update"). */
        h = (h ^ (uint32_t)mc_mask_last * 65537u ^ (uint32_t)mc_live() * 374761393u ^
             (uint32_t)mc_off * 2654435761u) * 16777619u;
#if FELUCCA_DUAL_CORE                              /* the HAL's own state, only there with a second core */
        h = (h ^ fm1_mc_job_us() * 2246822519u ^ (uint32_t)fm1_mc_lost * 668265263u) * 16777619u;
        h = (h ^ (uint32_t)fm1_mc_barred() * 1013904223u) * 16777619u;
#endif
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    /* (1.7) PRESETS and PROJECT are painted by this one row, so their chrome lands here once: the
     * picked row is a bevel like MENU's -- a light edge on top, a shadow under it -- and the rows
     * below it keep the page's own rule (nothing else about the row changed: same text, same ink). */
    if (sel) {
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        cv_rect(12, y, 216, 1, ux_mix(T_THEME, T_TEXT, 34));
        cv_rect(12, y + 15, 216, 1, ux_mix(T_THEME, T_BG, 34));
    }
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + CAP_IN(S, 16), &AF_S, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(54, y + CAP_IN(S, 16), &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel note hint centred");
    cv_text_on(cv_keycap(HALF_UP(240 - kh_ink((uint32_t)id, s)), y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* preset browser: the global list (every engine), the current one selected; tag DIM, name TEXT,
 * favourites starred (the accent), the selected row's suggested pattern at its right */
static __attribute__((noinline)) void graph_browse(void)
{
    uint32_t total, cur = preset_pos(&total), e, k;
    int32_t row;
    if (!total) {                                       /* (FASE 2) which filter emptied the list is said */
        if (pcat && favorites.filter)
            panel_note("NO FAVORITES", "IN THIS CATEGORY", 0);
        else if (pcat)
            panel_note("NO SOUNDS", "IN THIS CATEGORY", 0);
        else
            panel_note("NO FAVORITES", "LIST ALL TO ADD SOUNDS", 0);
        return;
    }
    {   /* (v1.11) the pop-up's five rows are the pop-up's: painted by its band, not here (see graph_preset_pop) */
        int32_t pop = ui.preset_pop_ms && (uint32_t)(fm1_ms - ui.preset_pop_ms) < 1500u;
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3), x1 = 212;
        if (pop && row >= -2 && row <= 2) continue;
        char tag[6], nm[13], pt[4], pn[13];
        uint32_t index = preset_visible(cur, total, (uint32_t)(row + 3));
        int sel = index == cur;
        int32_t hint = sel ? preset_pat_hint() : -1;    /* the suggested pattern */
        if (index >= total) continue;
        e = preset_at(index, &k);
        if (e == NENGINES) {                             /* user preset: "U07" and its name */
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        if (hint >= 0) {
            pat_label((uint32_t)hint, pt, pn);
            x1 = 206 - text_w(&AF_S, pt) - 6;
        }
        /* (FASE 3) the preset's own level, measured from a render of it (preset_level.c, the same number
         * loading the preset writes into P_LEVEL): a bar, so the browser says how loud a sound is before
         * it is loaded instead of being a list of names. User slots have no measurement: they keep the
         * full width for the name. */
        if (e != NENGINES) {
            int32_t lv = (int32_t)preset_level_of(e, k);
            int32_t bw = 16;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row level bar centred up/down");
            cv_rect(170, y + 6, bw, 3, sel ? T_INK : T_RAISE);
            if (lv > 0)
                cv_rect(170, y + 6, bw * (lv > 127 ? 127 : lv) / 127, 3, sel ? T_INK : T_ACCENT);
            if (x1 > 164)
                x1 = 164;                       /* (the bar's own column: the selected row keeps its pattern
                                                     * number at 186 and the star at 214 clear of it) */
        }
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, x1);
        if (e != NENGINES && snd_edited(TSEL) && e == (uint32_t)(TSEL->eng_req % NENGINES) &&
            k == (uint32_t)(TSEL->preset % ENGINES[e]->npresets))
            cv_rect(230, y + 6, 4, 4, sel ? T_INK : T_ACCENT);   /* (1.2.6) this one is SOUND EDITED */
        if (hint >= 0) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(206, y + CAP_IN(S, 16), &AF_S, pt, T_INK, T_THEME);
        }
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
            cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
    }
}

/* Compact semi-translucent 1.5s HUD popup centered on screen when changing presets via knob */
static __attribute__((noinline)) void graph_pop_plate(int32_t x0, int32_t oy)
{
    uint32_t total, cur = preset_pos(&total), e = NENGINES, k = 0u;
    int32_t r, y0 = oy, w = 208, h = 86;
    char b[24];
    if (!total)
        return;
    cur = preset_pos(&total);
    if (!total)
        return;
    cv_bg = T_SURF;
    /* (v1.8.7) A CARD, NOT FIVE BARE NAMES. The transient said the preset's name and nothing else: not what the
     * sound is, not how loud it comes out, not where it sits in the library -- the three things a player weighing
     * "stay or turn again" actually needs. It is a QUANTUM card now: the gradient body and bevel the FM6 operator
     * boxes and MENU's rows use, an accent rail along its head, a head strip with the picked sound's engine, its
     * category, its measured level (the same number the browser's rows carry as a bar) and its place in the list
     * ("06/291"), then the five rows of the list with the picked one lit and starred if it is a favourite.
     *
     * The box, the place and the send discipline are unchanged: 208 x 86 at (16, Y_GRAPH + 18) on a page of its
     * own, or into the panel's canvas at (16, 18) when the panel is drawn -- the browser blanks exactly those five
     * rows for it, and --regions case A measures that nothing outside the box is touched. */
    if (ux.chamfer && !ux.style) {
        gw_oct_panel(x0, y0, w, h, 6, T_SURF, T_ACCENT, 1, T_ACCENT);
    } else {
        gw_bevel(x0, y0, w, h, T_SURF, ux_mix(T_SURF, T_TEXT, 12), ux_mix(T_SURF, T_BG, 58));
        cv_rect(x0 + 7, y0 + 1, w - 14, 2, T_ACCENT);          /* the rail: the card's own edge, as HOME's panels */
    }
    {   /* the head: what the picked sound is, and how loud. Its own canvas tone keeps it inside the plate. */
        uint32_t total2, index = cur;
        int32_t lvx = 0;
        char bq[24];
        e = preset_at(index, &k);
        GFX_HOOK_ALIGN(0, y0 + 5, 0, y0 + 18, AL_V, "preset card head text centred up/down");
        fmt_int(bq, (int32_t)index + 1);                        /* "06/291": where the sound sits */
        str_cpy(bq + str_len(bq), "/", 2);
        fmt_int(bq + str_len(bq), (int32_t)total);
        if (e == NENGINES) {
            cv_text(x0 + 9, y0 + 4, &AF_S, "USER", T_MID);
            cv_text(x0 + 40, y0 + 4, &AF_S, "SOUND", T_TEXT);
        } else {
            int32_t lv = (int32_t)preset_level_of(e, k);
            const char *cat = PCAT_NAME[pcat_of(e, k) < PCAT_N ? pcat_of(e, k) : 0u];
            str_cpy(b, ENGINES[e]->name, sizeof b);
            str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), cat, sizeof b - str_len(b));
            cv_text(x0 + 9, y0 + 4, &AF_S, b, T_TEXT);         /* "ANALOG KEYS" */
            lvx = x0 + w - 9 - text_w(&AF_S, bq) - 8 - 26;      /* the bar sits left of the place, clear of both */
            cv_rect(lvx, y0 + 11, 26, 3, T_RAISE);             /* its measured level, as the browser's bar */
            if (lv > 0)
                cv_rect(lvx, y0 + 11, 26 * (lv > 127 ? 127 : lv) / 127, 3, T_ACCENT);
        }
        total2 = total;
        (void)total2;
        GFX_HOOK_ALIGN(0, y0 + 5, 0, y0 + 18, AL_V, "preset card head text centred up/down");
        cv_text_r(x0 + w - 9, y0 + 4, &AF_S, bq, T_TEXT, T_SURF);
    }
    for (r = -2; r <= 2; r++) {
        int32_t ry = y0 + 19 + (r + 2) * 13;
        int32_t x1 = x0 + w - 9;
        char tag[6], nm[16];
        uint32_t index = preset_visible(cur, total, (uint32_t)(r + 3));
        int sel = index == cur, fav;
        if (index >= total)
            continue;
        e = preset_at(index, &k);
        if (e == NENGINES) {
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        fav = favorite_has(e, k);
        if (fav)
            x1 -= 14;
        if (sel) {
            cv_rrect(x0 + 4, ry, w - 8, 13, 3, T_THEME, T_SURF);
            GFX_HOOK_ALIGN(0, ry, 0, ry + 13, AL_V, "preset card row text centred up/down");
            cv_text_on(x0 + 9, ry - 1, &AF_S, tag, T_INK, T_THEME);
            GFX_HOOK_ALIGN(0, ry, 0, ry + 13, AL_V, "preset card row text centred up/down");
            cv_free_text(x0 + 40, ry - 1, &AF_S, nm, T_INK, T_THEME, x1 - (x0 + 40));
        } else {
            GFX_HOOK_ALIGN(0, ry, 0, ry + 13, AL_V, "preset card row text centred up/down");
            cv_text(x0 + 9, ry - 1, &AF_S, tag, T_MID);
            GFX_HOOK_ALIGN(0, ry, 0, ry + 13, AL_V, "preset card row text centred up/down");
            cv_free_text(x0 + 40, ry - 1, &AF_S, nm, T_TEXT, T_SURF, x1 - (x0 + 40));
        }
        if (fav) {
            GFX_HOOK_ALIGN(0, ry, 0, ry + 13, AL_V, "preset card row text centred up/down");
            cv_icon_in(x1 - 1, ry - 1, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
}
/* (v1.7.20) THE PLATE RIDES THE PANEL, OR IT IS A SEND OF ITS OWN. It used to blit the 240 x 86 band AFTER
 * draw_graph had already sent the panel that contains those rows: on every step of the PRESETS knob the same rows
 * went to the panel twice, first as the list (its five middle rows blanked) and then as the plate over them -- the
 * by-the-book definition of the preset menu's flicker, and the third attempt at it (v1.11 made the browser leave
 * those rows alone, which stopped the *stale* list but not the second send). The plate is drawn into the panel's
 * canvas before the one blit now; only a frame where the panel was cached still needs its own send. */
static uint32_t graph_pop_frame = 1u << 31;        /* ui.frame of the last frame the plate rode a panel blit */

/* (v1.8.7) WHAT THE CARD SHOWS, AS ONE NUMBER. The card's pixels are a function of the sound picked, the list it is
 * picked from, the filter, the star and the two styles -- nothing else. While the knob rests, the 1.5 s window keeps
 * running and the card was SENT AGAIN every frame of it: 17 888 px, thirty times a second, of pixels identical to
 * the ones already on the glass. That is the "0 changed, everything written" the idle instrument reports, and on a
 * panel that shows every write it is the card flickering on its own. It is sent once now, and once more only when
 * one of these numbers moves. */
static uint32_t graph_pop_sig(void)
{
    uint32_t total, cur = preset_pos(&total), e, k, h;
    if (!total)
        return 0xFFFFFFFFu;
    e = preset_at(cur, &k);
    h = cur * 2654435761u ^ total * 40503u ^ (uint32_t)pcat * 7919u ^ (uint32_t)favorites.filter * 104729u;
    h = (h ^ (uint32_t)e * 2246822519u ^ (uint32_t)k * 668265263u) * 16777619u;
    h = (h ^ (uint32_t)favorite_has(e, k) * 131u ^ (uint32_t)ui.uslot * 1299709u ^ up_gen * 65537u) * 16777619u;
    h ^= (uint32_t)ux.style << 8 | (uint32_t)ux.chamfer << 9 | (uint32_t)(cur_page()->graph == GR_BROWSE) << 10;
    return h ^ 0x9E3779B9u;
}
static uint32_t graph_pop_sent;                    /* graph_pop_sig() of the card last put on the glass */

static __attribute__((noinline)) void graph_preset_pop_in_panel(void)
{
    uint32_t total;
    int32_t full = cur_page()->graph == GR_BROWSE;
    if (!preset_pos(&total))
        return;
    if (full)                                       /* the band's margins, as the standalone canvas paints them */
        cv_rect(0, 18, 240, 86, T_BG);
    graph_pop_plate(16, 18);
    graph_pop_frame = ui.frame;
    graph_pop_sent = graph_pop_sig();
}

/* the plate alone, for a frame whose panel was not drawn (its signature did not move): one send, not two */
static __attribute__((noinline)) void graph_preset_pop(void)
{
    uint32_t total, sig = graph_pop_sig();
    int32_t full = cur_page()->graph == GR_BROWSE;
    if (!preset_pos(&total))
        return;
    if (sig == graph_pop_sent)                     /* (v1.8.7) the card on the glass is this card: nothing to send */
        return;
    cv_begin(full ? 240u : 208u, 86u, full ? T_BG : T_SURF);
    cv_oy = 0;
    graph_pop_plate(full ? 16 : 0, 0);
    cv_blit(full ? 0 : 16, Y_GRAPH + 18);
    graph_pop_frame = ui.frame;
    graph_pop_sent = sig;
}

/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, e->npresets ? e->presets[TSEL->preset % e->npresets].name : "", sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* user preset slots around the selected one: "U07  NAME" / EMPTY */
/* (5.1) SOLAR HEAT ON EVERY SECTION PAGE. HOME's screens open on a warm, dithered ground and the rest of
 * the firmware opened on a flat T_SURF card, which is the inconsistency the brief names: this is that ground,
 * adapted to a page that has to stay readable. Twelve bands over the panel, darkened a shade at the top and
 * warmed towards the bottom, all inside the rounded corners (7 px in, 2 px down) so no band can paint a
 * straight edge over the radius. A function of the theme and the panel's height only, so it costs twelve
 * dithered rects on a full redraw and nothing on a cached frame, uses no animation clock, and leaves MONO
 * and the BW palette flat: a dither there is only noise. */
/* (5.1 / FASE 3) THE SOLAR HEAT GROUND, on every style now, and cheap.
 * It used to run only in the LINE style -- the rounded styles got a plain panel -- and it dithered every
 * pixel of the graph through gw_dither: a 226 x 96 area is 21 000 single-pixel fills, more than the panel it
 * sits in is worth. Twelve solid bands cost twelve fills and read the same at 240 x 240; the seam rows are a
 * step hotter so the joins do not show as bands.
 * It used to ask heat_col() for the tint as well, and that was wrong twice over: in the rounded styles
 * heat_col() is not the ramp at all, it is the theme ACCENT (cyan in QUANTUM), so the first version painted a
 * cold teal floor and the page read as the old one with a border. The ground is built from the two tokens the
 * palette already defines for exactly this: the page it sits on (T_BG, the deep indigo the bezel casts) at the
 * top, and the palette's signature colour (T_THEME, QUANTUM's gold) at the foot. Violet shadow above, solar
 * heat below, in every palette and in every style; GREY and MONO get a neutral bevel because both tokens are
 * neutral there, and MONO is skipped outright: a black-and-white page has no heat, and its contrast belongs
 * to the data. */
static __attribute__((noinline)) void graph_heat(void)
{
    /* Two separate ramps, not one wash: the shade the bezel casts (the top tenth), the panel's own colour
     * untouched through the middle, and the Solar Heat glow (the bottom quarter, six steps to 15 %). The first
     * attempt tinted the whole area and every list turned sepia, which cost the dim rows their contrast; the
     * second left hard bands where the glow began, which read as a stripe across the panel. Keeping the middle
     * untouched is also the cheapest: nine fills, and the screens whose data sits in the middle keep the tone
     * they were drawn on. */
    int32_t k, h = graph_h(), y0 = 2, y1 = h - 2, span = y1 - y0;
    uint16_t warm;
    int32_t top;
    if (ux.bw || span < 24)
        return;
    warm = ux_mix(T_ACCENT, T_TEXT, 12);           /* the accent, warmed one step toward gold */
    top = 15;
    if (ux_luma(T_SURF) > 150) {                   /* a light page: the accent is darker than the paper, so a
                                                    * full-strength foot reads as a smudge rather than a glow */
        warm = ux_mix(T_SURF, T_TEXT, 8);
        top = 9;
    }
    for (k = 0; k < 3; k++) {                      /* under the bezel: -18 .. -6 */
        int32_t a = y0 + k * span * 4 / 100, b = y0 + (k + 1) * span * 4 / 100;
        cv_rect(7, a, 226, b - a, ux_mix(T_SURF, T_BG, 18 - k * 6));
    }
    /* (7.4) ONE RAMP, NOT TWO. The foot glow that used to end here is gone: graph_ground_ramp() runs from under the
     * bezel to the panel's own floor, so each page carries a single gradient -- the console's -- instead of a ramp
     * that stopped short of another one below it. `warm` and `top` are the ramp's, which the caller now uses. */
    (void)warm;
    (void)top;
}

/* (3.7) THE SEQUENCER'S GROUND. graph_heat() already gives every page the bezel's shade at the top and the
 * Solar Heat glow at its foot; what the panels lacked was the ramp between the two, the one HOME's views
 * (hv_grad) and FX > BUSES' blocks (fxb_grad) are drawn on. It is the shared primitive -- ux_ground_bands,
 * ten bands of the same law, solid rather than dithered -- clipped to the rows between the bezel and the
 * glow so neither is painted over. Solid is the point: the checker the other two screens use, drawn under
 * the sequencer's grid, read as dirt on the panel. Ten bands of two units each is a blend with no seam and
 * no texture, and the accent stays at 0..18 % so a row of steps keeps its contrast in every palette,
 * GREY and MONO included. */
static __attribute__((noinline)) void graph_ground_ramp(void)
{
    int32_t span = graph_h() - 4;
    /* (7.4) from under the bezel's shade to the panel's own floor: one ramp a page, ten bands, and the accent
     * arrives at the bottom edge instead of stopping a quarter of the panel short and starting again below. */
    int32_t y0 = 2 + span * 10 / 100, y1 = 2 + span * 97 / 100;
    uint16_t warm = ux_mix(T_ACCENT, T_TEXT, 12);
    int32_t top = ux_luma(T_SURF) > 150 ? 8 : 22;                /* a light page takes a lighter hand */
    if (y1 - y0 < 12)
        return;
    ux_ground_ramp(7, y0, 226, y1 - y0, 10, T_SURF, ux_mix(T_SURF, warm, top), 100);
}

static __attribute__((noinline)) void graph_user(void)
{
    int32_t row, first = clamp((int32_t)ui.uslot - 3, 0, UP_SLOTS - 7);
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        int used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "--", sizeof nm);
        list_row(LIST_Y(row), k == ui.uslot, tag, T_MID, nm, used ? T_TEXT : T_DIM, 232);
    }
}
/* SEQ > PATTERNS: the pattern list around the one picked ("03  MELODY", "U07  MY BASS") */
static __attribute__((noinline)) void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, T_TEXT, 232);
    }
}
/* SEQ > AUTO LIST (ui_events.c): the track's records around the one selected, a row each: the kind (LOCK in the
 * accent, AUTO), the step, the parameter's name, the value at the right; the last row + ADD LOCK */
/* (FASE 5/9) SEQ > AUTOMATION: the movement itself. One parameter (KNOB 3 walks the ones the track records), its
 * automation drawn as it sounds -- the sound's own value until the first record, each record held to the next --
 * with its locks as ticks on the step they belong to, the recorder's grid under it (KNOB 2) and, while the song
 * plays, the step sounding. The counts stay in the columns above, the page's actions in KNOB 4. */
static __attribute__((noinline)) void graph_motion(void)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), k = trk_index(t), id = ui.mo_id, i, step;
    uint32_t r = motion_res_steps(k), span;
    int32_t x0 = 10, x1 = 230, y0 = 24, y1 = 112, px = -1, py = -1, lo, hi, v;
    char b[24], nm[12];
    const param_desc_t *d;
    if (id >= P_COUNT || !motion_has_id(t, id))
        for (id = 0; id < P_COUNT && !motion_has_id(t, id); id++)
            ;                                        /* (the first one it does record: the movement is the point) */
    span = len > 1u ? len - 1u : 1u;
    if (id >= P_COUNT) {                             /* (no parameter to draw: the columns' counts say the rest) */
        cv_line(x0, (y0 + y1) / 2, x1, (y0 + y1) / 2, T_SURF);
        return;
    }
    d = track_desc(t, id);
    str_cpy(nm, d->label, sizeof nm);
    cv_text_on(14, LIST_Y(0) + CAP_IN(S, 16), &AF_S, nm, T_ACCENT, T_BG);
    str_cpy(b, "GRID ", sizeof b);                   /* the grid the recorder takes a move on: 1, 2 or 4 steps */
    b[5] = (char)('0' + r);
    b[6] = 0;
    cv_text_r(226, LIST_Y(0) + CAP_IN(S, 16), &AF_S, b, T_DIM, T_BG);
    lo = d->min;
    hi = d->max > d->min ? d->max : d->min + 1;
    v = motion_base_value(t, id);
    for (step = 0; step < len; step++) {             /* the value as it sounds, step by step */
        int32_t x = x0 + (int32_t)((x1 - x0) * (int32_t)step / (int32_t)span), y;
        for (i = 0; i < motion.count; i++) {
            const motion_event_t *e = &motion.event[i];
            if (MOTION_TRK(e) != k || MOTION_ID(e) != id || MOTION_STEP(e) != step) continue;
            if (!(e->param & MOTION_LOCK)) v = MOTION_VAL(e);   /* (a lock holds its step only: not the curve) */
        }
        y = y1 - (int32_t)((y1 - y0) * (v - lo) / (hi - lo));
        y = y < y0 ? y0 : y > y1 ? y1 : y;
        if (py >= 0) cv_line(px, py, x, y, T_THEME);
        cv_rect(x, y - 1, 2, 3, T_ACCENT);
        px = x; py = y;
    }
}
static __attribute__((noinline)) void graph_events(void)
{
    const track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), cur = ev_row(n), k;
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n + 1 > 7 ? (int32_t)n + 1 - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) <= n; row++) {
        int32_t y = LIST_Y(row);
        int sel;
        uint16_t bg;
        k = (uint32_t)(first + row);
        sel = k == cur;
        bg = sel ? T_THEME : T_SURF;
        if (sel)
            cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
        if (k == n) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            break;
        }
        {
            const motion_event_t *e = &motion.event[idx[k]];
            uint32_t id = MOTION_ID(e), lock = (e->param & MOTION_LOCK) != 0u;
            char b[16], nm[12];
            const char *unit;
            const param_desc_t *d = track_desc(t, id);
            b[0] = (char)('0' + (MOTION_STEP(e) + 1u) / 10u);
            b[1] = (char)('0' + (MOTION_STEP(e) + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, lock ? "LOCK" : "AUTO", sel ? T_INK : lock ? T_ACCENT : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(54, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_MID, bg);
            param_format(d, MOTION_VAL(e), b, &unit);
            if (unit && unit[0] && str_len(b) + str_len(unit) < sizeof b)
                str_cpy(b + str_len(b), unit, sizeof b - str_len(b));
            ev_name(t, id, nm);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_free_text(80, y + CAP_IN(S, 16), &AF_S, nm, sel ? T_INK : T_TEXT, bg, 226 - text_w(&AF_S, b) - 8 - 80);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_TEXT, bg);
        }
    }
}
/* (0.6.12) SAVE > TOOLS: a list of what it can do (on what: the track, the song's row), the one picked (KNOB 1 or
 * PRESETS, 0.6.25: was a knob each) filled; OCT+ does it (a dialog first) */
static __attribute__((noinline)) void graph_tools(void)
{
    static const char *const WHAT[7] = {"CLEAR SEQ", "INIT SOUND", "DELETE ROW", "CLEAR SONG", "NEW PROJECT", "CPU TEST",
                                        "CLEAR PROJ"};
    static const char *const ON[7] = {"STEPS + AUTO", "ENGINE DEFAULT", "", "ALL ROWS", "EVERYTHING", "HEAVY SONG",
                                      "SECTIONS A-D"};
    uint32_t c;
    char b[24];
    for (c = 0; c < 7u; c++) {                          /* (1.2.1-GW, issue #22: 7, CLEAR PROJECT; the list fills the
                                                         * panel, OCT+'s action in the footer as before) */
        int32_t y = LIST_Y(c);
        int sel = ui.act == c + 1u;
        uint16_t bg = sel ? T_THEME : T_SURF;
        b[0] = (char)('1' + c); b[1] = 0;               /* (0.6.25: a list, KNOB 1 / PRESETS: was a knob each) */
        list_row(y, sel, b, T_MID, WHAT[c], T_TEXT, 150);
        if (c == 2u) {                                   /* (the row SONG has picked) */
            str_cpy(b, "ROW ", sizeof b);
            fmt_int(b + 4, (int32_t)ui.song_row + 1);
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_DIM, bg);
        } else {
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, ON[c], sel ? T_INK : T_DIM, bg);
        }
    }
}
/* (0.6.12) SAVE > PROJECT: the project (one of NSONG: a song of storage.c) and its four sections. A title line
 * (PROJECT n, the section the music came from or went to), the sections A-D (name, USED or --; the one picked filled;
 * the music's: NOW; the bars each plays in the project's song), then the song (ARRANGE) in a line: A4 > B8 > .. */
static __attribute__((noinline)) void graph_slots(void)
{
    uint32_t i, r, bars[4] = {0, 0, 0, 0};
    char b[40];
    for (r = 0; r < chain_config.count && r < CHAIN_ROWS; r++)
        bars[chain_config.row[r].slot & 3u] += chain_config.row[r].bars;
    str_cpy(b, "PROJECT ", sizeof b);
    fmt_int(b + 8, (int32_t)song_cur + 1);
    cv_text_on(14, LIST_Y(0) + CAP_IN(S, 16), &AF_S, b, T_ACCENT, T_SURF);
    str_cpy(b, "OF ", sizeof b);
    fmt_int(b + 3, (int32_t)NSONG);
    cv_text_r(226, LIST_Y(0) + CAP_IN(S, 16), &AF_S, b, T_DIM, T_SURF);
    for (i = 0; i < 4u; i++) {
        int32_t y = LIST_Y(i + 1u);
        int sel = (int32_t)i + 1 == song.g[G_SLOT], used = graph_project_used(i), now = live.cur == (int8_t)i;
        const char *n = graph_project_name(i);
        uint16_t bg = sel ? T_THEME : T_SURF;
        b[0] = (char)('A' + i);
        b[1] = 0;
        list_row(y, sel, b, now ? T_ACCENT : T_MID, !used ? "--" : n[0] ? n : "USED", used ? T_TEXT : T_DIM, 150);
        if (bars[i]) {                               /* (its bars in the song) */
            fmt_int(b, (int32_t)bars[i]);
            str_cpy(b + str_len(b), " BAR", 8);
            cv_text_r(now ? 190 : 226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_DIM, bg);
        }
        if (now) cv_text_r(226, y + CAP_IN(S, 16), &AF_S, "NOW", sel ? T_INK : T_ACCENT, bg);
    }
    if (!chain_config.count)
        str_cpy(b, "NO SONG YET: SAVE > ARRANGE", sizeof b);
    else {
        uint32_t n = 0;
        for (r = 0; r < chain_config.count && n + 6u < sizeof b; r++) {
            if (r) { b[n++] = '>'; }
            b[n++] = (char)('A' + (chain_config.row[r].slot & 3u));
            fmt_int(b + n, chain_config.row[r].bars);
            n = str_len(b);
        }
        if (r < chain_config.count) { b[n++] = '.'; b[n++] = '.'; }
        b[n] = 0;
    }
    cv_free_text(14, LIST_Y(5) + 2 + CAP_IN(S, 16), &AF_S, b, chain_config.count ? T_THEME : T_DIM, T_SURF, 226 - 14);
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];                              /* (the cards: JIANT's six columns, else the four from trk_base4) */
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg, 0), *rv = ramp(vc, bg, 0);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* (v1.7.0) The sound's name cut to px, the way both mixers print it: its first word, as many letters as fit,
 * no "...". The six columns explained the rule; the LARGE strip ellipsised instead, so a 57 px card said
 * "PULSE..." where a 38 px column said four letters -- the wider card showing less of the same name. One rule,
 * one spelling, in either size. */
static void trk_cut_name(const char *b, char *out, int32_t px)
{
    uint32_t n;
    for (n = 0; b[n] && b[n] != ' ' && n < 12u; n++)
        out[n] = b[n];
    out[n] = 0;
    while (n > 1u && text_w(&AF_S, out) > px)
        out[--n] = 0;
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_icon_on(4, 4, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else {
        char sn[13];
        trk_cut_name(b, sn, CARD_W - 10);
        cv_text_on(5, 22, &AF_S, sn, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF);
    }
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c - trk_base4()), LG_Y_GRAPH);
}
/* (JIANT 0.6) the JIANT style's mixer: the six tracks side by side, 38 px each. The track's cushion (armed: a dot,
 * red recording), the sound's name (muted: MUTE), LEVEL as the thermal column with the meter brighter over it, its dB
 * beside it, PAN (from the middle) and the REV send as the two bars under it */
#define T6_W 38
#define T6_X(c) (3 + 39 * (int32_t)(c))
static void track_col6(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t hot, uint32_t lvl, int32_t pan,
                       int32_t rv, const char *b)
{
    char v[8];
    int32_t k, lv = (int32_t)lvl * 14 / 127, mt = ts.meter[c] * 14 / TS_MH, pw = T6_W - 8;
    uint16_t lc = hot == 1u ? T_ACCENT : lvl ? (mute ? T_DIM : T_TEXT) : T_DIM;
    cv_begin(T6_W, H_GRAPH, T_BG);
    cv_rrect(0, 0, T6_W, H_GRAPH, 5, T_SURF, T_BG);
    if (ux.chamfer && !ux.style) {
        cv_rect(4, 0, T6_W - 8, 1, sel ? T_ACCENT : T_RAISE);
        cv_rect(4, H_GRAPH - 1, T6_W - 8, 1, sel ? T_ACCENT : T_RAISE);
    }
    cv_icon_on(4, 5, 16, trk_icon(c, sel), sel ? T_ACCENT : mute ? T_DIM : T_MID, T_SURF);
    if (st == 1u || st == 2u)                        /* armed: a dot (recording: red) */
        cv_rrect(T6_W - 11, 10, 6, 6, 3, st == 1u ? T_REC : T_ACCENT, T_SURF);
    if (mute == 2u)                                  /* asleep (TRACKS 4) */
        cv_text_in(2, 22, T6_W - 4, &AF_S, "OFF", T_DIM, T_SURF);
    else {                                           /* (0.6.24) the sound's name cut to the column; muted: in MUTE's
                                                      * colour, still there (it said MUTE instead: which sound, gone) */
        char sn[13];
        trk_cut_name(b, sn, T6_W - 7);
        cv_text_on(4, 22, &AF_S, sn, mute ? T_ACCENT : sel ? T_TEXT : T_MID, T_SURF);
    }
    for (k = 0; k < 14; k++) {                       /* LEVEL: cold at the bottom, hot at the top */
        int32_t sy = 95 - k * 4;
        uint16_t sc = heat_col(20 + k * 220 / 13);
        sc = mute ? (k < lv ? T_DIM : T_RAISE) : k < mt ? sc : k < lv ? ux_mix(T_SURF, sc, 42) : T_RAISE;
        cv_rect(4, sy, 12, 3, sc);
    }
    if (lvl) {
        int32_t d = LEVEL_DB_X10[lvl];
        fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
    } else {
        str_cpy(v, "OFF", sizeof v);
    }
    cv_text_in(17, 44, T6_W - 18, &AF_S, v, lc, T_SURF);
    cv_text_in(17, 58, T6_W - 18, &AF_S, "dB", T_DIM, T_SURF);
    cv_rect(4, 104, pw, 3, T_RAISE);                 /* PAN, from the middle */
    cv_rect(4 + pw / 2, 102, 1, 7, hot == 2u ? T_ACCENT : T_MID);
    if (pan)
        cv_rect(pan > 0 ? 4 + pw / 2 + 1 : 4 + pw / 2 + pan * (pw / 2) / 64, 104, (pan > 0 ? pan : -pan) * (pw / 2) / 64 + 1,
                3, mute ? T_DIM : hot == 2u ? T_ACCENT : heat_col(70 + (pan > 0 ? pan : -pan) * 2));
    cv_rect(4, 113, pw, 3, T_RAISE);                 /* the REV send */
    if (rv)
        cv_rect(4, 113, rv * pw / 127 + 1, 3, mute ? T_DIM : hot == 3u ? T_ACCENT : heat_col(40 + rv * 3 / 2));
    cv_blit((uint32_t)T6_X(c), Y_GRAPH);
}
/* (v1.5.5) THE PANEL A POP-UP COVERED IS NOT A COLUMN'S. graph_preset_pop() paints a 208 x 86 HUD over the panel
 * and ui_draw.c cleared the graph's signature when it closed, so the panel's ground came back -- but every column
 * has its own stamp ("nothing changed in this one") and kept the HUD's ink where it had covered it: the preset's
 * name left in the MIXER's dB zone. This flag says a whole panel was covered, so the ground and every column are
 * painted again (ui_render --regions, case A). */
static uint8_t graph_covered;
static __attribute__((noinline)) void graph_invalidate(void)
{
    graph_covered = 1;
    ui.graph_sig = 0;
    graph_pop_sent = 0;                            /* (v1.8.7) the band is to be painted again: so is the card */
}
static __attribute__((noinline)) void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK, six = !sk;   /* (0.6.80: the six columns with every COLOR and STYLE;
                                                         * MENU > LARGE: four, bigger) */
    uint32_t b4 = six ? 0u : trk_base4(), n = six ? NTRK : 4u;
    /* (v1.5.5) a full panel: a new page / a knob's page, or one a pop-up covered. A TRACK CHANGE (ui.sel_only) is
     * not one -- only the columns whose stamp moved are painted, which is what the per-column cache is for */
    int all = (ui.force && !ui.sel_only) || graph_covered;
    graph_covered = 0;
    if (all) {
        lcd_fill(0, graph_y(), 240, graph_h(), T_BG);
        if (ux.style && !six)                        /* LINE: the strips divided as the cards above (four) */
            draw_rules(graph_y(), graph_h());
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = b4; c < b4 + n; c++) {                  /* (JIANT 0.6: the six, or four of them) */
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        if (c >= trk_live)                           /* (0.6.22) TRACKS 4: asleep, grey as muted, "OFF" (mute 2) */
            mute = 2u;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pk = t->vu, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        /* (F4, #106) THE SELECTION IS THIS COLUMN'S ALONE. `sel` (the whole panel's) was in every column's stamp,
         * so changing track -- ALGORITHM's knob -- invalidated all six and the panel repainted itself column by
         * column: the "entire mixer box flickers and redraws" in the report. Only the column that lost the
         * selection and the one that took it can look different, and each one's own bit says so. */
        sig = str_hash(1u + (sel ? 1u : 0u) + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u + c * 0x51u +
              six * 0x3C6Fu;
        if (!all && sig == ts.col[c - b4])
            continue;
        ts.col[c - b4] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b);
            continue;
        }
        if (six) {
            track_col6(c, sel, st, mute, hot, lvl, pan, rv, b);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        cv_icon_on(4, 5, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* (the JIANT style: track_col6) */
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10[lvl];
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            /* (2.2) THE SAME INSTRUMENT AS HOME. This meter drew a solid 4 px bar while HOME drew eight
             * lit segments on the heat ramp, so the two meters of one device read as two instruments --
             * that is the audit's finding, and the reason this is one call now: gw_plasma_bar is the bar
             * HOME draws, at the same 0..256 scale and on the same heat walk. */
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            gw_plasma_bar(45, TS_MY, 4, TS_MH, 8, (int32_t)ts.meter[c] * 256 / (TS_MH - 2), 60);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / 64);
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        }
        cv_blit((uint32_t)CARD_X(c - b4), Y_GRAPH);
    }
}
/* (QG-1) Full-Screen Integrated Oscilloscope & Realtime Matrix:
 * Rendered in 2 vertically-tiled 240x88 canvas bands to respect CV_MAX buffer limit.
 * - Dual-layer phosphor oscilloscope beam with rising zero-crossing trigger.
 * - 16-band energy spectrum analyzer with decaying peak markers.
 * - 6-track mini VU activity meter across top reticle.
 * - Orbital phase space trajectory x(t) vs x(t - tau).
 * - High-contrast, crystal-clear bottom telemetry & exit hint [HOME] EXIT. */
static int16_t scope_snap[SCOPE_N];

/* (FASE EXTRA) HOW DARK THE SCOPE'S WELL IS. The wells used to be opaque plates of T_SURF, which is why the
 * visualizer's field, its dust, its band and its two worlds could not be seen behind the trace. They are a
 * veil now: cv_rect_a() mixes this share of a page tone over whatever the field already drew, so the
 * background reads through the glass and the grid and the trace (drawn after, solid) stay the brightest thing
 * in the well. 60 %: the field is legible and the trace still leads. (A MENU row for it was written and
 * measured; with 1.4.7's panel work there are not 90 B of XIP left for a setting, so the share is fixed.) */
#define VIZ_VEIL 60
#define Y_LABEL_GAP_H (26 - Y_LABEL)   /* (v1.11) rows Y_LABEL..25: the only rows above the panel's 26 that the
                                        * visualizer has to cover -- 26 and up are the two passes' own */
static __attribute__((noinline)) void graph_home_viz(uint16_t c)
{
    static uint8_t peak_decay[16];
    uint32_t w = scope_w, i, trig = 0, pass;
    int32_t peak = 1000, bars[16];
    /* (3.5) a slightly deeper page: the panel and the space around it sit one step under the palette's own
     * tones, so the data (the only bright thing here) leads and the field behind it recedes. bg_darken()
     * can only move a tone down, so MONO stays black and PAPER stays legible. */
    /* page and plate, one step under the palette's own tones: bg_darken() only moves a tone down, so MONO stays
     * black and PAPER stays legible */
    uint16_t vbg = ux_mix(bg_darken(T_BG, 14), T_BG, 62), vsf = bg_darken(T_SURF, 10);
    /* (1.4) the two wells are the plate's own tone (T_SURF), not black boxes: opaque, so the field and its worlds
     * stay out of the grid and the trace, but painted in what surrounds them, so they read as part of the page */
    int mono = ux.mono || settings.palette == UI_BW_INDEX;

    for (i = 0; i < SCOPE_N; i++) {
        scope_snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (scope_snap[i] > peak) peak = scope_snap[i];
        else if (-scope_snap[i] > peak) peak = -scope_snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (scope_snap[i - 1] < 0 && scope_snap[i] >= 0) { trig = i; break; }

    /* One measurement per frame: the two band passes draw the same bars and peak caps. */
    for (i = 0; i < 16u; i++) {
        int32_t bsum = 0, j;
        for (j = 0; j < 8; j++) {
            int32_t val = scope_snap[(trig + i * 8u + (uint32_t)j) & (SCOPE_N - 1u)];
            if (val < 0) val = -val;
            bsum += val;
        }
        bsum = (bsum * 44) / (peak ? peak * 8 : 1);
        if (bsum > 34) bsum = 34;
        bars[i] = bsum;
        /* (v1.7.20) `>` made an EQUAL level decay the cap and the next frame raise it back: the peak mark
         * alternated between two rows every frame -- a 30 Hz blink across the spectrum floor, which is the
         * visualizer flicker of the report. Equal holds; only a level below the cap lets it fall. */
        if (bsum >= peak_decay[i]) peak_decay[i] = (uint8_t)bsum;
        else if (peak_decay[i]) peak_decay[i]--;
    }

    /* (1.3.9) THE SIX ROWS ABOVE THE PANEL ARE THE VISUALIZER'S TOO. Its two 88-row passes start at 26, and rows
     * Y_LABEL (20) .. 25 were painted by nobody: draw_frame() only fills the gaps BETWEEN the strips, so the top of
     * the four knob cards HOME had just drawn stayed on screen -- the squares that showed in the visualizer. A cap
     * of the page's own tone, blitted at Y_LABEL, covers them; the passes then draw over 26..202.
     * (v1.11) THE CAP IS THE SIX ROWS IT IS FOR. It was 26 rows tall, so it painted -- and the panel was made to
     * send -- rows 20..45: the panel's own top 20 rows, twice a frame, first as the cap's flat page tone and then
     * as the panel and its title. A band sent twice with different content between the two sends is a flicker on
     * the glass, and it was in the one place the report named: the top of the visualizer. Six rows here; 26..202
     * belong to the two passes alone. */
    cv_oy = 0;
    cv_begin(240, Y_LABEL_GAP_H, vbg);
    cv_blit(0, Y_LABEL);

    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? 114 : 26, cy = 100, amp = 34;
        cv_begin(240, 88, vbg);
        cv_oy = -top_y;
        cv_bg = vsf;

        if (ux.chamfer && !ux.style)
            gw_oct_panel(4, 26, 232, 176, 6, vsf, T_LINE, 1, c);
        else
            cv_rrect(4, 26, 232, 176, 6, vsf, vbg);

        /* (3.5) the deep field, before anything is measured: the bodies the scope sits in front of */
        gw_viz_deep(ui_viz_q);

        /* A single quiet frame: header, a scope field, then the spectrum floor. */
        if (!pass) {
            cv_text(14, 30, &AF_S, "QG-1 QUANTUM SCOPE", T_THEME);
            cv_text_r(226, 30, &AF_S, ENGINES[TSEL->eng_req % NENGINES]->name, T_TEXT, T_SURF);
            cv_rect(14, 43, 212, 1, T_LINE);
            for (i = 0; i < trk_live && i < 6u; i++) {   /* (1.7) seven blocks a track, the 16-bit meter */
                int32_t vx = 16 + (int32_t)i * 35, k;
                int32_t nb = trk[i].peak > 0 ? clamp(trk[i].peak * 7 / 32767, 0, 7) : 0;
                for (k = 0; k < 7; k++)
                    cv_rect(vx + k * 3, 49, 2, 3, k < nb ? (i == song.sel ? T_ACCENT : T_THEME) : T_RAISE);
            }
        }

        /* The bay's dust, before anything is measured on it: the ember field is behind the trace, the
         * phase plot, the spectrum bars and every label (2.0: it used to be painted after the bars, so it
         * sat in front of the data). gw_embers() is ui_being.c's one implementation, shared with HOME's
         * own bay; 18 embers at HIGH, 12 at BALANCED, 6 at LOW, deterministic and allocation-free. */
        gw_embers(12, top_y, 216, 88, ui_viz_q >= 2u ? 10u : ui_viz_q == 1u ? 18u : 28u, 0u, 80, vsf);

        /* Scope field: the live trace on the left, its X/Y phase trajectory on the right. */
        /* (1.1.1) THE SCOPE'S OWN FLOOR, NOW SEE-THROUGH (FASE EXTRA). The deep field -- the milky way's band,
         * its dust and the two worlds -- is drawn on this panel. It used to be covered by an opaque well so the
         * trace could be read; the well is a veil of viz_veil() instead: enough of a near-page tone to keep the
         * trace and its grid legible, little enough that the field behind it still reads through the glass. */
        cv_rect_a(12, 63, 144, 72, T_BG, VIZ_VEIL);
        cv_rect(12, cy, 144, 1, T_LINE);
        for (i = 0; i < 4u; i++)
            cv_rect(12 + (int32_t)i * 48, 64, 1, 66, T_GRID);
        cv_rect(12, 64, 144, 1, T_GRID);
        cv_rect(12, 130, 144, 1, T_GRID);
        for (i = 0; i < 18u; i++)                        /* (1.7) the field's scale, every 8 px */
            cv_rect(12 + (int32_t)i * 8, 126, 1, 4, T_GRID);
        {
            int32_t py = cy, x;
            for (x = 0; x <= 144; x += 2) {
                int32_t sv = scope_snap[(trig + (uint32_t)x) & (SCOPE_N - 1u)];
                int32_t y = cy - (sv * amp) / peak;
                y = clamp(y, cy - amp, cy + amp);
                if (x) {                                 /* (1.7) one 2 px pass. The 3 px glow under it was a second
                                                          * line per segment and a ux_mix per pixel of reach: measured
                                                          * at 144 line calls a frame for a blur the panel cannot hold */
                    uint16_t core = mono ? T_TEXT : heat_col(76 + (y > cy ? y - cy : cy - y) * 178 / amp);
                    cv_line_t(12 + x - 2, py, 12 + x, y, core, 2);
                }
                py = y;
            }
        }
        /* (3.9) NO NOTE SPARKS HERE. They were born in the middle of the screen -- the scope field's floor, which
         * is where the notes' height put them -- and the pool's gravity took them down over the spectrum floor, so
         * the page rained particles from its centre. That emitter is off: the field's own embers, the milky way,
         * the stars, the two worlds and every other animation stay exactly as they were. HOME's bay keeps its
         * sparks: they belong to the gesture, not to this screen. */

        /* (FASE 3) THE SECOND SCOPE, THE SAME INSTRUMENT AS THE FIRST. The phase plot had its own recipe: a well
         * in the panel's SURF tone (lighter than the scope's), a frame mixed out of the grid at one weight, one
         * vertical line and one horizontal at different tones, and a trace straight in the accent. Side by side
         * the two read as two screens. Both wells are the page's own dark tone with the same 1 px frame, both
         * carry the same three-division grid and the same foot ticks, and both traces are on the same heat ramp,
         * so the pair reads as one instrument with two probes. */
        cv_rect_a(164, 64, 64, 67, T_BG, VIZ_VEIL);
        {
            uint16_t fr = ux_mix(T_BG, T_GRID, 62);
            cv_rect(164, 64, 64, 1, fr);
            cv_rect(164, 130, 64, 1, fr);
            cv_rect(164, 64, 1, 67, fr);
            cv_rect(227, 64, 1, 67, fr);
            for (i = 1; i < 4u; i++) {                   /* the same three divisions the scope has */
                cv_rect(164 + (int32_t)i * 16, 65, 1, 65, T_GRID);
                cv_rect(165, 64 + (int32_t)i * 22, 62, 1, T_GRID);
            }
            for (i = 0; i < 5u; i++)                     /* and its foot scale */
                cv_rect(165 + (int32_t)i * 12, 126, 1, 4, T_GRID);
        }
        /* (3.5) THE RIGHT SCOPE IS ALWAYS THERE, AND ALWAYS ITS OWN. It was drawn only when the snapshot's
         * peak passed 1400, so anything played quietly left the reticle empty -- the "it does not appear"
         * report -- and just above the threshold the few samples it had were clamped against the frame, which
         * read as a stuck, broken plot. It is scaled to its own peak now (the whole snapshot's, the same one
         * the trace uses), a quiet signal still draws inside the reticle instead of collapsing onto an edge,
         * and silence is the still point at the centre: a scope at rest, never a missing one. */
        {
            int32_t px = 196, py = 98;
            if (peak > 260) {
                for (i = 0; i <= 64u; i += 3) {         /* (1.7) every third sample: the orbit keeps its shape */
                    int32_t ox = 196 + (scope_snap[(trig + i) & (SCOPE_N - 1u)] * 27) / peak;
                    int32_t oy = 98 + (scope_snap[(trig + i + 16u) & (SCOPE_N - 1u)] * 24) / peak;
                    ox = clamp(ox, 169, 223);
                    oy = clamp(oy, 73, 122);
                    if (i) cv_line_t(px, py, ox, oy, mono ? T_TEXT : heat_col(150), 2);   /* (FASE 3) the same body
                                                                                           * the trace above has */
                    px = ox; py = oy;
                }
            } else {
                cv_rect(195, 97, 3, 3, mono ? T_TEXT : heat_col(150));
            }
        }

        /* Spectrum floor: 16 slim bars, with peak marks, below the trace. */
        cv_rect(12, 180, 216, 1, T_LINE);
        for (i = 0; i < 16u; i++) {
            int32_t bx = 14 + (int32_t)i * 13, bh = bars[i] * 26 / 34, ph = (int32_t)peak_decay[i] * 26 / 34;
            /* (1.7) the floor is a 16-bit meter: 2 px blocks with the scale always drawn, and the
             * peak a 1 px cap over it. A solid bar hid how loud the band was, not just that it was. */
            gw_seg_v(bx, 153, 9, 26, bh, 2, mono ? T_TEXT : heat_col(38 + (int32_t)i * 13), T_RAISE);
            if (peak_decay[i] > 1)
                /* (3.1) the cap is a coordinate of THIS meter, not of the page: it used to be
                 * drawn at 152 - ph, up to 27 px above the 153..178 field, so the peak marks
                 * floated over the scope. The field's top row is 153, its bottom 178. */
                cv_rect(bx, clamp(178 - ph, 153, 178), 9, 1, mono ? T_TEXT : T_ACCENT);
            /* an ambient scan tick: a UI marker, never presented as measured level */
            if (!song.playing && (uint32_t)((ui.frame / 6u + i) % 16u) == 0u)
                cv_rect(bx + 4, 181, 1, 2, T_DIM);
        }

        if (pass) {
            cv_rrect(12, 184, 72, 15, 3, T_THEME, vsf);
            cv_text_on(16, 183, &AF_S, "[HOME] EXIT", T_INK, T_THEME);
            cv_text_r(228, 183, &AF_S, "SPECTRUM / WAVEFORM", T_TEXT, vsf);
        }

        cv_oy = 0;
        cv_blit(0, (uint32_t)top_y);
    }
}

/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static __attribute__((noinline)) void graph_scope(uint16_t c)
{
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h == H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
    for (i = 0; i < SCOPE_N; i++) {
        scope_snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (scope_snap[i] > peak)
            peak = scope_snap[i];
        else if (-scope_snap[i] > peak)
            peak = -scope_snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (scope_snap[i - 1] < 0 && scope_snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - scope_snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)                                                   /* GAMMA / WRAITH / JIANT: each stroke as hot as it is loud */
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, ux.chamfer ?
                      heat_col(40 + (y > cy ? y - cy : cy - y) * 216 / a) : c, 2);
        py = y;
    }
}

/* SONG: the rows, each a section (the saved project A..D, sounds and steps) and its bars (song_chain.c) */
/* (JIANT) MSEQ: the 16 levels as bars (past LEN dim), the step KNOB 1 picked framed, the one playing in the accent, and
 * the level the matrix reads now (STEP, slewed) as a line */
static __attribute__((noinline)) void graph_mseq(const track_t *t, uint16_t c)
{
    uint32_t i, len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
    uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), now = song.playing && per ? ms_clock / per % len : 0xFFu;
    int32_t y0 = 92, hmax = 80;
    for (i = 0; i < 16u; i++) {
        int32_t x = 13 + (int32_t)i * 13, h = t->p[P_MS0 + i] * hmax / 127;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : ux.chamfer ?
                       heat_col(48 + t->p[P_MS0 + i] * 200 / 127) : c;
        cv_rrect(x, y0 - hmax, 10, hmax, 2, T_RAISE, T_SURF);
        if (h > 0)
            cv_rrect(x, y0 - (h < 3 ? 3 : h), 10, h < 3 ? 3 : h, 2, col, T_RAISE);
        if (i == ms_ui_step)
            cv_rect(x, y0 + 3, 10, 2, T_TEXT);
    }
    if (song.playing)
        cv_rect(13, y0 - t->ms_v * hmax / 32767, 205, 1, T_TEXT);
}
/* (0.6.22) ARRANGE / SEQ > SONG, after SLOOP's arranger in JIANT's colours: the song as a column of rows, each a section
 * chip (A..D, each its own heat), its bars as blocks (one a bar, up to 16; past 16 each a 16th of it) filling as they
 * play, the bars' count, a dot for its scene; under them + ADD (the rows used of CHAIN_ROWS) and the song's END (STOP,
 * x2..x8, LOOP). On the right the whole song as a strip, each row's height its share of the bars, the play head on it,
 * the bars in all. KNOB 1 the row, 2 its section (on END: STOP / x2 .. / LOOP), 3 its bars, 4 its scene */
static uint16_t sec_col(uint32_t s)
{
    static const uint8_t HJ[4] = {60, 115, 175, 230}, HO[4] = {120, 160, 200, 240};
    return heat_col(ux.chamfer ? HJ[s & 3u] : HO[s & 3u]);
}
static void song_chip(int32_t x, int32_t y, uint32_t s, int saved, int sel, uint16_t bg)
{
    char b[2] = {(char)('A' + (s & 3u)), 0};
    uint16_t c = sel ? T_INK : saved ? sec_col(s) : T_RAISE;   /* (on the selected row: ink, its letter the theme's) */
    cv_rrect(x, y, 18, 14, 3, c, bg);
    GFX_HOOK_ALIGN(x, y, x + 18, y + 14, AL_HV, "song chip letter centred");
    cv_text_on(x + (18 - text_w(&AF_S, b)) / 2, y + CAP_IN(S, 14), &AF_S, b, sel ? T_THEME : saved ? T_BG : T_DIM, c);
}
static __attribute__((noinline)) void graph_song(void)
{
    uint32_t end = song_end_row(), first, i, total = 0, played = 0;
    int32_t mh = (int32_t)graph_h() - 22, my = 4, acc = 0;
    if (!chain_config.count && ui.song_row == 0u) {
        panel_note("A 4 BARS > B 2 BARS", "[K2] ADD SECTION", "SECTIONS: SAVE > PROJECT");
        return;
    }
    first = ui.song_row > 3u ? ui.song_row - 3u : 0u;
    if (first + 7u > end + 1u)
        first = end + 1u > 7u ? end + 1u - 7u : 0u;
    for (i = first; i <= end && i < first + 7u; i++) {
        char b[16];
        int32_t y = LIST_Y(i - first), k, nb;
        int sel = i == ui.song_row, play = chain.running && i == chain.row;
        uint16_t bg = sel ? T_THEME : T_SURF, dim = sel ? T_INK : T_DIM;
        if (sel) cv_rrect(5, y, 182, 16, 4, T_THEME, T_SURF);
        if (i == end) {                                  /* the song's END */
            uint32_t e = SONG_END(&chain_config);
            cv_icon_on(9, y + 2, 12, ICON_LOOP, sel ? T_INK : T_MID, bg);
            cv_text_on(28, y + 1, &AF_S, "END", sel ? T_INK : T_MID, bg);
            if (e == SONG_FOREVER) str_cpy(b, "LOOP", sizeof b);
            else if (e >= 2u) { b[0] = 'x'; fmt_int(b + 1, (int32_t)e); }
            else str_cpy(b, "STOP", sizeof b);
            cv_text_on(60, y + 1, &AF_S, b, sel ? T_INK : T_TEXT, bg);
            if (e >= 2u && e != SONG_FOREVER && chain.running) {   /* (the time through) */
                fmt_int(b, chain.pass + 1); str_cpy(b + str_len(b), "/", 2); fmt_int(b + str_len(b), (int32_t)e);
                cv_text_on(110, y + 1, &AF_S, b, dim, bg);
            }
            break;
        }
        if (i == chain_config.count) {                   /* + ADD: the rows used of all */
            cv_text_on(28, y + 1, &AF_S, "+ ADD", sel ? T_INK : T_MID, bg);
            fmt_int(b, (int32_t)chain_config.count); str_cpy(b + str_len(b), "/", 2);
            fmt_int(b + str_len(b), (int32_t)CHAIN_ROWS);
            cv_text_on(110, y + 1, &AF_S, b, dim, bg);
            continue;
        }
        if (play) cv_icon_on(8, y + 2, 12, ICON_X_RIGHT, sel ? T_INK : T_ACCENT, bg);
        else { fmt_int(b, (int32_t)i + 1); cv_text_on(9, y + 1, &AF_S, b, sel ? T_INK : T_MID, bg); }
        {
            const chain_row_t *r = &chain_config.row[i];
            int saved = graph_project_used(r->slot);
            uint32_t bars = r->bars, done = play ? bars - chain.remaining : 0u;
            song_chip(28, y + 1, r->slot, saved, sel, bg);
            if (!saved && bars <= 9u)                    /* (a section with nothing stored: it plays silence) */
                cv_text_on(108, y + 1, &AF_S, "EMPTY", dim, bg);
            nb = bars > 16u ? 16 : (int32_t)bars;
            for (k = 0; k < nb; k++) {                   /* the bars as blocks, played ones full */
                uint32_t upto = bars > 16u ? (uint32_t)(k + 1) * bars / 16u : (uint32_t)k + 1u;
                uint16_t c = play && upto <= done ? (sel ? T_INK : T_TEXT) : saved ? sec_col(r->slot) : T_RAISE;
                cv_rect(52 + k * 6, y + 4, 5, 8, sel && !(play && upto <= done) ? (saved ? T_INK : T_DIM) : c);
            }
            if (song_idx.scene[song_cur][i].on)          /* the row has a scene: a dot */
                cv_rrect(150, y + 5, 6, 6, 3, sel ? T_INK : T_ACCENT, bg);
            fmt_int(b, (int32_t)bars);
            cv_text_r(182, y + 1, &AF_S, b, sel ? T_INK : saved ? T_TEXT : T_DIM, bg);
        }
    }
    for (i = 0; i < chain_config.count; i++) {          /* the song strip: the bars in all, played so far */
        total += chain_config.row[i].bars;
        if (chain.running && i < chain.row) played += chain_config.row[i].bars;
    }
    if (chain.running && chain.row < chain_config.count)
        played += chain_config.row[chain.row].bars - chain.remaining;
    if (!total)
        return;
    for (i = 0; i < chain_config.count; i++) {
        int32_t y0 = my + acc * mh / (int32_t)total, y1;
        acc += chain_config.row[i].bars;
        y1 = my + acc * mh / (int32_t)total;
        if (y1 - y0 < 2) y1 = y0 + 2;
        cv_rect(196, y0, 30, y1 - y0 - (y1 - y0 > 3 ? 1 : 0),
                graph_project_used(chain_config.row[i].slot) ? sec_col(chain_config.row[i].slot) : T_RAISE);
        if (i == ui.song_row)
            cv_rect(192, y0, 2, y1 - y0 - (y1 - y0 > 3 ? 1 : 0), T_THEME);
    }
    if (chain.running)
        cv_rect(194, my + (int32_t)(played * (uint32_t)mh / total), 34, 2, T_TEXT);   /* the play head */
    {
        char b[12];
        fmt_int(b, (int32_t)total);
        str_cpy(b + str_len(b), " BAR", 6);
        cv_text_r(230, my + mh + 4, &AF_S, b, T_MID, T_SURF);
    }
}

#include "ui_tech.c"                                /* (JIANT 0.5) the technical graphs (FX, DEST, VOICE, GLOBAL) */
/* (1.7) MASTER's page: four faders from the live G_* values, a scale and fixed dust, in the cards' language */
static __attribute__((noinline)) void graph_master(void)
{
    /* (3.1) the master VU's geometry. The four LVL / LIM / CPU / PK boxes that used to fill y 80..98
     * are gone (the owner asked for them out of MASTER), so the meters take that band: y 80 and 87, with
     * their caption on the graphs' caption line at y 66. The meter is 46 px below PUNCH's state word, so
     * the old collision with the "OFF" above it cannot happen; every value it draws is still
     * audio_out_peak(), the same ring the DAC writes. */
#define VU_X0 108
#define VU_W 108
#define VU_Y0 80
#define VU_H 6
    cv_oy = 0;                                       /* this panel is laid out on its own 0..100 scale */
    if (str_eq(cur_page()->title, "STEREO M/S")) {     /* M/S width: the two sides separate with M-WIDE */
        int32_t wd = clamp(song.g[G_MWIDTH], 0, 127), off = 8 + wd * 52 / 127, i;
        cv_rect(20, 50, 200, 1, T_LINE);
        for (i = 0; i < 4; i++) {
            cv_rect(100 - off - i * 4, 30, 2, 40, T_THEME);
            cv_rect(100 + off + i * 4, 30, 2, 40, T_ACCENT);
        }
        return;
    }
    int32_t i, py = 0, en = bg_energy(), clip = clamp(song.g[G_CLIP], 0, 127);
    int32_t g = 1024 + clip * 31, punch = clamp(song.g[G_PUNCH], 0, 127);
    int32_t duck = clamp(song.g[G_DUCK], 0, 127), drel = clamp(song.g[G_DREL], 0, 127);
    int32_t x0 = 14, y0 = 14, sw = 88, sh = 50, cy = y0 + sh / 2;
    /* CLIP: the actual transfer curve (the same softclip the DSP uses), the live level on it. */
    cv_rect(x0, y0 + sh, sw, 1, T_LINE);
    cv_rect(x0, y0, 1, sh, T_RAISE);
    for (i = 0; i <= sw; i += 8)
        cv_rect(x0 + i, y0, 1, 3, T_LINE);
    for (i = 0; i <= sw; i++) {
        int32_t x = (i - sw / 2) * 32767 / (sw / 2);
        int32_t y = cy - softclip(x * g >> 10) * (sh / 2) / 32767;
        if (i)
            cv_line_t(x0 + i - 1, py, x0 + i, y, heat_y(y < cy ? y : 2 * cy - y, y0, cy, T_THEME), 2);
        py = y;
    }
    for (i = 0; i <= sw; i += 6)                      /* unity, dotted */
        cv_rect(x0 + i, y0 + sh - i * sh / sw, 1, 1, T_DIM);
    if (en > 2) {
        int32_t dx = x0 + sw / 2 + en * (sw / 2) / 256;
        int32_t dy = cy - softclip(en * 128 * g >> 10) * (sh / 2) / 32767;
        cv_rrect(dx - 3, dy - 3, 7, 7, 3, T_TEXT, T_SURF);
    }
    cv_text_on(x0, y0 + sh + 3, &AF_S, "CLIP", T_DIM, T_SURF);
    cv_text_on(x0 + 42, y0 + sh + 3, &AF_S, en > 2 ? "LIVE" : "OUT", en > 2 ? T_ACCENT : T_DIM, T_SURF);

    /* PUNCH: the transient shaper's real envelope (punch_gain), not decoration. */
    cv_text_on(116, 5, &AF_S, "PUNCH", T_DIM, T_SURF);
    cv_rect(116, 43, 56, 1, T_LINE);
    py = 42;
    for (i = 0; i <= 56; i++) {
        int32_t y = 42 - (punch_gain((uint32_t)i * 255u / 56u, punch) >> 15) * 22 / 32767;
        if (i)
            cv_line_t(115 + i, py, 116 + i, y, heat_y(y, 20, 42, T_THEME), 2);
        py = y;
    }
    cv_text_on(116, 46, &AF_S, punch > 0 ? "HIT" : "OFF", punch > 0 ? T_ACCENT : T_DIM, T_SURF);

    /* DUCK: the sidechain gain the other tracks get right now (duck_g1), and REL below it. */
    cv_rrect(180, 16, 12, 38, 2, T_BG, T_RAISE);
    {
        int32_t gh = clamp((int32_t)duck_g1, 0, 32767) * 36 / 32767;
        if (gh > 0)
            cv_rrect(181, 53 - gh, 10, gh, 2, duck ? heat_col(60 + gh * 180 / 36) : T_DIM, T_BG);
    }
    cv_rect(180, 34, 12, 1, T_ACCENT);                /* unity gain reference */
    cv_text_on(180, 57, &AF_S, duck ? "ON" : "OFF", duck ? T_ACCENT : T_DIM, T_SURF);
    cv_rrect(198, 16, 10, 38, 2, T_BG, T_RAISE);
    cv_rrect(199, 16, 8, clamp(drel, 0, 127) * 38 / 127, 2, T_THEME, T_BG);
    /* OUT: the master's stereo VU. (2.2) It was two 7 x 38 px slivers at the far right -- the smallest
     * meter on the panel, and the one figure the page could not read at a glance. The band under the three
     * graphs was empty, so the meter lives there now: one long row per DAC channel (108 px of travel, the
     * scale's quarters marked), a peak cap that holds and falls, and the page's own OUT caption, which reads
     * CLIP while the lamp is latched. Every value is audio_out_peak(): the peak of the very ring the DAC
     * just wrote (audio.c), so a part panned right moves its own row and nothing here is derived from the
     * left channel, from a fader or from a shape drawn for the screen. */
    {
        static uint8_t vpx[2], vhold[2];             /* lit px and the held peak, per channel */
        static uint8_t clip_hold;                    /* frames the CLIP lamp has left */
        int32_t ch, k, live = 0;
        for (ch = 0; ch < 2; ch++) {
            int32_t pk = audio_out_peak((uint32_t)ch);
            int32_t px = meter_px(pk) * VU_W / (TS_MH - 2);
            int32_t y = VU_Y0 + ch * (VU_H + 1);
            cv_rect(VU_X0, y, VU_W, VU_H, T_BG);
            if (px > (int32_t)vpx[ch]) vpx[ch] = (uint8_t)px;       /* instant rise */
            else if (vpx[ch] > 0u) vpx[ch]--;                       /* a px a frame back down */
            if (px > (int32_t)vhold[ch]) vhold[ch] = (uint8_t)px;   /* the cap rises with the bar, */
            else if (vhold[ch] > 2u) vhold[ch]--;                   /* then holds and falls a px a frame */
            if (pk >= 32700) clip_hold = 90;                        /* full scale: the caption latches */
            if (vpx[ch]) {
                cv_rect(VU_X0, y, vpx[ch], VU_H, heat_col(70 + (int32_t)vpx[ch] * 150 / VU_W));
                live = 1;
            }
            for (k = 3; k > 0; k--)                                 /* the scale's quarters, over the bar */
                cv_rect(VU_X0 + k * VU_W / 4, y, 1, VU_H, T_BG);
            if (vhold[ch] > 2)
                cv_rect(VU_X0 + vhold[ch] - 2, y, 2, VU_H, T_TEXT);  /* the held peak */
        }
        if (clip_hold)
            clip_hold--;
        /* (3.1) one caption, not two LIVEs: CLIP's own state already reads LIVE at the left.
         * The meter says OUT, and lights while it moves. */
        cv_text_on(VU_X0, 66, &AF_S, clip_hold ? "CLIP" : "OUT",
                   clip_hold || live ? T_ACCENT : T_DIM, T_SURF);
    }

    /* (3.1) The LVL / LIM / CPU / PK band is gone by request: MASTER shows what it controls (CLIP,
     * PUNCH, DUCK, REL) and the output meters, not the CPU or the limiter's internal envelope. The
     * space it held is the VU's now, so the page has no empty hole and nothing moved off the panel. */
}

static __attribute__((noinline)) void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    {   /* (0.6.57) LOAD LITE / ECO: the panel's animation (the beings, the scope, LFO, the envelope's dot, a turn
         * redrawing a curve) at most every 50 / 70 ms (~20 / 14 frames a second, FULL: as fast as it changes), so the
         * main loop keeps its time for the keys, the knobs and the LCD. A new page, track or engine: at once */
        static uint32_t gr_ms, gr_key;
        uint32_t key = ui.page * 7u + song.sel * 131u + ui.home * 3u + TSEL->eng_req * 1031u;
        if (sys_load && !ui.force && key == gr_key && fm1_ms - gr_ms < (sys_load == LOAD_ECO ? 70u : 50u))
            return;                                  /* (graph_sig stays: drawn on a later frame) */
        gr_key = key;
        gr_ms = fm1_ms;
    }
    ui.graph_sig = sig;
    if (ui.home_viz) {                               /* (1.7) the visualizer owns the whole body: its two bands
                                                      * cover rows 26..201, so the page's panel under them (234 wide,
                                                      * graph_h tall) was painted and then covered whole -- a third
                                                      * of the frame paid for nothing. It starts on its own canvas */
        graph_home_viz(ACC);
        return;
    }
    cv_begin(240, graph_h(), T_BG);
    cv_plate_flat = 1;                               /* (FASE 3) flat: this panel paints its own ground below */
    cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_plate_flat = 0;
    graph_heat();                                    /* (5.1) the Solar Heat ground, as HOME draws it */
    /* (FASE 6) THE RAMP, ON EVERY PAGE. graph_heat() gave each panel the bezel's shade at the top and the
     * Solar Heat glow at its foot and left the middle untouched, so only HOME's views and (3.7) the
     * sequencer had a gradient running the whole height: the rest read as a flat plate with two edges lit.
     * The same primitive now paints that middle for every page at once -- one call in the shared chrome
     * instead of a copy per screen -- and SEQ keeps it through the same path rather than its own. */
    graph_ground_ramp();
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    if (drift_on())
        graph_drift();
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {                   /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else if (k == SK_STEPS) graph_steps(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        if (ui.home_view >= HV_OVERVIEW)             /* (3.0) HOME's other screens: OVERVIEW, SYSTEM */
            graph_home_view(c);
        else
            graph_ecosys();                          /* (JIANT 0.5: the system's beings, was the scope) */
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_TILT:
            graph_tilt(t, c);
            break;
        case GR_FILTER:                              /* (v2.3.4) the part's filter response, in depth */
            graph_filter(t, c);
            break;
        case GR_STEPS:
            graph_steps(t, c);
            break;
        case GR_COMP:
            graph_comp(t, c);
            break;
        case GR_MUTATE:
            graph_mutate();
            break;
        case GR_MORPH:
            graph_morph(t);
            break;
        case GR_DXIMG:                               /* (0.6.58) KIT 3: the colony; STEREO: IMAGE */
            if (DX_KIT3(pg)) {
                cv_oy = 0;
                graph_colony(t, -1);
            } else
                graph_dximg(t);
            break;
        case GR_FUGUE:
            graph_fugue(t);
            break;
        case GR_CHANCE:                              /* (ETAPA 6) the bank's 16 chances, drawn */
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_chance(t, c);
            break;
        case GR_ROLL:
        case GR_SHAPE:                               /* (0.6.85) STRETCH: the whole sequence, as the roll shows it */
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_SCALE:                               /* (6.1) the SCL layer's keyboard (ui_layer.c LY_SCL) */
            graph_scale(t, c);
            break;
        case GR_SCL_SCALE:                           /* (2.8) SCL's tools (ui_scl.c) */
            cv_oy = 0;
            graph_scl_scale(t, c);
            break;
        case GR_SCL_CHORD:
            cv_oy = 0;
            graph_scl_chord(t, c);
            break;
        case GR_SCL_TUNE:                             /* (FASE 7) SCL > TUNE (ui_scl.c) */
            cv_oy = 0;
            graph_scl_tune(t);
            break;
        case GR_SCL_PROG:
            cv_oy = 0;
            graph_scl_prog(t, c);
            break;
        case GR_SEQTOOLS:                            /* (6.2) SEQ > TOOLS: the edits and their undo (ui_seqtools.c) */
            cv_oy = 0;
            graph_seq_tools(t, c);
            break;
        case GR_FX_BUSES:                            /* (6.1) FX > BUSES: the routing console (ui_fxb.c) */
            cv_oy = 0;
            graph_fx_buses(t);
            break;
        case GR_FX:
            cv_oy = 0;
            graph_fx(t, c);
            break;
        case GR_DIAG:
            graph_diag(c);
            break;
        case GR_SLCR:
            graph_slicer(c);
            break;
        case GR_ARP:
            cv_oy = 0;                               /* the arp page is laid out on the whole 122 px panel */
            graph_arp(t, c);
            break;
        case GR_SLPIT:
            graph_slpitch(c);
            break;
        case GR_MOD:
            cv_oy = 0;
            graph_mod(t, c, 0);
            break;
        case GR_MODMAP:
            cv_oy = 0;
            graph_mod(t, c, 1);
            break;
        case GR_MACROS:
            cv_oy = 0;
            graph_macros(c);
            break;
        case GR_SCENES:                                /* (v1.8.5) the morph's rail */
            cv_oy = 0;
            graph_scenes();
            break;
        case GR_BROWSE:
            cv_oy = 0;
            graph_browse();
            break;
        case GR_SLOTS:
            cv_oy = 0;
            graph_slots();
            break;
        case GR_USER:
            cv_oy = 0;
            graph_user();
            break;
        case GR_MSEQ:
            graph_mseq(t, c);
            break;
        case GR_SONG:
            cv_oy = 0;
            graph_song();
            break;
        case GR_PATS:
            cv_oy = 0;
            graph_pats();
            break;
        case GR_MOTION:                              /* (FASE 5/9) the recorded movement, over the steps */
            cv_oy = 0;
            graph_motion();
            break;
        case GR_EVENTS:
            cv_oy = 0;
            graph_events();
            break;
        case GR_FMOP:                                /* (0.6.10) FM6's operators: the chart, the operator picked */
        case GR_FMOP2:
            if (fop_ok()) graph_fm6(t, c, fop_op % 6u);
            break;
        case GR_FMENV:                               /* .. its envelope */
            if (fop_ok()) graph_fm6env(t, c);
            break;
        case GR_DXSND:                               /* EDIT > SOUND: the kit's lanes, the sound edited singled out */
            cv_oy = 0;
            graph_colony(t, dx_ui_lane);
            break;
        case GR_TOOLS:                               /* (0.6.12) what each knob does, the one picked filled */
            cv_oy = 0;
            graph_tools();
            break;
        default:
            if (str_eq(pg->title, "MASTER") || str_eq(pg->title, "STEREO M/S")) {
                graph_master();
                break;
            }
            if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL) graph_wheel(t, c);
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_ADDITIVE) graph_additive(t, c);
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_LOFI) { cv_oy = 0; graph_lofi(t, c); }
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c, 6u);   /* EDIT 1 and 2 */
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_DRUM) {
                cv_oy = 0;
                graph_colony(t, -1);
            }
            else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_PHASE) {
                cv_oy = 0;
                graph_phase(t, c);                   /* (3.6) PHASE's EDIT 1 and EDIT 2 */
            }
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else if (pg->scope == SC_ENGINE) { cv_oy = 0; graph_being(t); }   /* (JIANT 0.5: the being, was the scope) */
            else {                                   /* the FX, DEST, VOICE, GLOBAL pages: their graphs (else the scope) */
                cv_oy = 0;
                if (!graph_tech(t, pg))
                    graph_scope(c);
            }
            break;
        }
    }
    cv_oy = 0;
    if (ui.preset_pop_ms && (uint32_t)(fm1_ms - ui.preset_pop_ms) < 1500u)
        graph_preset_pop_in_panel();                /* (v1.7.20) into the panel: one send per row */
    cv_blit(0, graph_y());
}
