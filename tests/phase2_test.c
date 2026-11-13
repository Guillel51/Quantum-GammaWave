/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FASE 2 (QUANTUM SYNTHESIS): the engine CHARACTER bank, LFO SHAPE and ENV VEL on the host.
 *   build/host/phase2_test
 *
 * 1. every engine with a CHARACTER bank (ch_n > 0): with everything at its neutral value, a render; then each slot at
 *    the other end (0 for an inverted amount, 127 for a direct one): the samples must differ. DRUM-X has no bank.
 * 2. LFO SHAPE: SLEW (P_X2) and INVERT (P_X3) with the LFO routed to PITCH: each moves the sound, neutral is a
 *    no-op (bit-identical to its own default).
 * 3. ENV VEL (P_DXIMG on a non-DRUM engine): 127 = the velocity curve as before, 0 = full velocity: a quiet note
 *    gets louder.
 * 4. the descriptors: CHARACTER's page exists for the engines that have slots and one page_desc call per slot returns
 *    the engine's label and the right value; LFO SHAPE returns SLEW / INVERT; both are neutral at their stored 127.
 * 5. a project round trip: the four values and the third CHARACTER slot (MORPH) survive proj_pack -> proj_import.
 *
 * No hardware: this is the host build of the same sources, and it says nothing about the FM-1's own CPU or flash. */
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
        printf("phase2: FAIL %s\n", what);
        fails++;
    }
}

static uint64_t go_child(uint32_t e, uint32_t pi, int x0, int x1, int xm, int x2, int x3, int dx, int vel)
{
    static int32_t buf[CTL];
    track_t *t = &trk[0];
    uint64_t h = 1469598103934665603ull;
    uint32_t i, k;
    host_tracks_init();
    host_preset(t, e, pi);
    t->p[P_X0] = (int16_t)x0;
    t->p[P_X1] = (int16_t)x1;
    t->p[P_MORPH] = (int16_t)xm;
    t->p[P_X2] = (int16_t)x2;
    t->p[P_X3] = (int16_t)x3;
    t->p[P_DXIMG] = (int16_t)dx;
    t->p[P_LD_PIT] = 100;                            /* the LFO on the pitch, so its shape is audible */
    t->p[P_LRATE] = 110;
    t->p[P_LWAVE] = 4;                               /* S&H: SLEW has a step to ease */
    trk_note_on(t, 76, (uint32_t)vel);
    for (i = 0; i < 200u; i++) {
        memset(buf, 0, sizeof buf);
        track_render(t, buf, CTL);
        for (k = 0; k < CTL; k++)
            h = (h ^ (uint64_t)(uint32_t)buf[k]) * 1099511628211ull;
    }
    return h;
}

/* each render in its own child: the DSP's static state starts as at boot (as regress.c), so two calls with the same
 * values are bit-identical and a difference is the parameter's */
static uint64_t go(uint32_t e, uint32_t pi, int x0, int x1, int xm, int x2, int x3, int dx, int vel)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        h = go_child(e, pi, x0, x1, xm, x2, x3, dx, vel);
        if (write(fd[1], &h, sizeof h) != (ssize_t)sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (pid < 0 || read(fd[0], &h, sizeof h) != (ssize_t)sizeof h)
        h = 0;
    close(fd[0]);
    if (pid > 0)
        waitpid(pid, 0, 0);
    return h;
}

/* a preset that gives each bank something to work on: LOFI's PULSE LD (not BYTE / FLOAT), WHEEL's JAZZ PERC
 * (percussion, where PERC DCY applies) */
static uint32_t preset_for(uint32_t e) { return e == ENGI_LOFI ? 3u : e == 7u ? 1u : 0u; }
static uint64_t neutral(uint32_t e) { return go(e, preset_for(e), 127, 127, 0, 127, 127, 127, 90); }

static void audit(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        char nm[96];
        if (!eng_ok(e))
            continue;
        uint64_t base = neutral(e), v;
        int s;
        if (!en->ch_n) {
            if (e == ENGI_DRUM)
                check("DRUM-X has no CHARACTER bank", !en->ch);
            continue;
        }
        for (s = 0; s < (int)en->ch_n; s++) {
            uint32_t id = s < 2 ? P_XID[s] : P_MORPH;
            int nv = s < 2 ? 0 : 127;               /* inverted amount / direct amount */
            v = go(e, preset_for(e), s == 0 ? nv : 127, s == 1 ? nv : 127, s == 2 ? nv : 0, 127, 127, 127, 90);
            snprintf(nm, sizeof nm, "%s %s moves the sound", en->name, track_desc(&trk[0], id)->label);
            check(nm, v != base);
        }
    }
}

