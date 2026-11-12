/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A bench for the TIMER5 tick (hal/fm1_input.h fm1_input_tick), so a change to the key/LED scan can be
 * compared with a number instead of an instruction count nobody can take here (the pi32v2 disassembly
 * needs the JieLi toolchain; tests/target_budget.py skips without it).
 *
 *   tests/isr_bench [ticks]
 *
 * One tick is 100 us on the device: one column of the 11, its 595 shift, its keys and, every 11 ticks,
 * the frame (debounce, encoders, breath). The bench counts the same ticks the firmware does, with the
 * GPIO modelled as plain memory and no simulated bus time, so what it measures is the C the tick runs --
 * on this host, x86-64, not on the FM-1.
 *
 * Scenes:
 *   idle      nothing lit, nothing breathing, no key down: the floor.
 *   leds      16 LEDs lit and one column breathing: every pulse writes the shift twice, the breath's
 *             frame math runs, the mid level is on.
 *   keys      the same plus a whole column of keys closed (the debounce early-returns: a held key).
 *
 * Prints process CPU time per tick for each scene (ns), the wall clock beside it, and the share of each
 * stage from a second pass that calls the pieces directly. Like tests/fx_bench.c it is built by the suite
 * and not run by it: it measures, it does not approve. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint32_t host_now;                          /* TIMER4 ticks (24 MHz); the bench does not advance it */
#define FM1_INPUT_NOW() host_now
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];         /* the GPIO registers, as memory */
#undef FM1_PR
#define FM1_PR(p, r) (*(&host_reg[(p) & 15u][((r) / 4u) & 63u]))
#define fm1_ticks host_ticks
static uint32_t host_ticks(void) { return host_now; }
#include "../firmware/hal/fm1_input.h"

static double now_cpu_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void scene_idle(void)
{
    uint32_t c;
    fm1_led_anim_start(0);
    fm1__an_f = 0;                                  /* the sweep is over: the UI's picture */
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[c] = fm1_led_breath[c] = fm1_led_mid[c] = 0;
    host_reg[FM1_PA][FM1_IN / 4u] = 0x3Fu;          /* rows pulled up: nothing closed */
    host_reg[FM1_PB][FM1_IN / 4u] = 0xFFu;
}
static void scene_leds(void)
{
    uint32_t c;
    scene_idle();
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = 0x0Eu;                         /* the four lit rows */
    fm1_led_breath[3] = 0x02u;                      /* one column breathing: every pulse's two writes */
    fm1_led_mid[5] = 0x04u;                         /* and the mid level on */
    fm1_led_dim[7] = 0x08u;                         /* and the glow */
}
static void scene_keys(void)
{
    scene_leds();
    host_reg[FM1_PA][FM1_IN / 4u] = 0x00u;          /* every row of the column reads closed */
    host_reg[FM1_PB][FM1_IN / 4u] = 0x00u;
}

/* a stage's cost, alone, over the same number of calls: what the tick spends in each piece. The pieces
 * are the header's own (static inline): the call is the measurement boundary. */
static double stage_ns(void (*f)(void), uint32_t n)
{
    double c0, c1;
    uint32_t i;
    c0 = now_cpu_s();
    for (i = 0; i < n; i++)
        f();
    c1 = now_cpu_s();
    return (c1 - c0) * 1e9 / (double)n;
}
static uint32_t sw_;
static void st_shift(void) { fm1__sr_bits(sw_ ^ 0x1u, 0, 16u); }
static void st_latch(void) { fm1__sr_latch(); }
static void st_lines(void) { fm1__led_lines(0x0Eu); }
static void st_rows(void) { fm1_in.raw[0] = (uint8_t)fm1__rows(); }
static void st_keys(void) { fm1__keys(2u); }
static void st_frame(void) { fm1__frame(); }
static void st_breath(void) { fm1__breath_frame(); }

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 200000u, i, r;
    static const char *const SCENE[3] = {"idle (nothing lit, nothing down)", "leds (16 lit, one breathing)",
                                         "keys (the same, a column held)"};
    void (*const SET[3])(void) = {scene_idle, scene_leds, scene_keys};

    printf("isr_bench: host x86, the TIMER5 tick (one column of 11, %u ticks = %u frames)\n", n, n / FM1_NCOL);
    for (r = 0; r < 3u; r++) {
        double c0, c1, w0, w1;
        fm1_input_init();
        SET[r]();
        for (i = 0; i < 5000u; i++)                  /* warm (5 frames) */
            fm1_input_tick();
        c0 = now_cpu_s();
        w0 = now_s();
        for (i = 0; i < n; i++)
            fm1_input_tick();
        c1 = now_cpu_s();
        w1 = now_s();
        printf("  scene %u %-38s %7.1f ns CPU / tick, %7.1f ns wall (%.2f %% of the 100 us tick)\n",
               r, SCENE[r], (c1 - c0) * 1e9 / (double)n, (w1 - w0) * 1e9 / (double)n,
               (c1 - c0) * 1e9 / (double)n / 1000.0);
    }
    fm1_input_init();
    scene_keys();
    printf("  stage, alone (CPU ns per call):\n");
    printf("    fm1__sr_bits x16 bits   %7.1f\n", stage_ns(st_shift, n));
    printf("    fm1__sr_latch           %7.1f\n", stage_ns(st_latch, n));
    printf("    fm1__led_lines          %7.1f\n", stage_ns(st_lines, n));
    printf("    fm1__rows               %7.1f\n", stage_ns(st_rows, n));
    printf("    fm1__keys (one column)  %7.1f\n", stage_ns(st_keys, n / FM1_NCOL));
    printf("    fm1__frame (11 ticks)   %7.1f\n", stage_ns(st_frame, n / FM1_NCOL));
    printf("    fm1__breath_frame       %7.1f\n", stage_ns(st_breath, n / FM1_NCOL));
    return 0;
}
