/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MORPH under load: six tracks, the FX buses, MIDI notes and preset changes -- measured on the
 * real DSP sources, with the latency and the click at every transition.
 *
 * tests/morph_audible_test.c proves one held note per engine. This proves the same thing where it
 * has to survive the instrument's worst case: six parts sounding at once, the four FX sends and the
 * master clipper live, notes arriving over MIDI while MORPH moves, and a preset load landing on top.
 *
 * The whole measurement rests on one property, checked before every number: the control render and
 * the moving one are BIT-IDENTICAL up to the turn. If they were not, "the first sample that differs"
 * would mean nothing. Only then:
 *
 *   pre     the difference before the turn (must be exactly 0; anything else fails the case)
 *   first   samples from the turn to the first sample that differs (-1: never)   -> latency
 *   full    samples from the turn to 90 % of the final difference                -> the slew's length
 *   ratio   the difference's RMS against the signal's: audible, or a drawing
 *   click   the largest sample step across the turn against the largest in the 20 blocks before it
 *
 * WHY THE RENDERS RUN IN CHILD PROCESSES. Two renders in one process inherit each other's static
 * state: the tracks and the FX buses can be cleared (reset_instrument), but the engines' own
 * registers, the automation's phase and the noise sources cannot all be enumerated from here -- and
 * a first pass that did clear what it knew found the second render diverging from the first at
 * sample ~250, before MORPH had done anything. So both renders are forked from ONE parent that has
 * just been set up: the children start from the same memory image, byte for byte, and whatever
 * state the siblings carry is the same state. What differs between them afterwards is the turn.
 *
 * Run by tests/run_tests.sh; by hand:
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/morph_six_test tests/morph_six_test.c -lm */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if !defined(_WIN32)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/params.c"
#include "../firmware/src/mod.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/slicer.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
#include "../firmware/src/song_chain.c"
#include "../firmware/src/seq.c"

#ifndef HOSTSIM_FLASH_ID
#define HOSTSIM_FLASH_ID 1
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static uint32_t fl_status_ram(void) { return 0u; }
#endif

static int host_rom_read(uint32_t k, uint8_t *pk)
{
    if (k >= FM6_ROMBANK_N) return 1;
    memcpy(pk, FM6_ROMBANK[k], FM6_PACKED);
    return 0;
}
static void host_rom_bank(void) { fm6_bank_used = (1u << FM6_ROMBANK_N) - 1u; fm6_bank_read = host_rom_read; }
/* the preset's values as ui.c apply_preset_to writes them (the same fields, the same order) */
static void host_preset(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const preset_t *p;
    uint32_t i, n = ENGINES[e]->npresets;
    t->eng_req = (uint8_t)e;
    if (!n) {
        t->preset = 0;
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
        t->engine = t->eng_req;
        return;
    }
    pi %= n;
    p = &ENGINES[e]->presets[pi];
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = (int16_t)(p->fx[i] ? p->fx[i] - 1 : FX_DEF[i]);
    for (i = 0; i < NELEM(p->x); i++)
        if (p->x[i].id && p->x[i].id - 1u < P_E0)
            t->p[p->x[i].id - 1u] = (int16_t)clamp(p->x[i].v, TP[p->x[i].id - 1u].min, TP[p->x[i].id - 1u].max);
    if (!fm6_bank_read)
        host_rom_bank();
    fm6_track_loaded(t);
    t->engine = t->eng_req;
}

/* ---- a clean instrument: the part every render must share --------------------- */
/* main.c felucca_init's audio-side state: the six tracks as at power-on (no voices, no automation),
 * the master, and the buses empty. fx_preset_flush() is the firmware's own (2.1: the buses'
 * registers, the chorus, the reverb's combs and allpasses, the pre-delay), and it deliberately
 * leaves the 128 KB delay line to be overwritten by the sound -- so this clears that too, plus the
 * two lines it does not own (the vinyl shim and the cloud reverb). */
static void reset_instrument(void)
{
    uint32_t i, k;
    memset((void *)trk, 0, sizeof trk);
    voices_boot();                  /* (3.5) the pool's windows: a part's voices are not all slot 0 */
    memset((void *)&song, 0, sizeof song);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
    trk_live = NTRK;
    fx_preset_flush();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(shim_buf, 0, sizeof shim_buf);
    memset(cloud_buf, 0, sizeof cloud_buf);
    voice_kills = 0;
    transport_req = 0;
    panic_req = 0;
    mi_r = mi_w = mo_w = mo_r = 0;
    midi_in_overflow = 0;
}

