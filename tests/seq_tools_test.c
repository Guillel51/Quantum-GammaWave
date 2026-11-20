/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SEQ > TOOLS' OPERATORS, DRIVEN DIRECTLY (firmware/src/seq.c seq_tool_apply): the four the page gained in v1.5.18
 * and the ones it had, each one checked on the pattern it produces -- not on a message it prints.
 *
 * The rule every operator keeps (and the page prints): a step's timing, its gate and its chance keep their place;
 * only what sounds changes. So each check here also proves what was NOT touched.
 *
 * Class: host x86-64, the real sources through tests/hostsim.c (which includes seq.c). No audio, no panel.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("seqtools: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t sounding(const track_t *t, uint32_t len)      /* steps that sound, a lane hit included */
{
    uint32_t i, n = 0;
    for (i = 0; i < len; i++)
        n += (t->step[i].n || t->step[i].hit) ? 1u : 0u;
    return n;
}
static uint32_t notes_of(const track_t *t, uint32_t len, uint8_t *out, uint32_t max)
{
    uint32_t i, k, n = 0;
    for (i = 0; i < len; i++)
        for (k = 0; k < t->step[i].n && n < max; k++)
            out[n++] = t->step[i].note[k];
    return n;
}
static void blank(track_t *t)
{
    uint32_t i;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < NSTEP; i++)
        t->step[i].time = ST_REST;
    t->p[P_SLEN] = 16;
    t->p[P_ROOT] = 0;
    t->p[P_SCALE] = 0;                              /* SCALE_MASK[0]: the first scale of the table (major) */
}
/* a phrase: a C major arpeggio on 0 4 8 12 (60 64 68 72), a two-note chord on 6, a lane hit on 10 */
static void phrase(track_t *t)
{
    uint32_t i;
    blank(t);
    for (i = 0; i < 4u; i++) {
        uint32_t s = i * 4u;
        t->step[s].n = 1;
        t->step[s].note[0] = (uint8_t)(60u + i * 4u);
        t->step[s].vel = (uint8_t)(70u + i * 10u);
        t->step[s].time = ST_NOTE;
        t->step[s].flags = (uint8_t)(i == 0 ? SF_ACCENT : 0);
        t->step[s].probability = (uint8_t)(i == 1 ? 50u : 0u);
    }
    t->step[6].n = 2;
    t->step[6].note[0] = 67;
    t->step[6].note[1] = 71;
    t->step[6].vel = 96;
    t->step[6].time = ST_NOTE;
    t->step[10].hit = 0x01;
    t->step[10].time = ST_NOTE;
    t->step[10].vel = 100;
}

