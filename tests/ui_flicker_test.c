/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Flicker on the two screens the report names: HOME's visualizer and the preset browser, both while a knob turns.
 *
 * A flicker is something the eye sees and a single frame cannot: an element, or a whole band, that is *erased
 * and drawn again* -- on the glass the panel shows every write, so a strip painted with the background and then
 * with its content is a flash even though the frame that ends holds the right pixels. Two senses are measured:
 *
 *   WITHIN a frame  tests/ui_test.c's host_flash: a pixel written at least twice that ends the frame as it began
 *                   (a body cleared and redrawn the same frame, a band sent twice with different content).
 *   BETWEEN frames  this file: three consecutive frames, and a pixel that is identical in frame i-1 and i+1 (so
 *                   nothing about it is moving) but different in frame i -- the element went away for exactly one
 *                   frame and came back. Animations cannot be counted: they differ in i-1 and i+1 too.
 *
 * Both senses are run over a knob's detents, one frame each, on the screens the report points at. The animation
 * is off (UI_NO_DRIFT) and the clock is the harness's, so a run is deterministic: same pixels, same verdict.
 *
 *   cc -O1 -w -DUI_NO_DRIFT=1 '-DFELUCCA_VERSION="v1.7.19"' -Ibuild/gen -Ifirmware/src -Itests \
 *      -o build/host/ui_flicker tests/ui_flicker_test.c -lm
 *   (needs build/capture/ui_render_nomain.c: tests/run_tests.sh writes it from tests/ui_render.c)
 *
 * Exit 1 if a detent flashes inside a frame or leaves an element erased for one frame, with the box, the pixels
 * and their colours printed. build/flicker/<case>_<n>_<i>.ppm holds the three frames of a finding. */
#include "../build/capture/ui_render_nomain.c"

#define FL_W 240
#define FL_H 240
#define FL_N (FL_W * FL_H)

uint32_t fl_frame_id;
static uint16_t fl_a[FL_N], fl_b[FL_N], fl_c[FL_N];

/* the ink of a row band: pixels that are not the screen's background (the harness paints T_BG at 0,0) */
static int32_t fl_ink(const uint16_t *f, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0;
    uint16_t bg = f[0];
    for (y = y0; y <= y1; y++)
        for (x = 0; x < FL_W; x++)
            n += f[y * FL_W + x] != bg;
    return n;
}

static void fl_dump(const char *dir, const char *name, int k, int which)
{
    char p[160];
    FILE *f;
    const uint16_t *s = which == 0 ? fl_a : which == 1 ? fl_b : fl_c;
    int i;
    snprintf(p, sizeof p, "%s/%s_%d_%d.ppm", dir, name, k, which);
    if (!(f = fopen(p, "wb")))
        return;
    fprintf(f, "P6\n%d %d\n255\n", FL_W, FL_H);
    for (i = 0; i < FL_N; i++) {
        uint16_t c = s[i];
        unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31u) * 255u / 31u),
                                (unsigned char)(((c >> 5) & 63u) * 255u / 63u),
                                (unsigned char)((c & 31u) * 255u / 31u)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* a pixel erased for one frame, between two frames that agree on it */
typedef struct { int32_t n, x0, y0, x1, y1; } fl_box_t;

static void fl_box_add(fl_box_t *b, int32_t x, int32_t y)
{
    b->n++;
    if (x < b->x0) b->x0 = x;
    if (y < b->y0) b->y0 = y;
    if (x > b->x1) b->x1 = x;
    if (y > b->y1) b->y1 = y;
}

/* the blink between three frames: identical in a and c, different in b -- and a SHAPE, not a breath.
 *
 * A single pixel whose tone differs for one frame and returns is ambient: the visualizer's dust field mixes each
 * grain out of the page's tone by a sine of the frame, so a stationary grain at the turning point alternates two
 * tones and reads exactly like a blink at 1 px. A flicker is bigger than that: a pixel that goes with its
 * neighbour (an edge or a cap moving), or one that takes the local background and comes back (a dot erased).
 * Only those two count; the isolated tonal ones are counted apart and printed, never failed. */
