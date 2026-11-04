/* SPDX-License-Identifier: GPL-3.0-only
 * Consonant random intervals (V10.38, adapted): every value param_semi_pick returns is one of the
 * musical intervals and inside the descriptor's range; param_semi_snap returns the nearest one, and a
 * value the range cannot hold falls back to unison. Host only; no DSP and no hardware. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int check(const char *what, int ok)
{
    printf("semi: %-70s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int consonant(int32_t v)
{
    return v == 0 || v == 5 || v == -5 || v == 7 || v == -7 || v == 12 || v == -12 || v == 19 || v == -19;
}

int main(void)
{
    int bad = 0;
    uint32_t i;
    int32_t v;
    for (i = 0; i < 4096u; i++) {
        v = param_semi_pick(i, -24, 24);
        if (!consonant(v) || v < -24 || v > 24) {
            printf("semi: pick(%u) = %d is not a consonant interval\n", i, (int)v);
            bad++;
            break;
        }
    }
    bad += check("4096 random picks stay on unison / fourth / fifth / octave, inside -24..24", bad == 0);
    bad += check("a narrow range falls back to unison", param_semi_pick(1u, -4, 4) == 0);
    bad += check("snap: 19 stays 19 (GLASS KEYS), 6 ties 5/7 and keeps 5, -10 becomes -12, 30 becomes 19",
                 param_semi_snap(19) == 19 && param_semi_snap(6) == 5 &&
                 param_semi_snap(-10) == -12 && param_semi_snap(30) == 19);
    printf("semi: %d FAILED\n", bad);
    return bad ? 1 : 0;
}
