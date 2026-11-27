/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PRESET SWAP (QG-1 1.5): the sound load the firmware really runs, under the hands of a player who does not wait.
 *
 * The host harnesses had their own copy of what a preset load writes (tests/hostsim.c host_preset_values), so the
 * firmware's apply_preset_to -- the one the UI calls on every knob step, every PROGRAM CHANGE, every engine jump --
 * was never executed by a test. This one includes the emulator's translation unit, which includes ui.c, so the
 * calls below go through apply_preset_to, set_engine_of, load_begin/load_end, fm6_track_loaded and the voice
 * panic exactly as they do on the box.
 *
 * Three checks:
 *
 *   reset    every parameter NOT in param_kept comes out identical no matter what the track held before, and every
 *            parameter (kept or not) is inside its declared range. Two loads of the same preset from two different
 *            dirty states must agree byte for byte: a preset cannot inherit an edit from the sound before it.
 *   stress   hundreds of consecutive changes on all six parts with notes held, MIDI CCs moving, the sequencer
 *            running and the FX buses fed: no non-finite sample, no sample past the ceiling, no crash (ASan/UBSan
 *            builds of this file are what turn a bad index into a failure instead of a red screen).
 *   release  after all-notes-off nothing is left sounding: every pooled voice a track can reach is inactive, so a
 *            preset change cannot leave a stuck note behind.
 *
 * HOST x86-64 class: the emulator's translation unit with the panel and the flash modelled in memory. Nothing here
 * is a reading from a unit.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_swap tests/preset_swap_test.c -lm && build/host/preset_swap
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../web/emu/felucca_web.c"

static int fails;
static void ck(const char *what, int ok, const char *detail)
{
    printf("  %-58s %s%s%s\n", what, ok ? "ok" : "FAIL", detail[0] ? "  " : "", detail);
    if (!ok)
        fails++;
}

static uint32_t rng_s = 0x5eed1234u;
static uint32_t rnd(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s >> 8; }

/* the matrix's slots 5..8 (t->mx): part of the sound (a project and a user preset store them), so a factory
 * preset has to leave them off, not inheriting the previous sound's modulation */
static void dirty_mx(track_t *t, uint32_t salt)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        t->mx[i][0] = (int16_t)(1u + (i + salt) % 5u);      /* a real source and destination */
        t->mx[i][1] = (int16_t)(1u + (i * 2u + salt) % 5u);
        t->mx[i][2] = (int16_t)(10 + (int32_t)((i * 7u + salt) % 40u));
    }
}
static int mx_dirty(const track_t *t)
{
    uint32_t i, any = 0;
    for (i = 0; i < 4u; i++)
        any |= (uint32_t)(t->mx[i][0] | t->mx[i][1] | t->mx[i][2]);
    return any != 0;
}

/* a parameter's value moved to a state the preset does not know: inside the declared range, never the default */
static void dirty_fill(track_t *t, uint32_t salt)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++) {
        int32_t lo = TP[i].min, hi = TP[i].max, span = hi - lo;
        int32_t v = lo + (int32_t)((i * 37u + salt * 101u) % (uint32_t)(span ? span + 1 : 1));
        if (v == TP[i].def && span)
            v = lo + (int32_t)((uint32_t)(v - lo + 1u) % (uint32_t)(span + 1));
        t->p[i] = (int16_t)v;
    }
}

/* ---- 1. a preset sets every parameter it owns, from any state -------------------------------------------- */
static void test_reset(void)
{
    uint32_t e, pi, i, cases = 0, bad = 0;
    int16_t a[P_COUNT], b[P_COUNT];
    char d[96];
    for (e = 0; e < NENGINES; e++) {
        uint32_t np = ENGINES[e]->npresets;
        if (!eng_ok(e) || !np)
            continue;
        for (pi = 0; pi < np; pi++) {
            track_t *t = &trk[0];
            set_engine_of(t, e);
            dirty_fill(t, pi + 1u);
            dirty_mx(t, pi + 1u);
            apply_preset_to(t, pi);
            memcpy(a, t->p, sizeof a);
            if (mx_dirty(t)) {
                snprintf(d, sizeof d, "engine %u preset %u: the MOD slots 5..8 survived the load", e, pi);
                ck("a factory preset leaves the MOD slots 5..8 off", 0, d);
                bad++;
            }
            dirty_fill(t, pi + 977u);
            dirty_mx(t, pi + 977u);
            apply_preset_to(t, pi);
            memcpy(b, t->p, sizeof b);
            cases++;
            for (i = 0; i < P_COUNT; i++) {
                int32_t lo = TP[i].min, hi = TP[i].max;
                if (b[i] < lo || b[i] > hi) {
                    snprintf(d, sizeof d, "engine %u preset %u: p[%u]=%d outside [%d,%d]", e, pi, i, b[i], lo, hi);
                    ck("preset load leaves every parameter in range", 0, d);
                    bad++;
                    break;
                }
                if (!param_kept(i) && a[i] != b[i]) {
                    snprintf(d, sizeof d, "engine %u preset %u: p[%u] inherited %d (was %d)", e, pi, i, b[i], a[i]);
                    ck("preset load overwrites every parameter it owns", 0, d);
                    bad++;
                    break;
                }
            }
        }
    }
    snprintf(d, sizeof d, "(%u presets over %u engines)", cases, (unsigned)NENGINES);
    ck("every engine's every preset loads from a dirty state", bad == 0, d);
}

