/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ISSUE #107: the reported polyphony, reproduced on the firmware's own allocator.
 *
 * The owner (issue #107, 1.4.17) reports that with DUAL CORE the counter does not go past 6-7 voices,
 * that ANALOG BRASS steals held notes early, that ANALOG PLUCK reaches twelve once earlier notes have
 * ended, and that twelve keys pressed at once steal again and settle around seven.
 *
 * This test takes the exact path of that report -- the engine's own preset, the keybed's 27 keys (the
 * HAL's fm1_in.notes mask), the key layer's chord/strum, trk_note_on -- and prints what the allocator
 * does for 1, 4, 6, 7, 8 and 12 keys, pressed one at a time and all at once, with and without SUSTAIN,
 * with the load governor set (what audio.c does above 65 % of a half) and with DUAL CORE's split.
 *
 * Class: host x86-64, the real sources (hostsim.c includes voice.c, seq.c, audio.c). No FM-1 here.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return host_dma_advanced && host_half_reads++ > 0u; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 0
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("issue107: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
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
    kb_boot_hold = 0;
    kb_layer = 0;
    for (p = 0; p < 27u; p++) { kb_note[p] = 255u; kb_on[p] = 0; kb_sn[p] = 0; kb_sdue[p] = 0; }
    song.sel = 0;
    song.grid = 0;
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
    voice_kills = 0;
}

static uint32_t sounding(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).stage != 4u;
    return n;
}
static uint32_t gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).gate && V(t, i).stage != 4u;
    return n;
}

static int preset_of(const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[ENGI_ANALOG]->npresets; i++)
        if (!strcmp(PR_NAME(&ENGINES[ENGI_ANALOG]->presets[i]), name))
            return (int)i;
    return -1;
}

/* press keys 0..n-1 of the keybed, one at a time (the player) or all in one scan (the report's "at once") */
static void press(int pi, uint32_t n, int at_once)
{
    uint32_t k;
    fresh();
    trk[0].eng_req = trk[0].engine = ENGI_ANALOG;
    if (pi >= 0) host_preset(&trk[0], ENGI_ANALOG, (uint32_t)pi);
    else trk[0].p[P_VOICE] = V_POLY;                /* ANALOG's first preset is a MONO sound: POLY is the mode here */
    if (at_once) {
        for (k = 0; k < n; k++) fm1_in.notes |= 1u << k;
        keyboard_block();
    } else
        for (k = 0; k < n; k++) { fm1_in.notes |= 1u << k; keyboard_block(); }
}

static void report(const char *what, int pi, uint32_t n, int at_once)
{
    char b[160];
    uint32_t snd, rel, fad, pk, stl, den;
    press(pi, n, at_once);
    voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
    printf("issue107: %-28s keys %2u %-9s -> gated %2u sounding %2u busy %2u kills %2u vn %2u mode %u | S %u R %u F %u PK %u STL %u DEN %u\n",
           what, n, at_once ? "at once" : "one by one", gated(&trk[0]), sounding(&trk[0]), voices_busy(),
           voice_kills, trk[0].vn, (unsigned)trk[0].p[P_VOICE], snd, rel, fad, pk, stl, den);
    snprintf(b, sizeof b, "%s: %u keys pressed %s are %u sounding voices", what, n, at_once ? "at once" : "one by one", n);
    check(b, sounding(&trk[0]) == n && gated(&trk[0]) == n);
}

