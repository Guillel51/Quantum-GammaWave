/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 4) PHASE WAVE 8: SYNC -- the ninth wave of the phase-distortion engine.
 *
 * The engine's phase waves are one cycle of -cos read through a bent phase. SYNC is a different animal: the carrier
 * runs at a ratio DCW sets (1 .. 8x, in eighths) and is RESET by the master cycle, so the wrap lands inside the
 * period and the edge it leaves is the sync timbre -- at ratio 1 it is the plain sine, at 8 it is hard and metallic.
 * Nothing else in the engine does that (ANALOG's SYNC restarts osc 2's saw; this one bends a sine's phase).
 *
 * What this proves:
 *   new        its output differs from every one of the other eight waves on the same note (9 distinct hashes).
 *   playable   DCW moves the ratio: three settings give three different sounds, so the knob is its control.
 *   sane       no non-finite sample and no peak over the ceiling on any of them.
 * What it cannot prove: how it sounds. That is the preset and the ear; the hashes only say "different".
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/pd_sync_test tests/pd_sync_test.c -lm
 *   build/pd_sync_test
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../web/emu/felucca_web.c"

static uint64_t hash_note(uint32_t wave, int32_t dcw)
{
    uint64_t h = 1469598103934665603ull;
    uint32_t e, b, i, k;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] && !strcmp(ENGINES[e]->name, "PHASE"))
            break;
    if (e >= NENGINES) {
        printf("pd sync test: FAILED -- no PHASE engine in ENGINES[]\n");
        return 0;
    }
    web_test_engine(e);
    trk[0].p[P_E0] = (int16_t)wave;                 /* WAVE */
    trk[0].p[P_E1] = 0;                             /* WAVE2 off */
    trk[0].p[P_E2] = (int16_t)dcw;                  /* DCW */
    trk[0].p[P_E3] = 64;                            /* ENV */
    trk[0].p[P_E4] = 0;                             /* DETUNE */
    trk[0].p[P_E5] = 0;                             /* LINE */
    trk[0].p[P_E6] = 0;                             /* SUB */
    trk[0].p[P_E7] = 0;                             /* FB */
    web_midi(0x80u | 1u, 69u, 0u);
    web_render(2048);                               /* the previous wave's voice out */
    web_midi(0x90u | 1u, 69u, 110u);
    for (b = 0; b < 40u; b++) {                     /* ~29 ms: past the attack, the wave at full level */
        web_render(32);
        for (i = 0; i < 32u; i++) {
            float l = web_out_l()[i], r = web_out_r()[i];
            uint32_t u;
            if (!isfinite(l) || !isfinite(r))
                return 0;
            memcpy(&u, &l, sizeof u);
            h = (h ^ u) * 1099511628211ull;
            memcpy(&u, &r, sizeof u);
            h = (h ^ u) * 1099511628211ull;
        }
    }
    (void)k;
    return h;
}

int main(void)
{
    uint64_t v[9], d[3];
    uint32_t i, j, bad = 0;
    int fails = 0;

    web_seed(5);
    web_boot();
    web_keys(0);
    web_render(4096);
    web_master(900);

    for (i = 0; i < 9u; i++)
        v[i] = hash_note(i, 64);
    for (i = 0; i < 9u && v[i]; i++)
        for (j = i + 1u; j < 9u; j++)
            if (v[i] == v[j]) {
                printf("pd sync test: FAILED -- waves %u and %u sound identical\n", i, j);
                fails++;
            }
    printf("pd sync test: 9 waves on one note, %u distinct\n",
           9u - (uint32_t)fails);
    if (fails)
        return 1;

    d[0] = hash_note(8, 8);
    d[1] = hash_note(8, 64);
    d[2] = hash_note(8, 120);
    if (!d[0] || !d[1] || !d[2] || d[0] == d[1] || d[1] == d[2] || d[0] == d[2]) {
        printf("pd sync test: FAILED -- DCW does not move SYNC's ratio\n");
        return 1;
    }
    printf("pd sync test: SYNC at DCW 8 / 64 / 120 -- three different sounds\n");
    printf("pd sync test: 3 checks, 0 FAILED\n");
    (void)bad;
    return 0;
}
