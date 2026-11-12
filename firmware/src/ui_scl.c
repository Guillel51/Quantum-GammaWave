/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (2.8) SCL as a player's tool: the notes of the scale and their intervals (SCALE), the chords the key has
 * (CHORD), how each of them lies on the keyboard (VOICING), the intervals up and down from the root
 * (INTERVAL), the progressions the degrees make (PROG) and the keys themselves (NOTE).
 *
 * Everything on them is computed from the same scale mask, chord_tones and chord_from the sequencer plays
 * (seq.c), so no page can keep a number the sound disagrees with -- a chord the page prints is the chord a
 * key plays, and the README's "no decorative screens" is the rule here. They are read-only: the four cards
 * above each of them are the only thing they set, and none of them adds a parameter, so a project saved
 * before them loads and sounds as it did.
 *
 * Geometry: the panel is 240 x 122 (ui.c H_GRAPH) and every screen is drawn inside it with cv_oy = 0. The
 * twelve-note pages use a grid of two columns of six cells (a chromatic has twelve notes, DIMHW and DIMWH
 * eight); the chord pages use eight rows of 12 px and a legend line under them. */
static const char *const N_ITVN[] = {"P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"};
static const char *const N_ROMAN[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
static const char *const N_ROMANM[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii"};
static const char *const N_CHRDN[] = {"OFF", "TRIAD", "7TH", "ADD9", "SUS2", "SUS4", "MAJ", "MIN", "7", "MAJ7",
                                      "MIN7", "POWER"};
/* the grid the twelve-note pages use: two columns of six cells, 17 px apart */
#define SCL_GY(k) (18 + 17 * (int32_t)(k))
/* the list the chord pages use: eight rows of 12 px, then a line under them */
#define SCL_LY(k) (14 + 12 * (int32_t)(k))
static void scl_head(const char *l, const char *r, uint16_t rc)
{
    cv_text_on(12, 3 + CAP_IN(S, 16), &AF_S, l, T_THEME, T_SURF);
    cv_text_r(228, 3 + CAP_IN(S, 16), &AF_S, r, rc, T_SURF);
}
static void scl_field(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg)
{
    cv_text_on(x, y + CAP_IN(S, 16), &AF_S, s, fg, bg);
}
/* the chord's short name from its close-position tones, and whether its numeral is lower case (minor, dim) */
static const char *scl_quality(const uint8_t *nt, uint32_t n, int *lower)
{
    int32_t t = n > 1u ? (int32_t)nt[1] - (int32_t)nt[0] : -1;
    int32_t f = n > 2u ? (int32_t)nt[2] - (int32_t)nt[0] : -1;
    int32_t s = n > 3u ? (int32_t)nt[3] - (int32_t)nt[0] : -1;
    t = t < 0 ? -1 : t % 12;
    f = f < 0 ? -1 : f % 12;
    s = s < 0 ? -1 : s % 12;
    if (lower)
        *lower = t == 3 || f == 6;
    if (t == 2 || t == 5)
        return t == 5 ? "sus4" : "sus2";
    if (t < 0 || f < 0)
        return "5";                                      /* (a bare root and fifth: POWER on a kit, or n == 1) */
    if (f == 6)
        return s == 9 ? "dim7" : s == 10 ? "m7b5" : t == 3 ? "dim" : "majb5";
    if (f == 8)
        return t == 3 ? "mb6" : "aug";                   /* (m3 + b6: the major triad a fourth below, its fifth
                                                          * missing from the scale -- what the notes say) */
    if (s == 10)
        return t == 3 ? "min7" : "7";
    if (s == 11)
        return t == 3 ? "mM7" : "maj7";
    if (s == 9)
        return "6";
    if (s == 2)
        return "add9";
    return t == 3 ? "min" : "maj";
}
/* a degree's name: a numeral in a seven-note scale, its number in any other (a pentatonic's fifth degree is
 * its fifth, not the V of seven) */
static const char *scl_degname(uint32_t d, uint32_t c, int lower, char *b)
{
    if (c == 7u && d < 8u)
        return lower ? N_ROMANM[d] : N_ROMAN[d];
    fmt_int(b, (int32_t)d + 1);
    return b;
}
/* the degrees the chord pages work in: CHR has no chords of its own -- chord_tones borrows the major scale
 * of ROOT -- so the pages show that scale rather than triads on a chromatic ladder */
static uint32_t scl_chord_offsets(const track_t *t, uint8_t *off)
{
    static const uint8_t MAJ[7] = {0, 2, 4, 5, 7, 9, 11};
    uint32_t n = scale_offsets(t, off), i;
    if (n != 12u)
        return n;
    for (i = 0; i < 7u; i++)
        off[i] = MAJ[i];
    return 7u;
}
/* the tones of a chord with their octave, for the voicing page: "G3 A#3 D4 F4" (C4 = 60) */
static void scl_notes_oct(const uint8_t *nt, uint32_t n, char *b, uint32_t sz)
{
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < n; i++) {
        char o[4];
        if (i)
            str_cpy(b + str_len(b), " ", 2);
        if (str_len(b) + 5u > sz)
            break;
        str_cpy(b + str_len(b), N_NOTE[nt[i] % 12u], sz - str_len(b));
        fmt_int(o, (int32_t)nt[i] / 12 - 1);
        str_cpy(b + str_len(b), o, sz - str_len(b));
    }
}
/* the tones of a chord as text: "C E G" */
static void scl_notes(const uint8_t *nt, uint32_t n, char *b, uint32_t sz)
{
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < n; i++) {
        if (i)
            str_cpy(b + str_len(b), " ", 2);
        if (str_len(b) + 4u > sz)
            break;
        str_cpy(b + str_len(b), N_NOTE[nt[i] % 12u], sz - str_len(b));
    }
}
/* SCL > SCALE: the scale in full -- its family, its notes, the interval each is above the root and the triad
 * that stands on it -- in a grid of two columns of six (a chromatic, or one of the eight-note diminished
 * scales, needs them all). The row of the tonic is in the accent. */
