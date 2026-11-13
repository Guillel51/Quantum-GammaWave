/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.1) SCL as a player's tool, in three pages: SCALE (the circle of the twelve notes and the keyboard under it),
 * CHORD (the chord a key plays, how its voices lie and how far they strum, over the degrees of the key) and PROG
 * (the progressions those degrees make).
 *
 * Everything on them is computed from the same scale mask, chord_tones and chord_from the sequencer plays
 * (seq.c), so no page can keep a number the sound disagrees with -- a chord the page prints is the chord a
 * key plays, and the README's "no decorative screens" is the rule here. The cards above each page are what
 * they set: ROOT SCALE QUANT on the first, CHORD VOICING STRUM on the second (and KNOB 1 of PROG, which is
 * the progression), so a project saved before them loads and sounds as it did.
 *
 * Geometry: the panel is 240 x 122 (ui.c H_GRAPH) and every page is drawn inside it with cv_oy = 0. The circle
 * is centred at (52, 63) with its twelve labels on a ring of 47 px: the twelve note names never collide with
 * the ring, and the right column keeps 130 px for the key, its scale and the degrees. */

static const char *const N_ITVN[] = {"P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"};
static const char *const N_ROMAN[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
static const char *const N_ROMANM[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii"};
static const char *const N_CHRDN[] = {"OFF", "TRIAD", "7TH", "ADD9", "SUS2", "SUS4", "MAJ", "MIN", "7", "MAJ7",
                                      "MIN7", "POWER"};
/* the twelve positions of the circle, 30 degrees apart from the tonic at the top (Q8: 256 = 1) */
static const int16_t SCL_COS[12] = {0, 128, 222, 256, 222, 128, 0, -128, -222, -256, -222, -128};
static const int16_t SCL_SIN[12] = {-256, -222, -128, 0, 128, 222, 256, 222, 128, 0, -128, -222};
/* the keyboard the pages draw: C C# D D# E F F# G G# A A# B */
static const uint8_t SCL_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
static const uint8_t SCL_WKEY[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
static const uint8_t SCL_WNOTE[7] = {0, 2, 4, 5, 7, 9, 11};

static void scl_head(const char *l, const char *r, uint16_t rc)
{
    cv_text_on(12, 3 + CAP_IN(S, 16), &AF_S, l, T_THEME, T_SURF);
    cv_text_r(228, 3 + CAP_IN(S, 16), &AF_S, r, rc, T_SURF);
}
static void scl_field(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg)
{
    cv_text_on(x, y + CAP_IN(S, 16), &AF_S, s, fg, bg);
}
/* a key of the keyboard strip: rounded, a lit top and a contour, drawn from rows (no registered cell: the
 * accidentals sit over the naturals) */
static void scl_pkey(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t face, uint16_t brd)
{
    cv_rect(x + 2, y, w - 4, h, face);
    cv_rect(x, y + 2, 2, h - 4, face);
    cv_rect(x + w - 2, y + 2, 2, h - 4, face);
    cv_pset(x + 1, y + 1, brd);
    cv_pset(x + w - 2, y + 1, brd);
    cv_pset(x + 1, y + h - 2, brd);
    cv_pset(x + w - 2, y + h - 2, brd);
    cv_rect(x + 2, y, w - 4, 1, ux_mix(brd, face, 60));
    cv_rect(x + 1, y + h - 1, w - 2, 1, brd);
    cv_rect(x, y + 2, 1, h - 4, brd);
    cv_rect(x + w - 1, y + 2, 1, h - 4, brd);
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
/* the tones of a chord with their octave, for the chord page: "G3 A#3 D4 F4" (C4 = 60) */
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
/* the keyboard strip: twelve keys from x0 to x1, in the scale coloured (the tonic in the accent), and the
 * notes a voice is sounding lit. It reads the same mask as the circle above it, so the two cannot disagree. */
static void scl_keys(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t mask,
                     uint32_t root, int small)
{
    uint32_t i, live = 0;
    int32_t gap = 2, kw = (w - 11 * gap) / 12;
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &V(t, i);
        if (v->active && v->gate && v->note < 128u)
            live |= 1u << (v->note % 12u);
    }
    /* twelve cells, one a key: the cell sits high on a white key and low on a black one, as the keys do, and the
     * ones in the scale are filled with the theme (the tonic with the accent). A voice sounding marks its cell. */
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - root) % 12u;
        int32_t kx = x0 + (int32_t)i * (kw + gap);
        int in = (mask >> deg) & 1u, is_root = in && deg == 0u, black = SCL_BLACK[i];
        int sound = (live >> i) & 1u;
        int32_t ky = y0 + (black ? h / 3 : 0), kh = h - (black ? h / 3 : 0);
        uint16_t face = is_root ? T_ACCENT : in ? T_THEME : black ? T_RAISE : T_SURF;
        uint16_t ink = is_root ? T_INK : in ? T_INK : black ? T_DIM : T_MID;
        scl_pkey(kx, ky, kw, kh, face, ux_mix(face, T_BG, 55));
        if (black)
            cv_rect(kx + 1, y0 + 1, kw - 2, h / 3 - 1, T_BG);
        if (!small)
            cv_text_in(kx, ky + (kh - 9) / 2, kw, &AF_S, N_NOTE[i], ink, face);
        if (sound)
            cv_rect(kx + 2, ky + kh - 3, kw - 4, 2, is_root || in ? T_TEXT : T_ACCENT);
    }
}
/* the scale's own strip: the same keys, masked by the track's scale */
static void scl_keyboard(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, int small)
{
    scl_keys(t, x0, y0, w, h, scale_mask(t), (uint32_t)t->p[P_ROOT] % 12u, small);
}
/* SCL > SCALE: the twelve notes on a circle -- the tonic at the top, the scale's degrees joined by the ring in
 * the theme's colour, the notes outside it on a dim stub -- and the keyboard under it with the same notes lit.
 * The right column says the key, its family, whether it quantizes and how many notes the scale has. */
static void graph_scl_scale(const track_t *t, uint16_t c)
{
    uint32_t i, mask = scale_mask(t), n = 0, root = (uint32_t)t->p[P_ROOT] % 12u;
    uint32_t sc = (uint32_t)clamp(t->p[P_SCALE], 0, 15);
    int32_t cx = 54, cy = 60, r = 22;
    char l[24], b[8];
    (void)c;
    for (i = 0; i < 12u; i++)
        n += (mask >> i) & 1u;
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALELONG[sc], sizeof l - str_len(l));
    scl_head(l, t->p[P_QUANT] ? "QUANT" : "FREE", t->p[P_QUANT] ? T_ACCENT : T_DIM);

    /* the ring: the twelve neighbours joined, the degrees of the scale brighter than the rest */
    for (i = 0; i < 12u; i++) {
        uint32_t j = (i + 1u) % 12u;
        uint32_t deg = (i + 12u - root) % 12u, deg2 = (j + 12u - root) % 12u;
        int both = ((mask >> deg) & 1u) && ((mask >> deg2) & 1u);
        int32_t x0 = cx + SCL_COS[i] * r / 256, y0 = cy + SCL_SIN[i] * r / 256;
        int32_t x1 = cx + SCL_COS[j] * r / 256, y1 = cy + SCL_SIN[j] * r / 256;
        cv_line_t(x0, y0, x1, y1, both ? T_THEME : T_LINE, both ? 2 : 1);
    }
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - root) % 12u;
        int in = (mask >> deg) & 1u, is_root = in && deg == 0u;
        int32_t nx = cx + SCL_COS[i] * r / 256, ny = cy + SCL_SIN[i] * r / 256;
        int32_t lx = cx + SCL_COS[i] * 34 / 256, ly = cy + SCL_SIN[i] * 34 / 256;
        if (in) {                                        /* a spoke from the centre: the tonic to each degree */
            if (!is_root)
                cv_line(cx, cy, nx, ny, ux_mix(T_THEME, T_SURF, 26));
            cv_rect(nx - 2, ny - 2, 5, 5, is_root ? T_ACCENT : T_THEME);
            if (is_root)
                cv_rect(nx - 3, ny - 3, 7, 7, T_ACCENT);
        } else
            cv_pset(nx, ny, T_RAISE);
        cv_text_in(lx - 8, ly - 6, 16, &AF_S, N_NOTE[(i + root) % 12u],
                   is_root ? T_ACCENT : in ? T_TEXT : T_DIM, T_SURF);
    }
    cv_text_in(cx - 4, cy - 6, 9, &AF_S, N_NOTE[root], T_ACCENT, T_SURF);

    /* the right column: the key, its family and what the page's three cards set */
    {
        int32_t x = 104;
        cv_text_on(x, 8, &AF_L, N_NOTE[root], T_ACCENT, T_SURF);
        cv_text_on(x + text_w(&AF_L, N_NOTE[root]) + 4, 14, &AF_S, N_SCALE[sc], T_TEXT, T_SURF);
    }
    scl_field(104, 30, N_SCLFAM[sc], T_MID, T_SURF);
    {
        char nb[16];
        fmt_int(nb, (int32_t)n);
        str_cpy(nb + str_len(nb), " NOTES", sizeof nb - str_len(nb));
        scl_field(104, 42, nb, T_DIM, T_SURF);
    }
    scl_field(104, 54, t->p[P_QUANT] ? "SNAPS TO KEY" : "AS PLAYED", t->p[P_QUANT] ? T_THEME : T_DIM, T_SURF);
    if (t->p[P_CHRD]) {
        uint8_t out[KB_CH];
        uint32_t tn = chord_build(t, 60u, out);
        char q[20];
        str_cpy(q, N_CHRDN[(uint32_t)clamp(t->p[P_CHRD], 0, CD_POW)], sizeof q);
        str_cpy(q + str_len(q), " ", 2);
        str_cpy(q + str_len(q), scl_quality(out, tn, 0), sizeof q - str_len(q));
        scl_field(104, 66, "CHRD ON", T_ACCENT, T_SURF);
        cv_text_fit(152, 66, &AF_S, q, T_TEXT, T_SURF, 78);
    } else
        scl_field(104, 66, "CHRD OFF", T_DIM, T_SURF);
    scl_keyboard(t, 8, 98, 224, 20, 0);
}
/* SCL > CHORD: the chord a key plays on this track (CHRD's shape on ROOT, laid out by VOICING and strummed by
 * STRUM), the degrees of the key with the one the chord stands on lit, and the voices themselves as four lanes
 * over the chord's own compass -- the closer the dots, the tighter the voicing; the stagger between the lanes
 * is the strum the engine applies. When CHRD is OFF the page says so and keeps the lanes empty: a key plays
 * itself, nothing here is added. */
