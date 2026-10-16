/* SPDX-License-Identifier: GPL-3.0-only
 * (GAMMAWAVE) the trap family's own generator: TRAP, TRAP ATL, UK DRILL, DARK TRAP, RAGE TRAP. Included by gen.c
 * (after its helpers, before gen_parts); every other genre never comes here, so its pieces stay as they were.
 *
 * What trap is, as the generator plays it (producers' conventions, see the README of this change):
 *  - half time at 130..165 BPM: the snare and the clap on beat 3 of every bar, the kick around it, short and punchy;
 *  - the hats are the groove: a bed of eighths or sixteenths, rolls before the snare and at the bar's end (1/32,
 *    1/64), triplet bursts (on a second DRUM track at 16T when TRACKS 6 lets one live), velocity moving all the time;
 *  - the 808 is the bass and the kick's shadow: a long sine on every kick, held to the next, gliding between notes
 *    (drill the most), driven so it is heard on a small speaker (dark trap and rage clip it);
 *  - the melody: a short loop of chord tones in minor, harmonic minor or phrygian, high and clear (bells, keys, a
 *    flute, a cowbell in dark trap, a bright detuned lead in rage), repeated, varied at the phrase's end, wet; a dark
 *    pad under it, a counter line;
 *  - a song: intro (the melody alone), hook (everything), verse (another hat style and kick, the melody out), bridge
 *    (the drums out, a snare roll into the last hook); a stop on the last beat before a hook. */

enum { TS_ATL, TS_DRILL, TS_DARK, TS_RAGE, TS_N };
#define TG_WH 7u                                        /* WHEEL (song_dice.c names the others) */

/* ------------------------------------------------------------------------------------------------ harmony --- */
/* trap's progressions (degrees of the scale, a chord every 64 / n steps): i-VI, i-iv, i-VI-III-VII, i-V (the V
 * major: harmonic minor's), i alone, i-bII (phrygian), i-VII-VI-V */
static const gprog_t TPROG[8] = {
    {2, {0, 5}, {Q_TRI, Q_TRI}},
    {2, {0, 3}, {Q_TRI, Q_TRI}},
    {4, {0, 5, 2, 6}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI}},
    {2, {0, 4}, {Q_TRI, Q_TRI | CQ_DOM}},
    {1, {0}, {Q_TRI}},
    {2, {0, 1}, {Q_TRI, Q_TRI}},
    {4, {0, 6, 5, 4}, {Q_TRI, Q_TRI, Q_TRI, Q_TRI | CQ_DOM}},
    {2, {5, 0}, {Q_TRI, Q_TRI}},
};
static const uint8_t TPROG_W[TS_N][8] = {
    [TS_ATL]   = {30, 20, 25, 10, 0, 0, 5, 10},
    [TS_DRILL] = {25, 20, 10, 15, 0, 10, 15, 5},
    [TS_DARK]  = {15, 20, 5, 10, 25, 20, 5, 0},
    [TS_RAGE]  = {30, 15, 30, 5, 5, 0, 5, 10},
};
static void trap_harmony(void)
{
    gs.prog = &TPROG[gpickw(TPROG_W[gs.ts], 8)];
    if (gs.prog == &TPROG[5] && gs.scale != SC_PHRY) gs.prog = &TPROG[1];   /* (bII only where the mode has it) */
    gs.n = gs.prog->n;
    gs.vm = VM_PLAIN;
}

/* ---------------------------------------------------------------------------------------------------- drums --- */
/* the kick, two bars a pattern: 'X' accented, 'x' a hit, 'o' a hit half the time, '.' none; never on the snare's
 * beat 3 (steps 8 and 24) */
static const char *const TK_KICK[TS_N][6] = {
    [TS_ATL]   = {"X.....x...x...x.X.....x..x....o.", "X......x..x.....X.....x...x.x...", "X..x......x.....X..x..o...x.....",
                  "X.....x..x....x.X.....x..x..x...", "X.o.......x...o.X.....x.....x.x.", "X.....xx......x.X......x..x....."},
    [TS_DRILL] = {"X.....x...x..x..X..x..x...x.....", "X......x..x.....X.....x..x...x..", "X..x.....x...x..X.....x....x....",
                  "X.....x...x.....X..x......x..x..", "X..x..x.....x...X......x..x..x..", "X.....x..x....x.X..x.....x......"},
    [TS_DARK]  = {"X.....x...x...x.X.....x...x...x.", "X..x..x...x.....X..x..x...x.x...", "X.....xx..x.....X.....xx..x...x.",
                  "X...x.x...x.x...X...x.x...x.....", "X.....x...x..x..X.x...x...x..x..", "X..x......x.x...X..x......x.x.x."},
    [TS_RAGE]  = {"X..x..x...x...x.X..x..x...x..x..", "X.x...x...x.x...X.x...x...x.x...", "X..x..x...x..x..X..x..x...x...x.",
                  "X...x.x...x.x.x.X...x.x...x...x.", "X..x..x..xx.....X..x..x..x..x.x.", "X.x.x.x...x.x.x.X.x.x.x...x.x.x."},
};
/* dark trap's cowbell (the drum kit's BELL), a bar: the Memphis figure under the melody */
static const char *const TK_BELL[4] = {"x..x..x.x.x.x...", "x.x..x.x..x.x.x.", "x..x..x..x..x.x.", "x...x.x.x...x.x."};
/* drill's percussion (the RIM lane), a bar: the gallop's off-beats */
static const char *const TK_PERC[3] = {"..x..x...x..x...", "...x..x...x..x..", "..x...x..x...x.."};

