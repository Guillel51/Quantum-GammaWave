/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT (0.6.91) the song generator, second version: HOME + SELECT opens GEN, a screen of three pages (GENRE SEED
 * ENERGY FORM; DRUMS BASS CHORDS ARP; LEAD COMPLEX SWING HUMAN), OCT+ makes the piece, HOME (or OCT-) is done. Included by
 * ui_input.c after song_dice.c (whose helpers it shares: the roles, dhold, euclid, dtonic, dname_has, dlevel ..).
 *
 * The piece is deterministic: a seed and the knobs make the same piece every time (the same settings and OCT+ again
 * throw a new seed). Each part (drums, bass, chords, arp, lead, sounds, form) draws from its own stream, so turning
 * one part's knob leaves the others as they were. What the tables know (gen_tables.c): the genre's tempo, scales and
 * progressions, its drums as a map of priorities (a hit where the level clears the density: ghosts go first, anchors
 * last), its bass as one of ~50 archetypes written against the kick (the root at every chord change, approaches by a
 * semitone, the kick doubled, answered or interlocked), its chords with a quality each, voiced in C3..G5 and led to
 * the nearest voicing (or moved in parallel), its arp as an order over the voiced chord (polymetric cycles of 3 or 5),
 * its lead as a one-bar motif in a phrase (A A A A', A R A R' ..), varied by one operator a bar, its sounds as recipes
 * a role (a preset whose name fits, the envelope, the filter, a slow LFO, the sends), its kit, HUMAN by how human the
 * genre is, the snare laid back where the genre does. FORM: a loop, parts (A B A C in ARRANGE, as 0.6.81), or a club
 * song (intro, build with a filter opening and a roll, drop, break), each section the same piece with parts removed
 * or varied. UNDO (GLO + G4) brings back what was there. */

#include "gen_tables.c"

/* ------------------------------------------------------------------------------------------- the settings --- */
static struct {
    uint8_t genre;                                      /* 0 RANDOM, 1 + the genre */
    uint16_t seed;                                      /* 0..9999 */
    uint8_t energy, form;                               /* 0..100; 0 LOOP, 1 PARTS, 2 SONG */
    uint8_t amt[GP_N];                                  /* 0 OFF .. 100, GA_AUTO the genre's */
    uint8_t complex, swing, human;                      /* 0..100 (swing: GA_AUTO the genre's) */
    uint8_t page, hot;                                  /* the GEN screen's page, its last knob */
    uint32_t sig;                                       /* the settings of the last piece made (OCT+ again: a new seed) */
    uint32_t open_ms;                                   /* when the screen opened (SELECT still turning: no page) */
    char last[20];                                      /* the last piece: "HOUSE 0421" */
} gp = {0, 1, 50, 1, {GA_AUTO, GA_AUTO, GA_AUTO, GA_AUTO, GA_AUTO}, 50, GA_AUTO, 30, 0, 0, 0, 0, ""};

/* the generator's own random numbers: a stream a part, from the seed */
static uint32_t gq = 1;
static uint32_t gr(void) { gq ^= gq << 13; gq ^= gq >> 17; gq ^= gq << 5; return gq; }
static void gseed(uint32_t part)
{
    gq = ((uint32_t)gp.seed + 1u) * 2654435761u ^ (part + 1u) * 0x9E3779B9u ^ 0x6D2B79F5u;
    if (!gq) gq = 1;
    gr(); gr(); gr();
}
static int32_t grr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(gr() % (uint32_t)(hi - lo + 1)); }
static int gch(uint32_t pct) { return gr() % 100u < pct; }
static uint32_t gpickw(const uint8_t *w, uint32_t n)    /* an index by its weight */
{
    uint32_t i, s = 0, r;
    for (i = 0; i < n; i++) s += w[i];
    if (!s) return 0;
    r = gr() % s;
    for (i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; }
    return n - 1u;
}
enum { GS_PICK, GS_DRUMS, GS_BASS, GS_CHORDS, GS_ARP, GS_LEAD, GS_SOUND, GS_FORM, GS_MIX, GS_TEX };

/* ------------------------------------------------------------------------------------------------ the piece --- */
static struct {
    const ggenre_t *g;
    uint32_t gi;
    int16_t root, scale;
    const gprog_t *prog;
    uint8_t q[4];                                       /* the chords' qualities as voiced (the mode's) */
    uint32_t n, csteps, len, nsc;                       /* chords, steps a chord, the loop, the scale's notes */
    uint32_t amt[GP_N], energy, complex, human, swing;
    uint8_t kick[NSTEP];                                /* the kick's steps (the bass reads them) */
    uint8_t snare[NSTEP];
    int32_t vo[4]; uint32_t nvo;                        /* the chord track's last voicing */
    int32_t avo[4]; uint32_t navo;                      /* the arp's */
    uint32_t fm6, ba, vm;
    uint8_t role[NTRK], act[RL_N], trk_of[RL_N];
    uint32_t drum_fill;                                 /* the fill forced (a BUILD section) */
    uint8_t mutant;                                     /* (0.6.95) RANDOM: a hybrid of two or three genres */
    uint8_t trap, ts;                                   /* the trap family (gen_trap.c) and its style */
    int32_t mlo, mhi;                                   /* (trap) the melody's window, its sound's */
    uint8_t th[48];                                     /* (trap) the triplet hats, two bars of 16T */
} gs;
static ggenre_t gmut;                                   /* (0.6.95) the genre as this throw plays it (its neighbours' parts) */
static char gmut_name[20];

static void gclear(step_t *s) { memset(s, 0, sizeof *s); s->time = ST_REST; }
static int32_t gchord_deg(uint32_t step) { return gs.prog->deg[(step / gs.csteps) % gs.n]; }
static uint32_t gchord_q(uint32_t step) { return gs.q[(step / gs.csteps) % gs.n]; }
static uint32_t gchord_idx(uint32_t step) { return (step / gs.csteps) % gs.n; }

static void gnote(step_t *s, const int32_t *n, uint32_t cnt, int32_t vel, uint32_t flags)
{
    uint32_t j;
    int32_t top = 0;
    s->n = (uint8_t)cnt;
    for (j = 0; j < 4u; j++) {
        s->note[j] = (uint8_t)(j < cnt ? clamp(n[j], 0, 127) : 0);
        if (j < cnt && n[j] > top) top = n[j];
    }
    s->time = ST_NOTE;
    s->flags = (uint8_t)flags;
    if (top > 74) vel -= (top - 74) * 2 < 30 ? (top - 74) * 2 : 30;   /* (high notes measured: softer) */
    s->vel = (uint8_t)clamp(vel, 30, 127);
}
/* the scale's note d steps from the tonic at or above lo (the degree d of the key) */
static int32_t gdeg(const track_t *t, int32_t lo, int32_t d) { return scale_steps(t, dtonic(t, lo), d); }
/* a chord's notes (cnt) from its root degree and quality, from the tonic at lo, in scale steps (the DOM flag: the
 * third raised to a leading tone when it is minor) */
static uint32_t gchord_notes(const track_t *t, int32_t lo, int32_t deg, uint32_t q, int32_t *n)
{
    uint32_t j, cnt = 0, qq = q & 0x7Fu;
    if (qq == Q_POWER) {
        n[0] = gdeg(t, lo, deg); n[1] = n[0] + 7; n[2] = n[0] + 12;
        return 3;
    }
    for (j = 0; j < 4u && QSTACK[qq][j] != 127; j++) n[cnt++] = gdeg(t, lo, deg + QSTACK[qq][j]);
    if ((q & CQ_DOM) && cnt > 1u && n[1] - n[0] == 3) n[1]++;
    return cnt;
}
/* voiced: inside [lo, hi], led to the nearest of the last voicing (prev, np), or in parallel (plane: the root at the
 * bottom, the register nearest the last). Sorted low to high, no unisons */
static uint32_t gvoice(int32_t *n, uint32_t cnt, int32_t lo, int32_t hi, int32_t *prev, uint32_t *np, int plane)
{
    int32_t best[4], c[4], bcost = 0x7FFFFFFF;
    uint32_t j, i, inv, ninv = plane ? 1u : cnt;
    for (j = 0; j < cnt; j++) {                         /* each into [lo, lo + 12) */
        while (n[j] < lo) n[j] += 12;
        while (n[j] >= lo + 12) n[j] -= 12;
    }
    for (i = 1; i < cnt; i++)                           /* low to high */
        for (j = i; j > 0 && n[j] < n[j - 1]; j--) { int32_t x = n[j]; n[j] = n[j - 1]; n[j - 1] = x; }
    for (inv = 0; inv < ninv; inv++) {
        int32_t sh;
        for (sh = -12; sh <= 12; sh += 12) {
            int32_t cost = 0, top, bot;
            for (j = 0; j < cnt; j++) c[j] = n[(j + inv) % cnt] + (j + inv >= cnt ? 12 : 0) + sh;
            bot = c[0]; top = c[cnt - 1u];
            if (bot < lo || top > hi) continue;
            if (*np && !plane) {
                for (j = 0; j < cnt; j++) {             /* each voice to the nearest old one */
                    int32_t d = 127, k;
                    for (k = 0; k < (int32_t)*np; k++) { int32_t e = c[j] - prev[k]; if (e < 0) e = -e; if (e < d) d = e; }
                    cost += d;
                }
            } else if (*np) {
                cost = c[0] - prev[0]; if (cost < 0) cost = -cost;   /* (parallel: the bottom nearest) */
            } else {
                cost = (c[0] + top) / 2 - (lo + hi) / 2; if (cost < 0) cost = -cost;   /* (the first: in the middle) */
            }
            if (top - bot > 19) cost += 6;              /* (a tight hand) */
            if (cost < bcost) { bcost = cost; for (j = 0; j < cnt; j++) best[j] = c[j]; }
        }
    }
    if (bcost == 0x7FFFFFFF) for (j = 0; j < cnt; j++) best[j] = n[j];
    for (j = 1; j < cnt; j++) if (best[j] <= best[j - 1]) best[j] = best[j - 1] + (best[j] == best[j - 1] ? 12 : 0);
    while (cnt && best[cnt - 1u] > hi && best[0] - 12 >= lo) for (j = 0; j < cnt; j++) best[j] -= 12;   /* (0.6.95: a power
                                                         * chord's octave pushed over the top: the whole hand down) */
    for (j = 0; j < cnt; j++) { n[j] = best[j]; prev[j] = best[j]; }
    *np = cnt;
    return cnt;
}
/* the chord at step i, voiced for a track: cnt notes */
static uint32_t gchord_at(const track_t *t, uint32_t i, int32_t lo, int32_t hi, int32_t *n, int32_t *prev, uint32_t *np)
{
    uint32_t cnt = gchord_notes(t, 36, gchord_deg(i), gchord_q(i), n);
    return gvoice(n, cnt, lo, hi, prev, np, gs.vm == VM_PLANE);
}
static int gchord_tone(int32_t deg, int32_t cdeg)        /* a degree of the key on the chord (root, third, fifth, 7th) */
{
    int32_t d = ((deg - cdeg) % (int32_t)gs.nsc + (int32_t)gs.nsc) % (int32_t)gs.nsc;
    return d == 0 || d == 2 || d == 4 || (d == 6 && gs.complex > 40u);
}
static int32_t gnear_ct(int32_t deg, int32_t cdeg)        /* the chord tone nearest a degree */
{
    int32_t best = deg, k;
    for (k = -4; k <= 4; k++)
        if (gchord_tone(deg + k, cdeg) && (k < 0 ? -k : k) < (best - deg < 0 ? deg - best : best - deg)) best = deg + k;
    return gchord_tone(deg, cdeg) ? deg : best;
}

