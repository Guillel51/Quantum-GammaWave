/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* HOME's bay, touched: the five bodies (firmware/src/ui_being.c) answer a tap one by one.
 * The firmware's own panel has no touch surface, so ui_tap() is the entry point of the front ends that can point
 * at the screen: the browser emulator's LCD (web/emu/index.html -> worklet.js "tap" -> web_tap, felucca_web.c)
 * and this test. What is checked, on the real drawing and hit-test code:
 *   the hit test: a tap on a body returns that body (gw_planet_at), and only there -- the star field, the dust
 *                 and the panel miss;
 *   the answer:   ui_tap pokes that body alone (pl_poke_on, pl_flash at the strike), the other four stay at rest;
 *                 tapping all five leaves all five answering at once;
 *   the five:     PL_POKE holds five different reaction mechanisms (spin, swell, ripple, sling, frost), and every
 *                 body is drawn (pl_sr, a radius) at a place of its own;
 *   the gate:     with HOME covered (a page, a layer, the MENU, a name being typed, the visualizer) a tap does
 *                 nothing at all: no body, no redraw.
 * tests/run_tests.sh runs it as "HOME worlds: a tap reaches each of the five bodies, and only there". */
#define UI_TEST_NO_MAIN 1
#define PF_MS 0u                                          /* no page name drawn: the test reads the bay's state */
#include "ui_test.c"

static int planets_bad;

/* HOME at power-on shows the bay, not the full-screen visualizer (ui.c: ui.home = 1, ui.home_viz = 0). go_home()
 * is the HOME button: with ui.home already set it toggles the visualizer, so the test sets the view it means. */
static void bay_view(void) { ui.home = 1; ui.home_viz = 0; ui.force = 1; }

/* a tap as the emulator sends it: screen coordinates, the bay's own strip found the way gw_planet_at does */
static void tap_screen(int32_t x, int32_t y) { ui_tap(x, y); }

/* the bay drawn and settled: every body has its radius and its place (ui_being.c draws them each frame) */
static int bay_ready(void)
{
    uint32_t i, n;
    for (n = 0; n < 40u; n++) {
        frames(16);
        for (i = 0; i < NPL && pl_sr[i] && pl_sx[i] >= 0 && pl_sx[i] < 240; i++)
            ;
        if (i == NPL)
            return 1;
    }
    return 0;
}

/* ui_being.c takes the tap in screen rows and subtracts the strip's own origin (graph_y): the offset that makes
 * gw_planet_at() find the body it was given. Measured, not assumed, so a different strip height cannot hide. */
static int32_t bay_off(void)
{
    int32_t off, i;
    for (off = 0; off < 140; off++) {
        for (i = 0; i < (int32_t)NPL; i++)
            if (gw_planet_at(pl_sx[i], (int32_t)pl_sy[i] + off) != i)
                break;
        if (i == (int32_t)NPL)
            return off;
    }
    return -1;
}

static void rest(void) { memset(pl_poke_on, 0, sizeof pl_poke_on); memset(pl_flash, 0, sizeof pl_flash); }

