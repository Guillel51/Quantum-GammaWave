/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared PER4 layout: each feature updates only its own fields, preserving
 * the other feature's saved preferences when either is built independently.
 * PER1/PER2 are upstream; PER3 added bold; PER4 added favorites.
 * palette: UI_PAL_TAG + index; an old id (below 20, earlier firmware) is migrated on import.
 * bold: no longer used (one font weight); kept as it was saved, unless it holds the HOLD setting (panel.c).
 * zoom: no longer used (the large readout); kept as it was saved, unless it holds the LEDS setting (panel.c).
 * favorites.factory[15][23]: JIANT_PAL_SEEN (JIANT 0.6.81). favorites.factory[15][28..31]: CLICK / CLICK LEVEL / COUNT-IN (1.1), STYLE, MENU's flags, the layers seen (bytes no
 * engine uses; 0 in older settings = every one's default); [15][27]: SCREEN OFF (1.1.5; 0 = 30 MIN, the default).
 * MENU's flags (ui.c PREF_*) are append-only bits whose 0 is the default: BPM LOCK (16) is clear in every older setting
 * = unlocked, LARGE (32) = OFF, RESTORE LAST OFF (128, 1.2) = ON, read as saved (nothing to migrate, so importing twice changes nothing). */
#include "tune.c"                    /* (FASE 7) the temperaments this record carries */

typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
    tune_t tune[TUNE_USER];             /* (FASE 7) the three user temperaments, 13 bytes a slot */
} persist_t;
#define PERSIST_MAGIC 0x50455235u
#define PERSIST_MAGIC_4 0x50455234u
#define JIANT_PAL_SEEN 0x51u                    /* favorites.factory[15][23]: the palette set to QUANTUM once (0x47: GAMMA, GW-1) */

