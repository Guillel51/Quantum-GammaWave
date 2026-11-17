/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LEN per track (firmware/src/seq.c, firmware/src/project.c): the sequencer wraps every track at its own P_SLEN,
 * so a 4-step and a 6-step pattern run against each other, and the project keeps four different lengths. A
 * track that leaves LEN at its default stores nothing in the packed project and loads 16 again, which is how
 * every project written before a longer pattern loads.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    static project_t q, q2;
    static union { project_t v; uint8_t raw[sizeof(project_t)]; } buf;
    track_t *a = &trk[0], *b = &trk[1];
    uint32_t i, k, fa = 0, fb = 0;
    int bad = 0, ok;

    /* two tracks, 4 and 6 steps, stepped together: each one wraps on its own step 0 */
    song.g[G_BPM] = 120;
    song.playing = 1;
    for (k = 0; k < 2u; k++) {
        track_t *t = k ? b : a;
        t->p[P_SLEN] = (int16_t)(k ? 6 : 4); t->p[P_SDIV] = 2; t->p[P_SGATE] = 100; t->p[P_AMODE] = 0;
        t->nheld = 0;
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFF;
        t->step[0] = (step_t){ .note = {60, 0, 0, 0}, .n = 1, .time = ST_NOTE, .flags = 0, .vel = 100, .hit = 0, .acc = 0 };
    }
    for (i = 0; i < 60u; i++) {
        uint32_t pa = a->seq_idx, pb = b->seq_idx;
        seq_tick(a, div_samples(2));
        seq_tick(b, div_samples(2));
        if (a->seq_idx != pa && a->seq_idx == 0u) fa++;
        if (b->seq_idx != pb && b->seq_idx == 0u) fb++;
    }
    bad += check("LEN 4 and LEN 6, 60 steps: each wraps at its own step 0 (15 and 10)",
                 fa == 15u && fb == 10u);

    /* the same thing through the project: four different lengths, one per track */
    memset(&q, 0, sizeof q);
    q.magic = PROJ_MAGIC; q.size = sizeof q; q.parts = NPART;
    for (k = 0; k < NTRK; k++) {
        q.t[k].engine = 0;
        for (i = 0; i < P_COUNT; i++) q.t[k].p[i] = proj_pdef(0, i);
    }
    q.t[0].p[P_SLEN] = 5; q.t[1].p[P_SLEN] = 7; q.t[2].p[P_SLEN] = 12; q.t[3].p[P_SLEN] = 64;
    q.sum = proj_sum(&q);
    memcpy(buf.raw, &q, sizeof q);
    ok = proj_import(&q2, buf.raw, (int)sizeof q);
    bad += check("FUN6 project: LEN 5 / 7 / 12 / 64 comes back per track, not shared",
                 ok && q2.t[0].p[P_SLEN] == 5 && q2.t[1].p[P_SLEN] == 7 &&
                 q2.t[2].p[P_SLEN] == 12 && q2.t[3].p[P_SLEN] == 64);

    /* compatibility: LEN at its default is not stored at all, so an older project without it loads 16 */
    bad += check("LEN's default is 16 (what a project that never set it loads)", proj_pdef(0, P_SLEN) == 16);
    for (k = 0; k < NTRK; k++)
        q.t[k].p[P_SLEN] = 16;
    q.sum = proj_sum(&q);
    memcpy(buf.raw, &q, sizeof q);
    ok = proj_import(&q2, buf.raw, (int)sizeof q);
    bad += check("a project with LEN untouched loads 16 on every track",
                 ok && q2.t[0].p[P_SLEN] == 16 && q2.t[1].p[P_SLEN] == 16 &&
                 q2.t[2].p[P_SLEN] == 16 && q2.t[3].p[P_SLEN] == 16);

    printf("pattern length per track: %s\n", bad ? "FAILED" : "passed");
    return bad;
}