/* -------------------------------------------------------------------------------------------------- drums --- */
static uint32_t glevel(char c) { return c == 'X' ? 255u : c >= '1' && c <= '9' ? (uint32_t)(c - '0') * 26u : 0u; }
static void gdrum_clear_region(track_t *t, uint32_t from, uint32_t to, uint32_t lanes)
{
    uint32_t i;
    for (i = from; i < to; i++) {
        step_t *s = &t->step[i];
        s->hit &= (uint8_t)~lanes; s->acc &= s->hit;
        if (!s->hit) gclear(s);
    }
}
static void gdrum_hit(track_t *t, uint32_t i, uint32_t lane, int32_t vel, int acc)
{
    step_t *s = &t->step[i];
    if (s->time != ST_NOTE || !s->hit) { gclear(s); s->time = ST_NOTE; s->vel = (uint8_t)clamp(vel, 30, 127); }
    else if (!acc) s->vel = (uint8_t)clamp((s->vel + vel) / 2, 30, 127);
    s->hit |= (uint8_t)(1u << lane);
    if (acc) s->acc |= (uint8_t)(1u << lane); else s->acc &= (uint8_t)~(1u << lane);
}
static void gen_drums_idm(track_t *t, uint32_t len)
{
    uint32_t i, kb = (uint32_t)grr(3, 6), ks = (uint32_t)grr(2, 4), kh = (uint32_t)grr(7, 13), rot = (uint32_t)grr(0, 7);
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t h = 0, b = i / 16u;
        if (euclid(i, kb, 16, rot + (b == 3u ? 2u : 0u))) h |= 1u;
        if (euclid(i, ks, 16, rot + 4u) && !(h & 1u)) h |= 1u << (gch(50) ? 1 : 6);
        if (euclid(i, kh + (b & 1u) * 2u - (b == 2u ? 3u : 0u), 16, b) && gch(40 + gs.amt[GP_DRUMS] / 2u)) h |= 1u << (gch(20) ? 4 : 3);
        if (gch(gs.complex / 10u)) h |= 1u << (5u + gr() % 3u);
        if (!h) continue;
        s->time = ST_NOTE; s->hit = (uint8_t)h; s->acc = (uint8_t)(i % 4u == 0u ? h & 1u : 0u);
        s->vel = (uint8_t)(h & 1u ? 104 : grr(70, 100));
        if ((h & 8u) && gch(25)) step_set_ratchet(s, (uint32_t)grr(2, 4));
        if (!(h & 1u) && gch(30)) step_set_chance(s, (uint32_t)grr(45, 85));
    }
}
static void gen_drums(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    const char *map[8];
    static const int8_t OFF[8] = {40, 20, 20, 0, 0, -20, -20, -20};
    uint32_t a = gs.amt[GP_DRUMS], dens = (uint32_t)clamp(40 + (int32_t)a * 2 + ((int32_t)gs.energy - 50) / 2, 20, 255), i, l, bars = len / 16u ? len / 16u : 1u;
    /* (0.6.93) ENERGY: the density and the velocities with it, the hats sixteenths above 66, the percussion thinned under 34 */
    uint32_t chaos = 24u + gs.complex * 48u / 100u, ng = (g->ghosts * a + 50u) / 100u + (gs.complex > 60u);
    int trap = g->kick_mode == KM_808;                  /* (trap, drill, rage: steady hats with rolls, the kick for the 808) */
    const uint8_t *cell = HAT_CELL[g->hats];
    gseed(GS_DRUMS);
    memset(gs.kick, 0, sizeof gs.kick);
    memset(gs.snare, 0, sizeof gs.snare);
    for (l = 0; l < 8u; l++) map[l] = DRUM_MAP[g->drum][l];
    if (trap) {                                         /* only straight eighths or sixteenths from the pool (drill keeps
                                                         * its 3-3-2); its open hats, percussion and snares are its own */
        static const uint8_t TRAP_HATS[4] = {0, 1, 3, 7};
        if (g->drum != DM_UK_DRILL && gch(45)) map[3] = HAT_POOL[TRAP_HATS[gr() % 4u]];
        ng = 0;
    } else if (g->drum != DM_IDM && g->drum != DM_NONE && g->drum != DM_SPARSE) {
        if (g->drum != DM_UK_DRILL && gch(45)) map[3] = HAT_POOL[gr() % 8u];
        if (gch(50)) map[4] = OH_POOL[gr() % 6u];
        if (gch(45)) map[6] = RS_POOL[gr() % 6u];
        if (g->drum != DM_PHONK && gch(25)) map[7] = CB_POOL[gr() % 4u];
        if (gch(40)) map[5] = TM_POOL[gr() % 4u];
        if (map[1][0] && map[2][0] && gch(35)) { const char *x = map[1]; map[1] = map[2]; map[2] = x; }
    }
    if (g->drum == DM_IDM) {
        gen_drums_idm(t, len);
        for (i = 0; i < len; i++) { gs.kick[i] = (uint8_t)(t->step[i].hit & 1u); gs.snare[i] = (uint8_t)((t->step[i].hit >> 1) & 1u); }
        return;
    }
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t b = i / 16u, p = i % 16u, h = 0, acc = 0, hat_only = 1, lv_sum = 0, lv_n = 0, ghost_lv = 0;
        for (l = 0; l < 8u; l++) {
            uint32_t lv = map[l][0] ? glevel(map[l][p]) : 0u, lim;
            int32_t d;
            if (!lv) {
                /* Per-seed syncopated kick / perc injection on offbeat 16ths */
                if (l == 0u && p && (p == 3u || p == 6u || p == 7u || p == 10u || p == 11u || p == 14u) &&
                    (!map[1][0] || glevel(map[1][p]) < 180u) && gch(10 + gs.complex / 6u))
                    lv = 150u;
                else continue;
            }
            if (lv < 230u && !(trap && l == 3u)) {      /* Signed perturbation on every bar so each seed roll changes the groove */
                int32_t k = (int32_t)chaos + (b & 1u ? 18 : 0);
                lv = (uint32_t)clamp((int32_t)lv + grr(-k, k / 2), 0, 229);
            }
            d = (int32_t)dens + OFF[l] - 36;
            lim = (uint32_t)clamp(d, 0, 255);
            if (lv <= 255u - lim) continue;
            if (l >= 3u && lv < 230u && !(trap && l == 3u) && gr() % 100u >= 40u + a * 50u / 100u) continue;
            h |= 1u << l;
            if (lv >= 230u) acc |= 1u << l;
            else { lv_sum += lv; lv_n++; }
            if (l < 3u || l > 4u) hat_only = 0;
            if (lv < 90u && l < 3u) ghost_lv = lv;
        }
        if (!h) continue;
        s->time = ST_NOTE; s->hit = (uint8_t)h; s->acc = (uint8_t)(acc & h);
        {
            int32_t vel = lv_n ? 56 + (int32_t)(lv_sum / lv_n) * 58 / 255 : 100;
            if (ghost_lv) vel = 36 + (int32_t)ghost_lv * 20 / 90;
            if (hat_only) vel = vel * cell[p % 4u] / 100;
            if (!(acc & h)) vel = vel * (88 + (int32_t)gs.energy / 4) / 100;
            if (gs.human) vel += grr(-(int32_t)gs.human / 12, (int32_t)gs.human / 12);
            s->vel = (uint8_t)clamp(vel, 30, 127);
        }
    }
    if (gs.energy > 66u && map[3][0]) {                 /* the hats driven: the empty sixteenth after a closed hat, softly */
        uint8_t ch[NSTEP];
        for (i = 0; i < len; i++) ch[i] = (uint8_t)(t->step[i].time == ST_NOTE && (t->step[i].hit & 8u));
        for (i = 1; i < len; i++) {
            step_t *s = &t->step[i];
            if ((s->time == ST_NOTE && (s->hit & 0x18u)) || !ch[i - 1u] || !gch(55 + gs.energy / 4u)) continue;
            gdrum_hit(t, i, 3, 44 + (int32_t)gs.energy / 5, 0);
        }
    } else if (gs.energy < 34u) {                       /* the room: half the open hats and the percussion gone */
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            if (s->time != ST_NOTE || !(s->hit & 0xD0u) || !gch(50)) continue;
            s->hit &= (uint8_t)~0xD0u; s->acc &= s->hit;
            if (!s->hit) gclear(s);
        }
    }
    for (i = 0; i < len && ng; i++) {                   /* the snare's ghosts: the best positions first, a bar at a time */
        uint32_t b = i / 16u, k, placed = 0;
        if (i % 16u) continue;
        for (k = 0; k < 12u && placed < ng; k++) {
            uint32_t p = GHOST_RANK[k], j = b * 16u + p;
            step_t *s = &t->step[j];
            if (j >= len || (s->time == ST_NOTE && (s->hit & 6u)) || !gch(55 + gs.complex / 3u)) continue;
            if (p && t->step[j - 1u].time == ST_NOTE && (t->step[j - 1u].hit & 2u) && (t->step[j - 1u].acc & 2u)) continue;
            gdrum_hit(t, j, 1, grr(34, 50), 0);
            placed++;
        }
    }
    if (gs.complex > 20u) {                             /* a variation on the second and fourth bars: a kick added on a
                                                         * weak step, a snare dragged, a perc moved */
        for (i = 16; i < len; i++) {
            uint32_t b = i / 16u, p = i % 16u;
            step_t *s = &t->step[i];
            if (!(b & 1u) || p % 2u == 0u) continue;
            if (s->time != ST_NOTE && gch(8 + gs.complex / 8u) && (p == 7u || p == 10u || p == 11u || p == 14u) && g->kick_mode != KM_ANSWER) {
                gdrum_hit(t, i, 0, 80, 0);
            } else if (s->time == ST_NOTE && (s->hit & 0xC0u) && gch(gs.complex / 4u)) {
                s->hit ^= (uint8_t)(s->hit & 0xC0u) ^ (uint8_t)(((s->hit & 0x40u) << 1) | ((s->hit & 0x80u) >> 1));
                s->acc &= s->hit;
            }
        }
    }
    {   /* the fill: the phrase's last bar (and half of one at the fourth bar of eight); the genre's kind */
        uint32_t fl = gs.drum_fill ? gs.drum_fill : g->fills, last = bars - 1u, base = last * 16u, k;
        int32_t e = (int32_t)gs.energy;
        if (bars >= 2u && fl != FL_NONE && (bars >= 4u || gs.drum_fill || gch(50))) {
            uint32_t lane = map[1][0] ? 1u : map[2][0] ? 2u : 6u;
            switch (fl) {
            case FL_ROLL:
                gdrum_clear_region(t, base + 12u, base + 16u, 0x10u);
                for (k = 0; k < 4u; k++) {
                    gdrum_hit(t, base + 12u + k, lane, 70 + (int32_t)k * 15, k == 3u);
                    if (e > 55 && k >= 2u) step_set_ratchet(&t->step[base + 12u + k], 2);
                }
                if (bars >= 8u) { gdrum_clear_region(t, base + 8u, base + 12u, 0x10u); for (k = 0; k < 4u; k++) gdrum_hit(t, base + 8u + k, lane, 50 + (int32_t)k * 5, 0); }
                break;
            case FL_TOM: {
                static const uint8_t TM[8] = {1, 0, 1, 1, 0, 1, 0, 1};
                gdrum_clear_region(t, base + 8u, base + 16u, 0x18u);
                for (k = 0; k < 8u; k++) if (TM[k]) gdrum_hit(t, base + 8u + k, 5, 88 + (int32_t)k * 3, k == 0u || k == 5u);
                if (bars >= 4u && len >= 16u) gdrum_hit(t, 0, 4, 110, 1);   /* (a crash on the one) */
                break;
            }
            case FL_DROP:
                gdrum_clear_region(t, base + 8u, base + 16u, 0x01u);
                gdrum_hit(t, base + 15u, 4, 100, 1);
                break;
            case FL_HAT:
                for (k = 12; k < 16u; k++) { gdrum_hit(t, base + k, 3, 70 + (int32_t)(k - 12u) * 12, 0); step_set_ratchet(&t->step[base + k], k < 14u ? 2u : 3u); }
                break;
            case FL_LINEAR: {
                static const uint8_t BD[8] = {1, 0, 1, 0, 1, 0, 0, 1}, SD[8] = {0, 1, 0, 1, 0, 1, 1, 0};
                gdrum_clear_region(t, base + 8u, base + 16u, 0x1Fu);
                for (k = 0; k < 8u; k++) {
                    if (BD[k]) gdrum_hit(t, base + 8u + k, 0, 100, k == 0u);
                    if (SD[k]) gdrum_hit(t, base + 8u + k, 1, 84 + (int32_t)k * 4, k == 6u);
                }
                break;
            }
            case FL_KICK16:
                for (k = 12; k < 16u; k++) gdrum_hit(t, base + k, 0, 96, 1);
                break;
            case FL_DUB:
                gdrum_clear_region(t, base + 12u, base + 16u, 0x46u);
                gdrum_hit(t, base + 12u, 6, 110, 1);
                break;
            default:                                    /* FL_BREAK: the last beat silent, then the one */
                gdrum_clear_region(t, base + 12u, base + 16u, 0xFFu);
                if (len >= 16u) gdrum_hit(t, 0, 4, 110, 1);
                break;
            }
        }
        if (bars >= 8u && fl != FL_NONE && g->kick_mode != KM_DOUBLE && gch(30 + gs.complex / 3u))
            gdrum_clear_region(t, (bars - 2u) * 16u + 12u, (bars - 2u) * 16u + 16u, 0x01u);   /* (a breath before the fill) */
    }
    if (g->drum == DM_TRAP || g->drum == DM_TRAP_ATL || g->drum == DM_UK_DRILL ||
        g->drum == DM_PHONK || g->drum == DM_RAGE || g->drum == DM_DUBSTEP || g->drum == DM_UKG) {   /* (0.6.93) the hats' rolls: a step before the
                                                         * snare and the bar's end, sixteenth or triplet rolls */
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            uint32_t p = i % 16u;
            if (s->time != ST_NOTE || !(s->hit & 8u) || (s->hit & 3u)) continue;
            if ((p == 6u || p == 7u || p == 10u || p == 14u || p == 15u) && gch(45 + gs.complex / 2u))
                step_set_ratchet(s, (uint32_t)grr(2, p >= 14u ? 4 : 3));
        }
    }
    for (i = 0; i < len; i++) {                         /* the snare laid back where the genre plays late; the kick's steps */
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE) continue;
        gs.kick[i] = (uint8_t)(s->hit & 1u);
        gs.snare[i] = (uint8_t)((s->hit >> 1) & 1u);
        if (g->sd_micro && gs.human && (s->hit & 2u) && !(s->hit & 1u)) ST_MICRO_SET(s, g->sd_micro);
        if (s->acc & ~s->hit) s->acc &= s->hit;
    }
}

/* --------------------------------------------------------------------------------------------------- bass --- */
static int32_t gbass_root(const track_t *t, int32_t lo, int32_t deg)   /* a chord's root as the bass plays it */
{
    int32_t n = gdeg(t, lo, deg);
    while (n > lo + 11) n -= 12;
    return n;
}
static void gen_acid(track_t *t, uint32_t len, int32_t lo)
{
    static const uint8_t W[6] = {55, 15, 10, 10, 5, 5};   /* R 8 b7 5 b3 2 */
    static const int8_t D[6] = {0, 0, 6, 4, 2, 1};
    uint8_t pt[16], on[16];
    uint32_t i, b, bars = len / 16u ? len / 16u : 1u;
    for (i = 0; i < 16u; i++) {
        on[i] = (uint8_t)(i % 4u == 0u || gch(45 + gs.complex / 3u));
        pt[i] = (uint8_t)gpickw(W, 6);
    }
    for (b = 0; b < bars; b++) {
        uint32_t na = (uint32_t)grr(3, 5), ns = (uint32_t)grr(2, 3), k, tries;
        uint8_t acc[16] = {0}, sl[16] = {0};
        for (tries = 0, k = 0; k < na && tries < 32u; tries++) { uint32_t p = gr() % 16u; if (on[p] && !acc[p]) { acc[p] = 1; k++; } }
        for (tries = 0, k = 0; k < ns && tries < 32u; tries++) {
            uint32_t p = gr() % 15u;
            if (on[p] && on[p + 1u] && !sl[p]) { sl[p] = 1; k++; if (pt[p + 1u] == 0u && gch(50)) pt[p + 1u] = 1; }
        }
        for (i = 0; i < 16u; i++) {
            uint32_t s = b * 16u + i;
            int32_t n[1], cd;
            if (s >= len || !on[i]) continue;
            cd = gchord_deg(s);
            n[0] = gbass_root(t, lo, cd) + (pt[i] == 1u ? 12 : 0);
            if (pt[i] >= 2u) n[0] = scale_steps(t, gbass_root(t, lo, cd), D[pt[i]]);
            gnote(&t->step[s], n, 1, acc[i] ? 112 : 90, (acc[i] ? SF_ACCENT : 0u) | (sl[i] ? SF_SLIDE : 0u));
        }
    }
}
/* the 808 (trap, drill, rage): a long note on every kick, held to the next kick (or the next chord); its pitch the
 * archetype's at that step (the root, the octave, the fifth ..), slides between notes of different pitch, drill the most */
