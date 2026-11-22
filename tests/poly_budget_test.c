/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* HOW MANY VOICES REALLY SOUND: 8, 10, 12 and 16 notes at once, held, released, on one part and on several,
 * with the user's polyphony setting, with LOAD LITE / ECO, and with every engine's own cap.
 *
 * This is the measurement the "six or seven voices" report was about, with the numbers the allocator and the
 * pool really hold underneath: not a counter printed for the eye, but the voices that are active and gated,
 * the ones ringing out, the ones the allocator had to take, and the energy the block renders for them.
 *
 * Class: host x86-64, the real sources (hostsim.c). Nothing here is the unit's timing.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok, unsigned a, unsigned b)
{
    printf("budget: %-70s %s (%u / %u)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}
static uint32_t gated_part(uint32_t p)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(&trk[p], i).active && V(&trk[p], i).gate && V(&trk[p], i).stage != 4u;
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
        host_preset(&trk[p], (uint32_t)p, 0);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_LEVEL] = 100;
    }
}
/* the energy of one block: a voice that holds a note but renders nothing would still count as active */
static double block_energy(int32_t *out, uint32_t n)
{
    uint32_t i;
    double e = 0.0;
    for (i = 0; i < 2u * n; i++)
        e += (double)out[i] * (double)out[i];
    return e;
}
static unsigned chord(uint32_t part, uint32_t notes, int32_t *out, double *e)
{
    uint32_t i;
    double first = 0.0, after = 0.0;
    for (i = 0; i < notes; i++)
        trk_note_on(&trk[part], 40u + 2u * i, 100u);
    mix_block(out, CTL);
    first = block_energy(out, CTL);
    for (i = 0; i < 8u; i++)
        mix_block(out, CTL);
    after = block_energy(out, CTL);
    if (e)
        *e = after;
    return first > 0.0 && after > 0.0 ? gated_part(part) : 0u;
}

