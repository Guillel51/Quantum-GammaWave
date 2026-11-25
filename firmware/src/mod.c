/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Modulation matrix: per track EIGHT slots, each SRC -> DST by AMT (-64..63), added to the fixed routings
 * (ENV DEST, LFO DEST). Runs in the audio ISR (fx.c mix_part, voice.c track_render). Slots 1..4 are the stored
 * parameters P_M1SRC..; 5..8 are track_t.mx (core.h, docs/MOD_MATRIX_SLOTS.md): the parameter ids are 7 bits and
 * the table is full at 125 of 127, so they have none and are not automatable. mslot() reads either.
 *
 * Sources, Q15:
 *   LFO   the track LFO with its FADE (as LFO DEST), bipolar
 *   ENV   the amp envelope of the voice (after the engine's own amp curve, as ENV DEST), 0..1
 *   VEL   the note-on velocity (before UNISON's level scaling), 0..1
 *   KEY   the note relative to C4, bipolar: +-64 semitones = +-1 (an octave = 0.19)
 *   RAND  a new random value per note-on (its own generator: the shared rng is not touched), bipolar
 *   MODW  MIDI CC1, AT channel aftertouch, EXPR CC11 of the track's channel (seq.c), 0..1; until a
 *         controller arrives (and after CC121 RESET ALL CONTROLLERS) MODW and AT are 0, EXPR 1
 *   STEP  (JIANT) the track's modulation sequence (LFO > MSEQ, a CV sequencer): the level of its step now (LEN steps of
 *         DIV from PLAY; stopped: where it was), 0..1, slewed by SLEW (ms_tick, once a block when a slot reads it)
 *   M1..M4 (JIANT) the project's macros (macro_v, the knobs with LFO held), 0..1: the same for every track, so a
 *         macro reaches as many destinations as the slots that name it
 * Destinations:
 *   per voice (vmod_t, like the fixed routings):
 *     PITCH  AMT 63 at a full source = +-11.8 semitones (as LFO DEST PIT)
 *     CUT    m->cutoff, SHP m->shape: AMT 63 = +-63 steps (as ENV DEST); the sums are clamped to what
 *            the fixed routings can reach, so the engines see no new range
 *     AMP    a gain <= 1: AMT > 0 the source opens it (AMT 64: silent at 0, full at 1), AMT < 0 closes it
 *            (bipolar sources as 0..1, as LFO DEST AMP); several AMP slots multiply
 *   per track, once per block: PAN, DIST CHO DLY REV (the sends), RATE (LFO rate), VIB (LFO DEST PIT, the
 *     vibrato depth), E1..E8 (the engine's parameters P_E0..P_E7, named by the engine): AMT 64 at a full
 *     source = the parameter's whole range; the sum is clamped to its range
 *   the master (JIANT): CLIP and PNCH (G_CLIP, G_PUNCH, FX > MASTER), from any track's slot, AMT 64 at a full
 *     source = 0..100; the slots of all tracks add (macro_master, once an audio block)
 * A per-voice source on a per-block destination takes the latest note-on's value (VEL, KEY, RAND), ENV the
 * envelope of the latest note-on's voice (one block late, 0 when it has ended).
 *
 * Per-block destinations are modulated in place: mod_begin writes the modulated values into t->p for the
 * part's block (its render and its mix in fx.c mix_part) and mod_end puts the stored values back. Both run
 * in the audio ISR, so nothing outside it ever sees a modulated value (the UI, the editor, projects and
 * presets read and write t->p from the main loop, which the ISR preempts; TIMER5 only fills the MIDI rings),
 * and the engines read t->p as before. An engine reads its parameters in note_on (events_block) unmodulated.
 * Nothing active (every slot with SRC, DST or AMT at 0): no work, and the sound is bit-identical. */
enum { MS_OFF, MS_LFO, MS_ENV, MS_VEL, MS_KEY, MS_RAND, MS_MODW, MS_AT, MS_EXPR, MS_M1, MS_STEP = MS_M1 + 4, MS_NOISE,
       MS_PAT, MS_BAR, MS_N };   /* (1.2.1-GW, JIANT 0.7.8) NOISE: appended, the stored numbers stay; (QG-1 1.6) PAT: this
                           * key's polyphonic aftertouch (MIDI 0xA0), appended too; (FASE 5) BAR: the bar's phase */
static uint32_t div_samples(uint32_t div);   /* fx.c */
#define CLK_START 0xFFFFFFFFu
static uint32_t clk_pos = CLK_START, clk_step;  /* (seq.c click_tick) samples into the transport's 1/16 step, the step of
                                                 * the bar (0..15); after events_block: the block's start */
static uint32_t clk_n;                          /* (JIANT) .. the 1/16 steps since PLAY (the SLICER's and the FX layer's grid) */
static uint32_t ms_clock;        /* (JIANT) samples since PLAY on the transport's grid (fx.c mix_block; seq.c seq_start:
                                  * 0): MS_STEP's place */
enum { MD_OFF, MD_PITCH, MD_CUT, MD_SHP, MD_AMP, MD_PAN, MD_DIST, MD_CHO, MD_DLY, MD_REV, MD_RATE, MD_VIB, MD_E1,
       MD_CLIP = MD_E1 + 8, MD_PNCH, MD_X1, MD_X2, MD_X3, MD_X4, MD_RES, MD_ATK, MD_DEC, MD_REL, MD_HARM, MD_LSHP, MD_N };   /* (FASE 2) X1..X4: the engine CHARACTER
                                                          * bank, appended after the master so every stored
                                                          * destination number keeps its meaning */
static uint8_t macro_v[4];       /* (JIANT) the macros M1..M4, 0..127: the project's (project.c), LFO held turns them */
/* (FASE 8) the MACROS page reads and writes them through these: the same values the matrix's sources M1..M4 read,
 * and the ones project_capture saves. */
static uint32_t macro_get(uint32_t i) { return macro_v[i & 3u]; }
static void macro_set(uint32_t i, int32_t v) { macro_v[i & 3u] = (uint8_t)clamp(v, 0, 127); }
_Static_assert(NELEM(N_MSRC) == MS_N && NELEM(N_MDST) == MD_N, "N_MSRC / N_MDST == MS_* / MD_*");
_Static_assert(P_M2SRC == P_M1SRC + 3 && P_M4AMT == P_M1SRC + 11, "matrix slots: 3 ids each");
/* (FASE 8) EIGHT SLOTS A TRACK. 1..4 are still the stored parameters P_M1SRC.. (automatable, in every store as
 * before); 5..8 live in t->mx (core.h), because the parameter ids are 7 bits (the motion records) and the table is
 * full at 125 of 127: a slot needs three ids and there are two. f: 0 SRC, 1 DST, 2 AMT */
#define NMSLOT 8u
static int32_t mslot(const track_t *t, uint32_t k, uint32_t f)
{
    return k < 4u ? (int32_t)t->p[P_M1SRC + 3u * k + f] : (int32_t)t->mx[k - 4u][f];
}
/* (QG-1 1.6) the numbers core.h hands a preset's x[] (QG_SRC_* / QG_DST_*): keep both lists in step */
_Static_assert(QG_SRC_VEL == MS_VEL && QG_SRC_AT == MS_AT && QG_SRC_PAT == MS_PAT, "QG_SRC_* == MS_*");
_Static_assert(QG_SRC_N == MS_N && QG_DST_N == MD_N, "QG_SRC_N / QG_DST_N == MS_N / MD_N");
_Static_assert(QG_DST_CUT == MD_CUT && QG_DST_SHP == MD_SHP && QG_DST_REV == MD_REV && QG_DST_VIB == MD_VIB &&
               QG_DST_E1 == MD_E1, "QG_DST_* == MD_*");
#define MS_VOICE(s) (((s) >= MS_ENV && (s) <= MS_RAND) || (s) == MS_PAT)   /* a value per voice */
#define MS_BIPOLAR(s) ((s) == MS_LFO || (s) == MS_KEY || (s) == MS_RAND || (s) == MS_NOISE)

/* the track parameter of a per-block destination */
static uint32_t mod_param(uint32_t d)
{
    static const uint8_t ID[MD_E1] = {[MD_PAN] = P_PAN, [MD_DIST] = P_DIST, [MD_CHO] = P_CHOR, [MD_DLY] = P_DLY,
                                      [MD_REV] = P_REV, [MD_RATE] = P_LRATE, [MD_VIB] = P_LD_PIT};
    if (d >= MD_X1 && d <= MD_X4)
        return P_XID[d - MD_X1];                    /* (FASE 2) CHARACTER 1..4 */
    if (d == MD_RES)                                /* (FASE 3) the part filter's resonance */
        return P_FRES;
    if (d == MD_LSHP)                               /* (v1.12.5) the LFO's curve: a block's offset moves it as it
                                                     * moves any other parameter, and mod_end restores it */
        return P_LSHP;
    if (d == MD_HARM)                               /* (v1.12.4) TILT: a block's offset moves the tilt, as ATK/DEC/REL
                                                     * move the envelope's times: voice.c reads p[P_HARM] and hands it
                                                     * to the voice, and mod_end puts the stored value back */
        return P_HARM;
    if (d >= MD_ATK)                                /* (FASE 5) the envelope's three times: the shaper reads p[] every
                                                     * tick, so a block's offset moves ATK, DEC and REL like any other
                                                     * parameter, and mod_end puts the stored values back */
        return d == MD_ATK ? P_ATK : d == MD_DEC ? P_DEC : P_REL;
    return d >= MD_E1 ? P_E0 + d - MD_E1 : ID[d];
}

typedef struct {                 /* the part in mix_part (one at a time) */
    uint8_t on;                  /* a slot is active: track_render calls mod_voice */
    uint8_t amp;                 /* an AMP slot: the voice gain applies */
    uint8_t nv, nk;              /* voice terms, per-block parameters held */
    int32_t pit, cut, shp, gain; /* per voice, from the per-track sources (gain Q15) */
    uint8_t vsrc[NMSLOT], vdst[NMSLOT];
    int8_t vamt[NMSLOT];
    uint8_t kid[NMSLOT];         /* per-block: the parameter, its stored value */
    int16_t keep[NMSLOT];
    uint32_t sc_mask;            /* (v1.8.1) the per-track sources already resolved for this block: bit s */
    int32_t sc[MS_N];            /* .. and their values, 72 B a core */
} mod_t;
static mod_t mod_c[2];                      /* (JIANT 0.6.71) one a core: each renders its own parts at once (fx.c mix_block) */
#define MOD (mod_c[MC_CPU() & 1u])

static uint32_t mod_seed = 0x2545F491u;
static int16_t mod_rand(void)    /* RAND of a note-on: xorshift32, bipolar */
{
    mod_seed ^= mod_seed << 13;
    mod_seed ^= mod_seed >> 17;
    mod_seed ^= mod_seed << 5;
    return (int16_t)(mod_seed >> 16);
}

static int32_t mod_key(uint32_t note) { return clamp(((int32_t)note - 60) * 512, -32767, 32767); }

/* (1.2.1-GW, JIANT 0.7.8) a smooth value noise, -32768..32767: a new random point every len steps of clk, eased between
 * (smoothstep); seed picks the stream. No state: NOISE's per track (mod_tsrc) */
static int32_t mod_noise(uint32_t seed, uint32_t clk, uint32_t len)
{
    uint32_t k = clk / len, f = (clk % len) * 256u / len, a = (seed + k) * 2654435761u, b = (seed + k + 1u) * 2654435761u;
    int32_t ya, yb;
    a ^= a >> 15; a *= 2246822519u; a ^= a >> 13;
    b ^= b >> 15; b *= 2246822519u; b ^= b >> 13;
    ya = (int32_t)(a >> 16) - 32768;
    yb = (int32_t)(b >> 16) - 32768;
    f = f * f * (768u - 2u * f) >> 16;                  /* smoothstep, 0..256 */
    return ya + (((yb - ya) * (int32_t)f) >> 8);
}
static uint32_t mns_clk;                                 /* NOISE's clock: audio blocks (macro_master) */

/* (3.5) N.RATE: how many blocks a point of the random wander holds. 0 is the 64 blocks (~46 ms) it always had; the
 * rest are the transport's own grid, so the wander lands on the beat instead of drifting against it. */
static uint32_t mod_nlen(const track_t *t)
{
    static const uint8_t SIX[5] = {16, 8, 4, 2, 1};   /* 1/1, 1/2, 1/4, 1/8, 1/16 in 1/16 steps */
    uint32_t v = (uint32_t)clamp(t->p[P_NRATE], 0, 5), n;
    if (v >= 5u)
        return 64u;                                    /* the free 46 ms (params.c N_NRATE's last) */
    n = (div_samples(2u) * SIX[v] + 16u) / 32u;          /* the 1/16 in samples -> blocks of 32 */
    return n ? n : 1u;
}
#ifdef MOD_TSRC_COUNT
/* (v1.8.1, tests only) how many times a block resolves a per-track source: a count, so the test that guards the
 * per-block cache cannot flake on a busy host. Nothing at all when the macro is off. */
uint32_t mod_tsrc_n;
#endif
/* source s of the track: the per-track ones, the per-voice ones of the latest note-on */
static __attribute__((noinline)) int32_t mod_tsrc(const track_t *t, uint32_t s, int32_t lfo)
{
#ifdef MOD_TSRC_COUNT
    mod_tsrc_n++;
#endif
    switch (s) {
    case MS_LFO:
        return lfo;
    case MS_ENV:
        return t->m_env;
    case MS_VEL:
        return t->m_vel * 258;
    case MS_KEY:
        return mod_key(t->m_key);
    case MS_RAND:
        return t->m_rnd;
    case MS_MODW:
        return t->mw * 258;
    case MS_AT:
        return t->at * 258;
    case MS_EXPR:                            /* (FASE 4) expression or breath, whichever asks for less */
        return (127 - (t->ex_off > t->br_off ? t->ex_off : t->br_off)) * 258;
    case MS_STEP:
        return t->ms_v;
    case MS_BAR: {                                      /* (FASE 5) the bar's phase: a ramp from the downbeat to the
                                                         * next one, 0..1, on the transport's grid. It costs no
                                                         * voice state and no LFO slot: a filter arcs once a bar
                                                         * while the LFO keeps its own job */
        uint32_t len = div_samples(0) * 4u;              /* four beats, the transport's bar */
        if (!len)
            return 0;
        return (int32_t)(((uint64_t)(ms_clock % len) << 15) / len);
    }
    case MS_NOISE:                                      /* (1.2.1-GW, JIANT 0.7.8) a random wander, eased: a smooth
                                                         * random LFO, each track its own; (3.5) N.RATE sets how long
                                                         * a point holds, 46 ms being what it always was */
        return mod_noise((uint32_t)(t - trk) * 7919u + 0x1234u, mns_clk, mod_nlen(t));
    default:
        return macro_v[(s - MS_M1) & 3u] * 258;
    }
}

/* (v1.8.1) ONE SOURCE VALUE A BLOCK, NOT ONE A SLOT. mod_tsrc() is pure -- it reads the LFO of the block before,
 * the latest note-on's VEL/KEY/RAND/PAT, the wheel, the macros, the transport and the noise clock, and writes
 * nothing -- so two slots that name the same source were resolving it twice: four LFO routes, four times, and
 * every NOISE route a mod_noise of its own. The values are kept for the block in the part's own mod_t and reset
 * where a block starts. Same numbers, fewer calls: the audio is bit-identical, which regress holds. */
static __attribute__((noinline)) int32_t mod_src_block(mod_t *mp, const track_t *t, uint32_t s, int32_t lfo)
{
    if (!(mp->sc_mask & (1u << s))) {
        mp->sc[s] = mod_tsrc(t, s, lfo);
        mp->sc_mask |= 1u << s;
    }
    return mp->sc[s];
}

/* (FASE 8) the live value of a slot's source, Q15, for the MOD page: the LFO as the block sees it, the per-voice
 * ones of the latest note-on, the macros, the modulation sequence where it is. Read from the main loop, never
 * from the ISR: it shows one block late and touches nothing. */
static int32_t mod_src_live(const track_t *t, uint32_t k)
{
    uint32_t s = (uint32_t)mslot(t, k, 0);
    return s < MS_N ? mod_tsrc(t, s, mulq15(t->lfo_val, t->lfo_fade)) : 0;
}

/* (v1.8.0) WHAT A ROUTE APPLIES: the same product the engine uses, source x depth, in Q15 and signed.
 * mod_block_slot scales by ((x * a) >> 6) >> 15 with AMT 64 = 100 %, so the MOD page's activity bar asks this and
 * shows the amount that actually reaches the destination -- a +50 route and a -50 route from the same source read
 * as opposite sides, and a shallow route reads short. Pure, clamped, no state: the UI calls it from the main loop
 * and the test checks it. */
static int32_t mod_contrib(int32_t sv, int32_t amt)
{
    int32_t v = clamp(sv, -32767, 32767) * (clamp(amt, 0, 127) - 64) / 64;
    return clamp(v, -32767, 32767);
}

/* AMP: the gain (Q15, 0..32767) of source x (bipolar: as 0..1) at amount a */
static int32_t mod_gain(uint32_t s, int32_t x, int32_t a)
{
    int32_t u = MS_BIPOLAR(s) ? (x + 32768) >> 1 : x;
    return clamp(32767 - (a > 0 ? ((32767 - u) * a) >> 6 : (u * -a) >> 6), 0, 32767);
}

/* a per-voice destination d by source value x, amount a */
static __attribute__((noinline)) void mod_voice_add(uint32_t s, uint32_t d, int32_t x, int32_t a, int32_t *pit, int32_t *cut, int32_t *shp,
                          int32_t *gain)
{
    if (d == MD_PITCH)
        *pit += (x * a * 3) >> 15;
    else if (d == MD_CUT)
        *cut += (x * a) >> 7;
    else if (d == MD_SHP)
        *shp += (x * a) >> 7;
    else
        *gain = mulq15(*gain, mod_gain(s, x, a));
}

/* (JIANT) MS_STEP: the level of the step now towards t->ms_v, at SLEW's pace (0: at once; 127: ~1.5 s) */
static __attribute__((noinline)) void ms_tick(track_t *t)
{
    const int16_t *p = t->p;
    uint32_t per = div_samples((uint32_t)p[P_MSDIV]), len = p[P_MSLEN] > 0 ? (uint32_t)p[P_MSLEN] : 1u;
    int32_t target = p[P_MS0 + (per ? ms_clock / per % len : 0u)] * 258, s = p[P_MSSLW];
    if (!s) {
        t->ms_v = target;
        return;
    }
    t->ms_v += ((target - t->ms_v) * (32767 / (1 + s * s / 8))) >> 15;   /* (a block's step: 1 / (1 + s^2 / 8)) */
}

/* before a part's block (fx.c mix_part): the per-track offsets, the per-block destinations into t->p.
 * 0 = no slot active (then nothing changed) */
/* (1.7) one slot's work, out of mod_begin's loop: the body was most of the loop's target cost. The
 * slots are four; the call is cheaper than inlining the per-block branch, the descriptor lookup and
 * the hold/search into the loop four times, and the behaviour is the same. */
static __attribute__((noinline)) uint32_t mod_scan(const track_t *t, int *step)
{
    uint32_t k, first = NMSLOT;
    *step = 0;
    for (k = 0; k < NMSLOT; k++) {
        uint32_t s = (uint32_t)mslot(t, k, 0);
        if (s && mslot(t, k, 1) && mslot(t, k, 2)) {
            if (first == NMSLOT)
                first = k;
            if (s == MS_STEP)
                *step = 1;
        }
    }
    return first;
}

static __attribute__((noinline)) void mod_block_slot(track_t *t, uint32_t k, const int16_t *p, int32_t lfo,
                                                     int32_t *off, mod_t *mp)
{
    uint32_t s = (uint32_t)mslot(t, k, 0), d = (uint32_t)mslot(t, k, 1), j;
    int32_t a = mslot(t, k, 2), x;
    if (!s || !d || !a || s >= MS_N || d == MD_CLIP || d == MD_PNCH)   /* (the master's: macro_master; X1..X4 and RES
                                                                         * are ours) */
        return;
    if (d <= MD_AMP) {                                 /* per voice */
        mp->amp |= d == MD_AMP;
        if (MS_VOICE(s)) {
            mp->vsrc[mp->nv] = (uint8_t)s;
            mp->vdst[mp->nv] = (uint8_t)d;
            mp->vamt[mp->nv++] = (int8_t)a;
        } else {
            mod_voice_add(s, d, mod_src_block(mp, t, s, lfo), a, &mp->pit, &mp->cut, &mp->shp, &mp->gain);
        }
        return;
    }
    {                                                   /* per block: AMT 64 = the whole range */
        uint32_t id = mod_param(d);
        const param_desc_t *pd = track_desc(t, id);   /* (FASE 2) the engine's labels for its CHARACTER bank */
        x = mod_src_block(mp, t, s, lfo);
        for (j = 0; j < mp->nk && mp->kid[j] != id; j++)
            ;
        if (j == mp->nk) {
            mp->kid[j] = (uint8_t)id;
            mp->keep[j] = p[id];
            off[j] = 0;
            mp->nk++;
        }
        off[j] += (((x * a) >> 6) * (id == P_LRATE ? 127 : pd->max - pd->min)) >> 15;   /* (0.6.51: RATE's free span,
                                                                                           * as before its in-time values) */
    }
}

static __attribute__((noinline)) int mod_begin(track_t *t)
{
    mod_t *const mp = &MOD;                          /* (0.6.71) this core's */
    const int16_t *p = t->p;
    int32_t lfo, off[NMSLOT];
    uint32_t k, j;
    mp->on = 0;
    {
        int step;
        k = mod_scan(t, &step);
        if (k == NMSLOT)
            return 0;
        lfo = mulq15(t->lfo_val, t->lfo_fade);          /* (the LFO of the block before: track_render ticks it) */
        if (step)
            ms_tick(t);                                 /* (once, before any slot reads it) */
    }
    mp->on = 1;
    mp->sc_mask = 0;                                 /* (v1.8.1) a block's sources: resolved once each */
    mp->amp = 0;
    mp->nv = mp->nk = 0;
    mp->pit = mp->cut = mp->shp = 0;
    mp->gain = 32767;
    for (; k < NMSLOT; k++)
        mod_block_slot(t, k, p, lfo, off, mp);
    for (j = 0; j < mp->nk; j++) {
        uint32_t id = mp->kid[j];
        const param_desc_t *pd = track_desc(t, id);
        int32_t lo = pd->min, hi = pd->max;
        if (id == P_LRATE)                              /* (0.6.51) the LFO's RATE moved within its kind: free / in time */
            lo = mp->keep[j] < 0 ? lo : 0, hi = mp->keep[j] < 0 ? -1 : hi;
        t->p[id] = (int16_t)clamp(mp->keep[j] + off[j], lo, hi);
    }
    return 1;
}

/* (JIANT) once an audio block, before the parts (fx.c): CLIP and PUNCH as the master plays them, the stored
 * values plus every track's slots on them (per-voice sources: the latest note-on's value; LFO: the block before) */
static __attribute__((noinline)) void macro_master(void)
{
    int32_t off[2] = {0, 0};
    uint32_t i, k;
    mns_clk++;                                          /* (1.2.1-GW) NOISE's clock: a block */
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);   /* (v1.8.1) once a track, not once a slot */
        MOD.sc_mask = 0;
        for (k = 0; k < NMSLOT; k++) {
            uint32_t s = (uint32_t)mslot(t, k, 0), d = (uint32_t)mslot(t, k, 1);
            int32_t a = mslot(t, k, 2);
            if (s && a && s < MS_N && (d == MD_CLIP || d == MD_PNCH))
                off[d - MD_CLIP] += (((mod_src_block(&MOD, t, s, lfo) * a) >> 6) * 100) >> 15;
        }
    }
    clip_eff = (int16_t)clamp(song.g[G_CLIP] + off[0], 0, 100);
    pnch_eff = (int16_t)clamp(song.g[G_PUNCH] + off[1], 0, 100);
}