/* the hat figures, a beat (12 ticks; 3 a sixteenth, 2 a triplet sixteenth) */
enum { HF_8, HF_16, HF_R32, HF_R64, HF_T16, HF_T8, HF_GAP, HF_OFF, HF_N };
/* the bed (most beats) and the rolls (the beat before the snare, the bar's last), as weights, a style */
static const uint8_t HF_BED[TS_N][HF_N] = {
    [TS_ATL]   = {40, 60, 0, 0, 0, 0, 0, 0},
    [TS_DRILL] = {0, 0, 0, 0, 0, 0, 0, 0},              /* (drill's bed is its gallop, a bar) */
    [TS_DARK]  = {15, 85, 0, 0, 0, 0, 0, 0},
    [TS_RAGE]  = {70, 25, 0, 0, 0, 0, 0, 5},
};
static const uint8_t HF_ROLL[TS_N][HF_N] = {
    [TS_ATL]   = {0, 10, 30, 15, 30, 15, 0, 0},
    [TS_DRILL] = {0, 0, 15, 5, 40, 30, 10, 0},
    [TS_DARK]  = {0, 10, 45, 20, 20, 5, 0, 0},
    [TS_RAGE]  = {5, 5, 15, 10, 45, 20, 0, 0},
};
static const char DRILL_BED[16] = {'x', '.', '.', 'x', '.', '.', 'x', '.', 'x', '.', '.', 'x', '.', '.', 'x', '.'};

/* a hit on the main track: a lane, its velocity (the step's, shared with the other lanes: the louder wins) */
static void trap_hit(track_t *t, uint32_t i, uint32_t lane, int32_t vel, int acc)
{
    step_t *s = &t->step[i];
    if (s->time != ST_NOTE || !s->hit) { gclear(s); s->time = ST_NOTE; s->vel = (uint8_t)clamp(vel, 30, 127); }
    else if (vel > s->vel) s->vel = (uint8_t)clamp(vel, 30, 127);
    s->hit |= (uint8_t)(1u << lane);
    if (acc) s->acc |= (uint8_t)(1u << lane);
}
/* the drums of a section: var 0 the hook, 1 the verse (another kick, another hat bed), 2 the bridge (only a snare
 * roll into the last hook); the triplet bursts into gs.th (the 16T track's two bars) */