static int32_t fl_blink(const uint16_t *a, const uint16_t *b, const uint16_t *c, fl_box_t *out, int32_t *ambient)
{
    static uint8_t ch[FL_N];
    int32_t x, y, n = 0;
    out->n = 0;
    out->x0 = out->y0 = 9999;
    out->x1 = out->y1 = -1;
    *ambient = 0;
    for (uint32_t i = 0; i < FL_N; i++)
        ch[i] = (uint8_t)(a[i] == c[i] && a[i] != b[i]);
    for (y = 0; y < FL_H; y++)
        for (x = 0; x < FL_W; x++) {
            uint32_t i = (uint32_t)(y * FL_W + x);
            int32_t nb, bg_here = 0;
            if (!ch[i])
                continue;
            nb = (x > 0 && ch[i - 1]) || (x < FL_W - 1 && ch[i + 1]) || (y > 0 && ch[i - FL_W]) ||
                 (y < FL_H - 1 && ch[i + FL_W]);
            if (!nb) {                                 /* alone: is b the background it sits in? */
                int32_t dx, dy, hist[8] = {0}, col[8], best = -1, bc = 0, nhi = 0, k;
                for (dy = -2; dy <= 2; dy++)
                    for (dx = -2; dx <= 2; dx++) {
                        int32_t sx = x + dx, sy = y + dy;
                        uint16_t v;
                        if (sx < 0 || sy < 0 || sx >= FL_W || sy >= FL_H)
                            continue;
                        v = a[sy * FL_W + sx];
                        for (k = 0; k < nhi; k++)
                            if (col[k] == v)
                                break;
                        if (k == nhi && nhi < 8) {
                            col[nhi] = v;
                            hist[nhi] = 1;
                            nhi++;
                        } else if (k < nhi)
                            hist[k]++;
                    }
                for (k = 0; k < nhi; k++)
                    if (hist[k] > bc) {
                        bc = hist[k];
                        best = col[k];
                    }
                bg_here = best >= 0 && b[i] == (uint16_t)best && a[i] != (uint16_t)best;
            }
            if (nb || bg_here) {
                fl_box_add(out, x, y);
                n++;
            } else
                (*ambient)++;
        }
    return n;
}

/* a whole band that loses most of its ink for one frame while its neighbours keep it: the strip erased and
 * drawn again. "Most" is 60 % of the smaller neighbour, which a value or a digit that really moved cannot do. */
static int32_t fl_band_blink(const uint16_t *a, const uint16_t *b, const uint16_t *c, int32_t *y0, int32_t *y1)
{
    static const int32_t BANDS[][2] = {{0, 25}, {26, 117}, {118, 201}, {202, 239}};
    uint32_t k;
    int32_t worst = 0, wy0 = -1, wy1 = -1;
    for (k = 0; k < sizeof BANDS / sizeof BANDS[0]; k++) {
        int32_t ia = fl_ink(a, BANDS[k][0], BANDS[k][1]), ib = fl_ink(b, BANDS[k][0], BANDS[k][1]),
                ic = fl_ink(c, BANDS[k][0], BANDS[k][1]);
        int32_t ref = ia < ic ? ia : ic;
        if (ref > 40 && ib * 100 < ref * 60) {
            int32_t loss = ref - ib;
            if (loss > worst) {
                worst = loss;
                wy0 = BANDS[k][0];
                wy1 = BANDS[k][1];
            }
        }
    }
    *y0 = wy0;
    *y1 = wy1;
    return worst;
}

/* one case: a screen set up, a knob turned one detent a frame, every frame kept for its neighbours */
typedef struct {
    const char *name;
    int screen;                 /* the setup() state */
    uint32_t role;              /* the encoder (panel.c EN_*) */
    int32_t step;               /* detents a frame */
    int frames;                 /* how many detents */
    int flash_ok;               /* a within-frame flash the screen legitimately has, if any */
} fl_case_t;

