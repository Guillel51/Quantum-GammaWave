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

/* ============================================================================================
 * (FASE 3) THE SELECTION AND THE CHORD IT MAKES.
 *
 * Until now SCL answered questions about the *scale* and about the shape a key plays; nothing on it could be
 * asked "what is this chord?". The pages now carry a selection of notes -- tapped on the keybed or on the
 * circle, or filled from a progression -- and the whole SCL section reads from it: the keys, the circle, the
 * chord's name, its inversion and the voicing lanes are five views of one set.
 *
 * The state is per track (two tracks can look at two different chords), twelve bits plus the note that was
 * pressed last, and the frame of the last touch so a key can flash. Nothing here is recomputed per pixel and
 * nothing depends on a counter: the picture of one frame is a function of (mask, bass, frame).
 * ========================================================================================== */
#define SCL_PC(n) (1u << ((n) & 11))                    /* a pitch class as a bit */
static uint16_t scl_selm[NTRK];                        /* the notes picked, by pitch class */
static uint8_t scl_selb[NTRK];                         /* the lowest one: the chord's bass, 0xFF = none */
static uint8_t scl_selk;                               /* the note touched last (its key flashes) */
static uint32_t scl_selt;                              /* and the frame it was touched in */
static void scl_sel_clear(uint32_t ti)
{
    scl_selm[ti % NTRK] = 0;
    scl_selb[ti % NTRK] = 0xFFu;
}
/* the twelve shapes a set of notes can be, by interval mask over its root: the names a musician uses, from the
 * simplest (a bare fifth) to the extensions. Read as masks so a set is compared by equality and not by a chain
 * of "if this and not that" -- a chain can answer a chord twice with two names, which is the one thing the page
 * must never do silently. */
#define IV_(a, b) ((1u << (a)) | (1u << (b)))
#define IV2(a, b, c) (IV_(a, b) | (1u << (c)))
#define IV3(a, b, c, d) (IV2(a, b, c) | (1u << (d)))
#define IV4(a, b, c, d, e) (IV3(a, b, c, d) | (1u << (e)))
#define IV5(a, b, c, d, e, f) (IV4(a, b, c, d, e) | (1u << (f)))
enum { SC_NONE, SC_MAJ, SC_MIN, SC_DIM, SC_AUG, SC_SUS2, SC_SUS4, SC_P5, SC_7, SC_MAJ7, SC_MIN7,
       SC_M7B5, SC_DIM7, SC_MM7, SC_6, SC_M6, SC_ADD9, SC_9, SC_MAJ9, SC_MIN9, SC_11, SC_13, SC_7SUS4,
       SC_KINDS };
typedef struct {
    uint16_t mask;
    uint8_t kind;
} scl_shape_t;
static const scl_shape_t SCL_SHAPES[] = {
    {IV_(0, 7), SC_P5},                     {IV2(0, 4, 7), SC_MAJ},        {IV2(0, 3, 7), SC_MIN},
    {IV2(0, 3, 6), SC_DIM},                 {IV2(0, 4, 8), SC_AUG},        {IV2(0, 2, 7), SC_SUS2},
    {IV2(0, 5, 7), SC_SUS4},                {IV3(0, 4, 7, 10), SC_7},      {IV3(0, 4, 7, 11), SC_MAJ7},
    {IV3(0, 3, 7, 10), SC_MIN7},            {IV3(0, 3, 6, 10), SC_M7B5},   {IV3(0, 3, 6, 9), SC_DIM7},
    {IV3(0, 3, 7, 11), SC_MM7},             {IV3(0, 4, 7, 9), SC_6},       {IV3(0, 3, 7, 9), SC_M6},
    {IV3(0, 2, 4, 7), SC_ADD9},             {IV4(0, 2, 4, 7, 10), SC_9},   {IV4(0, 2, 4, 7, 11), SC_MAJ9},
    {IV4(0, 2, 3, 7, 10), SC_MIN9},         {IV5(0, 2, 4, 5, 7, 10), SC_11},
    {IV5(0, 2, 4, 7, 9, 10), SC_13},        {IV3(0, 5, 7, 10), SC_7SUS4}
};
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
static const char *const N_SCKIND[SC_KINDS] = {"-", "", "min", "dim", "aug", "sus2", "sus4", "5", "7",
                                              "maj7", "m7", "m7b5", "dim7", "mM7", "6", "m6", "add9",
                                              "9", "maj9", "m9", "11", "13", "7sus4"};
typedef struct {
    uint8_t kind;                                       /* SC_* */
    int8_t root, bass;                                  /* pitch classes, -1 = unknown */
    uint8_t inv;                                        /* 0 root position, 1..3 */
    uint8_t amb;                                        /* how many roots fit: > 1 is ambiguous, reported */
    uint8_t n;                                          /* notes in the set */
} scl_rec_t;
/* rotate a shape onto a root: the intervals a chord of that root has from zero */
static uint16_t scl_rot(uint16_t m, uint32_t r)
{
    r %= 12u;                                          /* (a mask, not a bit test: 5 & 11 is 1) */
    return (uint16_t)(((m << r) | (m >> (12u - r))) & 0xFFFu);
}
/* (FASE 3) WHAT THIS SET OF NOTES IS. Equality against the shapes, one root at a time, so every answer is a
 * chord whose notes the set has exactly -- never a chord guessed from the first three notes of five. When two
 * roots fit the same shape (an augmented triad fits three, a dim7 four, a 6 and its m7 the same notes) the
 * page keeps them all: the tonic of the track names the reading if it is one of them, and if it is not, the
 * answer is marked ambiguous and the lowest note is printed as the bass. */
