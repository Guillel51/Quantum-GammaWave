/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The real audio ISR with a simulated DMA and clock: overload must free work
 * within one block without cutting another part's lead or bass. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;              /* audio.c's: TIMER5 nested in the render (main.c) */
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return host_dma_advanced && host_half_reads++ > 0u; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1                                  /* audio.c taps USB audio: here into host_tap */
static int32_t host_tap[2 * 64];
static uint32_t host_tap_n;
static void uac_tap(const int32_t *out, uint32_t n) { memcpy(host_tap, out, 8u * n); host_tap_n = n; }
static uint32_t host_acks_at_start;
static void uac_render_start(void) { host_acks_at_start = host_acks; }   /* (just before the blocks) */
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;                        /* the MASTER pot as the ADC reads it (-1: timeout) */
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include "../firmware/src/master.c"

static int bad;

#include <math.h>
/* (1.4.4) POLY, FROM THE OUTPUT: three keys on one part must put three voices in the air and three notes in the
 * spectrum, with or without the load governor set. The owner reports no polyphony on the unit; this proves the
 * host side and pins the behaviour that must not regress. */
static int bad;
static void check(const char *what, int ok)
{
    printf("poly: %-76s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh2(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (p = 0; p < NPART; p++) host_preset(&trk[p], 0, 0);
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    audio_cpu_rem = audio_halves = audio_max_us = 0;
    shed_req = 0; shed_count = 0; shed_over = 0;
    load_tight = 0; tight_calm = 0; memset((void *)&load_diag, 0, sizeof load_diag);
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
    transport_req = panic_req = 0;
    song.master_q12 = 4096;
}
/* the energy at one MIDI note's fundamental, over 4096 mono samples (Gibson's Goertzel) */
static double tone(double *x, uint32_t n, uint32_t note)
{
    double f = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0), w = 2.0 * 3.14159265358979 * f / 44100.0;
    double c = 2.0 * cos(w), s1 = 0, s2 = 0, mag;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    mag = s1 * s1 + s2 * s2 - c * s1 * s2;
    return mag > 0 ? sqrt(mag) / (double)n : 0;
}
static uint32_t held_n(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += V(t, i).active && V(t, i).gate && V(t, i).stage != 4u;
    return n;
}
static void chord(const char *what, int tight)
{
    static double x[4096];
    int32_t out[2 * CTL];
    uint32_t i, k, n = 0;
    double a, b, c, m;
    fresh2();
    load_tight = tight ? 1u : 0u;
    trk[0].engine = trk[0].eng_req = ENGI_ANALOG;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk_note_on(&trk[0], 48, 100);
    trk_note_on(&trk[0], 52, 100);
    trk_note_on(&trk[0], 55, 100);
    for (i = 0; i < 4u; i++) mix_block(out, CTL);        /* the attack settles */
    while (n + CTL <= 4096u) {
        mix_block(out, CTL);
        for (k = 0; k < CTL; k++) x[n++] = (double)(out[2 * k] + out[2 * k + 1]) * 0.5;
    }
    a = tone(x, n, 48); b = tone(x, n, 52); c = tone(x, n, 55);
    m = a > b ? (a > c ? a : c) : (b > c ? b : c);
    check(what, held_n(&trk[0]) == 3u && m > 0 && a > m * 0.2 && b > m * 0.2 && c > m * 0.2);
    printf("poly:   voices %u of 3, fundamentals C3 %.3f E3 %.3f G3 %.3f\n", held_n(&trk[0]), a, b, c);
}
/* (1.4.4) THE OTHER HALF OF THE OWNER'S REPORT. A sound carries its own voice mode: ui.c apply_preset_to sets
 * P_VOICE from the preset (mono -> LEGATO), so a bass or lead patch plays one note and a track reads as having no
 * polyphony no matter what the VOICE page said before the load. POLY on the VOICE page wins until the next load. */
static void preset_mode(void)
{
    uint32_t i, mp = NENGINES;
    for (i = 0; i < ENGINES[ENGI_ANALOG]->npresets; i++)
        if (ENGINES[ENGI_ANALOG]->presets[i].mono) { mp = i; break; }
    fresh2();
    host_preset(&trk[0], ENGI_ANALOG, mp);
    check("a MONO sound loads the track as LEGATO: the mode is the sound's", mp < NENGINES && trk[0].p[P_VOICE] == V_LEGATO);
    trk_note_on(&trk[0], 48, 100); trk_note_on(&trk[0], 52, 100);
    check("..so two keys on it sound one note (it is not broken poly, it is the sound)", held_n(&trk[0]) == 1u);
    trk[0].p[P_VOICE] = V_POLY;                        /* the VOICE page, until the next sound load */
    trk_note_on(&trk[0], 55, 100);
    check("..and POLY on the VOICE page gives the notes back", held_n(&trk[0]) == 2u);
}
/* (1.4.2.1) NO POP AT POWER-ON: the DAC output ramps in over the first blocks instead of starting at level, so the
 * codec's own transient under the splash has nothing to jump from. A loud note from the first block: block 0 must
 * be a fraction of the level the same note reaches once the fade is over. */
static void boot_fade(void)
{
    int32_t out[2 * CTL];
    double pk0 = 0, pk1 = 0;
    uint32_t b, k;
    fresh2();
    trk[0].engine = trk[0].eng_req = ENGI_ANALOG;
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 60, 120);
    for (b = 0; b < 40u; b++) {
        audio_block(out, CTL);
        for (k = 0; k < CTL; k++) {
            double v = (double)(out[2 * k] + out[2 * k + 1]) * 0.5;
            v = v < 0 ? -v : v;
            if (b == 0u && v > pk0) pk0 = v;
            if (b >= 30u && v > pk1) pk1 = v;
        }
    }
    check("power-on: the output fades in instead of starting at level (no pop under the splash)",
          pk1 > 0 && pk0 < pk1 * 0.25);
    printf("poly:   first block peak %.0f, after the fade %.0f\n", pk0, pk1);
}
int main(void)
{
    boot_fade();
    chord("POLY: three keys, three voices, three notes in the spectrum", 0);
    chord("POLY under the load governor: still three voices", 1);
    preset_mode();
    printf("poly_test: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