static void graph_scl_scale(const track_t *t, uint16_t c)
{
    uint8_t off[12], tri[KB_CH];
    uint32_t n = scale_offsets(t, off), i, rows = (n + 1u) / 2u, tn;
    uint32_t sc = (uint32_t)clamp(t->p[P_SCALE], 0, 15);
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[28], b[8];
    (void)c;
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALELONG[sc], sizeof l - str_len(l));
    scl_head(l, N_SCLFAM[sc], T_DIM);
    for (i = 0; i < n; i++) {
        uint32_t col = i / rows, row = i % rows, pc = ((uint32_t)root + off[i]) % 12u;
        int32_t x = 12 + (int32_t)col * 112, y = SCL_GY((int32_t)row);
        fmt_int(b, (int32_t)i + 1);
        scl_field(x, y, b, i ? T_MID : T_ACCENT, T_SURF);
        scl_field(x + 20, y, N_NOTE[pc], i ? T_TEXT : T_ACCENT, T_SURF);
        scl_field(x + 44, y, N_ITVN[off[i]], T_MID, T_SURF);
        if (sc != 0u) {                                  /* (CHR: its chords are the major scale's, so none) */
            tn = chord_from(t, 60u + pc, CD_TRI, CV_CLOSE, tri, 0, 0);
            scl_field(x + 68, y, scl_quality(tri, tn, 0), T_DIM, T_SURF);
        }
    }
}
/* SCL > CHORD: every degree's own chord -- its numeral, the note it stands on, the notes it is made of, its
 * name and its harmonic function (T tonic, S subdominant, D dominant) -- with the row the track played last
 * marked by an accent bar (chord_build remembers it, seq.c). The chords are close-position triads: what a key
 * plays is CHRD's shape, which VOICING shows. */
