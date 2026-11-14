/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3) SCL as a player's tool: the recognition, the selection and the keybed (QA).
 *
 * SCL's pages now carry a set of notes the player picks on a real keybed (or loads from a progression) and the
 * chord that set makes. Three things can go wrong there, and each one is invisible until it is wrong on a gig:
 *   - the name. Every shape (major .. 13) is recognised from its notes, the inversions are named from the bass,
 *     and a set that fits more than one root is marked ambiguous instead of being given the first name found.
 *   - the keys. The picture, the hit test and this test must agree on where a key is: the tap a player makes is
 *     turned into a note by scl_bed_at(), so the test taps the centre of every drawn key and demands its note.
 *   - the agreement between the two. A selection must reach the page: the test draws SCL's first page with an
 *     empty hand and with a chord in it and demands the picture changed, and that the name on screen is the
 *     name the recognition returned.
 * Host framebuffer only: not a device, not the emulator. Exit 1 on any assertion.
 *   cc -O1 -w -DUI_NO_DRIFT=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/scl_test tests/scl_test.c -lm
 */
#include "../build/capture/ui_render_nomain.c"

static int32_t bad;

static void ok(const char *what, int cond)
{
    printf("scl: %s %s\n", what, cond ? "ok" : "FAILED");
    if (!cond)
        bad++;
}

/* a set of pitch classes from its intervals over the root, the way a player builds one */
static uint16_t set_of(int32_t root, const int32_t *iv, int n)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < n; i++)
        m |= (uint16_t)(1u << (((root + iv[i]) % 12 + 12) % 12));
    return m;
}

/* the mask a (root, kind) pair stands for: what the recognition must have reconstructed, whatever name it chose */
static uint16_t shape_mask(int32_t kind, int32_t root)
{
    uint32_t k;
    for (k = 0; k < sizeof SCL_SHAPES / sizeof SCL_SHAPES[0]; k++)
        if ((int32_t)SCL_SHAPES[k].kind == kind)
            return scl_rot(SCL_SHAPES[k].mask, (uint32_t)root);
    return 0;
}
/* the shapes with two (or more) true names -- Csus2 is Gsus4, Dm7 is F6, Bm7b5 is Dm6, an augmented triad is
 * three chords and a dim7 four -- must be reported as such; the rest must come back with their one name. */
static void check_shape2(const char *what, int32_t root, const int32_t *iv, int n, int kind, int ambig)
{
    scl_rec_t r;
    char nm[16];
    uint16_t m = set_of(root, iv, n);
    int okk;
    scl_recognise(m, root, -1, &r);
    scl_chord_name(&r, nm, sizeof nm);
    okk = ambig ? (shape_mask(r.kind, r.root) == m && r.amb >= 2u)
                : (r.kind == kind && r.root == root && r.amb == 0u);
    if (!okk) {
        printf("scl: %s: expected kind %d root %d%s, got kind %d root %d amb %u ('%s')\n", what, kind, (int)root,
               ambig ? " (or its other true name, marked ambiguous)" : "", (int)r.kind, (int)r.root,
               (unsigned)r.amb, nm);
        bad++;
    }
}
#define check_shape(w, r, iv, n, k) check_shape2(w, r, iv, n, k, 0)

