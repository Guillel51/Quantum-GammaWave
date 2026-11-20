/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SIX TRACKS AND TWELVE VOICES, TOGETHER: the pool's real behaviour when the whole instrument is playing.
 *
 * What this proves, and each one counted where the allocator counts it (voices that are active and gated, the
 * ones ringing out, the ones given up, the ones refused) -- not a number printed for the eye:
 *
 *   A. six live tracks, two voices each: twelve gated at once, twelve busy, peak twelve, not one given up;
 *   B. the same twelve concentrated in a single part (the window grows and borrows), and then the other five
 *      parts still get their voices back;
 *   C. a distribution the pool has to balance: 4 + 4 + 2 + 1 + 1 = twelve over five parts;
 *   D. releases: six of twelve let go read as six gated and six in release, peak twelve, and a new note takes
 *      a free slot instead of taking a tail;
 *   E. THE LOAD GOVERNOR (load_tight, what audio.c sets above 65 % of a half): a new note with free slots and
 *      tails ringing must NOT cut a tail short -- the budget is the contract, the governor protects at the cap;
 *   F. with the FX on (chorus, delay, reverb on every part) the twelve still sound and nothing is given up;
 *   G. a preset change under a held twelve-voice chord: the voices stay, nothing is given up, the block still
 *      carries signal;
 *   H. TRACKS 4 (the shipped default): T5 and T6 refuse a note, say so (asleep_hit), and that refusal is not a
 *      stolen voice;
 *   I. the cost of the full scene on the host, per block, against the audio deadline (class host: this is not
 *      the unit's timing; tests/target_budget.py is the static one and neither is a measurement on hardware).
 *
 * Class: host x86-64, the real sources (hostsim.c includes voice.c, engines, fx.c, seq.c). No FM-1 here.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("six12: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t gated_part(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).gate && V(t, i).stage != 4u;
    return n;
}
static uint32_t release_part(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && !V(t, i).gate && V(t, i).stage != 4u;
    return n;
}
static uint32_t gated_all(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += gated_part(&trk[p]);
    return n;
}
static uint32_t release_all(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += release_part(&trk[p]);
    return n;
}
static uint32_t slots_all(void)                  /* the slots the parts hold (their windows), VDEAD excluded */
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += trk[p].vn;
    return n;
}

static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    voices_boot();
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], p, 0);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_LEVEL] = 100;
        trk[p].p[P_MUTE] = 0;
    }
    trk_live = NTRK;
    trk_hush = 0;
    load_tight = 0;
    sys_load = LOAD_FULL;
    voice_ucap = 0;                              /* the user's setting at its default: twelve */
    song.master_q12 = 4096;
    voice_kills = 0;
    voice_denied = 0;
    voice_stats_reset();
    asleep_hit = 0;
}

static int32_t out[2 * CTL];
static uint32_t peak_now(void)                   /* the monitor's peak is taken in voice_stats, once a frame */
{
    uint32_t s, r, f, pk, st, dc;
    voice_stats(&s, &r, &f, &pk, &st, &dc);
    return pk;
}
static void blocks(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        mix_block(out, CTL);
}
static int32_t block_peak(void)
{
    uint32_t i;
    int32_t pk = 0;
    for (i = 0; i < 2u * CTL; i++) {
        int32_t a = out[i] < 0 ? -out[i] : out[i];
        if (a > pk) pk = a;
    }
    return pk;
}

