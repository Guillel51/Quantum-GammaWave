/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* CHORD on the scale (seq.c chord_build / chord_from / chord_tones), the 2.8 work that gave a scale of
 * five, six or eight notes the chord its intervals ask for instead of "every other degree" -- which in Cm
 * pentatonic made the triad on C a C F Bb. On the real sources (tests/ui_test.c's stubs):
 *   INVARIANT  every tone of every chord, 16 scales x 12 roots x 11 modes x 4 voicings, is in the scale;
 *              the chord is 1..4 tones, strictly ascending, inside 0..127.
 *   7 NOTES    the ten seven-note scales (and CHR, which borrows MAJOR) give exactly the tones the old
 *              walk gave, degree for degree: the change cannot have moved a chord anybody was playing.
 *   5 6 8      the written tables: C major's triads, Cm pentatonic's five, F7sus4 in it, and that every
 *              tone of every one of them is in the mask.
 *   VOICING    CLOSE / LEAD / OPEN / +BASS: LEAD stays within a sixth of the chord it followed, OPEN lifts
 *              the second tone an octave, +BASS puts the root below.
 *   EXCEPTIONS MONO, LEGATO, UNISON and a one-shot engine (DRUM) return the one note: not chords.
 *   SNAP       a key outside the scale takes the chord of the scale note below it.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t fails;

static void fail(const char *what, const char *sc, uint32_t root, uint32_t mode, uint32_t vc, uint32_t i, int32_t got)
{
    if (fails++ < 25u)
        fprintf(stderr, "FAIL %-22s %s root %2u mode %2u vc %u: tone %u = %d\n", what, sc, root, mode, vc, i, got);
}

/* the track and the page set for one case: ROOT, SCALE, CHRD, CVOI, polyphonic ANALOG */
static track_t *setup(uint32_t scale, uint32_t root, uint32_t mode, uint32_t vc)
{
    track_t *t = &trk[0];
    song.sel = 0;
    t->p[P_SCALE] = (int16_t)scale;
    t->p[P_ROOT] = (int16_t)root;
    t->p[P_CHRD] = (int16_t)mode;
    t->p[P_CVOI] = (int16_t)vc;
    t->p[P_VOICE] = V_POLY;
    set_engine_of(t, ENGI_ANALOG);
    return t;
}

/* the walk the seven-note scales had before 2.8 (seq.c chord_tones, c == 7), for the comparison */
static uint32_t legacy(const track_t *t, uint32_t root, uint32_t mode, int32_t *nt)
{
    static const int8_t DIA[5][4] = {{0, 2, 4, -1}, {0, 2, 4, 6}, {0, 2, 4, 8}, {0, 1, 4, -1}, {0, 3, 4, -1}};
    uint32_t mask = SCALE_MASK[clamp(t->p[P_SCALE], 0, 15)], deg[12], c = 0, at = 0, n = 0, i, guard = 12;
    int32_t rt = t->p[P_ROOT], r = (int32_t)root;
    if ((mask & 0xFFFu) == 0xFFFu || !(mask & 0xFFFu))
        mask = SCALE_MASK[1];
    while (guard-- && !((mask >> (uint32_t)((r - rt + 120) % 12)) & 1u))
        r--;
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u) {
            if (i == (uint32_t)((r - rt + 120) % 12)) at = c;
            deg[c++] = i;
        }
    for (i = 0; i < 4u && DIA[mode - CD_TRI][i] >= 0; i++) {
        uint32_t k = at + (uint32_t)DIA[mode - CD_TRI][i];
        nt[n++] = r + (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
    }
    return n;
}

static uint32_t notes_in_scale(const track_t *t, uint32_t scale, uint32_t mode, const uint8_t *out, uint32_t n)
{
    uint32_t i, bad = 0, mask = SCALE_MASK[scale];
    if (mode >= CD_MAJ)
        return 0;                                        /* (MAJ MIN 7 MAJ7 MIN7 POWER: the shape, not the key) */
    for (i = 0; i < n; i++)
        if (!((mask >> ((uint32_t)((int32_t)out[i] - t->p[P_ROOT] + 120) % 12u)) & 1u))
            bad |= 1u << i;
    return bad;
}

static int invariant(void)
{
    uint32_t sc, root, mode, vc, n, i, bad;
    int32_t ok = 0;
    for (sc = 0; sc < 16u; sc++)
        for (root = 0; root < 12u; root++)
            for (mode = CD_TRI; mode <= CD_POW; mode++)
                for (vc = 0; vc < 4u; vc++) {
                    track_t *t = setup(sc, root, mode, vc);
                    uint8_t out[KB_CH];
                    n = chord_from(t, 60u + root, mode, vc, out, 0, 0);
                    if (!n || n > KB_CH) {
                        fail("note count", N_SCALE[sc], root, mode, vc, n, (int32_t)n);
                        continue;
                    }
                    for (i = 0; i < n; i++)
                        if (i && out[i] <= out[i - 1u])
                            fail("not ascending", N_SCALE[sc], root, mode, vc, i, out[i]);
                    bad = notes_in_scale(t, sc, mode, out, n);
                    if (bad) {
                        for (i = 0; i < n; i++)
                            if ((bad >> i) & 1u)
                                fail("tone out of the scale", N_SCALE[sc], root, mode, vc, i, out[i]);
                    } else {
                        ok++;
                    }
                    if (vc == CV_CLOSE && mode == CD_TRI) {   /* CHRD OFF: the key's own note, one */
                        uint32_t n2;
                        t->p[P_CHRD] = 0;
                        n2 = chord_build(t, 60u + root, out);
                        t->p[P_CHRD] = (int16_t)mode;
                        if (n2 != 1u || out[0] != (uint8_t)(60u + root))
                            fail("CHRD OFF: one note", N_SCALE[sc], root, mode, vc, n2, out[0]);
                    }
                }
    printf("INVARIANT  %ld of %u chords: every tone in the scale, 1..4 ascending, inside the keys\n", ok, 16u * 12u * 11u * 4u);
    return fails == 0;
}

static int seven(void)
{
    static const uint8_t SEVEN[11] = {0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 5};   /* CHR MAJ MIN DOR MIX HARM PHRY LYD LOC MEL (PEN: no) */
    uint32_t k, root, mode, i, n, ln, cmp = 0;
    for (k = 0; k < 10u; k++) {                          /* (CHR and the nine modes of seven) */
        uint32_t sc = SEVEN[k];
        for (root = 0; root < 12u; root++)
            for (mode = CD_TRI; mode <= CD_ADD9; mode++) {
                track_t *t = setup(sc, root, mode, CV_CLOSE);
                int32_t a[KB_CH + 1], b[KB_CH + 1];
                n = chord_tones(t, 60u + root, mode, a);
                ln = legacy(t, 60u + root, mode, b);
                cmp++;
                if (n != ln) {
                    fail("7-note count", N_SCALE[sc], root, mode, 0, n, (int32_t)ln);
                    continue;
                }
                for (i = 0; i < n; i++)
                    if (a[i] != b[i])
                        fail("7-note changed", N_SCALE[sc], root, mode, 0, i, a[i]);
            }
    }
    printf("7 NOTES    %u chords of the seven-note scales are the ones the walk gave, tone for tone\n", cmp);
    return fails == 0;
}

/* one written chord: the tones the code returns, in order. key is the note played (60 = C4), sroot the
 * scale's ROOT under it -- the tables below are all in C, so sroot is 0 and only the key moves */
static void one_at(const char *what, uint32_t sc, uint32_t sroot, uint32_t key, uint32_t mode, uint32_t vc,
                   const int32_t *want, uint32_t wn)
{
    track_t *t = setup(sc, sroot, mode, vc);
    uint8_t out[KB_CH];
    uint32_t n = chord_from(t, key, mode, vc, out, 0, 0), i;
    if (n != wn) {
        fail(what, N_SCALE[sc], key, mode, vc, n, (int32_t)wn);
        return;
    }
    for (i = 0; i < n; i++)
        if (out[i] != (uint8_t)want[i])
            fail(what, N_SCALE[sc], key, mode, vc, i, out[i]);
}

static int tables(void)
{
    static const int32_t MAJ_TRI[7][3] = {{60, 64, 67}, {62, 65, 69}, {64, 67, 71}, {65, 69, 72},
                                          {67, 71, 74}, {69, 72, 76}, {71, 74, 77}};   /* C Dm Em F G Am Bdim */
    static const uint32_t MAJ_ROOT[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int32_t MPEN[5][3] = {{60, 63, 67}, {63, 67, 70}, {65, 70, 72}, {67, 70, 75},
                                       {70, 75, 77}};     /* Cm Eb Fsus4 Gm(b6) Bbsus4 */
    static const uint32_t MPEN_ROOT[5] = {0, 3, 5, 7, 10};
    static const int32_t F7SUS[4] = {65, 70, 72, 75};     /* F Bb C Eb */
    static const int32_t G7[4] = {67, 71, 74, 77};        /* G B D F: the seven-note walk's dominant */
    static const int32_t DM7[4] = {62, 65, 69, 72};       /* D F A C: DORIAN's seventh on its tonic */
    uint32_t i;
    for (i = 0; i < 7u; i++)
        one_at("MAJ triad", 1, 0, 60u + MAJ_ROOT[i], CD_TRI, CV_CLOSE, MAJ_TRI[i], 3);
    for (i = 0; i < 5u; i++)
        one_at("MPEN triad", 6, 0, 60u + MPEN_ROOT[i], CD_TRI, CV_CLOSE, MPEN[i], 3);
    one_at("MPEN 7TH on F", 6, 0, 65, CD_SEV, CV_CLOSE, F7SUS, 4);
    one_at("MAJ 7TH on G", 1, 0, 67, CD_SEV, CV_CLOSE, G7, 4);
    one_at("DOR 7TH on D", 3, 2, 62, CD_SEV, CV_CLOSE, DM7, 4);
    printf("TABLES     C major's seven triads, Cm pentatonic's five, F7sus4, G7 and Dm7 as written\n");
    return fails == 0;
}

static int voicings(void)
{
    track_t *t = setup(1, 0, CD_TRI, CV_CLOSE);           /* C major, the tonic's triad: C E G */
    uint8_t c[KB_CH], o[KB_CH];
    uint32_t nc = chord_from(t, 60u, CD_TRI, CV_CLOSE, c, 0, 0), n, i;
    int bad = 0;
    n = chord_from(t, 60u, CD_TRI, CV_OPEN, o, 0, 0);
    if (n != nc || o[n - 1u] != c[1] + 12u) {             /* (the second tone, up an octave, sorted again) */
        fail("OPEN lifts the 2nd", "MAJ", 0, CD_TRI, CV_OPEN, n - 1u, n ? o[n - 1u] : -1);
        bad++;
    }
    n = chord_from(t, 60u, CD_TRI, CV_BASS, o, 0, 0);
    if (!n || o[0] != c[0] - 12u) {
        fail("+BASS an octave down", "MAJ", 0, CD_TRI, CV_BASS, 0, n ? o[0] : -1);
        bad++;
    }
    for (i = 0; i < 12u; i++) {                           /* LEAD after every triad: within a sixth of its middle */
        uint8_t prev[KB_CH], led[KB_CH];
        uint32_t np = chord_from(t, 60u + i, CD_TRI, CV_CLOSE, prev, 0, 0);
        int32_t mid = 0;
        uint32_t k;
        n = chord_from(t, 60u + i + 3u, CD_TRI, CV_LEAD, led, prev, np);
        for (k = 0; k < np; k++)
            mid += prev[k];
        mid /= (int32_t)np;
        for (k = 0; k < n; k++)
            if (led[k] - mid > 6 || mid - led[k] > 6) {
                fail("LEAD within a sixth", "MAJ", i, CD_TRI, CV_LEAD, k, led[k]);
                bad++;
            }
    }
    printf("VOICING    OPEN lifts the second tone, +BASS adds the root below, LEAD stays within a sixth\n");
    return bad == 0 && fails == 0;
}

static int exceptions(void)
{
    track_t *t = setup(6, 0, CD_TRI, CV_CLOSE);
    uint8_t out[KB_CH];
    uint32_t n;
    n = chord_from(t, 60u, CD_TRI, CV_CLOSE, out, 0, 0);
    if (n < 2u) {
        fail("MPEN C is a chord", "MPEN", 0, CD_TRI, 0, n, (int32_t)n);
        return 0;
    }
    t->p[P_VOICE] = V_MONO;
    n = chord_build(t, 60u, out);
    if (n != 1u || out[0] != 60u)
        fail("MONO: one note", "MPEN", 0, CD_TRI, 0, n, out[0]);
    t->p[P_VOICE] = V_LEGATO;
    n = chord_build(t, 60u, out);
    if (n != 1u)
        fail("LEGATO: one note", "MPEN", 0, CD_TRI, 0, n, out[0]);
    t->p[P_VOICE] = V_UNISON;
    n = chord_build(t, 60u, out);
    if (n != 1u)
        fail("UNISON: one note", "MPEN", 0, CD_TRI, 0, n, out[0]);
    t->p[P_VOICE] = V_POLY;
    set_engine_of(t, ENGI_DRUM);                          /* a kit: the root alone (a key is a hit) */
    n = chord_build(t, 60u, out);
    if (n != 1u)
        fail("DRUM: no chord", "MPEN", 0, CD_TRI, 0, n, out[0]);
    printf("EXCEPTIONS MONO / LEGATO / UNISON / a kit play the one note, not a chord\n");
    return fails == 0;
}

static int snap(void)
{
    track_t *t = setup(6, 0, CD_TRI, CV_CLOSE);           /* Cm pentatonic, a key on D (outside it) */
    uint8_t out[KB_CH], c[KB_CH];
    uint32_t n = chord_from(t, 62u, CD_TRI, CV_CLOSE, out, 0, 0), nc = chord_from(t, 60u, CD_TRI, CV_CLOSE, c, 0, 0);
    uint32_t i;
    int bad = n != nc;
    for (i = 0; i < n && !bad; i++)
        bad = out[i] != c[i];
    if (bad)
        fail("D snaps down to C", "MPEN", 0, CD_TRI, 0, n, n ? out[0] : -1);
    printf("SNAP       a key outside the scale takes the chord of the scale note below it\n");
    return !bad && fails == 0;
}

int main(void)
{
    ui_power_on();
    invariant();
    seven();
    tables();
    voicings();
    exceptions();
    snap();
    if (fails) {
        printf("CHORD: %u failures\n", fails);
        return 1;
    }
    printf("CHORD: all checks pass\n");
    return 0;
}
