/* SPDX-License-Identifier: GPL-3.0-only
 * (1.0-GW) TAKES: the generator's reel. Every piece made (OCT+, a take, DICE ALL) goes on it as its recipe — the
 * settings that remake it exactly (the generator is deterministic) — with what its card shows: the genre and the trap
 * flavour, the seed, the tempo, the key, each live track's first two bars as a thumbnail and the song's sections.
 * PRESETS on the GEN screen opens the reel and scrolls it; while the music plays the take under the cursor becomes the
 * music once PRESETS rests (gen_take_poll), stopped OCT+ loads it; KNOB 4 keeps a take (kept ones never drop off: the
 * reel holds the last 32). A recipe is not a recording: edits made after are not on it, SAVE keeps those.
 * (TAKES v2) A take has an ID of its own (#0001 .. #FFFF, never one still on the reel: a take's ID stays its ID when
 * others go) and its parent's: a take made from another (a part, a sound, a section made again, a setting changed) is
 * its child, the card says from which and what changed ("<0A3B DRUMS"); a new seed starts a family. EDIT deletes the
 * take under the cursor (OCT+ yes, OCT- no), EDIT held the takes not kept. Included by gen.c (after dice_print). */

static uint16_t heat_col(int32_t v);                    /* (ui_graph.c) */
#define REEL_N 32u
#define GEN_VERSION 2u                                  /* (TAKES v2) the generator a recipe was made with: a recipe is
                                                         * remade exactly by the same generator only */