static void scl_recognise(uint16_t pcs, int32_t bass_pc, int32_t tonic, scl_rec_t *r)
{
    uint32_t i, k, n = 0, hits = 0;
    int32_t best = -1, bestk = -1;
    memset(r, 0, sizeof *r);
    r->root = r->bass = -1;
    pcs &= 0xFFFu;
    for (i = 0; i < 12u; i++)
        n += (pcs >> i) & 1u;
    r->n = (uint8_t)n;
    if (!n)
        return;
    r->bass = (int8_t)(bass_pc >= 0 ? (bass_pc % 12) : -1);
    if (n == 1u) {                                     /* one note is a note, not a chord: no name is invented,
                                                        * and the name it does have is its own letter */
        for (i = 0; i < 12u; i++)
            if ((pcs >> i) & 1u) {
                r->root = (int8_t)i;
                break;
            }
        return;
    }
    for (k = 0; k < sizeof SCL_SHAPES / sizeof SCL_SHAPES[0]; k++) {
        for (i = 0; i < 12u; i++) {
            if (scl_rot(SCL_SHAPES[k].mask, i) != pcs)
                continue;
            hits++;
            if (best < 0 || (int32_t)i == tonic) {     /* the tonic decides between readings */
                best = (int32_t)i;
                bestk = (int32_t)SCL_SHAPES[k].kind;
            }
        }
    }
    if (best < 0) {
        for (i = 0; i < 12u; i++)                      /* nothing fits: the lowest note, and no name */
            if ((pcs >> i) & 1u) {
                best = (int32_t)i;
                break;
            }
    }
    r->root = (int8_t)best;
    r->kind = (uint8_t)(bestk < 0 ? SC_NONE : bestk);
    r->amb = (uint8_t)(hits > 1u ? hits : 0u);
    if (r->bass >= 0 && r->root >= 0) {
        int32_t d = (r->bass - r->root + 12) % 12;
        r->inv = (uint8_t)(d == 0 ? 0 : d == 3 || d == 4 ? 1 : d == 6 || d == 7 ? 2 : 3);
    }
}
/* the chord's name as a player writes it: "Dm7", "C", "G7/B", and a "?" when the set fits more than one root
 * (the page says which ones underneath, so a symbol is never the only thing on screen) */
static void scl_chord_name(const scl_rec_t *r, char *b, uint32_t sz)
{
    if (r->n == 0u) {
        str_cpy(b, "-", sz);
        return;
    }
    if (r->n == 1u) {                                   /* a single note: its name and nothing more */
        str_cpy(b, N_NOTE[r->root < 0 ? 0 : r->root], sz);
        return;
    }
    if (r->kind == SC_NONE) {
        str_cpy(b, "?", sz);
        return;
    }
    str_cpy(b, N_NOTE[r->root], sz);
    str_cpy(b + str_len(b), N_SCKIND[r->kind], sz - str_len(b));
    if (r->amb > 1u)
        str_cpy(b + str_len(b), "?", sz - str_len(b));
    else if (r->inv && r->bass >= 0) {
        str_cpy(b + str_len(b), "/", sz - str_len(b));
        str_cpy(b + str_len(b), N_NOTE[r->bass], sz - str_len(b));
    }
}
/* the notes of the selection, low to high inside one octave, as the pages spell them */
static uint32_t scl_sel_notes(uint32_t ti, uint8_t *out, uint32_t max)
{
    uint16_t m = scl_selm[ti % NTRK];
    uint32_t i, n = 0;
    for (i = 0; i < 12u && n < max; i++)
        if ((m >> i) & 1u)
            out[n++] = (uint8_t)(60u + i);
    return n;
}
/* the notes the track is sounding right now (a key held, a sequencer step, a MIDI note): the page marks them on
 * the keys so a live event and a picked note are never confused with each other */
static uint32_t scl_live_mask(const track_t *t)
{
    uint32_t i, m = 0;
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &V(t, i);
        if (v->active && v->gate && v->note < 128u)
            m |= SCL_PC(v->note);
    }
    return m;
}

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
    {   /* (FASE 4) each key carries its own light: a three-step ramp from the lit top to the body instead of a flat
         * face, which under the page's gradient read as a hole cut in the panel. Two bands on a short strip. */
        int32_t b, n = h > 8 ? 3 : 2, bh = h / n;
        for (b = 0; b < n; b++)
            cv_rect(x + 2, y + b * bh, w - 4, b == n - 1 ? h - bh * (n - 1) : bh,
                    ux_mix(face, brd, 26 - b * 13));
    }
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
/* (6.2) the chord spelled as what its tones are: "P1 C  M3 E  P5 G  m7 Bb" -- each tone with the interval
 * it makes over the chord's root, read from N_ITVN and from the notes chord_from returned, so the page says
 * the third is a third and not only a letter. It is the same note array the key plays: nothing here is
 * recomputed from the shape. */
static void scl_spell(const uint8_t *nt, uint32_t n, char *b, uint32_t sz)
{
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < n; i++) {
        int32_t iv = ((int32_t)nt[i] - (int32_t)nt[0]) % 12;
        if (iv < 0)
            iv += 12;
        if (i && str_len(b) + 8u < sz)
            str_cpy(b + str_len(b), "  ", 2);
        if (str_len(b) + 10u > sz)
            break;
        str_cpy(b + str_len(b), N_ITVN[iv], sz - str_len(b));
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), N_NOTE[nt[i] % 12u], sz - str_len(b));
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
/* (3.6) THE KEYBOARD IS A KEYBOARD. It was twelve square cells with a note name in each, one cell drawn
 * high on an accidental and low on a natural: at 20 px it read as a row of buttons, not as an instrument.
 * It is a real keybed now, the same one SCL's SCALE page draws at 60 px: seven naturals 30 px wide with a
 * 2 px gap, five accidentals 18 px wide and 12 px tall sitting on their seam, both bevelled with scl_pkey
 * and a 1 px contour. The scale is on the key: a 2 px strip along its top edge (the tonic in the accent,
 * the other degrees in the theme) and a warmer face; a voice sounding marks the foot of its key. The mask
 * is the circle's own, so the ring above and the keys cannot disagree. Names are drawn only when the key
 * has the room for them (the chord page's 16 px strip keeps its keys unlabelled). */
