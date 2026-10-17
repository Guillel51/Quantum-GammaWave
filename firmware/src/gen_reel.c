/* SPDX-License-Identifier: GPL-3.0-only
 * (1.0-GW) TAKES: the generator's reel. Every piece made (OCT+, a take, DICE ALL) goes on it as its recipe — the
 * settings that remake it exactly (the generator is deterministic) — with what its card shows: the genre and the trap
 * flavour, the seed, the tempo, the key, each live track's first two bars as a thumbnail and the song's sections.
 * PRESETS on the GEN screen opens the reel and scrolls it; while the music plays the take under the cursor becomes the
 * music once PRESETS rests (gen_take_poll), stopped OCT+ loads it; KNOB 4 keeps a take (kept ones never drop off: the
 * reel holds the last 32). A recipe is not a recording: edits made after are not on it, SAVE keeps those. Included by
 * gen.c (after dice_print). */

static uint16_t heat_col(int32_t v);                    /* (ui_graph.c) */
#define REEL_N 32u
typedef struct {                                        /* the recipe: what gen_make reads (and the tracks it ran on) */
    uint16_t seed;
    uint8_t genre, energy, form, complex, swing, human, tempo, key, scale, length, space, fills, wet, grit, nlive;
    uint8_t amt[GP_N];
} gset_t;
typedef struct {
    gset_t set;
    char name[14];                                      /* the genre as played (a RANDOM throw's, a hybrid's) */
    uint8_t flav, root, scl, nl, keep, nrow;            /* flav: the trap flavour + 1 (0: none) */
    int16_t bpm;
    uint32_t thumb[6];                                  /* a live track's first 32 steps: bit i, a note or hit */
    uint8_t role[6];
    uint8_t row[10][2];                                 /* the song's rows: section, bars */
} take_t;
static take_t reel[REEL_N] __attribute__((section(".pool")));   /* (zeroed at boot) */
static uint8_t reel_n, reel_cur, reel_play = 0xFF;      /* takes, the cursor, the take playing now (0xFF: none) */
static int8_t reel_want = -1;                           /* a take to load once PRESETS rests (playing) */
static const char *const REEL_FLAV[7] = {"ATLANTA", "PI'ERRE", "METRO", "DRILL", "MEMPHIS", "WHOLE LOTTA RED", "MUSIC"};
static const char *const REEL_KEY[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static void gset_get(gset_t *s)
{
    uint32_t k;
    memset(s, 0, sizeof *s);
    s->seed = gp.seed; s->genre = gp.genre; s->energy = gp.energy; s->form = gp.form; s->complex = gp.complex;
    s->swing = gp.swing; s->human = gp.human; s->tempo = gp.tempo; s->key = gp.key; s->scale = gp.scale;
    s->length = gp.length; s->space = gp.space; s->fills = gp.fills; s->wet = gp.wet; s->grit = gp.grit;
    s->nlive = (uint8_t)(trk_live < NTRK ? trk_live : NTRK);
    for (k = 0; k < GP_N; k++) s->amt[k] = gp.amt[k];
}
static void gset_put(const gset_t *s)
{
    uint32_t k;
    gp.seed = s->seed; gp.genre = s->genre; gp.energy = s->energy; gp.form = s->form; gp.complex = s->complex;
    gp.swing = s->swing; gp.human = s->human; gp.tempo = s->tempo; gp.key = s->key; gp.scale = s->scale;
    gp.length = s->length; gp.space = s->space; gp.fills = s->fills; gp.wet = s->wet; gp.grit = s->grit;
    for (k = 0; k < GP_N; k++) gp.amt[k] = s->amt[k];
}
/* the piece just made, on the reel (the same recipe again: the cursor on it) */
static void reel_add(void)
{
    gset_t s;
    take_t *t;
    uint32_t i, k, nl = trk_live < NTRK ? trk_live : NTRK;
    gset_get(&s);
    for (i = 0; i < reel_n; i++)
        if (!memcmp(&reel[i].set, &s, sizeof s)) { reel_cur = reel_play = (uint8_t)i; return; }
    if (reel_n >= REEL_N) {                             /* full: the oldest not kept goes (all kept: the oldest) */
        for (i = 0; i < REEL_N && reel[i].keep; i++) {}
        if (i == REEL_N) i = 0;
        for (; i + 1u < REEL_N; i++) reel[i] = reel[i + 1u];   /* (no memmove here: libc.c) */
        reel_n = REEL_N - 1u;
    }
    t = &reel[reel_n];
    memset(t, 0, sizeof *t);
    t->set = s;
    str_cpy(t->name, gs.g->name, sizeof t->name);
    t->flav = (uint8_t)(gs.trap ? tk_tf + 1u : 0u);
    t->root = (uint8_t)(gs.root % 12);
    t->scl = (uint8_t)gs.scale;
    t->bpm = song.g[G_BPM];
    t->nl = (uint8_t)(nl > 6u ? 6u : nl);
    for (k = 0; k < t->nl; k++) {
        const track_t *tr = &trk[k];
        uint32_t len = (uint32_t)clamp(tr->p[P_SLEN], 1, NSTEP);
        for (i = 0; i < 32u; i++)
            if (tr->step[i % len].time == ST_NOTE) t->thumb[k] |= 1u << i;
        t->role[k] = gs.role[k];
    }
    if (live.mode)
        for (i = 0; i < chain_config.count && i < 10u; i++) {
            t->row[i][0] = chain_config.row[i].slot;
            t->row[i][1] = chain_config.row[i].bars;
            t->nrow = (uint8_t)(i + 1u);
        }
    reel_cur = reel_play = reel_n++;
}
/* OCT+ on the reel (gen_press): the take under the cursor made; 0: not on the reel (OCT+ makes a piece) */
static int gen_reel_load(void)
{
    if (!gp.view || !reel_n)
        return 0;
    gset_put(&reel[reel_cur].set);
    gen_go();
    reel_tempo(0);
    return 1;
}
/* a DICE ALL throw moves the tempo after the piece (a BPM off the last one): the take keeps the throw's (set), a recall
 * plays at it (the tempo on AUTO, not locked) */
static void reel_tempo(int set)
{
    if (reel_play >= reel_n) return;
    if (set) reel[reel_play].bpm = song.g[G_BPM];
    else if (!gp.tempo && !(ui_prefs & PREF_BPM_LOCK)) song.g[G_BPM] = reel[reel_play].bpm;
}
/* PRESETS: the reel opened (the settings' view: its first turn), then scrolled; playing, the take under the cursor
 * loads once PRESETS rests */
static void gen_reel_turn(int32_t s)
{
    if (!gp.view) {
        gp.view = 1;
    } else if (reel_n) {
        reel_cur = (uint8_t)clamp((int32_t)reel_cur + (s > 0 ? 1 : -1), 0, (int32_t)reel_n - 1);
        if ((song.playing || chain_busy()) && reel_cur != reel_play) {
            reel_want = (int8_t)reel_cur;
            gen_take_ms = fm1_ms | 1u;
        }
    }
    ui.force = 1;
}
/* KNOB 4 on the reel: keep (right) or let go (left) */
static void gen_reel_keep(int32_t s)
{
    if (reel_n) reel[reel_cur].keep = (uint8_t)(s > 0);
    ui.force = 1;
}
/* a thumbnail's colour, a role's */
static uint16_t reel_col(uint32_t role)
{
    static const uint8_t H[RL_N] = {230, 175, 70, 125, 205, 255};   /* DRUM BASS PAD LEAD ARP TEX */
    return heat_col(H[role % RL_N]);
}
static void draw_reel(void)
{
    char t[40];
    const take_t *k;
    uint32_t i, r, tot = 0, x;
    str_cpy(t, "TAKES  ", sizeof t);
    if (reel_n) {
        uint32_t n = str_len(t), c = reel_cur + 1u;
        if (c >= 10u) t[n++] = (char)('0' + c / 10u);
        t[n++] = (char)('0' + c % 10u); t[n++] = '/';
        if (reel_n >= 10u) t[n++] = (char)('0' + reel_n / 10u);
        t[n++] = (char)('0' + reel_n % 10u); t[n] = 0;
    } else {
        str_cpy(t + str_len(t), "EMPTY", sizeof t - str_len(t));
    }
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    if (!reel_n) {
        lcd_fill(0, 50, 240, 148, T_BG);
        draw_text_box(0, 100, 240, &AF_S, "EVERY PIECE MADE GOES HERE", T_MID, 1);
        draw_text_box(0, 116, 240, &AF_S, "OCT+ MAKES THE FIRST", T_DIM, 1);
    } else {
        char b[40];
        uint32_t nl, y0 = 56u, rh;
        k = &reel[reel_cur];
        nl = k->nl ? k->nl : 1u;
        rh = 60u / nl;
        cv_begin(240, 120, T_BG);
        if (reel_cur > 0u) cv_rrect(-12, 8, 22, 104, 6, T_SURF, T_BG);   /* (the takes either side) */
        if (reel_cur + 1u < reel_n) cv_rrect(230, 8, 22, 104, 6, T_SURF, T_BG);
        cv_rrect(16, 0, 208, 120, 8, reel_cur == reel_play ? T_RAISE : T_SURF, T_BG);
        cv_text(26, 3, &AF_M, k->name, T_TEXT);
        if (k->keep) {
            cv_rrect(176, 5, 40, 15, 4, T_THEME, T_SURF);
            cv_text(181, 6, &AF_S, "KEEP", T_INK);
        }
        if (reel_cur == reel_play && (song.playing || chain_busy())) {   /* (the music now) */
            cv_rrect(k->keep ? 136 : 176, 5, 36, 15, 4, T_TEXT, T_RAISE);
            cv_text(k->keep ? 141 : 181, 6, &AF_S, "NOW", T_INK);
        }
        if (k->flav) {
            cv_text(26, 23, &AF_S, REEL_FLAV[(k->flav - 1u) % 7u], T_THEME);
        } else {                                        /* (not trap: the tracks it ran on, its form) */
            b[0] = (char)('0' + k->nl); b[1] = 0;
            str_cpy(b + 1, k->nrow ? " TRACKS  SONG" : " TRACKS  LOOP", sizeof b - 1u);
            cv_text(26, 23, &AF_S, b, T_THEME);
        }
        b[0] = (char)('0' + k->set.seed / 1000u % 10u); b[1] = (char)('0' + k->set.seed / 100u % 10u);
        b[2] = (char)('0' + k->set.seed / 10u % 10u); b[3] = (char)('0' + k->set.seed % 10u);
        b[4] = ' '; b[5] = ' ';
        x = 6;
        if (k->bpm >= 100) b[x++] = (char)('0' + k->bpm / 100 % 10);
        b[x++] = (char)('0' + k->bpm / 10 % 10); b[x++] = (char)('0' + k->bpm % 10);
        b[x] = 0;
        str_cpy(b + x, " BPM  ", sizeof b - x);
        str_cpy(b + str_len(b), REEL_KEY[k->root % 12u], 4);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), N_SCALE[k->scl % NELEM(N_SCALE)], 6);
        cv_text(26, 37, &AF_S, b, T_MID);
        for (r = 0; r < nl; r++)                        /* the thumbnail: a row a live track, 32 steps */
            for (i = 0; i < 32u; i++)
                cv_rect((int32_t)(24u + i * 6u), (int32_t)(y0 + r * rh), 5, (int32_t)(rh > 2u ? rh - 2u : 1u),
                        (k->thumb[r] >> i) & 1u ? reel_col(k->role[r]) : reel_cur == reel_play ? T_SURF : T_RAISE);
        cv_blit(0, 50);
        /* the song: its rows as blocks (section colours), the playhead on the take playing */
        cv_begin(240, 24, T_BG);
        for (i = 0; i < k->nrow; i++) tot += k->row[i][1];
        if (!tot) {
            cv_rrect(24, 4, 192, 16, 4, T_SURF, T_BG);
            cv_text(108, 5, &AF_S, "LOOP", T_MID);
        } else {
            static const uint8_t SH[4] = {245, 120, 60, 200};   /* A B C D */
            static const char *const TRAP_SEC[4] = {"HOOK", "VERSE", "INTRO", "PRE"};   /* (narrow: H V I P) */
            uint32_t pos = 0, xs = 24;
            for (i = 0; i < k->nrow; i++) {
                uint32_t w = k->row[i][1] * 192u / tot, sl = k->row[i][0] & 3u;
                char sn[2] = {(char)(k->flav ? TRAP_SEC[sl][0] : 'A' + sl), 0};
                cv_rect((int32_t)xs, 4, (int32_t)(w > 1u ? w - 1u : 1u), 16, heat_col(SH[sl]));
                if (k->flav && (int32_t)w >= text_w(&AF_S, TRAP_SEC[sl]) + 5) cv_text((int32_t)xs + 3, 5, &AF_S, TRAP_SEC[sl], T_INK);
                else if ((int32_t)w >= text_w(&AF_S, sn) + 4) cv_text((int32_t)xs + 2, 5, &AF_S, sn, T_INK);
                if (reel_cur == reel_play && chain.running && i < chain.row) pos += k->row[i][1];
                xs += w;
            }
            if (reel_cur == reel_play && chain.running && chain.row < k->nrow) {
                uint32_t b2 = k->row[chain.row][1];
                pos += b2 > chain.remaining ? b2 - chain.remaining : 0u;
                cv_rect((int32_t)(24u + pos * 192u / tot), 0, 2, 24, T_TEXT);   /* (the playhead) */
            }
        }
        cv_blit(0, 172);
    }
    draw_text_box(0, 200, 240, &AF_S, "PRESETS SCROLL  OCT+ LOAD  K4 KEEP", T_TEXT, 1);
    draw_text_box(0, 216, 240, &AF_S, "SEL PAGES  ALGO SEED  HOME BACK", T_DIM, 1);
}
