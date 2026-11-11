/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FX > MASTER's four globals reach the audio, and each one reaches a different part of it:
 *   CLIP   drives the whole mix into the soft clip (fx.c master_out)
 *   PUNCH  the drum bus's first milliseconds and its body (eng_drum.c punch_gain)
 *   DUCK   the kick's sidechain on the parts that are not DRUM (fx.c duck_block)
 *   DREL   how long that sidechain takes to come back
 * The page is four knobs and a rack of live meters; this file is what says the knobs are not decoration.
 * The reported symptom was "FX > MASTER does not change the sound", so each parameter is rendered twice,
 * once at 0 and once at 100, on the real block (mix_block through audio_block) and the two are compared. */
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
#define FELUCCA_UAC 1
static int32_t host_tap[2 * 64];
static uint32_t host_tap_n;
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; host_tap_n = n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("master: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], 0, 0);
        trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;   /* no tail carried in */
    }
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0;
    dc_l = dc_r = dce_l = dce_r = 0;
    duck_env = 0; duck_g0 = duck_g1 = 32767;
    rng_state = 0x1234567u;                             /* libc.c: the same noise, the same S&H, twice */
    clip_eff = pnch_eff = 0;
    song.g[G_CLIP] = 0; song.g[G_PUNCH] = 0; song.g[G_DUCK] = 0; song.g[G_DREL] = 30;
    song.master_q12 = MASTER_FULL;                     /* the MASTER pot at the top: the mix, not the knob, is measured */
}

typedef struct { double rms, rms_late; uint32_t peak; } run_t;

/* one kick on a DRUM part and a held chord on a synth part; the first `ms` after the hit and then the tail */
static run_t run(uint32_t blocks)
{
    int32_t out[2 * 32];
    uint32_t b, i, early = blocks / 3u;
    double e = 0, l = 0;
    run_t r = { 0, 0, 0 };
    host_drums(&trk[0]);
    host_preset(&trk[1], ENGI_FM6, 0);
    {   /* no send: the chorus and reverb buffers would carry the previous run into this one */
        uint32_t p;
        for (p = 0; p < NPART; p++)
            trk[p].p[P_DIST] = trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
    }
    trk_note_on(&trk[0], 36, 110);                     /* the kick: the drum bus, PUNCH and DUCK's trigger */
    trk_note_on(&trk[1], 48, 100);
    trk_note_on(&trk[1], 55, 100);
    for (b = 0; b < blocks; b++) {
        audio_block(out, 32);
        for (i = 0; i < 64u; i++) {
            uint32_t v = (uint32_t)abs(out[i]);
            if (v > r.peak) r.peak = v;
        }
        for (i = 0; i < 32u; i++) {
            double s = (double)out[2u * i] * (double)out[2u * i];
            if (b < early) e += s; else l += s;
        }
    }
    r.rms = sqrt(e / (double)(early * 32u));
    r.rms_late = sqrt(l / (double)((blocks - early) * 32u));
    return r;
}
static double db(double x) { return 20.0 * log10(x > 1e-9 ? x : 1e-9); }

/* every configuration from the same starting state: fresh(), the two parts, the kick, the chord */
static run_t measured(uint32_t g_clip, uint32_t g_punch, uint32_t g_duck, uint32_t g_drel)
{
    fresh();
    song.g[G_CLIP] = (int16_t)g_clip; song.g[G_PUNCH] = (int16_t)g_punch;
    song.g[G_DUCK] = (int16_t)g_duck; song.g[G_DREL] = (int16_t)g_drel;
    return run(900);
}

int main(void)
{
    run_t off, on;

    /* CLIP: the same mix, x1 and x4 into the soft clip (fx.c master_begin / master_out) */
    off = measured(0, 0, 0, 30);
    on  = measured(100, 0, 0, 30);
    printf("master: CLIP   0: peak %u, %.2f dB | 100: peak %u, %.2f dB\n", off.peak, db(off.rms), on.peak, db(on.rms));
    check("CLIP changes the master's sound (its drive into the soft clip, not just the drawing)",
          fabs(db(on.rms) - db(off.rms)) > 0.3 || on.peak != off.peak);

    /* PUNCH: the drum bus's first milliseconds and body (eng_drum.c, pnch_eff from macro_master) */
    off = measured(0, 0, 0, 30);
    on  = measured(0, 100, 0, 30);
    printf("master: PUNCH  0: peak %u, %.2f dB | 100: peak %u, %.2f dB\n", off.peak, db(off.rms), on.peak, db(on.rms));
    check("PUNCH changes the drum bus (a kick: four milliseconds, then the body)",
          off.peak != on.peak || fabs(db(on.rms) - db(off.rms)) > 0.3);

    /* DUCK: the kick takes the parts that are not DRUM (fx.c duck_block, applied in mix_part) */
    off = measured(0, 0, 0, 30);
    on  = measured(0, 0, 100, 30);
    printf("master: DUCK   0: peak %u, %.2f dB | 100: peak %u, %.2f dB\n", off.peak, db(off.rms), on.peak, db(on.rms));
    check("DUCK lowers the parts the kick is not (the sidechain reaches the mix)",
          db(on.rms) < db(off.rms) - 0.05 || on.peak < off.peak);

    /* DREL: how long that gain takes to come back (40 .. 600 ms) */
    off = measured(0, 0, 100, 0);
    on  = measured(0, 0, 100, 100);
    printf("master: DREL   0: late %.2f dB | 100: late %.2f dB\n", db(off.rms_late), db(on.rms_late));
    check("DREL changes how fast the duck comes back", fabs(db(on.rms_late) - db(off.rms_late)) > 0.05);

    /* and with no DUCK there is no sidechain to release: DREL cannot be heard */
    off = measured(0, 0, 0, 0);
    on  = measured(0, 0, 0, 100);
    printf("master: DUCK 0, DREL 0 / 100: %.4f / %.4f dB\n", db(off.rms), db(on.rms));
    check("DREL does nothing while DUCK is 0 (no sidechain to release)", fabs(db(on.rms) - db(off.rms)) < 0.05);

    if (bad) { printf("master: %d FAILED\n", bad); return 1; }
    printf("master: all ok\n");
    return 0;
}