static void graph_scl_chord(const track_t *t, uint16_t c)
{
    /* T / S / D of a degree, by the semitones of its root above the key's: I iii vi tonic, ii IV bVI
     * subdominant, V bVII vii dominant (the triads of a major key and of a minor one both read so) */
    static const int8_t FN[12] = {0, -1, 1, 0, 0, 1, -1, 2, 1, 0, 2, 2};
    static const char *const FN_N[] = {"T", "S", "D", "-"};
    uint8_t off[12], out[KB_CH];
    uint32_t n = scl_chord_offsets(t, off), i, tn, ti = trk_index(t) % NTRK;
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[20], b[8], nts[24];
    (void)c;
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALE[(uint32_t)clamp(t->p[P_SCALE], 0, 15)], sizeof l - str_len(l));
    scl_head(l, "CHORDS IN KEY", T_DIM);
    for (i = 0; i < n; i++) {
        int32_t y = SCL_LY((int32_t)i), pc = ((int32_t)root + off[i]) % 12, lower;
        int sel = chord_nlast[ti] && (int32_t)(chord_last[ti][0] % 12u) == pc;
        const char *qn;
        tn = chord_from(t, (uint32_t)(60 + pc), CD_TRI, CV_CLOSE, out, 0, 0);
        qn = scl_quality(out, tn, &lower);
        scl_notes(out, tn, nts, sizeof nts);
        if (sel)
            cv_rect(8, y - 3, 2, 13, T_ACCENT);           /* (12 px rows: a bar, not a pill) */
        scl_field(14, y, scl_degname(i, n, lower, b), sel ? T_ACCENT : T_MID, T_SURF);
        scl_field(40, y, N_NOTE[pc], T_TEXT, T_SURF);
        scl_field(64, y, nts, T_TEXT, T_SURF);
        scl_field(132, y, qn, T_DIM, T_SURF);
        scl_field(182, y, FN_N[FN[off[i]] < 0 ? 3 : FN[off[i]]], T_MID, T_SURF);
    }
    scl_field(14, 110, "T TONIC  S SUBDOM  D DOM", T_DIM, T_SURF);
}
/* SCL > VOICING: the chord the page sets (CHRD) on the root, in its four dispositions -- close, voice led,
 * open, and with the bass an octave down -- each with a bar along the keyboard's compass: a dot a note, so
 * CLOSE reads tight, OPEN wide and +BASS under the root, which is what the four knob settings do. */
static void graph_scl_voice(const track_t *t, uint16_t c)
{
    static const char *const VN[] = {"CLOSE", "LEAD", "OPEN", "+BASS"};
    uint8_t close[KB_CH], out[KB_CH];
    uint32_t mode = (uint32_t)clamp(t->p[P_CHRD], 0, CD_POW), vc = (uint32_t)clamp(t->p[P_CVOI], 0, 3);
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    uint32_t nc, k, i;
    char l[24], nts[24];
    (void)c;
    if (!mode)
        mode = CD_TRI;                                   /* OFF: the page still says what a triad would be */
    nc = chord_from(t, (uint32_t)(60 + root), CD_TRI, CV_CLOSE, close, 0, 0);
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_CHRDN[mode], sizeof l - str_len(l));
    scl_head(l, "VOICINGS", T_DIM);
    for (k = 0; k < 4u; k++) {
        uint32_t n = chord_from(t, (uint32_t)(60 + root), mode, k, out, close, nc);
        int32_t y = 20 + 21 * (int32_t)k, x0 = 150, x1 = 226, lo = 24, hi = 108;
        int sel = k == vc;
        uint16_t bg = sel ? T_THEME : T_SURF;
        int32_t a = 0, bb = 0;
        if (sel) {
            cv_rrect(6, y - 4, 228, 19, 4, T_THEME, T_SURF);
            cv_rect(12, y - 4, 216, 1, ux_mix(T_THEME, T_TEXT, 34));
            cv_rect(12, y + 14, 216, 1, ux_mix(T_THEME, T_BG, 34));
        }
        scl_notes_oct(out, n, nts, sizeof nts);
        scl_field(12, y, VN[k], sel ? T_INK : T_MID, bg);
        scl_field(66, y, nts, sel ? T_INK : T_TEXT, bg);
        for (i = 0; i < n; i++) {                        /* the compass of the chord: one dot a note */
            int32_t p = x0 + ((int32_t)out[i] - lo) * (x1 - x0) / (hi - lo);
            p = p < x0 ? x0 : p > x1 ? x1 : p;
            if (!i || p < a) a = p;
            if (!i || p > bb) bb = p;
            cv_rect(p - 1, y + 7, 3, 3, sel ? T_INK : T_ACCENT);
        }
        cv_rect(a, y + 8, bb - a > 0 ? bb - a : 1, 1, sel ? ux_mix(T_INK, T_THEME, 40) : ux_mix(T_THEME, T_SURF, 30));
    }
    scl_field(12, 106, "KNOB 1 CHORD  KNOB 2 VOICING", T_DIM, T_SURF);
}
/* SCL > INTERVAL: the twelve intervals from the root, up (the note above it) and down (the note below), with
 * their semitones; the ones the scale has are in the accent. That is what makes a mode legible: DOR has a
 * major sixth, LOC a flat fifth, MPEN no second. */