/* after the part's block (when MOD.on): the stored values back */
static __attribute__((noinline)) void mod_end(track_t *t)
{
    mod_t *const mp = &MOD;                          /* (0.6.71) this core's */
    uint32_t j;
    for (j = 0; j < mp->nk; j++)
        t->p[mp->kid[j]] = mp->keep[j];
    mp->on = 0;
}

/* a voice's modulation (voice.c track_render, when MOD.on), onto its vmod_t m as track_render made it:
 * pitch (the increment again, with the fine factor fine as there), cutoff and shape (clamped to what the
 * fixed routings can reach), the amplitude (and v->env_out, the next block's start). env: its envelope */
static __attribute__((noinline)) void mod_voice(track_t *t, voice_t *v, vmod_t *m, int32_t fine)
{
    mod_t *const mp = &MOD;                          /* (0.6.71) this core's */
    int32_t pit = mp->pit, cut = mp->cut, shp = mp->shp, gain = mp->gain, env = m->envq15;
    uint32_t k;
    if (v == &V(t, t->m_vi))
        t->m_env = env;                                 /* ENV of the latest note, for per-block destinations */
    /* (1.7) the per-voice sources resolved once, outside the slot loop: a vsrc is always one of these
     * (MS_VOICE), so the loop does one indexed load instead of re-testing the source every slot. That
     * keeps PAT (1.6) from growing the loop's weighted cost, without touching the budget. */
    int32_t sv[MS_N];
    sv[MS_ENV] = env;
    sv[MS_VEL] = v->mvel * 258;
    sv[MS_KEY] = mod_key(v->note);
    sv[MS_RAND] = v->mrnd;
    sv[MS_PAT] = (int32_t)v->pat * 258;
    for (k = 0; k < mp->nv; k++) {
        uint32_t s = mp->vsrc[k];
        mod_voice_add(s, mp->vdst[k], sv[s], mp->vamt[k], &pit, &cut, &shp, &gain);
    }
    if (pit) {
        m->pitch16 = clamp(m->pitch16 + pit, 0, 2047);
        m->inc = tune_inc(m->pitch16);
        if (fine)
            m->inc += (uint32_t)((int32_t)(m->inc >> 12) * fine);
    }
    m->cutoff = clamp(m->cutoff + cut, -150 * 256, 150 * 256);
    m->shape = clamp(m->shape + shp, -62 * 256, 190 * 256);
    if (mp->amp) {
        m->amp1 = mulq15(m->amp1, gain);
        v->env_out = m->amp1;
    }
}

