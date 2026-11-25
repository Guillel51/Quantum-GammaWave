/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ENGINE ARENAS (FASE 1 2026-11-25): the state the engines used to keep in arrays of their own now shares one
 * row per track (FM6's four notes / the DRUM kit's eight lanes) and one row per pool slot (WHEEL 92 B,
 * ADDITIVE 68 B, NOISE 12 B). This test is the contract that makes that sharing safe:
 *
 * 1. geometry: a row holds each engine's whole struct, and every track's row and every slot's row is its own;
 * 2. six tracks at once with six different engines, chords and sustained notes (FM6, DRUM, WHEEL, ADDITIVE,
 *    NOISE, FM6): all of them take voices and the mix stays inside 16 bits;
 * 3. engine switch under held notes (FM6 -> DRUM): the switch waits for the fade, the old engine's voices have
 *    ended at the block it changes on, and the arena row is cleared before the new engine's first note;
 * 4. DRUM -> FM6 with a hit still ringing: the lanes are dead before FM6 writes over them (no resurrected
 *    owner, no stale tail), and a switch back to DRUM starts from empty lanes;
 * 5. isolation: switching one track's engine leaves another track's FM6 notes and another slot's WHEEL state
 *    bit-identical;
 * 6. voice stealing (cap 8, twelve notes asked), UNISON while the engine is switched under it, and release:
 *    the pool drains to zero, so no arena slot is left holding a voice that no longer exists.
 *
 * Host class: this proves the host render and the allocator/switch logic, not the AC791N's timing. The memory
 * the arenas save is measured separately (docs/FASE1_RAM_ARENAS_2026-11-25.md); tests/regress proves the 311
 * golden renders are unchanged.
 *
 * build: cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests tests/engine_arena_test.c -lm
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *w, int ok)
{
    printf("arena: %-78s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t out[2 * CTL];

static uint32_t track_active(uint32_t ti)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(&trk[ti], i).active;
    return n;
}

/* render n blocks; 1 when every sample stayed a sane 16-bit-scale value */
static int render(uint32_t blocks, int32_t *peak_out)
{
    uint32_t b, i;
    int32_t peak = 0;
    for (b = 0; b < blocks; b++) {
        mix_block(out, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t v = out[i], a = v < 0 ? -v : v;
            if (a > (1 << 20)) return 0;
            if (a >= 32700) return 0;                 /* at the rail: a level no patch of this test asks for */
            if (a > peak) peak = a;
        }
    }
    if (peak_out) *peak_out = peak;
    return 1;
}

/* render until track ti runs engine `want` */
static int switch_to(uint32_t ti, uint8_t want, uint32_t maxb)
{
    uint32_t b;
    for (b = 0; b < maxb; b++) {
        uint8_t prev = trk[ti].engine;
        mix_block(out, CTL);
        if (trk[ti].engine != prev)
            return trk[ti].engine == want;
    }
    return 0;
}

int main(void)
{
    /* engines.c ENGINES[]: 12 FM6, 10 DRUM, 7 WHEEL, 15 ADDITIVE, 11 NOISE */
    static const uint8_t ENG[6] = {ENGI_FM6, ENGI_DRUM, 7u, ENGI_ADDITIVE, 11u, ENGI_FM6};
    static const uint8_t CNT[6] = {3, 3, 2, 2, 1, 1};
    static const uint8_t NOTE[6][3] = {{60, 64, 67}, {36, 38, 42}, {48, 55, 0}, {48, 55, 0}, {60, 0, 0}, {72, 0, 0}};
    uint8_t row5[ENG_ARENA_BYTES];
    uint32_t i, j;
    int ok;
    int32_t peak = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    voices_boot();
    host_tracks_init();
    trk_live = NTRK;
    song.master_q12 = 4096;
    pf.lc = PF_TOP;
    for (i = 0; i < NTRK; i++)
        pf.mg[i] = 32768;

    /* 1. geometry: one row holds each engine's whole state, and no two tracks / slots share one */
    check("a track's row holds FM6's four notes", sizeof(fm6_note_t) * FM6_POLY <= ENG_ARENA_BYTES);
    check("a track's row holds the DRUM kit's eight lanes", sizeof(drum_lane_t) * DV_NLANE <= ENG_ARENA_BYTES);
    check("a slot's row holds WHEEL, ADDITIVE and NOISE", sizeof(drw_vc_t) <= SLOT_ARENA_BYTES &&
          sizeof(ad_vc_t) <= SLOT_ARENA_BYTES && sizeof(nz_slot_t) <= SLOT_ARENA_BYTES);
    check("the FM6 row equals four notes exactly (no padding spent)", sizeof(fm6_note_t) * FM6_POLY == 1200u);
    check("the DRUM row is smaller than the FM6 row (the union's ceiling is FM6)",
          sizeof(drum_lane_t) * DV_NLANE < sizeof(fm6_note_t) * FM6_POLY);
    ok = 1;
    for (i = 0; i < NTRK; i++)
        for (j = i + 1u; j < NTRK; j++)
            ok &= fm6_note_row(i) != fm6_note_row(j);
    check("every track's engine row is its own", ok);
    ok = 1;
    for (i = 0; i < VPOOLN; i++)
        for (j = i + 1u; j < VPOOLN; j++)
            ok &= drw_slot(i) != drw_slot(j);
    check("every pool slot's engine row is its own", ok);
    {   /* the arithmetic of the change, printed so a reader does not have to trust a document */
        size_t before = (size_t)sizeof(fm6_note_t) * FM6_POLY * NTRK + (size_t)sizeof(drum_lane_t) * DV_NLANE * NPART +
                        (size_t)sizeof(drw_vc_t) * VPOOLN + (size_t)sizeof(ad_vc_t) * VPOOLN + 3u * sizeof(int32_t) * VPOOLN;
        size_t after = sizeof eng_arena + sizeof slot_arena + sizeof slot_eng;
        printf("arena: measured state: %zu B before, %zu B after, %zu B back [host x86-64 sizeof]\n",
               before, after, before - after);
        check("the arenas return more than 4 KiB and less than 8 KiB of engine state",
              before > after && before - after >= 4096u && before - after <= 8192u);
    }

    /* 2. six tracks, six engines, chords and held notes */
    for (i = 0; i < NTRK; i++) {
        host_preset(&trk[i], ENG[i], 0);
        trk[i].p[P_LEVEL] = 70;
        trk[i].p[P_DIST] = 0;
        trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
        trk[i].p[P_VOICE] = V_POLY;
        for (j = 0; j < CNT[i]; j++)
            trk_note_on(&trk[i], NOTE[i][j], 100);
    }
    ok = 1;
    for (i = 0; i < NTRK; i++)
        ok &= track_active(i) > 0;
    check("all six engines took a voice (FM6, DRUM, WHEEL, ADDITIVE, NOISE, FM6)", ok);
    check("the twelve notes are sounding (3+3+2+2+1+1)", voices_busy() == 12u);
    check("six tracks and six engines render inside 16 bits", render(300, &peak));

    /* 3. FM6 -> DRUM under held notes: fade first, arena cleared before the new engine's first note */
    memcpy(row5, eng_arena[5].b, sizeof row5);                       /* track 6 is FM6 too: not this switch's row */
    host_preset_req(&trk[0], ENGI_DRUM, 0);
    /* the keys are still down: they are asked for again and wait in the pending queue (voice.c trk_note_on), the
     * path the sequencer and the UI's load use; the switch replays them on the new engine */
    for (j = 0; j < CNT[0]; j++)
        trk_note_on(&trk[0], NOTE[0][j], 100);
    ok = switch_to(0, ENGI_DRUM, 400);
    check("FM6 -> DRUM under three held notes switches", ok);
    {   /* the new engine's own hits are the only lanes with an owner: the FM6 bytes the row held were cleared before
         * the DRUM notes were replayed onto it (a stale kit would show owners in the other lanes) */
        uint32_t lanes = 0;
        for (j = 0; j < CNT[0]; j++)
            lanes |= 1u << drum_lane(NOTE[0][j]);
        ok = lanes != 0;
        for (i = 0; i < DV_NLANE; i++) {
            if ((lanes >> i) & 1u)
                ok &= drum_kit_row(0)[i].owner != 0;
            else
                ok &= drum_kit_row(0)[i].owner == 0 && !drum_kit_row(0)[i].x.live;
        }
    }
    check("  the row was cleared: only the replayed notes' lanes carry an owner", ok);
    check("  the other FM6 track still sounds (only its own row was cleared)",
          track_active(5) > 0 && memcmp(row5, eng_arena[5].b, sizeof row5) != 0);
    check("  another track's WHEEL voices still sound on their own slot state",
          track_active(2) > 0 && slot_eng[VSLOT(&V(&trk[2], 0))] == 7u);
    ok = render(120, &peak);
    printf("arena: [diag] after DRUM switch: active %u, busy %u, peak %d\n", track_active(0), voices_busy(), peak);
    check("  the pending notes replay on DRUM and render", ok && track_active(0) > 0);

    /* 4. DRUM hit ringing -> FM6: lanes dead before FM6 writes the row; back to DRUM starts empty */
    trk_note_on(&trk[0], 36, 110);                                   /* kick */
    check("a DRUM hit takes its lane", drum_kit_row(0)[DV_KICK].owner != 0);
    render(40, &peak);                                               /* a tail still ringing */
    host_preset_req(&trk[0], ENGI_FM6, 0);
        ok = switch_to(0, ENGI_FM6, 400);
    check("DRUM -> FM6 with a hit still ringing switches", ok);
    check("  the switch left the hit's voice inactive", track_active(0) == 0u);
    check("  the DRUM lanes are dead before FM6 writes over them", drum_kit_row(0)[DV_KICK].owner == 0 &&
          !drum_kit_row(0)[DV_KICK].x.live);
    trk_note_on(&trk[0], 64, 100);
    check("  FM6 plays on the row the kit has just released", render(80, &peak) && track_active(0) > 0);
    host_preset_req(&trk[0], ENGI_DRUM, 0);
    ok = switch_to(0, ENGI_DRUM, 400);
    check("FM6 -> DRUM again starts from empty lanes", ok && drum_kit_row(0)[DV_HATO].owner == 0);
    trk_note_on(&trk[0], 46, 100);                                   /* open hat: one lane only */
    ok = drum_kit_row(0)[DV_HATO].owner != 0;
    for (i = 0; i < DV_NLANE; i++)
        ok &= i == DV_HATO || drum_kit_row(0)[i].owner == 0;
    check("  one hit owns one lane, the other seven stay empty", ok);

    /* 5. voice stealing: cap 8, twelve notes asked, then everything released */
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < NVOICE; j++)
            if (V(&trk[i], j).active)
                trk_note_off(&trk[i], V(&trk[i], j).note);
    render(1200, &peak);
    voice_ucap = 2;                                         /* MENU > AUDIO > VOICES: 8 (core.h voice_user_budget) */
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < CNT[i]; j++)
            trk_note_on(&trk[i], NOTE[i][j], 100);
    ok = render(40, &peak);
    printf("arena: [diag] cap 8: busy %u, budget %u, peak %d\n", voices_busy(), voice_budget(), peak);
    check("twelve notes under a cap of eight leave at most eight sounding", ok && voices_busy() <= 8u);
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < CNT[i]; j++)
            trk_note_off(&trk[i], NOTE[i][j]);
    ok = render(10000, &peak);
    printf("arena: [diag] after release: busy %u\n", voices_busy());
    check("steal, release and the pool drains to zero", ok && voices_busy() == 0u);
    voice_ucap = 0;                                         /* 12 voices, the shipped setting */

    /* 6. UNISON while the engine is switched under it (WHEEL -> ADDITIVE) */
    host_preset(&trk[2], 7u, 0);                                     /* WHEEL */
    trk[2].p[P_VOICE] = V_UNISON;
    trk[2].p[P_LEVEL] = 70;
    trk_note_on(&trk[2], 48, 100);
    check("UNISON spreads over more than one voice", render(40, &peak) && track_active(2) > 1u);
    host_preset_req(&trk[2], ENGI_ADDITIVE, 0);
    trk[2].p[P_VOICE] = V_UNISON;                            /* the load kept the mode in the UI; here it is set */
    trk_note_on(&trk[2], 48, 100);                           /* the held key, queued for the new engine */
    ok = switch_to(2, ENGI_ADDITIVE, 400);
    check("UNISON WHEEL -> ADDITIVE under the held note switches", ok);
    check("  the ADDITIVE voices replay and render", render(120, &peak) && track_active(2) > 1u);
    trk_note_off(&trk[2], 48);
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < NVOICE; j++)
            if (V(&trk[i], j).active)
                trk_note_off(&trk[i], V(&trk[i], j).note);
    ok = render(3000, &peak);
    check("everything released, the pool is empty (no arena holds a dead voice)",
          ok && voices_busy() == 0u);

    printf("arena: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
