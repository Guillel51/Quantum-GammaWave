/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PERFORM: the effects of the FX hold layer, on the master. A key pressed while FX is held belongs to the
 * layer (seq.c keyboard_block): it plays no note, sends no MIDI, records nothing, and holds its effect until
 * it is let go (ui_input.c opens the layer and shows the map). The 5 white keys from the left (F3 .. C4):
 *   REPEAT 1/8 1/16 1/32, LPF | HPF; then (JIANT) the 10 punch-in MIDI effects D4 .. F5 (pfx.c); black keys 1..4
 *   (F#3 G#3 A#3 C#4): tracks 1..4 muted while held (not P_MUTE, never saved); A#4 the MIDI effects' tracks.
 * TONIC keeps only the audio stutters (REPEAT): REVERSE, TAPE STOP, FREEZE and OCT UP / DN (the harmonizer) were
 * removed; the punch-in effects of TONIC act on the notes instead (FELUCCA-TONIC-SPEC.md).
 * REPEAT starts on the next 1/16 (at once while stopped); the filters at once; all end when let go, with a 2.9 ms
 * ramp. Effects of different kinds stack. The six capture effects (the three REPEAT rates, REV, FRZ, TAPE, GLCH,
 * RTRG) have three independent read heads over one shared 32 KB capture: up to three are audible together, not
 * "last pressed wins". The capture is taken once; STUT tracks play live while it is borrowed and their recordings
 * are dropped afterwards. A REPEAT 1/8 longer than the loop at the tempo (below 81 BPM) does nothing (dimmed).
 * KNOB 1..4 with FX: the macros FILTER (the LPF / HPF), CRUSH, THROW (the dry mix into the delay and reverb
 * sends), DEPTH (the REPEAT's level).
 * MENU > FX LATCH ON (#40, a hand that cannot hold FX, a key and a knob together): a key pressed with FX turns its
 * effect on, the next press turns it off (perf_latched; letting the key or FX go does nothing), the macros keep
 * their values when FX is let go, FX + OCT- turns everything off (ui_layer.c), so does the menu or a dialog.
 * Chain: REPEAT -> LPF -> HPF -> CRUSH, after the master level and before
 * master_out (the limiter); THROW and the mutes act before the buses (fx.c mix_block / mix_part).
 * Idle (no key, no knob, no ramp left) every stage is skipped: the output is bit-identical. */
enum { PF_R8, PF_R16, PF_R32, PF_LPF, PF_HPF,
       PF_OCTD, PF_OCTU, PF_HALF, PF_DSHT, PF_DLNG, PF_S8, PF_S16, PF_ATK, PF_ARP, PF_RND,   /* the MIDI ones: pfx.c */
       PF_M1, PF_N = PF_M1 + NTRK, PF_TGT = PF_N + 1,     /* PF_TGT: A#4, the MIDI effects' tracks (pfx_tgt) */
       PF_CLR = PF_N + 2,                                  /* PF_CLR: G5, erases the punch-in lane (pfx.c) */
       /* (QG-1) the black keys left over: G#4 VIBRATO and F#5 DIVE on the notes (pfx.c), C#5 GATE,
        * D#5 RETRIGGER on the master (here) */
       /* SWEEP was dropped from the key map to keep the PF bits inside one word; its DSP stays in the
        * filter block only through the LPF/HPF macros. D#5 now opens RETRIGGER. */
       PF_VIB = PF_N + 3, PF_GATE, PF_DIVE,
       /* (1.8) the second FX bank: the five buffer effects that were missing. They share the SLICER's
        * existing loop; no new buffer, voice or parameter was added. A#4 held opens the bank. */
       PF_REV, PF_FRZ, PF_TAPE, PF_GLCH, PF_RTRG, PF_END };
#define PF_IS(e) ((e) < PF_N || ((e) >= PF_VIB && (e) < PF_END))   /* a key with an effect */
_Static_assert(PF_END <= 32, "the PF bits in a word");
#define PF_BIT(e) (1u << (e))
#define PF_REPEAT 0x7u                                         /* the three REPEAT rates */
#define PF_Q PF_REPEAT                                         /* start on the 1/16 */
#define PF_BUF (PF_REPEAT | PF_BIT(PF_REV) | PF_BIT(PF_FRZ) | PF_BIT(PF_TAPE) | \
                PF_BIT(PF_GLCH) | PF_BIT(PF_RTRG))                       /* the buffer effects */
#define PF_MUTE (((1u << NTRK) - 1u) << PF_M1)
#define PF_MIDI ((((1u << (PF_RND + 1)) - 1u) & ~((1u << PF_OCTD) - 1u)) | PF_BIT(PF_VIB) | PF_BIT(PF_DIVE))   /* PF_OCTD .. PF_RND,
                                                                 * VIB, DIVE: on the notes (pfx.c) */
#define PB_FRAMES SL_FR                       /* stereo frames in sl_buf (a power of 2: the mask), 371 ms at 22.05 kHz */
_Static_assert(PB_FRAMES <= SL_FR && !(PB_FRAMES & (PB_FRAMES - 1u)), "the REPEAT loop in sl_buf");
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
#define PF_TOP (63 << 8)                      /* filter cutoff index, Q8 (PF_SVF): the LPF's open end */
#define PF_LOW (28 << 8)                      /* .. the LPF sweep's end (~480 Hz), the HPF sweep's (and K1's) top:
                                               * a punch filter that keeps the body; 14 (~120 Hz) killed it */
enum { BM_NONE, BM_LOOP, BM_REV, BM_FRZ, BM_TAPE, BM_GLCH, BM_RTRG };

static volatile uint32_t perf_mask;   /* main: the FX button's bit while its layer may own keys, 0 = none */
static volatile uint32_t kb_mask;     /* main: the button bits whose hold makes keys a layer's (ui_layer.c), 0 = none */
static volatile uint8_t perf_solo;    /* main: tracks soloed in the GLO layer (the others muted as by a black key) */
static volatile uint32_t scene_pfx;  /* (JIANT) the punch-in MIDI effects a song row's scene holds (song_play.c), PF bits */
static volatile uint8_t perf_kill;    /* main: every effect off (the menu, a dialog, UBOOT) */
static volatile int8_t perf_k[4];     /* main: the knob macros, 0 = untouched: FILTER -100..100 (- LPF, + HPF),
                                       * CRUSH 0..100, THROW 0..100, DEPTH cut 0..100 (the buffer effects' level) */
static volatile uint32_t kb_layer;    /* ISR: the keys held that are the layer's, a bit per key */
static volatile uint32_t perf_held;   /* ISR: the effects held (PF_*) */
static volatile uint8_t perf_latch_on;   /* main: MENU > FX LATCH ON (#40): a key with FX toggles its effect */
static volatile uint32_t perf_latched;   /* ISR: the effects latched (FX LATCH; main clears it: OCT-, the menu) */
static volatile uint32_t perf_act;    /* ISR: .. running (a 1/16 one from its start on) */

/* the ZDF state-variable filter at cutoff index i (30 Hz * 2^(i / 7), 0..63: up to 15.4 kHz), k = 1:
 * a1 a2 a3 in Q14 (tools: g = tan(pi fc / fs), a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2) */
static const int16_t PF_SVF[64][3] = {
    {16349, 35, 0}, {16345, 39, 0}, {16341, 43, 0}, {16337, 47, 0}, {16332, 52, 0}, {16327, 57, 0}, {16321, 63, 0},
    {16314, 70, 0}, {16307, 77, 0}, {16299, 85, 0}, {16290, 94, 1}, {16280, 103, 1}, {16269, 114, 1},
    {16257, 126, 1}, {16244, 139, 1}, {16229, 153, 1}, {16213, 169, 2}, {16196, 186, 2}, {16176, 205, 3},
    {16154, 227, 3}, {16130, 250, 4}, {16104, 275, 5}, {16075, 303, 6}, {16043, 334, 7}, {16007, 368, 8},
    {15968, 406, 10}, {15925, 447, 13}, {15877, 492, 15}, {15824, 541, 19}, {15766, 596, 22}, {15702, 655, 27},
    {15631, 720, 33}, {15553, 791, 40}, {15467, 869, 49}, {15372, 953, 59}, {15267, 1046, 72}, {15151, 1146, 87},
    {15024, 1255, 105}, {14883, 1374, 127}, {14729, 1502, 153}, {14559, 1640, 185}, {14372, 1790, 223},
    {14166, 1950, 268}, {13940, 2121, 323}, {13692, 2304, 388}, {13420, 2499, 465}, {13122, 2704, 557},
    {12797, 2921, 667}, {12441, 3147, 796}, {12053, 3382, 949}, {11631, 3624, 1129}, {11173, 3870, 1341},
    {10677, 4119, 1589}, {10140, 4365, 1879}, {9562, 4605, 2218}, {8940, 4832, 2612}, {8274, 5040, 3070},
    {7563, 5219, 3602}, {6807, 5359, 4219}, {6006, 5444, 4934}, {5163, 5456, 5766}, {4282, 5369, 6733},
    {3372, 5149, 7863}, {2449, 4745, 9190},
};

static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute;          /* sync: a 1/16 starts with the next block; mute: tracks to ramp */
    /* the filters, CRUSH, THROW, mutes */
    int32_t lc, hc, la, ha;            /* cutoffs (Q8 index) and shares (Q15) of LPF and HPF */
    int32_t lz[4], hz[4];              /* their states: L ic1 ic2, R ic1 ic2 */
    int16_t lk[3], hk[3];              /* their coefficients this block */
    uint32_t lt, ht;                   /* samples the LPF / HPF key has been held */
    int32_t cw, cc, chl, chr;          /* CRUSH share, amount, held sample */
    uint32_t cn;
    int32_t td;                        /* THROW: the share of the dry mix sent, Q15 */
    int32_t mg[NTRK];                  /* mute gains, Q15 (32768 = open) */
    /* (QG-1) GATE: the cut now (Q15, 0 = open), the place in the 1/16 at the block's start */
    int32_t gc;
    uint32_t gph;
} pf = {.lc = PF_TOP, .mg = {[0 ... NTRK - 1] = 32768}};   /* (every track open: 0.6, six) */

/* (1.8) The three buffer heads: one shared recording in sl_buf, three independent readers. The six capture
 * effects used to share one reader, so the last one selected muted the others. They now keep their own mode,
 * length, cursor and gain; the recording is taken once. No second 32 KB buffer is allocated. */
#define PB_HEADS 3u
typedef struct {
    uint8_t mode, src, state, on;      /* BM_*, its PF_*, waiting for its length / playing, selected */
    uint32_t len, rp;                  /* loop length and read position, samples */
    uint32_t tp, spd, tdec;            /* TAPE STOP */
    uint32_t gl_next, gl_pos, gl_dir;  /* GLITCH */
    int32_t w, tw;                     /* its share and target, Q15 */
} pb_head_t;
static pb_head_t pb[PB_HEADS];
static uint32_t pb_wr;                 /* shared capture: frames written */
static uint32_t pb_odd;                /* the first sample of a pair */
static int32_t pb_hl, pb_hr;
static uint8_t pb_recording;
#define PB_CAP (PB_MAX / 2u)           /* stereo frames in the shared capture (371 ms at 22.05 kHz) */
static int pb_any(void)
{
    uint32_t i;
    for (i = 0; i < PB_HEADS; i++)
        if (pb[i].mode != BM_NONE)
            return 1;
    return 0;
}
/* (2.1) A NEW PRESET: nothing the old one latched may follow it into the new sound. The latched
 * punch-in effects (FX LATCH), the four macros and the buffer heads' recordings are state, not
 * settings: the sound changed under them. Called from ui.c's preset load and engine change, in main
 * context, with the audio IRQ off around it, so the ISR sees either side of the change and never a
 * half-cleared map. Mutes are the player's (a latch per track is a live decision, not the preset's),
 * so they stay. */
static void pb_reset_all(void)
{
    uint32_t i;
    for (i = 0; i < PB_HEADS; i++) {
        pb[i].mode = BM_NONE;
        pb[i].on = pb[i].state = 0;
        pb[i].w = pb[i].tw = 0;
        pb[i].rp = pb[i].len = 0;
    }
    pb_wr = 0;
    pb_recording = 0;
}
void perf_reset(void)
{
    perf_kill = 1;                     /* the ISR plays the map as empty this block, not as the old one */
    perf_latched = 0;
    perf_held = 0;
    perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    pb_reset_all();
}

/* UI: how many heads are selected, how many are already reading, and one effect's state (0 none, 1 capture, 2 read) */
static uint32_t perf_buf_heads(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < PB_HEADS; i++)
        n += pb[i].mode != BM_NONE;
    return n;
}
static uint32_t perf_buf_playing(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < PB_HEADS; i++)
        n += pb[i].mode != BM_NONE && pb[i].state;
    return n;
}
static uint32_t perf_buf_state(uint32_t e)
{
    uint32_t i;
    for (i = 0; i < PB_HEADS; i++)
        if (pb[i].mode != BM_NONE && pb[i].src == (uint8_t)e)
            return pb[i].state ? 2u : 1u;
    return 0;
}
static int pb_free(void)
{
    uint32_t i;
    for (i = 0; i < PB_HEADS; i++)
        if (pb[i].mode == BM_NONE)
            return (int)i;
    return -1;
}
static void pb_release_idle(void)
{
    if (!pb_any() && sl_lent) {
        sl_lent = 0;
        slicer_drop();
        pb_wr = 0;
        pb_odd = 0;
        pb_recording = 0;
    }
}