/* (FASE 3) THE BED'S GEOMETRY, in one place. A piano's black key is about 0.6 of a white key's width and 0.6 of
 * its length, and sits on the seam between two naturals: the face was already close (11/18 and 3/5) but every
 * page carried its own arithmetic, so a tap could not be turned into a note without repeating it. scl_bed()
 * builds the layout and scl_bed_at() reads a point with the same numbers the drawing used -- one source of
 * truth for the picture, the hit test and the tests. */
typedef struct {
    int32_t x0, y0, w, h, kw, gap, bw, bh;
} scl_bed_t;
static void scl_bed(int32_t x0, int32_t y0, int32_t w, int32_t h, scl_bed_t *b)
{
    b->x0 = x0;
    b->y0 = y0;
    b->w = w;
    b->h = h;
    b->gap = 2;
    b->kw = (w - 6 * b->gap) / 7;
    b->bw = b->kw * 11 / 18;
    b->bh = h * 3 / 5;
    if (b->bw < 9)
        b->bw = 9;
    if (b->bh < 7)
        b->bh = 7;
}
/* the note under a point (a pitch class), or -1: the accidentals are on top, so they are asked first */
static int32_t scl_bed_at(const scl_bed_t *b, int32_t x, int32_t y)
{
    uint32_t i;
    static const uint8_t W_NOTE[7] = {0, 2, 4, 5, 7, 9, 11};
    if (x < b->x0 || x >= b->x0 + b->w || y < b->y0 || y >= b->y0 + b->h)
        return -1;
    if (y < b->y0 + b->bh) {
        for (i = 0; i < 12u; i++) {
            int32_t kx;
            if (!SCL_BLACK[i])
                continue;
            kx = b->x0 + (int32_t)SCL_WKEY[i] * (b->kw + b->gap) + b->kw + b->gap / 2 - b->bw / 2;
            if (x >= kx && x < kx + b->bw)
                return (int32_t)i;
        }
    }
    i = (uint32_t)((x - b->x0) / (b->kw + b->gap));
    if (i > 6u)
        i = 6u;
    return (int32_t)W_NOTE[i];
}
static void scl_keys(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t mask,
                     uint32_t root, uint32_t sel, int small)
{
    static const uint8_t W_KEY[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    scl_bed_t b;
    uint32_t i, live = scl_live_mask(t), flash = 0;
    int names;
    scl_bed(x0, y0, w, h, &b);
    if (b.kw < 8)
        return;
    names = !small && b.h >= 18 && b.kw >= 24;                /* the key's foot holds a 9 px name */
    if (scl_selt && ui.frame - scl_selt < 8u)                  /* (FASE 3) the key that was just touched lights */
        flash = SCL_PC(scl_selk);
    /* the seven naturals */
    for (i = 0; i < 7u; i++) {
        uint32_t note = SCL_WNOTE[i], deg = (note + 12u - root) % 12u;
        int32_t kx = b.x0 + (int32_t)i * (b.kw + b.gap);
        int in = (mask >> deg) & 1u, is_root = in && deg == 0u;
        int sound = (live >> note) & 1u, picked = (sel >> note) & 1u;
        int hot = (flash >> note) & 1u;
        uint16_t face = in ? T_KEYW : ux_mix(T_KEYW, T_BG, 40);
        uint16_t strip = is_root ? T_ACCENT : in ? T_THEME : 0;
        uint16_t brd;
        if (picked)                                        /* (FASE 3) picked: the key's face takes the accent */
            face = ux_mix(face, T_ACCENT, hot ? 62 : 40);
        else if (hot)
            face = ux_mix(face, T_TEXT, 30);
        brd = picked ? T_ACCENT : ux_mix(face, T_BG, 58);
        scl_pkey(kx, b.y0, b.kw, b.h, face, brd);
        if (strip)                                          /* the scale, on the key's top edge */
            cv_rect(kx + 3, b.y0 + 1, b.kw - 6, 2, strip);
        cv_rect(kx + b.kw - 1, b.y0 + 3, 1, b.h - 6, ux_mix(face, T_BG, 34));   /* the seam's shade */
        if (names) {   /* (FASE 7) the name sat at b.h - 11 with a 22 px key, so its last row landed on the
                        * key's own edge and read as a label under the keyboard; centred in the key it reads as
                        * part of it, equally airy above and below at every key height */
            int32_t ny = b.y0 + CAP_IN(S, b.h);   /* a pen is a line top: CAP_IN centres AF_S's caps band */
            GFX_HOOK_ALIGN(kx, ny, kx + b.kw, ny + 12, AL_H, "SCL natural name in its key");
            cv_text_in(kx, ny, b.kw, &AF_S, N_NOTE[note], T_INK, face);
        }
        if (sound)                                          /* the note is sounding: a bar at its foot */
            cv_rect(kx + 3, b.y0 + b.h - 3, b.kw - 6, 2, picked ? T_INK : T_TEXT);
    }
    /* the five accidentals, over the seam they belong to */
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - root) % 12u;
        int32_t kx;
        int in, is_root, sound, picked, hot;
        uint16_t face, strip;
        if (!SCL_BLACK[i])
            continue;
        kx = b.x0 + (int32_t)W_KEY[i] * (b.kw + b.gap) + b.kw + b.gap / 2 - b.bw / 2;
        in = (mask >> deg) & 1u;
        is_root = in && deg == 0u;
        sound = (live >> i) & 1u;
        picked = (sel >> i) & 1u;
        hot = (flash >> i) & 1u;
        face = sound ? ux_mix(T_KEYB, T_THEME, 46) : T_KEYB;
        if (picked)
            face = ux_mix(T_KEYB, T_ACCENT, hot ? 78 : 60);
        strip = is_root ? T_ACCENT : in ? T_THEME : 0;
        scl_pkey(kx, b.y0, b.bw, b.bh, face, picked ? T_ACCENT : ux_mix(T_KEYB, T_TEXT, 24));
        /* (FASE 7) THE ACCIDENTALS ARE NAMED TOO. The five black keys carried a strip and no name, so a scale
         * with F# or Bb in it could not be read off the keyboard at all. The 12 px face fits an 18 x 15 key:
         * the name takes the top of it and the scale's strip moves to the foot, where it also marks the key's
         * end, so nothing is drawn outside the key's own face. */
        if (names) {   /* the ink follows the face it sits on: dark on a lit key (a picked one, or a palette
                        * whose black keys are mid grey), light on a dark one -- read off the face itself, so no
                        * palette can make a name disappear */
            uint16_t fg = ux_luma(face) > 110u ? T_INK : picked ? T_KEYW : in ? T_TEXT : T_DIM;
            cv_text_in(kx, b.y0 + CAP_IN(S, b.bh - 3), b.bw, &AF_S, N_NOTE[i], fg, face);
        }
        if (strip)                                          /* the scale, at the accidental's foot */
            cv_rect(kx + 3, b.y0 + b.bh - 2, b.bw - 6, 2, strip);
        cv_rect(kx, b.y0 + b.bh, b.bw, 2, T_BG);            /* its shadow on the naturals */
        if (sound)
            cv_rect(kx + 1, b.y0 + b.bh - 1, b.bw - 2, 1, T_TEXT);
    }
}
/* (FASE 3) PICKING A NOTE. The keyboard the pages draw is one octave (C4..B4), so "the lowest note" a chord's
 * inversion is read from is the lowest key of the set -- which is what a player means when they call G7/B an
 * inversion of G7. Toggling keeps the bass honest: remove the bass and the lowest remaining note takes over,
 * add one below it and it is the new bass. */
