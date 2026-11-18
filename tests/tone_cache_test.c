/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5) The TONE coefficient cache (src/fx.c tc_dly / tc_rev): the delay's TONE and the reverb's FILT used
 * to rebuild their coefficients on every audio block (1 378 times a second, three divides and a tangent each).
 * They are now kept for the value they were built for. What has to hold:
 *   1. the coefficients a cache HIT uses are the ones a first build makes: two fresh renders of the same
 *      settings are the same bytes (the second one is served from the cache inside its own process after
 *      the first block)
 *   2. a knob turn reaches the DSP: different TONE / FILT values are different renders, and A -> B -> A
 *      inside one process comes back to A's sound, not B's (nothing is pinned to the first value)
 *   3. TONE at its middle (0) takes the delay's tone out of the chain and the render still sounds
 * (A render cannot be compared with the NEXT render in the same process: the delay line and the reverb hold
 *  the previous one's tail, and that state is the instrument's, not the cache's -- so a hit is pinned by a
 *  fresh child repeating the same settings.)
 * Every render runs in its own fork()ed child of a process that never touched the DSP (tests/regress.c's rule),
 * and the A -> B -> A sequence runs inside ONE child, in order, so it is the same instrument with a knob turn.
 * Usage: build/host/tone_cache_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <sys/wait.h>
#include <unistd.h>

static int bad;
static void check(const char *what, int ok, long a, long b)
{
    printf("tone: %-70s %s (%ld / %ld)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

#define NBLK 40

/* one note through the buses with the DELAY and REVERB sends up, `dcolor` and `rfilt` as the knobs read them */
static uint64_t render(int32_t dcolor, int32_t rfilt)
{
    track_t *t = &trk[0];
    int32_t out[2 * CTL], i, b;
    uint64_t h = 1469598103934665603ull;
    uint32_t k;

    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            V(&trk[k], i).active = 0;
    host_tracks_init();
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;
    memset(dist_w, 0, sizeof dist_w);
    fx_boot_gain(32768);
    host_preset(t, 0, 0);
    t->p[P_LEVEL] = 100;
    t->p[P_DIST] = 0;
    t->p[P_DLY] = 90;
    t->p[P_REV] = 70;
    t->p[P_CHOR] = 0;
    for (i = 0; i < 4; i++) {
        t->p[P_M1SRC + (int16_t)(3 * i)] = 0;
        t->p[P_M1DST + (int16_t)(3 * i)] = 0;
        t->p[P_M1AMT + (int16_t)(3 * i)] = 0;
    }
    song.g[G_DMIX] = 100;
    song.g[G_DFDBK] = 40;
    song.g[G_DCOLOR] = (int16_t)dcolor;
    song.g[G_RFILT] = (int16_t)rfilt;
    trk_note_on(t, 45, 100);
    for (b = 0; b < NBLK; b++) {
        mix_block(out, CTL);
        for (i = 0; i < 2 * CTL; i++)
            h = (h ^ (uint64_t)(uint32_t)out[i]) * 1099511628211ull;
    }
    return h;
}

/* one child: the hashes of a sequence of settings, in order, on a fresh process */
static void seq(const int32_t (*set)[2], int n, uint64_t *out)
{
    int fd[2], st, i;
    pid_t pid;
    if (pipe(fd) != 0)
        return;
    pid = fork();
    if (pid == 0) {
        for (i = 0; i < n; i++) {
            uint64_t h = render(set[i][0], set[i][1]);
            ssize_t w = write(fd[1], &h, sizeof h);
            if (w != (ssize_t)sizeof h)
                _exit(1);
        }
        _exit(0);
    }
    {
        size_t got = 0, want = sizeof(*out) * (size_t)n;
        while (got < want) {                             /* a pipe hands back what it has, not what was asked */
            ssize_t r = read(fd[0], (char *)out + got, want - got);
            if (r <= 0)
                break;
            got += (size_t)r;
        }
        if (got != want || waitpid(pid, &st, 0) < 0)
            for (i = 0; i < n; i++)
                out[i] = 0;
    }
    close(fd[0]);
    close(fd[1]);
}

/* one setting in a fresh child */
static uint64_t once(int32_t dcolor, int32_t rfilt)
{
    int32_t set[1][2];
    uint64_t h = 0;
    set[0][0] = dcolor;
    set[0][1] = rfilt;
    seq(set, 1, &h);
    return h;
}

int main(void)
{
    uint64_t a = once(30, 20), a2 = once(30, 20), b = once(-40, -50);
    check("two fresh renders of the same settings are the same bytes (a hit == a first build)",
          a != 0 && a == a2, (long)a, (long)a2);
    check("different TONE / FILT are different renders", a != b, (long)a, (long)b);

    {   /* A -> B -> A inside one process: the last one is A's sound again, not B's */
        static const int32_t aba[3][2] = {{30, 20}, {-40, -50}, {30, 20}};
        uint64_t h[3];
        seq(aba, 3, h);
        check("A -> B -> A comes back to A (nothing pinned to the first value)", h[2] != h[1] && h[2] != 0,
              (long)h[2], (long)h[1]);
    }
    check("TONE at its middle still renders (tone out of the chain)", once(64, 64) != 0, 1, 0);
    if (bad)
        printf("tone cache: %d FAILED\n", bad);
    return bad != 0;
}
