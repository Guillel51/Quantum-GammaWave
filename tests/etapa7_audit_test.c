/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 7) Regression guards for the invariants the last stages leaned on, written as the class of check that
 * would have caught their bugs:
 *
 *   ARP      every index every mode returns is < len, for len 2..64 and 256 steps. (AM_PEDAL divides by len-1
 *            and AM_ALBERTI indexes up to 3; a len of 1 or a wrong modulo is out of bounds -- exactly what the
 *            ASan runs are for, but this states it as a property.)
 *   PHASE    the wave knob accepts 0..8 and anything stored past it plays the last wave instead of an
 *            uninitialised pd_t, and no wave produces a non-finite sample. (ETAPA 4 widened the mask to 15 and
 *            added the clamp for this.)
 *   SCALES   the four tables agree: N_SCALE, SCALE_MASK, N_SCALELONG and N_SCLFAM have the same length, every
 *            mask has between 5 and 12 notes, and HIJAZ / HUNGMIN are exactly Phrygian dominant and the
 *            harmonic minor with a #4.
 *   LFO      RATE's floor is the last synced division and every division advances the phase without UB.
 *   MOD      N_MDST is a table and every destination name exists: the loop that compared against the array
 *            instead of its size hung the whole UI sweep in ETAPA 6, so the count is asserted here.
 *
 * Built with ASan/UBSan by the suite: any of these leaving its range is memory the DSP then writes through.
 *
 *   cc -O1 -g -fsanitize=address,undefined -w -Ibuild/gen -Ifirmware/src -o build/etapa7_audit_test \
 *      tests/etapa7_audit_test.c -lm && ./build/etapa7_audit_test
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../web/emu/felucca_web.c"

static int fails;
static void ck(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        fails++;
    }
}

#define NELEM_(a) (sizeof(a) / sizeof((a)[0]))

/* ---- ARP: no mode may ever return an index outside the note list ---- */
static void arp_bounds(void)
{
    static const uint32_t modes[] = {1, 2, 3, 7, 9, 10, 11, 12, 16, 17};
    uint32_t m, i, len, bad = 0;
    for (m = 0; m < NELEM_(modes); m++)
        for (len = 1; len <= 64; len++)
            for (i = 0; i < 256; i++) {
                uint32_t k = arp_step_index(modes[m], i, len);
                if (k >= len) {
                    if (bad < 4)
                        printf("ARP: mode %u i %u len %u -> %u\n", modes[m], i, len, k);
                    bad++;
                }
            }
    ck(!bad, "ARP: a mode returned an index outside the note list");
    printf("ARP: 10 modes x 64 lengths x 256 steps, every index inside\n");
}

/* ---- PHASE: the nine waves and a stored value past them ---- */
static void phase_waves(void)
{
    uint32_t w, dcw, i;
    int peak_ok = 1;
    web_boot();
    web_render(2048);
    for (i = 0; i < NENGINES; i++)
        if (ENGINES[i] && !strcmp(ENGINES[i]->name, "PHASE"))
            break;
    ck(i < NENGINES, "PHASE is not in ENGINES[]");
    if (i >= NENGINES)
        return;
    web_test_engine(i);
    for (w = 0; w <= 15u; w++) {                        /* 9..15 are stored values past the last wave */
        for (dcw = 0; dcw <= 127u; dcw += 31u) {
            uint32_t b, s;
            trk[0].p[P_E0] = (int16_t)w;
            trk[0].p[P_E2] = (int16_t)dcw;
            web_midi(0x80u | 1u, 69u, 0u);
            web_render(512);
            web_midi(0x90u | 1u, 69u, 110u);
            for (b = 0; b < 12u; b++) {
                web_render(32);
                for (s = 0; s < 32u; s++) {
                    float l = web_out_l()[s], r = web_out_r()[s];
                    if (!isfinite(l) || !isfinite(r) || fabsf(l) > 4.0f || fabsf(r) > 4.0f)
                        peak_ok = 0;
                }
            }
        }
    }
    ck(peak_ok, "PHASE: a wave (or a value past the last one) produced a bad sample");
    printf("PHASE: 16 stored wave values x 5 DCW settings, all finite and bounded\n");
}