/* the loop length of a REPEAT at the tempo, 44.1 kHz samples; 0 = not one */
static const uint8_t PF_DEN[PF_R32 + 1] = {2, 4, 8};
static uint32_t perf_len(uint32_t e)
{
    if (e <= PF_R32)
        return beat_samples() / PF_DEN[e];
    if (e == PF_REV || e == PF_FRZ || e == PF_TAPE || e == PF_GLCH)
        return beat_samples() > PB_MAX ? PB_MAX : beat_samples();   /* one beat, the loop's room */
    if (e == PF_RTRG)
        return beat_samples() / 8u > PB_MAX ? PB_MAX : beat_samples() / 8u;
    return 0u;
}
/* the effects that can run at the tempo: a REPEAT longer than the loop cannot */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e <= PF_R32; e++)
        if (b > PB_MAX * PF_DEN[e])
            m &= ~PF_BIT(e);
    return m;
}

/* seq.c keyboard_block: a layer key down / up (e: PF_*, PF_N = no effect) */
static volatile uint8_t pfx_tgt;      /* main / keys: the MIDI effects' tracks, 0 ALL, 1 SYN, 2 DRM (pfx.c) */
static volatile uint8_t pfx_bank;     /* 0: the original FX map; 1: REV FRZ TAPE GLCH RTRG (A#4 held) */
static volatile uint8_t pfx_bank_force; /* the UI redraws the map after the bank changes */
static uint32_t pfx_tgt_t0;            /* A#4 down: a short press steps TGT, a long one opens bank 1 */
static volatile uint8_t pfx_clr;      /* keys: G5 held (pfx.c: the lane erased where it passes, or all of it) */
static uint32_t pfx_lph;              /* samples into the punch-in lane (pfx.c): from the transport's start, a section's */
static uint32_t pf_rs = 0x2545F491u;  /* GLITCH's own random numbers */
static uint8_t pfx_lane[32], pfx_ltgt;   /* the section's punch-in lane (pfx.c), its tracks (as pfx_tgt) */
#define PF_MAX_ACTIVE 3u
/* the musical effects active (mutes and the target/clear keys are not effects) */
static uint32_t perf_count(uint32_t a)
{
    uint32_t e, n = 0;
    for (e = 0; e < PF_END; e++)
        if (PF_IS(e) && e != PF_M1 && e != PF_M1 + 1u && e != PF_M1 + 2u &&
            e != PF_M1 + 3u && e != PF_M1 + 4u && e != PF_M1 + 5u && ((a >> e) & 1u))
            n++;
    return n;
}
static void perf_press(uint32_t e, int down)
{
    if (e == PF_TGT) {
        if (down) {
            pfx_tgt_t0 = fm1_ms;
            return;
        }
        if (fm1_ms - pfx_tgt_t0 >= 600u) {       /* a long A#4: the second FX bank */
            pfx_bank ^= 1u;
            pfx_bank_force = 1;
        }
        else
            pfx_tgt = (uint8_t)((pfx_tgt + 1u) % 3u);
        return;
    }
    if (e == PF_CLR)
        pfx_clr = (uint8_t)(down ? 1u : 0u);
    if (e == PF_RND && pfx_bank && down)   /* bank 2's CRUSH must wake perf_begin without a new branch there */
        pf.busy = 1;
    if (!PF_IS(e))
        return;
    if (perf_latch_on) {                  /* FX LATCH: a press turns it on or off, letting go does nothing */
        if (down) {
            if (!(e >= PF_M1 && e < PF_M1 + NTRK) && !(perf_latched & PF_BIT(e)) &&
                perf_count(perf_latched) >= PF_MAX_ACTIVE)
                return;                     /* a fourth effect refused; the six mutes still work */
            perf_latched ^= PF_BIT(e);
        } else {
            perf_held &= ~PF_BIT(e);        /* a key already down when LATCH was turned on: no stuck hold */
        }
        return;
    }
    if (down) {
        if (!(e >= PF_M1 && e < PF_M1 + NTRK) && !(perf_held & PF_BIT(e)) &&
            perf_count(perf_held | perf_latched) >= PF_MAX_ACTIVE)
            return;                         /* a fourth effect refused; the six mutes still work */
        perf_held |= PF_BIT(e);
    } else {
        perf_held &= ~PF_BIT(e);
    }
}