static void graph_scl_int(const track_t *t, uint16_t c)
{
    uint32_t mask = scale_mask(t), i;
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[20], b[8];
    (void)c;
    str_cpy(l, "FROM ", sizeof l);
    str_cpy(l + 5, N_NOTE[root], sizeof l - 5);
    scl_head(l, "UP / DOWN", T_DIM);
    for (i = 0; i < 12u; i++) {
        uint32_t col = i / 6u, row = i % 6u;
        int32_t x = 12 + (int32_t)col * 112, y = SCL_GY((int32_t)row);
        int32_t dn = ((root - (int32_t)i) % 12 + 12) % 12;
        int in = (mask >> i) & 1u;
        scl_field(x, y, N_ITVN[i], in ? T_ACCENT : T_MID, T_SURF);
        b[0] = i ? '+' : ' ';
        fmt_int(b + 1, (int32_t)i);
        scl_field(x + 28, y, b, in ? T_TEXT : T_DIM, T_SURF);
        scl_field(x + 56, y, N_NOTE[((uint32_t)root + i) % 12u], in ? T_TEXT : T_DIM, T_SURF);
        scl_field(x + 88, y, N_NOTE[dn], T_DIM, T_SURF);
    }
}
/* SCL > PROG: the progressions the degrees make and what each step becomes in this key -- the step, its
 * degree, the chord's root, its notes and its name, in the scale's own count (a pentatonic has five, so a
 * VII folds down onto the degree below it, and the row says so in DIM instead of pretending) */