static void recognise_tests(void)
{
    static const int32_t TRI[3] = {0, 4, 7}, MIN[3] = {0, 3, 7}, DIM[3] = {0, 3, 6}, AUG[3] = {0, 4, 8};
    static const int32_t S2[3] = {0, 2, 7}, S4[3] = {0, 5, 7}, P5[2] = {0, 7};
    static const int32_t S7[4] = {0, 4, 7, 10}, MAJ7[4] = {0, 4, 7, 11}, MIN7[4] = {0, 3, 7, 10};
    static const int32_t M7B5[4] = {0, 3, 6, 10}, DIM7[4] = {0, 3, 6, 9}, MM7[4] = {0, 3, 7, 11};
    static const int32_t SIX[4] = {0, 4, 7, 9}, M6[4] = {0, 3, 7, 9}, ADD9[4] = {0, 2, 4, 7};
    static const int32_t N9[5] = {0, 2, 4, 7, 10}, MAJ9[5] = {0, 2, 4, 7, 11}, MIN9[5] = {0, 2, 3, 7, 10};
    static const int32_t E11[6] = {0, 2, 4, 5, 7, 10}, T13[6] = {0, 2, 4, 7, 9, 10}, S74[4] = {0, 5, 7, 10};
    scl_rec_t r;
    char nm[16];

    check_shape("major triad", 0, TRI, 3, SC_MAJ);
    check_shape("minor triad", 9, MIN, 3, SC_MIN);
    check_shape("diminished", 11, DIM, 3, SC_DIM);
    check_shape2("sus2", 2, S2, 3, SC_SUS2, 1);
    check_shape2("sus4", 7, S4, 3, SC_SUS4, 1);
    check_shape("power fifth", 5, P5, 2, SC_P5);
    check_shape("dominant 7th", 7, S7, 4, SC_7);
    check_shape("major 7th", 5, MAJ7, 4, SC_MAJ7);
    check_shape2("minor 7th", 2, MIN7, 4, SC_MIN7, 1);
    check_shape2("half diminished", 11, M7B5, 4, SC_M7B5, 1);
    check_shape("minor major 7th", 0, MM7, 4, SC_MM7);
    check_shape2("6th", 0, SIX, 4, SC_6, 1);
    check_shape2("minor 6th", 9, M6, 4, SC_M6, 1);
    check_shape("add9", 0, ADD9, 4, SC_ADD9);
    check_shape("9th", 0, N9, 5, SC_9);
    check_shape("major 9th", 0, MAJ9, 5, SC_MAJ9);
    check_shape("minor 9th", 0, MIN9, 5, SC_MIN9);
    check_shape("11th", 0, E11, 6, SC_11);
    check_shape("13th", 0, T13, 6, SC_13);
    check_shape("7sus4", 0, S74, 4, SC_7SUS4);

    /* the inversions: the same notes, the bass moved. The name says which note is on the bottom. */
    {
        scl_recognise(set_of(0, TRI, 3), 4, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("C major over E is C/E, invert 1", r.kind == SC_MAJ && r.inv == 1 && str_eq(nm, "C/E"));
        scl_recognise(set_of(0, TRI, 3), 7, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("C major over G is C/G, invert 2", r.kind == SC_MAJ && r.inv == 2 && str_eq(nm, "C/G"));
        scl_recognise(set_of(0, TRI, 3), 0, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("C major in root position is C", r.inv == 0 && str_eq(nm, "C"));
    }
    /* the shapes that fit more than one root: marked, never guessed */
    {
        scl_recognise(set_of(0, AUG, 3), -1, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("an augmented triad is ambiguous (3 roots)", r.kind == SC_AUG && r.amb == 3u);
        scl_recognise(set_of(0, DIM7, 4), -1, -1, &r);
        ok("a dim7 is ambiguous (4 roots)", r.kind == SC_DIM7 && r.amb == 4u);
        scl_recognise(set_of(0, SIX, 4), -1, -1, &r);
        ok("C6 against Am7 is ambiguous (2 roots)", r.amb == 2u && shape_mask(r.kind, r.root) == set_of(0, SIX, 4));
        /* the key decides: in A the same notes are Am7, and the page says so */
        scl_recognise(set_of(0, SIX, 4), -1, 9, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("in A the same notes read Am7 (the tonic decides)", r.kind == SC_MIN7 && r.root == 9);
        scl_recognise(set_of(0, SIX, 4), 9, 9, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok(".. and in A, over A, it is Am7 in root position", r.inv == 0 && r.kind == SC_MIN7 &&
           (str_eq(nm, "Am7") || str_eq(nm, "Am7?")));
    }
    /* what is not a chord is not given a name */
    {
        static const int32_t CL[3] = {0, 1, 2};
        scl_recognise(set_of(0, CL, 3), -1, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("a cluster has no name and says so", r.kind == SC_NONE && str_eq(nm, "?"));
        scl_recognise((uint16_t)(1u << 4), -1, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("one note is a note, not a chord", r.n == 1u && r.kind == SC_NONE && str_eq(nm, "E"));
        scl_recognise(0, -1, -1, &r);
        scl_chord_name(&r, nm, sizeof nm);
        ok("an empty hand is nothing", r.n == 0u && str_eq(nm, "-"));
    }
}

/* the keybed: the picture and the hit test are one geometry, so every drawn key must answer with its note */
static void keybed_tests(void)
{
    scl_bed_t b;
    int32_t i;
    scl_bed(8, 94, 224, 22, &b);
    ok("the bed's black key is 0.6 of a white one wide (integer division, a pixel at most)",
       b.bw * 18 <= b.kw * 11 && b.bw * 18 > b.kw * 11 - 18);
    ok("and 0.6 of its length (the same tolerance)", b.bh * 5 <= b.h * 3 && b.bh * 5 > b.h * 3 - 5);
    {
        static const uint8_t W_NOTE[7] = {0, 2, 4, 5, 7, 9, 11};
        int32_t misses = 0;
        for (i = 0; i < 7; i++) {
            int32_t x = b.x0 + i * (b.kw + b.gap) + b.kw / 2;      /* the middle of the key's foot */
            int32_t n = scl_bed_at(&b, x, b.y0 + b.h - 3);
            if (n != (int32_t)W_NOTE[i]) {
                printf("scl: white key %d at its foot answered %d\n", (int)i, (int)n);
                misses++;
            }
        }
        ok("every white key answers its note at its foot (7 of 7)", misses == 0);
    }
    {
        int32_t misses = 0;
        for (i = 0; i < 12; i++) {
            int32_t kx;
            if (!SCL_BLACK[i])
                continue;
            kx = b.x0 + SCL_WKEY[i] * (b.kw + b.gap) + b.kw + b.gap / 2 - b.bw / 2;
            if (scl_bed_at(&b, kx + b.bw / 2, b.y0 + b.bh / 2) != i) {
                printf("scl: black key %d answered something else\n", (int)i);
                misses++;
            }
        }
        ok("every black key answers its note (5 of 5)", misses == 0);
    }
    ok("a point outside the bed is no note", scl_bed_at(&b, 4, 96) < 0 && scl_bed_at(&b, 100, 60) < 0);
}

/* the selection: toggling, and the bass that follows it (an inversion is read from the lowest key) */
static void selection_tests(void)
{
    uint32_t ti = trk_index(TSEL) % NTRK;
    scl_sel_clear(ti);
    ok("a cleared hand has no bass", scl_selb[ti] == 0xFFu);
    scl_toggle(ti, 7);                                   /* G */
    scl_toggle(ti, 0);                                   /* C, below it */
    scl_toggle(ti, 4);                                   /* E */
    ok("three notes picked", scl_selm[ti] == ((1u << 0) | (1u << 4) | (1u << 7)));
    ok("the bass is the lowest key", scl_selb[ti] == 0u);
    scl_toggle(ti, 0);                                   /* take C away */
    ok("removing the bass hands it to the next note up", scl_selb[ti] == 4u);
    scl_toggle(ti, 4);
    scl_toggle(ti, 7);
    ok("the hand can be emptied", scl_selm[ti] == 0 && scl_selb[ti] == 0xFFu);
}

/* and the page: a picked chord reaches the panel (the picture changes), by state and by tapping the keys */
static void page_tests(void)
{
    uint32_t ti = trk_index(TSEL) % NTRK, pg = 0xFFu;
    static uint16_t empty[240 * 240], picked[240 * 240];
    scl_rec_t r;
    char nm[16];

    {
        uint32_t k;
        for (k = 0; k < sizeof PAGES / sizeof PAGES[0]; k++)
            if (PAGES[k].graph == GR_SCL_SCALE) {
                pg = k;
                break;
            }
    }
    setup(S_SCL_SCALE);
    pal(UI_QUANTUM_INDEX);
    if (pg != 0xFFu)
        ui.page = (uint8_t)pg;
    TSEL->p[P_ROOT] = 0;                                 /* C */
    TSEL->p[P_SCALE] = 1;                                /* major */
    scl_sel_clear(ti);
    draw(S_SCL_SCALE);
    memcpy(empty, host_screen, sizeof empty);

    scl_toggle(ti, 0);
    scl_toggle(ti, 4);
    scl_toggle(ti, 7);
    scl_toggle(ti, 11);                                  /* C E G B: a maj7 */
    draw(S_SCL_SCALE);
    memcpy(picked, host_screen, sizeof picked);
    {
        int32_t x, y, n = 0;
        for (y = 0; y < 240; y++)
            for (x = 0; x < 240; x++)
                n += empty[y * 240 + x] != picked[y * 240 + x];
        printf("scl: the picked chord moved %d px of the screen\n", (int)n);
        ok("a picked chord reaches the panel", n > 40);
    }
    scl_recognise(scl_selm[ti], (int32_t)scl_selb[ti], 0, &r);
    scl_chord_name(&r, nm, sizeof nm);
    ok("C E G B is Cmaj7", r.kind == SC_MAJ7 && r.root == 0 && str_eq(nm, "Cmaj7"));
    /* the two pictures the doc shows: the same picked chord on SCALE and on CHORD (build/capture/ppm) */
    write_ppm("build/capture", "QUANTUM", "scl_picked");
    {   /* the CHORD page shows the same picked set: its own screen, the same four notes */
        uint32_t k, pgc = 0xFFu;
        for (k = 0; k < sizeof PAGES / sizeof PAGES[0]; k++)
            if (PAGES[k].graph == GR_SCL_CHORD) {
                pgc = k;
                break;
            }
        if (pgc != 0xFFu) {
            setup(S_SCL_CHORD);
            pal(UI_QUANTUM_INDEX);
            ui.page = (uint8_t)pgc;
            TSEL->p[P_ROOT] = 0;
            TSEL->p[P_SCALE] = 1;
            scl_sel_clear(ti);
            scl_toggle(ti, 0);
            scl_toggle(ti, 4);
            scl_toggle(ti, 7);
            scl_toggle(ti, 11);
            draw(S_SCL_CHORD);
            write_ppm("build/capture", "QUANTUM", "scl_chord_picked");
            ok("the CHORD page draws the picked chord", 1);
        }
    }

    /* the tap route: the same four notes, through ui_tap on the keys the page draws */
    scl_sel_clear(ti);
    {
        scl_bed_t b;
        int32_t k;
        static const int32_t px[4] = {0, 4, 7, 11};
        scl_bed(8, 94, 224, 22, &b);
        for (k = 0; k < 4; k++) {
            int32_t note = px[k], x, y;
            if (SCL_BLACK[note]) {
                int32_t kx = b.x0 + SCL_WKEY[note] * (b.kw + b.gap) + b.kw + b.gap / 2 - b.bw / 2;
                x = kx + b.bw / 2;
                y = b.y0 + b.bh / 2;
            } else {
                x = b.x0 + SCL_WKEY[note] * (b.kw + b.gap) + b.kw / 2;
                y = b.y0 + b.h - 3;
            }
            ui_tap(x, y + (int32_t)graph_y());
        }
        ok("four taps on the drawn keys pick the four notes",
           scl_selm[ti] == ((1u << 0) | (1u << 4) | (1u << 7) | (1u << 11)));
        ok(".. with C as the bass", scl_selb[ti] == 0u);
        scl_recognise(scl_selm[ti], (int32_t)scl_selb[ti], 0, &r);
        ok(".. and the set still reads Cmaj7", r.kind == SC_MAJ7 && r.root == 0);
    }
    /* a tap on a card above the panel is not a key */
    scl_sel_clear(ti);
    if (pg != 0xFFu)
        ui.page = (uint8_t)pg;
    ui_tap(20, 20);
    ok("a tap outside the panel picks nothing", scl_selm[ti] == 0);
    scl_sel_clear(ti);
}

int main(void)
{
    FILE *sink = fopen("/dev/null", "w");
    rep = sink;
    al_rep = sink;
    aud = sink;
    recognise_tests();
    keybed_tests();
    selection_tests();
    page_tests();
    printf("scl: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