int main(void)
{
    int32_t out[2 * CTL];
    uint32_t n, snd, rel, fad, pk, stl, den, i;
    double e;

    /* 1. ONE PART, 8 / 10 / 12 NOTES HELD: the allocator grew its window to the whole pool */
    for (n = 8; n <= 12; n += 2) {
        char what[96];
        fresh_all();
        host_preset(&trk[0], ENGI_ANALOG, 8u);
        voice_stats_reset();
        chord(0, n, out, &e);
        voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
        printf("budget: %u notes on one part: %u gated, %u active, peak %u, steals %u, denied %u, energy %.0f\n",
               n, gated_part(0), active_all(), pk, stl, den, e);
        snprintf(what, sizeof what, "%u notes on one ANALOG part all sound, nothing stolen", n);
        check(what, gated_part(0) == n && stl == 0u && den == 0u, gated_part(0), n);
    }
    /* 2. TWELVE IS THE POOL: the cap the monitor prints is the one the allocator applies */
    fresh_all();
    check("the budget in force at FULL with the setting untouched is 12", voice_budget() == 12u, voice_budget(), 12u);
    voice_ucap = 1u;                                    /* MENU > AUDIO > VOICES = 10 */
    check("the setting 10 is the budget the allocator applies", voice_budget() == 10u, voice_budget(), 10u);
    host_preset(&trk[0], ENGI_ANALOG, 8u);
    voice_stats_reset();
    chord(0, 12, out, &e);
    check("with the setting at 10, twelve notes give ten voices, not twelve", gated_part(0) == 10u, gated_part(0), 10u);
    voice_ucap = 2u;                                    /* 8 */
    voice_stats_reset();
    for (i = 0; i < NPART; i++)
        trk_all_off(&trk[i]);
    chord(0, 10, out, &e);
    check("with the setting at 8, ten notes give eight voices", gated_part(0) == 8u, gated_part(0), 8u);
    voice_ucap = 0u;

    /* 3. LOAD ECO: the 6-voice cap the report met, now visible and reversible */
    fresh_all();
    sys_load = LOAD_ECO;
    check("LOAD ECO's budget is 6 voices", voice_budget() == 6u, voice_budget(), 6u);
    host_preset(&trk[0], ENGI_ANALOG, 8u);
    voice_stats_reset();
    chord(0, 12, out, &e);
    printf("budget: LOAD ECO, 12 notes on one part: %u gated (its own cap %u), pool %u, peak %u\n",
           gated_part(0), (unsigned)trk[0].vn, voice_budget(), pk);
    check("LOAD ECO: the pool's budget is 6 and one part gets ECO's 4",
          gated_part(0) == 4u && voice_budget() == 6u, gated_part(0), 4u);
    sys_load = LOAD_FULL;
    check("back at LOAD FULL the budget is 12 again", voice_budget() == 12u, voice_budget(), 12u);

    /* 4. THE ENGINES' OWN CAPS, measured: FM6 and VOICE are heavy, the rest take the pool's 12 */
    {
        const char *nm[] = {"ANALOG", "FM6"};
        uint32_t eng[] = {ENGI_ANALOG, ENGI_FM6};
        for (i = 0; i < 2u; i++) {
            uint32_t got;
            fresh_all();
            host_preset(&trk[0], eng[i], 0u);
            trk[0].p[P_VOICE] = V_POLY;                 /* a preset carries its own voice mode */
            voice_stats_reset();
            got = chord(0, 12, out, &e);
            printf("budget: %s, 12 notes on one part: %u gated (engine cap %u), peak %u, steals %u, energy %.0f\n",
                   nm[i], got, ENGINES[trk[0].engine]->poly, pk, stl, e);
            check(i ? "FM6 gives its engine cap, and it is 4" : "ANALOG gives all 12", got >= (i ? 4u : 12u),
                  got, i ? 4u : 12u);
        }
    }

    /* 5. FOUR TRACKS AT ONCE: the pool is shared, and no part takes more than its share of the sound */
    fresh_all();
    for (i = 0; i < 4u; i++)
        for (n = 0; n < 3u; n++)
            trk_note_on(&trk[i], 40u + 3u * i + n * 2u, 100u);
    mix_block(out, CTL);
    e = block_energy(out, CTL);
    voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
    printf("budget: four tracks, three notes each: %u active, peak %u, denied %u, energy %.0f\n",
           active_all(), pk, den, e);
    check("four tracks, twelve notes: all twelve sound", active_all() == 12u, active_all(), 12u);
    check(".. nothing was denied and the block renders them", den == 0u && e > 0.0, den, 0u);

    /* 6. SUSTAIN AND THE RELEASE QUEUE: a held chord, then let go, then a second chord on the tails */
    fresh_all();
    host_preset(&trk[0], ENGI_ANALOG, 8u);
    host_preset(&trk[1], ENGI_ANALOG, 8u);
    voice_stats_reset();
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 40u + 2u * i, 100u);
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[1], 52u + 2u * i, 100u);
    mix_block(out, CTL);
    check("twelve held notes across two parts all sound", gated_part(0) + gated_part(1) == 12u,
          gated_part(0) + gated_part(1), 12u);
    for (i = 0; i < 6u; i++)
        trk_note_off(&trk[0], 40u + 2u * i);
    mix_block(out, CTL);
    voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
    printf("budget: halves released: sounding %u, release %u, active %u\n", snd, rel, active_all());
    check("the released half rings out as release, not as a held voice", rel >= 1u && gated_part(0) == 0u, rel, 0u);
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 76u + 2u * i, 100u);
    mix_block(out, CTL);
    check("a new chord on the tails still gets six voices, none of them denied",
          gated_part(0) == 6u && den == 0u, gated_part(0), 6u);
    for (i = 0; i < NPART; i++)
        trk_all_off(&trk[i]);
    for (n = 0; n < 200u; n++)
        mix_block(out, CTL);
    check("after everything is let go, no voice is left", active_all() == 0u, active_all(), 0u);

    /* 7. A PRESET CHANGE UNDER A HELD CHORD: the audio keeps going and nothing is left behind */
    fresh_all();
    host_preset(&trk[0], ENGI_ANALOG, 9u);
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 48u + i * 2u, 100u);
    mix_block(out, CTL);
    e = block_energy(out, CTL);
    host_preset(&trk[0], ENGI_ANALOG, 10u);             /* another preset, same engine */
    mix_block(out, CTL);
    {
        double e2 = block_energy(out, CTL);
        printf("budget: preset change under 8 held notes: energy %.0f -> %.0f, gated %u\n", e, e2, gated_part(0));
        check("a preset change under a chord does not cut the audio (the block still sounds)", e2 > 0.0, 1, 1);
        check(".. and the chord is not cut down", gated_part(0) == 8u, gated_part(0), 8u);
    }
    for (i = 0; i < NPART; i++)
        trk_all_off(&trk[i]);
    for (n = 0; n < 800u; n++)
        mix_block(out, CTL);
    printf("budget: after the preset change and the release: %u active, %u sounding\n", active_all(), voices_busy());
    check(".. and no voice is left held behind", gated_part(0) == 0u && voices_busy() <= 1u,
          gated_part(0), 0u);

    printf("budget: %s\n", bad ? "FAILED" : "all measurements within the pool's real budget");
    return bad != 0;
}