static void perf_start(void) { pf.ph = 0; pf.sync = 1; pfx_lph = 0; }   /* seq_start: a 1/16 starts with the transport */

/* the SLICER's recordings dropped (when the buffer is taken and given back) */
static void perf_drop_slicer(void) { slicer_drop(); }

static void perf_buf_start(uint32_t e)
{
    int i = pb_free();
    uint32_t len;
    pb_head_t *h;
    if (i < 0)
        return;
    len = perf_len(e);
    if (!len)
        return;
    if (len > PB_MAX)
        len = PB_MAX;
    if (!sl_lent) {
        sl_lent = 1;
        perf_drop_slicer();
        pb_wr = 0;
        pb_odd = 0;
    }
    h = &pb[i];
    memset(h, 0, sizeof *h);
    h->src = (uint8_t)e;
    h->mode = e == PF_REV ? BM_REV : e == PF_FRZ ? BM_FRZ : e == PF_TAPE ? BM_TAPE :
              e == PF_GLCH ? BM_GLCH : e == PF_RTRG ? BM_RTRG : BM_LOOP;
    h->len = len;
    h->state = 0;
    h->on = 1;
    h->spd = 1u << 16;
    h->tdec = (1u << 16) / len;
    h->gl_dir = 1;
    pb_recording = (uint8_t)(pb_wr < PB_CAP);
}

