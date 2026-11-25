/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 2 (ronda de polifonía): WHO LOST THE VOICE, AND WHO WENT SILENT.
 *
 * The pool's totals (voices_given_up, the DEN figure) say the instrument was short. They do not say from which
 * part a voice was taken, nor which part's note was refused, and those are the two questions a session with six
 * tracks actually asks -- the pad that vanishes under the lead's chord, the part that plays nothing while eleven
 * voices sit idle. This file is the evidence for the per-part counters added to voice.c:
 *
 *   A. a full pool, a thirteenth note: exactly one voice given up, exactly one part named, the note sounds,
 *      nothing refused -- and the sum of the parts equals the pool's own total;
 *   B. a part with no voice at all (window 0): the refusal is counted on THAT part, the total counting it too,
 *      and no other part pays for it;
 *   C. twelve released at once: a release is not a steal, no counter moves, the tails ring and then end, the
 *      windows are intact;
 *   D. a long session (several hundred note-ons over six parts, engines and presets changing under it): no
 *      counter wraps, the sum still equals the pool's total, the peak never passes the budget, the pool ends
 *      empty and whole;
 *   E. the counters saturate: a diagnostic that wraps reads as a small number, which is a lie.
 *
 * Class: host x86-64, the real sources (hostsim.c includes voice.c, seq.c, fx.c). No FM-1 here: this says what
 * the allocator counts, not what a unit's DAC does.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("parts: %-76s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t gated_part(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).gate && V(t, i).stage != 4u;
    return n;
}
static uint32_t gated_all(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += gated_part(&trk[p]);
    return n;
}
static int note_gated(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (V(t, i).active && V(t, i).gate && V(t, i).stage != 4u && V(t, i).note == note)
            return 1;
    return 0;
}
static uint32_t release_all(void)
{
    uint32_t p, i, n = 0;    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += V(&trk[p], i).active && !V(&trk[p], i).gate && V(&trk[p], i).stage != 4u;
    return n;
}
static uint32_t taken_sum(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += vpart_taken[p];
    return n;
}
static uint32_t denied_sum(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += vpart_denied[p];
    return n;
}
static uint32_t slots_all(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += trk[p].vn;
    return n;
}
static uint32_t peak_now(void)
{
    uint32_t s, r, f, pk, st, dc;
    voice_stats(&s, &r, &f, &pk, &st, &dc);
    return pk;
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
        host_preset(&trk[p], ENGI_ANALOG, 0);
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
}

static int32_t out[2 * CTL];
static void blocks(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        mix_block(out, CTL);
}

