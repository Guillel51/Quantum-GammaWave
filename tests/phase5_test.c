/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 5 (QUANTUM SYNTHESIS): the matrix reaches the envelope's times, and a source for the bar.
 *   build/host/phase5_test
 *
 * 1. the tables: BAR is source 16, the three envelope times are destinations 27..29, and every number
 *    of before them still names what it named (PAT 15, RES 26); the knobs' ranges follow.
 * 2. BAR is the bar's phase: with the transport stopped it reads 0 and the sound is the one without the
 *    slot, sample for sample; with the transport playing the same sound moves, and what it does in the
 *    first half of a bar is not what it does in the second.
 * 3. ATK, DEC and REL are the voice's own envelope times: with a slot on each the sound changes, and the
 *    attack measured from the audio is longer for a loud note than for a quiet one when VEL -> ATK is on
 *    and the two are the same when it is off.
 * 4. nothing leaks: after the blocks the stored values are back (the ISR's mod_end), and a slot naming
 *    the new source and destination survives a project round trip.
 *
 * No hardware: this is the host build of the same sources, and it says nothing about the FM-1's own CPU
 * or flash. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>
#define PROJ_HOST 1                             /* (project.c's host hooks) */
#include "../firmware/src/project.c"

static int fails;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("phase5: FAIL %s\n", what);
        fails++;
    }
}

/* a note through the whole mix (fx.c mix_block: the matrix's per-block path, BAR's clock) with one slot
 * set; returns the hash of the audio and, when wanted, the envelope's shape block by block */
typedef struct { uint64_t h; uint32_t atk; int32_t peak[256]; } res_t;

static void run(uint32_t e, uint32_t pi, int src, int dst, int amt, int note, int vel, int blocks, int play,
                int atk0, int rel_at, res_t *r)
{
    static int32_t buf[CTL];
    track_t *t = &trk[0];
    uint32_t i, k, first = 0;
    int32_t mx = 0;
    memset(r, 0, sizeof *r);
    r->h = 1469598103934665603ull;
    host_tracks_init();
    host_preset(t, e, pi);
    t->p[P_M1SRC] = (int16_t)src;
    t->p[P_M1DST] = (int16_t)dst;
    t->p[P_M1AMT] = (int16_t)amt;
    t->p[P_ATK] = (int16_t)atk0;
    t->p[P_SUS] = rel_at ? 30 : t->p[P_SUS];     /* a low sustain: the decay has somewhere to go */
    t->p[P_FCUT] = 45;                           /* a filter to arc */
    t->p[P_FTYPE] = 0;
    song.g[G_BPM] = 120;
    song.playing = (uint8_t)play;
    trk_note_on(t, (uint32_t)note, (uint32_t)vel);
    for (i = 0; i < (uint32_t)blocks; i++) {
        uint32_t pk = 0;
        memset(buf, 0, sizeof buf);
        if (rel_at && (int)i == rel_at)          /* the key up: REL gets to apply */
            trk_note_off(t, (uint32_t)note);
        mix_block(buf, CTL);
        for (k = 0; k < CTL; k++) {
            uint32_t a = (uint32_t)(buf[k] < 0 ? -buf[k] : buf[k]);
            r->h = (r->h ^ (uint64_t)(uint32_t)buf[k]) * 1099511628211ull;
            if (a > pk)
                pk = a;
        }
        if (i < 256u)
            r->peak[i] = (int32_t)pk;
        if ((int32_t)pk > mx)
            mx = (int32_t)pk;
    }
    for (i = 0; i < 256u && i < (uint32_t)blocks; i++)   /* the first block at half the run's own peak */
        if (!first && mx > 0 && r->peak[i] * 2 >= mx)
            first = i + 1u;
    r->atk = first;
}

static void tables(void)
{
    check("BAR is source 16", N_MSRC[MS_BAR] && !strcmp(N_MSRC[MS_BAR], "BAR") && MS_BAR == 16);
    check("the envelope's times are destinations 27, 28 and 29",
          !strcmp(N_MDST[MD_ATK], "ATK") && !strcmp(N_MDST[MD_DEC], "DEC") && !strcmp(N_MDST[MD_REL], "REL") &&
              MD_ATK == 27 && MD_DEC == 28 && MD_REL == 29);
    check("every number of before keeps its meaning",
          !strcmp(N_MSRC[15], "PAT") && !strcmp(N_MDST[26], "RES") && !strcmp(N_MDST[21], "PNCH"));
    /* (v1.12.13) the two lists the knobs are built from, not the numbers they had on the day: TILT and LFO
     * CURVE are destinations now, so the ceiling is the table's own count. */
    check("the knobs follow the tables", TP[P_M1SRC].max == (int16_t)(QG_SRC_N - 1) &&
          TP[P_M1DST].max == (int16_t)(QG_DST_N - 1));
}

/* each render in its own child: the DSP's static state -- ms_clock among it -- starts as at boot, so two
 * runs of the same numbers are bit-identical and a difference is the parameter's (as tests/phase2_test.c) */