/* ---- SCALES: the four tables, and the two appended ones ---- */
static const uint32_t HIJAZ_ = (1u << 0) | (1u << 1) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10);
static const uint32_t HUNGMIN_ = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 11);
static void scales(void)
{
    uint32_t n = (uint32_t)NELEM_(N_SCALE), i;
    ck(n == (uint32_t)NELEM_(SCALE_MASK), "SCL: N_SCALE and SCALE_MASK have different lengths");
    ck(n == (uint32_t)NELEM_(N_SCALELONG), "SCL: N_SCALE and N_SCALELONG have different lengths");
    ck(n == (uint32_t)NELEM_(N_SCLFAM), "SCL: N_SCALE and N_SCLFAM have different lengths");
    ck((uint32_t)TP[P_SCALE].max + 1u == n, "SCL: the SCALE knob's range is not the table's length");
    for (i = 0; i < n; i++) {
        uint32_t m = SCALE_MASK[i] & 0xFFFu, bits = 0, k;
        for (k = 0; k < 12u; k++)
            bits += (m >> k) & 1u;
        if (bits < 5u || bits > 12u) {
            printf("SCL: scale %u (%s) has %u notes\n", i, N_SCALE[i], bits);
            ck(0, "SCL: a scale has fewer than 5 or more than 12 notes");
        }
        ck(N_SCALELONG[i] && N_SCALELONG[i][0] && N_SCLFAM[i] && N_SCLFAM[i][0], "SCL: a scale has no long name or family");
    }
    ck((SCALE_MASK[16] & 0xFFFu) == HIJAZ_, "SCL: HIJAZ is not Phrygian dominant");
    ck((SCALE_MASK[17] & 0xFFFu) == HUNGMIN_, "SCL: HUNG MINOR is not the harmonic minor with a #4");
    printf("SCL: %u scales, four tables agree, HIJAZ and HUNG MINOR exact\n", n);
}

/* ---- LFO: the divisions ---- */
static void lfo_divisions(void)
{
    int32_t r;
    uint32_t prev = 0;
    int order = 1;
    ck(TP[P_LRATE].min == -(int32_t)NELEM_(N_LSYNC), "LFO: RATE's floor is not the last synced division");
    for (r = -(int32_t)NELEM_(N_LSYNC); r <= -1; r++) {   /* the slower the division, the shorter the step */
        char val[12];
        const char *unit = 0;
        trk[0].p[P_LRATE] = (int16_t)r;
        trk[0].p[P_LPHASE] = 0;
        trk[0].lfo_ph = 0;
        for (int k = 0; k < 8; k++)
            track_lfo_tick(&trk[0]);
        if (r > -(int32_t)NELEM_(N_LSYNC) && trk[0].lfo_ph > prev)
            order = 0;                                     /* each index must be slower than the one before */
        prev = trk[0].lfo_ph;
        param_format(&TP[P_LRATE], r, val, &unit);
        ck(val[0] != 0, "LFO: a synced division formats as nothing");
    }
    ck(order, "LFO: the divisions are not in order of speed");
    printf("LFO: %u divisions, in order, every one named\n", (unsigned)NELEM_(N_LSYNC));
}

/* ---- MOD: the table the ETAPA 6 loop tripped on ---- */
static void mod_destinations(void)
{
    uint32_t i, n = (uint32_t)NELEM_(N_MDST);
    ck(n == 27u, "MOD: N_MDST's length changed; the destination numbers are part of the preset format");
    ck((uint32_t)TP[P_M1DST].max + 1u == n, "MOD: the DEST knob's range is not the table's length");
    for (i = 0; i < n; i++)
        ck(N_MDST[i] && N_MDST[i][0], "MOD: a destination has no name");
    ck(n == (uint32_t)NELEM_(N_MSRC) + 11u, "MOD: the source table is not the one the preset format expects");
    printf("MOD: %u destinations, %u sources, both tables whole\n", n, (unsigned)NELEM_(N_MSRC));
}

int main(void)
{
    web_seed(5);
    web_boot();
    web_render(2048);
    arp_bounds();
    phase_waves();
    scales();
    lfo_divisions();
    mod_destinations();
    if (fails)
        return 1;
    printf("etapa 7 audit: 5 groups, 0 FAILED\n");
    return 0;
}
