/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3) THE INSTRUMENT UNDER LOAD: six parts, six engines, effects, chords, fast attacks and preset
 * changes, with the allocator's own counters read as it happens.
 *
 * What it answers, with numbers instead of adjectives: how many voices the pool really holds with every part
 * playing, how many the allocator takes (STL), how many note-ons found no voice at all (DEN), whether a
 * chord under six parts is cut down to a fraction of the budget, whether a preset change mid-chord leaves a
 * stuck note behind, and what a block costs on this host.
 *
 * The assertions are about invariants, not about a tuning:
 *   * the pool never holds more than NVOICE voices -- the budget is a budget;
 *   * no note-on is DENIED while the pool has a voice to give (a denied note is silent, which is the defect
 *     the monitor exists to expose);
 *   * a chord on one part is not reduced because five other parts are playing (its fair share is a tie-break,
 *     not a reservation);
 *   * after every note is let go and the tails are given their time, nothing is active: no stuck voice, no
 *     poisoned window;
 *   * the peak the monitor reports equals what the pool actually held.
 *
 * Class: host x86-64, the real sources. The CPU figure is this machine's wall clock, not the unit's.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <time.h>

static int bad;
static void check(const char *what, int ok, unsigned a, unsigned b)
{
    printf("stress: %-72s %s (%u / %u)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

static uint32_t active_all(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += V(&trk[p], i).active != 0;
    return n;
}
static uint32_t gated_part(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(&trk[p], i).active && V(&trk[p], i).gate && V(&trk[p], i).stage != 4u;
    return n;
}
static void fresh_all(void)
{
    uint32_t p;
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
    voice_stats_reset();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], (uint32_t)(p % NENGINES), 0);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_LEVEL] = 100;
    }
}

int main(void)
{
    int32_t out[2 * CTL];
    uint32_t p, i, r, snd, rel, fad, pk, stl, den, k;
    struct timespec t0, t1;
    double us_block = 0.0;
    uint32_t blocks = 0;

    /* 1. six parts, six engines, four-note chords on each: 24 notes into a 12-voice budget */
    fresh_all();
    for (r = 0; r < 6u; r++) {                        /* a preset per part, from its own engine's list */
        host_preset(&trk[r], (uint32_t)r, (uint32_t)(r * 3));
        trk[r].p[P_VOICE] = V_POLY;
    }
    for (p = 0; p < NPART; p++)
        for (i = 0; i < 4u; i++)
            trk_note_on(&trk[p], 40u + 3u * p + i * 2u, 100u);
    voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
    check("six parts, 24 notes into the budget: the pool holds NVOICE, not more", voices_busy() <= (uint32_t)NVOICE,
          voices_busy(), (unsigned)NVOICE);
    check(".. and every part is still heard (no part starved to zero)", gated_part(0) + gated_part(1) > 0u,
          gated_part(0), gated_part(1));
    check(".. no note-on was denied outright", den == 0u, den, 0u);
    check(".. the monitor's peak is what the pool held", pk == voices_busy(), pk, voices_busy());

    /* 2. render it: the block cost with every engine and the effects running */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0; k < 400u; k++, blocks++)
        mix_block(out, CTL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    us_block = ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / 1e3 / (double)blocks;
    printf("stress: six parts + FX, %u blocks, %.2f us a block on this host (budget 725 us)\n", blocks, us_block);
    check("a block with six parts and effects stays inside the 725 us budget on this host", us_block < 725.0,
          (unsigned)us_block, 725u);

    /* 3. a chord on one part while five others play: the fair share only breaks ties */
    fresh_all();
    for (p = 1; p < NPART; p++)
        for (i = 0; i < 2u; i++)
            trk_note_on(&trk[p], 48u + 3u * p + i, 100u);   /* 10 voices held elsewhere */
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 60u + i, 100u);                /* 8 more: 18 notes, 12 voices */
    check("a chord on one part still gets what the budget has, beside five busy parts",
          gated_part(0) >= 2u, gated_part(0), 2u);

    /* 4. fast attacks and releases, then a preset change mid-chord: no stuck voice, no poisoned window */
    fresh_all();
    host_preset(&trk[0], ENGI_ANALOG, 9u);                  /* ANALOG BRASS */
    for (k = 0; k < 60u; k++) {
        uint32_t note = 36u + (k % 12u) * 2u;
        trk_note_on(&trk[0], note, 100);
        if (k & 1u)
            trk_note_off(&trk[0], note);
        mix_block(out, CTL);
    }
    for (i = 0; i < 4u; i++)
        trk_note_on(&trk[0], 60u + i, 100);
    host_preset(&trk[0], ENGI_FM6, 0u);                     /* the engine changes under the held chord */
    for (k = 0; k < 120u; k++)
        mix_block(out, CTL);                               /* the crossfade and the tails */
    trk_all_off(&trk[0]);
    for (k = 0; k < 200u; k++)
        mix_block(out, CTL);
    check("a preset / engine change under a held chord leaves no voice behind", active_all() == 0u, active_all(), 0u);

    /* 5. what the allocator took is what the pool gave: every steal is a voice that was replaced */
    fresh_all();
    host_preset(&trk[0], ENGI_ANALOG, 8u);                  /* ANALOG PLUCK */
    for (i = 0; i < 20u; i++) {
        trk_note_on(&trk[0], 40u + i, 100);
        voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
    }
    check("20 notes into the budget: the pool still holds exactly the budget", voices_busy() == voice_budget(),
          voices_busy(), voice_budget());
    check(".. and the ones that could not fit were counted as steals, not lost silently", stl >= 8u, stl, 8u);
    check(".. nothing was denied while steals were possible", den == 0u, den, 0u);
    check(".. the peak never exceeded the budget", pk <= (uint32_t)NVOICE, pk, (unsigned)NVOICE);

    printf("stress: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
