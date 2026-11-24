/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* HOW DEEP THE STACK ACTUALLY GOES, in the scenes that stress it, measured instead of assumed.
 *
 *   tests/stack_watermark
 *
 * app.ld gives the user stack 13.75 KiB and the exception stack 7.75 KiB, both behind 256-byte guard
 * bands, and the firmware's compiler has no stack-usage report (the JieLi clang rejects -fstack-usage).
 * So this paints the memory below the current stack pointer, runs the worst things the instrument does,
 * and reads back the boundary: the deepest byte the C wrote over the pattern.
 *
 * The class of the number is HOST x86-64. Frame sizes are the host's ABI, not pi32v2's, so this is not a
 * measurement of the FM-1's stack. What it does prove is the shape of the C -- which chain is the deep
 * one, and that no scene recurses through the audio or project path -- and it fails loudly if a change
 * makes that chain deeper than BUDGET. The target's own margin is the linker's: 13.75 / 7.75 KiB
 * against frames this test shows are two orders of magnitude smaller.
 *
 * Scenes, each after its own paint(): a six-track mix with every bus fed (the audio ISR's call chain),
 * a project captured + packed + unpacked (the largest single staging operation in the firmware), and
 * forty UI frames (input, LEDs, draw -- the main loop's chain, which shares core 0 with the audio).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

/* the whole firmware as the capture harness builds it: audio, project and UI, on the stubs ui_render.c uses
 * (tests/frame_bench.c includes the same file for the same reason) */
#include "../build/capture/ui_render_nomain.c"

#define PAINT     (192u * 1024u)     /* how far below the stack pointer the pattern goes */
#define MAGIC     0xA5u
#define BUDGET    24576u             /* the mark must stay under this, in this host's bytes */

static uint8_t *painted;             /* [painted, mark_top): below the frames that are live now */
static uint8_t *mark_top;            /* its highest address (closest to the stack in use) */

/* The paint is a mapped region, not a guess below the stack pointer: writing megabytes below the SP of
 * the main thread faults (the kernel only grows the stack for addresses inside its guard gap), so each
 * scene runs in a thread whose stack IS an allocated buffer, and the pattern lives in the lower half of
 * it. Everything the scene pushes goes down into that buffer, so the boundary read back is exact. */
static uint8_t *tbuf;                        /* the thread's stack buffer (mapped, so the paint is safe) */
#define STACKSZ  (PAINT + 65536u)               /* the pattern plus room for the live frames above it */

static __attribute__((noinline)) void paint(void)
{
    volatile uint8_t probe;
    uint8_t *sp = (uint8_t *)&probe;
    mark_top = sp - 256u;                       /* leave paint()'s own frame out of the mark */
    painted = mark_top - PAINT;
    if (painted < tbuf)                         /* the thread started too low: not possible with STACKSZ */
        painted = tbuf;
    memset(painted, MAGIC, (size_t)(mark_top - painted));
}

/* The deepest write: the pattern is intact from the bottom up to the first byte the stack overwrote. */
static uint32_t used(void)
{
    uint8_t *p = painted;
    while (p < mark_top && *p == MAGIC)
        p++;
    return (uint32_t)(mark_top - p);
}

/* the audio ISR's chain: six parts sounding, every send open, thirty-two blocks */
static void scene_mix(void)
{
    static const uint8_t NOTES[3] = {48, 55, 60};
    uint32_t p, i, b;
    int32_t out[2 * 32];
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], p % 3u, p + 1u);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_SUS] = 127;
        for (i = 0; i < 4u; i++)
            trk[p].p[P_DIST + i] = 96;          /* the four sends wide open: every bus fed */
        for (i = 0; i < 3u; i++)
            trk_note_on(&trk[p], (uint8_t)(NOTES[i] + 12u * p), 100);
    }
    for (b = 0; b < 32u; b++)
        mix_block((int32_t *)out, 32);
}

/* the largest staging operation: a whole project captured, packed and decoded again */
static void scene_project(void)
{
    project_capture(&proj_scratch);
    if (proj_pack(&proj_wire, &proj_scratch))
        proj_unpack(&proj_scratch, (const uint8_t *)&proj_wire, sizeof proj_wire);
}

/* the main loop's chain on a dense page */
static void scene_ui(void)
{
    uint32_t i;
    for (i = 0; i < 40u; i++) {
        ui.frame++;
        ui_input();
        ui_leds();
        ui_draw();
    }
}

static void *entry(void *arg)
{
    void (*fn)(void) = (void (*)(void))arg;
    paint();
    fn();
    return NULL;
}

static uint32_t run(const char *name, void (*fn)(void))
{
    pthread_attr_t at;
    pthread_t th;
    uint32_t m;
    tbuf = (uint8_t *)malloc(STACKSZ);
    if (!tbuf) {
        printf("stack: no room for a %u B thread stack\n", STACKSZ);
        exit(2);
    }
    memset(tbuf, 0, STACKSZ);
    pthread_attr_init(&at);
    pthread_attr_setstack(&at, tbuf, STACKSZ);
    pthread_create(&th, &at, entry, (void *)fn);
    pthread_join(th, NULL);
    pthread_attr_destroy(&at);
    m = used();
    free(tbuf);
    printf("stack: %-40s %6u B of the %u B painted\n", name, m, PAINT);
    return m;
}

int main(void)
{
    uint32_t a, b, c, worst;
    printf("stack: host x86-64; the pattern is painted %u B below paint()'s frame\n", PAINT);
    fflush(stdout);
    state();                                    /* the capture harness's init, as frame_bench does */
    setup(S_PRESETS);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.msg_t = ui.msg2[0] = 0;
    ui.menu = ui.confirm = ui.layer = 0;
    fm1_ms = 10000u;
    ui.frame = 200u;
    a = run("mix, 6 tracks, every bus fed", scene_mix);
    b = run("project capture + pack + unpack", scene_project);
    c = run("40 UI frames (input, leds, draw)", scene_ui);
    worst = a > b ? (a > c ? a : c) : (b > c ? b : c);
    if (worst > BUDGET) {
        printf("stack: FAIL -- %u B is over the %u B budget (host frames)\n", worst, BUDGET);
        return 1;
    }
    printf("stack: ok -- deepest %u B, inside the %u B budget (host x86-64, not the unit)\n", worst, BUDGET);
    return 0;
}