static void graph_scl_chord(const track_t *t, uint16_t c)
{
    static const int8_t FN[12] = {0, -1, 1, 0, 0, 1, -1, 2, 1, 0, 2, 2};
    static const char *const FN_N[] = {"T", "S", "D", "-"};
    uint8_t off[12], out[KB_CH];
    uint32_t n = scl_chord_offsets(t, off), i, tn, ti = trk_index(t) % NTRK;
    uint32_t mode = (uint32_t)clamp(t->p[P_CHRD], 0, CD_POW), vc = (uint32_t)clamp(t->p[P_CVOI], 0, 3);
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    char l[24], b[8], nts[24], vo[16];
    (void)c;
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_CHRDN[mode], sizeof l - str_len(l));
    scl_head(l, mode ? "CHORD" : "OFF", mode ? T_THEME : T_DIM);

    /* the chord its shape makes on the tonic: name, notes, and the degree it stands on */
    tn = chord_from(t, (uint32_t)(60 + root), mode ? mode : CD_TRI, mode ? vc : CV_CLOSE, out, 0, 0);
    {
        const char *q = scl_quality(out, tn, 0);
        int32_t w = text_w(&AF_L, N_NOTE[root]);
        cv_text_on(12, 14, &AF_L, N_NOTE[root], mode ? T_ACCENT : T_DIM, T_SURF);
        scl_field(12 + w + 3, 22, q, mode ? T_TEXT : T_DIM, T_SURF);
        scl_notes(out, tn, nts, sizeof nts);
        cv_text_fit(12, 40, &AF_S, nts, mode ? T_TEXT : T_DIM, T_SURF, 120);
        str_cpy(vo, "VOICE ", sizeof vo);
        str_cpy(vo + 6, vc == 0 ? "CLOSE" : vc == 1 ? "LEAD" : vc == 2 ? "OPEN" : "BASS", sizeof vo - 6);
        scl_field(12, 53, vo, T_MID, T_SURF);
    }
    {   /* the strum: how far the voices are apart, in ms (chord_gap: 127 x 0.5 ms) */
        char sb[20];
        int32_t ms = t->p[P_CSTR] * 1000 / 2000;
        fmt_int(sb, ms);
        str_cpy(sb + str_len(sb), " ms", sizeof sb - str_len(sb));
        scl_field(120, 53, t->p[P_CSTR] ? sb : "STRUM OFF", t->p[P_CSTR] ? T_THEME : T_DIM, T_SURF);
    }
    /* the degrees of the key: a pill each, the one the chord stands on filled with the accent (and the one the
     * track played last marked with a bar under it, as the sequencer remembers it) */
    for (i = 0; i < n; i++) {
        int32_t cw = 210 / (int32_t)(n ? n : 1u), x = 12 + (int32_t)i * cw, y = 68;
        int32_t pc = ((int32_t)root + off[i]) % 12, lower;
        int sel = mode && pc == root, last = chord_nlast[ti] && (int32_t)(chord_last[ti][0] % 12u) == pc;
        uint32_t dtn = chord_from(t, (uint32_t)(60 + pc), CD_TRI, CV_CLOSE, out, 0, 0);
        uint16_t bg = sel ? T_ACCENT : T_RAISE;
        scl_quality(out, dtn, &lower);
        cv_rrect(x, y, cw - 3, 15, 3, bg, T_SURF);
        cv_text_in(x, y + 3, cw - 3, &AF_S, scl_degname(i, n, lower, b), sel ? T_INK : T_TEXT, bg);
        if (last)
            cv_rect(x, y + 16, cw - 6, 2, T_TEXT);
    }
    /* (FASE 3) the chord on the keys, then the voices over it. The strip is the one SCALE draws, masked with the
     * chord's own tones instead of the scale's: the chord's root in the accent, the other tones in the theme, the
     * rest of the key's scale lit low and everything else dark, so a chord is read as notes on an instrument and
     * not as a list of names. Under it the four lanes keep what the keys cannot say: the voicing's spread (one
     * pitch axis, so CLOSE reads tight and OPEN wide) and the strum the engine applies between the voices. */
    {
        uint32_t cmask = 0;
        for (i = 0; i < tn; i++)
            cmask |= 1u << (((out[i] % 12u) + 12u - (uint32_t)root) % 12u);
        scl_keys(t, 12, 86, 216, 16, mode ? cmask : 0u, (uint32_t)root, 1);
        if (mode && tn) {
            int32_t x0 = 16, x1 = 222, lo = 127, hi = 0, k;
            int32_t strum = (int32_t)t->p[P_CSTR] * 18 / 127;
            for (i = 0; i < tn; i++) {
                if ((int32_t)out[i] < lo) lo = out[i];
                if ((int32_t)out[i] > hi) hi = out[i];
            }
            hi = hi - lo < 7 ? lo + 7 : hi;
            for (k = 0; k < (int32_t)tn && k < 4; k++) {
                int32_t y = 104 + k * 4;
                int32_t px = x0 + ((int32_t)out[k] - lo) * (x1 - x0) / (hi - lo);
                px += k * strum;
                px = px < x0 ? x0 : px > x1 ? x1 : px;
                cv_rect(x0 + k * strum, y + 2, x1 - x0 - 3 * strum, 1, ux_mix(T_THEME, T_SURF, 24));
                cv_rect(px - 1, y, 3, 3, T_ACCENT);
            }
        }
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
    scl_head(l, "", T_DIM);
    /* the progression as pills, left to right: the steps in the order they are played */
    for (k = 0; k < cnt; k++) {
        uint32_t o = PROG_SEQ[pi][k] % n, pc = ((uint32_t)root + off[o]) % 12u;
        int32_t w = 214 / (int32_t)cnt, x = 12 + (int32_t)k * w;
        int32_t y = 18;
        cv_rrect(x, y, w - 4, 14, 3, T_RAISE, T_SURF);
        scl_field(x + 4, y + 1, scl_degname(o, n, 0, b), T_ACCENT, T_RAISE);
        scl_field(x + 4 + 12, y + 1, N_NOTE[pc], T_TEXT, T_RAISE);
        cv_rect(x + 2, y + 13, w - 8, 2, T_THEME);
    }
    for (k = 0; k < cnt; k++) {
        uint32_t d = PROG_SEQ[pi][k], o = d % n;
        int32_t y = 36 + 12 * (int32_t)k, pc = ((int32_t)root + off[o]) % 12;
        uint32_t tn = chord_from(t, (uint32_t)(60 + pc), CD_TRI, CV_CLOSE, out, 0, 0);
        int wrap = d >= n, lower;                         /* (a degree this scale does not have) */
        scl_quality(out, tn, &lower);
        scl_notes(out, tn, nts, sizeof nts);
        fmt_int(b, (int32_t)k + 1);
        scl_field(14, y, b, T_DIM, T_SURF);
        scl_field(30, y, scl_degname(o, n, lower, b), wrap ? T_DIM : T_ACCENT, T_SURF);
        scl_field(58, y, N_NOTE[pc], T_TEXT, T_SURF);
        scl_field(80, y, nts, wrap ? T_DIM : T_TEXT, T_SURF);
        scl_field(170, y, scl_quality(out, tn, 0), wrap ? T_DIM : T_MID, T_SURF);
    }
    fmt_int(pg, (int32_t)pi + 1);
    str_cpy(pg + str_len(pg), "/8", 4);
    cv_text_r(228, 3 + CAP_IN(S, 16), &AF_S, pg, T_MID, T_SURF);
}