static const uint8_t PROG_SEQ[8][9] = {
    {0, 4, 5, 3, 0, 0, 0, 0, 4},                        /* I V VI IV */
    {0, 5, 3, 4, 0, 0, 0, 0, 4},                        /* I VI IV V */
    {1, 4, 0, 0, 0, 0, 0, 0, 3},                        /* II V I */
    {0, 3, 4, 0, 0, 0, 0, 0, 3},                        /* I IV V */
    {0, 4, 5, 2, 3, 0, 3, 4, 8},                        /* I V VI III IV I IV V */
    {0, 6, 5, 4, 0, 0, 0, 0, 4},                        /* I VII VI V */
    {0, 3, 0, 4, 3, 0, 4, 0, 8},                        /* I IV I V IV I V I */
    {2, 5, 1, 4, 0, 0, 0, 0, 5},                        /* III VI II V I */
};
static void graph_scl_prog(const track_t *t, uint16_t c)
{
    uint8_t off[12], out[KB_CH];
    uint32_t n = scl_chord_offsets(t, off), k, cnt, pi = (uint32_t)clamp((int32_t)scl_prog, 0, 7);
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[28], b[8], nts[24], pg[8];
    (void)c;
    cnt = PROG_SEQ[pi][8];
    str_cpy(l, N_PROGN[pi], sizeof l);
    str_cpy(l + str_len(l), ": ", 4);
    for (k = 0; k < cnt; k++) {                          /* the degrees as the progression is written, I V VI IV
                                                          * (the rows below give each chord's own case) */
        uint32_t o = PROG_SEQ[pi][k] % n;
        if (str_len(l) > 18u)
            break;
        str_cpy(l + str_len(l), " ", 2);
        str_cpy(l + str_len(l), scl_degname(o, n, 0, b), sizeof l - str_len(l));
    }
    fmt_int(pg, (int32_t)pi + 1);
    str_cpy(pg + str_len(pg), "/8", 4);
    scl_head(l, pg, T_DIM);
    for (k = 0; k < cnt; k++) {
        uint32_t d = PROG_SEQ[pi][k], o = d % n;
        int32_t y = SCL_LY((int32_t)k), pc = ((int32_t)root + off[o]) % 12;
        uint32_t tn = chord_from(t, (uint32_t)(60 + pc), CD_TRI, CV_CLOSE, out, 0, 0);
        int wrap = d >= n, lower;                         /* (a degree this scale does not have) */
        scl_quality(out, tn, &lower);
        scl_notes(out, tn, nts, sizeof nts);
        fmt_int(b, (int32_t)k + 1);
        scl_field(14, y, b, T_MID, T_SURF);
        scl_field(36, y, scl_degname(o, n, lower, b), wrap ? T_DIM : T_ACCENT, T_SURF);
        scl_field(66, y, N_NOTE[pc], T_TEXT, T_SURF);
        scl_field(90, y, nts, T_TEXT, T_SURF);
        scl_field(182, y, scl_quality(out, tn, 0), wrap ? T_DIM : T_MID, T_SURF);
    }
    scl_field(14, 110, cnt < 8u ? "KNOB 1 NEXT PROGRESSION" : "8 STEPS", T_DIM, T_SURF);
}
/* SCL > NOTE: the twelve keys -- their name, the degree they are in the scale (or none), their MIDI number at
 * C4 = 60 and the other name of the same key (Db for C#) -- so a note can be found instead of guessed */
static void graph_scl_note(const track_t *t, uint16_t c)
{
    static const char *const N_ENH[] = {"", "Db", "", "Eb", "", "", "Gb", "", "Ab", "", "Bb", ""};
    uint32_t mask = scale_mask(t), i;
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[24], b[8];
    (void)c;
    str_cpy(l, "KEYS OF ", sizeof l);
    str_cpy(l + 8, N_NOTE[root], sizeof l - 8);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALE[(uint32_t)clamp(t->p[P_SCALE], 0, 15)], sizeof l - str_len(l));
    scl_head(l, "MIDI 60 = C4", T_DIM);
    for (i = 0; i < 12u; i++) {
        uint32_t col = i / 6u, row = i % 6u;
        int32_t x = 12 + (int32_t)col * 112, y = SCL_GY((int32_t)row), d = scale_degree_of(t, 60 + (int32_t)i);
        int in = (mask >> ((i + 12u - (uint32_t)root) % 12u)) & 1u;
        scl_field(x, y, N_NOTE[i], in ? T_TEXT : T_MID, T_SURF);
        if (d < 0)
            str_cpy(b, "-", 4);
        else
            fmt_int(b, d + 1);
        scl_field(x + 26, y, b, in ? T_ACCENT : T_DIM, T_SURF);
        fmt_int(b, 60 + (int32_t)i);
        scl_field(x + 48, y, b, in ? T_TEXT : T_DIM, T_SURF);
        if (N_ENH[i][0])
            scl_field(x + 82, y, N_ENH[i], T_DIM, T_SURF);
    }
}