typedef struct {                                        /* the recipe: what gen_make reads (and the tracks it ran on) */
    uint16_t seed;
    uint8_t genre, energy, form, complex, swing, human, tempo, key, scale, length, space, fills, wet, grit, nlive;
    uint8_t amt[GP_N];
    uint8_t var[SL_N], svar[RL_N], secvar[3][SL_PARTS], arr;
    struct { uint16_t seed; uint8_t what, var; } pin[GP_PINS];
} gset_t;
typedef struct {
    gset_t set;
    uint16_t id, parent;                                /* (v2) the take's ID, its parent's (0: a family's first) */
    char name[14];                                      /* the genre as played (a RANDOM throw's, a hybrid's) */
    uint8_t flav, root, scl, nl, keep, nrow;            /* flav: the trap flavour + 1 (0: none) */
    uint8_t how, gv;                                    /* (v2) what changed from the parent (GH_*), the generator */
    int16_t bpm;
    uint32_t thumb[6];                                  /* a live track's first 32 steps: bit i, a note or hit */
    uint8_t role[6];
    uint8_t row[12][2];                                 /* the song's rows: section, bars */
} take_t;
static take_t reel[REEL_N] __attribute__((section(".pool")));   /* (zeroed at boot) */
static uint8_t reel_n, reel_cur, reel_play = 0xFF;      /* takes, the cursor, the take playing now (0xFF: none) */
static int8_t reel_want = -1;                           /* a take to load once PRESETS rests (playing) */
static uint16_t reel_next = 1;                          /* (v2) the next ID */
static uint8_t reel_ask;                                /* (v2) a question on the card: 1 delete this take, 2 the unkept */
static const char *const REEL_FLAV[7] = {"ATLANTA", "PI'ERRE", "METRO", "DRILL", "MEMPHIS", "WHOLE LOTTA RED", "MUSIC"};
static const char *const REEL_KEY[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* what made a take from its parent (gen_how, set by whoever makes it: the card's "<0A3B DRUMS") */
static const char *const GH_NAME[GH_N] = {"", "", "SETTINGS", "SOUNDS", "DRUMS", "BASS", "CHORDS", "ARP", "LEAD", "TEX",
                                          "KIT", "BASS SND", "PAD SND", "LEAD SND", "ARP SND", "TEX SND", "HARMONY", "MIX",
                                          "FORM", "ENERGY", "KEY", "TEMPO", "SECTION", "BORROW", "VARY"};

static void gset_get(gset_t *s)
{
    uint32_t k;
    memset(s, 0, sizeof *s);
    s->seed = gp.seed; s->genre = gp.genre; s->energy = gp.energy; s->form = gp.form; s->complex = gp.complex;
    s->swing = gp.swing; s->human = gp.human; s->tempo = gp.tempo; s->key = gp.key; s->scale = gp.scale;
    s->length = gp.length; s->space = gp.space; s->fills = gp.fills; s->wet = gp.wet; s->grit = gp.grit;
    s->nlive = (uint8_t)(trk_live < NTRK ? trk_live : NTRK);
    for (k = 0; k < GP_N; k++) s->amt[k] = gp.amt[k];
    memcpy(s->var, gp.var, sizeof s->var);
    memcpy(s->svar, gp.svar, sizeof s->svar);
    memcpy(s->secvar, gp.secvar, sizeof s->secvar);
    s->arr = gp.arr;
    memcpy(s->pin, gp.pin, sizeof s->pin);
}
static void gset_put(const gset_t *s)
{
    uint32_t k;
    gp.seed = s->seed; gp.genre = s->genre; gp.energy = s->energy; gp.form = s->form; gp.complex = s->complex;
    gp.swing = s->swing; gp.human = s->human; gp.tempo = s->tempo; gp.key = s->key; gp.scale = s->scale;
    gp.length = s->length; gp.space = s->space; gp.fills = s->fills; gp.wet = s->wet; gp.grit = s->grit;
    for (k = 0; k < GP_N; k++) gp.amt[k] = s->amt[k];
    memcpy(gp.var, s->var, sizeof gp.var);
    memcpy(gp.svar, s->svar, sizeof gp.svar);
    memcpy(gp.secvar, s->secvar, sizeof gp.secvar);
    gp.arr = s->arr;
    memcpy(gp.pin, s->pin, sizeof gp.pin);
}
static int reel_find(uint16_t id)                       /* a take's place on the reel by its ID (-1: gone) */
{
    uint32_t i;
    for (i = 0; id && i < reel_n; i++)
        if (reel[i].id == id) return (int)i;
    return -1;
}
static uint16_t reel_new_id(void)                       /* the next ID: 0001 .. FFFF, round, never one on the reel */
{
    uint32_t n;
    for (n = 0; n < 0x10000u; n++) {
        uint16_t id = reel_next;
        reel_next = (uint16_t)(reel_next == 0xFFFFu ? 1u : reel_next + 1u);
        if (id && reel_find(id) < 0) return id;
    }
    return 1;
}
/* take i off the reel: the cursor and the take playing follow */
static void reel_drop(uint32_t i)
{
    uint32_t k = i;
    if (k >= reel_n) return;
    if (reel_play == k) reel_play = 0xFF;
    else if (reel_play != 0xFF && reel_play > k) reel_play--;
    if (reel_cur > k) reel_cur--;
    for (; k + 1u < reel_n; k++) reel[k] = reel[k + 1u];   /* (no memmove here: libc.c) */
    reel_n--;
    reel_want = -1;
    if (reel_cur >= reel_n) reel_cur = (uint8_t)(reel_n ? reel_n - 1u : 0u);
}
/* the piece just made, on the reel (the same recipe again: the cursor on it) */
static void reel_add(void)
{
    gset_t s;
    take_t *t;
    uint32_t i, k, nl = trk_live < NTRK ? trk_live : NTRK;
    uint16_t parent = 0;
    gset_get(&s);
    for (i = 0; i < reel_n; i++)
        if (!memcmp(&reel[i].set, &s, sizeof s)) { reel_cur = reel_play = (uint8_t)i; gen_how = GH_NONE; return; }
    if (gen_how > GH_NEW && reel_play < reel_n && reel[reel_play].set.seed == s.seed)
        parent = reel[reel_play].id;                    /* (a child: made from the take playing, its seed) */
    if (reel_n >= REEL_N) {                             /* full: the oldest not kept goes (all kept: the oldest) */
        for (i = 0; i < REEL_N && reel[i].keep; i++) {}
        reel_drop(i == REEL_N ? 0u : i);
    }
    t = &reel[reel_n];
    memset(t, 0, sizeof *t);
    t->set = s;
    t->id = reel_new_id();
    t->parent = parent;
    t->how = (uint8_t)(parent ? gen_how : GH_NONE);
    t->gv = GEN_VERSION;
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
        for (i = 0; i < chain_config.count && i < 12u; i++) {
            t->row[i][0] = chain_config.row[i].slot;
            t->row[i][1] = chain_config.row[i].bars;
            t->nrow = (uint8_t)(i + 1u);
        }
    reel_cur = reel_play = reel_n++;
    gen_how = GH_NONE;
}
/* a DICE ALL throw moves the tempo after the piece (a BPM off the last one): the take keeps the throw's (set), a recall
 * plays at it (the tempo on AUTO, not locked) */
static void reel_tempo(int set)
{
    if (reel_play >= reel_n) return;
    if (set) reel[reel_play].bpm = song.g[G_BPM];
    else if (!gp.tempo && !(ui_prefs & PREF_BPM_LOCK)) song.g[G_BPM] = reel[reel_play].bpm;
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
/* PRESETS: the reel opened (the settings' view: its first turn), then scrolled; playing, the take under the cursor
 * loads once PRESETS rests */
static struct {                                         /* (v2) OCT+ held on the reel: the take layer */
    uint8_t held, used, focus;                          /* OCT+ down; a key acted (no load when let go); KNOB 2's section */
    uint32_t down, t[27];                               /* the layer's keys down, since when */
} gl;
static struct { uint8_t on, bars, n; } jam = {0, 8, 0};   /* (v2) JAM: a take every `bars` bars, by itself */
/* (v2) the SET: takes in a row, each for its bars (REC while playing records one; the SET page edits and plays it) */
#define SET_N 32u
static struct { uint16_t id; uint8_t bars; } set_e[SET_N];
static uint8_t set_n, set_rec, set_play, set_i, set_left, set_cur, set_seen;
static void gen_reel_turn(int32_t s)
{
    reel_ask = 0;
    if (!gp.view) {
        gp.view = 1;
    } else if (reel_n) {
        reel_cur = (uint8_t)clamp((int32_t)reel_cur + (s > 0 ? 1 : -1), 0, (int32_t)reel_n - 1);
        if (gl.held) {                                  /* (v2) OCT+ held: a cue, the take when OCT+ is let go */
            reel_want = -1;
            gen_take_ms = 0;
        } else if ((song.playing || chain_busy()) && reel_cur != reel_play) {
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
/* (v2) EDIT on the reel: ask to delete the take under the cursor (held: the takes not kept); a kept take stays */
static void gen_reel_edit(int held)
{
    uint32_t i, n = 0;
    if (!reel_n) return;
    if (held) {
        for (i = 0; i < reel_n; i++) n += !reel[i].keep;
        if (!n) { ui_message("ALL TAKES KEPT"); return; }
        reel_ask = 2;
    } else if (reel[reel_cur].keep) {
        ui_message("KEPT: KNOB 4 LEFT FIRST");
        return;
    } else {
        reel_ask = 1;
    }
    ui.force = 1;
}
/* the answer: OCT+ yes (1), OCT- no (0) */
static void gen_reel_answer(int yes)
{
    uint32_t i, n = 0;
    if (yes && reel_ask == 1u && reel_cur < reel_n) {
        reel_drop(reel_cur);
        ui_message("TAKE DELETED");
    } else if (yes && reel_ask == 2u) {
        for (i = reel_n; i-- > 0;)
            if (!reel[i].keep) { reel_drop(i); n++; }
        ui_message(n ? "UNKEPT TAKES DELETED" : "ALL TAKES KEPT");
    }
    reel_ask = 0;
    ui.force = 1;
}
/* a thumbnail's colour, a role's */
static uint16_t reel_col(uint32_t role)
{
    static const uint8_t H[RL_N] = {230, 175, 70, 125, 205, 255};   /* DRUM BASS PAD LEAD ARP TEX */
    return heat_col(H[role % RL_N]);
}
static void reel_hex(char *b, uint32_t id)              /* "0A3F" */
{
    static const char HX[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    b[0] = HX[(id >> 12) & 15u]; b[1] = HX[(id >> 8) & 15u]; b[2] = HX[(id >> 4) & 15u]; b[3] = HX[id & 15u]; b[4] = 0;
}
static void draw_reel(void)
{
    char t[40];
    const take_t *k;
    uint32_t i, r, tot = 0, x;
    str_cpy(t, "TAKES  ", sizeof t);
    if (reel_n) {
        uint32_t n = str_len(t);
        if (reel_n >= 10u) t[n++] = (char)('0' + reel_n / 10u);
        t[n++] = (char)('0' + reel_n % 10u); t[n] = 0;
    } else {
        str_cpy(t + str_len(t), "EMPTY", sizeof t - str_len(t));
    }
    if (gen_take_bar && gen_take_ms) str_cpy(t + str_len(t), "  NEXT BAR", sizeof t - str_len(t));   /* (v2) */
    else if (set_rec) str_cpy(t + str_len(t), "  REC SET", sizeof t - str_len(t));
    else if (jam.on) str_cpy(t + str_len(t), "  JAM", sizeof t - str_len(t));
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    if (!reel_n) {
        lcd_fill(0, 50, 240, 148, T_BG);
        draw_text_box(0, 100, 240, &AF_S, "EVERY PIECE MADE GOES HERE", T_MID, 1);
        draw_text_box(0, 116, 240, &AF_S, "OCT+ MAKES THE FIRST", T_DIM, 1);
    } else {
        char b[40];
        uint32_t nl, y0 = 56u, rh;
        int32_t nw;
        k = &reel[reel_cur];
        nl = k->nl ? k->nl : 1u;
        rh = 60u / nl;
        cv_begin(240, 120, T_BG);
        if (reel_cur > 0u) cv_rrect(-12, 8, 22, 104, 6, T_SURF, T_BG);   /* (the takes either side) */
        if (reel_cur + 1u < reel_n) cv_rrect(230, 8, 22, 104, 6, T_SURF, T_BG);
        cv_rrect(16, 0, 208, 120, 8, reel_cur == reel_play ? T_RAISE : T_SURF, T_BG);
        b[0] = '#';
        reel_hex(b + 1, k->id);
        cv_text(26, 6, &AF_S, b, T_THEME);               /* (v2) its ID */
        nw = text_w(&AF_S, b);
        cv_text(26 + nw + 6, 3, &AF_M, k->name, T_TEXT);
        if (k->keep) {
            cv_rrect(176, 5, 40, 15, 4, T_THEME, T_SURF);
            cv_text(181, 6, &AF_S, "KEEP", T_INK);
        }
        if (reel_cur == reel_play && (song.playing || chain_busy())) {   /* (the music now) */
            cv_rrect(k->keep ? 136 : 176, 5, 36, 15, 4, T_TEXT, T_RAISE);
            cv_text(k->keep ? 141 : 181, 6, &AF_S, "NOW", T_INK);
        }
        if (k->flav) {
            str_cpy(b, REEL_FLAV[(k->flav - 1u) % 7u], sizeof b);
        } else {                                        /* (not trap: the tracks it ran on, its form) */
            b[0] = (char)('0' + k->nl); b[1] = 0;
            str_cpy(b + 1, k->nrow ? " TRACKS  SONG" : " TRACKS  LOOP", sizeof b - 1u);
        }
        cv_text(26, 23, &AF_S, b, T_THEME);
        if (k->parent) {                                /* (v2) its parent and what changed: "<0A3B DRUMS" */
            int32_t w0 = text_w(&AF_S, b), w;
            b[0] = '<';
            reel_hex(b + 1, k->parent);
            if (k->how && k->how < GH_N) { b[5] = ' '; str_cpy(b + 6, GH_NAME[k->how], sizeof b - 6u); }
            w = text_w(&AF_S, b);
            if (26 + w0 + 8 + w <= 214) cv_text(214 - w, 23, &AF_S, b, reel_find(k->parent) >= 0 ? T_MID : T_DIM);
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
                int lit = !gl.focus || sl == gl.focus;  /* (v2) KNOB 2's section lit, the others dim */
                cv_rect((int32_t)xs, 4, (int32_t)(w > 1u ? w - 1u : 1u), 16, lit ? heat_col(SH[sl]) : T_RAISE);
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
        if (!gl.held) cv_blit(0, 172);
    }
    if (gl.held) {                                      /* (v2) OCT+ held: the keys */
        static const char *const LG[4] = {"F DRM G BAS A CHD B ARP C LED D TEX", "BLACK KEY: ITS SOUND  HOLD: LOCK",
                                          "E SOUNDS F HARM G- A+ ENRG B MIX", "C- D+ KEY E- F+ BPM G VARY C# UP"};
        uint32_t i;
        for (i = 0; i < 4u; i++) draw_text_box(0, 172 + i * 16u, 240, &AF_S, LG[i], i & 1u ? T_MID : T_TEXT, 1);
        return;
    }
    if (reel_ask) {                                     /* (v2) the question, in the footer */
        char q[32];
        if (reel_ask == 1u) {
            str_cpy(q, "DELETE #", sizeof q);
            reel_hex(q + 8, reel[reel_cur].id);
            str_cpy(q + 12, "?", 2);
        } else {
            str_cpy(q, "DELETE ALL NOT KEPT?", sizeof q);
        }
        draw_text_box(0, 200, 240, &AF_S, q, T_ACCENT, 1);
        draw_text_box(0, 216, 240, &AF_S, "OCT+ YES   OCT- NO", T_TEXT, 1);
        return;
    }
    if (gl.focus) {
        static const char *const SN[4] = {"", "SECTION B", "SECTION C", "SECTION D"};
        static const char *const TSN[4] = {"", "VERSE ONLY", "INTRO ONLY", "PRE-HOOK ONLY"};
        draw_text_box(0, 200, 240, &AF_S, reel_n && reel[reel_cur].flav ? TSN[gl.focus & 3u] : SN[gl.focus & 3u], T_ACCENT, 1);
        draw_text_box(0, 216, 240, &AF_S, "OCT+ AND A PART KEY: THAT PART", T_DIM, 1);
        return;
    }
    draw_text_box(0, 200, 240, &AF_S, "PRESETS SCROLL  OCT+ LOAD  K4 KEEP", T_TEXT, 1);
    draw_text_box(0, 216, 240, &AF_S, "ALGO SOUNDS  EDIT DEL  REC SET", T_DIM, 1);
}

/* ------------------------------------------------------------------- (v2) OCT+ held on the reel: the take layer --- */
/* A key makes a take from the one playing (a child: the card says what), on the next bar (stopped: at once): the white
 * keys F3 .. D4 a part's notes made again (DRUMS BASS CHORDS ARP LEAD TEX), the black key next to each its sound, E4 every
 * sound, F4 the harmony, G4 / A4 ENERGY down / up, B4 the mix, C5 / D5 the key down / up, E5 / F5 the tempo, G5 VARY (the
 * ornaments: echoes, octaves, bends, the kit's drift), C#5 back to the parent take. A part or a sound key held half a
 * second locks it instead (ALGORITHM and the rerolls leave it; a new seed keeps it). On another take's card (PRESETS
 * turned with OCT+ held), a part or a sound key borrows that take's into the playing one. Let go of OCT+ without a key:
 * the take under the cursor, on the next bar. seq.c keeps the keys silent (layer_masks) */
enum { GK_SOUNDS = 0x20, GK_HARM, GK_NRG_DN, GK_NRG_UP, GK_MIX, GK_KEY_DN, GK_KEY_UP, GK_BPM_DN, GK_BPM_UP, GK_VARY,
       GK_PARENT, GK_ARR_DN, GK_ARR_UP, GK_JAM };
static const uint8_t GKEY[27] = {                       /* F3 .. G5: a part (SL_*), a role's sound (0x10 + RL_*), an act */
    SL_DRUMS, 0x10 | RL_DRUM, SL_BASS, 0x10 | RL_BASS, SL_CHORDS, 0x10 | RL_PAD, SL_ARP, SL_LEAD, 0x10 | RL_LEAD, SL_TEX,
    0x10 | RL_TEX, GK_SOUNDS, GK_HARM, 0x10 | RL_ARP, GK_NRG_DN, GK_ARR_DN, GK_NRG_UP, GK_ARR_UP, GK_MIX, GK_KEY_DN,
    GK_PARENT, GK_KEY_UP, GK_JAM, GK_BPM_DN, GK_BPM_UP, 0, GK_VARY};
static const char *const GK_SAY[RL_N] = {"KIT", "BASS SOUND", "PAD SOUND", "LEAD SOUND", "ARP SOUND", "TEX SOUND"};
static const char *const SL_SAY[SL_PARTS] = {"DRUMS", "BASS", "CHORDS", "ARP", "LEAD", "TEX"};
/* a part or a sound borrowed: its pin (the same kind's replaced, else a free one, else the last) */
static void gl_pin(uint32_t what, uint32_t seed, uint32_t var)
{
    uint32_t i, at = GP_PINS - 1u;
    for (i = 0; i < GP_PINS; i++) if (gp.pin[i].what == what) { at = i; break; }
    if (i == GP_PINS)
        for (i = 0; i < GP_PINS; i++) if (!gp.pin[i].what) { at = i; break; }
    gp.pin[at].what = (uint8_t)what; gp.pin[at].seed = (uint16_t)seed; gp.pin[at].var = (uint8_t)var;
}
static void gl_unpin(uint32_t what)
{
    uint32_t i;
    for (i = 0; i < GP_PINS; i++) if (gp.pin[i].what == what) gp.pin[i].what = 0;
}
/* another take's own seed and variation for a stream or a sound (its pin, else its seed and variation) */
static uint32_t gl_of(const take_t *o, uint32_t what, uint32_t v, uint32_t *sd)
{
    uint32_t i;
    *sd = o->set.seed;
    for (i = 0; i < GP_PINS; i++) if (o->set.pin[i].what == what) { *sd = o->set.pin[i].seed; return o->set.pin[i].var; }
    return v;
}
static void gl_go(uint32_t how, const char *what, const char *say)
{
    gen_take_arm(how);
    gen_take_ms = (fm1_ms - 460u) | 1u;                 /* (no rest: the next bar; stopped, now) */
    gl.used = 1;
    ui_say(what, say);
    ui.force = 1;
}
static void gl_act(uint32_t c)
{
    const take_t *o = reel_cur < reel_n && reel_cur != reel_play && reel_play < reel_n ? &reel[reel_cur] : 0;
    uint32_t sd, v;
    int32_t x;
    if (c >= SL_DRUMS && c <= SL_TEX) {                 /* a part's notes */
        if (o) {                                        /* (borrowed from the take under the cursor) */
            v = gl_of(o, 1u + c, o->set.var[c], &sd);
            if (sd == gp.seed && !gpinned(1u + c)) gp.var[c] = (uint8_t)v;
            else gl_pin(1u + c, sd, v);
            gl_go(GH_BORROW, "BORROWED ", SL_SAY[c - SL_DRUMS]);
        } else if (gl.focus) {                          /* (a section's own: phase 3) */
            gp.secvar[(gl.focus - 1u) % 3u][c - SL_DRUMS]++;
            gl_go(GH_SECTION, "NEW SECTION ", SL_SAY[c - SL_DRUMS]);
        } else {
            gl_unpin(1u + c);
            gp.var[c]++;
            gl_go(GH_DRUMS + c - SL_DRUMS, "NEW ", SL_SAY[c - SL_DRUMS]);
        }
        return;
    }
    if (c >= 0x10u && c < 0x10u + RL_N) {               /* a role's sound */
        uint32_t r = c - 0x10u;
        if (o) {
            v = gl_of(o, 16u + r, o->set.svar[r], &sd);
            if (sd == gp.seed && !gpinned(16u + r)) gp.svar[r] = (uint8_t)v;
            else gl_pin(16u + r, sd, v);
            gl_go(GH_BORROW, "BORROWED ", GK_SAY[r]);
        } else {
            gl_unpin(16u + r);
            gp.svar[r]++;
            gl_go(GH_KIT + r, "NEW ", GK_SAY[r]);
        }
        return;
    }
    switch (c) {
    case GK_SOUNDS:
        for (v = 0; v < RL_N; v++) if (!((gp.lock >> (8u + v)) & 1u)) { gl_unpin(16u + v); gp.svar[v]++; }
        gl_go(GH_SOUNDS, "NEW SOUNDS", "");
        break;
    case GK_HARM: gp.var[SL_HARM]++; gl_go(GH_HARM, "NEW HARMONY", ""); break;
    case GK_NRG_DN: case GK_NRG_UP:
        gp.energy = (uint8_t)clamp((int32_t)gp.energy + (c == GK_NRG_UP ? 10 : -10), 0, 100);
        gl_go(GH_ENERGY, c == GK_NRG_UP ? "ENERGY UP" : "ENERGY DOWN", "");
        break;
    case GK_MIX: gp.var[SL_MIX]++; gl_go(GH_MIX, "NEW MIX", ""); break;
    case GK_KEY_DN: case GK_KEY_UP:
        x = gp.key ? (int32_t)gp.key - 1 : gs.root;      /* (AUTO: from the key playing) */
        x = (x + (c == GK_KEY_UP ? 1 : 11)) % 12;
        gp.key = (uint8_t)(x + 1);
        gl_go(GH_KEY, "KEY ", REEL_KEY[x % 12]);
        break;
    case GK_BPM_DN: case GK_BPM_UP:
        x = gp.tempo ? gp.tempo : song.g[G_BPM];
        gp.tempo = (uint8_t)clamp(x + (c == GK_BPM_UP ? 2 : -2), 60, 200);
        gl_go(GH_TEMPO, c == GK_BPM_UP ? "TEMPO UP" : "TEMPO DOWN", "");
        break;
    case GK_VARY: gp.var[SL_FORM]++; gl_go(GH_VARY, "VARIED", ""); break;
    case GK_ARR_DN: case GK_ARR_UP: {
        char b[4] = "1/4";
        if (!live.mode) { ui_message("A LOOP: NO SECTIONS"); break; }
        gp.arr = (uint8_t)((gp.arr + (c == GK_ARR_UP ? 1u : 3u)) % 4u);
        b[0] = (char)('1' + gp.arr);
        gl_go(GH_FORM, "ARRANGEMENT ", b);
        break;
    }
    case GK_JAM: {
        char b[4] = {0};
        jam.on = (uint8_t)!jam.on;
        jam.n = 0;
        gl.used = 1;
        b[0] = (char)('0' + jam.bars / 10u); b[1] = (char)('0' + jam.bars % 10u);
        if (b[0] == '0') { b[0] = b[1]; b[1] = 0; }
        if (jam.on) { ui_say("JAM: A TAKE EVERY BARS ", b); str_cpy(ui.msg2, "D#5 AGAIN: JAM OFF", sizeof ui.msg2); }
        else ui_message("JAM OFF");
        ui.force = 1;
        break;
    }
    case GK_PARENT:
        if (reel_play < reel_n && reel[reel_play].parent) {
            int i = reel_find(reel[reel_play].parent);
            if (i < 0) { ui_message("PARENT DELETED"); break; }
            reel_cur = (uint8_t)i;
            reel_want = (int8_t)i;
            gl_go(GH_NONE, "BACK TO PARENT", "");
        } else {
            ui_message("FIRST OF ITS FAMILY");
        }
        break;
    default: break;
    }
}
/* a part key or a sound key held: its lock */
static void gl_lock(uint32_t c)
{
    uint32_t bit = c >= 0x10u ? 8u + (c - 0x10u) : c, on;
    gp.lock ^= (uint16_t)(1u << bit);
    on = (gp.lock >> bit) & 1u;
    ui_say(on ? "LOCKED " : "UNLOCKED ", c >= 0x10u ? GK_SAY[(c - 0x10u) % RL_N] : SL_SAY[(c - SL_DRUMS) % SL_PARTS]);
    gl.used = 1;
    ui.force = 1;
}
/* every frame on the GEN screen: the layer's keys (lkeys: pressed with OCT+ held, silent: seq.c) */
static void gen_layer(uint32_t lkeys)
{
    uint32_t held = gp.view && ((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u), k, up;
    if (held && !gl.held) gl.used = 0;
    if (held != gl.held) ui.force = 1;
    gl.held = (uint8_t)held;
    up = gl.down & ~fm1_in.notes;
    for (k = 0; k < 27u; k++)
        if ((up >> k) & 1u) {                           /* a part or a sound key let go: held long, its lock */
            uint32_t c = GKEY[k];
            gl.down &= ~(1u << k);
            if (fm1_ms - gl.t[k] >= 500u) gl_lock(c);
            else gl_act(c);
        }
    if (!held) return;
    for (k = 0; k < 27u; k++)
        if ((lkeys >> k) & 1u) {
            uint32_t c = GKEY[k];
            gl.used = 1;
            if (c && c < 0x20u) { gl.down |= 1u << k; gl.t[k] = fm1_ms; }   /* (on release: tap or hold) */
            else if (c) gl_act(c);
        }
}

/* ---------------------------------------------------------------------------------- (v2) KNOB 2, KNOB 3, JAM --- */
/* KNOB 2 on the reel: the section the part keys make again (ALL, B, C, D: a song's; a loop has none) */
static void gen_reel_focus(int32_t s)
{
    static const char *const SN[4] = {"ALL SECTIONS", "SECTION B", "SECTION C", "SECTION D"};
    if (!reel_n || !reel[reel_cur].nrow) { gl.focus = 0; ui_message("A LOOP: NO SECTIONS"); return; }
    gl.focus = (uint8_t)clamp((int32_t)gl.focus + (s > 0 ? 1 : -1), 0, 3);
    ui_say(SN[gl.focus], "");
    ui.force = 1;
}
/* KNOB 3 on the reel: JAM's bars (4, 8, 16, 32) */
static void gen_reel_jam_bars(int32_t s)
{
    static const uint8_t B[4] = {4, 8, 16, 32};
    uint32_t i;
    char b[4] = {0};
    for (i = 0; i < 3u && B[i] != jam.bars; i++) {}
    i = (uint32_t)clamp((int32_t)i + (s > 0 ? 1 : -1), 0, 3);
    jam.bars = B[i];
    b[0] = (char)('0' + jam.bars / 10u); b[1] = (char)('0' + jam.bars % 10u);
    if (b[0] == '0') { b[0] = b[1]; b[1] = 0; }
    ui_say("JAM EVERY BARS ", b);
    ui.force = 1;
}
/* a bar went by (the music playing, the GEN screen open): JAM's count; its time: a take with one thing made again (a
 * part's notes mostly, a sound, the ornaments, the mix; never a locked one), on the next bar */
static void jam_bar(void)
{
    static const uint8_t W[4] = {60, 25, 10, 5};        /* a part, a sound, VARY, the mix */
    uint32_t c = 0, tries;
    if (!jam.on || gen_take_ms || ++jam.n < jam.bars) return;
    jam.n = 0;
    reel_cur = reel_play < reel_n ? reel_play : reel_cur;   /* (from the take playing: never a borrow) */
    for (tries = 0; tries < 12u && !c; tries++) {
        uint32_t w = rng() % 100u, k = W[0], r;
        if (w < k) {
            r = SL_DRUMS + rng() % SL_PARTS;
            if (!((gp.lock >> r) & 1u) && gs.act[SL_ROLE[r - SL_DRUMS]]) c = r;
        } else if (w < (k += W[1])) {
            r = rng() % RL_N;
            if (!((gp.lock >> (8u + r)) & 1u) && gs.trk_of[r] < NTRK) c = 0x10u + r;
        } else if (w < k + W[2]) {
            c = GK_VARY;
        } else {
            c = GK_MIX;
        }
    }
    if (c) { gl_act(c); gl.used = 0; }
}

/* ---------------------------------------------------------------------------------------- (v2) the SET --- */
/* REC on the reel: playing, a SET recorded (a new one: the take playing first, then every take that becomes the music,
 * each with the bars it played; REC again ends it); stopped, the SET page */
static void set_note_take(void)                         /* a take became the music: the SET's next entry */
{
    if (!set_rec || reel_play >= reel_n) return;
    if (set_n && set_e[set_n - 1u].id == reel[reel_play].id) return;
    if (set_n >= SET_N) { set_rec = 0; ui_message("SET FULL (32)"); return; }
    set_e[set_n].id = reel[reel_play].id;
    set_e[set_n].bars = 0;
    set_n++;
}
static void gen_reel_rec(void)
{
    if (gp.view == 2u) { gp.view = 1; ui.force = 1; return; }   /* (the SET page: back to the reel) */
    if (song.playing || chain_busy()) {
        set_rec = (uint8_t)!set_rec;
        if (set_rec) { set_n = 0; set_note_take(); ui_message("SET: RECORDING"); }
        else {
            if (set_n && !set_e[set_n - 1u].bars) set_n--;   /* (a take that never played a bar) */
            ui_message(set_n ? "SET RECORDED: REC STOPPED" : "SET EMPTY");
        }
    } else if (set_n) {
        gp.view = 2;
        set_cur = 0;
    } else {
        ui_message("SET EMPTY: REC WHILE PLAYING");
    }
    ui.force = 1;
}
/* the take an entry plays (-1: deleted) */
static int set_take(uint32_t i) { return i < set_n ? reel_find(set_e[i].id) : -1; }
static void set_arm(uint32_t i)                         /* an entry's take, on the next bar */
{
    int t = set_take(i);
    if (t < 0) return;
    reel_want = (int8_t)t;
    reel_cur = (uint8_t)t;
    gen_take_arm(GH_NONE);
    gen_take_ms = (fm1_ms - 460u) | 1u;
}
/* a bar went by: the SET's count (recording: the take's bars; playing: the next entry armed a bar before its turn) */
static void set_bar(void)
{
    if (set_rec && set_n && set_e[set_n - 1u].bars < 255u) set_e[set_n - 1u].bars++;
    if (!set_play || !set_n) return;
    if (set_left > 1u) set_left--;
    else {
        set_i = (uint8_t)((set_i + 1u) % set_n);
        set_left = set_e[set_i].bars ? set_e[set_i].bars : 1u;
    }
    if (set_left == 1u) set_arm((set_i + 1u) % set_n);
}
/* PLAY on the SET page: from the entry under the cursor (stopped), or stopped */
static void set_toggle(void)
{
    int t;
    if (song.playing || chain_busy() || seq_counting()) { transport_req = 2; set_play = 0; return; }
    if (!set_n || (t = set_take(set_cur)) < 0) { ui_message("TAKE DELETED"); return; }
    gset_put(&reel[t].set);
    gen_how = GH_NONE;
    gen_go();
    reel_tempo(0);
    gp.sig = gen_sig();
    set_play = 1;
    set_seen = 0;
    set_i = set_cur;
    set_left = set_e[set_i].bars ? set_e[set_i].bars : 1u;
    if (set_left == 1u && set_n > 1u) set_arm((set_i + 1u) % set_n);
    if (live.mode) chain_play_ui();
    else transport_req = 1;
}
/* the SET page's knobs: KNOB 1 the entry, KNOB 2 its take (the reel's, in order), KNOB 3 its bars */
static void set_knob(uint32_t k, int32_t s)
{
    if (!set_n) return;
    if (k == 0u) set_cur = (uint8_t)clamp((int32_t)set_cur + (s > 0 ? 1 : -1), 0, (int32_t)set_n - 1);
    else if (k == 1u && reel_n) {
        int t = set_take(set_cur);
        t = clamp((t < 0 ? 0 : t) + (s > 0 ? 1 : -1), 0, (int32_t)reel_n - 1);
        set_e[set_cur].id = reel[t].id;
    } else if (k == 2u) {
        set_e[set_cur].bars = (uint8_t)clamp((int32_t)set_e[set_cur].bars + (s > 0 ? 1 : -1), 1, 64);
    }
    ui.force = 1;
}
/* OCT+ on the SET page: the take playing (else under the reel's cursor) after the entry; EDIT: the entry out */
static void set_insert(void)
{
    uint32_t i, t = reel_play < reel_n ? reel_play : reel_cur;
    if (set_n >= SET_N || !reel_n) return;
    for (i = set_n; i > (uint32_t)set_cur + 1u; i--) set_e[i] = set_e[i - 1u];
    i = set_n ? set_cur + 1u : 0u;
    set_e[i].id = reel[t].id;
    set_e[i].bars = 4;
    set_n++;
    set_cur = (uint8_t)i;
    ui.force = 1;
}
static void set_delete(void)
{
    uint32_t i;
    if (!set_n) return;
    for (i = set_cur; i + 1u < set_n; i++) set_e[i] = set_e[i + 1u];
    set_n--;
    if (set_cur >= set_n && set_cur) set_cur--;
    if (!set_n) { gp.view = 1; ui_message("SET EMPTY"); }
    ui.force = 1;
}
static void draw_set(void)
{
    char t[32], b[40];
    uint32_t i, tot = 0, xs = 16, pos = 0;
    str_cpy(t, set_play ? "SET  PLAYING" : "SET", sizeof t);
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    for (i = 0; i < set_n; i++) tot += set_e[i].bars ? set_e[i].bars : 1u;
    cv_begin(240, 40, T_BG);
    for (i = 0; i < set_n; i++) {                       /* the entries as blocks, as long as their bars */
        uint32_t bars = set_e[i].bars ? set_e[i].bars : 1u, w = bars * 208u / (tot ? tot : 1u);
        int tk = set_take(i);
        uint16_t c = tk < 0 ? T_RAISE : heat_col((int32_t)(60u + (set_e[i].id * 37u) % 190u));
        cv_rect((int32_t)xs, i == set_cur ? 2 : 8, (int32_t)(w > 1u ? w - 1u : 1u), i == set_cur ? 36 : 24, c);
        if (set_play && i < set_i) pos += bars;
        xs += w;
    }
    if (set_play && set_i < set_n) {
        uint32_t bars = set_e[set_i].bars ? set_e[set_i].bars : 1u;
        pos += bars - (set_left < bars ? set_left : bars);
        cv_rect((int32_t)(16u + pos * 208u / (tot ? tot : 1u)), 0, 2, 40, T_TEXT);
    }
    cv_blit(0, 56);
    lcd_fill(0, 100, 240, 96, T_BG);
    if (set_n) {
        int tk = set_take(set_cur);
        uint32_t n;
        b[0] = (char)('0' + (set_cur + 1u) / 10u); b[1] = (char)('0' + (set_cur + 1u) % 10u);
        str_cpy(b + 2, " OF ", sizeof b - 2u);
        n = str_len(b);
        b[n++] = (char)('0' + set_n / 10u); b[n++] = (char)('0' + set_n % 10u); b[n] = 0;
        draw_text_box(0, 104, 240, &AF_S, b[0] == '0' ? b + 1 : b, T_MID, 1);
        b[0] = '#';
        reel_hex(b + 1, set_e[set_cur].id);
        str_cpy(b + 5, "  ", 3);
        str_cpy(b + 7, tk < 0 ? "DELETED" : reel[tk].name, sizeof b - 7u);
        draw_text_box(0, 122, 240, &AF_M, b, tk < 0 ? T_DIM : T_TEXT, 1);
        n = set_e[set_cur].bars;
        b[0] = (char)('0' + n / 10u); b[1] = (char)('0' + n % 10u);
        str_cpy(b + 2, n == 1u ? " BAR" : " BARS", sizeof b - 2u);
        draw_text_box(0, 146, 240, &AF_M, b[0] == '0' ? b + 1 : b, T_THEME, 1);
    }
    draw_text_box(0, 184, 240, &AF_S, "K1 ENTRY  K2 TAKE  K3 BARS", T_TEXT, 1);
    draw_text_box(0, 200, 240, &AF_S, "PLAY RUN  OCT+ ADD  EDIT DEL", T_TEXT, 1);
    draw_text_box(0, 216, 240, &AF_S, "REC OR HOME: THE REEL", T_DIM, 1);
}
