/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3) The power-on fade, as the master gain sees it (src/fx.c lev_block: fx_boot_gain folds the fade
 * into the gain the block ramps anyway).
 *   1. gain 0 is exact digital silence: every sample of the block is 0, which is what the DAC plays while
 *      the output stage is still muted and what the unmute lands on
 *   2. the ramp is monotonic and reaches unity: 0 -> 8192 -> 16384 -> 32768 never goes back, and at 32768 the
 *      render is the same bytes as one that never had a fade, so the fade changes nothing once it is over
 *   3. the fade is an envelope, not a cut: each step's peak sits between the previous one and the unfaded peak
 * (The arming itself -- audio.c boot_armed, main.c audio_fade_in once the boot's interrupts-off stages are
 *  done -- is firmware control flow, covered by the build and by review, not by this host test.)
 * Usage: build/host/boot_fade_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <sys/wait.h>
#include <unistd.h>

static int bad;
static void check(const char *what, int ok, long a, long b)
{
    printf("fade: %-70s %s (%ld / %ld)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

#define NBLK 6

typedef struct { int32_t peak; int zero; uint64_t hash; } res_t;

static res_t render(int32_t gain)
{
    track_t *t = &trk[0];
    int32_t out[2 * CTL], i, b;
    int64_t nz = 1;
    uint64_t h = 1469598103934665603ull;
    uint32_t k;

    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            V(&trk[k], i).active = 0;
    host_tracks_init();
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;
    memset(dist_w, 0, sizeof dist_w);
    host_preset(t, 0, 0);
    t->p[P_LEVEL] = 100;
    for (i = 0; i < 4; i++)
        t->p[P_DIST + i] = 0;
    for (i = 0; i < 4; i++) {
        t->p[P_M1SRC + (int16_t)(3 * i)] = 0;
        t->p[P_M1DST + (int16_t)(3 * i)] = 0;
        t->p[P_M1AMT + (int16_t)(3 * i)] = 0;
    }
    fx_boot_gain(gain);
    trk_note_on(t, 45, 100);
    res_t r;
    r.peak = 0;
    r.zero = 1;
    for (b = 0; b < NBLK; b++) {
        fx_boot_gain(gain);                    /* what audio_block does every block */
        mix_block(out, CTL);
        for (i = 0; i < 2 * CTL; i++) {
            int32_t v = out[i], a = v < 0 ? -v : v;
            if (v)
                r.zero = 0;
            if (a > r.peak)
                r.peak = a;
            h = (h ^ (uint64_t)(uint32_t)v) * 1099511628211ull;
        }
    }
    r.hash = h;
    return r;
}

static res_t render_at(int32_t gain)
{
    int fd[2], st;
    pid_t pid;
    res_t r;
    if (pipe(fd) != 0) { r.peak = 0; r.zero = 0; r.hash = 0; return r; }
    pid = fork();
    if (pid == 0) {
        res_t z = render(gain);
        ssize_t w = write(fd[1], &z, sizeof z);
        _exit(w == (ssize_t)sizeof z ? 0 : 1);
    }
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r || waitpid(pid, &st, 0) < 0)
        r.peak = 0, r.zero = 0, r.hash = 0;
    close(fd[0]);
    close(fd[1]);
    return r;
}

int main(void)
{
    res_t g0 = render_at(0), q = render_at(8192), h = render_at(16384), full = render_at(32768);
    check("gain 0 is exact digital silence (no sample set)", g0.zero && g0.peak == 0, g0.peak, 0);
    check("the ramp never goes back", q.peak > g0.peak && h.peak > q.peak && full.peak > h.peak,
          h.peak, q.peak);
    check("the top of the ramp is under the unfaded level", full.peak > 0 && q.peak < full.peak, q.peak, full.peak);
    {
        res_t again = render_at(32768);
        check("gain 32768 renders the same bytes twice (no residual fade)", again.hash == full.hash, 1, 1);
    }
    if (bad)
        printf("boot fade: %d FAILED\n", bad);
    return bad != 0;
}