/* Reconcile the selected buffer effects with the three heads at a 1/16. A head that is no longer selected
 * fades out; a newly selected one starts immediately. The capture keeps filling until PB_CAP so a later
 * one-beat effect can still use the whole recording. */
static void perf_buf_select(uint32_t bits)
{
    uint32_t e, i, found;
    bits &= PF_BUF;
    for (i = 0; i < PB_HEADS; i++)
        if (pb[i].mode != BM_NONE && !((bits >> pb[i].src) & 1u))
            pb[i].on = 0;
    for (e = PF_R8; e <= PF_R32; e++)
        if ((bits >> e) & 1u) {
            for (i = found = 0; i < PB_HEADS; i++)
                if (pb[i].mode != BM_NONE && pb[i].src == (uint8_t)e) {
                    pb[i].on = 1;
                    found = 1;
                    break;
                }
            if (!found)
                perf_buf_start(e);
        }
    for (e = PF_REV; e < PF_END; e++)
        if ((bits >> e) & 1u) {
            for (i = found = 0; i < PB_HEADS; i++)
                if (pb[i].mode != BM_NONE && pb[i].src == (uint8_t)e) {
                    pb[i].on = 1;
                    found = 1;
                    break;
                }
            if (!found)
                perf_buf_start(e);
        }
    pb_release_idle();
}