/* Normalize in place; 1 = current, 2 = migrated, 0 = invalid. */
static int settings_import(persist_t *p, int n)
{
    int current = n == (int)sizeof *p && p->magic == PERSIST_MAGIC;
    int old4 = n == (int)(sizeof *p - sizeof p->tune) && p->magic == PERSIST_MAGIC_4;
    int old3 = n == (int)(sizeof *p - sizeof p->favorites - sizeof p->tune) && p->magic == 0x50455233u;
    int old2 = n == (int)(sizeof *p - sizeof p->favorites - sizeof p->bold - sizeof p->tune) && p->magic == 0x50455232u;
    int old1 = n == (int)(8u + sizeof(panel_t)) && p->magic == 0x50455231u;
    (void)old4;
    if (!(current || old4 || old3 || old2 || old1)) return 0;
    if (old1) {
        panel_t old;
        memcpy(&old, (uint8_t *)p + 8, sizeof old);
        p->panel = old;
        p->lowcut = p->zoom = 0;
    }
    if (old1 || old2) p->bold = 0;
    if (!current) memset(&p->favorites, 0, sizeof p->favorites);
    if (!current && !old4)            /* (FASE 7) a record without temperaments had no user ones: EQUAL */
        memset(&p->tune, 0, sizeof p->tune);
    if (p->favorites.factory[15][29] > 1u)        /* STYLE (ui.c ui_style): 2, the retired PIXEL = LINE; unknown = FLAT */
        p->favorites.factory[15][29] = p->favorites.factory[15][29] == 2u ? 1u : 0u;
    {   /* CLICK, CLICK LEVEL, COUNT-IN (ui.c ui_rec_prefs): two bits each, 3 is no value: the default (0) */
        uint32_t b = p->favorites.factory[15][28], f;
        for (f = 0; f < 3u; f++)
            if (((b >> (2u * f)) & 3u) == 3u)
                b &= ~(3u << (2u * f));
        p->favorites.factory[15][28] = (uint8_t)b;
    }
    if (p->favorites.factory[15][26] > 2u)         /* LOAD (JIANT 0.6.30, ui.c ui_load): unknown = FULL */
        p->favorites.factory[15][26] = 0;
    if (p->favorites.factory[15][22] > 4u)         /* KNOB SELECT MODE (QG-1, ui.c ui_sel_home): 0..4, default 0 (BPM) */
        p->favorites.factory[15][22] = 0;
    if (p->favorites.factory[15][24] > 3u)         /* MIDI OUT (JIANT 0.6.72, ui.c midi_out_mode): unknown = KEYS */
        p->favorites.factory[15][24] = 0;
    if (p->favorites.factory[15][27] > 7u)         /* SCREEN OFF (1.1.5, ui.c SCR_CODE): unknown = 0 (NEVER since 1.1.5.1) */
        p->favorites.factory[15][27] = 0;
    /* (QG-1 v1.5) MENU > EXTRA: 0 is the default in every older record (a byte no engine uses, engine 15), so the
     * three only need the unknown-value clamp the rest have: a corrupt or future value reads as the default. */
    if (p->favorites.factory[15][19] > 2u)         /* STARTUP SPLASH: unknown = FULL */
        p->favorites.factory[15][19] = 0;
    if (p->favorites.factory[15][20] > 2u)         /* VISUALIZER QUALITY: unknown = HIGH */
        p->favorites.factory[15][20] = 0;
    if (p->favorites.factory[15][21] > 2u)         /* CPU MODE: unknown = AUTO */
        p->favorites.factory[15][21] = 0;
    if (p->favorites.factory[15][16] > 1u)         /* (1.4.11) PANEL Y: unknown = rows 0..239 */
        p->favorites.factory[15][16] = 0;
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.lowcut = p->lowcut;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
    settings_yoff = p->favorites.factory[15][16] & 1u;   /* (1.4.11) PANEL Y */
#ifdef FELUCCA_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = favorites.filter == 1u;
#if defined(FM4_NPRESETS) && !FELUCCA_FM4
    {   /* DIGITAL's starred presets (engine 1, retired) -> the FM6 presets that cover them (fm4_convert.c) */
        uint32_t k;
        for (k = 0; k < FM4_NPRESETS; k++)
            if ((favorites.factory[ENGI_DIGITAL][0] >> k) & 1u)
                favorites.factory[ENGI_FM6][FM4_TO_FM6[k] / 8u] |= (uint8_t)(1u << (FM4_TO_FM6[k] % 8u));
        memset(favorites.factory[ENGI_DIGITAL], 0, sizeof favorites.factory[ENGI_DIGITAL]);
    }
#endif
#ifdef SMP_SET_PERC
    if ((favorites.factory[ENGI_SAMPLE][0] >> SMP_SET_PERC) & 1u) {   /* SAMPLE's PERC (preset 4, retired): DRUM's */
        favorites.factory[ENGI_DRUM][0] |= 1u;                         /* kit (core.h drum_from_perc) */
        favorites.factory[ENGI_SAMPLE][0] &= (uint8_t)~(1u << SMP_SET_PERC);
    }
#endif
#endif
    memcpy(tune_slot, &p->tune, sizeof tune_slot);   /* (FASE 7) the user temperaments, before anything reads them */
    {   /* a corrupt cents byte would be a class a semitone off: clamped, and the live table goes back to EQUAL
         * (the factories are TUNE_TEMP's rodata, so there is nothing of theirs to repair) */
        uint32_t k, n;
        for (k = 0; k < TUNE_USER; k++) {
            for (n = 0; n < 12u; n++)
                tune_slot[k].ct[n] = (int8_t)tune_clamp(tune_slot[k].ct[n], -TUNE_CMAX, TUNE_CMAX);
            tune_slot[k].all = (int8_t)tune_clamp(tune_slot[k].all, -TUNE_CMAX, TUNE_CMAX);
        }
        tune_pick(0);
        tune_ui_slot = tune_ui_note = tune_ui_ct = tune_ui_all = 0;
        tune_ui_slot_l = tune_ui_note_l = -1;
    }
    memcpy(midi_learn_map, &p->favorites.factory[15][0], sizeof midi_learn_map);   /* (1.9.2) the MIDI Learn map */
    if (p->panel.magic == PANEL_MAGIC) panel = p->panel;
    return current ? 1 : 2;
}

static void settings_jiant_once(persist_t *p)
{
    if (p->favorites.factory[15][23] == JIANT_PAL_SEEN)
        return;
    p->favorites.factory[15][23] = JIANT_PAL_SEEN;
    p->palette = palette_to_stored(UI_QUANTUM_INDEX);
    settings.palette = UI_QUANTUM_INDEX;
#ifdef FELUCCA_FAVORITES
    favorites.factory[15][23] = JIANT_PAL_SEEN;
#endif
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
    memcpy(p->tune, tune_slot, sizeof p->tune);   /* (FASE 7) the temperaments */
    memcpy(&p->favorites.factory[15][0], midi_learn_map, sizeof midi_learn_map);   /* (1.9.2) the MIDI Learn map */
    p->favorites.factory[15][16] = settings_yoff;   /* (1.4.11) PANEL Y, after the copy of the live favorites: that
                                                     * copy is what would otherwise put the byte back */
}
