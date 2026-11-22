/* (v1.7.21) HOW MUCH MIXING A FRAME ASKS FOR, as a COUNT. The veil over the visualizer's scope well was 14 656 px
 * of translucent fill a frame, each one a full ux_mix -- two colour unpacks, three multiplies, three divisions by
 * 100 and three clamps -- 19 052 blends a frame, and the frame cost 348 us on the host. With the channel tables
 * (gfx.c, cv_rect_a) the same pixels cost 4 396 blends and 180 us. A time would flake on a busy host; the number
 * of blends is a property of the code, so it is what this test holds: a screen that comes back to mixing pixel by
 * pixel over a big area fails here, whichever machine runs it.
 *
 * The caps are the measured count with headroom for a screen that gains a gradient on purpose (+-25 %), not a
 * rubber stamp: the visualizer's own cap is a fifth of what the veil alone used to cost.
 *
 * Run: the host harness (tests/ui_render.c as a library, UI_NO_DRIFT), no drift, fixed clock. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../build/capture/ui_render_nomain.c"

extern uint32_t gfx_mix_n;                          /* (gfx.c, GFX_MIX_COUNT) one increment a blend */

static FILE *sink;

static uint32_t blends_in_a_frame(int screen)
{
    uint32_t before;
    int i;
    setup(screen);
    pal(UI_QUANTUM_INDEX);
    song.playing = 0;
    ui.force = 1;
    fm1_ms = 4000u;
    host_ticks = 6000000u;
    for (i = 0; i < 8; i++)                         /* the caches warm, the animations settle */
        frame();
    before = gfx_mix_n;
    frame();
    return gfx_mix_n - before;
}

struct cap { const char *name; int screen; uint32_t cap; };

int main(void)
{
    static const struct cap caps[] = {
        {"home_viz (the visualizer: two veils, the deep field, the trace)", S_HOME_VIZ, 6000u},
        {"home_eco (HOME's ecosystem)", S_HOME, 200u},
        {"presets (the browser)", S_PRESETS, 200u},
        {"mixer (the meters)", S_HOME_MIX, 200u},
        {"overview (HOME 2/5)", S_HOME_OVERVIEW, 200u},
        {"drum", S_DRUM, 200u},
    };
    unsigned i, bad = 0;

    sink = fopen("/dev/null", "w");
    rep = sink;
    al_rep = sink;
    aud = sink;

    printf("blend budget: blends a frame (a count, not a time -- the host's clock does not enter it)\n");
    for (i = 0; i < sizeof caps / sizeof caps[0]; i++) {
        uint32_t n = blends_in_a_frame(caps[i].screen);
        if (n > caps[i].cap) {
            printf("  FAIL  %-64s %6u blends, cap %u\n", caps[i].name, n, caps[i].cap);
            bad++;
        } else {
            printf("  ok    %-64s %6u blends, cap %u\n", caps[i].name, n, caps[i].cap);
        }
    }
    fclose(sink);
    if (bad) {
        printf("blend budget: %u of %u screens over the cap -- a per-pixel mix came back into a frame\n",
               bad, (unsigned)(sizeof caps / sizeof caps[0]));
        return 1;
    }
    printf("blend budget: all passed\n");
    return 0;
}