/* each block, after the keys (seq.c) and before the parts: what runs, the 1/16, the filters, the mutes.
 * Returns 1 when a stage has something to do */
/* (0.6.19) a track let go from its mute, silent (its voices ended at 0, perf_hush_end): nothing to ramp back, open at
 * once; else mix_part never reached perf_mute again (no signal) and the FX layer stayed busy every block */
static __attribute__((noinline)) void perf_unhush(void)   /* (out of perf_begin: target_budget.py) */
{
    uint32_t k, i, on;
    for (k = 0; k < NTRK; k++)
        if (!pf.mg[k] && !((pf.act >> (PF_M1 + k)) & 1u)) {
            for (i = on = 0; i < NVOICE; i++)
                on |= trk[k].v[i].active;
            if (!on)
                pf.mg[k] = 32768;
        }
}
static __attribute__((noinline)) int perf_begin(uint32_t n)
{
    uint32_t held = perf_kill ? 0u : (perf_held | perf_latched | (perf_solo ? (~(uint32_t)perf_solo & ((1u << NTRK) - 1u)) << PF_M1 : 0u)) & perf_avail() & ~PF_MIDI;
    uint32_t q, k, ph0, bnd;
    int32_t m;
    if (!held && !pf.busy && !(perf_k[0] | perf_k[1] | perf_k[2])) {   /* idle: the clock only */
        pf.ph += n;
        pf.sync = 0;
        perf_act = 0;
        trk_hush = 0;
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    if (song.playing && clk_pos != CLK_START)       /* (JIANT) playing: the transport's 1/16 (seq.c click_tick, through
                                                     * tempo changes and an external clock), so a REPEAT starts on the grid */
        pf.P = div_samples(2);
    ph0 = (song.playing && clk_pos != CLK_START ? clk_pos : pf.ph) % pf.P;   /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act |= held & ~PF_Q;
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    /* the filters: LPF from open down over a bar while its key is held, KNOB 1 left; HPF up, KNOB 1 right */
    m = perf_k[0];
    {
        uint32_t bar = 16u * pf.P, t;
        int32_t lt = PF_TOP, ht = 0;
        pf.lt = (pf.act & PF_BIT(PF_LPF)) ? pf.lt + n : 0u;
        pf.ht = (pf.act & PF_BIT(PF_HPF)) ? pf.ht + n : 0u;
        if (pf.lt) {
            t = pf.lt < bar ? pf.lt : bar;
            lt = PF_TOP - (int32_t)((uint32_t)(PF_TOP - PF_LOW) / 64u * (t * 64u / bar));
        }
        if (m < 0 && PF_TOP + m * (PF_TOP - PF_LOW) / 100 < lt)
            lt = PF_TOP + m * (PF_TOP - PF_LOW) / 100;
        if (pf.ht) {
            t = pf.ht < bar ? pf.ht : bar;
            ht = (int32_t)((uint32_t)(PF_TOP - PF_LOW) / 64u * (t * 64u / bar));
        }
        if (m > 0 && m * (PF_TOP - PF_LOW) / 100 > ht)
            ht = m * (PF_TOP - PF_LOW) / 100;
        pf.lc += clamp(lt - pf.lc, -192, 192);      /* ~45 ms over the range */
        pf.hc += clamp(ht - pf.hc, -192, 192);
        for (k = 0; k < 3u; k++) {                  /* the coefficients, between two table rows */
            int32_t i = pf.lc >> 8, f = pf.lc & 255, j = pf.hc >> 8, g = pf.hc & 255;
            pf.lk[k] = (int16_t)(PF_SVF[i][k] + (((PF_SVF[i < 63 ? i + 1 : 63][k] - PF_SVF[i][k]) * f) >> 8));
            pf.hk[k] = (int16_t)(PF_SVF[j][k] + (((PF_SVF[j < 63 ? j + 1 : 63][k] - PF_SVF[j][k]) * g) >> 8));
        }
    }
    pf.gph = ph0;                                   /* (QG-1) GATE: the place in the 1/16 */
    pf.mute = 0;
    perf_unhush();
    for (k = 0; k < NTRK; k++)
        if (((held >> (PF_M1 + k)) & 1u) || pf.mg[k] != 32768)
            pf.mute |= (uint8_t)(1u << k);
    trk_hush = (uint8_t)((pf.act >> PF_M1) & ((1u << NTRK) - 1u));   /* (0.6.15) muted: no new notes (voice.c) */
    pf.busy = 1;
    return 1;
}

/* a track muted by its black key: its signal ramps to 0 and back (fx.c mix_part, after the SLICER) */
static __attribute__((noinline)) void perf_hush_end(uint32_t k)   /* (out of perf_mute's loop: target_budget.py) */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[k].v[i];
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = v->env_out = 0;
    }
}
static __attribute__((noinline)) void perf_mute(uint32_t k, int32_t *b, uint32_t n)
{
    uint32_t i;
    int32_t g = pf.mg[k], t = (pf.act >> (PF_M1 + k)) & 1u ? 0 : 32768;
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
    if (!g && !t)                                       /* (0.6.15) silent: its voices end, the track costs nothing
                                                         * while muted (a MIDI mute: no new notes, trk_hush) */
        perf_hush_end(k);
}

