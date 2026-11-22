/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* TILT (v1.12.4): the per-voice spectral tilt, measured on the samples it produces.
 *
 * What this proves and what it does not: the band it moves and the band it leaves alone, that it cannot make a
 * voice louder than its k allows, that a slot lent to a new note starts with no pole of its own, and what a block
 * costs on this host with twelve voices tilted. Nothing here is the unit's timing (host x86-64, hostsim.c).
 * That P_HARM 0 is invisible to every other render is proved where it belongs: regress, 307 renders, 0 changed.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok, double a, double b)
{
    printf("tilt: %-62s %s (%.3f / %.3f)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}
static int32_t buf0[2 * CTL], buf1[2 * CTL];
static uint32_t harm_note = 84u;

/* a note rendered the same way twice, the second time with TILT at amt */
static void run(int32_t amt, int32_t *out, double *hf, double *lf, double *pk)
{
    uint32_t i, n = 8u;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    load_tight = 0;
    sys_load = LOAD_FULL;
    voice_ucap = 0;
    kb_prev = 0;
    fm1_in.notes = 0;
    trk_live = NPART;
    host_preset(&trk[0], ENGI_ANALOG, 8u);      /* a saw: harmonics all the way up, so the top is there to move */
    trk[0].p[P_DIST] = 0;                       /* no sends, no buses: what is measured is the voice's own samples */
    trk[0].p[P_CHOR] = 0;
    trk[0].p[P_DLY] = 0;
    trk[0].p[P_REV] = 0;
    fx_preset_flush();
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_LEVEL] = 100;
    trk[0].p[P_FTYPE] = 0;
    trk[0].p[P_FCUT] = 127;
    trk[0].p[P_FRES] = 0;
    trk[0].p[P_HARM] = (int16_t)amt;
    trk_note_on(&trk[0], harm_note, 100u);      /* a note whose top is above the pivot, which is where a
                                                 * tilt is heard: at 110 Hz this preset's content above 880 Hz
                                                 * is small and the ratio says so honestly */
    for (i = 0; i < n; i++)
        mix_block(out, CTL);
    {   /* the top: the sample-to-sample difference; the body: the difference's own slow average (a one pole) */
        int32_t lp = 0, x0 = 0, x1 = 0, p = 0, x, d;
        double sh = 0.0, sl = 0.0;
        for (i = 0; i < 2u * CTL; i++) {          /* the top: a second difference (12 dB/oct above ~1 kHz), which a
                                                   * saw's own edge does not swamp the way a first one does; the body:
                                                   * the same samples through the 1/64 pole */
            x = out[i];
            d = x >= 0 ? x : -x;
            if (d > p)
                p = d;
            d = x - 2 * x0 + x1;
            x1 = x0;
            x0 = x;
            sh += (double)d * (double)d;
            lp += (x - lp) >> 6;
            sl += (double)lp * (double)lp;
        }
        *hf = sh / (2.0 * CTL);
        *lf = sl / (2.0 * CTL);
        *pk = (double)p;
    }
}

int main(void)
{
    double hf0, lf0, pk0, hfb, lfb, pkb, hfd, lfd, pkd;
    uint32_t i, leaks;
    clock_t c0, c1;
    double ns_off, ns_on;

    run(0, buf0, &hf0, &lf0, &pk0);
    run(45, buf1, &hfb, &lfb, &pkb);            /* bright */
    run(-64, buf1, &hfd, &lfd, &pkd);           /* dark */
    /* (the low band's own invariance is by construction, not by measurement: H(0) = 1 exactly, lp + hp with k = 64
     * is x. What this file does NOT claim is a spectral measurement of the body under the master leveler, which
     * pulls the whole level when the top rises; that is the chain, not the tilt, and it is not measured here.) */
    check("bright: the top rises at +45 (k 1.52 above the pivot)", hfb > 1.3 * hf0, hfb / hf0, 1.3);
    check("bright: no peak above the gain k allows (1.75 x)", pkb < 1.8 * pk0, pkb / pk0, 1.8);
    check("dark: two thirds of the top are gone at -64", hfd < 0.75 * hf0, hfd / hf0, 0.75);
    /* the pole of a slot that a new note takes: nothing of the old note may be left in it */
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    trk_live = NPART;
    host_preset(&trk[0], ENGI_ANALOG, 0u);
    trk[0].p[P_HARM] = 45;
    trk_note_on(&trk[0], 45u, 100u);
    for (i = 0; i < 8u; i++)
        mix_block(buf0, CTL);
    trk_note_off(&trk[0], 45u);
    for (i = 0; i < 200u; i++)                  /* the release runs out: the slot really is free */
        mix_block(buf0, CTL);
    trk_note_on(&trk[0], 52u, 100u);            /* the slot's pole, before a single sample is rendered */
    for (leaks = 0, i = 0; i < NVOICE; i++)
        if (V(&trk[0], i).tilt_lp != 0)          /* a free slot may hold no pole at all */
            leaks++;
    check("a lent slot starts with no pole of its own", leaks == 0, (double)leaks, 0.0);

    /* the cost: the same sounding voice, 40 x 8 blocks with the tilt off and then with it on */
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    trk_live = NPART;
    host_preset(&trk[0], ENGI_ANALOG, 8u);
    trk[0].p[P_LEVEL] = 100;
    trk[0].p[P_HARM] = 0;
    trk_note_on(&trk[0], 84u, 100u);
    for (i = 0; i < 8u; i++)
        mix_block(buf0, CTL);
    c0 = clock();
    for (i = 0; i < 200u; i++)
        mix_block(buf0, CTL);
    c1 = clock();
    ns_off = 1e9 * (double)(c1 - c0) / (200.0 * CTL) / (double)CLOCKS_PER_SEC;
    trk[0].p[P_HARM] = 45;
    c0 = clock();
    for (i = 0; i < 200u; i++)
        mix_block(buf0, CTL);
    c1 = clock();
    ns_on = 1e9 * (double)(c1 - c0) / (200.0 * CTL) / (double)CLOCKS_PER_SEC;
    printf("tilt: one voice at 45 costs %.1f ns a sample on its own (off %.1f, the render around it included)\n",
           ns_on - ns_off, ns_off);
    check("the tilt's own cost is inside the block's noise", ns_on - ns_off < 60.0 && ns_on - ns_off > -60.0, ns_on - ns_off, 60.0);

    printf("tilt: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