static int gen_808(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    const barch_t *a = &BASS_ARCH[gs.ba];
    int32_t lo = g->bass_lo, prev = -1;
    uint32_t i, alen = 16u * a->bars, kicks = 0, start = 0;
    uint32_t slide = g->drum == DM_UK_DRILL ? 55u : g->drum == DM_RAGE ? 20u : 32u;
    uint32_t jump = g->drum == DM_UK_DRILL ? 30u : 15u;
    for (i = 0; i < len; i++) kicks += gs.kick[i];
    if (kicks < 2u) return 0;                           /* (no drums: the archetype as written) */
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        int32_t cd = gchord_deg(i), root = gbass_root(t, lo, cd), n[1], vel = 104;
        uint32_t fl = 0;
        char c = a->n[i % alen];
        if (!gs.kick[i] && (i % gs.csteps || prev < 0)) {
            if (prev >= 0) dstep_tie(s);
            continue;
        }
        switch (c) {
        case '8': n[0] = root + 12; break;
        case '5': n[0] = scale_steps(t, root, 4); break;
        case '7': n[0] = scale_steps(t, root, 6); break;
        case '3': n[0] = scale_steps(t, root, 2); break;
        case '2': n[0] = scale_steps(t, root, 1); break;
        default: n[0] = root; break;
        }
        if (i % 16u && n[0] == root && gch(jump)) n[0] = gch(60) ? root + 12 : scale_steps(t, root, 4);
        if (n[0] > lo + 26) n[0] -= 12;
        if (n[0] < lo - 7) n[0] += 12;
        if (i % 16u == 0u) { vel = 112; fl |= SF_ACCENT; }
        if (gs.human) vel += grr(-(int32_t)gs.human / 12, (int32_t)gs.human / 12);
        if (prev >= 0 && n[0] != prev && gch(slide)) t->step[start].flags |= SF_SLIDE;
        gnote(s, n, 1, vel, fl);
        prev = n[0];
        start = i;
    }
    return 1;
}
static void gen_bass(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    const barch_t *a = &BASS_ARCH[gs.ba];
    int32_t lo = g->bass_lo, prev = -1;
    uint32_t i, alen = 16u * a->bars, held = 0;
    gseed(GS_BASS);
    if (gs.ba == BA_ACID) { gen_acid(t, len, lo); return; }
    if (g->kick_mode == KM_808 && gen_808(t, len)) return;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        char c = a->n[i % alen], f = a->f ? a->f[i % alen] : '.';
        int32_t cd = gchord_deg(i), nd = gchord_deg(i + gs.csteps - i % gs.csteps), root, n[1], vel = 96;
        uint32_t fl = 0;
        if (c == '.') { held = 0; continue; }
        if (c == '=') { if (held) dstep_tie(s); continue; }
        if (gs.kick[i] && i % 16u && (g->kick_mode == KM_ANSWER || g->kick_mode == KM_INTERLOCK) && c != 'x' && c != 'r' &&
            i % gs.csteps) { held = 0; continue; }     /* (the kick's step is the kick's) */
        root = gbass_root(t, lo, cd);
        switch (c) {
        case '2': n[0] = scale_steps(t, root, 1); break;
        case '3': n[0] = scale_steps(t, root, 2); break;
        case '5': n[0] = scale_steps(t, root, 4); break;
        case '6': n[0] = scale_steps(t, root, 5); break;
        case '7': n[0] = scale_steps(t, root, 6); break;
        case '9': n[0] = scale_steps(t, root, 8); break;
        case 'L': n[0] = scale_steps(t, root, 4) - 12; break;
        case '8': n[0] = root + 12; break;
        case 'A': n[0] = nd == cd ? scale_steps(t, root, 4) : gbass_root(t, lo, nd) - 1; break;
        case 'B': n[0] = nd == cd ? scale_steps(t, root, 4) : gbass_root(t, lo, nd) + 1; break;
        case 'N': n[0] = gbass_root(t, lo, nd); break;
        case 'S': { int32_t r2 = gbass_root(t, lo, nd); n[0] = nd == cd ? scale_steps(t, root, 1) : r2 > root ? scale_steps(t, r2, -1) : scale_steps(t, r2, 1); break; }
        default: n[0] = root; break;                    /* R r x */
        }
        if (n[0] > lo + 26) n[0] -= 12;                /* (two octaves from the root's: the octave jumps live) */
        if (n[0] < lo - 7) n[0] += 12;
        if (c == 'r') vel = 70;
        if (c == 'x') vel = 42;
        if (f == 'g') vel = 50;
        if (f == 'a') { vel = 110; fl |= SF_ACCENT; }
        if (f == 's') fl |= SF_SLIDE;
        if (gs.human) vel += grr(-(int32_t)gs.human / 10, (int32_t)gs.human / 10);
        gnote(s, n, 1, vel, fl);
        held = 1;
        prev = n[0];
    }
    (void)prev;
    if (g->kick_mode == KM_DOUBLE || g->kick_mode == KM_INTERLOCK) {   /* the kick doubled (DOUBLE: on its steps when the
                                                         * bass rests), interlocked (the step after it) */
        for (i = 0; i < len; i++) {
            uint32_t j = g->kick_mode == KM_DOUBLE ? i : i + 1u;
            int32_t n[1];
            if (!gs.kick[i] || j >= len || t->step[j].time != ST_REST || dsounding(t, j) >= 0) continue;
            if (!gch(g->kick_mode == KM_DOUBLE ? 30 + gs.amt[GP_BASS] / 4u : 35)) continue;
            n[0] = gbass_root(t, lo, gchord_deg(j));
            gnote(&t->step[j], n, 1, g->kick_mode == KM_DOUBLE ? 92 : 72, 0);
        }
    }
    if (len >= 64u && gs.complex > 30u && a->bars < 4u) {   /* the fourth bar's pickup: an approach into the next root */
        uint32_t j = 62;
        int32_t n[1], cd = gchord_deg(63), nd = gchord_deg(0);
        if (t->step[j].time == ST_REST && t->step[j + 1u].time == ST_REST && gch(60)) {
            n[0] = nd == cd ? scale_steps(t, gbass_root(t, lo, cd), 4) : gbass_root(t, lo, nd) - 1;
            gnote(&t->step[j], n, 1, 84, 0);
        }
    }
    if (gs.amt[GP_BASS] < 60u) {                        /* a thinner bass: the weak notes thinned */
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            if (s->time == ST_NOTE && i % 4u && !(s->flags & SF_ACCENT) && gr() % 100u < (60u - gs.amt[GP_BASS])) {
                gclear(s);
                if (i + 1u < len && t->step[i + 1u].time == ST_TIE) gclear(&t->step[i + 1u]);
            }
        }
    }
}

/* -------------------------------------------------------------------------------------------------- chords --- */
static void gen_pads(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    uint32_t i, a = gs.amt[GP_CHORDS], last = 0xFFFFu, cnt = 0, style = g->pads;
    int32_t n[4];
    uint8_t dub[16] = {0}, comp[16] = {0}, brk[16] = {0};
    gseed(GS_CHORDS);
    gs.nvo = 0;
    {   /* the bar figures kept the loop through */
        static const uint8_t AT[6] = {6, 10, 14, 7, 11, 13}, CP[6] = {2, 6, 7, 11, 14, 15}, BR[6] = {0, 3, 6, 10, 11, 14};
        uint32_t m = (uint32_t)grr(1, 2) + (gs.energy > 66u), j;
        dub[2] = 1;
        for (j = 0; j < m; j++) dub[AT[gr() % 6u]] = 1;
        for (j = 0, m = (uint32_t)grr(3, 5); j < m; j++) comp[CP[(gr() + j * 2u) % 6u]] = 1;
        comp[gch(50) ? 6u : 14u] = 1;                   /* (the anticipation of 3 or of the bar) */
        for (j = 0, m = (uint32_t)grr(3, 4); j < m; j++) brk[BR[gr() % 6u]] = 1;
    }
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t pp = i % gs.csteps, p = i % 16u, b = i / 16u;
        int on = 0, vel = 80, hold = 0;
        switch (style) {
        case CS_HOLD: on = pp == 0u; hold = (int)gs.csteps - 1; break;
        case CS_SWELL: on = pp == 0u; hold = (int)gs.csteps - 1; vel = 72; break;
        case CS_STAB: on = (p % 4u == 2u && gch(40 + a / 2u)) || (pp == 0u && gch(50)); vel = p % 4u == 2u ? 84 : 92; break;
        case CS_PULSE: on = i % (gs.energy > 66u ? 2u : 4u) == 0u && !(p == 12u && gch(40)); hold = gch(40); vel = p % 4u ? 76 : 86; break;
        case CS_DUB: on = dub[p]; vel = p == 2u ? 92 : 78; break;
        case CS_SKANK: on = p % 4u == 2u; vel = 84; break;
        case CS_COMP: on = (comp[p] && gch(70 + a / 4u)) || (p == 0u && gch(40)) || (!comp[p] && (p == 6u || p == 10u || p == 13u) && gch(a / 4u));
            hold = p == 14u || p == 7u; vel = p == 0u ? 84 : 80; break;   /* (0.6.93: the bar's figure, varied a bar) */
        case CS_POWER: on = p == 0u || (p == 8u && (b & 1u) && gch(60)); hold = p == 8u ? 5 : 7; vel = 96; break;   /* (0.6.93: a push every bar) */
        case CS_DRONE: on = i == 0u; hold = (int)len - 1; vel = 70; break;
        case CS_SPARSE: on = pp == 0u && (i == 0u || gch(40 + a / 3u)); hold = (int)gs.csteps - 1; vel = 76; break;
        case CS_BROKEN: on = brk[p]; vel = p ? 80 : 92; break;
        default: on = ((0xDB6Du >> p) & 1u) && gch(70 + a / 4u); vel = p % 4u ? 78 : 96; break;   /* CS_TRANCE */
        }
        if (!on || !gch(style == CS_HOLD || style == CS_SWELL || style == CS_DRONE || style == CS_SPARSE ? 100 : 70 + a * 3u / 10u)) continue;
        if (gchord_idx(i) != last || !cnt || style == CS_HOLD || style == CS_SWELL || style == CS_SPARSE) {
            cnt = gchord_at(t, i, 48, 79, n, gs.vo, &gs.nvo);
            last = gchord_idx(i);
        } else {
            uint32_t j;
            for (j = 0; j < gs.nvo; j++) n[j] = gs.vo[j];
            cnt = gs.nvo;
        }
        if (gs.human) vel += grr(-(int32_t)gs.human / 12, (int32_t)gs.human / 12);
        gnote(s, n, cnt, vel, 0);
        if (hold > 0) dhold(t, i, (uint32_t)hold, len);
    }
}

/* ----------------------------------------------------------------------------------------------------- arp --- */
static void gen_arp(track_t *t, uint32_t len)
{
    const garp_t *ap = &ARP[gs.g->arp[gr() % 2u]];
    uint32_t i, k = 0, idx = 0, a = gs.amt[GP_ARP], no = 0, last = 0xFFFFu, cnt = 0, rate;
    int32_t c[4], n[1];
    gseed(GS_ARP);
    ap = &ARP[gs.g->arp[gch(70) ? 0 : 1]];
    rate = ap->rate;
    if (rate == 1u && (a < 35u || (gs.energy < 34u && gch(50)))) rate = 2;   /* (0.6.93) a small amount: eighths */
    gs.navo = 0;
    for (no = 0; no < 8u && ap->ord[no] >= 0; no++) {}
    for (i = 0; i < len; i++) {
        uint32_t j;
        int32_t o;
        if (i % rate) continue;
        idx = i / rate;
        if (!((ap->mask >> (idx % 16u)) & 1u)) { k++; continue; }
        if (gr() % 100u >= 20u + a * 3u / 4u) { k++; continue; }   /* (holes: space; 0.6.93: the amount is the density) */
        if (gchord_idx(i) != last || !cnt) {
            cnt = gchord_at(t, i, 48, 72, c, gs.avo, &gs.navo);
            last = gchord_idx(i);
        } else {
            for (j = 0; j < gs.navo; j++) c[j] = gs.avo[j];
            cnt = gs.navo;
        }
        o = ap->ord[k % no];
        j = (uint32_t)(o & 3) % cnt;
        n[0] = c[j] + (o >= 4 ? 12 : 0);
        while (n[0] > 79) n[0] -= 12;
        while (n[0] < 48) n[0] += 12;
        gnote(&t->step[i], n, 1, idx % 4u == 0u ? 92 : 78, 0);
        if (gs.complex > 50u && gch(6)) step_set_ratchet(&t->step[i], 2);
        k++;
    }
    t->p[P_SGATE] = (int16_t)clamp((int32_t)ap->gate + grr(-8, 8), 10, 127);
}

