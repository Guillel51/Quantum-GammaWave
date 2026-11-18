/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 2) DIST's five models (src/fx.c dist_shape, DT_SOFT .. DT_FOLD): every one of them has to be a
 * usable insert at a high DRIVE, not just the four that were there before FOLD arrived.
 *   1. each model sounds (a peak well over the noise floor) and stays inside 16 bits
 *   2. FOLD is loudness-neutral: its peak is the same as SOFT's, not a whisper and not a scream
 *      (the fold scales by T so DRIVE changes partials, not level)
 *   3. FOLD has no DC (it folds |v| and restores the sign: odd symmetry)
 *   4. the five models are five different sounds (their sample streams differ)
 *   5. DRIVE 0 is out of the chain for all five: byte-identical to each other, so a stored sound that
 *      switches TYPE with DIST at 0 does not change
 * Every render runs in its own fork()ed child of a process that never touched the DSP, the way
 * tests/regress.c does it: the noise / RAND / unison-phase seeds start as at boot, so two renders of the
 * same settings are the same bytes.
 * Usage: build/host/dist_models_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <sys/wait.h>
#include <unistd.h>

#define NMODEL 5

static int bad;
static void check(const char *what, int ok, long a, long b)
{
    printf("dst: %-72s %s (%ld / %ld)\n", what, ok ? "ok" : "FAIL", a, b);
    bad += !ok;
}

typedef struct { int32_t peak; int32_t dc; uint64_t hash; } res_t;

/* one note through the insert, DRIVE 90 (the blend is fully in at 8), the sends off so only DIST is heard */
static res_t render(int type, int drive)
{
    track_t *t = &trk[0];
    int32_t out[2 * CTL], pk = 0, i, b;
    int64_t sum = 0, n = 0;
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
    t->p[P_DTYPE] = (int16_t)type;
    t->p[P_DIST] = (int16_t)drive;
    t->p[P_DTONE] = 0;
    for (i = 1; i < 4; i++)                              /* the three sends */
        t->p[P_DIST + i] = 0;
    for (i = 0; i < 4; i++) {                            /* the matrix off */
        t->p[P_M1SRC + (int16_t)(3 * i)] = 0;
        t->p[P_M1DST + (int16_t)(3 * i)] = 0;
        t->p[P_M1AMT + (int16_t)(3 * i)] = 0;
    }
    trk_note_on(t, 45, 100);
    for (b = 0; b < 40; b++) {
        mix_block(out, CTL);
        for (i = 0; i < 2 * CTL; i++) {
            int32_t v = out[i], a = v < 0 ? -v : v;
            if (a > pk) pk = a;
            sum += v;
            n++;
            h = (h ^ (uint64_t)(uint32_t)v) * 1099511628211ull;
        }
    }
    res_t r;
    r.peak = pk;
    r.dc = (int32_t)(sum / n);
    r.hash = h;
    return r;
}

/* a render in its own child of a spotless process: deterministic, as tests/regress.c does it */
static res_t render_at(int type, int drive)
{
    int fd[2], st;
    pid_t pid;
    res_t r;
    if (pipe(fd) != 0) { r.peak = r.dc = 0; r.hash = 0; return r; }
    pid = fork();
    if (pid == 0) {
        res_t z = render(type, drive);
        ssize_t w = write(fd[1], &z, sizeof z);
        _exit(w == (ssize_t)sizeof z ? 0 : 1);
    }
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r || waitpid(pid, &st, 0) < 0)
        r.peak = r.dc = 0, r.hash = 0;
    close(fd[0]);
    close(fd[1]);
    return r;
}

int main(void)
{
    res_t m[NMODEL], dry = render_at(0, 0);
    int i, j, distinct = 1;

    for (i = 0; i < NMODEL; i++)
        m[i] = render_at(i, 90);
    check("SOFT, TAPE, HEARTH, FLUX and FOLD all sound at DRIVE 90", m[0].peak > 1500 && m[1].peak > 1500 &&
          m[2].peak > 1500 && m[3].peak > 1500 && m[4].peak > 1500, m[4].peak, 1500);
    {
        int in16 = 1;
        for (i = 0; i < NMODEL; i++)
            in16 &= m[i].peak < 32768;
        check("every model stays inside 16 bits", in16, m[0].peak, 32767);
    }
    check("FOLD is loudness-neutral against SOFT (+-35 %)",
          m[4].peak * 100 >= m[0].peak * 65 && m[4].peak * 100 <= m[0].peak * 135, m[4].peak, m[0].peak);
    check("FOLD has no DC (odd symmetry)", m[4].dc > -400 && m[4].dc < 400, m[4].dc, 400);
    for (i = 0; i < NMODEL; i++)
        for (j = i + 1; j < NMODEL; j++)
            distinct &= m[i].hash != m[j].hash;
    check("the five models are five different renders", distinct, NMODEL, NMODEL);
    {
        int same = 1;
        res_t z0 = render_at(0, 0);
        for (i = 0; i < NMODEL; i++) {
            res_t z = render_at(i, 0);
            same &= z.hash == z0.hash;                   /* TYPE is out of the chain at DRIVE 0 */
        }
        check("DRIVE 0 is out of the chain: TYPE changes nothing", same, z0.peak, dry.peak);
    }
    if (bad)
        printf("dist models: %d FAILED\n", bad);
    return bad != 0;
}
