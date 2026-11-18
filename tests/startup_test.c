/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Native emulator boot: includes main.c's real felucca_init, not just the storage loader. */
/* (JIANT 0.6.84) by @workeffortwaste (pull request #134, the ARRANGE power-off bug): it fails without main.c's
 * 0.6.81 fix (felucca_init clearing the arrangement persist_boot had read) and passes with it. */
#include "../web/emu/felucca_web.c"

static int startup_case(int saved, int restore)
{
    song_index_t want, got;
    persist_t prefs = {0};
    uint32_t i;
    int ok;
    web_nor_erase();
    memset(&want, 0, sizeof want);
    want.magic = SONG_MAGIC;
    want.version = 2;
    want.cur = saved ? 2u : 0u;
    for (i = 0; i < NSONG; i++) chain_defaults(&want.rows[i]);
    if (saved) {
        want.rows[2].count = 2;
        want.rows[2].row[0] = (chain_row_t){0, 3};
        want.rows[2].row[1] = (chain_row_t){1, 7};
        SONG_END(&want.rows[2]) = 3;
        want.scene[2][0] = (scene_t){1, 2, 4, {11, 22, 33, 44}};
        want.rows[0].count = 1;
        want.rows[0].row[0] = (chain_row_t){3, 9};
        if (st_save(OBJ_SONGIDX, &want, sizeof want)) return 1;
    }
    prefs.magic = PERSIST_MAGIC;
    prefs.panel = PANEL_DEFAULT;
    prefs.favorites.factory[15][30] = restore ? PREF_RESTORE_ON : 0;
    if (st_save(OBJ_SETTINGS, &prefs, sizeof prefs)) return 1;

    web_boot();
    ok = song_cur == want.cur && !memcmp(&chain_config, &want.rows[want.cur], sizeof chain_config);
    ok &= !!(ui_prefs & PREF_RESTORE_ON) == restore;
    ok &= !memcmp(song_idx.scene, want.scene, sizeof want.scene);
    /* Advance beyond the splash and the arrangement's two-second automatic save. */
    for (i = 0; i < 2000u; i++) web_render(128);
    ok &= !memcmp(&chain_config, &want.rows[want.cur], sizeof chain_config);
    if (saved)
        ok &= st_load(OBJ_SONGIDX, &got, sizeof got) == (int)sizeof got && !memcmp(&got, &want, sizeof got);
    else
        ok &= st_load(OBJ_SONGIDX, &got, sizeof got) < 0;
    printf("startup: %s, RESTORE LAST %s: rows, bars, END, scenes and flash %s\n",
           saved ? "saved project 3" : "empty flash", restore ? "ON" : "OFF", ok ? "ok" : "FAIL");
    return !ok;
}

/* (1.4.2.1) MENU > SPLASH: FULL, SHORT and OFF boot, each one its own way. FULL writes the art on over the boot
 * (many frames), SHORT draws it whole (one), OFF draws nothing before the UI; and the value the unit booted with is
 * the one the row shows. */
static int splash_case(int mode)
{
    persist_t prefs = {0};
    uint32_t d0, d1, i;
    web_nor_erase();
    prefs.magic = PERSIST_MAGIC;
    prefs.panel = PANEL_DEFAULT;
    prefs.favorites.factory[15][19] = (uint8_t)mode;   /* ui.c ui_splash */
    if (st_save(OBJ_SETTINGS, &prefs, sizeof prefs)) return 1;
    web_boot();
    d0 = web_screen_draws();
    for (i = 0; i < 200u; i++) web_render(128);
    d1 = web_screen_draws() - d0;
    printf("startup: SPLASH %s: %u draw(s) for the splash, %u after it, row reads %u\n",
           mode == 0 ? "FULL " : mode == 1 ? "SHORT" : "OFF  ", d0, d1, (unsigned)ui_splash);
    if (ui_splash != (uint8_t)mode) return 1;          /* the row shows what the unit booted with */
    if (mode == 2) {                                    /* OFF: not one frame of splash */
        if (d0 != 0u) return 1;
    } else if (d0 == 0u) {                              /* FULL and SHORT draw it before the UI */
        return 1;
    }
    /* (the difference between FULL's writing animation and SHORT's single frame is a matter of the clock, which
     * this emulator does not advance inside the boot: tests/ui_render.c measures that timeline, 244 of 1125 steps) */
    return d1 == 0u;                                    /* and the UI comes up in all three */
}

int main(void)
{
    int mode, status, bad = 0;
    for (mode = 0; mode < 3; mode++) {                 /* (1.4.2.1) FULL / SHORT / OFF */
        pid_t child = fork();
        if (child < 0) return 2;
        if (!child) exit(splash_case(mode));
        if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) bad++;
    }
    /* Each child starts with fresh static RAM, as a new device instance does. */
    for (mode = 0; mode < 3; mode++) {
        pid_t child = fork();
        if (child < 0) return 2;
        if (!child) exit(startup_case(mode != 0, mode == 2));
        if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) bad++;
    }
    puts(bad ? "startup test FAILED" : "startup test passed");
    return !!bad;
}