/* ---------------------------------------------------------------------------------------------------- lead --- */
static const uint8_t TONAL_W[7] = {255, 96, 128, 64, 192, 96, 128};   /* Marbles' weights: the tonic, its fifth, third .. */
static const uint8_t PROX_W[5] = {80, 255, 180, 90, 40};               /* |step| 0..4 of the scale */
static int32_t gl_prev;                                  /* (0.7.0) the line's last note: the next stays in its octave */
static int32_t glead_note(const track_t *t, int32_t deg)
{
    int32_t n = gdeg(t, 55, deg);
    while (n > 79) n -= 12;
    while (n < 55) n += 12;
    if (gl_prev) {                                      /* (0.7.0) a leap past a sixth (the motif moved with the chord,
                                                         * the window's edge): the octave nearer the last note */
        if (n - gl_prev > 9 && n - 12 >= 55) n -= 12;
        else if (gl_prev - n > 9 && n + 12 <= 79) n += 12;
    }
    gl_prev = n;
    return n;
}
/* the next degree of a line: near the last, the key's weights, a chord tone on a strong step three times in four */
static int32_t glead_next(int32_t prev, int32_t cdeg, int strong, int32_t lo, int32_t hi)
{
    uint8_t w[9];
    int32_t k;
    for (k = -4; k <= 4; k++) {
        int32_t d = prev + k, cls = ((d % (int32_t)gs.nsc) + (int32_t)gs.nsc) % (int32_t)gs.nsc;
        uint32_t ww = (uint32_t)PROX_W[k < 0 ? -k : k] * TONAL_W[cls % 7u] / 255u;
        if (strong) ww = gchord_tone(d, cdeg) ? ww * 3u : ww / 3u;
        if (d < lo || d > hi) ww = 0;
        w[k + 4] = (uint8_t)(ww > 255u ? 255u : ww);
    }
    return prev + (int32_t)gpickw(w, 9) - 4;
}
static void gen_lead(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    uint32_t i, j, a = gs.amt[GP_LEAD], bars = len / 16u ? len / 16u : 1u, style = g->lead;
    int32_t n[1];
    gseed(GS_LEAD);
    gl_prev = 0;
    if (style == LD_NONE) return;
    if (style == LD_OSTINATO) {
        int8_t cl[4];
        uint32_t k = (uint32_t)grr(3, 4) + (gs.energy > 66u), rot = (uint32_t)grr(0, 3), nc = (uint32_t)grr(2, 4), q = 0;
        int32_t d = 0;
        for (j = 0; j < nc; j++) { cl[j] = (int8_t)d; d = glead_next(d, gchord_deg(0), 1, -3, 7); }
        for (i = 0; i < len; i++) {
            uint32_t b = i / 16u;
            if (!euclid(i % 16u, k, 16, rot)) continue;
            if (b % 4u == 3u && gch(50)) continue;      /* (the fourth bar: a breath) */
            if (gr() % 100u >= 50u + a / 2u) continue;
            n[0] = glead_note(t, cl[q % nc] + (b % 4u == 2u && q % nc == nc - 1u ? 1 : 0) + gchord_deg(i) - gchord_deg(0));
            gnote(&t->step[i], n, 1, q % nc == 0u ? 98 : 82, gch(12) ? SF_SLIDE : 0u);
            q++;
        }
        return;
    }
    if (style == LD_MECH) {
        static const int8_t MT[4][8] = {{0, 2, 4, 2, 0, 4, 7, 4}, {4, 4, 2, 0, 2, 2, 0, -1}, {0, 0, 4, 0, 5, 4, 2, 0}, {7, 4, 2, 4, 0, 2, 4, 0}};
        const int8_t *mt = MT[gr() % 4u];
        uint32_t k = (uint32_t)grr(4, 6) + (gs.energy > 66u), rot = (uint32_t)grr(0, 3), idx, q;
        for (i = 0; i < len; i++) {
            if (!euclid(i % 16u, k, 16, rot)) continue;
            for (q = 0, idx = 0; q < i % 16u; q++) idx += euclid(q, k, 16, rot);
            n[0] = glead_note(t, mt[idx % 8u] + gchord_deg(i - i % 16u));
            gnote(&t->step[i], n, 1, 92, 0);
        }
        return;
    }
    if (style == LD_SPARSE || style == LD_TINTI) {      /* long notes: one or two a bar (TINTI: a scale line); (0.6.93)
                                                         * SPARSE: a long note, or a short figure of two or three into it,
                                                         * from the bar's one or its third beat */
        int32_t d = style == LD_TINTI ? grr(2, 5) : 2, dir = gch(50) ? -1 : 1;
        uint32_t fig = style == LD_SPARSE && gch(55), off = style == LD_SPARSE && gch(40) ? 4u : 0u;
        for (i = 0; i < len; i += 8u) {
            uint32_t b = i / 16u, at = i + off, hold;
            if (at >= len) break;
            if (style == LD_SPARSE && ((i % 16u && gr() % 100u >= a) || (b % 2u == 1u && gch(40)))) continue;
            if (style == LD_SPARSE) d = gnear_ct(glead_next(d, gchord_deg(at), 0, -2, 7), gchord_deg(at));
            else { d += dir; if (d < -2 || d > 7) { dir = -dir; d += 2 * dir; } }
            hold = style == LD_TINTI ? 6u : 12u - (gs.energy > 66u ? 4u : 0u) - off;
            if (fig && gch(60)) {                       /* the figure: two short notes stepping into the long one */
                uint32_t q, nf = (uint32_t)grr(1, 2);
                int32_t dd = d - (int32_t)nf * (gch(50) ? 1 : -1);
                for (q = 0; q < nf && at + q * 2u + 2u < len; q++) {
                    n[0] = glead_note(t, dd + (int32_t)q * (dd < d ? 1 : -1));
                    gnote(&t->step[at + q * 2u], n, 1, 76, 0);
                }
                at += nf * 2u;
                hold = hold > nf * 2u ? hold - nf * 2u : 2u;
            }
            if (at >= len) break;
            n[0] = glead_note(t, d);
            gnote(&t->step[at], n, 1, 84, 0);
            dhold(t, at, hold, len);
        }
        return;
    }
    {   /* LD_MOTIF, LD_HOOK: a one-bar motif of k onsets (the strong steps likelier), its contour by the key's weights
         * (a leap filled back), chord tones on strong steps; the phrase over four bars from a template, a variation
         * operator on the varied bars; HOOK: the second bar answers (the same rhythm, the contour turned, ending on a
         * chord tone) */
        static const uint8_t TPL[5][4] = {{0, 0, 0, 1}, {0, 1, 0, 2}, {0, 3, 0, 1}, {0, 0, 3, 1}, {0, 1, 3, 2}};   /* 0 M 1 V1 2 V2 3 rest */
        /* (0.6.97) the motif's rhythm: a cell from a library of figures that hold together (a long note and two short
         * ones, a tresillo, a pickup into the three, a dotted push ..), its onsets thinned to k by complexity */
        static const char *const CELL[12] = {"x..x..x...x.....", "x.x.x...x.x.....", "x...x.x.x.......", "..x...x...x...x.",
                                             "x..x.x..x..x.x..", "x.......x.x.x...", "x...x...x...x...", "x.x...x.x...x...",
                                             "..x.x...x.x.....", "x..x..x..x..x...", "x.....x...x.x...", "x...x.x.......x."};
        uint8_t at[6], used[16] = {0};
        int8_t deg[6], ans[6];
        uint32_t k = 2u + gs.complex / 35u + (a > 60u), tpl = gr() % 5u, op1 = gr() % 5u, op2 = gr() % 5u, nc = 0;
        int32_t d = (int32_t)(gr() % 3u) * 2, prev_mv = 0;
        const char *cell = CELL[gr() % 12u];
        if (k > 5u) k = 5u;
        if (style == LD_HOOK) tpl = 4;
        for (i = 0; i < 16u; i++) if (cell[i] == 'x') { used[i] = 1; nc++; }
        while (nc > k) {                                /* (thinned: a weak onset dropped, never the first) */
            uint32_t p = 1u + gr() % 15u;
            if (used[p] && (p % 4u || gch(30))) { used[p] = 0; nc--; }
        }
        k = nc;
        for (j = 0, i = 0; i < 16u; i++) if (used[i]) at[j++] = (uint8_t)i;
        for (j = 0; j < k; j++) {                       /* the contour */
            if (j) {
                int32_t nd = glead_next(d, gchord_deg(0), at[j] % 4u == 0u, -3, 8), mv = nd - d;
                if (prev_mv >= 3 || prev_mv <= -3) mv = prev_mv > 0 ? -1 : 1;   /* (the leap filled) */
                prev_mv = mv;
                d += mv;
            }
            deg[j] = (int8_t)d;
            ans[j] = (int8_t)(deg[0] - (d - deg[0]));   /* (the answer: the contour turned) */
        }
        if (style == LD_HOOK) ans[k - 1u] = (int8_t)gnear_ct(ans[k - 1u], gchord_deg(16 < len ? 16 : 0));
        for (i = 0; i < bars; i++) {
            uint32_t ph = TPL[tpl][i % 4u], b0 = i * 16u, op = i % 4u == 3u ? op1 : op2, kk = k;
            int32_t ch = gchord_deg(b0), shift = gch(45) ? ch - gchord_deg(0) : 0, endmv = 0, half = (int32_t)gs.nsc / 2;
            const int8_t *dg = deg;
            while (shift > half) shift -= (int32_t)gs.nsc;   /* (0.7.0) the motif follows the chord the nearer way: a
                                                         * fourth up is a fifth down, never an octave's worth of leap */
            while (shift < -half) shift += (int32_t)gs.nsc;
            if (ph == 3u) continue;                     /* (a bar of rest) */
            if (style == LD_HOOK && (i & 1u)) dg = ans;
            if (ph == 1u || ph == 2u) {                 /* the variation: one operator */
                if (op == 0u) kk = k > 2u ? k - 1u : k;  /* TRUNC */
                else if (op == 1u) endmv = gch(50) ? 1 : -1;   /* END moved */
                else if (op == 2u) shift += 2;          /* TRANSP up a third */
                else if (op == 3u) shift -= 1;          /* down a step */
                /* 4: ORN, a passing note before the last */
            }
            for (j = 0; j < kk; j++) {
                int32_t dd = dg[j] + shift + (j == kk - 1u ? endmv : 0);
                uint32_t pos = b0 + at[j], nxt = j + 1u < kk ? at[j + 1u] : 16u + at[0], gap = nxt - at[j];
                if (pos >= len) break;
                if (at[j] % 4u == 0u && gch(65)) dd = gnear_ct(dd, ch);
                if (i % 4u == 3u && j == kk - 1u) dd = gnear_ct(dd, ch);   /* (0.7.0) the phrase ends on a chord tone */
                if (gr() % 100u >= 55u + a / 2u && j && j != kk - 1u) continue;   /* (space with less amount) */
                n[0] = glead_note(t, dd);
                gnote(&t->step[pos], n, 1, at[j] % 4u == 0u ? 96 : 82, gch(8) ? SF_SLIDE : 0u);
                if ((ph == 1u || ph == 2u) && op == 4u && j == kk - 1u && at[j] && t->step[pos - 1u].time != ST_NOTE) {
                    int32_t m[1] = {glead_note(t, dd - 1)};
                    gnote(&t->step[pos - 1u], m, 1, 70, 0);
                    continue;
                }
                if (gap > 2u + (gs.energy < 34u ? 2u : 0u)) dhold(t, pos, gap - 2u - (gs.energy < 34u ? 2u : 0u), len);
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------- texture --- */
static void gen_texture(track_t *t, uint32_t len)
{
    uint32_t i, ri = gs.g->tex;
    int32_t n[1], lo = ri == SR_DRONE || ri == SR_SUB || ri == SR_NOISE || ri == SR_TEXTURE ? 43 : 55;   /* (an organ, a
                                                         * choir, strings: above the bass) */
    gseed(GS_TEX);
    for (i = 0; i < len; i += 16u) {
        if (i && gch(45)) continue;
        n[0] = gdeg(t, lo, gchord_deg(i) + (gch(30) ? 4 : 0));
        gnote(&t->step[i], n, 1, 72, 0);
        dhold(t, i, gs.g->form == FM_ARC ? 63 : 15, len);
    }
}

/* -------------------------------------------------------------------------------------------------- sounds --- */
static int gnever(const char *s)
{
    uint32_t i;
    for (i = 0; i < NELEM(GEN_NEVER); i++) if (dname_has(s, GEN_NEVER[i])) return 1;
    return 0;
}
static void gen_sound(track_t *t, uint32_t role, uint32_t ri)
{
    const grecipe_t *r = &RECIPE[ri];
    static const uint8_t EW[3] = {55, 30, 15};
    uint32_t e = r->eng[gpickw(EW, 3)], i, k, n, nm = 0, na = 0;
    uint8_t m[32], any[32];
    const engine_t *en;
    if (e == ENGI_FM6 && gs.fm6 >= 2u) e = r->eng[0] == ENGI_FM6 ? EG_A : r->eng[0];
    gs.fm6 += e == ENGI_FM6;
    set_engine_of(t, e);
    en = ENGINES[t->eng_req % NENGINES];
    n = en->npresets;
    if (!n) return;
    for (i = 0; i < n; i++) {
        const char *pn = en->presets[i].name;
        if (gnever(pn)) continue;
        if (role != RL_BASS && (dname_has(pn, "BASS") || dname_has(pn, "GROWL") || dname_has(pn, "ACID"))) continue;
        if (na < sizeof any) any[na++] = (uint8_t)i;
    }
    for (k = 0; k < 3u && r->w[k] && !nm; k++)         /* (0.6.93) the recipe's words in order: the first that fits */
        for (i = 0; i < n; i++) {
            const char *pn = en->presets[i].name;
            if (!gnever(pn) && dname_has(pn, r->w[k]) && nm < sizeof m) m[nm++] = (uint8_t)i;
        }
    apply_preset_to(t, nm ? m[gr() % nm] : na ? any[gr() % na] : gr() % n);
    for (i = 0; i < 8u; i++) {                          /* (an eighth of each range: still that sound, a bit its own) */
        const param_desc_t *d = track_desc(t, P_E0 + i);
        int32_t span = (d->max - d->min) / 8;
        if (!d->label || d->label[0] == '-' || d->max <= d->min || d->fmt == F_ENUM || !span) continue;
        t->p[P_E0 + i] = (int16_t)clamp(t->p[P_E0 + i] + grr(-span, span), d->min, d->max);
    }
    t->p[P_ATK] = (int16_t)grr(r->atk[0], r->atk[1]);
    t->p[P_DEC] = (int16_t)grr(r->dec[0], r->dec[1]);
    t->p[P_SUS] = (int16_t)grr(r->sus[0], r->sus[1]);
    t->p[P_REL] = (int16_t)grr(r->rel[0], r->rel[1]);
    if (e == EG_FM) t->p[P_E2] = (int16_t)clamp(t->p[P_E2] - grr(4, 20), -64, 63);   /* (warmer) */
    if (r->ftype != 255u) {
        int32_t cut = grr(r->cut[0], r->cut[1]), res = grr(r->res[0], r->res[1]);
        if (role == RL_BASS && e != EG_A) cut += 20;    /* (a bass: only its fizz taken) */
        if (gs.g->human >= 40u && role != RL_BASS) cut += 10;   /* (a band: air) */
        if (gs.energy > 66u) cut += 6;
        t->p[P_FTYPE] = (int16_t)r->ftype;
        if (e == EG_A) {                                /* (0.6.93: the recipe's cutoff, not the duller of the two) */
            t->p[P_E4] = (int16_t)clamp(cut, 0, 127);
            t->p[P_E5] = (int16_t)clamp(res, 0, 90);
            t->p[P_FTYPE] = 0;
        } else {
            t->p[P_FCUT] = (int16_t)clamp(cut, 0, 127);
            t->p[P_FRES] = (int16_t)res;
        }
        t->p[P_ED_FLT] = (int16_t)grr(r->envf[0], r->envf[1]);
    }
    if (r->lfo[1]) {
        t->p[P_LWAVE] = (int16_t)(gch(70) ? 0 : 1);
        t->p[P_LRATE] = (int16_t)grr(r->lfo_rate[0], r->lfo_rate[1]);
        t->p[P_LD_FLT] = (int16_t)grr(r->lfo[0], r->lfo[1]);
    }
    if (role == RL_LEAD && gch(55)) {                   /* (0.6.97) a vibrato that comes in late, now and then a tremolo */
        t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)grr(52, 72); t->p[P_LFADE] = (int16_t)grr(40, 90);
        t->p[P_LD_PIT] = (int16_t)grr(3, 9);
        if (gch(30)) t->p[P_LD_AMP] = (int16_t)grr(10, 30);
    } else if (role == RL_PAD && gch(35)) {             /* (a pad breathing: a slow tremolo or a slow pan) */
        if (gch(50)) { t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)grr(6, 18); t->p[P_LD_AMP] = (int16_t)grr(12, 30); }
        else { t->p[P_APDEP] = (int16_t)grr(25, 55); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(1, 3); }
    } else if (role == RL_ARP && gch(40)) {             /* (an arp moving across the stereo on the beat) */
        t->p[P_APDEP] = (int16_t)grr(30, 70); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(3, 5);
    }
    if (r->dist[1] && gch(70)) { t->p[P_DIST] = (int16_t)grr(r->dist[0], r->dist[1]); t->p[P_DTYPE] = (int16_t)(gch(70) ? 0 : 2); }
    else t->p[P_DIST] = 0;
    t->p[P_CHOR] = (int16_t)(r->chor[1] ? grr(r->chor[0], r->chor[1]) : 0);
    t->p[P_GLIDE] = (int16_t)(r->glide ? grr(r->glide - 10, r->glide + 10) : 0);
    t->p[P_VOICE] = (int16_t)r->voice;
    t->p[P_DETUNE] = (int16_t)(ri == SR_REESE || ri == SR_WALL || ri == SR_HOOVER ? grr(30, 60) : t->p[P_DETUNE]);
    if (ri == SR_GTR || ri == SR_DRONE) {               /* (a tremolo: AUTO PAN on the beat) */
        t->p[P_APDEP] = (int16_t)grr(30, 60); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(4, 6); t->p[P_APWAVE] = (int16_t)(ri == SR_GTR ? 3 : 0);
    }
}
static void gen_kit(track_t *t)
{
    const gkit_t *k = &KIT[gs.g->kit];
    uint32_t l, i;
    for (l = 0; l < 8u; l++) {
        const dx_lane_t *D = &DX_KIT_DEF[l];
        dx_lane_t *L = &dx_kit[l];
        L->mode = D->mode;
        for (i = 0; i < DXP_N; i++) {
            int32_t j = grr(-12, 12) * (int32_t)(30u + gs.complex) / 100;
            L->a[i] = (uint8_t)clamp(D->a[i] + k->d[l][i] + j, 0, 127);
            L->b[i] = (uint8_t)clamp(D->b[i] + k->d[l][i] + grr(-12, 12), 0, 127);
        }
    }
    memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);
    {   /* (0.6.94) the snare a snare (it came out tonal): mostly noise, shorter, less of the pitch sweep, the noise's
         * band a little brighter, and its own FEEDBK on top of the LAYER */
        dx_lane_t *S = &dx_kit[DV_SNARE];
        int32_t nz = grr(26, 40), dc = grr(10, 18), pm = grr(8, 14), cl = grr(4, 10);
        S->a[DXP_NOISE] = (uint8_t)clamp(S->a[DXP_NOISE] + nz, 0, 127); S->b[DXP_NOISE] = (uint8_t)clamp(S->b[DXP_NOISE] + nz, 0, 127);
        S->a[DXP_DECAY] = (uint8_t)clamp(S->a[DXP_DECAY] - dc, 0, 127); S->b[DXP_DECAY] = (uint8_t)clamp(S->b[DXP_DECAY] - dc, 0, 127);
        S->a[DXP_PMOD] = (uint8_t)clamp(S->a[DXP_PMOD] - pm, 0, 127); S->b[DXP_PMOD] = (uint8_t)clamp(S->b[DXP_PMOD] - pm, 0, 127);
        S->a[DXP_COLOR] = (uint8_t)clamp(S->a[DXP_COLOR] + cl, 0, 127); S->b[DXP_COLOR] = (uint8_t)clamp(S->b[DXP_COLOR] + cl, 0, 127);
        dx_lay[DV_SNARE] = (uint8_t)(DX_LAY_L(DX_LAY_DEF[DV_SNARE]) | ((uint32_t)grr(3, 6) << 4));
    }
    {   /* (0.6.97) the snare and the clap into the reverb (the kit's space), the rim into the delay where the genre is
         * wet (dub, dub techno ..): the kit's own SENDs (eng_drum.c dx_snd) */
        int32_t wet = (int32_t)gs.g->wet, rv = clamp((18 + grr(0, 22)) * (40 + wet) / 100, 6, 63);
        dx_snd[DV_SNARE] = (uint8_t)((3u << 6) | (uint32_t)rv);
        dx_snd[DV_CLAP] = (uint8_t)((3u << 6) | (uint32_t)clamp(rv + grr(-6, 10), 6, 63));
        dx_snd[DV_RIM] = (uint8_t)(wet >= 60 ? (2u << 6) | (uint32_t)grr(20, 45) : 0u);
        dx_snd[DV_HATO] = (uint8_t)(wet >= 70 && gch(50) ? (3u << 6) | (uint32_t)grr(8, 20) : 0u);
    }
    for (l = DV_HATC; l <= DV_HATO; l++) {               /* (0.6.94) the hats: a band-pass high up, more noise in them */
        dx_lane_t *H = &dx_kit[l];
        int32_t nz = grr(18, 28), cl = grr(10, 18);
        H->mode = (uint8_t)((H->mode & ~0x0Cu) | (DXF_BP << 2));
        H->a[DXP_NOISE] = (uint8_t)clamp(H->a[DXP_NOISE] + nz, 0, 127); H->b[DXP_NOISE] = (uint8_t)clamp(H->b[DXP_NOISE] + nz, 0, 127);
        H->a[DXP_COLOR] = (uint8_t)clamp(H->a[DXP_COLOR] + cl, 0, 127); H->b[DXP_COLOR] = (uint8_t)clamp(H->b[DXP_COLOR] + cl, 0, 127);
    }
    t->p[P_E0] = (int16_t)grr(18, 110);                 /* MORPH */
    t->p[P_E1] = (int16_t)(64 + grr(-7, 7));            /* TUNE (0..127, 64 the kit's own pitch) */
    t->p[P_E2] = (int16_t)clamp(70 + k->tone + grr(-14, 14), 0, 127);
    t->p[P_E3] = (int16_t)clamp(64 + k->decy + grr(-14, 14), 0, 127);
    t->p[P_E4] = (int16_t)clamp(64 + k->nois + grr(-12, 14), 0, 127);
    t->p[P_E5] = (int16_t)grr(8, 52);                   /* FM ENV */
    t->p[P_E6] = (int16_t)grr(6, 48);                   /* FOLD */
    t->p[P_E7] = (int16_t)clamp(k->drv + grr(-10, 16), 0, 127);
    t->p[P_FTYPE] = 0;
    t->p[P_FCUT] = (int16_t)(gs.g->human >= 40u ? grr(114, 127) : grr(98, 122));
    t->p[P_FRES] = (int16_t)grr(0, 18);
    for (l = 0; l < 8u; l++) t->p[P_LN0 + l] = 127;
    t->p[P_LN3] = (int16_t)(gs.g->hats == HC_MACHINE ? 112 : 118);   /* (the hats a little under) */
    t->p[P_LN4] = 118;
    t->p[P_LN6] = t->p[P_LN7] = 110;
}
/* the mix and the humanity of a role */
static void gen_mix(track_t *t, uint32_t r)
{
    const ggenre_t *g = gs.g;
    int32_t wet = (int32_t)g->wet + (gs.energy < 34 ? 20 : gs.energy > 66 ? -15 : 0), hum = (int32_t)(gs.human);
    static const uint8_t HV[RL_N] = {20, 25, 45, 60, 35, 40}, HT[RL_N] = {0, 0, 30, 25, 10, 60}, HG[RL_N] = {0, 10, 10, 40, 30, 0};
    wet = clamp(wet, 0, 100);
    t->p[P_LEVEL] = (int16_t)(r == RL_DRUM ? 106 : r == RL_BASS ? 102 : r == RL_LEAD ? 88 : r == RL_TEX ? (g->tex == SR_NOISE ? 44 : 54) : r == RL_PAD ? 80 : 82);
    t->p[P_PAN] = (int16_t)(r == RL_ARP ? grr(-30, -10) : r == RL_TEX ? grr(10, 30) : r == RL_PAD ? grr(-8, 8) : r == RL_LEAD ? grr(-6, 12) : 0);
    t->p[P_REV] = (int16_t)clamp((r == RL_PAD || r == RL_TEX ? grr(45, 95) : r == RL_LEAD ? grr(30, 70) : r == RL_ARP ? grr(20, 60) :
                                  r == RL_DRUM ? grr(4, 20) : grr(0, 6)) * (40 + wet) / 100, 0, 127);
    t->p[P_DLY] = (int16_t)clamp((r == RL_LEAD || r == RL_ARP ? grr(25, 70) : r == RL_PAD && (g->pads == CS_DUB || g->pads == CS_SKANK) ? grr(55, 95) :
                                  r == RL_PAD ? grr(0, 30) : r == RL_DRUM ? grr(0, 10) : 0) * (40 + wet) / 100, 0, 127);
    if (r == RL_LEAD && t->p[P_REV] < 25 && t->p[P_DLY] < 20) t->p[P_DLY] = (int16_t)grr(25, 55);
    t->p[P_SSWING] = 0;
    t->p[P_HVEL] = (int16_t)(127 - clamp((HV[r] + HT[r] + HG[r]) * hum / 150, 0, 127));   /* (0.6.92: one amount) */
    t->p[P_HTIM] = t->p[P_HGAT] = 127;
}

/* ---------------------------------------------------------------------------------------- the form: parts --- */
static step_t gen_keep[2][NSTEP];
static void section_store(uint32_t s);                  /* (song_main.c) */
static int section_load(uint32_t s);
#include "gen_trap.c"
static void gen_parts(void)
{
    const ggenre_t *g = gs.g;
    uint32_t form = gp.form == 0u ? FM_LOOP : gp.form == 2u ? (g->form == FM_ARC ? FM_ARC : FM_CLUB) : g->form == FM_LOOP ? FM_PARTS : g->form;
    uint32_t dr = gs.trk_of[RL_DRUM], bs = gs.trk_of[RL_BASS], pd = gs.trk_of[RL_PAD], ld = gs.trk_of[RL_LEAD], ar = gs.trk_of[RL_ARP];
    uint32_t k, n = 0, rows = 0, bars = gp.form == 2u ? 16u : 8u, i;
    static const uint8_t ORD_CLUB[6] = {1, 2, 0, 3, 2, 0}, ORD_PARTS[4] = {0, 1, 0, 2}, ORD_ARC[4] = {1, 0, 2, 3};
    static const uint8_t FILLS[6] = {FL_ROLL, FL_TOM, FL_HAT, FL_DROP, FL_LINEAR, FL_KICK16};   /* (0.6.97) a fill a section */
    const uint8_t *ord;
    uint32_t nord;
    if (gs.trap) { if (form != FM_LOOP) trap_parts(); return; }   /* (the trap family: its own song) */
    if (form == FM_LOOP) return;
    gseed(GS_FORM);
    for (k = 0; k < 4u; k++)
        if (project_used(k) && !((dice_secs >> k) & 1u)) return;   /* (a section of yours: kept) */
    live.cur = -1;
    section_store(0);                                   /* A: the whole piece */
    if (live.cur != 0) return;
    dice_secs |= 1u;
    if (dr < NTRK) memcpy(gen_keep[0], trk[dr].step, sizeof gen_keep[0]);
    if (pd < NTRK) memcpy(gen_keep[1], trk[pd].step, sizeof gen_keep[1]);
    n = form == FM_CLUB ? 4u : form == FM_ARC ? 4u : 3u;
    for (k = 1; k < n; k++) {
        /* back to the whole piece, then this section's cut */
        for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = 0;
        if (dr < NTRK) { load_begin(&trk[dr], LOAD_PAT); memcpy(trk[dr].step, gen_keep[0], sizeof gen_keep[0]); motion_clear(&trk[dr]); load_end(&trk[dr]); }
        if (pd < NTRK) { load_begin(&trk[pd], LOAD_PAT); memcpy(trk[pd].step, gen_keep[1], sizeof gen_keep[1]); motion_clear(&trk[pd]); load_end(&trk[pd]); }
        if (dr < NTRK && gs.act[RL_DRUM] && k != 2u) { /* (0.6.97) each section its own fill (the BUILD's below: a
                                                         * roll); the phrase's last bar differs section to section */
            track_t *t = &trk[dr];
            uint32_t f0 = g->fills == FL_NONE ? FL_ROLL : g->fills;
            load_begin(t, LOAD_PAT);
            for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
            gs.drum_fill = k == 1u ? f0 : FILLS[(gs.gi + k) % 6u];
            gen_drums(t, dlen(t));
            gs.drum_fill = 0;
            load_end(t);
            memcpy(gen_keep[0], trk[dr].step, sizeof gen_keep[0]);
        }
        if (form == FM_CLUB) {
            if (k == 1u) {                              /* B INTRO: the drums and the bass */
                if (pd < NTRK) trk[pd].p[P_MUTE] = 1;
                if (ld < NTRK) trk[ld].p[P_MUTE] = 1;
                if (ar < NTRK) trk[ar].p[P_MUTE] = 1;
            } else if (k == 2u) {                       /* C BUILD: the chords in, their filter opening over the loop, a
                                                         * roll at its end, the delay thrown on the last step */
                if (ld < NTRK) trk[ld].p[P_MUTE] = 1;
                if (dr < NTRK && gs.act[RL_DRUM]) {
                    track_t *t = &trk[dr];
                    load_begin(t, LOAD_PAT);
                    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
                    gs.drum_fill = FL_ROLL;
                    gen_drums(t, dlen(t));
                    gs.drum_fill = 0;
                    load_end(t);
                }
                memset(pfx_lane, 0, sizeof pfx_lane);   /* (0.6.97) the punch-in lane: a stutter into the drop (1/8 on
                                                         * the last beat, 1/16 on its last two sixteenths), every track */
                pfx_ltgt = 0;
                for (i = 60; i < 64u; i++) pfx_lane_put(i, i < 62u ? 6u : 7u);
                if (pd < NTRK && gs.act[RL_PAD]) {
                    track_t *t = &trk[pd];
                    uint32_t id = t->eng_req == EG_A ? P_E4 : P_FCUT, base = (uint32_t)clamp(t->p[id], 0, 127), len = dlen(t), j;
                    for (j = 0; j < 4u && j * len / 4u < len; j++)
                        motion_set_event(t, j * len / 4u, id, (int16_t)clamp((int32_t)base - 30 + (int32_t)j * 14, 0, 127));
                    motion_set_event(t, len - 1u, P_DLY, (int16_t)clamp(t->p[P_DLY] + 50, 0, 127));
                    motion_set_event(t, 0, P_DLY, t->p[P_DLY]);
                }
            } else {                                    /* D BREAK: no drums, no bass; the chords wetter; (0.6.97) the
                                                         * synths an octave up on the break's last bar */
                memset(pfx_lane, 0, sizeof pfx_lane);
                pfx_ltgt = 1;
                for (i = 48; i < 64u; i++) pfx_lane_put(i, 2u);
                if (dr < NTRK) trk[dr].p[P_MUTE] = 1;
                if (bs < NTRK) trk[bs].p[P_MUTE] = 1;
                if (pd < NTRK) motion_set_event(&trk[pd], 0, P_REV, (int16_t)clamp(trk[pd].p[P_REV] + 30, 0, 127));
            }
        } else if (form == FM_ARC) {
            if (k == 1u) {                              /* B: thin (the chords and the bass) */
                if (dr < NTRK) trk[dr].p[P_MUTE] = 1;
                if (ld < NTRK) trk[ld].p[P_MUTE] = 1;
                if (ar < NTRK) trk[ar].p[P_MUTE] = 1;
            } else if (k == 2u) {                       /* C: the drums in, the lead out */
                if (ld < NTRK) trk[ld].p[P_MUTE] = 1;
            } else {                                    /* D: the apex, the drums denser */
                if (dr < NTRK && gs.act[RL_DRUM]) {
                    track_t *t = &trk[dr];
                    uint32_t a0 = gs.amt[GP_DRUMS];
                    load_begin(t, LOAD_PAT);
                    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
                    gs.amt[GP_DRUMS] = a0 + 30u > 100u ? 100u : a0 + 30u;
                    gen_drums(t, dlen(t));
                    gs.amt[GP_DRUMS] = a0;
                    load_end(t);
                }
            }
        } else {                                        /* FM_PARTS: B a break (drums and bass out), C a drop (denser drums) */
            if (k == 1u) {
                if (dr < NTRK) trk[dr].p[P_MUTE] = 1;
                if (bs < NTRK && (ld < NTRK || ar < NTRK || pd < NTRK)) trk[bs].p[P_MUTE] = 1;
            } else if (dr < NTRK && gs.act[RL_DRUM]) {
                track_t *t = &trk[dr];
                uint32_t a0 = gs.amt[GP_DRUMS], c0 = gs.complex;
                load_begin(t, LOAD_PAT);
                for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
                gs.amt[GP_DRUMS] = a0 + 25u > 100u ? 100u : a0 + 25u;
                gs.complex = c0 + 20u > 100u ? 100u : c0 + 20u;
                gen_drums(t, dlen(t));
                gs.amt[GP_DRUMS] = a0; gs.complex = c0;
                load_end(t);
            }
        }
        section_store(k);
        if (live.cur != (int8_t)k) { n = k; break; }
        dice_secs |= (uint8_t)(1u << k);
    }
    section_load(0);                                    /* (the whole piece back, as A holds it) */
    live.cur = 0;
    if (n < 2u) return;
    chain_defaults(&chain_config);
    if (form == FM_CLUB) { ord = ORD_CLUB; nord = n >= 4u ? 6u : n == 3u ? 4u : 2u; }
    else if (form == FM_ARC) { ord = ORD_ARC; nord = n; }
    else { ord = ORD_PARTS; nord = n >= 3u ? 4u : 2u; }
    for (k = 0; k < nord && rows < CHAIN_ROWS; k++) {
        if (ord[k] >= n) continue;
        chain_config.row[rows].slot = ord[k];
        chain_config.row[rows].bars = (uint8_t)(form == FM_CLUB && ord[k] == 0u ? bars * 2u : bars);
        rows++;
    }
    chain_config.count = (uint8_t)rows;
    SONG_END(&chain_config) = SONG_FOREVER;
    ui.song_row = 0;
    live.mode = 1;
}

/* --------------------------------------------------------------------------------------------- the piece --- */
static void gen_post(void)
{
    uint32_t dr = gs.trk_of[RL_DRUM], bs = gs.trk_of[RL_BASS], ld = gs.trk_of[RL_LEAD], i, k;
    if (dr < NTRK && bs < NTRK && gs.act[RL_DRUM] && gs.act[RL_BASS] && gs.g->kick_mode == KM_SUSTAIN) {
        track_t *b = &trk[bs];                          /* the bass under the kick: softer there */
        for (i = 0; i < dlen(b); i++)
            if (gs.kick[i] && dstarts(b, i) && i % 16u) b->step[i].vel = (uint8_t)clamp((int32_t)b->step[i].vel - 18, 40, 127);
    }
    if (ld < NTRK && bs < NTRK && gs.act[RL_LEAD] && gs.act[RL_BASS]) {
        track_t *l = &trk[ld], *b = &trk[bs];
        uint32_t len = dlen(l);
        for (i = 0; i < len; i++) {                     /* no minor second, no tritone against the bass */
            step_t *s = &l->step[i];
            int32_t bn = dsounding(b, i), iv, up, dn;
            if (!dstarts(l, i) || bn < 0) continue;
            iv = ((s->note[0] - bn) % 12 + 12) % 12;
            if (iv != 1 && iv != 6) continue;
            up = scale_steps(l, s->note[0], 1);
            dn = scale_steps(l, s->note[0], -1);
            iv = ((dn - bn) % 12 + 12) % 12;
            s->note[0] = (uint8_t)(iv != 1 && iv != 6 ? dn : up <= 84 ? up : s->note[0]);
        }
    }
    for (k = 0; k < NTRK; k++) {                        /* the breath: each fourth bar's last two steps softer */
        track_t *t = &trk[k];
        uint32_t len = dlen(t);
        if (len < 64u || k == dr) continue;
        for (i = 0; i < len; i++)
            if ((i / 16u) % 4u == 3u && i % 16u >= 14u && t->step[i].time == ST_NOTE)
                t->step[i].vel = (uint8_t)clamp((int32_t)t->step[i].vel * 60 / 100, 30, 127);
    }
    if (gs.complex > 30u) {                             /* step components (0.6.97: more of them, some every other loop):
                                                         * the lead echoed, an octave up or bent, the arp jumping its
                                                         * octave, ratcheted or pulsed, a chord arpeggiated or echoed */
        for (k = 0; k < NTRK; k++) {
            uint32_t r = gs.role[k], tries, want = 1u + (gs.complex > 60u) + (gs.complex > 85u), got = 0;
            if (!gs.act[r] || r == RL_DRUM || r == RL_TEX || r == RL_BASS || !gch(40 + gs.complex / 3u)) continue;
            for (tries = 0; tries < 12u && got < want; tries++) {
                uint32_t at = 1u + gr() % (dlen(&trk[k]) - 1u > 0u ? dlen(&trk[k]) - 1u : 1u);
                step_t *s = &trk[k].step[at];
                if (s->time != ST_NOTE || !s->n || s->comp) continue;
                if (r == RL_PAD) {
                    if (s->n < 2u) continue;
                    if (gch(60)) { s->comp = CP_ARPN; s->carg = CP_ARG(gr() % 3u, gch(50) ? 0u : 2u, 0u); }
                    else { s->comp = CP_ECHO; s->carg = (uint8_t)(gr() % 2u); }
                } else if (r == RL_LEAD) {
                    uint32_t c = gr() % 5u;
                    s->comp = (uint8_t)(c == 0u ? CP_ECHO : c == 1u ? CP_OCT : c == 2u ? CP_BEND : c == 3u ? CP_PULSE : CP_WALK);
                    s->carg = (uint8_t)(s->comp == CP_ECHO ? gr() % 2u : s->comp == CP_OCT ? 2u : s->comp == CP_BEND ? (gch(50) ? 2u : 10u) :
                                        s->comp == CP_WALK ? CP_WARG(1u + gr() % 3u, gch(60) ? 2u : 0u, gr() % 2u) : 1u);   /* (0.6.98: a walk
                                                         * every 2..4 loops, there and back mostly, 3 or 5 degrees) */
                } else {
                    uint32_t c = gr() % 4u;
                    s->comp = (uint8_t)(c == 0u ? CP_OCT : c == 1u ? CP_RAND : c == 2u ? CP_ECHO : (gch(50) ? CP_WALK : CP_CLIMB));
                    s->carg = (uint8_t)(s->comp == CP_RAND ? 1u + gr() % 2u : s->comp == CP_OCT ? 4u :
                                        s->comp == CP_WALK ? CP_WARG(gr() % 4u, gr() % 4u, gr() % 2u) :
                                        s->comp == CP_CLIMB ? CP_WARG(2u + gr() % 6u, gch(50) ? 2u : 0u, gr() % 2u) : 0u);
                    if (c == 2u && gs.energy > 60u && gch(50)) { s->comp = 0; step_set_ratchet(s, 2); }
                }
                if (s->comp && gch(40)) ST_EVERY_SET(s, gch(50) ? EV_1_2 : EV_2_2);   /* (every other loop) */
                got++;
            }
        }
    }
}
static int8_t dice_force_style = -1, dice_force_energy = -1;   /* (a film's script, the renders: this genre, this energy) */
static void gen_make(void)
{
    const ggenre_t *g;
    uint32_t k, dr = NTRK, nlive = trk_live < NTRK ? trk_live : NTRK, nmel, drums, next, pr, wet;
    uint32_t steps;
    /* the choices from the seed: the genre (RANDOM), the root, the scale, the progression, the bass */
    gseed(GS_PICK);
    gs.gi = dice_force_style >= 0 ? (uint32_t)dice_force_style % NGENRE : gp.genre ? (gp.genre - 1u) % NGENRE : gr() % NGENRE;
    g = gs.g = &GENRE[gs.gi];
    gmut = *g;
    gs.mutant = 0;
    if (!gp.genre && dice_force_style < 0 && gch(45)) {   /* (0.6.95) RANDOM: a mutant, the drums of one genre, the bass
                                                         * of another, the rest from either; a name of its own: "EBM+JAZZ" */
        const ggenre_t *b = &GENRE[gr() % NGENRE], *c = &GENRE[gr() % NGENRE];
        uint32_t k2, n2 = 0;
        gmut.drum = b->drum; gmut.kit = b->kit; gmut.ghosts = b->ghosts; gmut.hats = b->hats; gmut.fills = b->fills;
        gmut.sd_micro = b->sd_micro; gmut.snd[GP_DRUMS] = b->snd[GP_DRUMS];
        gmut.drum_pct = (uint8_t)(b->drum_pct > g->drum_pct ? b->drum_pct : g->drum_pct);
        for (k2 = 0; k2 < 3u; k2++) gmut.bass[k2] = c->bass[k2];
        gmut.bass_lo = c->bass_lo; gmut.kick_mode = c->kick_mode; gmut.bass_gate = c->bass_gate; gmut.snd[GP_BASS] = c->snd[GP_BASS];
        if (gch(50)) {
            for (k2 = 0; k2 < 3u; k2++) gmut.prog[k2] = b->prog[k2];
            gmut.cbars = b->cbars; gmut.pads = b->pads; gmut.voicing = b->voicing; gmut.snd[GP_CHORDS] = b->snd[GP_CHORDS];
        }
        if (gch(50)) { gmut.arp[0] = c->arp[0]; gmut.arp[1] = c->arp[1]; gmut.snd[GP_ARP] = c->snd[GP_ARP]; }
        if (gch(50)) { gmut.lead = b->lead; gmut.snd[GP_LEAD] = b->snd[GP_LEAD]; }
        if (gch(50)) { gmut.rtype = b->rtype; gmut.dtime = b->dtime; gmut.wet = b->wet; gmut.dtype = b->dtype; }
        gmut.scale[1] = b->scale[0]; gmut.scale[3] = c->scale[0];
        gmut.tex = c->tex;
        gmut.bpm_lo = (uint8_t)(((uint32_t)g->bpm_lo + b->bpm_lo) / 2u);
        gmut.bpm_hi = (uint8_t)(((uint32_t)g->bpm_hi + b->bpm_hi) / 2u);
        if (gmut.bpm_hi < gmut.bpm_lo + 4u) gmut.bpm_hi = (uint8_t)(gmut.bpm_lo + 4u);
        gmut.swing = (uint8_t)(((uint32_t)g->swing + b->swing) / 2u);
        for (k2 = 0; g->name[k2] && g->name[k2] != ' ' && k2 < 7u; k2++) gmut_name[n2++] = g->name[k2];   /* (the first words) */
        gmut_name[n2++] = '+';
        for (k2 = 0; b->name[k2] && b->name[k2] != ' ' && k2 < 6u; k2++) gmut_name[n2++] = b->name[k2];
        gmut_name[n2] = 0;
        gmut.name = gmut_name;
        gs.mutant = 1;
    }
    {   /* the trap family: its own generator (gen_trap.c), its style from the genre's drums */
        const ggenre_t *o = &GENRE[gs.gi];
        gs.trap = (uint8_t)(!gs.mutant && o->kick_mode == KM_808);
        gs.ts = (uint8_t)(o->drum == DM_UK_DRILL ? TS_DRILL : o->drum == DM_PHONK ? TS_DARK : o->drum == DM_RAGE ? TS_RAGE : TS_ATL);
        if (gs.trap) trap_flavour();                    /* (its producers' flavour: tempo, scales, sounds) */
    }
    /* (0.6.95) a throw its own: the drums from the map's family, the chords' play and a role's recipe from the neighbours */
    if (gmut.drum < DM_NONE && gmut.drum != DM_IDM && gch(35)) gmut.drum = DM_ALT[gmut.drum][gr() % 2u];
    if (gmut.pads <= CS_TRANCE && gch(35)) gmut.pads = CS_ALT[gmut.pads][gr() % 2u];
    for (k = 1; k < GP_N; k++) if (gmut.snd[k] < SR_N && gch(30)) gmut.snd[k] = SR_ALT[gmut.snd[k]][gr() % 2u];
    g = gs.g = &gmut;
    gs.root = (int16_t)grr(0, 11);
    gs.scale = (int16_t)g->scale[gr() % 4u];
    gs.energy = dice_force_energy >= 0 ? (uint32_t)dice_force_energy * 50u : gp.energy;
    gs.complex = gp.complex;
    if (gs.mutant) gs.complex = gs.complex + 20u > 100u ? 100u : gs.complex + 20u;   /* (a mutant: more going on) */
    gs.human = (uint32_t)gp.human * (g->human + 30u) / 100u;
    gs.swing = gp.swing == GA_AUTO ? g->swing : gp.swing;
    for (k = 0; k < GP_N; k++) {
        uint32_t a = gp.amt[k] == GA_AUTO ? g->amt[k] : gp.amt[k];
        if (gp.amt[k] == GA_AUTO && a) a = (uint32_t)clamp((int32_t)a + ((int32_t)gs.energy - 50) / 2, 10, 100);
        gs.amt[k] = a;
    }
    pr = gr() % 3u;
    gs.prog = &PROG[g->prog[pr]];
    gs.n = gs.prog->n;
    gs.vm = g->voicing;
    if (gs.complex > 70u && gs.vm == VM_PLAIN && gch(40)) gs.vm = VM_RICH;
    if (gs.trap) trap_harmony();
    for (k = 0; k < gs.n; k++) {                        /* the qualities as the voicing mode wants them */
        uint32_t q = gs.prog->q[k], qq = q & 0x7Fu;
        switch (gs.vm) {
        case VM_PLAIN: qq = Q_TRI; break;
        case VM_RICH: if (qq == Q_TRI) qq = gch(50) ? Q_7 : gch(50) ? Q_ADD9 : Q_9; break;
        case VM_SUS: qq = gch(50) ? Q_SUS4 : gch(50) ? Q_SUS2 : Q_7NO5; break;
        case VM_QUART: qq = gch(70) ? Q_QUART : Q_SUS4; break;
        case VM_POWER: qq = Q_POWER; break;
        default: break;                                 /* PAD, PLANE: as written */
        }
        gs.q[k] = (uint8_t)(qq | (q & CQ_DOM));
    }
    gs.csteps = 16u * g->cbars;
    if (gs.csteps * gs.n > NSTEP) gs.csteps = NSTEP / gs.n;
    steps = gs.csteps * gs.n;
    if (steps < 16u) steps = 16u;
    if (gs.complex > 50u && steps < 64u && gch(50)) steps = 64;   /* (four bars: fills, variations, a fourth-bar pickup) */
    gs.len = steps;
    if (gs.trap) { gs.csteps = 64u / gs.n; gs.len = steps = 64u; }   /* (four bars: the hook's loop) */
    gs.ba = g->bass[gr() % 3u];
    if (gs.amt[GP_BASS] > 70u && gch(30)) gs.ba = g->bass[0];
    gs.fm6 = 0;
    {   /* the scale's size (the degrees wrap at it) */
        uint32_t m = SCALE_MASK[clamp(gs.scale, 0, (int32_t)(sizeof SCALE_MASK / sizeof SCALE_MASK[0]) - 1)], c = 0;
        for (k = 0; k < 12u; k++) c += (m >> k) & 1u;
        gs.nsc = c ? c : 7u;
    }
    /* the orchestration: the drums (unless the genre or the knob leaves them out), then the melodic roles with an
     * amount, the most important first; the drums on the DRUM track (none: the fourth, or the last live) */
    {
        static const uint8_t PRI[GP_N] = {RL_DRUM, RL_BASS, RL_PAD, RL_ARP, RL_LEAD};
        uint8_t pri[5];
        uint32_t np = 0;
        drums = gs.amt[GP_DRUMS] && g->drum != DM_NONE && (gp.amt[GP_DRUMS] != GA_AUTO || gch(g->drum_pct));
        for (k = 0; k < RL_N; k++) gs.act[k] = 0;
        gs.act[RL_DRUM] = (uint8_t)drums;
        static const uint8_t TRAP_K[GP_N - 1u] = {GP_BASS, GP_LEAD, GP_CHORDS, GP_ARP};   /* (trap: the melody before the
                                                         * pad: it lives on four tracks) */
        for (k = 1; k < GP_N; k++) {
            uint32_t kk = gs.trap ? TRAP_K[k - 1u] : k;
            if (gs.amt[kk]) { pri[np++] = PRI[kk]; gs.act[PRI[kk]] = 1; }
        }
        nmel = np;
        if (nmel < 2u) {                                /* (never fewer than two voices: the bass, the chords) */
            if (!gs.act[RL_BASS]) { gs.act[RL_BASS] = 1; gs.amt[GP_BASS] = 60; pri[np++] = RL_BASS; }
            if (np < 2u && !gs.act[RL_PAD]) { gs.act[RL_PAD] = 1; gs.amt[GP_CHORDS] = 60; pri[np++] = RL_PAD; }
        }
        for (k = 0; k < GP_N - 1u; k++) {               /* (the silent ones after) */
            uint32_t kk = gs.trap ? TRAP_K[k] : k + 1u;
            if (!gs.act[PRI[kk]]) pri[np++] = PRI[kk];
        }
        for (k = 0; k < NTRK && dr == NTRK; k++) if (drum_track(&trk[k])) dr = k;
        if (dr == NTRK) { dr = nlive > 3u ? 3u : nlive - 1u; set_engine_of(&trk[dr], ENGI_DRUM); }
        for (k = 0; k < RL_N; k++) gs.trk_of[k] = NTRK;
        next = 0;
        for (k = 0; k < NTRK; k++) {
            if (k == dr) { gs.role[k] = RL_DRUM; gs.trk_of[RL_DRUM] = (uint8_t)k; continue; }
            gs.role[k] = next < 4u ? pri[next] : RL_TEX;
            if (gs.trk_of[gs.role[k]] == NTRK) gs.trk_of[gs.role[k]] = (uint8_t)k;
            next++;
        }
        gs.act[RL_TEX] = (uint8_t)(nlive > 5u && gs.trk_of[RL_TEX] < nlive && (gs.amt[GP_CHORDS] > 0u || (gs.trap && drums)));
        if (gs.trap && !drums) gs.act[RL_TEX] = 0;     /* (trap: the TEX track the triplet hats, with the drums) */
    }
    for (k = 0; k < NTRK; k++) { trk[k].p[P_ROOT] = gs.root; trk[k].p[P_SCALE] = gs.scale; }
    sl_root = gs.root; sl_scale = gs.scale;
    wet = (uint32_t)clamp((int32_t)g->wet + (gs.energy < 34 ? 20 : gs.energy > 66 ? -15 : 0), 0, 100);
    /* the sounds (their own stream), then the parts in the pipeline's order: drums, bass (reads the kick), chords,
     * arp (reads the voicing), lead (reads the bass) */
    gseed(GS_SOUND);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t r = gs.role[k];
        if (gs.trap) trap_sound(t, r);
        else if (r == RL_DRUM) gen_kit(t);
        else gen_sound(t, r, r == RL_BASS ? g->snd[GP_BASS] : r == RL_PAD ? g->snd[GP_CHORDS] : r == RL_LEAD ? g->snd[GP_LEAD] :
                             r == RL_ARP ? g->snd[GP_ARP] : g->tex);
        t->p[P_ROOT] = gs.root; t->p[P_SCALE] = gs.scale;
    }
    gseed(GS_MIX);
    for (k = 0; k < NTRK; k++) gen_mix(&trk[k], gs.role[k]);
    {
        static const uint8_t ORDER[5] = {RL_DRUM, RL_BASS, RL_PAD, RL_ARP, RL_LEAD};
        uint32_t o;
        for (o = 0; o < 6u; o++) {
            uint32_t r = o < 5u ? ORDER[o] : RL_TEX;
            for (k = 0; k < NTRK; k++) {
                track_t *t = &trk[k];
                uint32_t len = gs.len;
                if (gs.role[k] != r) continue;
                if (r == RL_TEX && gs.complex > 60u && gch(40)) len = (uint32_t)grr(5, 7) * 2u + (gch(50) ? 1u : 0u);
                if (gs.mutant && (r == RL_ARP || r == RL_LEAD) && gch(50)) {   /* (0.6.95) a mutant: a part in another metre */
                    static const uint8_t ODD[4] = {12, 20, 24, 28};
                    len = ODD[gr() % 4u];
                }
                load_begin(t, LOAD_PAT);
                track_defaults_steps(t);
                { uint32_t j; for (j = 0; j < NSTEP; j++) gclear(&t->step[j]); }   /* (no note left from before) */
                motion_clear(t);
                if (gs.trap && r == RL_TEX) len = 48u;   /* (the triplet hats: two bars of 16T) */
                if (gs.trap && gs.act[r] && (r != RL_TEX || k == gs.trk_of[RL_TEX])) {
                    switch (r) {
                    case RL_DRUM: trap_drums(t, len, 0); break;
                    case RL_BASS: trap_808(t, len, 0); break;
                    case RL_PAD: trap_pads(t, len); break;
                    case RL_ARP: trap_counter(t, len); break;
                    case RL_LEAD: trap_lead(t, len, 0); break;
                    default: trap_hats2(t, 0); break;
                    }
                } else if (gs.act[r] && (r != RL_TEX || k == gs.trk_of[RL_TEX])) {
                    switch (r) {
                    case RL_DRUM: gen_drums(t, len); break;
                    case RL_BASS: gen_bass(t, len); break;
                    case RL_PAD: gen_pads(t, len); break;
                    case RL_ARP: gen_arp(t, len); break;
                    case RL_LEAD: gen_lead(t, len); break;
                    default: gen_texture(t, len); break;
                    }
                }
                t->p[P_SLEN] = (int16_t)len;
                if (gs.trap) { if (r != RL_LEAD && r != RL_ARP) t->p[P_SGATE] = (int16_t)(r == RL_BASS ? 95 : 120); }   /* (the lead's,
                                                         * the counter's: their own; the 808 ends before the next step: struck
                                                         * again, but for its slides) */
                else if (r == RL_BASS) t->p[P_SGATE] = (int16_t)clamp(g->bass_gate, 1, 127);
                else if (r == RL_PAD) t->p[P_SGATE] = (int16_t)(g->pads == CS_STAB || g->pads == CS_DUB || g->pads == CS_SKANK || g->pads == CS_BROKEN ? grr(25, 45) :
                                                              g->pads == CS_TRANCE ? 35 : g->pads == CS_COMP ? 60 : 120);
                else if (r == RL_LEAD) t->p[P_SGATE] = (int16_t)(g->lead == LD_MECH ? 50 : 85);
                else if (r != RL_ARP) t->p[P_SGATE] = 120;
                if (r != RL_DRUM) dlevel(t, len);
                load_end(t);
                pat_sig[k] = steps_sig(t);
            }
        }
    }
    if (gs.trap) {                                      /* the trap mix (after dlevel: the melody high and still loud) */
        gseed(GS_MIX + 32u);
        for (k = 0; k < NTRK; k++) trap_mix(&trk[k], gs.role[k], 0);
        if (gs.trk_of[RL_TEX] < NTRK && dr < NTRK && gs.act[RL_TEX]) trap_hat_track_like(&trk[gs.trk_of[RL_TEX]], &trk[dr]);
    }
    gseed(GS_FORM);
    gen_post();
    if (dr < NTRK && gs.act[RL_DRUM] && !gs.trap && gch(45 + gs.complex / 4u)) {   /* (0.6.97) MORPH moving over the loop: the kit's
                                                         * character drifting a little and back (the drum track's
                                                         * automation) */
        track_t *t = &trk[dr];
        uint32_t len = dlen(t), j, n = len >= 32u ? 4u : 2u;
        int32_t base = t->p[P_E0], sw = grr(14, 34) * (gch(50) ? 1 : -1);
        for (j = 0; j < n; j++)
            motion_set_event(t, j * len / n, P_E0, (int16_t)clamp(base + (j == 0u ? 0 : j == n / 2u ? sw : sw / 2), 0, 127));
    }
    for (k = 0; k < NTRK; k++) pat_sig[k] = steps_sig(&trk[k]);
    /* the tempo (the genre's range, the energy in it; always moved), the swing, the space, the master */
    if (!(ui_prefs & PREF_BPM_LOCK)) {                  /* the genre's range, the energy's place in it (from the seed:
                                                         * the same piece, the same tempo) */
        int32_t lo = g->bpm_lo, hi = g->bpm_hi, v;
        if (gs.trap) trap_bpm(&lo, &hi);
        v = lo + (hi - lo) * (int32_t)gs.energy / 100 + grr(-3, 3);
        song.g[G_BPM] = (int16_t)clamp(v, lo, hi);
    }
    song.g[G_SWING] = (int16_t)clamp((int32_t)gs.swing, 0, 100);
    song.g[G_DTIME] = g->dtime;
    song.g[G_DFDBK] = (int16_t)clamp(wet >= 70u ? grr(58, 80) : grr(30, 55) + (int32_t)wet / 3, 0, 95);   /* (0.6.97: the wet
                                                         * genres' long tails: dub, dub techno, shoegaze, ambient ..) */
    song.g[G_DMIX] = (int16_t)grr(70, 110);
    song.g[G_DCOLOR] = (int16_t)(g->dtype & 1u ? grr(50, 70) : grr(30, 52));
    song.g[G_DTYPE] = (int16_t)(g->dtype & 1u);
    song.g[G_DPING] = (int16_t)((g->dtype >> 1) & 1u);
    song.g[G_DMOD] = (int16_t)(wet >= 70u ? grr(20, 50) : g->human >= 40u && gch(50) ? grr(10, 36) : 0);   /* (0.6.97: the
                                                         * tape's wobble on the long tails) */
    song.g[G_DPIT] = 0; song.g[G_DSPRY] = 0;
    song.g[G_RTYPE] = g->rtype;
    song.g[G_RSIZE] = (int16_t)clamp(grr(40, 80) + (int32_t)wet / 2, 0, 127);
    song.g[G_RRATE] = (int16_t)grr(10, 60);
    song.g[G_RWIDE] = (int16_t)grr(40, 100);
    song.g[G_RDAMP] = (int16_t)grr(55, 100);
    song.g[G_RFILT] = (int16_t)grr(-40, -8);
    song.g[G_RMOD] = (int16_t)grr(0, 60);
    song.g[G_RPRE] = (int16_t)(g->rtype == 3 ? grr(0, 4) : g->rtype == 2 || g->rtype == 4 ? grr(-12, 0) : grr(0, 30));
    song.g[G_CRATE] = (int16_t)grr(10, 60);
    song.g[G_CDEPTH] = (int16_t)grr(20, 80);
    song.g[G_STRN] = 0;
    song.g[G_SLPQ] = 1;
    song.g[G_DUCK] = (int16_t)(gs.act[RL_DRUM] ? (g->kick_mode == KM_SUSTAIN ? grr(30, 50) : g->kick_mode == KM_808 ? grr(0, 8) :
                                                  grr(14, 30)) : 0);
    song.g[G_DREL] = (int16_t)(g->drum == DM_TECHNO || g->drum == DM_HARD ? grr(15, 30) : grr(25, 45));
    song.g[G_CLIP] = (int16_t)(gs.act[RL_DRUM] ? grr(25, 55) : grr(10, 30));
    song.g[G_PUNCH] = (int16_t)(gs.act[RL_DRUM] ? grr(45, 85) : 0);
    if (gs.trap) trap_master();
    rng_state = gq | 1u;
    macro_dice(3);
    for (k = 0; k < NTRK; k++) trk[k].p[P_MSND] = trk[k].p[P_MPAT] = 0;   /* (0.6.96) a new piece: MUTATE off on every
                                                         * track, its anchor the piece (a reroll kept the lanes and
                                                         * orbited the old piece's sounds) */
    mut_keep();
    gen_parts();
}
/* the piece made from the settings (UNDO brings back what was there); the message */
static void gen_go(void)
{
    uint32_t k;
    char b[8];
    undo_mark();
    gen_make();
    str_cpy(gp.last, gs.g->name, sizeof gp.last);
    k = str_len(gp.last);
    b[0] = ' '; b[1] = (char)('0' + gp.seed / 1000u % 10u); b[2] = (char)('0' + gp.seed / 100u % 10u);
    b[3] = (char)('0' + gp.seed / 10u % 10u); b[4] = (char)('0' + gp.seed % 10u); b[5] = 0;
    str_cpy(gp.last + k, b, sizeof gp.last - k);
    ui_say("GEN: ", gp.last);
    str_cpy(ui.msg2, live.mode ? "PARTS: PLAY THE SONG" : "UNDO: GLO + G4", sizeof ui.msg2);
    ui.force = 1;
}
static uint32_t gen_sig(void)                           /* the settings that make the piece */
{
    uint32_t h = 2166136261u, k;
    h = (h ^ gp.genre) * 16777619u; h = (h ^ gp.seed) * 16777619u; h = (h ^ gp.energy) * 16777619u; h = (h ^ gp.form) * 16777619u;
    for (k = 0; k < GP_N; k++) h = (h ^ gp.amt[k]) * 16777619u;
    h = (h ^ gp.complex) * 16777619u; h = (h ^ gp.swing) * 16777619u; h = (h ^ gp.human) * 16777619u;
    return h ^ (trk_live << 28);
}
static void gen_new_seed(void)
{
    static uint16_t seen[64];                           /* (the last 64 seeds: a throw never one of them again) */
    static uint8_t nseen;
    uint32_t i, tries, dup;
    rng_state ^= fm1_ticks() * 2654435761u;
    if (!rng_state) rng_state = 0x1234567u;
    for (tries = 0; tries < 8u; tries++) {
        gp.seed = (uint16_t)(rng() % 10000u);
        for (i = 0, dup = 0; i < nseen && i < 64u; i++) dup |= seen[i] == gp.seed;
        if (!dup) break;
    }
    seen[nseen % 64u] = gp.seed;
    nseen = (uint8_t)(nseen + 1u >= 128u ? 64u : nseen + 1u);
}
/* OCT+ on the GEN screen: the piece; the same settings again: a new seed first */
static void gen_press(void)
{
    if (gp.sig == gen_sig()) gen_new_seed();
    gen_go();
    gp.sig = gen_sig();
}
/* (the old name: tests, the films, the renders) a new throw: any genre, a new seed (the screen's settings otherwise) */
static void dice_all(void)
{
    uint8_t g = gp.genre;
    gp.genre = 0;
    gen_new_seed();
    gen_go();
    gp.sig = gen_sig();
    gp.genre = g;
}
static uint32_t dice_print(void)                        /* a fingerprint of the piece (the tests) */
{
    uint32_t k, h = 2166136261u ^ (uint32_t)song.g[G_BPM];
    for (k = 0; k < NTRK; k++) h = (h ^ steps_sig(&trk[k]) ^ trk[k].eng_req) * 16777619u;
    return h;
}

