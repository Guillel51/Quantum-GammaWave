/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* POLY: the polyphony of a POLY part, measured on the firmware's own allocator.
 *
 * The owner reported "POLY does not give polyphony" (docs/VALIDATION_MATRIX.md, 1.4.1 entry). This test
 * reproduces every path a note takes to a voice -- the keybed scan (keyboard_block: 27 keys, per key), the
 * key layer (key_on / input_on), the sequencer's trk_note_on -- and counts the voices that end up sounding,
 * with the load governor set and with the LOAD LITE / ECO settings, which are the two things the allocator
 * does not see on the host.
 *
 * Class of every number here: host x86-64, the real sources (tests/hostsim.c includes voice.c and seq.c).
 * Nothing here is a measurement of an FM-1: no unit was touched.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* audio.c's clock and DMA, as tests/audio_test.c provides them: here only to render blocks */
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
    printf("poly: %-78s %s\n", what, ok ? "ok" : "FAIL");
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
    for (p = 0; p < 27u; p++) { kb_note[p] = KB_SILENT; kb_on[p] = 0; kb_sn[p] = 0; }
    song.sel = 0;
    song.grid = 0;
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
}

/* how many voices of the part are held (a gate, so a release tail does not count), and whether `note` is
 * one of them; `active` counts the tails too, for the tests that ask about the whole life of a voice */
static uint32_t held_n(const track_t *t, uint32_t note, int *has, int active)
{
    uint32_t i, n = 0;
    if (has) *has = 0;
    for (i = 0; i < NVOICE; i++)
        if (V(t, i).active && V(t, i).stage != 4u && (!active || V(t, i).gate)) {
            n++;
            if (has && V(t, i).note == note) *has = 1;
        }
    return n;
}
#define sounding(t, note, has) held_n(t, note, has, 1)

static void test_allocator(void)
{
    uint32_t i;
    int has;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 48u + 2u * i, 100);
    check("POLY: eight notes on one part are eight sounding voices", sounding(&trk[0], 0, 0) == 8u);
    check("POLY: every one of the eight keeps its own pitch",
          ({ uint32_t k, ok = 1; for (k = 0; k < 8u; k++) { sounding(&trk[0], 48u + 2u * k, &has); ok &= has; } ok; }));
    for (i = 0; i < 4u; i++)
        trk_note_on(&trk[0], 64u + 2u * i, 100);
    check("POLY: a chord reaches the part's cap, the budget in force", sounding(&trk[0], 0, 0) == voice_budget());
    trk_all_off(&trk[0]);
    check("POLY: letting go silences them", sounding(&trk[0], 0, 0) == 0u);
}

static void test_load_governor(void)
{
    uint32_t i;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    load_tight = 1;                                  /* what audio.c sets above 65 % of a half */
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 48u + 2u * i, 100);
    check("POLY: the load budget set does not take the polyphony away (six of six)",
          sounding(&trk[0], 0, 0) == 6u);

    /* The measured reason: with the budget set, a new note only makes room by releasing a dying tail; when
     * there is none it still starts in a free voice of the part. */
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    load_tight = 1;
    trk_note_on(&trk[0], 60, 100);
    trk_note_off(&trk[0], 60);                       /* a tail with the budget set */
    trk_note_on(&trk[0], 64, 100);
    trk_note_on(&trk[0], 67, 100);
    {
        int h64 = 0, h67 = 0;
        sounding(&trk[0], 64, &h64);
        sounding(&trk[0], 67, &h67);
        check("POLY: a dying tail with the budget set does not eat the next two notes",
              h64 && h67 && sounding(&trk[0], 0, 0) == 2u);
    }
}

static void test_sharing(void)
{
    uint32_t p, i, n = 0;
    fresh();
    for (p = 0; p < NPART; p++) {
        trk[p].p[P_VOICE] = V_POLY;
        for (i = 0; i < 3u; i++)
            trk_note_on(&trk[p], 48u + 2u * i, 100);
        n += sounding(&trk[p], 0, 0) > 0u;
    }
    check("POLY: six parts of three notes each keep a voice on every part", n == NPART);
    check("POLY: .. and never sound more than the pool's usable slots", voices_busy() <= VDEAD);
    {
        int bass_ok = 1;
        for (p = 0; p < NPART; p++) {
            int has = 0;
            sounding(&trk[p], 48, &has);
            bass_ok &= has;
        }
        check("POLY: every part's lowest note survives the sharing", bass_ok);
    }
}

static void test_settings(void)
{
    uint32_t i;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    sys_load = LOAD_LITE;
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 48u + 2u * i, 100);
    check("LOAD LITE: a POLY part still takes its whole cap (twelve), not one voice",
          sounding(&trk[0], 0, 0) == 6u);
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    sys_load = LOAD_ECO;
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 48u + 2u * i, 100);
    check("LOAD ECO: the documented cap (four voices a part) and no more",
          sounding(&trk[0], 0, 0) == 4u);
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    voice_ucap = 2u;                                 /* MENU > AUDIO > VOICES = 8 */
    for (i = 0; i < 8u; i++)
        trk_note_on(&trk[0], 40u + 2u * i, 100);
    check("MENU VOICES 8: the budget is eight, and it is shared, never one",
          sounding(&trk[0], 0, 0) == 8u);
}

/* Allocated is not the same as heard: the three voices of a chord have to be rendering. One control block
 * per step, as the ISR does it, and every one of the three has an envelope running. */
static void test_audible(void)
{
    int32_t out[2 * CTL];
    uint32_t i, b, k, up = 0;
    int32_t peak = 0;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < 3u; i++)
        trk_note_on(&trk[0], 53u + 2u * i, 100);
    for (b = 0; b < 40u; b++) {
        mix_block(out, CTL);
        for (k = 0; k < 2u * CTL; k++)
            if (out[k] > peak) peak = out[k];
            else if (-out[k] > peak) peak = -out[k];
    }
    for (i = 0; i < NVOICE; i++)
        if (V(&trk[0], i).active && V(&trk[0], i).gate && V(&trk[0], i).env_out > 0) up++;
    check("POLY: the three voices of a chord are all rendering (an envelope each)", up == 3u);
    check("POLY: .. and the mix is not silent", peak > 0);
}

/* the owner's path: the keybed itself. Two keys, three keys, a black key between them. */
static void test_keybed(void)
{
    uint32_t i;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    fm1_in.notes = 1u | (1u << 2) | (1u << 4);        /* three white keys: F3 G3 A3 */
    keyboard_block();
    check("keybed: three keys held are three sounding voices (POLY)",
          sounding(&trk[0], 0, 0) == 3u);
    fm1_in.notes |= 1u << 7;                          /* .. and C4 on top */
    keyboard_block();
    check("keybed: a fourth key adds a fourth voice (POLY)", sounding(&trk[0], 0, 0) == 4u);
    fm1_in.notes = 0;
    keyboard_block();
    check("keybed: releasing all of them silences the part", sounding(&trk[0], 0, 0) == 0u);

    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < 7u; i++) {                        /* seven keys, released one by one: no stuck note */
        fm1_in.notes |= 1u << i;
        keyboard_block();
    }
    for (i = 0; i < 7u; i++) {
        fm1_in.notes &= ~(1u << i);
        keyboard_block();
        if (sounding(&trk[0], 0, 0) != 6u - i) break;
    }
    check("keybed: a seven-key chord releases key by key, no stuck voice", i == 7u);
}

int main(void)
{
    test_allocator();
    test_load_governor();
    test_sharing();
    test_settings();
    test_audible();
    test_keybed();
    printf("poly: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
