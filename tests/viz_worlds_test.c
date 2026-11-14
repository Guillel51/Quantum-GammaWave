/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The visualizer's two deep worlds (QA). HOME > VISUALIZER keeps a banded giant on the left of the scope and a
 * rocky world on the right, both behind the measured data. They are meant to answer the music: the giant reads
 * tracks 1-3 and the rock tracks 4-6, so a half that plays lifts that world's lit face, widens its atmosphere
 * and brings out its own event. This test draws the field with one half silent and then loud, and demands that
 * world moved -- and that the OTHER one did not, which is what "each reads its own half" means.
 *   cc -O1 -w ... -o build/host/viz_worlds tests/viz_worlds_test.c -lm
 * It calls gw_viz_world() into a canvas and blits it, exactly as graph_home_viz does, instead of going through
 * a whole page draw: the engine runs inside a page draw and would overwrite the levels being tested.
 * Host framebuffer only: not a device and not the emulator. Exit 1 on any assertion. */
#include "../build/capture/ui_render_nomain.c"

/* canvas rows: the canvas is 240 x 88 and is drawn with cv_oy = -120, so screen y 120..207 is row 0..87 */
#define GX0 14
#define GY0 4
#define GX1 56
#define GY1 44
#define RX0 198
#define RY0 6
#define RX1 232
#define RY1 40
#define CW 240
#define CH 88

static int32_t diff_box(const uint16_t *a, const uint16_t *b, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t x, y, n = 0;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            n += a[y * CW + x] != b[y * CW + x];
    return n;
}

/* how many tones the body holds: a two-tone disc has two, a world with belts, a rim and its marks has many */
static int32_t tones_box(const uint16_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint16_t seen[64];
    int32_t x, y, n = 0, k;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            uint16_t c = fb[y * CW + x];
            for (k = 0; k < n; k++)
                if (seen[k] == c)
                    break;
            if (k == n && n < 64)
                seen[n++] = c;
        }
    return n;
}

/* one field, two levels: lo plays tracks 1-3, hi plays tracks 4-6 (0..127, the scale gw_viz_deep builds) */
static void field(uint16_t *dst, int32_t lo, int32_t hi)
{
    int32_t i;
    cv_begin(CW, CH, T_BG);
    cv_oy = -120;                                  /* screen row = canvas row + 120 */
    gw_viz_world(T_BG, 34, 143, 15, 20000, 1, 150, lo, 0);
    gw_viz_world(T_BG, 214, 141, 10, 4000, 0, 62, hi, 0);
    cv_oy = 0;
    for (i = 0; i < CW * CH; i++)                  /* the canvas itself: no blit, no LCD in the way */
        dst[i] = cv_px[i];
}

static uint16_t quiet[CW * CH], loud_lo[CW * CH], loud_hi[CW * CH];

int main(void)
{
    FILE *sink = fopen("/dev/null", "w");
    int32_t dg, dr;
    int bad = 0;

    rep = sink;
    al_rep = sink;
    aud = sink;
    pal(0);                                        /* a palette: an unset theme is black on black */

    field(quiet, 0, 0);                            /* nothing playing */
    field(loud_lo, 120, 0);                        /* tracks 1-3 loud, 4-6 silent */
    field(loud_hi, 0, 120);                        /* tracks 4-6 loud, 1-3 silent */

    /* each world is there, and it is a world: more than a flat disc of two tones */
    if (tones_box(quiet, GX0, GY0, GX1, GY1) < 6) {
        fprintf(stderr, "worlds: the giant holds %d tones (< 6): no surface\n",
                (int)tones_box(quiet, GX0, GY0, GX1, GY1));
        bad++;
    }
    if (tones_box(quiet, RX0, RY0, RX1, RY1) < 5) {
        fprintf(stderr, "worlds: the rock holds %d tones (< 5): no surface\n",
                (int)tones_box(quiet, RX0, RY0, RX1, RY1));
        bad++;
    }

    /* tracks 1-3 play: the giant answers, the rock does not */
    dg = diff_box(quiet, loud_lo, GX0, GY0, GX1, GY1);
    dr = diff_box(quiet, loud_lo, RX0, RY0, RX1, RY1);
    fprintf(stderr, "worlds: tracks 1-3 loud -> giant %d px changed, rock %d px changed\n", (int)dg, (int)dr);
    if (dg < 150) {
        fprintf(stderr, "worlds: the giant ignores its half (%d px, wanted >= 150)\n", (int)dg);
        bad++;
    }
    if (dr) {
        fprintf(stderr, "worlds: the rock answered tracks 1-3 (%d px, wanted 0)\n", (int)dr);
        bad++;
    }

    /* tracks 4-6 play: the other way round */
    dg = diff_box(quiet, loud_hi, GX0, GY0, GX1, GY1);
    dr = diff_box(quiet, loud_hi, RX0, RY0, RX1, RY1);
    fprintf(stderr, "worlds: tracks 4-6 loud -> giant %d px changed, rock %d px changed\n", (int)dg, (int)dr);
    if (dr < 40) {
        fprintf(stderr, "worlds: the rock ignores its half (%d px, want >= 40)\n", (int)dr);
        bad++;
    }
    if (dg) {
        fprintf(stderr, "worlds: the giant answered tracks 4-6 (%d px, wanted 0)\n", (int)dg);
        bad++;
    }

    fprintf(stderr, "worlds: %s\n", bad ? "FAILED" : "ok: each world reads its own half of the tracks");
    return bad ? 1 : 0;
}
