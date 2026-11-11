/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (3.6) The FX page's meters: the ballistics the page uses for each of its four bars (core.h,
 * fx_meter_step) as arithmetic. Host x86, no hardware and no DSP: this pins the behaviour of the helper
 * that replaced the page's dead meter code (vu[]/hold[] were declared and drawn but never written, so the
 * trail, the hold line and the send tick drew nothing at all).
 *
 *   attack   the level the DSP measured this block is shown at once
 *   release  a quarter of the way each frame, never rising, reaching 0
 *   hold     stands 30 frames over the fallen bar, then falls a pixel a frame
 *   clamp    a target past the meter's height is the meter's height
 *
 * The drawing itself (the bar, the hold line, the clip mark) is measured by tests/ui_render.c, and the
 * levels come from fx.c's real bus peaks (fx_vu), pinned by tests/perform_test.c. */
#include <stdio.h>
#include <stdint.h>
#include "../firmware/src/core.h"

static int bad;

static void check(const char *what, int ok)
{
    printf("%-78s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
}

int main(void)
{
    uint8_t vu, hold, ht;
    int32_t i, v, prev;

    {   /* attack: immediate, and the hold follows the new peak */
        vu = hold = ht = 0;
        v = fx_meter_step(&vu, &hold, &ht, 18, 42);
        check("attack: a new level shows at once (18 px), held at it",
              v == 18 && vu == 18 && hold == 18 && ht == 0);
        v = fx_meter_step(&vu, &hold, &ht, 30, 42);
        check("attack: a higher level too (30 px), the hold re-armed",
              v == 30 && hold == 30 && ht == 0);
    }
    {   /* release: monotone, a quarter of the way, to nothing */
        int ok = 1;
        vu = hold = ht = 0;
        fx_meter_step(&vu, &hold, &ht, 30, 42);
        prev = vu;
        for (i = 0; i < 64 && vu; i++) {
            int32_t n = fx_meter_step(&vu, &hold, &ht, 0, 42);
            if (n > prev || n < 0)
                ok = 0;
            prev = n;
        }
        check("release: falls a quarter of the way a frame, never rises, reaches 0",
              ok && vu == 0 && i > 1);
    }
    {   /* clamping */
        vu = hold = ht = 0;
        v = fx_meter_step(&vu, &hold, &ht, 9999, 42);
        check("clamp: a target past the height is the height (42)", v == 42 && vu == 42);
        vu = hold = ht = 0;
        v = fx_meter_step(&vu, &hold, &ht, -50, 42);
        check("clamp: a negative target is silence", v == 0 && vu == 0 && hold == 0);
    }
    {   /* the peak hold: 30 frames over the fallen bar, then a pixel a frame */
        vu = hold = ht = 0;
        fx_meter_step(&vu, &hold, &ht, 18, 42);
        for (i = 0; i < 8; i++)
            fx_meter_step(&vu, &hold, &ht, 0, 42);
        check("hold: the bar has fallen (0) and the hold still stands at the peak (18)",
              vu == 0 && hold == 18);
        for (i = 0; i < 21; i++)                      /* frames 9..29 of the hold */
            fx_meter_step(&vu, &hold, &ht, 0, 42);
        check("hold: still standing 29 frames after the peak (18)", hold == 18 && ht == 29);
        fx_meter_step(&vu, &hold, &ht, 0, 42);        /* the 30th */
        check("hold: the 30th frame starts it down (17)", hold == 17);
        prev = hold;
        {   int ok = 1;
            for (i = 0; i < 40 && hold; i++) {
                fx_meter_step(&vu, &hold, &ht, 0, 42);
                if (hold != 0 && hold != prev - 1)
                    ok = 0;
                prev = hold;
            }
            check("hold: falls one pixel a frame, down to nothing", ok && hold == 0);
        }
    }
    {   /* a level below the hold leaves it standing, and the hold's clock keeps running */
        vu = 0;
        hold = 10;
        ht = 10;
        fx_meter_step(&vu, &hold, &ht, 4, 42);
        check("hold: a lower level does not re-arm it; its clock keeps running (10, 11)",
              hold == 10 && ht == 11);
    }
    {   /* once its 30 frames are spent the hold keeps falling, unless a new peak re-arms it */
        vu = 0;
        hold = 10;
        ht = 30;
        fx_meter_step(&vu, &hold, &ht, 4, 42);
        check("hold: spent, it keeps falling on a lower level (9, 30)", hold == 9 && ht == 30);
        vu = 0;
        hold = 10;
        ht = 30;
        fx_meter_step(&vu, &hold, &ht, 12, 42);
        check("hold: a new peak re-arms it at once (12, 0)", hold == 12 && ht == 0);
    }
    {   /* silence from the start: nothing lit, nothing held, and it stays that way */
        vu = hold = ht = 0;
        for (i = 0; i < 100; i++)
            fx_meter_step(&vu, &hold, &ht, 0, 42);
        check("silence: nothing lit and nothing held, however long it runs",
              vu == 0 && hold == 0);
    }
    printf("%s\n", bad ? "fx meter test FAILED" : "fx meter test passed");
    return bad ? 1 : 0;
}