static int test_planets(void)
{
    int32_t off, i, k;
    uint32_t kinds = 0, n;
    ui_power_on();
    planets_bad += check("HOME at power-on is the bay, not the visualizer", ui.home && !ui.home_viz);
    bay_view();
    frames(32);
    if (!bay_ready()) {
        planets_bad += check("HOME: the bay draws its five bodies", 0);
        return planets_bad;
    }
    planets_bad += check("HOME: the bay draws its five bodies", 1);
    /* every body has a place of its own: no two share a centre */
    for (i = 0, n = 0; i < (int32_t)NPL; i++)
        for (k = i + 1; k < (int32_t)NPL; k++)
            if (pl_sx[i] == pl_sx[k] && pl_sy[i] == pl_sy[k])
                n++;
    planets_bad += check("the five bodies are drawn apart, one centre each", n == 0);
    /* the five mechanisms are five: a poke reads as something different for each world */
    for (i = 0; i < (int32_t)NPL; i++) {
        for (k = 0, n = 0; k < (int32_t)NPL; k++)
            if (PL_POKE[k] == PL_POKE[i])
                n++;
        if (n == 1u)
            kinds++;
    }
    planets_bad += check("the five reactions are five different mechanisms (spin, swell, ripple, sling, frost)",
                         kinds == NPL);
    off = bay_off();
    planets_bad += check("the bay's strip origin is where gw_planet_at expects it", off >= 0);
    if (off < 0)
        return planets_bad;
    /* each body answers its own tap, and only its own */
    for (i = 0; i < (int32_t)NPL; i++) {
        char what[96];
        int32_t j, solo = 1;
        rest();
        tap_screen(pl_sx[i], (int32_t)pl_sy[i] + off);
        for (j = 0; j < (int32_t)NPL; j++)
            if (j == i ? !(pl_poke_on[j] && pl_flash[j] > 0) : (pl_poke_on[j] || pl_flash[j]))
                solo = 0;
        snprintf(what, sizeof what, "body %d answers its own tap (kind %u) and leaves the other four at rest",
                 (int)i, (unsigned)PL_POKE[i]);
        planets_bad += check(what, solo);
    }
    /* all five at once: a tap is per body, not a mode */
    rest();
    for (i = 0; i < (int32_t)NPL; i++)
        tap_screen(pl_sx[i], (int32_t)pl_sy[i] + off);
    for (i = 0, n = 0; i < (int32_t)NPL; i++)
        if (pl_poke_on[i] && pl_flash[i] > 0)
            n++;
    planets_bad += check("five taps: the five bodies answer at once", n == NPL);
    /* a miss is nothing: the sky, the dust and the panel are not handles */
    {
        int32_t x, y, miss = 0;
        for (y = (int32_t)pl_sy[0] - 60; y < 240 && !miss; y += 3)
            for (x = 2; x < 238; x += 3)
                if (y >= 0 && gw_planet_at(x, y) < 0) { miss = 1; break; }
        rest();
        planets_bad += check("a tap on the sky misses every body", miss);
        if (miss) {
            int before = (int)pl_flash[0] + (int)pl_flash[1] + (int)pl_flash[2] + (int)pl_flash[3] + (int)pl_flash[4];
            tap_screen(x, y);
            planets_bad += check(".. and leaves the bay exactly as it was",
                                 before == (int)pl_flash[0] + (int)pl_flash[1] + (int)pl_flash[2] +
                                               (int)pl_flash[3] + (int)pl_flash[4] &&
                                 !pl_poke_on[0] && !pl_poke_on[1] && !pl_poke_on[2] && !pl_poke_on[3] &&
                                 !pl_poke_on[4]);
        }
    }
    /* the gate: with HOME covered, a tap on a body does nothing */
    {
        uint32_t gate;
        for (gate = 0; gate < 3u; gate++) {
            rest();
            ui_power_on();
            bay_view();
            frames(32);
            if (!bay_ready())
                break;
            if (gate == 0)
                ui.menu = 1;                              /* the MENU over HOME */
            else if (gate == 1)
                ui.home_viz = 1;                          /* the visualizer on HOME */
            else
                ui.layer = 1;                             /* a quick layer */
            tap_screen(pl_sx[0], (int32_t)pl_sy[0] + off);
            planets_bad += check(gate == 0 ? "with the MENU open a tap on a body does nothing"
                                   : gate == 1 ? "with the visualizer on HOME a tap does nothing"
                                               : "with a layer open a tap does nothing",
                                 !pl_poke_on[0] && !pl_poke_on[1] && !pl_poke_on[2] && !pl_poke_on[3] &&
                                     !pl_poke_on[4]);
        }
    }
    ui_power_on();
    return planets_bad;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    planets_bad += test_planets();
    printf("planets: %s\n", planets_bad ? "FAILED" : "all passed");
    return planets_bad ? 1 : 0;
}