static void lfo_shape(void)
{
    uint64_t base = go(ENGI_WAVETABLE, 0, 127, 127, 0, 127, 127, 127, 90);
    check("LFO SLEW moves the sound", go(ENGI_WAVETABLE, 0, 127, 127, 0, 0, 127, 127, 90) != base);
    check("LFO INVERT moves the sound", go(ENGI_WAVETABLE, 0, 127, 127, 0, 127, 0, 127, 90) != base);
    check("LFO SLEW + INVERT move the sound", go(ENGI_WAVETABLE, 0, 127, 127, 0, 20, 40, 127, 90) != base);
    check("LFO SHAPE neutral is the default", go(ENGI_WAVETABLE, 0, 127, 127, 0, 127, 127, 127, 90) == base);
}

static void env_vel(void)
{
    uint64_t full = go(ENGI_ANALOG, 0, 127, 127, 0, 127, 127, 127, 40);
    uint64_t none = go(ENGI_ANALOG, 0, 127, 127, 0, 127, 127, 0, 40);
    check("ENV VEL 0 gives a quiet note more level", full != none);
    check("ENV VEL 127 is the velocity curve", go(ENGI_ANALOG, 0, 127, 127, 0, 127, 127, 127, 40) == full);
}

static void pages(void)
{
    uint32_t i, e;
    int seen_ch = 0, seen_lfo = 0, seen_vel = 0;
    for (i = 0; i < NELEM(PAGES); i++) {
        if (PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] == P_X0)
            seen_ch++;
        if (PAGES[i].fam == FAM_LFO && PAGES[i].id[0] == P_X2)
            seen_lfo++;
        if (PAGES[i].id[0] == P_MSTART && PAGES[i].id[2] == P_DXIMG)
            seen_vel++;
    }
    check("one CHARACTER page", seen_ch == 1);
    check("one LFO SHAPE page", seen_lfo == 1);
    check("VOICE 2 carries ENV VEL", seen_vel == 1);
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        int s;
        track_t *t = &trk[0];
        if (!eng_ok(e))
            continue;
        host_tracks_init();
        host_preset(t, e, 0);
        for (s = 0; s < 3; s++) {
            int16_t *vp = 0;
            const param_desc_t *d = 0;
            for (i = 0; i < NELEM(PAGES); i++)
                if (PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] == P_X0) {
                    d = page_desc(&PAGES[i], (uint32_t)s, &vp);
                    break;
                }
            if (s < (int)en->ch_n) {
                check("CHARACTER slot has a descriptor", d && d->label[0]);
                check("CHARACTER slot points at its value", vp == &t->p[s < 2 ? P_XID[s] : P_MORPH]);
                check("CHARACTER slot is neutral (127 inverted, 0 direct)", d && d->def == (s < 2 ? 127 : 0));
            } else {
                check("CHARACTER slot 3 absent on a two-slot engine", !d && !vp);
            }
        }
    }
    for (i = 0; i < NELEM(PAGES); i++)
        if (PAGES[i].fam == FAM_LFO && PAGES[i].id[0] == P_X2) {
            int s;
            track_t *t = &trk[0];
            for (s = 0; s < 2; s++) {
                int16_t *vp = 0;
                const param_desc_t *d = page_desc(&PAGES[i], (uint32_t)s, &vp);
                check("LFO SHAPE slot", d && (s ? !strcmp(d->label, "INVERT") : !strcmp(d->label, "SLEW")) &&
                                             vp == &t->p[s ? P_X3 : P_X2] && d->def == 127);
            }
            break;
        }
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
    f.t[0].engine = ENGI_WAVETABLE;
    f.t[0].p[P_X0] = 7;
    f.t[0].p[P_X1] = 13;
    f.t[0].p[P_MORPH] = 99;
    f.t[0].p[P_X2] = 21;
    f.t[0].p[P_X3] = 34;
    f.sum = proj_sum(&f);
    check("the project packs", proj_pack(&fs, &f));
    check("the project imports", proj_import(&g, &fs, (int)sizeof fs));
    check("CHARACTER 1..4 and MORPH survive the round trip",
          g.t[0].p[P_X0] == 7 && g.t[0].p[P_X1] == 13 && g.t[0].p[P_MORPH] == 99 && g.t[0].p[P_X2] == 21 &&
              g.t[0].p[P_X3] == 34);
}

int main(void)
{
    check("P_COUNT is 125", P_COUNT == 125);
    audit();
    lfo_shape();
    env_vel();
    pages();
    store();
    if (fails)
        return 1;
    printf("phase2: PASS (CHARACTER per engine, LFO SHAPE, ENV VEL, pages, project round trip)\n");
    return 0;
}