static void trap_drums(track_t *t, uint32_t len, uint32_t var)
{
    uint32_t i, b, k, bars = len / 16u, two = gs.trk_of[RL_TEX] < NTRK && gs.act[RL_TEX];
    uint8_t fig[16];                                    /* a figure a beat: four bars of four */
    uint32_t bed;
    int32_t base = 88 + (int32_t)gs.energy / 6;
    const char *kick;
    gseed(GS_DRUMS + 16u * var);
    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
    memset(gs.kick, 0, sizeof gs.kick);
    memset(gs.snare, 0, sizeof gs.snare);
    memset(gs.th, 0, sizeof gs.th);
    if (var == 2u) {                                    /* the bridge: the last bar's snare roll, louder to the hook */
        for (i = len - 8u; i < len; i++) {
            trap_hit(t, i, 1, 50 + (int32_t)(i - (len - 8u)) * 9, i == len - 1u);
            if (i >= len - 4u) step_set_ratchet(&t->step[i], i >= len - 2u ? 4u : 2u);
        }
        return;
    }
    k = gr() % 6u;
    if (var == 1u) k = (k + 1u + gr() % 5u) % 6u;       /* (the verse: never the hook's kick) */
    kick = TK_KICK[gs.ts][k];
    for (i = 0; i < len; i++) {                         /* the kick: two bars, again */
        char c = kick[i % 32u];
        if (c == '.' || (c == 'o' && gch(50))) continue;
        trap_hit(t, i, 0, c == 'X' ? 118 : 104, c == 'X');
    }
    for (b = 0; b < bars; b++) {                        /* the snare and the clap on 3; the pickups */
        uint32_t s3 = b * 16u + 8u;
        trap_hit(t, s3, 1, 118, 1);
        trap_hit(t, s3, 2, 118, 1);
        if (gs.ts == TS_DRILL && (b & 1u)) {            /* drill: the second bar's extra snare, late */
            trap_hit(t, b * 16u + (gch(60) ? 11u : 14u), 1, 100, 0);
        } else if ((b & 1u) && gch(b == 3u ? 40 : 20) && t->step[b * 16u + 15u].time != ST_NOTE) {
            trap_hit(t, b * 16u + 15u, 1, 78, 0);       /* (a pickup into the next bar) */
        }
    }
    if (gs.ts == TS_DRILL) {                            /* drill: an open hat on the bar's "and of 4" now and then, the
                                                         * percussion's gallop */
        const char *pc = TK_PERC[gr() % 3u];
        for (i = 0; i < len; i++) {
            if (i % 16u == 14u && gch(35)) trap_hit(t, i, 4, 90, 0);
            if (pc[i % 16u] == 'x' && gch(70)) trap_hit(t, i, 6, 84, 0);
        }
    } else if (gs.ts == TS_DARK) {                      /* dark trap: the cowbell */
        const char *cb = TK_BELL[gr() % 4u];
        for (i = 0; i < len; i++) if (cb[i % 16u] == 'x') trap_hit(t, i, 7, 96 + (i % 4u ? 0 : 12), !(i % 8u));
    } else if (gs.ts == TS_ATL && gch(30)) {            /* ATL: an open hat at the end of a phrase */
        trap_hit(t, len - 2u, 4, 92, 0);
    }
    /* the hats: a bed figure for the section, rolls before the snare and at the bar's end; a triplet figure the same
     * in bars 3-4 as in 1-2 (the 16T track loops two bars) */
    bed = gpickw(HF_BED[gs.ts], HF_N);
    if (var == 1u && gs.ts != TS_DRILL) bed = bed == HF_16 ? HF_8 : HF_16;   /* (the verse: the other bed) */
    for (k = 0; k < 16u; k++) {
        uint32_t beat = k % 4u, bar = k / 4u;
        fig[k] = (uint8_t)bed;
        if ((beat == 1u && gch(45 + gs.complex / 3u)) || (beat == 3u && (bar & 1u) && gch(70 + gs.complex / 4u)) ||
            (beat == 3u && !(bar & 1u) && gch(20 + gs.complex / 4u)))
            fig[k] = (uint8_t)gpickw(HF_ROLL[gs.ts], HF_N);
        if (bar >= 2u && (fig[k] == HF_T16 || fig[k] == HF_T8 || fig[k - 8u] == HF_T16 || fig[k - 8u] == HF_T8))
            fig[k] = fig[k - 8u];
    }
    for (k = 0; k < 16u && k * 4u < len; k++) {
        uint32_t s0 = k * 4u, j;
        if (gs.ts == TS_DRILL && fig[k] == HF_8) fig[k] = HF_16;
        switch (fig[k]) {
        case HF_8:
            trap_hit(t, s0, 3, base + 10, 0);
            trap_hit(t, s0 + 2u, 3, base - 14, 0);
            break;
        case HF_OFF:
            trap_hit(t, s0 + 2u, 3, base, 0);
            break;
        case HF_16: case HF_R32: case HF_R64:
            for (j = 0; j < 4u; j++) {
                uint32_t i2 = s0 + j;
                if (gs.ts == TS_DRILL && fig[k] == HF_16 && DRILL_BED[i2 % 16u] != 'x') continue;
                trap_hit(t, i2, 3, base + (j == 0u ? 12 : j == 2u ? 0 : -18 + grr(-6, 6)), 0);
                if (t->step[i2].hit & ~8u) continue;    /* (a ratchet rolls the whole step: never a kick, a snare ..) */
                if (fig[k] == HF_R32 && j >= 2u) { step_set_ratchet(&t->step[i2], 2); t->step[i2].vel = (uint8_t)clamp(base - 10 + (int32_t)j * 8, 30, 127); }
                if (fig[k] == HF_R64 && j == 3u) { step_set_ratchet(&t->step[i2], 4); t->step[i2].vel = (uint8_t)clamp(base + 6, 30, 127); }
            }
            break;
        case HF_T16: case HF_T8:
            if (two) {                                  /* the burst on the 16T track (a beat: six of its steps) */
                uint32_t tb = (k % 8u) * 6u;
                for (j = 0; j < 6u; j += fig[k] == HF_T8 ? 2u : 1u)
                    gs.th[tb + j] = (uint8_t)clamp(base - 20 + (int32_t)j * 7, 30, 127);
            } else {                                    /* (no second drum track: the beat's second half in triplet 32nds) */
                trap_hit(t, s0, 3, base + 10, 0);
                for (j = 2u; j < 4u; j++) {
                    trap_hit(t, s0 + j, 3, base - 6 + (int32_t)j * 4, 0);
                    if (!(t->step[s0 + j].hit & ~8u)) step_set_ratchet(&t->step[s0 + j], fig[k] == HF_T16 ? 3u : 2u);
                }
            }
            break;
        default: break;                                 /* HF_GAP: space */
        }
    }
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE) continue;
        gs.kick[i] = (uint8_t)(s->hit & 1u);
        gs.snare[i] = (uint8_t)((s->hit >> 1) & 1u);
        s->acc &= s->hit;
    }
}
/* the triplet bursts: the second DRUM track at 16T, two bars (48 of its steps), the hat lane only */
static void trap_hats2(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
    for (i = 0; i < 48u; i++) {
        if (!gs.th[i]) continue;
        t->step[i].time = ST_NOTE;
        t->step[i].hit = 1u << 3;
        t->step[i].vel = gs.th[i];
    }
    t->p[P_SDIV] = 5;                                   /* 16T */
}