static void scl_toggle(uint32_t ti, uint32_t pc)
{
    uint16_t *m = &scl_selm[ti % NTRK];
    pc %= 12u;
    if ((*m >> pc) & 1u) {
        *m = (uint16_t)(*m & ~(1u << pc));
        if (scl_selb[ti % NTRK] == (uint8_t)pc) {
            uint32_t k;
            int32_t lo = -1;
            for (k = 0; k < 12u; k++)
                if ((*m >> k) & 1u) {
                    lo = (int32_t)k;
                    break;
                }
            scl_selb[ti % NTRK] = lo < 0 ? 0xFFu : (uint8_t)lo;
        }
    } else {
        *m = (uint16_t)(*m | (1u << pc));
        if (scl_selb[ti % NTRK] == 0xFFu || (int32_t)pc < (int32_t)scl_selb[ti % NTRK])
            scl_selb[ti % NTRK] = (uint8_t)pc;
    }
    scl_selk = (uint8_t)pc;                                /* the key that was touched, for its flash */
    scl_selt = ui.frame | 1u;
    ui.force = 1;
}
/* (FASE 3) THE PAGE ANSWERS THE FINGER -- the one interaction SCL has, and only inside its own panel: the keybed
 * of SCALE and CHORD, the twelve nodes of SCALE's circle, and PROG's steps (a step loads its chord into the
 * selection, so the other two pages show what that step is). Everything is a physical control on the FM-1: the
 * pointer is the emulator's, never an invented touchscreen. Returns 1 when the tap was taken. */
