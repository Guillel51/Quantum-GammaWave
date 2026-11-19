/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* How long a note takes to start: the allocator's own path, timed on the host. Not a target measurement --
 * it is here so a change to voice_alloc can be compared with the one it replaces (same compiler, same flags).
 * Prints ns per note-on for a POLY part with the window already at twelve and with free slots. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>

static double bench_ns(void) { return (double)now_ns(); }   /* hostsim.c's clock */
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
    trk[0].engine = trk[0].eng_req = ENGI_ANALOG;
    trk[0].p[P_VOICE] = V_POLY;
}
int main(void)
{
    const uint32_t n = 2000u, keys = 12u;
    uint32_t k, r;
    double t0, ns_one, ns_free;
    fresh();
    trk_note_on(&trk[0], 48, 100);                 /* the window starts at 2: the first notes grow it */
    for (k = 0; k < keys; k++)
        trk_note_on(&trk[0], 48u + 2u * k, 100);
    t0 = bench_ns();
    for (r = 0; r < n; r++)
        for (k = 0; k < keys; k++) {
            trk_note_on(&trk[0], 48u + 2u * k, 100);   /* the same note again: reuse, no allocation */
            trk_note_off(&trk[0], 48u + 2u * k);
        }
    ns_one = (bench_ns() - t0) / (double)(n * keys);
    fresh();
    for (k = 0; k < keys; k++)
        trk_note_on(&trk[0], 48u + 2u * k, 100);
    t0 = bench_ns();
    for (r = 0; r < n; r++)
        for (k = 0; k < keys; k++) {
            uint32_t note = 36u + ((r * keys + k) % 48u);
            trk_note_on(&trk[0], note, 100);           /* a new note each time: the allocator's free path */
            trk_note_off(&trk[0], note);
        }
    ns_free = (bench_ns() - t0) / (double)(n * keys);
    printf("alloc_bench: same note again %.1f ns, new note %.1f ns per note (host x86-64, %u x %u)\n",
           ns_one, ns_free, n, keys);
    return 0;
}