int main(void)
{
    int brass = preset_of("BRASS"), pluck = preset_of("PLUCK"), pad = preset_of("PAD");
    uint32_t n;
    char b[160];
    printf("issue107: ANALOG presets BRASS=%d PLUCK=%d PAD=%d, NVOICE=%u VDEAD=%u\n", brass, pluck, pad,
           (unsigned)NVOICE, (unsigned)VDEAD);
    for (n = 1; n <= 12u; n = n == 1u ? 4u : n == 4u ? 6u : n + 1u) {
        report("generic POLY", -1, n, 0);
        report("ANALOG BRASS", brass, n, 0);
        report("ANALOG PLUCK", pluck, n, 0);
    }
    report("generic POLY", -1, 12, 1);
    report("ANALOG BRASS", brass, 12, 1);
    report("ANALOG PLUCK", pluck, 12, 1);

    /* the mode each preset loads is part of the report: a MONO sound is one voice by design */
    fresh();
    host_preset(&trk[0], ENGI_ANALOG, (uint32_t)brass);
    snprintf(b, sizeof b, "ANALOG BRASS loads as POLY (a held chord is not one voice): mode %u",
             (unsigned)trk[0].p[P_VOICE]);
    check(b, trk[0].p[P_VOICE] == V_POLY);
    fresh();
    host_preset(&trk[0], ENGI_ANALOG, (uint32_t)pluck);
    snprintf(b, sizeof b, "ANALOG PLUCK loads as POLY: mode %u", (unsigned)trk[0].p[P_VOICE]);
    check(b, trk[0].p[P_VOICE] == V_POLY);

    /* with the load governor set (audio.c above 65 % of a half): the same twelve keys, held */
    fresh();
    host_preset(&trk[0], ENGI_ANALOG, (uint32_t)brass);
    load_tight = 1;
    for (n = 0; n < 12u; n++) { fm1_in.notes |= 1u << n; keyboard_block(); }
    snprintf(b, sizeof b, "LOAD TIGHT + BRASS: twelve held keys still sound twelve (got %u)", sounding(&trk[0]));
    check(b, sounding(&trk[0]) == 12u);

    /* SUSTAIN vs no SUSTAIN: setting SUSTAIN on leaves the polyphony alone */
    fresh();
    host_preset(&trk[0], ENGI_ANALOG, (uint32_t)pluck);
    trk[0].p[P_SUS] = 100;
    for (n = 0; n < 12u; n++) { fm1_in.notes |= 1u << n; keyboard_block(); }
    snprintf(b, sizeof b, "PLUCK + SUSTAIN 100: twelve held keys sound twelve (got %u)", sounding(&trk[0]));
    check(b, sounding(&trk[0]) == 12u);

    /* DUAL CORE: the split takes parts, not voices; the same chord on part 0 */
    fresh();
    host_preset(&trk[0], ENGI_ANALOG, (uint32_t)brass);
    mc_mask_last = 0x3Fu;                       /* core 1 took every part (the worst split) */
    for (n = 0; n < 12u; n++) { fm1_in.notes |= 1u << n; keyboard_block(); }
    snprintf(b, sizeof b, "DUAL CORE (every part on core 1): twelve held keys sound twelve (got %u)",
             sounding(&trk[0]));
    check(b, sounding(&trk[0]) == 12u);

    /* release: the tails are a state of their own, not "voices" (the report read them as polyphony) */
    {
        uint32_t k, snd, rel, fad, pk, stl, den;
        press(brass, 12, 1);
        for (k = 0; k < 6u; k++) fm1_in.notes &= ~(1u << k);      /* the keybed's first six keys go up */
        keyboard_block();
        voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
        snprintf(b, sizeof b, "release: six of twelve let go read as %u sounding / %u release, peak %u",
                 snd, rel, pk);
        check(b, snd + rel == 12u && rel > 0u && pk == 12u);
    }
    /* a long chord: the allocator must not take a held note while twelve are held */
    {
        int32_t out[2 * CTL];
        uint32_t k, bnum;
        press(brass, 12, 0);
        for (bnum = 0; bnum < 400u; bnum++) mix_block(out, CTL);   /* ~0.29 s of the real render */
        (void)k;
        snprintf(b, sizeof b, "long chord: after 400 blocks BRASS still holds twelve and stole nothing (%u held, %u kills)",
                 gated(&trk[0]), voice_kills);
        check(b, gated(&trk[0]) == 12u && voice_kills == 0u);
    }
    /* fast note-on / note-off: no stuck voice, no poisoned window */
    {
        uint32_t k, snd, rel, fad, pk, stl, den;
        press(pluck, 0, 0);
        for (k = 0; k < 24u; k++) {
            uint32_t note = 36u + 2u * (k % 12u);
            trk_note_on(&trk[0], note, 100);
            trk_note_off(&trk[0], note);
        }
        trk_all_off(&trk[0]);
        voice_stats(&snd, &rel, &fad, &pk, &stl, &den);
        snprintf(b, sizeof b, "fast on/off: 24 notes struck and let go leave nothing sounding (%u/%u/%u), peak %u",
                 snd, rel, fad, pk);
        check(b, snd == 0u && gated(&trk[0]) == 0u);
    }

    printf("issue107: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