/* before the buses: THROW (KNOB 3) adds the dry mix (and the SLICER's bus, bl / br) to the delay and reverb sends */
static __attribute__((noinline)) void perf_pre(const int32_t *ml, const int32_t *mr, const int32_t *bl, const int32_t *br,
                                               int32_t *sd, int32_t *sr, uint32_t n)
{
    uint32_t i;
    int32_t m = perf_k[2] * 327;
    for (i = 0; i < n; i++) {
        pf.td += clamp(m - pf.td, -SL_SLOPE, SL_SLOPE);
        if (pf.td) {
            int32_t x = mulq16((ml[i] + mr[i] + bl[i] + br[i]) >> 1, (uint32_t)pf.td << 1);
            sd[i] += x;
            sr[i] += x;
        }
    }
}

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}

/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;    /* (an odd loop's last sample: the frame before) */
    const int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;                               /* a frame holds sample 2f: an odd sample is the mean of 2f and 2f + 2 */
    if (s & 1u)
        q = f + 1u < n ? p + 2 : p;
    else {
        *l = p[0];
        *r = p[1];
        return;
    }
    *l = (p[0] + q[0]) >> 1;
    *r = (p[1] + q[1]) >> 1;
}
/* REPEAT at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}

/* record the pair into frame f (the mean, >> 1: headroom); the capture is shared by all heads */
static inline uint32_t pb_capture(uint32_t f, int32_t l, int32_t r)
{
    if (!pb_odd) {
        pb_hl = l;
        pb_hr = r;
        pb_odd = 1;
        return f;
    }
    pb_odd = 0;
    pb_put(f, pb_hl, pb_hr);                       /* the first sample: exact phase at the loop's start */
    return f + 1u;
}

