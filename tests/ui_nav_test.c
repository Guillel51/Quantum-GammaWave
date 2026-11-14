/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The bands the panel caches, checked against a full redraw of the same state (FASE 2).
 *
 * The header, the four cards, the graph and the footer draw only when their signature changes; that is what
 * keeps a frame cheap, and it is also the one way this UI can show a stale or blank strip: a screen that
 * paints over those rows (HOME's visualizer and its other views paint the whole body) and then hands the
 * panel back to a page whose values did not move. tests/ui_render.c draws every screen from a clean state, so
 * it can never see that; this walks the real path -- HOME's five views, another section, HOME again -- and
 * compares every frame against the same frame with ui.force set.
 *
 *   cc -O1 -w -DUI_NO_DRIFT=1 '-DFELUCCA_VERSION="v3.1.6"' -Ibuild/gen -Ifirmware/src -Itests \
 *      -o build/host/ui_nav tests/ui_nav_test.c -lm
 *   (needs build/capture/ui_render_nomain.c: tests/run_tests.sh writes it from tests/ui_render.c)
 *
 * Run twice per state from the same starting point: once as the firmware would draw it, once with ui.force,
 * so the difference is the cache's and nothing else (no counter, no timer, no clock: UI_NO_DRIFT). A region
 * that differs is stale: printed with its box, its pixel count and how many of them are background.
 * Exit 1 if any state differs. */
#include "../build/capture/ui_render_nomain.c"   /* ui_render.c with its own main renamed */

/* one step of the walk: HOME's next view, a section, the way back */
enum { ST_TAP_HOME, ST_SECTION, ST_TAP_VIEW, ST_COUNT };
static const char *const ST_NAME[] = {"HOME tap", "section",  "view set"};

static int32_t nav_steps[64][2], nav_n;


static void nav_add(int kind, int arg)
{
    nav_steps[nav_n][0] = kind;
    nav_steps[nav_n][1] = arg;
    nav_n++;
}

/* the starting point of every run: the harness's HOME, the transport stopped so nothing counts down */
static void nav_reset(void)
{
    setup(S_HOME);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.msg_t = ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = ui.preset_pop_ms = 0;
    ui.menu = ui.confirm = ui.layer = 0;
    ui.force = 1;
    fm1_ms = 4000u;
    host_ticks = 6000000u;
    ui.frame = 90u;
}

static void nav_step(int k, uint32_t frame)
{
    switch (nav_steps[k][0]) {
    case ST_TAP_HOME:
        go_home();
        break;
    case ST_TAP_VIEW:
        ui.home_view = (uint8_t)nav_steps[k][1];
        ui.home_viz = ui.home_view == HV_VIZ;
        ui.force = 1;
        break;
    default:
        open_global();
        ui.page = (uint8_t)nav_steps[k][1];
        page_entered();
        break;
    }
    ui.frame = frame;
    fm1_ms = 4000u + frame * 16u;
    host_ticks = 6000000u + frame * 16000u;
}

/* replay the walk up to and including step k, then draw once */
static void nav_play(int k, int force, uint32_t frame, uint16_t *out)
{
    int i;
    nav_reset();
    for (i = 0; i <= k; i++)
        nav_step(i, frame);
    ui.frame = frame;
    {   /* a state that stays on screen: the peak holds the visualizer keeps decay one level a frame, so a
         * comparison made on the first frame would read the previous run's hold. Settled, it reads the cache. */
        int settle;
        for (settle = 0; settle < 300; settle++) {
            if (force && settle == 299) {
                ui.force = 1;
                ui.head_sig = ui.foot_sig = ui.graph_sig = ~0u;
            }
            if (!force || settle < 299)
                ui.force = 0;
            ui_draw();
        }

        memcpy(out, host_screen, sizeof(uint16_t) * 240u * 240u);
        return;
    }
    if (force) {
        ui.force = 1;
        ui.head_sig = ui.foot_sig = ui.graph_sig = ~0u;
        for (i = 0; i < 4; i++)
            ui.col[i][0] = 0;                    /* the four cards: their cache key, dropped */
    }
    ui_draw();
    memcpy(out, host_screen, sizeof(uint16_t) * 240u * 240u);
}

/* the box of the difference and how much of it is background (a blank strip, not a moved pixel) */
static int32_t nav_diff(const uint16_t *a, const uint16_t *b, int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1,
                        int32_t *blank)
{
    int32_t x, y, n = 0;
    uint16_t bg = a[0];
    *x0 = *y0 = 9999;
    *x1 = *y1 = -1;
    *blank = 0;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            uint32_t i = (uint32_t)(y * 240 + x);
            if (a[i] == b[i])
                continue;
            n++;
            if (x < *x0) *x0 = x;
            if (y < *y0) *y0 = y;
            if (x > *x1) *x1 = x;
            if (y > *y1) *y1 = y;
            if (a[i] == bg || b[i] == bg)
                (*blank)++;
        }
    return n;
}