int main(void)
{
    track_t *t = &trk[0];
    uint8_t before[64], after[64];
    uint32_t nb, na, i, n;
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(&song, 0, sizeof song);

    /* ---- the operators the page already had --------------------------------------------------------- */
    phrase(t);
    seq_tool_apply(t, SQOP_DUP, 2);                 /* TIMES 2: the pattern twice */
    check("DUP x2: the pattern after itself, LEN doubles, the copy identical",
          t->p[P_SLEN] == 32 && t->step[16].note[0] == 60u && t->step[20].note[0] == 64u && t->step[22].n == 2u &&
          memcmp(t->step, t->step + 16, 16 * sizeof t->step[0]) == 0);
    seq_tool_apply(t, SQOP_HALF, 1);
    check("HALF: back to sixteen steps, the onsets where the merge put them",
          t->p[P_SLEN] == 16 && t->step[0].note[0] == 60u && t->step[2].note[0] == 64u && t->step[3].n == 2u);
    phrase(t);
    seq_tool_apply(t, SQOP_REV, 1);
    check("REV: the phrase backwards, LEN and gates kept",
          t->p[P_SLEN] == 16 && t->step[15].note[0] == 60u && t->step[3].note[0] == 72u && t->step[3].n == 1u);
    phrase(t);
    seq_tool_apply(t, SQOP_ROTR, 4);
    check("ROT > 4: four steps to the right (step 0 takes the old step 12)",
          t->step[0].note[0] == 72u && t->step[4].note[0] == 60u && t->step[12].note[0] == 68u);
    phrase(t);
    seq_tool_apply(t, SQOP_CLR, 1);
    check("CLEAR: nothing sounds, LEN kept", sounding(t, 16) == 0u && t->p[P_SLEN] == 16);

    /* ---- EUCLID: the same notes, on even onsets ----------------------------------------------------- */
    phrase(t);
    nb = notes_of(t, 16, before, sizeof before);
    seq_tool_apply(t, SQOP_EUCLID, 3);              /* TIMES 3: six onsets */
    na = notes_of(t, 16, after, sizeof after);
    n = sounding(t, 16);
    printf("seqtools: EUCLID x3: %u onsets, %u notes before, %u after: steps %u %u %u %u %u %u\n", n, nb, na,
           t->step[0].n || t->step[0].hit, t->step[3].n || t->step[3].hit, t->step[6].n || t->step[6].hit,
           t->step[8].n || t->step[8].hit, t->step[11].n || t->step[11].hit, t->step[14].n || t->step[14].hit);
    check("EUCLID x3: six onsets, evenly spaced over sixteen (0 3 6 8 11 14)",
          n == 6u && (t->step[0].n || t->step[0].hit) && (t->step[3].n || t->step[3].hit) &&
          (t->step[6].n || t->step[6].hit) && (t->step[8].n || t->step[8].hit) &&
          (t->step[11].n || t->step[11].hit) && (t->step[14].n || t->step[14].hit) && !t->step[1].n && !t->step[2].hit);
    check("EUCLID: every note of the phrase still sounds, in its order", na == nb && memcmp(before, after, nb) == 0);
    check("EUCLID: the length and each step's own chance and gate keep their place",
          t->p[P_SLEN] == 16u && t->step[3].probability == 0u && t->step[4].probability == 50u);
    check("EUCLID: the accent and the lane hit travel with their notes",
          !!(t->step[0].flags & SF_ACCENT) && t->step[11].hit == 0x01);
    phrase(t);
    seq_tool_apply(t, SQOP_CLR, 1);
    seq_tool_apply(t, SQOP_EUCLID, 2);
    check("EUCLID: a pattern that sounds nothing is left alone", sounding(t, 16) == 0u);

    /* ---- MIRROR: the line inverted around the tonic, inside the scale ------------------------------- */
    phrase(t);
    seq_tool_apply(t, SQOP_MIRROR, 1);
    nb = notes_of(t, 16, after, sizeof after);
    na = 0;
    for (i = 0; i < nb; i++)
        na += ((SCALE_MASK[0] >> (after[i] % 12)) & 1u);
    printf("seqtools: MIRROR: 60 64 68 72 -> %u %u %u %u, the chord 67/71 -> %u/%u, %u of %u in scale\n", after[0],
           after[1], after[2], after[3], t->step[6].note[0], t->step[6].note[1], na, nb);
    check("MIRROR: an arpeggio up around C4 becomes the same one down (the chord mirrored too)",
          after[0] == 60u && after[1] == 56u && after[2] == 53u && after[3] == 49u && after[4] == 52u &&
          after[5] == 48u);
    check("MIRROR: every note lands inside the track's scale", nb == 6u && na == nb);
    check("MIRROR: the chord keeps its size and its velocities, the length and chances stay",
          t->step[6].n == 2u && t->step[6].vel == 96u && t->p[P_SLEN] == 16u && t->step[4].probability == 50u &&
          t->step[3].probability == 0u);
    seq_tool_apply(t, SQOP_MIRROR, 1);
    check("MIRROR twice: back where it started", t->step[0].note[0] == 60u && t->step[4].note[0] == 64u &&
          t->step[12].note[0] == 72u);

    /* ---- THIN and CRESC: density and direction ------------------------------------------------------ */
    phrase(t);
    seq_tool_apply(t, SQOP_THIN, 2);
    n = sounding(t, 16);
    printf("seqtools: THIN 1 of 2: %u onsets left, at 0 %u 6 %u 10 %u\n", n, t->step[0].n || t->step[0].hit,
           t->step[6].n || t->step[6].hit, t->step[10].hit);
    check("THIN: every second onset out, the ones left where they were",
          n == 3u && (t->step[0].n || t->step[0].hit) && (t->step[6].n || t->step[6].hit) && t->step[10].hit &&
          !t->step[4].n && !t->step[8].n && !t->step[12].n);
    check("THIN: the length and the emptied steps' place untouched", t->p[P_SLEN] == 16u && t->step[4].time == ST_REST);
    phrase(t);
    seq_tool_apply(t, SQOP_CRESC, 4);
    {
        int32_t prev = -1, mono = 1;
        for (i = 0; i < 16u; i++)
            if (t->step[i].n || t->step[i].hit) {
                if (prev >= 0 && (int32_t)t->step[i].vel < prev)
                    mono = 0;
                prev = t->step[i].vel;
            }
        printf("seqtools: CRESC x4: v%u=%u v%u=%u v%u=%u v%u=%u v%u=%u\n", 0u, t->step[0].vel, 4u, t->step[4].vel,
               6u, t->step[6].vel, 8u, t->step[8].vel, 12u, t->step[12].vel);
        check("CRESC: one direction from the first onset to the last, the last at full velocity",
              mono && t->step[12].vel == 127u && t->step[0].vel == (uint8_t)(127 - 4 * 12));
        check("CRESC: the notes, the hits and the length are the ones it had",
              t->step[6].n == 2u && t->step[10].hit == 1u && t->step[6].note[1] == 71u && t->p[P_SLEN] == 16u &&
              t->step[4].probability == 50u);
    }

    /* ---- and the operators never write past the pattern's own length --------------------------------- */
    blank(t);
    for (i = 0; i < 8u; i++)
        if (i % 2u == 0u) {
            t->step[i].n = 1;
            t->step[i].note[0] = (uint8_t)(60u + i);
            t->step[i].time = ST_NOTE;
        }
    t->p[P_SLEN] = 8;
    seq_tool_apply(t, SQOP_EUCLID, 8);              /* sixteen onsets asked of eight steps */
    check("EUCLID: more onsets than steps is clamped to the length, nothing written past it",
          t->p[P_SLEN] == 8u && sounding(t, 8) == 8u && t->step[8].n == 0u && t->step[8].hit == 0u);

    printf("seqtools: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
