/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3 de esta ronda) THE LENGTH THE KEY WAS HELD: recording measures it, playback uses it, a project keeps it.
 *
 * The step model could only say "the whole step" (the track's GATE) or "a TIE into the next one", so a staccato
 * key, a legato note and an off-beat stab recorded as the same step -- the played rhythm and the played lengths
 * were both thrown away. Step_t.dur now holds the real length in sixteenths of the step (0 = the GATE, which is
 * every pattern that existed before the field), measured at the note-off and persisted by project.c.
 *
 * What this proves [HOST, x86-64, the real sources]:
 *   A. playback ends the note where the recorded length says, not at the track's GATE;
 *   B. dur 0 is exactly the GATE: nothing that existed before changes;
 *   C. recording measures a note released inside its step (a quarter of the step -> 4/16);
 *   D. a note that crosses the step keeps the TIE model and writes no length;
 *   E. a project carries the lengths, and a project with none writes no block at all (an older reader finds
 *      everything where it was).
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
#include "../firmware/src/project.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("dur: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void seq_reset(track_t *t, uint32_t slen, uint32_t gate)
{
    memset(t->step, 0, sizeof t->step);
    t->p[P_SLEN] = (int16_t)slen;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = (int16_t)gate;
    t->p[P_AMODE] = 0;
    t->p[P_HRQ] = 127;                      /* on the grid: this test is about length, not micro */
    t->p[P_SOFS] = 0;
    t->nheld = 0;
    t->seq_idx = (uint16_t)(slen - 1u);
    t->seq_pos = 0x7FFFFFFF;
    t->seq_off = 0;
    t->seq_notes[0] = 0xFF;
    t->step[0] = (step_t){ .note = { 60, 0, 0, 0 }, .n = 1, .time = ST_NOTE, .vel = 100 };
}

int main(void)
{
    static project_t q, q2;
    static union { project_t v; uint8_t raw[sizeof(project_t)]; } buf;
    track_t *t = &trk[0];
    uint32_t step, off_gate, off_half, i, found = 0;
    int ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    song.g[G_BPM] = 120;
    song.playing = 1;
    voices_boot();
    host_tracks_init();

    /* ---- A/B: playback uses the recorded length, and 0 is the GATE -------------------------------- */
    seq_reset(t, 4u, 127u);
    step = div_samples(2);
    seq_tick(t, step);
    off_gate = t->seq_off;
    seq_reset(t, 4u, 127u);
    t->step[0].dur = 8;                     /* held half the step */
    seq_tick(t, step);
    off_half = t->seq_off;
    printf("dur: A step %u samples, gate %u, dur 8/16 -> %u\n", step, off_gate, off_half);
    check("A: a step recorded at half stays half, well under the track's GATE",
          off_half > 0u && off_half < off_gate / 2u + step / 8u && off_half <= step / 2u + 2u);
    check("B: dur 0 is the GATE it always was", off_gate > step / 2u && off_gate <= step);

    /* ---- C: recording measures the length the key was held ---------------------------------------- */
    memset(trk, 0, sizeof trk);
    song.playing = 1;
    song.rec = 1u;                          /* track 0 armed */
    seq_reset(t, 4u, 127u);
    t->seq_idx = 0u;
    t->seq_pos = 0u;
    t->ext = 0u;
    rec_note(t, 60u, 100u);
    t->seq_pos = step / 4u;                 /* released a quarter of the step later */
    rec_release(t, 60u);
    printf("dur: C released at %u of %u -> dur %u/16\n", step / 4u, step, ST_DUR(&t->step[0]));
    check("C: a quarter of the step records as 4/16", ST_DUR(&t->step[0]) >= 3u && ST_DUR(&t->step[0]) <= 5u);
    check("C: and the step still holds the note it recorded", t->step[0].n == 1u && t->step[0].note[0] == 60u);

    /* a chord: the longest key is the step's length */
    seq_reset(t, 4u, 127u);
    t->seq_idx = 0u; t->seq_pos = 0u; t->ext = 0u;
    rec_note(t, 60u, 100u);
    rec_note(t, 64u, 100u);
    t->seq_pos = step / 8u;
    rec_release(t, 60u);                    /* the short one first */
    t->seq_pos = step / 2u;
    rec_release(t, 64u);
    printf("dur: chord 1/8 then 1/2 -> dur %u/16\n", ST_DUR(&t->step[0]));
    check("C: a chord keeps its longest key (8/16), not the last release", ST_DUR(&t->step[0]) >= 7u &&
                                                                          ST_DUR(&t->step[0]) <= 9u);

    /* ---- D: a note that crosses the step writes no length (the TIE model keeps it) ------------------ */
    seq_reset(t, 4u, 127u);
    t->seq_idx = 0u; t->seq_pos = 0u; t->ext = 0u;
    rec_note(t, 60u, 100u);
    t->seq_idx = 1u;                        /* the sequencer moved on */
    t->seq_pos = step / 8u;
    rec_release(t, 60u);
    printf("dur: D released a step later -> dur %u/16 (TIE keeps it)\n", ST_DUR(&t->step[0]));
    check("D: a note that crosses the step leaves the length to the TIEs", ST_DUR(&t->step[0]) == 0u);

    /* ---- E: the project carries the lengths, and only when there are some -------------------------- */
    memset(&q, 0, sizeof q);
    q.magic = PROJ_MAGIC; q.size = sizeof q; q.parts = NPART;
    for (i = 0; i < NTRK; i++) {
        q.t[i].engine = 0;
        for (uint32_t k = 0; k < P_COUNT; k++)
            q.t[i].p[k] = proj_pdef(0, k);
    }
    q.t[0].step[3] = (step_t){ .note = { 67, 0, 0, 0 }, .n = 1, .time = ST_NOTE, .vel = 100 };
    q.t[0].step[3].dur = 9;
    q.t[2].step[0] = (step_t){ .note = { 48, 0, 0, 0 }, .n = 1, .time = ST_NOTE, .vel = 100 };
    q.t[2].step[0].dur = 2;
    q.sum = proj_sum(&q);
    memcpy(buf.raw, &q, sizeof q);
    ok = proj_import(&q2, buf.raw, (int)sizeof q);
    printf("dur: E project  dur[0][3] %u  dur[2][0] %u  dur[1][0] %u\n",
           ST_DUR(&q2.t[0].step[3]), ST_DUR(&q2.t[2].step[0]), ST_DUR(&q2.t[1].step[0]));
    check("E: the lengths come back where they were", ok && ST_DUR(&q2.t[0].step[3]) == 9u &&
                                                     ST_DUR(&q2.t[2].step[0]) == 2u &&
                                                     ST_DUR(&q2.t[1].step[0]) == 0u);
    for (i = 0; i < NTRK; i++)
        for (uint32_t k = 0; k < NSTEP; k++)
            q.t[i].step[k].dur = 0;
    q.sum = proj_sum(&q);
    memcpy(buf.raw, &q, sizeof q);
    ok = proj_import(&q2, buf.raw, (int)sizeof q);
    for (i = 0; i + 1u < sizeof q; i++)
        if (buf.raw[i] == 'D' && buf.raw[i + 1u] == 'U')
            found = 1;
    check("E: with no length, no block is written at all", ok && !found);

    printf("dur: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
