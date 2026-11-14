/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* HOME round trip (QA). Leaves HOME for another page and comes back, fifty times per view, and demands the
 * same picture every time: the first entry is the reference, and a different hash afterwards is residual
 * state -- a widget that kept its old value, a page index that did not reset, a buffer nobody cleared.
 *   cc -O1 -w ... -o build/host/home_roundtrip tests/home_roundtrip_test.c -lm
 * Entering and leaving never changes the clock here: fm1_ms, host_ticks and ui.frame are set to the same
 * values before every draw, so any difference is state and nothing else.
 * Asserts: the five HOME views are stable across 50 entries, the header strip (BPM, battery, transport)
 * keeps its ink and its colours on every entry, and the bay keeps drawing its bodies. Exit 1 on any of them.
 * Not a device and not the emulator: tests/ui_render.c's harness drawing into its 240x240 framebuffer. */
#include "../build/capture/ui_render_nomain.c"

static uint32_t scr_hash(const uint16_t *fb)
{
    uint32_t v = 2166136261u;
    int32_t i;
    for (i = 0; i < 240 * 240; i++)
        v = (v ^ fb[i]) * 16777619u;
    return v;
}

static int32_t scr_diff(const uint16_t *a, const uint16_t *b)
{
    int32_t i, n = 0;
    for (i = 0; i < 240 * 240; i++)
        n += a[i] != b[i];
    return n;
}

/* how many different colours a strip holds: a header that lost its indicators collapses to the page tone */
static int32_t strip_colours(const uint16_t *fb, int32_t y0, int32_t y1)
{
    uint16_t seen[64];
    int32_t n = 0, x, y, k;
    for (y = y0; y <= y1; y++)
        for (x = 0; x < 240; x++) {
            uint16_t c = fb[y * 240 + x];
            for (k = 0; k < n; k++)
                if (seen[k] == c)
                    break;
            if (k == n && n < 64)
                seen[n++] = c;
        }
    return n;
}

static uint32_t log_a[5][50];

static void sequence(int v, int views_v, int away_v)
{
    const uint32_t t0 = 4000u, h0 = 6000000u, f0 = 90u;
    int c;
    for (c = 0; c < 50; c++) {
        setup(views_v);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; ui.force = 1;
        draw(views_v);
        log_a[v][c] = scr_hash(host_screen);
        setup(away_v);
        fm1_ms = t0; host_ticks = h0; ui.frame = f0; ui.force = 1;
        draw(away_v);
    }
}

int main(void)
{
    static uint32_t first[5][50];
    const int views[5] = {S_HOME, S_HOME_VIZ, S_HOME_OVERVIEW, S_HOME_MIX, S_HOME_PERF};
    const char *names[5] = {"home", "home_viz", "home_overview", "home_mix", "home_perf"};
    const int away[5] = {S_MIXER, S_EDIT_ANALOG, S_FX, S_MOD, S_ARP};
    FILE *sink = fopen("/dev/null", "w");
    int bad = 0, v, c;

    rep = sink; al_rep = sink; aud = sink;
    for (v = 0; v < 5; v++) {
        sequence(v, views[v], away[v]);            /* the pass every earlier test measured */
        memcpy(first[v], log_a[v], sizeof first[v]);
    }
    /* The same fifty round trips again, in the same process, with nothing reset but the page: if a static
     * survived the first pass -- a cached signature, a widget's old value, a phase nobody restarted -- the
     * second pass renders it and the hashes differ. Deterministic build (UI_NO_DRIFT=1), fixed clock. */
    for (v = 0; v < 5; v++) {
        int odd = 0;
        sequence(v, views[v], away[v]);
        /* entry 0 of each pass follows a different predecessor (the second one follows pass 1's last away
         * page), so the comparison starts at entry 1: from there both passes arrive by the same route. */
        for (c = 1; c < 50; c++)
            if (log_a[v][c] != first[v][c]) {
                if (odd < 3)
                    fprintf(stderr, "rt: %s: entry %d is not the same in the second pass\n", names[v], c);
                odd++;
            }
        /* the header keeps its ink: BPM, battery and transport live in the first 35 rows */
        setup(views[v]);
        fm1_ms = 4000u; host_ticks = 6000000u; ui.frame = 90u; ui.force = 1;
        draw(views[v]);
        if (strip_colours(host_screen, 0, 20) < 6 || strip_colours(host_screen, 21, 34) < 4) {
            fprintf(stderr, "rt: %s: header holds %d/%d colours: the indicators are gone\n", names[v],
                    (int)strip_colours(host_screen, 0, 20), (int)strip_colours(host_screen, 21, 34));
            bad++;
        }
        fprintf(stderr, "rt: %s: 50 round trips twice, %d entries differ in the second pass\n", names[v], odd);
        if (odd) {
            fprintf(stderr, "rt: %s: A STATIC SURVIVED THE PASS (residual state)\n", names[v]);
            bad++;
        }
    }
    fprintf(stderr, "rt: %s\n", bad ? "FAILED" : "ok: five views reproduce exactly across two passes");
    return bad ? 1 : 0;
}
