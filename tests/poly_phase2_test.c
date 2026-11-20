/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 2: THE SITUATIONS THE EARLIER POLY TESTS DO NOT REACH.
 *
 * poly_six_test covers the pool's arithmetic (twelve at once, concentrated, distributed, releases, the governor,
 * FX, TRACKS 4, saturation). This file covers what a player does to it that those scenes never ask for, and it
 * is the evidence behind the changes in v1.5.23:
 *
 *   A. A TWELVE-NOTE CHORD THROUGH A SOUND LOAD THAT CHANGES ENGINE. xp_note/xp_vel held four notes, so eight keys
 *      of a twelve-note chord were not re-struck on the new engine: they were simply gone (a key still held, its
 *      note not sounding, and its note-off landing on nothing). Now they are all kept and all twelve sound again.
 *   B. TWELVE KEYS ON A MONO / LEGATO PART WITH PRIO LOW. mono_stack held eight: the ninth key dropped the first,
 *      and the first was the one PRIO LOW wanted. The pitch the part plays is now the lowest of the twelve.
 *   C. A BURST OF TWENTY-FOUR NOTE-ONS in one block: the pool keeps the twelve it can, every voice it took is
 *      counted, nothing is refused, and the note-offs after the burst leave nothing held (no stuck note).
 *   D. ACCUMULATED RELEASES: twelve tails over six parts (a long REL) and then a new twelve-note chord. The new
 *      chord is what sounds, the tails are what pays, and no note that was sounding is cut.
 *   E. SIX PARTS COMPETING FOR THE POOL: one part asks for eight notes beside five parts holding ten. It grows by
 *      lending slots (nothing that sounds is moved), it does not get starved and it does not deny a note.
 *   F. A PRESET CHANGE (same engine) UNDER THE CHORD with the FX on: nothing dies, the block keeps signal.
 *   G. SATURATION AND RECOVERY, three times over: 24 notes into the pool, all off, the tails let go, and the pool
 *      comes back empty and whole -- then a single part still gets its twelve. No window is left poisoned.
 *
 * Class: host x86-64, the real sources (hostsim.c includes voice.c, engines, fx.c, seq.c). No FM-1 here: this says
 * what the allocator does, not what the unit's DAC does.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("phase2: %-76s %s\n", what, ok ? "ok" : "FAIL");
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
static uint32_t active_all(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += V(&trk[p], i).active != 0;
    return n;
}
/* how many of the part's sounding voices play one of the notes in nt[0..n) */
static uint32_t gated_in(const track_t *t, const uint8_t *nt, uint32_t n)
{
    uint32_t i, k, c = 0;
    for (i = 0; i < NVOICE; i++) {
        if (!V(t, i).active || !V(t, i).gate || V(t, i).stage == 4u)
            continue;
        for (k = 0; k < n; k++)
            if (nt[k] == V(t, i).note) {
                c++;
                break;
            }
    }
    return c;
}

/* what is still in the pool, for the failures: part, note, stage, envelope (a leftover voice is never a mystery) */
static void dump_active(const char *tag)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &V(&trk[p], i);
            if (!v->active)
                continue;
            printf("phase2: %s leftover T%u[%u] note %u gate %u stage %u env %d out %d eng %s(%u) rel %d\n", tag,
                   p + 1u, i, v->note, v->gate, v->stage, (int)v->env, (int)v->env_out,
                   ENGINES[trk[p].engine % NENGINES]->name, trk[p].engine, trk[p].p[P_REL]);
        }
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
        host_preset(&trk[p], ENGI_ANALOG, 0);       /* the same engine everywhere: a scene that needs another
                                                     * one loads it itself (a one-shot ends its own notes, which
                                                     * would read as a voice that went missing) */
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_LEVEL] = 100;
        trk[p].p[P_MUTE] = 0;
    }
    trk_live = NTRK;
    trk_hush = 0;
    load_tight = 0;
    sys_load = LOAD_FULL;
    voice_ucap = 0;
    song.master_q12 = 4096;
    voice_kills = 0;
    voice_denied = 0;
    voice_stats_reset();
    asleep_hit = 0;
    panic_req = 0;
    transport_req = 0;
}