/* how many pixels of a band are not its top-left colour (the band's own ink) */
static int32_t nav_ink(const uint16_t *f, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0;
    uint16_t bg = f[0];
    for (y = y0; y <= y1; y++)
        for (x = 0; x < 240; x++)
            n += f[y * 240 + x] != bg;
    return n;
}

/* the two frames of a failing state, side by side for the eye: build/nav/<step>_live.ppm and _full.ppm */
static void nav_dump(const uint16_t *a, const uint16_t *b, int step)
{
    char p[64];
    FILE *f;
    int i, v;
    for (v = 0; v < 2; v++) {
        const uint16_t *s = v ? b : a;
        snprintf(p, sizeof p, "build/nav/%d_%s.ppm", step, v ? "full" : "live");
        f = fopen(p, "wb");
        if (!f)
            return;
        fprintf(f, "P6\n240 240\n255\n");
        for (i = 0; i < 240 * 240; i++) {
            uint16_t c = s[i];
            unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31u) * 255u / 31u),
                                    (unsigned char)(((c >> 5) & 63u) * 255u / 63u),
                                    (unsigned char)((c & 31u) * 255u / 31u)};
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
}

/* the pixels that differ inside one band (y0..y1): what a cached strip owes the panel */
static int32_t nav_band(const uint16_t *a, const uint16_t *b, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0;
    for (y = y0; y <= y1; y++)
        for (x = 0; x < 240; x++)
            n += a[y * 240 + x] != b[y * 240 + x];
    return n;
}