/* ------------------------------------------------------------------------------------------------------ 808 --- */
/* a long note on every kick, held to the next kick (or the next chord), its pitch the root mostly, the octave and the
 * fifth at the turns (drill: a moving line, gliding nearly every change), slides between notes of different pitch */
static void trap_808(track_t *t, uint32_t len)
{
    static const uint8_t SLIDE[TS_N] = {34, 62, 38, 22}, JUMP[TS_N] = {26, 40, 18, 28};
    static const int8_t DRILL_MOVE[6] = {0, 4, 0, 7, 6, 3};     /* (scale steps over the root: drill's line) */
    int32_t lo = 30, prev = -1;                         /* (F#1 .. F2: over DIST's low cut at ~55 Hz, still a sub) */
    uint32_t i, start = 0, kicks = 0, q = 0;
    gseed(GS_BASS);
    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
    for (i = 0; i < len; i++) kicks += gs.kick[i];
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        int32_t root = gbass_root(t, lo, gchord_deg(i)), n[1], vel = 108;
        int head = i % 16u == 0u;
        if (kicks ? !gs.kick[i] && (i % gs.csteps || prev < 0) : i % 8u) {
            if (prev >= 0) dstep_tie(s);
            continue;
        }
        n[0] = root;
        if (!head && gch(JUMP[gs.ts])) {
            if (gs.ts == TS_DRILL) n[0] = scale_steps(t, root, DRILL_MOVE[q++ % 6u]);
            else n[0] = gch(65) ? root + 12 : scale_steps(t, root, 4);
        }
        if (head) vel = 118;
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = (uint8_t)clamp(n[0], 0, 127);
        s->vel = (uint8_t)vel;
        s->flags = head ? SF_ACCENT : 0u;
        if (prev >= 0 && n[0] != prev && gch(SLIDE[gs.ts])) t->step[start].flags |= SF_SLIDE;
        prev = n[0];
        start = i;
    }
}

/* ------------------------------------------------------------------------------------------- the melodies --- */
/* the melody's rhythm, two bars, a style: 'x' an onset */
static const char *const TM_RHY[TS_N][5] = {
    [TS_ATL]   = {"x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.x..x..x.x..x..x.", "x.....x...x.....x...x.x.x.......",
                  "x.x...x.x...x...x.x...x.x...x.x.", "x...x...x.x.x...x...x...x...x.x."},
    [TS_DRILL] = {"x.......x...x...x.......x.x.x...", "x..x..x...x.....x..x..x...x.....", "x...x.x.....x...x...x.x...x.x...",
                  "x.....x...x.x...x.....x...x.....", "x..x..x...x.....x..x..x.x.x....."},
    [TS_DARK]  = {"x..x..x.x.x.x...x..x..x.x.x.x.x.", "x.x.x..x.x..x.x.x.x.x..x.x..x.x.", "x..x..x..x..x.x.x..x..x..x..x.x.",
                  "x.x.x.x.x..x.x.x.x.x.x.x.x..x.x.", "x..x..x.x.x.x...x..x..x.x.x....."},
    [TS_RAGE]  = {"x.x.x..x.x.x..x.x.x.x..x.x.x..x.", "x..x..x.x..x..x.x..x..x.x..x.x.x", "xx.x.x..xx.x.x..xx.x.x..x.x.x.x.",
                  "x.x.x.x.x.x.x.x.x.x.x.x.xx.x.x.x", "x..x..x.x..x..x.x..x..x.x..x..x."},
};
/* the melody's cell: degrees over the chord's root (0 root, 2 third, 4 fifth, 7 octave, -1 the leading tone below,
 * 1 the second: phrygian's flat two), repeated over the onsets */
static const int8_t TM_CELL[10][6] = {
    {0, 4, 7, 4, 99}, {0, 2, 4, 2, 99}, {7, 4, 2, 4, 99}, {0, 4, 2, 4, 7, 4}, {4, 2, 0, -1, 99},
    {0, 0, 4, 2, 99}, {2, 4, 7, 4, 99}, {0, 1, 0, 4, 99}, {7, 6, 4, 2, 99}, {0, 4, 6, 4, 2, 99},
};
/* a note from a degree over the chord, in the window [lo, hi], the octave nearest the last */
static int32_t trap_mnote(const track_t *t, int32_t deg, int32_t lo, int32_t hi, int32_t *last)
{
    int32_t n = gdeg(t, lo, deg);
    while (n > hi) n -= 12;
    while (n < lo) n += 12;
    if (*last) {
        if (n - *last > 7 && n - 12 >= lo) n -= 12;
        else if (*last - n > 7 && n + 12 <= hi) n += 12;
    }
    *last = n;
    return n;
}
static void trap_put(step_t *s, int32_t n, int32_t vel, uint32_t fl)   /* (no softening of high notes: bells sing up there) */
{
    gclear(s);
    s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)clamp(n, 0, 127);
    s->vel = (uint8_t)clamp(vel, 30, 127); s->flags = (uint8_t)fl;
}
/* the lead: a two-bar rhythm, a cell of chord tones repeated over it, moved with the chord; the phrase's last bar
 * varied (the cell turned up, the last note held) */