/* a note-on of track t (voice.c trk_note_on): the per-voice sources of the latest note */
static void mod_note(track_t *t, uint32_t note, uint32_t vel)
{
    t->m_vel = (uint8_t)vel;
    t->m_key = (uint8_t)note;
    t->m_rnd = mod_rand();
}

/* (QG-1 1.6) polyphonic aftertouch (MIDI 0xA0) for the keys of track t that are held: the M-VAVE FM-1's own keys
 * have no pressure sensor, so this comes from an external controller and only touches the notes it names */
static void mod_pat(track_t *t, uint32_t note, uint32_t val)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (V(t, i).active && V(t, i).gate && V(t, i).note == (uint8_t)note)
            V(t, i).pat = (uint8_t)(val > 127u ? 127u : val);
}

/* a MIDI controller for track t (seq.c events_block): CC1 / CC11 / channel aftertouch; CC121 resets them */
static __attribute__((noinline)) void mod_midi(track_t *t, uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st == 0xD0u) {
        t->at = (uint8_t)d1;
    } else if (st == 0xB0u) {
        if (d1 == 1u)
            t->mw = (uint8_t)d2;
        else if (d1 == 11u)
            t->ex_off = (uint8_t)(127u - d2);
        else if (d1 == 2u)                       /* (FASE 4) CC2 BREATH: same field family, its own byte, so a
                                                  * breath controller and CC11 can both be connected */
            t->br_off = (uint8_t)(127u - d2);
        else if (d1 == 121u)
            t->mw = t->at = t->ex_off = t->br_off = 0;
    }
}

/* the name of destination value v for track t (UI, <= 5 characters): E1..E8 by the engine's labels (of the
 * engine the UI shows), "E5" for a parameter the engine does not use */
static const char *mod_dst_name(const track_t *t, int32_t v)
{
    v = clamp(v, 0, MD_N - 1);
    if (v >= MD_E1 && v < MD_CLIP) {
        const char *l = track_desc(t, P_E0 + (uint32_t)(v - MD_E1))->label;
        if (l && l[0] && l[0] != '-')
            return l;
    }
    if (v >= MD_X1 && v <= MD_X4) {                   /* (FASE 2) the CHARACTER bank's own labels */
        const char *l = v == MD_X3 ? SLEW_DESC.label : v == MD_X4 ? INVERT_DESC.label :
                        track_desc(t, P_XID[v - MD_X1])->label;
        if (l && l[0] && l[0] != '-')
            return l;
    }
    return N_MDST[v];
}