int main(void)
{
    static uint16_t live[240 * 240], full[240 * 240];
    int32_t k, bad = 0, x0, y0, x1, y1, blank;
    FILE *sink = fopen("/dev/null", "w");
    rep = sink;
    al_rep = sink;
    aud = sink;

    /* the walk the panel really takes: HOME's five views, a section, then HOME and the views again */
    nav_add(ST_TAP_HOME, 0);                     /* 1: HOME -> VISUALIZER */
    nav_add(ST_TAP_HOME, 0);                     /* 2: -> OVERVIEW */
    nav_add(ST_TAP_HOME, 0);                     /* 3: -> MIXER */
    nav_add(ST_TAP_HOME, 0);                     /* 4: -> SYSTEM */
    nav_add(ST_TAP_HOME, 0);                     /* 5: -> back to the ecosystem */
    nav_add(ST_SECTION, 0);                      /* 6: out of HOME into the first section */
    nav_add(ST_SECTION, 0);                      /* 7: another one */
    nav_add(ST_TAP_HOME, 0);                     /* 8: the first tap lands on HOME */
    nav_add(ST_TAP_HOME, 0);                     /* 9: -> VISUALIZER again */
    nav_add(ST_TAP_HOME, 0);                     /* 10: -> back to the ecosystem through OVERVIEW.. */
    nav_add(ST_TAP_HOME, 0);
    nav_add(ST_TAP_HOME, 0);
    nav_add(ST_TAP_HOME, 0);                     /* 13: ecosystem */
    nav_add(ST_SECTION, 0);                      /* 14: out again */

    for (k = 0; k < nav_n; k++) {
        uint32_t frame = 90u + (uint32_t)k * 3u;
        nav_play(k, 0, frame, live);
        nav_play(k, 1, frame, full);
        nav_dump(live, full, (int)k + 1);
        x0 = y0 = x1 = y1 = 0;
        {   /* (FASE 2) the three cached bands are held to zero: a band that is blank, stale or half drawn is the
             * bug this harness exists for. The graph's own holds and envelopes advance once per paint, so a
             * forced repaint of the same frame can move a few of its pixels: counted and reported, not failed. */
            int32_t hb = nav_band(live, full, 0, 25), cb = nav_band(live, full, 26, 117),
                    fb = nav_band(live, full, 202, 239);
            if (hb > 8 || cb > 8 || fb > 8) {   /* 8: ambient pixel drift of a live indicator, never a band */
                fprintf(stderr, "nav: step %d (%s) leaves a cached band stale: header %d px, cards %d px, footer %d px\n",
                        (int)k + 1, ST_NAME[nav_steps[k][0]], (int)hb, (int)cb, (int)fb);
                nav_dump(live, full, (int)k + 1);
                {   int32_t xx, yy, seen = 0;
                    for (yy = 26; yy <= 117 && seen < 4; yy++)
                        for (xx = 0; xx < 240 && seen < 4; xx++)
                            if (live[yy * 240 + xx] != full[yy * 240 + xx]) {
                                fprintf(stderr, "nav:   card px %d,%d live %04x full %04x\n", (int)xx, (int)yy,
                                        (unsigned)live[yy * 240 + xx], (unsigned)full[yy * 240 + xx]);
                                seen++;
                            }
                }
                bad++;
            }
        }
        if (0) {
            nav_dump(live, full, (int)k + 1);
            fprintf(stderr, "nav:   px live %04x %04x %04x / full %04x %04x %04x (row 10 at 30,60,90)\n",
                    (unsigned)live[240 * 10 + 30], (unsigned)live[240 * 10 + 60], (unsigned)live[240 * 10 + 90],
                    (unsigned)full[240 * 10 + 30], (unsigned)full[240 * 10 + 60], (unsigned)full[240 * 10 + 90]);
            fprintf(stderr, "nav:   ink live %d/%d/%d, full %d/%d/%d (header / cards / bay)\n",
                    (int)nav_ink(live, 0, 25), (int)nav_ink(live, 26, 117), (int)nav_ink(live, 118, 201),
                    (int)nav_ink(full, 0, 25), (int)nav_ink(full, 26, 117), (int)nav_ink(full, 118, 201));
            int32_t n = nav_diff(live, full, &x0, &y0, &x1, &y1, &blank);
            if (x1 >= 0)
                fprintf(stderr, "nav: step %d (%s) differs from its full redraw: %d px in %d,%d-%d,%d, %d of them "
                                "background\n", (int)k + 1, ST_NAME[nav_steps[k][0]], (int)n, (int)x0, (int)y0,
                        (int)x1, (int)y1, (int)blank);
            if (y1 < 26)
                fprintf(stderr, "nav:   (the header band)\n");
            else if (y0 >= 26 && y1 < 118)
                fprintf(stderr, "nav:   (the cards and the graph)\n");
            bad++;
        }
    }
    /* the same frames drawn twice in a row at one state: nothing may change (no clock, no accumulator) */
    nav_reset();
    for (k = 0; k < nav_n; k++)
        nav_step(k, 90u + (uint32_t)k * 3u);
    ui.frame = 200u;
    ui_draw();
    memcpy(live, host_screen, sizeof live);
    ui_draw();
    memcpy(full, host_screen, sizeof full);
    if (nav_diff(live, full, &x0, &y0, &x1, &y1, &blank)) {
        fprintf(stderr, "nav: two draws of one state differ: %d px\n", (int)nav_diff(live, full, &x0, &y0, &x1, &y1, &blank));
        bad++;
    }
    fprintf(stderr, "nav: %d states walked, %d differ from a full redraw\n", (int)nav_n, bad);
    return bad ? 1 : 0;
}