int main(void)
{
    uint32_t p, i, s, r, one, slots0;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- A: the voice a full pool takes is attributed to the part that lost it ----------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk_note_on(&trk[p], 48u + p * 5u, 100u);
        trk_note_on(&trk[p], 55u + p * 5u, 100u);
    }
    check("A: twelve sound on six parts, nothing given up", gated_all() == 12u && voice_kills == 0u);
    check("A: no counter has moved yet", taken_sum() == 0u && denied_sum() == 0u);
    trk_note_on(&trk[0], 72u, 100u);            /* a part that already holds two asks for a third */
    one = 0;
    for (p = 0; p < NPART; p++)
        one += vpart_taken[p] != 0u;
    printf("parts: A 13 notes into 12  gated %u (%u,%u,%u,%u,%u,%u) kills %u taken %u,/%u,%u,%u,%u,%u denied %u peak %u\n",
           gated_all(), gated_part(&trk[0]), gated_part(&trk[1]), gated_part(&trk[2]), gated_part(&trk[3]),
           gated_part(&trk[4]), gated_part(&trk[5]), voice_kills, vpart_taken[0], vpart_taken[1], vpart_taken[2], vpart_taken[3],
           vpart_taken[4], vpart_taken[5], voice_denied, peak_now());
    check("A: the pool gave up exactly one voice for it", voice_kills == 1u && taken_sum() == 1u);
    check("A: and the line names exactly one part", one == 1u);
    check("A: the part that asked sounds the new note (it gave up its own, the budget was full)",
          gated_part(&trk[0]) == 2u && V(&trk[0], 0).note != 0xFFu && note_gated(&trk[0], 72u));
    check("A: still twelve sounding, nothing refused", gated_all() == 12u && voice_denied == 0u);
    check("A: the parts' sum is the pool's total", taken_sum() == voice_kills);

    /* ---- B: a part with no voice: the refusal is counted on that part, and only there ----------------- */
    fresh();
    trk[3].vn = 0;                              /* a part whose window is empty (an imported project, a cap) */
    trk_note_on(&trk[3], 60u, 100u);
    printf("parts: B no window  denied %u by part %u,%u,%u,%u,%u,%u kills %u\n", voice_denied,
           vpart_denied[0], vpart_denied[1], vpart_denied[2], vpart_denied[3], vpart_denied[4],
           vpart_denied[5], voice_kills);
    check("B: the refusal is counted, once", voice_denied == 1u && denied_sum() == 1u);
    check("B: on the part that had no voice", vpart_denied[3] == 1u);
    check("B: no other part is blamed and nothing was taken", vpart_denied[0] == 0u && vpart_denied[5] == 0u &&
                                                             taken_sum() == 0u && voice_kills == 0u);
    check("B: nothing sounds", gated_all() == 0u);

    /* ---- C: twelve released at once: a release is not a steal ---------------------------------------- */
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_REL] = 110;
        trk_note_on(&trk[p], 45u + p * 4u, 100u);
        trk_note_on(&trk[p], 60u + p * 4u, 100u);
    }
    slots0 = slots_all();
    for (p = 0; p < NPART; p++) {
        trk_note_off(&trk[p], 45u + p * 4u);
        trk_note_off(&trk[p], 60u + p * 4u);
    }
    s = gated_all();
    r = release_all();
    printf("parts: C twelve let go at once  gated %u release %u taken %u denied %u\n", s, r, taken_sum(),
           denied_sum());
    check("C: twelve ringing out, none gated", s == 0u && r == 12u);
    check("C: a release is not a steal, in the total or by part", voice_kills == 0u && taken_sum() == 0u);
    blocks(300u);                               /* the tails end */
    check("C: the tails end and the pool is empty and whole",
          gated_all() == 0u && release_all() == 0u && slots_all() == slots0);
    check("C: no counter moved while they rang", taken_sum() == 0u && denied_sum() == 0u);

    /* ---- D: a long session, engines and presets changing under it ------------------------------------ */
    fresh();
    {
        /* ANALOG, FM6 (eng_fm6.c's own ENGI_FM6 = 12), LOFI, WAVETABLE: four engines whose poly cap is the
         * allocator's, so what D measures is the pool and not an engine's own limit */
        static const uint8_t engs[4] = { ENGI_ANALOG, 12u, ENGI_LOFI, ENGI_WAVETABLE };
        uint32_t round, note = 40u, most = 0;
        for (round = 0; round < 120u; round++) {
            if (round % 4u == 0u)                       /* a sound load: the engine moves under held notes */
                for (p = 0; p < NPART; p++)
                    host_preset(&trk[p], engs[(round / 4u + p) % 4u], 0);
            for (p = 0; p < NPART; p++)
                trk_note_on(&trk[p], note + p * 7u, 90u);
            blocks(2u);
            if (voices_busy() > most)
                most = voices_busy();               /* the pool's own count, sampled every round */
            for (p = 0; p < NPART; p++)
                trk_note_off(&trk[p], note + p * 7u);
            blocks(3u);
            note = 40u + (note - 40u + 3u) % 40u;
        }
        /* FM6 keeps a released voice's tail for 5 s before the 60 ms fade that gives the slot back
         * (eng_fm6.c FM6_REL_MAX), and its own envelope can hold longer than that on the way down: measured
         * on this host, the last released FM6 voice of this scene ends around block 12800 (~9 s), so the
         * drain is 20000 blocks. Anything still active after it is a voice the engine never gives back. */
        blocks(20000u);
        printf("parts: D 120 rounds x 6 parts  kills %u taken %u,%u,%u,%u,%u,%u denied %u most %u slots %u\n",
               voice_kills, vpart_taken[0], vpart_taken[1], vpart_taken[2], vpart_taken[3], vpart_taken[4],
               vpart_taken[5], voice_denied, most, slots_all());
        check("D: the parts' sum is still the pool's total", taken_sum() == voice_kills);
        check("D: the refusal sum is still the pool's total", denied_sum() == voice_denied);
        check("D: no counter wrapped", vpart_taken[0] < 0xFFFFu && vpart_taken[5] < 0xFFFFu);
        check("D: the pool never held more than the budget, and six parts did sound",
              most <= NVOICE && most >= 6u);
        if (voices_busy() || release_all()) {   /* what is left, if anything: a note, a gate, a part */
            uint32_t q, z;
            for (q = 0; q < NPART; q++)
                for (z = 0; z < NVOICE; z++)
                    if (V(&trk[q], z).active)
                        printf("parts: D left  part %u slot %u note %u gate %u stage %u env %d\n", q, z,
                               V(&trk[q], z).note, V(&trk[q], z).gate, V(&trk[q], z).stage,
                               (int)V(&trk[q], z).env_out);
        }
        check("D: the pool ends empty", voices_busy() == 0u && release_all() == 0u);
        check("D: no window was lost on the way", slots_all() >= 12u && slots_all() <= VDEAD);
        check("D: every part kept its window", trk[0].vn >= 1u && trk[5].vn >= 1u);
    }

    /* ---- E: the counters saturate ------------------------------------------------------------------- */
    {
        uint16_t c = 0;
        for (i = 0; i < 70000u; i++)
            vpart_add(&c);
        check("E: a counter fills at 65535 and stays there", c == 0xFFFFu);
    }

    printf("parts: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