/* ---- what the scenarios are ------------------------------------------------ */
enum { SC_KEYS = 1u, SC_FX = 2u, SC_MIDI = 4u, SC_MORPH_MIDI = 8u, SC_PRESET = 16u, SC_MORPH_AFTER = 32u };

#define NBLK 768u            /* 768 x 32 = 24 576 samples: 0.557 s at 44.1 kHz */
#define SWITCH 192u          /* the block MORPH turns at (0.14 s into the render) */
#define FADE_FROM 224u       /* the preset change's block */
#define BUFN (NBLK * CTL)
#define LOG_N 6u
static int32_t buf_ctrl[BUFN], buf_move[BUFN];
static uint32_t turn;
static uint32_t log_blk[LOG_N];

/* six parts: ANALOG / PHASE / LOFI twice, so every engine with a morph path is sounding at once */
static const uint8_t ENG6[NPART] = {0, 2, 3, 0, 2, 3};
static const uint8_t PRE6[NPART] = {0, 0, 1, 11, 5, 2};   /* SAW LEAD, PHASE 0, LOFI 1, STRINGS, PHASE 5, LOFI 2 */
static const uint8_t NOTE6[NPART] = {45, 57, 52, 64, 60, 48};

static void midi_note(uint32_t ch, uint32_t note, uint32_t vel, uint32_t on)
{
    /* the packet usb.c hands the main loop (hostsim.c tracks_demo writes them the same way) */
    midi_in_q[mi_w % MQ] = 0x09u | (uint32_t)((on ? 0x90u : 0x80u) | (ch & 15u)) << 8 |
                           (note & 127u) << 16 | ((on ? vel : 0u) & 127u) << 24;
    mi_w++;
}

/* ui.c's sound load, from the audio side in: load_begin's snapshot, the release, the new values
 * (apply_preset_to) and the fade voice.c renders on the old ones (trk_fade_out). */
static void load_sound(track_t *t, uint32_t pi)
{
    uint32_t k = (uint32_t)(t - trk);
    if (xf_snap_t == 0xFFu || !trk[xf_snap_t % NTRK].xf_on) {
        memcpy(xf_snap, t->p, sizeof xf_snap);
        xf_snap_t = (uint8_t)k;
    }
    motion_restore(t);
    panic_req |= (uint8_t)(1u << k);
    host_preset(t, t->eng_req, pi);
    motion_rebase(t);
}

/* the six parts, the FX buses and the master, as the scenario asks -- the state both children fork */
static void setup_six(uint32_t sc)
{
    uint32_t k;
    reset_instrument();
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        host_preset(t, ENG6[k], PRE6[k]);
        t->p[P_VOICE] = V_POLY;
        t->p[P_LEVEL] = 88;
        t->p[P_MORPH] = 0;
        t->mo_cur = t->mo_prev = 0;
        if (k == 2u)                 /* the part MORPH turns: LOFI's pos is clamp(wave*256 + mo*768/127, ..
                                      * RW_WRAM*256), so the wave must be at the head of its list for 0 -> 127
                                      * to travel; a preset already on the last wave clamps and the turn would be
                                      * a drawing rather than a sound (the audible test sets it the same way) */
            t->p[P_E1] = 0;
        if (sc & SC_FX) {
            t->p[P_DIST] = (int16_t)(10 + k * 6);
            t->p[P_CHOR] = (int16_t)(40 + k * 8);
            t->p[P_DLY] = (int16_t)(30 + k * 5);
            t->p[P_REV] = (int16_t)(45 + k * 7);
        } else {
            t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        }
        t->p[P_ATK] = t->p[P_ATK] < 30 ? 30 : t->p[P_ATK];
        t->p[P_SUS] = 110;
        t->p[P_REL] = 90;
    }
    if (sc & SC_FX) {
        song.g[G_CLIP] = 25;
        song.g[G_EQMODE] = 1;
    }
}

/* one render of the six parts. `move` is the ONLY difference between the two runs: the preset load
 * and the MIDI note happen in both, at the same block, so anything that differs is MORPH's doing. */