int scl_tap(int32_t graph, int32_t x, int32_t y)
{
    const track_t *t = TSEL;
    uint32_t ti = trk_index(t) % NTRK, i, root = (uint32_t)((t->p[P_ROOT] % 12) + 12) % 12u;
    int32_t n;
    scl_bed_t b;
    if (ui.home || ui.menu || ui.confirm || ui.layer)
        return 0;
    if (graph == GR_SCL_SCALE || graph == GR_SCL_CHORD) {
        if (graph == GR_SCL_SCALE)
            scl_bed(8, 92, 224, 26, &b);   /* (FASE 3/8) the rectangle the page DRAWS, not a smaller one:
                                            * taps were measured against a bed 2 px down and 4 px short */
        else
            scl_bed(12, 90, 216, 26, &b);  /* (FASE 3/8) likewise: scl_keys() draws this one */
        n = scl_bed_at(&b, x, y);
        if (n < 0 && graph == GR_SCL_SCALE) {              /* the circle: the nearest node within 7 px */
            int32_t best = -1, bd = 100;   /* 10 px of slack around a 20 px ring */
            for (i = 0; i < 12u; i++) {   /* (FASE 3/8) the drawing's own centre and radius: it was
                                           * 50/58/24 against a circle drawn at 50/54/20, so the rim the
                                           * player aimed at was not the rim the page drew */
                int32_t nx = 46 + SCL_COS[i] * 20 / 256, ny = 54 + SCL_SIN[i] * 20 / 256;
                int32_t dx = x - nx, dy = y - ny, d = dx * dx + dy * dy;
                if (d < bd) {
                    bd = d;
                    best = (int32_t)((i + root) % 12u);
                }
            }
            n = best;
        }
        if (n < 0)
            return 0;
        scl_toggle(ti, (uint32_t)n);
        return 1;
    }
    if (graph == GR_SCL_PROG) {                            /* a step of the progression: its chord, picked */
        uint8_t off[12], out[KB_CH];                     /* (nt[] was never read: 12 B of stack back) */
        uint32_t cnt, cn, pi = (uint32_t)clamp((int32_t)scl_prog, 0, 7);
        int32_t rootc, w;
        if (y < 18 || y > 33)
            return 0;
        cnt = PROG_SEQ[pi][8];
        if (!cnt)
            return 0;
        w = 214 / (int32_t)cnt;
        i = (uint32_t)((x - 12) / (w ? w : 1));
        if (i >= cnt)
            return 0;
        cn = scl_chord_offsets(t, off);
        rootc = ((int32_t)t->p[P_ROOT] % 12 + 12) % 12;
        {
            uint32_t o = PROG_SEQ[pi][i] % cn, pc = ((uint32_t)rootc + off[o]) % 12u;
            uint32_t tn = chord_from(t, (uint32_t)(60u + pc), CD_TRI, CV_CLOSE, out, 0, 0), k;
            scl_sel_clear(ti);
            for (k = 0; k < tn; k++)
                scl_toggle(ti, (uint32_t)out[k]);
            return 1;
        }
    }
    return 0;
}
/* the scale's own strip: the same keys, masked by the track's scale, with the page's selection on top */
static void scl_keyboard(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, int small)
{
    uint32_t ti = trk_index(t) % NTRK;
    scl_keys(t, x0, y0, w, h, scale_mask(t), (uint32_t)((t->p[P_ROOT] % 12) + 12) % 12u, scl_selm[ti], small);
}
/* the degree a note is in the track's scale: 0-based, or -1 when the note is outside it (a chromatic note has no
 * degree to print, and the page says so instead of inventing one) */
static int32_t scl_degree(uint8_t note, uint32_t root, const uint8_t *off, uint32_t n)
{
    uint32_t i, pc = ((uint32_t)note + 12u - root) % 12u;
    for (i = 0; i < n && i < 12u; i++)
        if (off[i] == pc)
            return (int32_t)i;
    return -1;
}
/* SCL > SCALE: the twelve notes on a circle -- the tonic at the top, the scale's degrees joined by the ring in
 * the theme's colour, the notes outside it on a dim stub -- and the keyboard under it with the same notes lit.
 * The right column says the key, its family, whether it quantizes and how many notes the scale has. */