static int fl_run(const fl_case_t *cs, int verbose)
{
    int k, bad = 0;
    int32_t worst_in = 0, worst_out = 0;
    uint32_t worst_flash = 0;
    fl_box_t box;
    int32_t sled = 0, wy0 = 0, wy1 = 0;

    setup(cs->screen);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.msg_t = ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = ui.preset_pop_ms = 0;
    ui.menu = ui.confirm = ui.layer = 0;
    ui.force = 1;
    fm1_ms = 4000u;
    host_ticks = 6000000u;
    frames(300);                                   /* the screen settled, its holds and envelopes at rest */
    memcpy(fl_a, host_screen, sizeof fl_a);        /* frame 0: the state before the first detent */
    for (k = 1; k <= cs->frames; k++) {
        host_flash = host_flash_frames = 0;
        turn(cs->role, cs->step);                  /* one detent, one frame (ui_test.c) */
        fl_frame_id = (uint32_t)k * 2u;
        memcpy(fl_b, host_screen, sizeof fl_b);
        if (0) printf("  hot_t=%u col0=%08x\n", (unsigned)ui.hot_t, (unsigned)ui.col[0][0]);
        if (host_flash > worst_flash) worst_flash = host_flash;
        frame();                                   /* the frame after it: the panel's own cadence, no detent */
        memcpy(fl_c, host_screen, sizeof fl_c);
        {
            int32_t ambient = 0;
            int32_t n = fl_blink(fl_a, fl_b, fl_c, &box, &ambient);
            if (n > worst_out) {
                int32_t q;
                worst_out = n;
                for (q = 0; q < 3; q++) fl_dump("build/flicker", cs->name, k, q);
            }
            n = fl_band_blink(fl_a, fl_b, fl_c, &wy0, &wy1);
            if (n > sled) sled = n;
            if (verbose && box.n) {   /* which rows, and what colour each side is */
                int32_t yy, xx, shown = 0;
                for (yy = 0; yy < FL_H; yy++) {
                    int32_t c2 = 0;
                    for (xx = 0; xx < FL_W; xx++) {
                        uint32_t i2 = (uint32_t)(yy * FL_W + xx);
                        if (fl_a[i2] == fl_c[i2] && fl_a[i2] != fl_b[i2]) {
                            c2++;
                            if (shown < 4) {
                                printf("    px %d,%d: %04x -> %04x -> %04x\n", (int)xx, (int)yy,
                                       (unsigned)fl_a[i2], (unsigned)fl_b[i2], (unsigned)fl_c[i2]);
                                shown++;
                            }
                        }
                    }
                    if (c2) printf("    row %d: %d px\n", (int)yy, (int)c2);
                }
            }
            if (verbose && (box.n || n || ambient))
                printf("  %s detent %d: %d px erased for one frame (%d,%d-%d,%d), %d px of ambient tone, band loss %d\n",
                       cs->name, k, (int)box.n, (int)box.x0, (int)box.y0, (int)box.x1, (int)box.y1, (int)ambient, (int)n);
            if (host_flash > (uint32_t)cs->flash_ok && verbose)
                printf("  %s detent %d: %u px written twice in the frame (%d,%d-%d,%d)\n", cs->name, k,
                       (unsigned)host_flash, (int)host_fx0, (int)host_fy0, (int)host_fx1, (int)host_fy1);
        }
        memcpy(fl_a, fl_c, sizeof fl_a);           /* walk on: c is the next triple's first frame */
    }
    if (worst_flash > (uint32_t)cs->flash_ok) {
        printf("flicker: %s: %u px written twice inside one frame at a detent (allowed %d)\n", cs->name,
               (unsigned)worst_flash, cs->flash_ok);
        bad++;
    }
    if (worst_out) {
        printf("flicker: %s: %d px erased for one frame and back (build/flicker/%s_*)\n", cs->name, (int)worst_out,
               cs->name);
        bad++;
    }
    if (sled) {
        printf("flicker: %s: a band loses %d ink px for one frame (rows %d..%d)\n", cs->name, (int)sled, (int)wy0,
               (int)wy1);
        bad++;
    }
    printf("flicker: %-10s %d detents: %d px erased-and-back, %d px band loss, %u px double-written\n", cs->name,
           cs->frames, (int)worst_out, (int)sled, (unsigned)worst_flash);
    (void)worst_in;
    return bad;
}

int main(void)
{
    static const fl_case_t CASES[] = {
        {"home_viz", S_HOME_VIZ, EN_K1, 1, 8, 0},     /* the full-screen visualizer, KNOB 1 turned */
        {"home_eco", S_HOME, EN_K1, 1, 8, 0},         /* HOME's own bay, its first card's value */
        {"browse", S_PRESETS, EN_PRESET, 1, 8, 0},    /* the preset browser, PRESETS turned */
    };
    uint32_t k;
    int bad = 0;
    FILE *sink = fopen("/dev/null", "w");
    rep = sink;
    al_rep = sink;
    aud = sink;
    mkdir("build/flicker", 0777);                     /* (EEXIST is fine: the frames are overwritten) */
    for (k = 0; k < sizeof CASES / sizeof CASES[0]; k++)
        bad += fl_run(&CASES[k], 1);
    if (bad)
        printf("flicker: %d case(s) flash while a knob turns\n", bad);
    else
        printf("flicker: all passed\n");
    return bad ? 1 : 0;
}