/* ------------------------------------------------------------------------------------------- the GEN screen --- */
static const char *const GEN_LBL[3][4] = {{"GENRE", "SEED", "ENERGY", "FORM"}, {"DRUMS", "BASS", "CHORDS", "ARP"},
                                          {"LEAD", "COMPLEX", "SWING", "HUMAN"}};
static const char *const GEN_FORM[3] = {"LOOP", "PARTS", "SONG"};
static uint8_t gen_fresh;                               /* (0.6.96) the screen's first frame: the body cleared once */
static void gen_open(void)
{
    gp.open_ms = fm1_ms | 1u;
    gp.hot = 0xFF;
    gen_fresh = 1;
    gp.genre = 0;                                       /* (0.6.93) opens on RANDOM: a genre is this visit's choice */
}
static void gen_amt_turn(uint8_t *v, int32_t s)         /* OFF, 10 .. 100, AUTO (above 100) */
{
    int32_t x = *v == GA_AUTO ? 110 : (int32_t)*v;
    x = clamp(x + s * 10, 0, 110);
    *v = (uint8_t)(x > 100 ? GA_AUTO : x);
}
static void gen_knob(uint32_t k, int32_t s)
{
    uint32_t p = gp.page;
    gp.hot = (uint8_t)k;
    if (p == 0u) {
        if (k == 0u) gp.genre = (uint8_t)((gp.genre + (uint32_t)(s > 0 ? 1 : NGENRE) ) % (NGENRE + 1u));
        else if (k == 1u) gp.seed = (uint16_t)((gp.seed + 10000u + (uint32_t)clamp(accel(EN_K2, s, 9999), -9999, 9999)) % 10000u);
        else if (k == 2u) gp.energy = (uint8_t)clamp((int32_t)gp.energy + s * 5, 0, 100);
        else gp.form = (uint8_t)clamp((int32_t)gp.form + (s > 0 ? 1 : -1), 0, 2);
    } else if (p == 1u) {
        gen_amt_turn(&gp.amt[k], s);
    } else {
        if (k == 0u) gen_amt_turn(&gp.amt[GP_LEAD], s);
        else if (k == 1u) gp.complex = (uint8_t)clamp((int32_t)gp.complex + s * 5, 0, 100);
        else if (k == 2u) { int32_t x = gp.swing == GA_AUTO ? 105 : (int32_t)gp.swing; x = clamp(x + s * 5, 0, 105); gp.swing = (uint8_t)(x > 100 ? GA_AUTO : x); }
        else gp.human = (uint8_t)clamp((int32_t)gp.human + s * 5, 0, 100);
    }
    ui.force = 1;
}
static void gen_select(int32_t s)
{
    if (gp.open_ms && fm1_ms - gp.open_ms < 400u) { gp.open_ms = fm1_ms | 1u; return; }   /* (the turn that opened it) */
    gp.open_ms = 0;
    gp.page = (uint8_t)((gp.page + 3u + (uint32_t)(s > 0 ? 1 : 2)) % 3u);
    gp.hot = 0xFF;
    ui.force = 1;
}
static void gen_value(uint32_t p, uint32_t k, char *b)
{
    uint32_t v;
    b[0] = 0;
    if (p == 0u) {
        if (k == 0u) { str_cpy(b, gp.genre ? GENRE[(gp.genre - 1u) % NGENRE].name : "RANDOM", 16); return; }
        if (k == 1u) { b[0] = (char)('0' + gp.seed / 1000u % 10u); b[1] = (char)('0' + gp.seed / 100u % 10u); b[2] = (char)('0' + gp.seed / 10u % 10u); b[3] = (char)('0' + gp.seed % 10u); b[4] = 0; return; }
        if (k == 3u) { str_cpy(b, GEN_FORM[gp.form % 3u], 16); return; }
        v = gp.energy;
    } else if (p == 1u) v = gp.amt[k];
    else if (k == 0u) v = gp.amt[GP_LEAD];
    else if (k == 1u) v = gp.complex;
    else if (k == 2u) v = gp.swing;
    else v = gp.human;
    if (v == GA_AUTO) { str_cpy(b, "AUTO", 16); return; }
    if (!v && (p == 1u || (p == 2u && k == 0u))) { str_cpy(b, "OFF", 16); return; }
    b[0] = (char)('0' + v / 100u); b[1] = (char)('0' + v / 10u % 10u); b[2] = (char)('0' + v % 10u); b[3] = '%'; b[4] = 0;
    if (b[0] == '0') { b[0] = b[1]; b[1] = b[2]; b[2] = b[3]; b[3] = 0; if (b[0] == '0' && v < 10u) { b[0] = b[1]; b[1] = b[2]; b[2] = 0; } }
}
static void draw_gen(void)
{
    uint32_t k;
    char t[24], v[16];
    if (gen_fresh || ui.head_sig != ~0u) {              /* (0.6.96) cleared once on entering, not on every knob: each
                                                         * card and line paints its own band (it flickered) */
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
        gen_fresh = 0;
    }
    ui.head_sig = ~0u;
    str_cpy(t, "GENERATOR  1/3", sizeof t);
    t[11] = (char)('1' + gp.page);
    draw_text_box(0, 30, 240, &AF_M, t, T_THEME, 1);
    for (k = 0; k < 4u; k++) {
        uint32_t y = 56u + k * 28u;
        int hot = gp.hot == k;
        cv_begin(208, 24, T_BG);
        cv_rrect(0, 0, 208, 24, 6, hot ? T_RAISE : T_SURF, T_BG);
        cv_text(10, 5, &AF_S, GEN_LBL[gp.page][k], T_MID);
        gen_value(gp.page, k, v);
        cv_text(198 - text_w(&AF_M, v), 3, &AF_M, v, hot ? T_ACCENT : T_TEXT);
        cv_blit(16, y);
    }
    if (gp.last[0]) {
        str_cpy(t, "LAST: ", sizeof t);
        str_cpy(t + 6, gp.last, sizeof t - 6);
        draw_text_box(0, 172, 240, &AF_S, t, T_MID, 1);
    }
    draw_text_box(0, 190, 240, &AF_S, "OCT+ MAKE   HOME DONE", T_TEXT, 1);
    draw_text_box(0, 206, 240, &AF_S, "SELECT: PAGE  KNOBS: SET", T_DIM, 1);
}