static void graph_scl_scale(const track_t *t, uint16_t c)
{
    uint32_t i, mask = scale_mask(t), n = 0, root = (uint32_t)((t->p[P_ROOT] % 12) + 12) % 12u;
    uint32_t sc = (uint32_t)clamp(t->p[P_SCALE], 0, 17), ti = trk_index(t) % NTRK;
    uint32_t sel = scl_selm[ti], live = scl_live_mask(t), sn, tn, ln = 0;
    uint8_t off[12], nt[12];
    int32_t cx = 46, cy = 54, rn = 20;                  /* (FASE 3/8) THE LABELS NO LONGER TOUCH. On one 30 px
                                                        * ring the twelve names sit 15.7 px apart while a two
                                                        * character name and its box need 16: "A#" and "B" met,
                                                        * and at the top and bottom the ring pressed on the
                                                        * header and on the keys. The names alternate between
                                                        * a 29 px ring and a 35 px one, which puts every pair
                                                        * 17.5 px apart (x 46 +- 35 = 11..81 inside the panel,
                                                        * y 18..90 clear of the header and the keybed). */
    int32_t bass = scl_selb[ti] == 0xFFu ? -1 : (int32_t)scl_selb[ti];
    scl_rec_t rec;
    char l[28], nm[16], b[20];
    (void)c;
    for (i = 0; i < 12u; i++)
        n += (mask >> i) & 1u;
    sn = scale_offsets(t, off);
    tn = scl_sel_notes(ti, nt, 12);
    scl_recognise(sel, bass, (int32_t)root, &rec);
    scl_chord_name(&rec, nm, sizeof nm);
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALELONG[sc], sizeof l - str_len(l));
    scl_head(l, t->p[P_QUANT] ? "QUANT" : "FREE", t->p[P_QUANT] ? T_ACCENT : T_DIM);

    /* --- the chromatic circle: the twelve notes, the scale joined round the ring, the picked ones ringed --- */
    for (i = 0; i < 12u; i++) {
        uint32_t j = (i + 1u) % 12u;
        uint32_t deg = (i + 12u - root) % 12u, deg2 = (j + 12u - root) % 12u;
        int both = ((mask >> deg) & 1u) && ((mask >> deg2) & 1u);
        int32_t x0 = cx + SCL_COS[i] * rn / 256, y0 = cy + SCL_SIN[i] * rn / 256;
        int32_t x1 = cx + SCL_COS[j] * rn / 256, y1 = cy + SCL_SIN[j] * rn / 256;
        cv_line_t(x0, y0, x1, y1, both ? T_THEME : T_LINE, both ? 2 : 1);
    }
    for (i = 0; i < 12u; i++) {
        uint32_t pc = (i + root) % 12u, deg = (pc + 12u - root) % 12u;
        int in = (mask >> deg) & 1u, is_root = (pc == root) && in;
        int picked = (sel >> pc) & 1u, sound = (live >> pc) & 1u;
        int32_t nx = cx + SCL_COS[i] * rn / 256, ny = cy + SCL_SIN[i] * rn / 256;
        int32_t rl = (i & 1u) ? 35 : 29;               /* the alternated ring: see the comment above */
        int32_t lx = cx + SCL_COS[i] * rl / 256, ly = cy + SCL_SIN[i] * rl / 256;
        if (in)
            cv_line(cx + SCL_COS[i] * 6 / 256, cy + SCL_SIN[i] * 6 / 256, nx, ny,
                    is_root ? T_ACCENT : ux_mix(T_THEME, T_SURF, 30));
        if (picked) {                                   /* (FASE 3) picked on the circle too: a bracket */
            cv_rect(nx - 4, ny - 4, 9, 1, T_ACCENT);
            cv_rect(nx - 4, ny + 4, 9, 1, T_ACCENT);
            cv_rect(nx - 4, ny - 3, 1, 7, T_ACCENT);
            cv_rect(nx + 4, ny - 3, 1, 7, T_ACCENT);
        }
        if (in) {
            if (is_root)
                cv_rect(nx - 3, ny - 3, 7, 7, T_ACCENT);
            else
                cv_rect(nx - 2, ny - 2, 5, 5, T_THEME);
        } else
            cv_pset(nx, ny, T_RAISE);
        if (sound)                                      /* a note sounding right now: a dot over it */
            cv_pset(nx, ny - 6, T_TEXT);
        cv_text_in(lx - 8, ly - 6, 16, &AF_S, N_NOTE[pc],
                   picked ? T_ACCENT : is_root ? T_ACCENT : in ? T_TEXT : T_DIM, T_SURF);
    }
    cv_text_in(cx - 4, cy - 6, 9, &AF_S, N_NOTE[root], T_ACCENT, T_SURF);

    /* --- the right column: the set as music, in four labelled rows --- */
    /* (FASE 3/8) WHAT THE COLUMN SAYS NOW. It printed "0 SEL 5 SCALE" and "0 LIVE" -- numbers a player had to
     * decode -- a chord name with no label, a row of degree digits with no caption, and an interval row that
     * only existed once the hand held something. Four rows with their own captions: the tones the hand holds,
     * their degrees in the key (a dash where a note is outside it, never an invented degree), their intervals
     * over the root, and the key's own semitone steps. Each row starts at the same x, so the four read as one
     * table, and the chord's name is the headline above them. Nothing here is stored: every row is a function
     * of (selection, key, scale). */
    cv_text_on(96, 0, &AF_L, tn ? nm : "-", tn ? T_ACCENT : T_DIM, T_SURF);
    if (rec.amb > 1u)
        cv_text_on(96 + text_w(&AF_L, tn ? nm : "-") + 3, 8, &AF_S, "AMB", T_MID, T_SURF);
    {
        char row[44];
        uint32_t k;
        scl_notes(nt, tn, row, sizeof row);
        scl_field(96, 24, "NOTES", T_DIM, T_SURF);
        cv_text_fit(140, 24, &AF_S, tn ? row : "TAP A KEY", tn ? T_TEXT : T_DIM, T_SURF, 92);
        row[0] = 0;
        for (k = 0; k < tn && str_len(row) + 5u < sizeof row; k++) {
            int32_t d = scl_degree(nt[k], root, off, sn);
            if (k)
                str_cpy(row + str_len(row), " ", 2);
            if (d >= 0) {
                char dg[4];
                fmt_int(dg, d + 1);
                str_cpy(row + str_len(row), dg, sizeof row - str_len(row));
            } else
                str_cpy(row + str_len(row), "-", sizeof row - str_len(row));
        }
        scl_field(96, 36, "DEG", T_DIM, T_SURF);
        cv_text_fit(140, 36, &AF_S, tn ? row : "-", tn ? T_THEME : T_DIM, T_SURF, 92);
        row[0] = 0;
        if (rec.kind != SC_NONE && rec.n > 1u) {
            for (k = 0; k < tn && str_len(row) + 5u < sizeof row; k++) {
                int32_t iv = ((int32_t)nt[k] - (int32_t)nt[0]) % 12;
                if (iv < 0)
                    iv += 12;
                if (k)
                    str_cpy(row + str_len(row), " ", 2);
                str_cpy(row + str_len(row), N_ITVN[iv], sizeof row - str_len(row));
            }
        } else
            str_cpy(row, rec.n ? "ONE NOTE" : "-", sizeof row);
        scl_field(96, 48, "IVL", T_DIM, T_SURF);
        cv_text_fit(140, 48, &AF_S, row, rec.kind != SC_NONE ? T_ACCENT : T_DIM, T_SURF, 92);
        /* (FASE 4/8) AND WHAT FITS. A twelve-tone scale has twelve numbers here and the row holds about
         * seventeen characters: CHROMATIC printed "0 1 2 3 4 5 6 7 8..." into an ellipsis on every engine's
         * SCALE page (the render lint caught it). The list stops where the row ends and the rest is counted
         * ("+6"), which says the same thing without a cut. */
        row[0] = 0;
        for (k = 0; k < sn; k++) {
            char sb[4];
            uint32_t n2;
            fmt_int(sb, (int32_t)off[k]);
            n2 = str_len(row) + (k ? 1u : 0u) + str_len(sb);
            if (n2 > 12u) {   /* and the count that follows (" +6") still has to fit the row */
                char rest[8];
                rest[0] = ' ';
                rest[1] = '+';
                fmt_int(rest + 2, (int32_t)(sn - k));
                str_cpy(row + str_len(row), rest, sizeof row - str_len(row));
                break;
            }
            if (k)
                str_cpy(row + str_len(row), " ", 2);
            str_cpy(row + str_len(row), sb, sizeof row - str_len(row));
        }
        scl_field(96, 60, "STEPS", T_DIM, T_SURF);
        cv_text_fit(140, 60, &AF_S, row, T_MID, T_SURF, 92);
        for (k = 0; k < 12u; k++)
            ln += (live >> k) & 1u;
        if (ln) {                                      /* what is sounding now, only while it sounds */
            fmt_int(row, (int32_t)ln);
            str_cpy(row + str_len(row), " LIVE", sizeof row - str_len(row));
            scl_field(96, 72, row, T_TEXT, T_SURF);
        }
    }
    scl_keyboard(t, 8, 92, 224, 26, 0);
}