/* the three buffer heads on one sample. Each active effect reads its own cursor from the shared capture;
 * the wet mix is their average at DEPTH, so three effects are audible together instead of the last one
 * replacing the other two. */
static __attribute__((noinline)) void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, dl = 0, dr = 0;
    uint32_t i, n = 0;
    /* Start a head at the sample after its length has been captured, not on the second sample of the
     * final pair: that would read rp = 1 and shift the loop by one sample. */
    for (i = 0; i < PB_HEADS; i++) {
        uint32_t nf = (pb[i].len + 1u) >> 1;
        if (pb[i].mode != BM_NONE && !pb[i].state && nf && pb_wr >= nf) {
            pb[i].state = 1;
            pb[i].rp = 0;
        }
    }
    if (pb_recording) {
        pb_wr = pb_capture(pb_wr, l, r);
        if (pb_wr >= PB_CAP) {
            pb_wr = PB_CAP;
            pb_recording = 0;
        }
    }
    for (i = 0; i < PB_HEADS; i++) {
        pb_head_t *h = &pb[i];
        int32_t yl = 0, yr = 0;
        uint32_t nf = (h->len + 1u) >> 1;
        if (h->mode == BM_NONE)
            continue;
        if (h->state) {
            if (h->mode == BM_LOOP || h->mode == BM_FRZ || h->mode == BM_RTRG) {
                pb_loop(h->rp, h->len, nf, &yl, &yr);
            } else if (h->mode == BM_REV) {
                pb_loop(h->len - 1u - h->rp, h->len, nf, &yl, &yr);
            } else if (h->mode == BM_TAPE) {
                if (h->spd) {
                    pb_at(h->tp >> 16, nf, &yl, &yr);
                    h->tp += h->spd;
                    h->spd = h->spd > h->tdec ? h->spd - h->tdec : 0u;
                    if (h->tp >= h->len)
                        yl = yr = 0;
                }
            } else if (h->mode == BM_GLCH) {
                if (!h->gl_next) {
                    pf_rs = pf_rs * 1664525u + 1013904223u;
                    h->gl_pos = (pf_rs >> 8) % (h->len ? h->len : 1u);
                    h->gl_dir = (pf_rs & 1u) ? 1u : 0u;
                    h->gl_next = 256u + ((pf_rs >> 16) & 511u);
                }
                pb_at(h->gl_pos, nf, &yl, &yr);
                h->gl_next--;
                if (h->gl_dir) {
                    if (++h->gl_pos >= h->len)
                        h->gl_pos = 0;
                } else if (h->gl_pos) {
                    h->gl_pos--;
                } else {
                    h->gl_pos = h->len ? h->len - 1u : 0u;
                }
            }
            if (h->mode == BM_LOOP || h->mode == BM_FRZ || h->mode == BM_REV || h->mode == BM_RTRG)
                if (++h->rp >= h->len)
                    h->rp = 0;
        }
        h->tw = h->on && h->state ? 32768 - perf_k[3] * 327 : 0;
        h->w += clamp(h->tw - h->w, -SL_SLOPE, SL_SLOPE);
        if (h->w) {
            dl += mulq16(yl - l, (uint32_t)h->w << 1);
            dr += mulq16(yr - r, (uint32_t)h->w << 1);
            n++;
        } else if (!h->on) {
            h->mode = BM_NONE;
            h->src = PF_N;
        }
    }
    if (n) {
        if (n == 1) {
            l += dl;
            r += dr;
        } else if (n == 2) {
            l += dl >> 1;
            r += dr >> 1;
        } else {
            l += dl / 3;
            r += dr / 3;
        }
        *pl = clamp(l, -32768, 32767);
        *pr = clamp(r, -32768, 32767);
    }
    pb_release_idle();
}