static void trap_lead(track_t *t, uint32_t len)
{
    const char *rh;
    const int8_t *cell;
    uint32_t i, q = 0, nc = 0, a = gs.amt[GP_LEAD];
    int32_t last = 0, lo = gs.mlo, hi = gs.mhi, up = gch(50) ? 2 : 4;
    gseed(GS_LEAD);
    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
    rh = TM_RHY[gs.ts][gr() % 5u];
    cell = TM_CELL[gr() % 10u];
    if (cell == TM_CELL[7] && gs.scale != SC_PHRY && gs.scale != SC_HARM) cell = TM_CELL[0];
    while (nc < 6u && cell[nc] != 99) nc++;
    for (i = 0; i < len; i++) {
        uint32_t bar = i / 16u;
        int32_t ch = gchord_deg(i), d;
        if (rh[i % 32u] != 'x') continue;
        if (a < 70u && i % 4u && gr() % 100u >= 40u + a) { q++; continue; }   /* (less amount: space) */
        d = cell[q % nc] + ch;
        if (bar % 4u == 3u && i % 16u >= 8u) d += up;    /* (the phrase's last half bar: up, to turn it round) */
        trap_put(&t->step[i], trap_mnote(t, d, lo, hi, &last), i % 4u ? 86 : 102, 0);
        q++;
    }
    t->p[P_SGATE] = (int16_t)(gs.ts == TS_DARK ? 45 : gs.ts == TS_RAGE ? 70 : 90);
}
/* the counter line: the chord's root and fifth an octave or two under the melody, on the beat's halves, sparse */
static void trap_counter(track_t *t, uint32_t len)
{
    static const char *const CR[4] = {"x.......x.......", "x.....x.........", "x.......x...x...", "x...........x..."};
    const char *r;
    uint32_t i;
    int32_t last = 0;
    gseed(GS_ARP);
    for (i = 0; i < NSTEP; i++) gclear(&t->step[i]);
    r = CR[(gr() >> 9) % 4u];
    for (i = 0; i < len; i++) {
        if (r[i % 16u] != 'x') continue;
        trap_put(&t->step[i], trap_mnote(t, gchord_deg(i) + (i % 16u ? 4 : 0), 60, 76, &last), 90, 0);
        dhold(t, i, 3, len);
    }
    t->p[P_SGATE] = 100;
}

/* ---------------------------------------------------------------------------------------------- the sounds --- */
/* a sound by its preset's name on an engine (the first word that fits), the engine's first otherwise */
static void trap_preset(track_t *t, uint32_t eng, const char *w1, const char *w2)
{
    const engine_t *en;
    uint32_t i, n;
    set_engine_of(t, eng);
    en = ENGINES[t->eng_req % NENGINES];
    n = en->npresets;
    if (!n) return;
    for (i = 0; i < n; i++) if (dname_has(en->presets[i].name, w1)) { apply_preset_to(t, i); return; }
    if (w2) for (i = 0; i < n; i++) if (dname_has(en->presets[i].name, w2)) { apply_preset_to(t, i); return; }
    apply_preset_to(t, 0);
}
typedef struct { uint8_t eng; const char *w; int8_t lo, hi; } tsound_t;   /* (lo, hi: the melody's window) */
static const tsound_t TS_LEAD[TS_N][4] = {
    [TS_ATL]   = {{ENGI_FM6, "BELL", 72, 88}, {EG_PH, "CRYPT BELL", 72, 88}, {TG_WH, "FLUTE", 69, 86}, {ENGI_FM6, "TINE EP", 67, 84}},
    [TS_DRILL] = {{EG_PH, "STRING", 60, 79}, {EG_A, "STRINGS", 62, 81}, {ENGI_FM6, "TINE EP", 64, 81}, {EG_PH, "CRYPT BELL", 67, 84}},
    [TS_DARK]  = {{ENGI_FM6, "BELL", 72, 86}, {EG_PH, "BELL", 72, 86}, {EG_A, "RING BELL", 72, 86}, {ENGI_FM6, "MARIMBA", 67, 84}},
    [TS_RAGE]  = {{EG_A, "SAW LEAD", 64, 84}, {EG_A, "RAVE", 64, 84}, {EG_A, "SYNC LEAD", 64, 84}, {EG_A, "PWM STR", 60, 81}},
};
static const tsound_t TS_PAD[TS_N][2] = {
    [TS_ATL]   = {{EG_A, "SOFT PAD", 0, 0}, {EG_VO, "CHOIR AAH", 0, 0}},
    [TS_DRILL] = {{EG_VO, "BLACK CHOIR", 0, 0}, {EG_A, "STRINGS", 0, 0}},
    [TS_DARK]  = {{EG_VO, "BLACK CHOIR", 0, 0}, {EG_A, "WRAITH PAD", 0, 0}},
    [TS_RAGE]  = {{EG_A, "PWM STR", 0, 0}, {EG_A, "SOFT PAD", 0, 0}},
};
static const tsound_t TS_CTR[TS_N][2] = {
    [TS_ATL]   = {{ENGI_FM6, "PLUCK", 0, 0}, {EG_A, "PLUCK", 0, 0}},
    [TS_DRILL] = {{ENGI_FM6, "TINE EP", 0, 0}, {EG_A, "SINE KEY", 0, 0}},
    [TS_DARK]  = {{TG_WH, "SOFT FLUTE", 0, 0}, {ENGI_FM6, "TINE EP", 0, 0}},
    [TS_RAGE]  = {{EG_A, "RAVE", 0, 0}, {EG_A, "PLUCK", 0, 0}},
};
/* the kit: a trap kit on top of the generic one (gen_kit's): the hats high-passed, bright and short, the kick a short
 * knock (the 808 is the boom), the snare and the clap snappy, the cowbell; the track's filter open, the hats loud */
