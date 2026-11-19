/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 1) THE NOTE THE KEY SCAN KEEPS WHILE CORE 1 RENDERS.
 *
 * voice.c postpones a note-on when `FELUCCA_MC_BUSY()` says the other core has a job in flight: the key scan
 * runs from TIMER5, nested in the audio ISR, and the pool it would allocate from is what core 1 reads through
 * its parts' windows. This test stands in for that core: the flag is a variable here, so the whole path
 * (defer, drop when full, replay in order at the block start, no stuck note) runs the real firmware code.
 *
 * Class: host x86-64. No FM-1: on the unit the flag is fm1_mc_job_busy(), which no host test can produce.
 */
static int host_mc_busy;
#define FELUCCA_MC_BUSY() (host_mc_busy)
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("defer: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).gate && V(t, i).stage != 4u;
    return n;
}
static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (p = 0; p < NPART; p++)
        host_preset(&trk[p], 0, 0);
    trk_live = NTRK;
    load_tight = 0;
    sys_load = LOAD_FULL;
    voice_ucap = 0;
    kb_prev = 0;
    fm1_in.notes = 0;
    trk[0].engine = trk[0].eng_req = ENGI_ANALOG;
    trk[0].p[P_VOICE] = V_POLY;
    host_mc_busy = 0;
    vq_n = 0;
    vq_replay = 0;
}

int main(void)
{
    uint32_t i;
    fresh();
    host_mc_busy = 1;                                /* core 1 is rendering */
    trk_note_on(&trk[0], 60, 100);
    check("a note while core 1 renders does not touch the pool", gated(&trk[0]) == 0u && vq_n == 1u);
    voice_defer_flush();
    check(".. and plays at the next block start", gated(&trk[0]) == 1u && vq_n == 0u);

    host_mc_busy = 1;
    trk_note_off(&trk[0], 60);
    check("a note-off while core 1 renders is kept too", vq_n == 1u && gated(&trk[0]) == 1u);
    voice_defer_flush();
    check(".. and ends the note at the next block", gated(&trk[0]) == 0u && vq_n == 0u);

    /* the keybed's path: twelve keys. (F4, #107) the eight the queue holds are replayed at the block start
     * and the four over it start where they were pressed -- the chord is twelve voices, not eight */
    fresh();
    host_mc_busy = 1;
    for (i = 0; i < 12u; i++)
        fm1_in.notes |= 1u << i;
    keyboard_block();
    check("a twelve-key chord while core 1 renders keeps the queue's eight", vq_n == VQ_N);
    check(".. and the four over the queue already sound (none is dropped)", gated(&trk[0]) == 4u);
    voice_defer_flush();
    check(".. and the twelve all sound", gated(&trk[0]) == 12u);

    /* a burst bigger than the queue: what does not fit starts at once; a note-off is never dropped */
    fresh();
    host_mc_busy = 1;
    for (i = 0; i < VQ_N + 4u; i++)
        trk_note_on(&trk[0], 40u + i, 100);
    check("a burst over the queue starts the rest at once, queues not one more", vq_n == VQ_N);
    fresh();
    host_mc_busy = 1;
    trk_note_on(&trk[0], 60, 100);                   /* kept, then played: this is the note to end */
    voice_defer_flush();
    host_mc_busy = 1;
    for (i = 0; i < VQ_N; i++)
        trk_note_on(&trk[0], 40u + i, 100);          /* the queue is full */
    trk_note_off(&trk[0], 60);                       /* a note-off is never dropped */
    check(".. and a note-off is applied at once when the queue is full", gated(&trk[0]) == 0u && vq_n == VQ_N);

    /* between blocks nothing is postponed */
    fresh();
    trk_note_on(&trk[0], 64, 100);
    check("with core 1 idle a note starts at once", gated(&trk[0]) == 1u && vq_n == 0u);

    printf("voice_defer_test: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
