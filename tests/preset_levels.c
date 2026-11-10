/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Every factory preset, rendered once and measured. The owner asked for the presets to be compared
 * and levelled: this is the comparison, in numbers, and the same file again after a change says
 * whether the change did what it was meant to.
 *
 *   tests/preset_levels            the table, one line a preset, sorted as the browser lists them
 *   tests/preset_levels --csv      the same behind bars for a spreadsheet
 *
 * One note, one velocity, one setup, for every sound: C4 at 100, a second and a half from the key
 * down (the attack, the body and the first of the release), rendered through the real mix at the
 * real block size. RMS is what a listener reads as loudness on a sustained sound, the peak is what
 * clips; both are reported in dBFS, and a preset is "level" when its RMS sits inside +-3 dB of the
 * median of the bank.
 *
 * This is a host measurement on x86: it proves the renders and the arithmetic, not the FM-1. */
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
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

#include <math.h>
static void pl_setup(void)
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.g[G_BPM] = 120;
}
static double db_of(int32_t q15) { return q15 > 0 ? 20.0 * log10((double)q15 / 32768.0) : -200.0; }

static int32_t pl_isqrt(uint64_t v)
{
    uint64_t r = 0, b = 1ull << 46;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return (int32_t)r;
}

int main(int argc, char **argv)
{
    static int32_t out[2 * CTL];
    uint32_t e, pi, i, n = 0, bad = 0;
    int csv = (argc > 1 && argv[1] && argv[1][0] == '-' && argv[1][1] == '-');
    int32_t rms_all[512], cnt = 0;
    for (e = 0; e < NENGINES; e++)
        n += ENGINES[e]->npresets;
    if (n > NELEM(rms_all)) {
        fprintf(stderr, "preset_levels: %u presets, the table holds %u\n", (unsigned)n, (unsigned)NELEM(rms_all));
        return 2;
    }
    for (e = 0; e < NENGINES; e++) {
        for (pi = 0; pi < ENGINES[e]->npresets; pi++) {
            uint64_t acc = 0;
            int32_t pk = 0, rms;
            pl_setup();
            host_preset(&trk[0], e, pi);
            trk_note_on(&trk[0], 60, 100);
            for (i = 0; i < 3u * FS / 2u; i += CTL) {          /* 1.5 s, block by block, as the ISR does */
                uint32_t k;
                mix_block(out, CTL);
                for (k = 0; k < 2u * CTL; k++) {
                    int32_t v = out[k], a = v < 0 ? -v : v;
                    acc += (uint64_t)((int64_t)v * v);
                    if (a > pk) pk = a;
                }
            }
            trk_note_off(&trk[0], 60);
            rms = pl_isqrt(acc / (uint64_t)(3u * (uint64_t)FS));
            if (pk > 32767) {                                 /* clipping: the peak is past full scale */
                bad++;
            }
            rms_all[cnt++] = rms;
            if (csv)
                printf("%u,%u,\"%s\",\"%s\",%.1f,%.1f\n", (unsigned)e, (unsigned)pi,
                       ENGINES[e]->name, ENGINES[e]->presets[pi].name, db_of(rms), db_of(pk));
            else
                printf("%-9s %2u %-12s rms %+6.1f dB  peak %+6.1f dB\n", ENGINES[e]->name, (unsigned)pi,
                       ENGINES[e]->presets[pi].name, db_of(rms), db_of(pk));
        }
    }
    if (!csv) {
        /* the median of the bank: the level every preset is compared against */
        int32_t j, med, off = 0;
        for (i = 1; i < cnt; i++) {                            /* insertion sort: 200 entries, once */
            int32_t v = rms_all[i];
            for (j = (int32_t)i - 1; j >= 0 && rms_all[j] > v; j--)
                rms_all[j + 1] = rms_all[j];
            rms_all[j + 1] = v;
        }
        med = cnt ? rms_all[cnt / 2] : 0;
        for (i = 0; i < cnt; i++)
            if (fabs(db_of(rms_all[i]) - db_of(med)) > 3.0)
                off++;
        printf("\n%u presets, median rms %+6.1f dB\n", (unsigned)cnt, db_of(med));
        printf("%u outside +-3 dB of the median, %u clipping\n", (unsigned)off, (unsigned)bad);
    }
    return 0;
}