int main(void)
{
    uint32_t p, i, s, r, pk, kills;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- A: six live tracks, two voices each = twelve ------------------------------------------------ */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 48u + p * 5u, 100u);
        trk_note_on(&trk[p], 55u + p * 5u, 100u);
    }
    s = gated_all();
    printf("six12: A six parts x 2 notes  gated %u busy %u peak %u kills %u windows %u\n",
           s, voices_busy(), peak_now(), voice_kills, slots_all());
    check("A: six live tracks hold twelve gated voices at once", s == 12u && gated_all() == 12u);
    check("A: twelve busy (sounding, not fading), peak twelve", voices_busy() == 12u && peak_now() == 12u);
    check("A: nothing given up, nothing refused", voice_kills == 0u && voice_denied == 0u);
    check("A: every one of the six parts sounds two voices of its own",
          gated_part(&trk[0]) == 2u && gated_part(&trk[1]) == 2u && gated_part(&trk[2]) == 2u &&
          gated_part(&trk[3]) == 2u && gated_part(&trk[4]) == 2u && gated_part(&trk[5]) == 2u);
    blocks(8u);
    check("A: the twelve still sound eight blocks later, still nothing given up",
          gated_all() == 12u && voice_kills == 0u);
    check("A: the block carries signal and does not sit on the rail", block_peak() > 1000 && block_peak() < 32767);

    /* ---- B: all twelve in one part, then the others get theirs back ----------------------------------- */
    fresh();
    for (i = 0; i < 12u; i++)
        trk_note_on(&trk[0], 40u + i * 3u, 100u);
    s = gated_part(&trk[0]);
    printf("six12: B one part x 12 notes  gated %u busy %u peak %u kills %u windows %u (part 0: %u)\n",
           s, voices_busy(), peak_now(), voice_kills, slots_all(), trk[0].vn);
    check("B: one part alone holds all twelve", s == 12u && voices_busy() == 12u && peak_now() == 12u);
    check("B: it grew its window to twelve without taking a sounding voice", trk[0].vn >= 12u && voice_kills == 0u);
    blocks(4u);
    check("B: twelve still gated after four blocks", gated_part(&trk[0]) == 12u);
    for (i = 0; i < 12u; i++)
        trk_note_off(&trk[0], 40u + i * 3u);
    blocks(200u);                                 /* the release tails end */
    trk_note_on(&trk[5], 60u, 100u);
    check("B: after it lets go, another part still gets a voice", gated_part(&trk[5]) == 1u && voice_kills == 0u);

    /* ---- C: a distribution the pool must balance ------------------------------------------------------ */
    fresh();
    for (i = 0; i < 4u; i++) { trk_note_on(&trk[0], 40u + i * 2u, 100u); trk_note_on(&trk[1], 52u + i * 2u, 100u); }
    for (i = 0; i < 2u; i++) trk_note_on(&trk[2], 64u + i * 2u, 100u);
    trk_note_on(&trk[3], 76u, 100u);
    trk_note_on(&trk[4], 84u, 100u);
    s = gated_all();
    printf("six12: C 4+4+2+1+1  gated %u busy %u kills %u denied %u  parts %u %u %u %u %u %u\n", s, voices_busy(),
           voice_kills, voice_denied, gated_part(&trk[0]), gated_part(&trk[1]), gated_part(&trk[2]),
           gated_part(&trk[3]), gated_part(&trk[4]), gated_part(&trk[5]));
    check("C: 4 + 4 + 2 + 1 + 1 sounds as twelve, each on its own part",
          s == 12u && gated_part(&trk[0]) == 4u && gated_part(&trk[1]) == 4u && gated_part(&trk[2]) == 2u &&
          gated_part(&trk[3]) == 1u && gated_part(&trk[4]) == 1u && gated_part(&trk[5]) == 0u);
    check("C: the balancing gave nothing up and refused nothing", voice_kills == 0u && voice_denied == 0u);

    /* ---- D: releases ---------------------------------------------------------------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_REL] = 110;                    /* a long release: the tails are what D and E measure */
        trk_note_on(&trk[p], 45u + p * 4u, 100u);
        trk_note_on(&trk[p], 60u + p * 4u, 100u);
    }
    for (p = 0; p < 3u; p++) {                    /* three parts let go: six voices */
        trk_note_off(&trk[p], 45u + p * 4u);
        trk_note_off(&trk[p], 60u + p * 4u);
    }
    s = gated_all();
    r = release_all();
    printf("six12: D releases  gated %u release %u peak %u kills %u\n", s, r, peak_now(), voice_kills);
    check("D: six let go read as six gated and six ringing out", s == 6u && r == 6u);
    check("D: the peak is twelve, the steals none", peak_now() == 12u && voice_kills == 0u);
    /* ---- D2: at the cap the victim is a released voice, never a held one ----------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 45u + p * 4u, 100u);
        trk_note_on(&trk[p], 60u + p * 4u, 100u);
    }
    trk_note_off(&trk[5], 60u + 5u * 4u);         /* one of the twelve lets go */
    s = gated_all();
    r = release_all();
    kills = voice_kills;
    trk_note_on(&trk[2], 91u, 100u);              /* the budget is full at twelve: something must go */
    printf("six12: D2 full budget  gated %u release %u kills %u -> %u  after: gated %u release %u\n",
           s, r, kills, voice_kills, gated_all(), release_all());
    check("D2: at the cap the new note takes the released voice and no held one",
          voice_kills == kills + 1u && gated_all() == 12u && release_all() == 0u);

    /* ---- E: the load governor must not cut a tail with room in the budget ----------------------------- */
    fresh();
    for (i = 0; i < 4u; i++) {
        trk[0].p[P_REL] = 110;
        trk_note_on(&trk[0], 44u + i * 2u, 100u);
    }
    for (i = 0; i < 4u; i++)
        trk_note_off(&trk[0], 44u + i * 2u);      /* four tails ringing, nothing held */
    load_tight = 1;                               /* what audio.c sets above 65 % of a half */
    kills = voice_kills;
    r = release_all();
    trk_note_on(&trk[3], 67u, 100u);              /* free slots exist (four of twelve busy at most) */
    printf("six12: E load_tight  release before %u after %u kills %u -> %u busy %u\n",
           r, release_all(), kills, voice_kills, voices_busy());
    check("E: with the budget not full, the governor does not cut a release tail",
          voice_kills == kills && release_all() == r);
    check("E: the new note sounds (it took a free slot)", gated_part(&trk[3]) == 1u);
    load_tight = 0;

    /* ---- F: twelve voices with the FX on --------------------------------------------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_CHOR] = 40;
        trk[p].p[P_DLY] = 35;
        trk[p].p[P_REV] = 30;
        trk_note_on(&trk[p], 43u + p * 5u, 100u);
        trk_note_on(&trk[p], 55u + p * 5u, 100u);
    }
    blocks(12u);
    s = gated_all();
    pk = block_peak();
    printf("six12: F with FX  gated %u busy %u kills %u peak %d\n", s, voices_busy(), voice_kills, pk);
    check("F: twelve voices still sound with chorus, delay and reverb on every part",
          s == 12u && voice_kills == 0u);
    check("F: the FX return is in the block and the rail is not", pk > 1000 && pk < 32767);

    /* ---- G: a preset change under a held twelve-voice chord -------------------------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 47u + p * 4u, 100u);
        trk_note_on(&trk[p], 59u + p * 4u, 100u);
    }
    blocks(4u);
    kills = voice_kills;
    for (p = 0; p < NPART; p++) {                 /* the next preset of its engine, while it sounds */
        uint32_t e = trk[p].engine, n = ENGINES[e]->npresets;
        host_preset(&trk[p], e, n ? (uint32_t)trk[p].preset + 1u < n ? (uint32_t)trk[p].preset + 1u : 0u : 0u);
    }
    blocks(4u);
    s = gated_all();
    pk = block_peak();
    printf("six12: G preset change  gated %u busy %u kills %u -> %u peak %d\n", s, voices_busy(), kills,
           voice_kills, pk);
    check("G: a preset change does not give up a voice or silence the chord",
          s == 12u && voice_kills == kills && pk > 200);

    /* ---- H: TRACKS 4 (the shipped default): T5 and T6 refuse, and that is not a steal ------------------ */
    fresh();
    trk_live = 4;
    asleep_hit = 0;
    kills = voice_kills;
    trk_note_on(&trk[4], 52u, 100u);
    trk_note_on(&trk[5], 57u, 100u);
    printf("six12: H tracks 4  T5 %u T6 %u asleep_hit %u kills %u\n", gated_part(&trk[4]),
           gated_part(&trk[5]), asleep_hit, voice_kills);
    check("H: with TRACKS 4 the two sleeping parts take no voice", gated_part(&trk[4]) == 0u &&
          gated_part(&trk[5]) == 0u);
    check("H: a note for a sleeping part says so and costs no steal", asleep_hit == 1u && voice_kills == kills);
    trk_live = NTRK;

    /* ---- S: saturated: a thirteenth and fourteenth note take voices, they do not get refused ------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 43u + p * 5u, 100u);
        trk_note_on(&trk[p], 55u + p * 5u, 100u);
    }
    kills = voice_kills;
    trk_note_on(&trk[0], 80u, 100u);
    trk_note_on(&trk[1], 81u, 100u);
    printf("six12: S saturated  gated %u busy %u kills +%u denied %u peak %u\n", gated_all(), voices_busy(),
           voice_kills - kills, voice_denied, peak_now());
    check("S: over the budget the notes still sound: a voice is taken, none is refused",
          gated_all() == 12u && voice_denied == 0u && voice_kills == kills + 2u);
    check("S: the peak never reads above the budget", peak_now() == 12u);

    /* ---- I: the cost of the full scene (host clock, not the unit's) ------------------------------------- */
    {
        uint64_t best = ~(uint64_t)0, sum = 0, sum4 = 0, best4 = ~(uint64_t)0;
        uint64_t deadline = (uint64_t)CTL * 1000000ull / FS * 1000ull;
        uint32_t b, n = 400u;
        fresh();
        for (p = 0; p < NPART; p++) {
            trk_note_on(&trk[p], 43u + p * 5u, 100u);
            trk_note_on(&trk[p], 55u + p * 5u, 100u);
        }
        blocks(20u);
        for (b = 0; b < n; b++) {
            uint64_t t0 = now_ns(), dt;
            mix_block(out, CTL);
            dt = now_ns() - t0;
            sum += dt;
            if (dt < best) best = dt;
        }
        check("I: it is still twelve voices after the measurement", gated_all() == 12u);
        /* the same scene with TRACKS 4: what waking T5 and T6 costs, measured on the same machine in the same run */
        fresh();
        trk_live = 4;
        for (p = 0; p < 4u; p++) {
            trk_note_on(&trk[p], 43u + p * 5u, 100u);
            trk_note_on(&trk[p], 55u + p * 5u, 100u);
        }
        blocks(20u);
        for (b = 0; b < n; b++) {
            uint64_t t0 = now_ns(), dt;
            mix_block(out, CTL);
            dt = now_ns() - t0;
            sum4 += dt;
            if (dt < best4) best4 = dt;
        }
        printf("six12: I 4 tracks, 8 voices: mean %.1f us/block | 6 tracks, 12 voices: mean %.1f us/block "
               "(+%.0f %%), best %.1f us, deadline %.1f us (host clock)\n",
               (double)sum4 / (double)n / 1000.0, (double)sum / (double)n / 1000.0,
               100.0 * ((double)sum / (double)sum4 - 1.0), (double)best / 1000.0, (double)deadline / 1000.0);
        check("I: the full scene fits the audio block on the host clock", sum / (uint64_t)n <= deadline);
        trk_live = NTRK;
    }

    printf("six12: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
