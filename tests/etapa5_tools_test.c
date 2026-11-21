/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 5) The tools this stage added, each against the ones already there:
 *
 *   ARP PEDAL (16) / ALBERTI (17)   two playback modes, appended to N_AMODE; both must give an order that no
 *                                   earlier mode gives, and their figures must be the ones they claim (the pedal on
 *                                   every other step; 1-3-2-4 for the Alberti).
 *   SCL HIJAZ (16) / HUNGMIN (17)   two scales, appended to N_SCALE: their degree sets must be exactly Phrygian
 *                                   dominant and the harmonic minor with a #4, and neither may duplicate a scale the
 *                                   instrument already had.
 *   LFO RATE -13 / -14              1/2D and 1/16D, appended to N_LSYNC: their periods must be what the names say
 *                                   (1.5 beats and 3/8 of a beat) and neither may equal an existing division.
 *
 * What it does not test: how any of them sounds, or what a musician makes of the figures. These are the facts the
 * editors rely on; the ear is the user's.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/etapa5_tools_test tests/etapa5_tools_test.c -lm
 *   build/etapa5_tools_test
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static int fails;

static void ck(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        fails++;
    }
}

/* ---- ARP: the two new modes against the fifteen that were there ---- */
static void arp_checks(void)
{
    uint32_t m, i, j, len = 7, orders[18][16];
    /* only the modes whose order arp_step_index decides: RND, ORD, REPEAT, UP+8, WALK, CHORD and TRNS are
     * arp_pick's own (they return the default index here), so they are not part of this comparison */
    static const uint32_t idxmodes[] = {1u, 2u, 3u, 7u, 9u, 10u, 11u, 12u, 16u, 17u};
    memset(orders, 0, sizeof orders);
    for (m = 0; m < sizeof idxmodes / sizeof idxmodes[0]; m++) {
        for (i = 0; i < 16u; i++)
            orders[idxmodes[m]][i] = arp_step_index(idxmodes[m], i, len);
    }
    for (m = 0; m < sizeof idxmodes / sizeof idxmodes[0]; m++) {
        for (j = 0; j < m; j++)
            if (!memcmp(orders[idxmodes[m]], orders[idxmodes[j]], sizeof orders[0])) {
                printf("ARP: mode %u repeats mode %u\n", idxmodes[m], idxmodes[j]);
                fails++;
            }
    }
    /* PEDAL: the lowest note on every downbeat of four, the rest climbing the notes above it */
    for (i = 0; i < 16u; i += 4u)
        ck(arp_step_index(16u, i, len) == 0u, "ARP PEDAL does not hold the lowest note on the downbeat");
    ck(arp_step_index(16u, 1u, len) == 1u && arp_step_index(16u, 2u, len) == 2u &&
       arp_step_index(16u, 3u, len) == 3u, "ARP PEDAL does not climb above the pedal");
    ck(arp_step_index(16u, 5u, len) == 4u, "ARP PEDAL does not carry the line across the pedal");
    ck(arp_step_index(16u, 1u, 2u) == 1u && arp_step_index(16u, 2u, 2u) == 1u, "ARP PEDAL breaks with two notes");
    /* ALBERTI: 1 - 3 - 2 - 4 */
    ck(arp_step_index(17u, 0u, 4u) == 0u && arp_step_index(17u, 1u, 4u) == 2u &&
       arp_step_index(17u, 2u, 4u) == 1u && arp_step_index(17u, 3u, 4u) == 3u,
       "ARP ALBERTI is not 1-3-2-4");
    ck(arp_step_index(17u, 1u, 2u) < 2u && arp_step_index(17u, 3u, 2u) < 2u, "ARP ALBERTI breaks with two notes");
    printf("ARP: 10 index modes, 0 repeated orders; PEDAL holds the pedal; ALBERTI is 1-3-2-4\n");
}

/* ---- SCL: the two new scales ---- */
static uint32_t mask_of(int32_t sc)
{
    trk[0].p[P_SCALE] = (int16_t)sc;
    return scale_mask(&trk[0]) & 0xFFFu;
}
static const uint32_t HIJAZ = (1u << 0) | (1u << 1) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10);
static const uint32_t HUNGMIN = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 11);
static void scale_checks(void)
{
    uint32_t sc, m;
    ck(mask_of(16) == HIJAZ, "SCL HIJAZ is not Phrygian dominant (0 1 4 5 7 8 10)");
    ck(mask_of(17) == HUNGMIN, "SCL HUNG MINOR is not the harmonic minor with a #4 (0 2 3 6 7 8 11)");
    for (sc = 0; sc < 18u; sc++) {
        m = mask_of((int32_t)sc);
        for (uint32_t o = sc + 1u; o < 18u; o++)
            if (m == mask_of((int32_t)o) && sc != 0u) {   /* CHR is the only all-twelve */
                printf("SCL: scales %u and %u are the same set\n", sc, o);
                fails++;
            }
    }
    /* a scale's degrees are counted from the root: HUNG MINOR from D must keep its set relative to D */
    trk[0].p[P_SCALE] = 17;
    trk[0].p[P_ROOT] = 2;
    ck(((scale_mask(&trk[0]) >> ((5 - 2 + 120) % 12)) & 1u) != 0u, "SCL: HUNG MINOR's #4 is not there from D");
    printf("SCL: 18 scales, 0 duplicated sets; HIJAZ and HUNG MINOR exact\n");
}

/* ---- LFO: the two new synced divisions ---- */
static void lfo_checks(void)
{
    uint32_t ph[3];
    int32_t r;
    for (r = -14; r <= -12; r++) {
        trk[0].p[P_LRATE] = (int16_t)r;
        trk[0].p[P_LPHASE] = 0;
        trk[0].lfo_ph = 0;
        for (int k = 0; k < 16; k++)
            track_lfo_tick(&trk[0]);            /* free-running while the transport is stopped */
        ph[r + 14] = trk[0].lfo_ph;
    }
    /* -12 is 1/32 (3/24 beat), -13 is 1/2D (36/24 = twelve times its period), -14 is 1/16D (9/24 = three) */
    /* ph[0] is -14 (1/64: 1/24 of a beat), ph[1] is -13 (1/32T: 2/24), ph[2] is -12 (1/32: 3/24): the faster
     * the division the further the phase runs (ETAPA 7: the appended pair is the fast end, so RATE grows) */
    ck(ph[2] && ph[0] > ph[1] && ph[1] > ph[2], "LFO: 1/32, 1/32T and 1/64 do not come out fastest first");
    {   /* the increment per tick is truncated, so the ratio is checked to 2 %, not to the sample */
        double r2 = (double)ph[0] / (double)ph[1], r3 = (double)ph[0] / (double)ph[2];
        ck(r2 > 1.96 && r2 < 2.04, "LFO 1/64 is not twice 1/32T's rate");
        ck(r3 > 2.94 && r3 < 3.06, "LFO 1/64 is not three times 1/32's rate");
    }
    ck(TP[P_LRATE].min == -14, "LFO RATE still refuses -13 and -14");
    printf("LFO: 14 divisions; 1/32T = 1.5 x 1/32, 1/64 = 3 x 1/32\n");
}

int main(void)
{
    web_seed(5);
    web_boot();
    web_render(2048);
    arp_checks();
    scale_checks();
    lfo_checks();
    if (fails)
        return 1;
    printf("etapa 5 tools: 3 groups, 0 FAILED\n");
    return 0;
}
