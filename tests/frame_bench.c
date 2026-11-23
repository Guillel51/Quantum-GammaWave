/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A bench for what the main loop does when nothing is happening, so a change to the per-frame
 * housekeeping (ui_input's scans, ui_leds, ui_draw's invalidation) can be compared with a number
 * instead of a feeling.
 *
 *   tests/frame_bench [frames]
 *
 * Two scenes, each on a screen that has settled (the UI's own holds and envelopes at rest):
 *   input   ui_input() alone: the edges, the encoders, the per-frame housekeeping ui_input_frame runs
 *           before it returns -- what the main loop calls again and again inside its 15 ms wait.
 *   frame   ui_input() + ui_leds() + ui_draw(): the whole loop body.
 *   keys    the frame with a key of the matrix held down (the debounce early-return path).
 *
 * Prints CPU time per call (ns) and the wall clock beside it. This is the C these functions run on
 * this host, x86-64: it is not a cycle count of the AC791N and not a reading from an FM-1. Built by
 * the suite, not run by it: it measures, it does not approve.
 *   (needs build/capture/ui_render_nomain.c: tests/run_tests.sh writes it from tests/ui_render.c)
 */
#include "../build/capture/ui_render_nomain.c"

static double fb_cpu(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 20000u, i;
    double c0, w0;

    printf("frame_bench: host x86, the main loop with nothing to do (%u calls)\n", (unsigned)n);
    {
        FILE *sink = fopen("/dev/null", "w");
        rep = sink;
        al_rep = sink;
        aud = sink;
    }
    if (!rep)
        rep = stderr;
    state();
    setup(S_PRESETS);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.msg_t = ui.msg2[0] = 0;
    ui.preset_pop_ms = 0;
    ui.menu = ui.confirm = ui.layer = 0;
    ui.force = 1;
    fm1_ms = 10000u;
    frames(120);                                  /* the screen settled */

    /* the loop body leaves the frame's own state alone: no fix-point here, just the calls */
    c0 = fb_cpu();
    w0 = (double)clock() / (double)CLOCKS_PER_SEC;
    for (i = 0; i < n; i++)
        ui_input();
    printf("  input   ui_input() alone              %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
    (void)w0;

    c0 = fb_cpu();
    for (i = 0; i < n; i++)
        ui_leds();
    printf("  leds    ui_leds() alone               %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
    c0 = fb_cpu();
    for (i = 0; i < n; i++)
        ui_draw();
    printf("  draw    ui_draw() alone               %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
    c0 = fb_cpu();
    for (i = 0; i < n; i++) {
        ui_input();
        ui_leds();
        ui_draw();
        (void)i;
    }
    printf("  frame   input + leds + draw           %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);

    /* a held key: the same frame, with one matrix column closed */
    fm1_in.notes |= 1u;
    c0 = fb_cpu();
    for (i = 0; i < n; i++) {
        ui_input();
        ui_leds();
        ui_draw();
    }
    printf("  keys    the same, a key held          %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);

    /* the main loop's slow housekeeping: the autosave's signature, quiet test and the section-fill watch,
     * which run on a 250 ms tick (project.c autosave_poll) and walk every track's sound, steps and patches */
    {
        volatile uint32_t acc = 0;
        c0 = fb_cpu();
        for (i = 0; i < n; i++)
            acc += autosave_sig();
        printf("  sig     autosave_sig()                %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
        c0 = fb_cpu();
        for (i = 0; i < n; i++)
            acc += (uint32_t)autosave_quiet();
        printf("  quiet   autosave_quiet()              %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
        c0 = fb_cpu();
        for (i = 0; i < n; i++) {
            proj_fill_poll(acc);
            mutate_poll();
        }
        printf("  fill    proj_fill_poll + mutate_poll  %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
    }
    {   /* the 250 ms watches, one call per frame: the tick that hashes the whole music lands on 1 frame in 15 */
        volatile uint32_t acc2 = 0;
        uint32_t ms0 = fm1_ms;
        c0 = fb_cpu();
        for (i = 0; i < n; i++) {
            fm1_ms = ms0 + i * 16u;
            autosave_poll();
            acc2 += fm1_ms;
        }
        printf("  poll    autosave_poll() per frame     %6.1f ns CPU / call\n", (fb_cpu() - c0) * 1e9 / (double)n);
        fm1_ms = ms0;
        (void)acc2;
    }
    return 0;
}