static void trap_kit(track_t *t)
{
    static const uint8_t P[8][5] = {                    /* PITCH PMOD DECAY NOISE COLOR */
        {8, 76, 40, 2, 40}, {50, 14, 46, 110, 104}, {58, 0, 48, 127, 96}, {104, 0, 12, 120, 124},
        {104, 0, 54, 120, 120}, {30, 40, 50, 10, 30}, {74, 0, 10, 40, 110}, {68, 0, 40, 6, 64},
    };
    uint32_t l, i;
    for (l = 0; l < 8u; l++) {
        dx_lane_t *L = &dx_kit[l];
        if (l == DV_HATC || l == DV_HATO) L->mode = DX_KIT_DEF[l].mode;   /* (high-passed, as the factory's) */
        for (i = 0; i < DXP_N; i++) {
            int32_t v = (int32_t)P[l][i] + grr(-4, 4);
            if (gs.ts == TS_RAGE && l == DV_KICK && i == DXP_COLOR) v += 30;   /* (rage: a driven knock) */
            L->a[i] = L->b[i] = (uint8_t)clamp(v, 0, 127);
        }
    }
    dx_tone = 0;
    t->p[P_E0] = 0;                                     /* MORPH (A and B are the same: no drift) */
    t->p[P_E2] = (int16_t)grr(84, 98);                  /* TONE */
    t->p[P_E3] = 64;
    t->p[P_E4] = (int16_t)grr(64, 74);
    t->p[P_E5] = (int16_t)grr(6, 18);                   /* FM ENV: a little click */
    t->p[P_E6] = (int16_t)(gs.ts == TS_RAGE ? grr(10, 30) : grr(0, 8));
    t->p[P_E7] = (int16_t)(gs.ts == TS_RAGE ? grr(40, 70) : gs.ts == TS_DARK ? grr(25, 45) : grr(8, 24));
    t->p[P_FTYPE] = 0; t->p[P_FCUT] = 127; t->p[P_FRES] = 0;
    t->p[P_LN0] = 112; t->p[P_LN1] = 118; t->p[P_LN2] = 122; t->p[P_LN3] = 124;
    t->p[P_LN4] = 108; t->p[P_LN5] = 100; t->p[P_LN6] = 104; t->p[P_LN7] = 118;
}
/* the 808: a sine, driven (dark trap and rage: clipped), a glide, held */
static void trap_808_sound(track_t *t)
{
    static const uint8_t DRV[TS_N][2] = {{84, 108}, {88, 112}, {110, 127}, {114, 127}};   /* (1x .. 3x into a soft clip:
                                                         * the harmonics a phone speaker plays) */
    trap_preset(t, EG_A, gch(60) ? "INGOT SUB" : "SUB BASS", "SUB BASS");
    t->p[P_E0] = 3;                                     /* WAVE: SIN */
    t->p[P_E1] = 0; t->p[P_E2] = 0; t->p[P_E3] = 0;     /* DETUNE MIX NOISE */
    t->p[P_E4] = (int16_t)grr(100, 127);                /* CUT open */
    t->p[P_E5] = (int16_t)grr(0, 8);
    t->p[P_E6] = (int16_t)grr(DRV[gs.ts][0], DRV[gs.ts][1]);   /* DRIVE */
    t->p[P_E7] = 0;                                     /* INT */
    t->p[P_FTYPE] = 0;
    t->p[P_ATK] = 0; t->p[P_DEC] = (int16_t)grr(96, 112); t->p[P_SUS] = (int16_t)grr(104, 118);
    t->p[P_REL] = (int16_t)(gs.ts == TS_RAGE ? grr(30, 45) : grr(40, 60));
    t->p[P_GLIDE] = (int16_t)(gs.ts == TS_DRILL ? grr(38, 52) : grr(28, 42));
    t->p[P_VOICE] = V_LEGATO;
    t->p[P_DIST] = (int16_t)(gs.ts == TS_DARK || gs.ts == TS_RAGE ? grr(76, 98) : grr(52, 68));   /* (1.6x .. 5x)
    t->p[P_DTYPE] = (int16_t)(gs.ts == TS_RAGE ? 1 : 0);   /* (SOFT; rage HARD) */
    t->p[P_CHOR] = 0;
    t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_AMP] = 0;
}
/* a role's sound in the trap family */
static void trap_sound(track_t *t, uint32_t r)
{
    const tsound_t *s;
    switch (r) {
    case RL_DRUM: gen_kit(t); trap_kit(t); return;
    case RL_BASS: trap_808_sound(t); return;
    case RL_LEAD:
        s = &TS_LEAD[gs.ts][gr() % 4u];
        trap_preset(t, s->eng, s->w, 0);
        gs.mlo = s->lo; gs.mhi = s->hi;
        t->p[P_VOICE] = gs.ts == TS_RAGE ? V_MONO : V_POLY;   /* (a bell, a key rings on under the next) */
        if (gs.ts == TS_DARK) { t->p[P_DEC] = (int16_t)grr(40, 58); t->p[P_SUS] = 0; t->p[P_REL] = (int16_t)grr(30, 45); }   /* (a cowbell's) */
        if (gs.ts == TS_RAGE) { t->p[P_DETUNE] = (int16_t)grr(40, 70); t->p[P_CHOR] = (int16_t)grr(40, 80); }
        return;
    case RL_PAD:
        s = &TS_PAD[gs.ts][gr() % 2u];
        trap_preset(t, s->eng, s->w, 0);
        return;
    case RL_ARP:
        s = &TS_CTR[gs.ts][gr() % 2u];
        trap_preset(t, s->eng, s->w, 0);
        t->p[P_VOICE] = V_MONO;
        return;
    default:                                            /* the triplet hats: a second DRUM track (the kit is the project's) */
        set_engine_of(t, ENGI_DRUM);
        return;
    }
}
/* the mix: the 808 and the kick up front and dry, the hats bright, the melody high and wet, the pad underneath */
static void trap_mix(track_t *t, uint32_t r)
{
    t->p[P_PAN] = 0;
    t->p[P_DLY] = 0;
    switch (r) {
    case RL_DRUM: t->p[P_LEVEL] = 110; t->p[P_REV] = (int16_t)grr(4, 10); break;
    case RL_BASS: t->p[P_LEVEL] = (int16_t)(gs.ts == TS_RAGE ? 98 : 102); t->p[P_REV] = 0; break;
    case RL_LEAD:
        t->p[P_LEVEL] = (int16_t)(gs.ts == TS_RAGE ? 108 : 114);
        t->p[P_REV] = (int16_t)grr(45, 80); t->p[P_DLY] = (int16_t)grr(20, 45);
        t->p[P_PAN] = (int16_t)grr(-6, 6);
        break;
    case RL_PAD: t->p[P_LEVEL] = (int16_t)grr(76, 86); t->p[P_REV] = (int16_t)grr(70, 100); break;
    case RL_ARP: t->p[P_LEVEL] = (int16_t)grr(92, 102); t->p[P_REV] = (int16_t)grr(30, 55); t->p[P_PAN] = (int16_t)grr(-20, -8); break;
    default: t->p[P_LEVEL] = 104; t->p[P_REV] = (int16_t)grr(4, 10); break;   /* (the triplet hats) */
    }
}
/* the triplet track: the drum track's kit settings, the hats only */
static void trap_hat_track_like(track_t *h, const track_t *d)
{
    uint32_t i;
    for (i = 0; i < 8u; i++) h->p[P_E0 + i] = d->p[P_E0 + i];
    h->p[P_FTYPE] = d->p[P_FTYPE]; h->p[P_FCUT] = d->p[P_FCUT]; h->p[P_FRES] = d->p[P_FRES];
    for (i = 0; i < 8u; i++) h->p[P_LN0 + i] = i == 3u ? d->p[P_LN3] : 0;
}
/* the master: no pump (the kick and the 808 start together), clipped where the genre is (dark trap, rage) */
static void trap_master(void)
{
    song.g[G_SWING] = (int16_t)(gs.ts == TS_DRILL ? grr(4, 10) : 0);
    song.g[G_DUCK] = 0;
    song.g[G_CLIP] = (int16_t)(gs.ts == TS_RAGE ? grr(60, 90) : gs.ts == TS_DARK ? grr(45, 70) : grr(25, 45));
    song.g[G_PUNCH] = (int16_t)grr(40, 70);
    song.g[G_DTIME] = 1;                                /* 1/8 */
    song.g[G_DFDBK] = (int16_t)grr(25, 45);
}