static void render_into(int32_t *buf, uint32_t sc, uint32_t move)
{
    uint32_t b, i;
    if (sc & SC_MIDI) {                            /* tracks 1..4 by MIDI (G_ROUTE 0: channels 1..4) */
        for (i = 0; i < 4u; i++)
            midi_note(i, NOTE6[i], 104, 1);
        for (i = 4; i < NPART; i++)                /* tracks 5, 6 by the keys: MIDI CH1-4 stops at part 4 */
            trk_note_on(&trk[i], NOTE6[i], 104);
    } else {
        for (i = 0; i < NPART; i++)
            trk_note_on(&trk[i], NOTE6[i], 104);
    }
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        if ((sc & SC_PRESET) && b == FADE_FROM)
            load_sound(&trk[0], 4u);
        if (b == turn) {
            if (move)
                trk[2].p[P_MORPH] = 127;           /* part 3: a LOFI sound, a window's turn */
            if (sc & SC_MORPH_MIDI)
                midi_note(1u, 69u, 100, 1);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
}

/* the two renders, forked from one prepared parent (see the header) */
static void render_pair(uint32_t sc)
{
    int32_t *dst[2];
    uint32_t mv;
    dst[0] = buf_ctrl;
    dst[1] = buf_move;
    setup_six(sc);
    turn = (sc & SC_MORPH_AFTER) ? FADE_FROM + 8u : SWITCH;
    log_blk[0] = 8u;
    log_blk[1] = turn / 2u;
    log_blk[2] = turn - 1u;
    log_blk[3] = turn;
    log_blk[4] = turn + 8u;
    log_blk[5] = NBLK - 1u;
#if defined(_WIN32)
    render_into(dst[0], sc, 0);
    render_into(dst[1], sc, 1);
    printf("morph: (no fork on this host: the two renders share one process)\n");
#else
    for (mv = 0; mv < 2u; mv++) {
        int fd[2];
        size_t got = 0;
        ssize_t n;
        pid_t pid;
        if (pipe(fd) != 0) {
            printf("morph: pipe: cannot measure\n");
            exit(2);
        }
        pid = fork();
        if (pid == 0) {                            /* the child: the parent's image, untouched */
            close(fd[0]);
            render_into(dst[mv], sc, mv);
            while (got < (size_t)sizeof buf_ctrl) {
                n = write(fd[1], (const char *)dst[mv] + got, (size_t)sizeof buf_ctrl - got);
                if (n <= 0)
                    _exit(1);
                got += (size_t)n;
            }
            _exit(0);
        }
        if (pid < 0) {
            printf("morph: fork: cannot measure\n");
            exit(2);
        }
        close(fd[1]);
        got = 0;
        while (got < (size_t)sizeof buf_ctrl) {
            n = read(fd[0], (char *)dst[mv] + got, (size_t)sizeof buf_ctrl - got);
            if (n <= 0)
                break;
            got += (size_t)n;
        }
        close(fd[0]);
        waitpid(pid, NULL, 0);
        if (got != (size_t)sizeof buf_ctrl) {
            printf("morph: the render did not deliver (child %u)\n", mv);
            exit(2);
        }
    }
#endif
}

/* ---- the measurements ------------------------------------------------------ */
static double seg_rms(const int32_t *s, uint32_t a, uint32_t b)
{
    double acc = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        acc += (double)s[i] * s[i];
    return b > a ? sqrt(acc / (double)(b - a)) : 0;
}
static int32_t seg_step(const int32_t *s, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 1; i < b && i; i++) {
        int32_t d = s[i] - s[i - 1];
        d = d < 0 ? -d : d;
        if (d > m) m = d;
    }
    return m;
}
static double diff_rms(const int32_t *a, const int32_t *b, uint32_t from, uint32_t to)
{
    double acc = 0;
    uint32_t i;
    for (i = from; i < to; i++) {
        double d = (double)b[i] - (double)a[i];
        acc += d * d;
    }
    return to > from ? sqrt(acc / (double)(to - from)) : 0;
}
static int32_t first_diff(const int32_t *a, const int32_t *b, uint32_t from, uint32_t to)
{
    uint32_t i;
    for (i = from; i < to; i++) {
        int32_t d = b[i] - a[i];
        if (d > 2 || d < -2)
            return (int32_t)(i - from);
    }
    return -1;
}
static int32_t settle90(const int32_t *a, const int32_t *b, uint32_t from, uint32_t to)
{
    double fin = diff_rms(a, b, to - 1024u, to), acc;
    uint32_t i, j;
    if (fin <= 0)
        return -1;
    for (i = from; i + 64u < to; i++) {
        acc = 0;
        for (j = 0; j < 64u; j++) {
            double d = (double)b[i + j] - (double)a[i + j];
            acc += d * d;
        }
        if (sqrt(acc / 64.0) >= fin * 0.9)
            return (int32_t)(i - from);
    }
    return -1;
}
static double to_ms(int32_t samples) { return samples < 0 ? -1.0 : (double)samples * 1000.0 / (double)FS; }

