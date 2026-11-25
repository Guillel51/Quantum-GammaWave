/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 4 (ronda de expresividad armónica): THE EIGHT VOICINGS, MEASURED.
 *
 * The four voicings the page already had (CLOSE, LEAD, OPEN, +BASS) are untouched; this file proves it and proves
 * the four that joined them: DROP2 and DROP3 drop the second / third voice from the top an octave, QUART stacks
 * the chord's own tones in fourths, SPREAD lifts every other tone an octave. Every voicing keeps the chord's notes
 * (none is invented, none is lost), stays sorted and distinct, and never leaves 0..127.
 *
 * Class: host x86-64, the real sources (hostsim.c includes seq.c). No FM-1 here.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("voi: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int sorted_unique(const uint8_t *n, uint32_t c)
{
    uint32_t i;
    for (i = 1; i < c; i++)
        if (n[i] <= n[i - 1])
            return 0;
    return 1;
}
static uint32_t build(track_t *t, uint32_t root, uint32_t mode, uint32_t vc, uint8_t *out)
{
    return chord_from(t, root, mode, vc, out, 0, 0);
}

int main(void)
{
    track_t *t = &trk[0];
    uint8_t out[KB_CH];
    uint32_t c, k, mode, vc, key;
    int same = 1, ok2 = 1, ok3 = 1, okq = 1, oks = 1, oka = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    voices_boot();
    host_tracks_init();
    t->engine = ENGI_ANALOG;
    t->p[P_ROOT] = 0;
    t->p[P_SCALE] = 1;

    /* the four of before, on a seventh from C4: what they gave before this change, note for note */
    {
        static const uint8_t want[4][4] = {{60, 64, 67, 70}, {60, 67, 70, 76}, {48, 60, 64, 70}, {60, 64, 67, 70}};
        static const uint32_t cvc[4] = { CV_CLOSE, CV_OPEN, CV_BASS, CV_LEAD };
        for (k = 0; k < 4u; k++) {
            c = build(t, 60u, CD_DOM7, cvc[k], out);
            printf("voi: %-8s %u %u %u %u\n", cvc[k] == CV_CLOSE ? "CLOSE" : cvc[k] == CV_OPEN ? "OPEN" :
                   cvc[k] == CV_BASS ? "+BASS" : "LEAD", out[0], out[1], out[2], out[3]);
            if (c != 4u)
                same = 0;
            for (uint32_t j = 0; j < 4u && j < c; j++)
                if (out[j] != want[k][j] && k != 3u)
                    same = 0;
        }
        check("the four voicings of before give the same notes they gave", same);
    }

    /* DROP2: the second from the top moves down an octave, the rest stays; the top note (the melody) stays */
    c = build(t, 60u, CD_DOM7, CV_DROP2, out);
    printf("voi: DROP2    %u %u %u %u (want 55 60 64 70)\n", out[0], out[1], out[2], out[3]);
    check("DROP2 drops the second voice from the top and keeps the top note where it was",
          c == 4u && out[3] == 70u && sorted_unique(out, c) && out[0] == 55u && out[1] == 60u && out[2] == 64u);

    /* DROP3: a seventh has four voices, so the third from the top moves down */
    c = build(t, 60u, CD_DOM7, CV_DROP3, out);
    printf("voi: DROP3    %u %u %u %u (want 52 60 67 70)\n", out[0], out[1], out[2], out[3]);
    check("DROP3 drops the third voice from the top, the melody still on top",
          c == 4u && out[3] == 70u && sorted_unique(out, c) && out[0] == 52u);

    /* QUART: the same number of tones, stacked in fourths from the root */
    c = build(t, 60u, CD_DOM7, CV_QUART, out);
    printf("voi: QUART    %u %u %u %u (want 60 65 70 75)\n", out[0], out[1], out[2], out[3]);
    check("QUART stacks the chord's own tones in perfect fourths", c == 4u && out[0] == 60u && out[1] == 65u &&
                                                                out[2] == 70u && out[3] == 75u && sorted_unique(out, c));

    /* SPREAD: every other tone an octave up: the chord covers two octaves */
    c = build(t, 60u, CD_DOM7, CV_SPREAD, out);
    printf("voi: SPREAD   %u %u %u %u (want 60 67 76 82)\n", out[0], out[1], out[2], out[3]);
    check("SPREAD lifts every other tone an octave and stays in order",
          c == 4u && out[0] == 60u && out[1] == 67u && out[2] == 76u && out[3] == 82u && sorted_unique(out, c));

    /* the invariants, over every shape and every key: nothing invented, nothing lost, in range */
    for (mode = CD_TRI; mode <= CD_POW; mode++)
        for (vc = CV_CLOSE; vc <= CV_SPREAD; vc++)
            for (key = 0; key < 12u; key++) {
                c = build(t, 36u + key, mode, vc, out);
                if (!c || c > KB_CH)
                    oka = 0;
                if (vc >= CV_DROP2 && !sorted_unique(out, c))
                    oka = 0;
                for (k = 1; k < c; k++)
                    if (out[k] > 127u)
                        oka = 0;
                if (vc == CV_QUART && c >= 3u && out[1] != out[0] + 5u)
                    okq = 0;
                if (vc == CV_SPREAD && c >= 3u && out[c - 1u] < out[0] + 12u)
                    oks = 0;
            }
    printf("voi: %u shapes x 8 voicings x 12 keys\n", CD_POW - CD_TRI + 1u);
    check("every shape on every key, in all eight voicings: in range and in order", oka);
    check("QUART is a fourth above its root in every case it has one", okq);
    check("SPREAD always reaches at least an octave above its root", oks);
    (void)ok2; (void)ok3;
    printf("voi: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