/* SCL > CHORD: the chord a key plays on this track (CHRD's shape on ROOT, laid out by VOICING and strummed by
 * STRUM), the degrees of the key with the one the chord stands on lit, and the voices themselves as four lanes
 * over the chord's own compass -- the closer the dots, the tighter the voicing; the stagger between the lanes
 * is the strum the engine applies. When CHRD is OFF the page says so and keeps the lanes empty: a key plays
 * itself, nothing here is added. */
static void graph_scl_chord(const track_t *t, uint16_t c)
{
    uint8_t off[12], nt[12];
    uint32_t sn, tn, ti = trk_index(t) % NTRK, i, oob = 0;
    uint32_t mode = (uint32_t)clamp(t->p[P_CHRD], 0, CD_POW), vc = (uint32_t)clamp(t->p[P_CVOI], 0, 3);
    int32_t root = ((t->p[P_ROOT] % 12) + 12) % 12;
    int32_t bass = scl_selb[ti] == 0xFFu ? -1 : (int32_t)scl_selb[ti];
    uint32_t sel = scl_selm[ti];
    scl_rec_t rec;
    char l[28], b[24], sp[40];
    (void)c;
    sn = scale_offsets(t, off);
    tn = scl_sel_notes(ti, nt, 12);
    scl_recognise(sel, bass, root, &rec);
    str_cpy(l, N_NOTE[root], sizeof l);
    str_cpy(l + str_len(l), " ", 2);
    str_cpy(l + str_len(l), N_SCALELONG[(uint32_t)clamp(t->p[P_SCALE], 0, 17)], sizeof l - str_len(l));
    scl_head(l, tn > 1u ? "CHORD" : tn ? "NOTE" : "FREE", tn > 1u ? T_ACCENT : tn ? T_THEME : T_DIM);

    /* the name, with the inversion the bass makes and a word when the set fits more than one root */
    if (tn)
        scl_chord_name(&rec, b, sizeof b);
    else if (mode) {                                    /* nothing picked: the page shows what a KEY plays, in
                                                         * DIM and tagged, so the two answers never blur */
        uint8_t kc[KB_CH];
        uint32_t kn = chord_from(t, (uint32_t)(60 + root), mode, vc, kc, 0, 0);
        str_cpy(b, N_NOTE[root], sizeof b);
        str_cpy(b + str_len(b), scl_quality(kc, kn, 0), sizeof b - str_len(b));
    } else
        str_cpy(b, "-", sizeof b);
    cv_text_on(12, 10, &AF_L, b, tn ? T_ACCENT : T_DIM, T_SURF);
    {
        int32_t x = 12 + text_w(&AF_L, b) + 9;
        char tag[12];
        if (rec.amb > 1u)
            str_cpy(tag, "AMBIG", sizeof tag);
        else if (rec.inv)
            str_cpy(tag, "INV ", sizeof tag),
            str_cpy(tag + 4, N_NOTE[rec.bass < 0 ? 0 : rec.bass], sizeof tag - 4);
        else
            str_cpy(tag, tn > 1u ? "ROOT" : "", sizeof tag);
        if (!tn && mode)
            str_cpy(tag, "KEY", sizeof tag);
        if (tag[0])
            scl_field(x, 16, tag, tn ? (rec.amb > 1u ? T_MID : T_THEME) : T_DIM, T_SURF);
    }
    /* the tones spelled with the interval each makes over the chord's root -- the interval is what the ear
     * hears, the letter is what the key is called, and the line prints both */
    scl_spell(nt, tn, sp, sizeof sp);
    if (tn)                                             /* the hand's set: its tones and intervals */
        cv_text_fit(12, 32, &AF_S, sp, T_TEXT, T_SURF, 216);
    else if (!mode)                                     /* nothing picked and no shape: the hint owns the row */
        cv_text_fit(12, 32, &AF_S, "TAP THE KEYS BELOW OR A PROG STEP", T_DIM, T_SURF, 216);
    /* the set against the key: how many of its notes the scale has, and the function of its degree (tonic,
     * subdominant or dominant -- the three jobs a chord has in a key) */
    for (i = 0; i < tn; i++)
        oob += scl_degree(nt[i], (uint32_t)root, off, sn) < 0;
    if (tn) {   /* (FASE 3/8) only with a hand: with nothing picked this row printed "0/0 IN KEY" over the
                 * hint above it, and the lint read the pair as one text on another */
        char kb[24];
        int32_t d = rec.root >= 0 ? scl_degree((uint8_t)(60 + rec.root), (uint32_t)root, off, sn) : -1;
        const char *fn = "-";
        if (d >= 0 && sn == 7u)
            fn = (d == 0 || d == 5 || d == 2) ? "TONIC" : (d == 4 || d == 6) ? "DOMINANT" : "SUB";
        fmt_int(kb, (int32_t)(tn - oob));
        str_cpy(kb + str_len(kb), "/", sizeof kb - str_len(kb));
        {
            char t2[4];
            fmt_int(t2, (int32_t)tn);
            str_cpy(kb + str_len(kb), t2, sizeof kb - str_len(kb));
        }
        str_cpy(kb + str_len(kb), " IN KEY", sizeof kb - str_len(kb));
        scl_field(12, 44, kb, oob ? T_MID : T_THEME, T_SURF);
        scl_field(96, 44, fn, tn > 1u && d >= 0 ? T_TEXT : T_DIM, T_SURF);
    }
    /* what a key plays on this track, next to what the hand picked: the two are different answers and the page
     * never lets one pass for the other */
    if (mode) {
        uint8_t kc[KB_CH];
        uint32_t kn = chord_from(t, (uint32_t)(60 + root), mode, vc, kc, 0, 0);
        char qb[28];
        str_cpy(qb, "KEY ", sizeof qb);
        str_cpy(qb + str_len(qb), N_CHRDN[mode], sizeof qb - str_len(qb));
        str_cpy(qb + str_len(qb), " ", 2);
        str_cpy(qb + str_len(qb), scl_quality(kc, kn, 0), sizeof qb - str_len(qb));
        scl_field(tn ? 150 : 12, tn ? 44 : 32, qb, T_DIM, T_SURF);
    } else if (tn)                                     /* no shape and nothing picked: the hint already said it */
        scl_field(150, 44, "NOTE ONLY", T_DIM, T_SURF);
    /* (FASE 3/8) THE CHORD, PLACED ON ITS OWN RULER. The middle of the page was four lane lines over a pitch
     * axis, drawn only when the hand held something: with nothing picked it was an empty band under the name
     * (the render showed a metre of nothing), and with a chord the lanes still did not say WHICH notes they were
     * -- a dot's x meant a pitch on a scale nobody could read. It is an interval ruler now: the twelve semitones
     * over the chord's root, each one named (P1 m2 M2 ... M7), every tone of the chord a chip on its own
     * semitone with its letter in it -- the root in the accent, a tone outside the key in the page's raise, the
     * bass on a bright tick -- and, when the hand is empty, the same chart drawn dim for the chord a KEY plays,
     * so the page always answers something. */
    {
        int hand = tn > 0;                             /* the hand's set, or the key's shape below */
        uint8_t kc[KB_CH];
        const uint8_t *tones = nt;
        uint32_t cnt = tn, k;
        int32_t rroot = rec.root, ax0 = 14, ax1 = 226, ay = 74, hh = 12;
        if (!hand && mode) {
            cnt = chord_from(t, (uint32_t)(60 + root), mode, vc, kc, 0, 0);
            tones = kc;
            rroot = root;
        }
        if (rroot < 0 && cnt)
            rroot = (int32_t)tones[0] % 12;
        cv_rect(ax0, ay, ax1 - ax0, 1, ux_mix(T_SURF, T_LINE, 64));
        for (k = 0; k < 12u; k++) {                    /* the twelve steps of the octave, the tonic raised */
            int32_t x = ax0 + ((int32_t)k * (ax1 - ax0) + 6) / 12;
            cv_rect(x, ay - 2, 1, 5, k == 0 ? T_THEME : ux_mix(T_SURF, T_GRID, 60));
        }
        for (i = 0; i < cnt && i < 6u; i++) {
            int32_t iv = ((int32_t)tones[i] - rroot) % 12;
            int in = scl_degree(tones[i], (uint32_t)root, off, sn) >= 0;
            int bass = hand && rec.bass >= 0 && (int32_t)(tones[i] % 12u) == (int32_t)rec.bass;
            int32_t x;
            uint16_t face, ink, lk;
            if (iv < 0)
                iv += 12;
            x = ax0 + (iv * (ax1 - ax0) + 6) / 12;
            face = !hand ? ux_mix(T_SURF, T_LINE, 120) : !in ? T_RAISE : iv == 0 ? T_ACCENT
                                                                    : ux_mix(T_THEME, T_SURF, 74);
            ink = !hand ? T_MID : !in ? T_MID : iv == 0 ? T_INK : T_TEXT;
            lk = !hand ? T_DIM : !in ? T_DIM : iv == 0 ? T_ACCENT : T_THEME;
            cv_rrect(x - 7, ay - 2 - hh, 14, hh, 3, face, T_SURF);     /* the chip sits ON the axis */
            cv_rect(x, ay - 3, 1, 7, bass ? T_TEXT : T_LINE);          /* the tone's tick, the bass bright */
            cv_text_in(x - 7, ay - 2 - hh, 14, &AF_S, N_NOTE[tones[i] % 12u], ink, face);
            /* its name over the root, UNDER its own tick: the full twelve-name row was two characters in a
             * 17 px cell and read as one word; a chord only needs the intervals it actually uses */
            cv_text_in(x - 8, ay + 3, 17, &AF_S, N_ITVN[iv], lk, T_SURF);
        }
        if (!cnt)                                      /* nothing picked and no shape: the ruler alone, and */
            cv_text_fit(ax0, ay - 19, &AF_S, "TAP THE KEYS OR A PROG STEP", T_DIM, T_SURF, 200);
    }
    /* the strip keeps BOTH readings: the key's scale as the context (so an out-of-key note is still marked as
     * one) and the picked set as the accent. Handing the selection over as the scale -- which this page did
     * first -- dimmed every note of the key that the chord does not use, and the page lost the key it is in. */
    scl_keys(t, 12, 90, 216, 26, scale_mask(t), (uint32_t)root, sel, 0);
}

/* SCL > PROG: the progressions the degrees make and what each step becomes in this key -- the step, its
 * degree, the chord's root, its notes and its name, in the scale's own count (a pentatonic has five, so a
 * VII folds down onto the degree below it, and the row says so in DIM instead of pretending) */
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
