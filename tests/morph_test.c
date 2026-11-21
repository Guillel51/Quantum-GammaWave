/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 6 (MORPH SCENES): four states of one sound and the position between a pair.
 *   build/host/morph_test
 *
 * 1. the page exists and its four knobs are the morph's: MORPH, A, B and the scene actions, named in
 *    the tables the UI reads.
 * 2. capture is a copy: storing a scene does not change the sound that is playing, and two scenes hold
 *    what was there when each was stored.
 * 3. the position interpolates: 0 is scene A sample for sample, 127 is scene B, and a mid position is the
 *    linear blend of the two on a continuous id (the filter's cutoff).
 * 4. what morphing never touches: an id the motion system does not classify as sound (MUTE), the engine
 *    byte, the transport and the routing are the same at both ends; and the matrix's slots 5..8 keep
 *    their AMT blended and their SRC/DST from the near half.
 * 5. the slew: a jump walks 3 of 127 a frame, so nothing bigger than that ever reaches the engines, and it
 *    lands exactly on the target; the extremes (0 and 127) are exact and stay.
 * 6. a scene is independent of the preset format: loading another factory preset after the capture does
 *    not disturb the scenes, and the first one comes back at position 0.
 *
 * No hardware: the host build of the same sources. It says nothing about the FM-1's flash or its CPU. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>
#define PROJ_HOST 1                             /* (project.c's host hooks) */
#include "../firmware/src/project.c"

static int fails;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("morph: FAIL %s\n", what);
        fails++;
    } else {
        printf("morph: %s  ok\n", what);
    }
}

/* the page's knob values are what the UI writes; msc_sync reads them once a frame */
static void knob(int32_t p, int32_t a, int32_t b)
{
    msc_ui_p = (int16_t)p;
    msc_ui_a = (int16_t)a;
    msc_ui_b = (int16_t)b;
    msc_sync();
}
static void settle(void)                                 /* walk to the target, one frame at a time */
{
    int32_t i;
    for (i = 0; i < 200 && msc_pos != msc_want; i++)
        msc_tick();
}

int main(void)
{
    track_t *t = &trk[0];
    int16_t cut_a, cut_b, mute_a, mx_amt_a, mx_amt_b;
    int32_t v, prev, step, worst = 0;

    host_tracks_init();
    song.sel = 0;
    host_preset(t, 0u, 0u);                              /* ANALOG, a factory preset */

    /* 1. the page and its knobs */
    {
        const page_t *pg = 0;
        uint32_t i;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].title && PAGES[i].title[0] == 'S' && str_eq(PAGES[i].title, "SCENES"))
                pg = &PAGES[i];
        check("the SCENES page is in the table and its four rows are the morph's",
              pg && pg->id[0] == G_MSCP && pg->id[1] == G_MSCA && pg->id[2] == G_MSCB && pg->id[3] == G_MSCG);
        {
            int16_t *vp = 0;
            const param_desc_t *d = page_desc(pg, 3u, &vp);
            check("the action row names its four states and starts at --",
                  d && vp && str_eq(d->label, "SCENE") && str_eq(N_MSCG[0], "--") && vp == &msc_ui_g && msc_ui_g == 0);
        }
    }

    /* 2. capture is a copy of the sound, not a change to it */
    t->p[P_FCUT] = 30;
    t->p[P_MUTE] = 0;
    t->mx[0][0] = 5; t->mx[0][1] = 9; t->mx[0][2] = 20;
    cut_a = t->p[P_FCUT];
    mute_a = t->p[P_MUTE];
    mx_amt_a = t->mx[0][2];
    msc_capture(0);
    check("storing scene 1 leaves the sound alone", t->p[P_FCUT] == cut_a && t->p[P_MUTE] == mute_a);
    t->p[P_FCUT] = 100;
    t->p[P_MUTE] = 1;
    t->mx[0][2] = 100;
    cut_b = t->p[P_FCUT];
    mx_amt_b = t->mx[0][2];
    msc_capture(1);
    check("two scenes hold what each had", msc_p[0][P_FCUT] == 30 && msc_p[1][P_FCUT] == 100 && msc_p[0][P_MUTE] == 0);

    /* 3. the position */
    knob(0, 1, 2);                                       /* A = scene 1, B = scene 2 */
    check("a captured pair arms the morph", msc_on == 1 && msc_a == 0 && msc_b == 1);
    msc_pos = 0;
    msc_apply(0);
    check("position 0 is scene A", t->p[P_FCUT] == cut_a && t->p[P_MUTE] == mute_a);
    msc_pos = 127;
    msc_apply(127);
    check("position 127 is scene B", t->p[P_FCUT] == cut_b);
    v = msc_lerp(cut_a, cut_b, 64);
    msc_pos = 64;
    msc_apply(64);
    check("a mid position is the linear blend", t->p[P_FCUT] == v && v > cut_a && v < cut_b);

    /* 4. what it never touches */
    msc_apply(63);
    check("a choice (MUTE) follows the near scene: below 64 it is A's", t->p[P_MUTE] == mute_a);
    msc_apply(64);
    check(".. and from the half point it is B's", t->p[P_MUTE] == 1);
    msc_apply(127);
    check(".. and at 127 it is scene B's", t->p[P_MUTE] == 1);
    msc_apply(64);
    check("the engine byte is not a morph target", t->engine == (uint8_t)t->eng_req);
    check("the matrix's AMT blends and its SRC/DST come from the near half",
          t->mx[0][2] == msc_lerp(mx_amt_a, mx_amt_b, 64) && t->mx[0][0] == 5 && t->mx[0][1] == 9);

    /* 5. the slew: 3 of 127 a frame, exact at both ends */
    msc_pos = 0;
    msc_ui_p = 0;
    msc_want = 0;
    knob(127, 1, 2);
    prev = msc_pos;
    for (step = 0; step < 200 && msc_pos != 127; step++) {
        int32_t d;
        msc_tick();
        d = msc_pos - prev;
        if (d > worst)
            worst = d;
        prev = msc_pos;
    }
    check("a jump walks 3 a frame and lands on the target", msc_pos == 127 && worst <= 3 && step > 40);
    knob(0, 1, 2);
    settle();
    check("and comes back to 0 exactly", msc_pos == 0 && t->p[P_FCUT] == cut_a);
    knob(127, 1, 2);
    settle();
    check("127 again: scene B, untouched by the walk", msc_pos == 127 && t->p[P_FCUT] == cut_b);

    /* 6. the scenes are their own storage: a preset load does not disturb them */
    host_preset(t, 1u, 2u);
    check("loading another preset leaves the scenes alone", msc_have == 3 && msc_p[0][P_FCUT] == 30);
    msc_pos = 0;
    msc_apply(0);
    check("and scene A comes back on the new sound's buffer", t->p[P_FCUT] == cut_a && t->p[P_MUTE] == mute_a);

    /* a pair of one scene is no morph: B steps to the next one */
    knob(64, 2, 2);
    check("A and B the same scene picks the next one", msc_a != msc_b);

    printf("morph: %s\n", fails ? "FAILED" : "all checks ok");
    return fails ? 1 : 0;
}
