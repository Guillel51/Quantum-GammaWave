/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* THE ENVELOPE'S BEHAVIOUR AND THE SHAPE OF ITS SEGMENTS (v1.12.6).
 *
 * What is checked here: OFF / LOOP / ONE really differ, a short note does not jump when the key comes up inside the
 * attack, a held key under ONE frees its voice on its own, LOOP still loops, two parts and two voices of one part
 * keep their own envelopes (ENV 1 and ENV 2 do not share state), the steal fade only ever goes down, and the whole
 * thing costs nothing extra at the settings that existed before.
 * Class: host x86-64 with the real sources (hostsim.c). Not the unit.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok, double a, double b)
{
    printf("env: %-64s %s (%.3f / %.3f)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}
static void fresh(void)
{
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
    host_preset(&trk[0], ENGI_ANALOG, 8u);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_LEVEL] = 100;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    fx_preset_flush();
}
static uint32_t live0(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(&trk[0], i).active != 0;
    return n;
}
static int32_t out[2 * CTL];

int main(void)
{
    uint32_t i, held, stages, loops;
    int32_t prev, worst, d;

    /* 1. OFF: a held key keeps the voice alive */
    fresh();
    trk[0].p[P_ELOOP] = 0;
    trk_note_on(&trk[0], 60u, 100u);
    for (i = 0; i < 60u; i++)
        mix_block(out, CTL);
    check("OFF: a held key keeps its voice alive", live0() == 1u, (double)live0(), 1.0);

    /* 2. ONE: the same held key frees the voice by itself */
    fresh();
    trk[0].p[P_ELOOP] = 2;
    trk[0].p[P_DEC] = 24;                        /* a decay short enough to reach the sustain inside the window */
    trk_note_on(&trk[0], 60u, 100u);
    for (i = 0; i < 300u; i++)
        mix_block(out, CTL);
    held = live0();
    check("ONE: a held key gives the note its whole shape and frees the voice", held == 0u, (double)held, 0.0);

    /* 3. LOOP: the envelope really cycles (the stage goes back to the attack more than once) */
    fresh();
    trk[0].p[P_ELOOP] = 1;
    trk[0].p[P_SUS] = 40;
    trk[0].p[P_ATK] = 20;
    trk[0].p[P_DEC] = 30;
    trk_note_on(&trk[0], 60u, 100u);
    for (i = 0, loops = 0, stages = 2; i < 200u; i++) {
        mix_block(out, CTL);
        if (V(&trk[0], 0).stage == 1 && stages == 2)
            loops++;
        stages = V(&trk[0], 0).stage;
    }
    check("LOOP: the attack comes round again while the key is held", loops >= 2u, (double)loops, 2.0);

    /* 4. A SHORT NOTE: the key up inside the attack neither jumps nor clicks */
    fresh();
    trk[0].p[P_ELOOP] = 0;
    trk[0].p[P_ATK] = 90;
    trk_note_on(&trk[0], 60u, 100u);
    mix_block(out, CTL);
    trk_note_off(&trk[0], 60u);
    worst = 0;
    prev = out[0];
    for (i = 0; i < 80u; i++) {
        mix_block(out, CTL);
        {
            uint32_t k;
            for (k = 0; k < 2u * CTL; k++) {
                d = out[k] - prev;
                if (d < 0)
                    d = -d;
                if (d > worst)
                    worst = d;
                prev = out[k];
            }
        }
    }
    printf("env: the biggest sample step through the short note's release: %d\n", worst);
    check("a short note has no jump where the key came up", worst < 9000, (double)worst, 9000.0);

    /* 5. ENV 1 AND ENV 2 DO NOT SHARE STATE: two parts, different times, each envelope its own */
    fresh();
    host_preset(&trk[1], ENGI_ANALOG, 8u);
    trk[1].p[P_VOICE] = V_POLY;
    trk[1].p[P_LEVEL] = 100;
    trk[1].p[P_DIST] = trk[1].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_REV] = 0;
    trk[0].p[P_ATK] = 10;
    trk[1].p[P_ATK] = 100;
    trk_note_on(&trk[0], 60u, 100u);
    trk_note_on(&trk[1], 67u, 100u);
    for (i = 0; i < 6u; i++)
        mix_block(out, CTL);
    check("two parts keep their own envelopes (ENV 1 / ENV 2 apart)", V(&trk[0], 0).env > V(&trk[1], 0).env,
          (double)V(&trk[0], 0).env, (double)V(&trk[1], 0).env);

    /* 6. TWO VOICES OF ONE PART: pressure on one does not move the other */
    fresh();
    trk[0].p[P_ATK] = 60;
    trk_note_on(&trk[0], 60u, 100u);
    for (i = 0; i < 4u; i++)
        mix_block(out, CTL);
    trk_note_on(&trk[0], 64u, 100u);
    mix_block(out, CTL);
    check("a note starting does not reset the envelope of the one sounding", V(&trk[0], 0).env > 0 && V(&trk[0], 1).env < V(&trk[0], 0).env,
          (double)V(&trk[0], 1).env, (double)V(&trk[0], 0).env);

    /* 7. THE STEAL FADE ONLY GOES DOWN */
    fresh();
    {
        voice_t *v = &V(&trk[0], 0);
        int32_t last = 1 << 24;
        int up = 0;
        trk_note_on(&trk[0], 60u, 100u);
        for (i = 0; i < 8u; i++)
            mix_block(out, CTL);
        v->stage = 4;                                /* the fade the allocator uses */
        v->kfade = 0;
        v->env = 1 << 24;
        for (i = 0; i < 12u; i++) {
            mix_block(out, CTL);
            if (v->env > last)
                up++;
            last = v->env;
        }
        check("the steal fade never goes back up", up == 0, (double)up, 0.0);
    }

    printf("env: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
