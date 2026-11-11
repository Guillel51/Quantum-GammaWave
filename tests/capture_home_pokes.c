/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host capture of HOME's touch reactions (3.5). Built with UI_NO_DRIFT=0, the gestures frozen in the
 * deterministic pixel builds:
 *   cc -O1 -w -DUI_NO_DRIFT=0 ... -o build/host/capture_pokes tests/capture_home_pokes.c -lm
 *   build/host/capture_pokes OUTDIR        (OUTDIR/ppm/QUANTUM_pokeN.ppm and _rest.ppm)
 * Not a device and not the emulator: tests/ui_render.c's own harness drawing into its 240x240 framebuffer,
 * i.e. the same code the firmware links. The picture is compared against a CONTROL drawn at exactly the same
 * frames with no touch, so the pixels counted are the reaction's and not the animation's.
 * Asserts: every body's touch changes its bay, the five are not one animation in five colours, a tap off the
 * bay does nothing, and a second touch restarts the gesture at once. Exit 1 if any of those fails. */
#include "../build/capture/ui_render_nomain.c"   /* ui_render.c with its own main renamed */

static void hash_rect(uint32_t *h, const uint16_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t v = 2166136261u;
    int32_t x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            v = (v ^ fb[y * 240 + x]) * 16777619u;
    *h = v;
}

static int32_t poke_find(int32_t p, int32_t *hx, int32_t *hy)
{
    int32_t x, y;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++)
            if (gw_planet_at(x, y) == p) { *hx = x; *hy = y; return 0; }
    return -1;
}

/* HOME's bay, measured from the bodies HOME drew: nothing here assumes the layout */
static void poke_bay(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
    int32_t i;
    *x0 = *y0 = 239; *x1 = *y1 = 0;
    for (i = 0; i < 5; i++) {
        int32_t r = (int32_t)pl_sr[i] + 8;
        if (pl_sx[i] - r < *x0) *x0 = pl_sx[i] - r;
        if (pl_sy[i] - r < *y0) *y0 = pl_sy[i] - r;
        if (pl_sx[i] + r > *x1) *x1 = pl_sx[i] + r;
        if (pl_sy[i] + r > *y1) *y1 = pl_sy[i] + r;
    }
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > 239) *x1 = 239;
    if (*y1 > 239) *y1 = 239;
}

static int32_t poke_diff(const uint16_t *a, const uint16_t *b, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t n = 0, x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            n += a[y * 240 + x] != b[y * 240 + x];
    return n;
}

static void poke_reset(void)                        /* nothing of the gesture or the pool survives a pass */
{
    int i;
    memset(vz_np, 0, sizeof vz_np);
    vz_np_next = 0;
    for (i = 0; i < 5; i++) { pl_poke_on[i] = 0; pl_poke_t[i] = 0; pl_flash[i] = 0; }
}

static void poke_frames(uint32_t t0, uint32_t h0, uint32_t f0, int n)
{
    int f;
    fm1_ms = t0; host_ticks = h0; ui.frame = f0;
    ui.force = 1;                                   /* both passes redraw whole: the only difference is the touch */
    for (f = 0; f < n; f++) {
        ui.frame++;
        fm1_ms += 16u;
        host_ticks += 16000u;
        ui_draw();
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/capture";
    static uint16_t ctl[5][240 * 240], got[5][240 * 240];
    uint32_t t0 = 4000u, h0 = 6000000u, f0 = 90u;
    int32_t p, x, y, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int bad = 0, nd[5];
    char nm[32];
    FILE *sink = fopen("/dev/null", "w");
    rep = sink; al_rep = sink; aud = sink;

    for (p = 0; p < 5; p++) {
        int32_t hx = -1, hy = -1, miss;
        poke_reset(); setup(S_HOME); pal(UI_QUANTUM_INDEX);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; draw(S_HOME);
        if (p == 0) poke_bay(&x0, &y0, &x1, &y1);
        if (poke_find(p, &hx, &hy) < 0) { fprintf(stderr, "poke: body %d is not on screen\n", (int)p); return 1; }
        poke_frames(t0, h0, f0, 6);                  /* control: the same frames, no touch */
        memcpy(ctl[p], host_screen, sizeof ctl[p]);
        poke_reset(); setup(S_HOME); pal(UI_QUANTUM_INDEX);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; draw(S_HOME);
        ui_tap(hx, hy);
        poke_frames(t0, h0, f0, 6);                  /* the touch, at the same frames */
        memcpy(got[p], host_screen, sizeof got[p]);
        nd[p] = poke_diff(ctl[p], got[p], 0, 0, 239, 239);
        snprintf(nm, sizeof nm, "poke%d", (int)p);
        write_ppm(dir, "QUANTUM", nm);
        if (nd[p] < 6) { fprintf(stderr, "poke: body %d's touch changed %d px\n", (int)p, (int)nd[p]); bad++; }
        fprintf(stderr, "poke: body %d at %d,%d: %d px answer (bay %d..%d x %d..%d)\n",
                (int)p, hx, hy, (int)nd[p], x0, x1, y0, y1);
        /* a miss is nothing: the same frames with a tap off the bay must be the control, pixel for pixel */
        poke_reset(); setup(S_HOME); pal(UI_QUANTUM_INDEX);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; draw(S_HOME);
        ui_tap(150, 40);
        poke_frames(t0, h0, f0, 6);
        miss = poke_diff(ctl[p], host_screen, 0, 0, 239, 239);
        if (miss) { fprintf(stderr, "poke: a tap off the bay changed %d px\n", (int)miss); bad++; }
        /* interrupted: a second touch must restart the gesture, not wait for the first to end */
        poke_reset(); setup(S_HOME); pal(UI_QUANTUM_INDEX);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; draw(S_HOME);
        ui_tap(hx, hy);
        poke_frames(t0, h0, f0, 12);
        if (poke_find(p, &hx, &hy) < 0) { fprintf(stderr, "poke: body %d left the bay\n", (int)p); bad++; continue; }
        ui_tap(hx, hy);
        ui.frame++;
        if (pl_age(p, POKE_L[p]) > 2u) { fprintf(stderr, "poke: body %d did not restart\n", (int)p); bad++; }
    }
    {   /* the five are not one gesture in five colours: their answers differ in size and in their own pixels */
        int32_t a, b;
        for (a = 0; a < 5; a++)
            for (b = a + 1; b < 5; b++)
                if (nd[a] == nd[b]) {
                    int d = 0;
                    for (y = y0; y <= y1; y++)
                        for (x = x0; x <= x1; x++)
                            d += (ctl[a][y * 240 + x] != got[a][y * 240 + x]) !=
                                 (ctl[b][y * 240 + x] != got[b][y * 240 + x]);
                    if (d < 8) { fprintf(stderr, "poke: bodies %d and %d answer alike (%d px)\n", a, b, d); bad++; }
                }
    }
    fprintf(stderr, "poke: %s (%d %d %d %d %d px)\n", bad ? "FAILED" : "ok: five distinct, restartable answers",
            nd[0], nd[1], nd[2], nd[3], nd[4]);
    return bad ? 1 : 0;
}