/* one channel of a state-variable filter (k = 1): x in Q15 >> 2 */
static inline int32_t pf_svf(int32_t x, int32_t *z, const int16_t *c, int hp)
{
    int32_t v3 = x - z[1], v1 = (c[0] * z[0] + c[1] * v3) >> 14, v2 = z[1] + ((c[1] * z[0] + c[2] * v3) >> 14);
    z[0] = 2 * v1 - z[0];
    z[1] = 2 * v2 - z[1];
    return hp ? x - v1 - v2 : v2;
}

static __attribute__((noinline)) void perf_switch(uint32_t a) { perf_buf_select(a); }

/* after the master level: the buffer effect, LPF, HPF, CRUSH on l / r (stereo, Q15) */
static __attribute__((noinline)) void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0, sh, hold, gp = pf.P ? pf.gph % pf.P : 0u, gh = pf.P / 2u;
    int32_t c = perf_k[1], lt, ht, ct, gd = 32768 - perf_k[3] * 327;
    if (pfx_bank && ((perf_held | perf_latched) & PF_BIT(PF_RND)))   /* (1.8) bank 2 F5: CRUSH independent */
        c = 100;
    if (c)
        pf.cc = c;
    ct = c ? 32768 : 0;
    sh = 4u + (uint32_t)pf.cc * 7u / 100u;
    hold = sh - 3u;
    if (pf.split)
        perf_switch(a);
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pb_any())
            perf_buf(&l, &r);
        lt = (a & PF_BIT(PF_LPF)) || perf_k[0] < 0 || pf.lc < PF_TOP ? 32768 : 0;
        pf.la += clamp(lt - pf.la, -SL_SLOPE, SL_SLOPE);
        if (pf.la) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.lz[0], pf.lk, 0) * 4;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.lz[2], pf.lk, 0) * 4;
            l = pf_mix(l, fl, pf.la);
            r = pf_mix(r, fr, pf.la);
        } else {
            pf.lz[0] = pf.lz[1] = pf.lz[2] = pf.lz[3] = 0;
        }
        ht = (a & PF_BIT(PF_HPF)) || perf_k[0] > 0 || pf.hc > 0 ? 32768 : 0;
        pf.ha += clamp(ht - pf.ha, -SL_SLOPE, SL_SLOPE);
        if (pf.ha) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.hz[0], pf.hk, 1) * 4;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.hz[2], pf.hk, 1) * 4;
            l = pf_mix(l, fl, pf.ha);
            r = pf_mix(r, fr, pf.ha);
        } else {
            pf.hz[0] = pf.hz[1] = pf.hz[2] = pf.hz[3] = 0;
        }
        {   /* CRUSH: fewer bits, a held sample (KNOB 2) */
            pf.cw += clamp(ct - pf.cw, -SL_SLOPE, SL_SLOPE);
            if (pf.cw) {
                if (++pf.cn >= hold) {
                    pf.cn = 0;
                    pf.chl = ((l >> sh) * (1 << sh)) + (1 << (sh - 1u));
                    pf.chr = ((r >> sh) * (1 << sh)) + (1 << (sh - 1u));
                }
                l = pf_mix(l, pf.chl, pf.cw);
                r = pf_mix(r, pf.chr, pf.cw);
            }
        }
        {   /* (QG-1) GATE: each 1/16 open for its first half, shut for the rest (KNOB 4 DEPTH: shut less) */
            int32_t gt = (a & PF_BIT(PF_GATE)) && gp >= gh ? gd : 0;
            if (++gp >= pf.P)
                gp = 0;
            if (gt | pf.gc) {
                pf.gc += clamp(gt - pf.gc, -SL_SLOPE, SL_SLOPE);
                l = mulq16(l, (uint32_t)(32768 - pf.gc) << 1);
                r = mulq16(r, (uint32_t)(32768 - pf.gc) << 1);
            }
        }
        bl[i] = l;
        br[i] = r;
    }
    pf.busy = pf.act || pb_any() || pb_recording || pf.la || pf.ha || pf.cw || pf.td || pf.mute || pf.gc ||
              perf_k[0] || perf_k[1] || perf_k[2];
}
