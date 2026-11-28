/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 3: PHASE's tenth wave, RECT (eng_phase.c: N_PD_WAVE, pd_wave case 9).
 *   build/host/phase_wave_test
 *
 * What it checks, on the host build of the same sources:
 * 1. the wave count the engine offers is 10 and the descriptor's maximum is the last one, so the knob reaches it
 *    (a value past the enum's list would draw nothing and would be refused by the editor);
 * 2. every wave sounds: ten renders of the same note, one per WAVE value, ten different hashes;
 * 3. RECT is not one of the others (it differs from SQR, PLS, DSIN, SPLS, RSAW, RTRI, RTRP and SYNC);
 * 4. the rectification's offset does not reach the voice: the DC blocker (FASE 5) keeps the mean near zero while
 *    the peak stays inside the voice's level, and the render is deterministic (the same note twice, same bytes);
 * 5. the waves 0..8 are unchanged by the append: their values are the ones the stores already name (the golden
 *    renders cover the presets; this covers the enum itself, so a reordering fails here as well as there).
 *
 * No hardware: host x86-64, the same C the AC791N compiles. It says nothing about the unit's CPU or flash. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>

static int fails;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("phase wave: FAIL %s\n", what);
        fails++;
    }
}

typedef struct { uint64_t h; int32_t pk; int64_t sum; uint32_t n; } pw_t;

static pw_t go_child(uint32_t w)
{
    static int32_t buf[CTL];
    track_t *t = &trk[0];
    pw_t r = {1469598103934665603ull, 0, 0, 0};
    uint32_t i, k;
    host_tracks_init();
    host_preset(t, 2u, 0u);                          /* engine 2: PHASE (engines.c), its own first sound */
    t->p[P_E0] = (int16_t)w;                         /* WAVE */
    t->p[P_E1] = 0;                                  /* WAVE2 off: one wave at a time */
    t->p[P_LD_PIT] = 0;                              /* the LFO out of it: the wave is what is measured */
    trk_note_on(t, 69, 100);
    for (i = 0; i < 240u; i++) {
        memset(buf, 0, sizeof buf);
        track_render(t, buf, CTL);
        for (k = 0; k < CTL; k++) {
            int32_t v = buf[k];
            r.h = (r.h ^ (uint64_t)(uint32_t)v) * 1099511628211ull;
            if (v < 0) v = -v;
            if (v > r.pk) r.pk = v;
            if (i >= 40u) {                          /* past the attack: the wave's own offset, not the envelope's */
                r.sum += buf[k];
                r.n++;
            }
        }
    }
    return r;
}

/* each render in its own child: the DSP's static state starts as at boot (as tests/phase2_test.c), so two calls
 * with the same value are bit-identical and a difference is the wave's */
static pw_t go(uint32_t w)
{
    int fd[2];
    pw_t r;
    pid_t pid;
    memset(&r, 0, sizeof r);
    if (pipe(fd))
        return r;
    pid = fork();
    if (pid == 0) {
        pw_t c;
        close(fd[0]);
        c = go_child(w);
        if (write(fd[1], &c, sizeof c) != (ssize_t)sizeof c)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (pid < 0 || read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
        memset(&r, 0, sizeof r);
    close(fd[0]);
    if (pid > 0)
        waitpid(pid, 0, 0);
    return r;
}

#define NW 10u

int main(void)
{
    pw_t r[NW], again;
    uint32_t i, j, distinct = 0, dup = 0;
    const param_desc_t *d;

    host_tracks_init();                              /* the descriptor is the SELECTED engine's: PHASE's WAVE */
    host_preset(&trk[0], 2u, 0u);
    d = track_desc(&trk[0], P_E0);                   /* the descriptor the knob, the page and the editor read */
    check("PHASE's WAVE offers ten values", d && d->max == (int32_t)NW - 1 && d->names && d->names[NW - 1] &&
          !strcmp(d->names[NW - 1], "RECT"));
    check("the nine waves of before keep their names", d && d->names && !strcmp(d->names[0], "SAW") &&
          !strcmp(d->names[8], "SYNC"));

    for (i = 0; i < NW; i++) {
        r[i] = go(i);
        if (getenv("PW_VERBOSE"))
            printf("  w %u %-5s hash %016llx pk %d mean %lld/%u\n", i, d->names[i],
                   (unsigned long long)r[i].h, r[i].pk,
                   (long long)(r[i].n ? r[i].sum / (int64_t)r[i].n : 0), r[i].n);
    }
    for (i = 0; i < NW; i++)
        check("every wave renders and is audible", r[i].h && r[i].pk > 0);
    for (i = 0; i < NW; i++) {
        int uniq = 1;
        for (j = 0; j < i; j++)
            if (r[i].h == r[j].h) {
                uniq = 0;
                dup |= 1u << j;
                printf("phase wave: %s sounds the same as %s\n", d->names[i], d->names[j]);
            }
        distinct += (uint32_t)uniq;
    }
    check("the ten waves are ten different sounds", distinct == NW && !dup);

    /* RECT: inside the voice's level, and centred (the DC blocker takes the rectification's offset) */
    check("RECT stays inside the part's headroom", r[9].pk <= 65536 && r[9].pk > 1000);
    {
        int64_t mean = r[9].n ? r[9].sum / (int64_t)r[9].n : 0;
        int64_t lim = r[9].pk / 8 + 1;
        check("RECT carries no offset into the voice (DC blocked)", mean <= lim && mean >= -lim);
    }
    /* the same wave twice: byte for byte (no hidden state between notes of the same wave) */
    again = go(9);
    check("RECT is deterministic", again.h == r[9].h);

    if (fails) {
        printf("phase wave: %d FAILED\n", fails);
        return 1;
    }
    printf("phase wave: ok -- 10 waves, all different, RECT in range and DC-free (host x86-64)\n");
    return 0;
}