static void runf(uint32_t e, uint32_t pi, int src, int dst, int amt, int note, int vel, int blocks, int play,
                 int atk0, int rel_at, res_t *r)
{
    int fd[2];
    pid_t pid;
    memset(r, 0, sizeof *r);
    if (pipe(fd))
        return;
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        run(e, pi, src, dst, amt, note, vel, blocks, play, atk0, rel_at, r);
        if (write(fd[1], r, sizeof *r) != (ssize_t)sizeof *r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (pid < 0 || read(fd[0], r, sizeof *r) != (ssize_t)sizeof *r)
        memset(r, 0, sizeof *r);
    close(fd[0]);
    if (pid > 0)
        waitpid(pid, 0, 0);
}

static void bar(const res_t *base, const res_t *off, const res_t *on, const res_t *half)
{
    check("stopped, BAR reads 0: the sound is the one without the slot", base->h == off->h);
    check("playing, BAR moves the sound", base->h != on->h);
    check("and the arc is inside the bar (its halves differ)", half->h != on->h);
}

static void env_times(void)
{
    res_t quiet, loud, quiet_off, loud_off;
    /* VEL -> ATK: a loud note's attack is longer than a quiet one's, and without the slot they match */
    runf(ENGI_ANALOG, 0, MS_VEL, MD_ATK, 50, 60, 30, 200, 0, 20, 0, &quiet);
    runf(ENGI_ANALOG, 0, MS_VEL, MD_ATK, 50, 60, 120, 200, 0, 20, 0, &loud);
    runf(ENGI_ANALOG, 0, MS_OFF, MD_ATK, 50, 60, 30, 200, 0, 20, 0, &quiet_off);
    runf(ENGI_ANALOG, 0, MS_OFF, MD_ATK, 50, 60, 120, 200, 0, 20, 0, &loud_off);
    check("VEL -> ATK: the loud note's attack is the longer one", loud.atk > quiet.atk && quiet.atk > 0);
    check("without the slot the two attacks are the same", quiet_off.atk == loud_off.atk);
    check("VEL -> ATK changes the sound", loud.h != loud_off.h);
    {   /* DEC and REL are destinations that move the sound too */
        res_t dec, rel, plain;
        runf(ENGI_ANALOG, 0, MS_OFF, MD_DEC, 40, 60, 100, 120, 0, 8, 60, &plain);
        runf(ENGI_ANALOG, 0, MS_VEL, MD_DEC, 40, 60, 100, 120, 0, 8, 60, &dec);
        runf(ENGI_ANALOG, 0, MS_VEL, MD_REL, 40, 60, 100, 120, 0, 8, 60, &rel);
        check("VEL -> DEC changes the sound", dec.h != plain.h);
        check("VEL -> REL changes the sound", rel.h != plain.h);
    }
}

static void no_leak(void)
{
    static int32_t buf[CTL];
    track_t *t = &trk[0];
    uint32_t i;
    int16_t atk, dec, rel;
    host_tracks_init();
    host_preset(t, ENGI_ANALOG, 0);
    t->p[P_ATK] = 96;
    t->p[P_DEC] = 70;
    t->p[P_REL] = 60;
    atk = t->p[P_ATK];
    dec = t->p[P_DEC];
    rel = t->p[P_REL];
    t->p[P_M1SRC] = MS_VEL;
    t->p[P_M1DST] = MD_ATK;
    t->p[P_M1AMT] = 63;
    t->p[P_M2SRC] = MS_VEL;
    t->p[P_M2DST] = MD_DEC;
    t->p[P_M2AMT] = 63;
    t->p[P_M3SRC] = MS_VEL;
    t->p[P_M3DST] = MD_REL;
    t->p[P_M3AMT] = 63;
    trk_note_on(t, 60, 120);
    for (i = 0; i < 40u; i++) {
        memset(buf, 0, sizeof buf);
        mix_block(buf, CTL);
    }
    check("the stored values are back after the blocks", t->p[P_ATK] == atk && t->p[P_DEC] == dec && t->p[P_REL] == rel);
}

static void store(void)
{
    static project_t f, g;
    static project_store_t fs;
    uint32_t k;
    host_tracks_init();
    memset(&f, 0, sizeof f);
    f.magic = PROJ_MAGIC;
    f.size = sizeof f;
    f.parts = NPART;
    f.phys = PROJ_PHYS;
    chain_defaults(&f.chain);
    proj_new_tracks(&f, 0u);
    for (k = 0; k < NTRK; k++)
        memcpy(f.fm6[k], FM6_INIT, FM6_PACKED);
    for (k = 0; k < G_COUNT; k++)
        f.g[k] = GP[k].def;
    memcpy(f.dx, DX_KIT_DEF, sizeof f.dx);
    f.t[0].engine = ENGI_ANALOG;
    f.t[0].p[P_M1SRC] = MS_BAR;                  /* the new source */
    f.t[0].p[P_M1DST] = MD_ATK;                  /* and the new destination */
    f.t[0].p[P_M1AMT] = 33;
    f.sum = proj_sum(&f);
    check("the project packs", proj_pack(&fs, &f));
    check("the project imports", proj_import(&g, &fs, (int)sizeof fs));
    check("a slot naming BAR -> ATK survives the round trip",
          g.t[0].p[P_M1SRC] == MS_BAR && g.t[0].p[P_M1DST] == MD_ATK && g.t[0].p[P_M1AMT] == 33);
}

int main(void)
{
    res_t base, off, on, half;
    tables();
    runf(ENGI_ANALOG, 0, MS_OFF, MD_CUT, 0, 60, 100, 60, 1, 30, 0, &base);
    runf(ENGI_ANALOG, 0, MS_BAR, MD_CUT, 50, 60, 100, 60, 0, 30, 0, &off);
    runf(ENGI_ANALOG, 0, MS_BAR, MD_CUT, 50, 60, 100, 60, 1, 30, 0, &on);
    runf(ENGI_ANALOG, 0, MS_BAR, MD_CUT, 50, 60, 100, 20, 1, 30, 0, &half);
    bar(&base, &off, &on, &half);
    env_times();
    no_leak();
    store();
    if (fails)
        return 1;
    printf("phase5: PASS (BAR: the bar's phase as a source; ATK DEC REL: the envelope's times as destinations)\n");
    return 0;
}