static int fails, cases;
static void probe_morph(const char *name, uint32_t sc)
{
    uint32_t from, to = BUFN, i;
    int32_t first, full, before, across;
    double sig, rat, pre;
    render_pair(sc);
    from = turn * CTL;
    pre = diff_rms(buf_ctrl, buf_move, 0, from);
    sig = seg_rms(buf_ctrl, from, to);
    rat = sig > 0 ? diff_rms(buf_ctrl, buf_move, from, to) / sig : 0;
    first = first_diff(buf_ctrl, buf_move, from, to);
    full = settle90(buf_ctrl, buf_move, from, to);
    before = seg_step(buf_ctrl, from - 20u * CTL, from);
    across = seg_step(buf_move, from, from + 4u * CTL);
    cases++;
    printf("morph: %s\n", name);
    printf("       rms per block  ctrl/move:");
    for (i = 0; i < LOG_N; i++) {
        uint32_t b = log_blk[i] * CTL;
        printf(" %u:%.0f/%.0f", log_blk[i], seg_rms(buf_ctrl, b, b + CTL), seg_rms(buf_move, b, b + CTL));
    }
    printf("\n       before-the-turn diff %.0f %s\n", pre,
           pre == 0 ? "(identical)" : "(THE RUNS ARE NOT COMPARABLE)");
    printf("       difference %.2f x  latency %5.2f ms (%d samples)  slew to 90%% %6.2f ms  "
           "step %5d vs %5d before %s\n",
           rat, to_ms(first), first, to_ms(full), across, before,
           pre == 0 && rat >= 0.02 && across <= before * 2 + 128 && first >= 0 && first <= (int32_t)CTL
               ? "ok" : "FAIL");
    if (pre != 0 || rat < 0.02 || across > before * 2 + 128 || first < 0 || first > (int32_t)CTL)
        fails++;
}
static void probe_preset(void)
{
    uint32_t to = BUFN, at = FADE_FROM * CTL;
    int32_t before, across;
    double pre, post;
    render_pair(SC_FX | SC_PRESET);
    before = seg_step(buf_ctrl, at - 20u * CTL, at);
    across = seg_step(buf_ctrl, at, at + 8u * CTL);
    pre = seg_rms(buf_ctrl, at - 8u * CTL, at);
    post = seg_rms(buf_ctrl, at + 16u * CTL, to);
    cases++;
    printf("morph: %-36s level %.0f -> %.0f  step %5d vs %5d before -> %s\n",
           "preset load under six parts + FX", pre, post, across, before,
           across <= before * 2 + 128 && post > 0 ? "ok (no click)" : "FAIL");
    if (across > before * 2 + 128 || post <= 0)
        fails++;
}

int main(void)
{
    printf("morph: the transition under load, on the real render path (six parts, FX, MIDI, presets)\n");
    probe_morph("6 parts, the keys", SC_KEYS);
    probe_morph("6 parts + FX buses + clipper", SC_KEYS | SC_FX);
    probe_morph("6 parts + FX, notes over MIDI", SC_KEYS | SC_FX | SC_MIDI);
    probe_morph("6 parts + FX, MIDI lands as it turns", SC_KEYS | SC_FX | SC_MIDI | SC_MORPH_MIDI);
    probe_morph("6 parts + FX, after a preset load", SC_KEYS | SC_FX | SC_PRESET | SC_MORPH_AFTER);
    probe_preset();
    printf("\nmorph: %u transitions, %d failed\n", cases, fails);
    return fails ? 1 : 0;
}