/* ------------------------------------------------------------------------------------------------ the song --- */
static uint8_t tk_th[48];                               /* the hook's triplet hats (its drums and 808: gen.c's gen_keep) */
/* the last beat silent on the drums, the hats and the 808: the stop before the hook */
static void trap_stop(track_t *t, uint32_t len)
{
    uint32_t i;
    for (i = len - 4u; i < len; i++) gclear(&t->step[i]);
}
/* the sections: A the hook (the piece as made), B the intro (the melody and the pad), C the verse (another kick and
 * hat bed, its own 808, the melody out, the counter in), D the bridge (no drums but a snare roll, no 808, the melody
 * wetter); the chain: intro, hook, verse, hook (PARTS), then bridge, hook (SONG) */
static void trap_parts(void)
{
    uint32_t dr = gs.trk_of[RL_DRUM], bs = gs.trk_of[RL_BASS], pd = gs.trk_of[RL_PAD], ld = gs.trk_of[RL_LEAD];
    uint32_t ar = gs.trk_of[RL_ARP], hx = gs.act[RL_TEX] ? gs.trk_of[RL_TEX] : NTRK;
    uint32_t k, i, n = 0, rows = 0, len = gs.len;
    static const uint8_t ORD[6] = {1, 0, 2, 0, 3, 0}, BARS[6] = {4, 8, 8, 8, 4, 8};
    uint32_t nord = gp.form == 2u ? 6u : 4u;
    gseed(GS_FORM);
    for (k = 0; k < 4u; k++)
        if (project_used(k) && !((dice_secs >> k) & 1u)) return;   /* (a section of yours: kept) */
    live.cur = -1;
    section_store(0);                                   /* A: the hook */
    if (live.cur != 0) return;
    dice_secs |= 1u;
    if (dr < NTRK) memcpy(gen_keep[0], trk[dr].step, sizeof gen_keep[0]);
    if (bs < NTRK) memcpy(gen_keep[1], trk[bs].step, sizeof gen_keep[1]);
    memcpy(tk_th, gs.th, sizeof tk_th);
    for (k = 1; k < 4u; k++) {
        for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = 0;
        if (k == 1u) {                                  /* B INTRO */
            if (dr < NTRK) trk[dr].p[P_MUTE] = 1;
            if (bs < NTRK) trk[bs].p[P_MUTE] = 1;
            if (hx < NTRK) trk[hx].p[P_MUTE] = 1;
            if (ar < NTRK) trk[ar].p[P_MUTE] = 1;
            if (ld < NTRK && gs.act[RL_LEAD]) {         /* (the melody's filter opening over the intro) */
                track_t *t = &trk[ld];
                uint32_t id = t->eng_req == EG_A ? P_E4 : P_FCUT, base = (uint32_t)clamp(t->p[id], 0, 127), j;
                for (j = 0; j < 4u; j++)
                    motion_set_event(t, j * len / 4u, id, (int16_t)clamp((int32_t)base - 56 + (int32_t)j * 18, 20, 127));
            }
        } else if (k == 2u) {                           /* C VERSE */
            if (dr < NTRK && gs.act[RL_DRUM]) {
                track_t *t = &trk[dr];
                load_begin(t, LOAD_PAT); trap_drums(t, len, 1); trap_stop(t, len); load_end(t);
                if (hx < NTRK) { load_begin(&trk[hx], LOAD_PAT); trap_hats2(&trk[hx]); trap_stop(&trk[hx], 48); load_end(&trk[hx]); }
                if (bs < NTRK && gs.act[RL_BASS]) { load_begin(&trk[bs], LOAD_PAT); trap_808(&trk[bs], len); trap_stop(&trk[bs], len); load_end(&trk[bs]); }
            }
            if (ld < NTRK) trk[ld].p[P_MUTE] = 1;
        } else {                                        /* D BRIDGE */
            if (dr < NTRK && gs.act[RL_DRUM]) { load_begin(&trk[dr], LOAD_PAT); trap_drums(&trk[dr], len, 2); load_end(&trk[dr]); }
            if (bs < NTRK) trk[bs].p[P_MUTE] = 1;
            if (hx < NTRK) trk[hx].p[P_MUTE] = 1;
            if (ar < NTRK) trk[ar].p[P_MUTE] = 1;
            if (ld < NTRK) motion_set_event(&trk[ld], 0, P_REV, (int16_t)clamp(trk[ld].p[P_REV] + 30, 0, 127));
        }
        if (pd < NTRK && k == 2u && gch(50)) trk[pd].p[P_MUTE] = 1;   /* (a verse without the pad, now and then) */
        section_store(k);
        if (live.cur != (int8_t)k) { n = k; break; }
        dice_secs |= (uint8_t)(1u << k);
        n = k + 1u;
        if (dr < NTRK) { load_begin(&trk[dr], LOAD_PAT); memcpy(trk[dr].step, gen_keep[0], sizeof gen_keep[0]); load_end(&trk[dr]); }
        if (bs < NTRK) { load_begin(&trk[bs], LOAD_PAT); memcpy(trk[bs].step, gen_keep[1], sizeof gen_keep[1]); load_end(&trk[bs]); }
        if (hx < NTRK) { memcpy(gs.th, tk_th, sizeof tk_th); load_begin(&trk[hx], LOAD_PAT); trap_hats2(&trk[hx]); load_end(&trk[hx]); }
        if (ld < NTRK) { load_begin(&trk[ld], LOAD_PAT); motion_clear(&trk[ld]); load_end(&trk[ld]); }
    }
    section_load(0);
    live.cur = 0;
    if (n < 2u) return;
    chain_defaults(&chain_config);
    for (k = 0; k < nord && rows < CHAIN_ROWS; k++) {
        if (ORD[k] >= n) continue;
        chain_config.row[rows].slot = ORD[k];
        chain_config.row[rows].bars = BARS[k];
        rows++;
    }
    chain_config.count = (uint8_t)rows;
    SONG_END(&chain_config) = SONG_FOREVER;
    ui.song_row = 0;
    live.mode = 1;
}