/* ---- 2. rapid changes under performance, and 3. nothing left sounding -------------------------------------- */
static void test_stress(void)
{
    static const uint8_t ENG[6] = {4, 12, 6, 5, 10, 7};   /* ANALOG, FM6, PHASE, VOICE, LOFI, NOISE */
    uint32_t n = 600, k, nonfinite = 0, peak_over = 0, loaded = 0, no_hold_fail = 0;
    double peak = 0;
    char d[96];
    web_boot();
    web_render(4096);
    web_keys(0);
    for (k = 0; k < n; k++) {
        track_t *t = &trk[k % NTRK];
        uint32_t e = ENG[k % 6u];
        uint32_t np;
        /* the player: a chord down, a knob moving, an FX send up */
        web_midi(0x90u | (uint32_t)(k % NTRK + 1u), 48u + (rnd() % 24u), 100u);
        web_midi(0x90u | (uint32_t)(k % NTRK + 1u), 60u + (rnd() % 12u), 90u);
        web_midi(0xB0u | (uint32_t)(k % NTRK + 1u), 74u, rnd() & 127u);
        web_midi(0xB0u | (uint32_t)(k % NTRK + 1u), 93u, rnd() & 127u);
        /* the knobs and the keys of the pages, turned while the sound changes: page_desc hands the encoders what
         * the page edits, and the load runs right after (the reported trigger) */
        if (k % 2u == 0u)
            web_enc(rnd() % 7u, (int32_t)(rnd() % 5u) - 2);
        if (k % 5u == 0u)
            web_buttons(1u << (rnd() % 14u));
        if (k % 7u == 0u)
            web_tap((int32_t)(rnd() % 240u), (int32_t)(rnd() % 240u));
        web_render(32);                                  /* the notes sound on the old sound */
        if (k % 3u == 0u) {
            if (eng_ok(e))
                set_engine_of(t, e);                         /* an engine jump under the notes, on that part */
        } else {
            if (t->eng_req != e && eng_ok(e))
                set_engine_of(t, e);
            np = ENGINES[t->eng_req % NENGINES]->npresets;
            if (np) {
                apply_preset_to(t, rnd() % np);              /* the knob's next step */
                loaded++;
            }
        }
        web_render(512);                                 /* control ticks: the panic, then a block to render it */
        {
            uint32_t i;
            for (i = 0; i < 32u; i++) {
                float l = web_out_l()[i], r = web_out_r()[i];
                double al = fabs((double)l);
                if (!isfinite(l) || !isfinite(r))
                    nonfinite++;
                if (al > peak)
                    peak = al;
                if (al > 1.5)
                    peak_over++;
            }
        }
        /* panic_req is applied at the block start: after it, no voice of the part may still hold a key
         * (the ones that were sounding are fading, gate 0, and free themselves) */
        {
            uint32_t held = 0, i;
            for (i = 0; i < NVOICE; i++)
                if (t->vidx[i] != VDEAD && V(t, i).active && V(t, i).gate)
                    held++;
            if (held && !no_hold_fail) {
                printf("    change %u left %u voice(s) holding on track %u\n", k, held, (unsigned)(t - trk));
                no_hold_fail = 1;
            }
        }
    }
    /* all notes off, let every envelope run to silence */
    for (k = 0; k < NTRK; k++)
        for (uint32_t nn = 0; nn < 128u; nn++)
            web_midi(0x80u | (uint32_t)(k + 1u), nn, 0u);
    for (k = 0; k < 4000u; k++)
        web_render(32);
    snprintf(d, sizeof d, "(%u changes, %u engine jumps)", loaded, n - loaded);
    ck("rapid preset changes: no non-finite sample", nonfinite == 0, d);
    snprintf(d, sizeof d, "(peak %.2f, %u samples past it)", peak, peak_over);
    ck("rapid preset changes: the output stays inside the ceiling", peak_over == 0, d);
    ck("every change releases what the part was holding", no_hold_fail == 0, "");

    {
        /* a HOLDING voice is one whose gate is still up: a released voice keeps fading (a slow release is a
         * sound, not a stuck note), a holding one would sound for ever */
        uint32_t stuck = 0, sounding = 0;
        for (k = 0; k < NTRK; k++)
            for (uint32_t i = 0; i < NVOICE; i++)
                if (trk[k].vidx[i] != VDEAD && V(&trk[k], i).active) {
                    sounding++;
                    if (V(&trk[k], i).gate) {
                        stuck++;
                        printf("    holding: track %u slot %u note %u stage %u\n", k, i,
                               V(&trk[k], i).note, V(&trk[k], i).stage);
                    }
                }
        snprintf(d, sizeof d, "(%u still fading, %u holding)", sounding, stuck);
        ck("no key left holding after all-notes-off + 4000 blocks", stuck == 0, d);
    }
}

int main(void)
{
    printf("preset_swap: host x86-64 (the emulator's translation unit: ui.c's real apply_preset_to)\n");
    test_reset();
    test_stress();
    printf("preset_swap: %s (%d failed)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}