static int32_t out[2 * CTL];
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
    uint32_t i, p, k, s;
    uint8_t chord[XP_N];
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- A: a twelve-note chord through a sound load that changes engine ------------------------------ */
    fresh();
    for (i = 0; i < XP_N; i++) {
        chord[i] = (uint8_t)(45u + i);
        trk_note_on(&trk[0], chord[i], 100u);
    }
    blocks(8u);
    s = gated_part(&trk[0]);
    printf("phase2: A engine change under %u held notes (xp_n %u)  gated %u\n",
           (unsigned)XP_N, (unsigned)trk[0].xp_n, s);
    check("A: the part holds its twelve notes before the load", s == 12u);
    host_preset_req(&trk[0], ENGI_WAVETABLE, 0u);  /* what a sound load does: the engine is asked, not swapped.
                                                    * (Not FM6: its engine_t.poly is 4, so a four-note cap there
                                                    * would be the engine's answer, not the allocator's) */
    panic_req = 1u;                                /* .. and the load is one (the notes held are kept for it) */
    blocks(20u);                                   /* the load's short fade, the switch and the replay */
    s = gated_part(&trk[0]);
    printf("phase2: A after the switch  engine %u (asked %u)  gated %u  kept for it %u  kept notes %u\n",
           trk[0].engine, trk[0].eng_req, s, (unsigned)trk[0].xp_n, gated_in(&trk[0], chord, XP_N));
    check("A: the engine did change", trk[0].engine == ENGI_WAVETABLE);
    check("A: all twelve keys of the chord sound again on the new engine",
          s == 12u && gated_in(&trk[0], chord, XP_N) == 12u);
    blocks(8u);
    check("A: and they are still the chord eight blocks later", gated_in(&trk[0], chord, XP_N) == 12u);
    for (i = 0; i < XP_N; i++)                     /* every key's note-off still finds its note */
        trk_note_off(&trk[0], chord[i]);
    check("A: every key released is a note that was sounding", gated_part(&trk[0]) == 0u);

    /* ---- B: twelve keys on a MONO / LEGATO part, PRIO LOW --------------------------------------------- */
    fresh();
    trk[0].p[P_VOICE] = V_LEGATO;
    trk[0].p[P_PRIO] = 1;                          /* LOW: the lowest key down wins */
    for (i = 0; i < 12u; i++)                      /* a twelve-key chord, pressed from the bottom up: the lowest is
                                                    * the first, so an eight-slot stack drops the key that wins */
        trk_note_on(&trk[0], 60u + i, 100u);
    printf("phase2: B %u keys in the stack (nmono %u, first %u, last %u)  note %u\n", (unsigned)MONO_STACK_N,
           trk[0].nmono, *ms_p(&trk[0], 0),
           *ms_p(&trk[0], trk[0].nmono ? trk[0].nmono - 1u : 0u), trk[0].mono_note);
    check("B: the stack keeps all twelve keys", trk[0].nmono == MONO_STACK_N);
    check("B: PRIO LOW still plays the lowest of the twelve (60)", trk[0].mono_note == 60u);
    for (i = 1; i < 12u; i++)                       /* let go of the others: 60 is what is left, and still plays */
        trk_note_off(&trk[0], 60u + i);
    check("B: with the eleven above released the low key is still it", trk[0].mono_note == 60u);
    trk_note_off(&trk[0], 60u);
    blocks(200u);
    check("B: and the part lets go of the last one", gated_part(&trk[0]) == 0u);

    /* ---- C: a burst of twenty-four note-ons in one block ---------------------------------------------- */
    fresh();
    k = voice_kills;
    for (i = 0; i < 24u; i++)
        trk_note_on(&trk[i % NPART], 40u + i, 100u);
    s = gated_all();
    printf("phase2: C burst 24  gated %u busy %u kills %u denied %u\n", s, voices_busy(),
           voice_kills, voice_denied);
    check("C: the pool holds twelve and refuses nothing", s == 12u && voice_denied == 0u);
    check("C: every note it could not keep is a counted steal", voice_kills >= 12u);
    {
        uint8_t last = (uint8_t)(40u + 23u);        /* the burst's last note, 63, on part 23 % NPART = 5 */
        printf("phase2: C last note %u on T%u: %u\n", last, (unsigned)(23u % NPART) + 1u,
               gated_in(&trk[23u % NPART], &last, 1u));
        check("C: the newest note of the burst is one of the twelve that sound",
              gated_in(&trk[23u % NPART], &last, 1u) == 1u);
    }
    check("C: every part still has at least one note (the burst is spread, not swallowed)", gated_part(&trk[0]) > 0u &&
          gated_part(&trk[1]) > 0u && gated_part(&trk[2]) > 0u && gated_part(&trk[3]) > 0u);
    for (i = 0; i < 24u; i++)                      /* every note-off of the burst: nothing may stay held */
        trk_note_off(&trk[i % NPART], 40u + i);
    check("C: after the note-offs nothing is left gated (no stuck note)", gated_all() == 0u);
    blocks(400u);
    if (active_all())
        dump_active("C");
    check("C: and the tails let go too: the pool is empty", active_all() == 0u);

    /* ---- D: accumulated releases, then a new chord: the tails pay, the chord is kept ------------------ */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_REL] = 110;                     /* a long release: twelve tails ringing at once */
        trk_note_on(&trk[p], 45u + p * 4u, 100u);
        trk_note_on(&trk[p], 60u + p * 4u, 100u);
    }
    blocks(16u);                                   /* the envelopes are up: a note released before its first
                                                    * block has no level to release, and would end at once */
    for (p = 0; p < NPART; p++) {
        trk_note_off(&trk[p], 45u + p * 4u);
        trk_note_off(&trk[p], 60u + p * 4u);
    }
    blocks(2u);
    k = voice_kills;
    printf("phase2: D accumulated  release %u busy %u\n", release_all(), voices_busy());
    check("D: twelve tails ring (twelve released voices, nothing given up yet)", release_all() == 12u && k == 0u);
    for (p = 0; p < NPART; p++) {                  /* the new chord, two notes on each of the six parts, above the
                                                    * old ones: a note that repeats would be a retrigger, not a steal */
        trk_note_on(&trk[p], 90u + p, 100u);
        trk_note_on(&trk[p], 100u + p, 100u);
    }
    s = gated_all();
    printf("phase2: D new chord  gated %u release %u kills +%u denied %u\n", s, release_all(),
           voice_kills - k, voice_denied);
    check("D: the new twelve-note chord is what sounds", s == 12u && voice_denied == 0u);
    check("D: the tails are what paid for it (twelve, none of them a sounding note)", voice_kills - k == 12u &&
          release_all() == 0u);
    blocks(400u);
    check("D: with the tails gone the pool still holds the chord", gated_all() == 12u);

    /* ---- E: six parts competing: one asks for eight beside five holding ten --------------------------- */
    fresh();
    for (p = 1; p < NPART; p++)
        for (i = 0; i < 2u; i++)
            trk_note_on(&trk[p], 48u + 3u * p + i, 100u);   /* ten voices held elsewhere */
    k = voice_kills;
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 60u + i, 100u);
    printf("phase2: E competition  part0 %u (window %u)  others %u %u %u %u %u  busy %u kills +%u\n",
           gated_part(&trk[0]), trk[0].vn, gated_part(&trk[1]), gated_part(&trk[2]), gated_part(&trk[3]),
           gated_part(&trk[4]), gated_part(&trk[5]), voices_busy(), voice_kills - k);
    check("E: the part that asked for eight really holds more than the two it started with",
          gated_part(&trk[0]) >= 6u && trk[0].vn >= 6u);
    check("E: the pool is exactly the budget, no note refused", voices_busy() == 12u && voice_denied == 0u);
    check("E: every part that was holding a note still holds one (a bass is never taken)",
          gated_part(&trk[1]) > 0u && gated_part(&trk[2]) > 0u && gated_part(&trk[3]) > 0u &&
          gated_part(&trk[4]) > 0u && gated_part(&trk[5]) > 0u);
    for (i = 0; i < 8u; i++)
        trk_note_off(&trk[0], 60u + i);
    blocks(400u);                                  /* its tails end, and the slots it lent go back */
    s = gated_part(&trk[5]);
    trk_note_on(&trk[5], 72u, 100u);                /* a part that lost its notes gets a voice again */
    check("E: after the chord lets go, a part that gave slots still gets a voice",
          gated_part(&trk[5]) == s + 1u);

    /* ---- F: a preset change (same engine) under the chord, FX on -------------------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_CHOR] = 40;
        trk[p].p[P_DLY] = 35;
        trk[p].p[P_REV] = 30;
        trk_note_on(&trk[p], 43u + p * 5u, 100u);
        trk_note_on(&trk[p], 55u + p * 5u, 100u);
    }
    blocks(8u);
    k = voice_kills;
    for (p = 0; p < NPART; p++) {
        uint32_t e = trk[p].engine, n = ENGINES[e]->npresets;
        host_preset(&trk[p], e, n ? ((uint32_t)trk[p].preset + 1u < n ? (uint32_t)trk[p].preset + 1u : 0u) : 0u);
    }
    blocks(8u);
    printf("phase2: F preset under the chord  gated %u kills +%u peak %d\n", gated_all(),
           voice_kills - k, block_peak());
    check("F: the twelve stay sounding through the load", gated_all() == 12u && voice_kills == k);
    check("F: the block still carries signal and does not sit on the rail",
          block_peak() > 1000 && block_peak() < 32767);

    /* ---- G: saturation and recovery, three cycles ----------------------------------------------------- */
    fresh();
    for (i = 0; i < 3u; i++) {
        for (k = 0; k < 24u; k++)
            trk_note_on(&trk[k % NPART], 40u + k, 100u);
        blocks(4u);
        check("G: the pool is the budget at saturation", voices_busy() == 12u && voice_denied == 0u);
        for (p = 0; p < NPART; p++)
            trk_all_off(&trk[p]);
        blocks(400u);                              /* every tail ends */
        printf("phase2: G cycle %u  active %u gated %u release %u\n", i, active_all(), gated_all(), release_all());
        if (active_all())
            dump_active("G");
        check("G: the pool comes back empty and whole", active_all() == 0u);
    }
    for (i = 0; i < 12u; i++)                      /* a single part still gets all twelve after the cycles */
        trk_note_on(&trk[0], 40u + i * 3u, 100u);
    printf("phase2: G after the cycles  one part gated %u window %u kills %u\n", gated_part(&trk[0]),
           trk[0].vn, voice_kills);
    check("G: no window was left poisoned: one part holds twelve again", gated_part(&trk[0]) == 12u);

    printf("phase2: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
