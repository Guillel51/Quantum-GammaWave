# MENU > EXTRA: the six rows, audited

The tab is `MTAB_GEN` in `firmware/src/menu_items.c` (`MTAB_NAME[4] = "EXTRA"`), six rows in MI order,
each with a storage byte, a get, an apply and a value string. Every row was followed from the menu
down to what it changes; the class of this audit is source reading plus the host tests named below,
not a measurement of a unit.

| Row | Storage | Range | Applies | Used by | Verified by |
|---|---|---|---|---|---|
| KNOB SELECT | `ui_sel_home` (`favorites.factory[15][22]`) | 0..4: BPM, TRK VOL, TRK PAN, MASTER VOL, OFF | at once | `ui_input.c` KNOB 1 on HOME | `ui_test` (MENU rows), `ui_render` |
| CPU MODE | `ui_cpu_mode` (`[15][21]`) | AUTO, DUAL CORE, SAFE SINGLE | at once (`fx.c cpu_mode_apply` starts/stops core 1 from the main loop) | `main.c` boot, `fx.c` | `ui_test`, `tests/multicore_*` |
| VIS QUALITY | `ui_viz_q` (`[15][20]`) | HIGH, BALANCED, LOW | at once (`ui.force = 1`) | `ui_graph.c`, `ui_being.c` (particle counts) | `ui_render` (three qualities over the visualizer) |
| SPLASH | `ui_splash` (`[15][19]`) | FULL, SHORT, OFF | next boot | `main.c` | `ui_render` (splash timeline) |
| VOICES | `ui_voices` (`[15][17]`) | stored 0..2 → 8, 10, 12 (the value is inverted on purpose so an older record reads 12) | at once (`voice_ucap`) | `voice.c` | `ui_test`, `tests/poly_*` |
| PANEL Y | `settings_yoff` (`[15][16]`) | 0 or 1: rows 0..239 / 80..319 | at once, whole screen repainted | `lcd.c lcd_yoff` → `lcd_window`, `lcd_fill` | `tests/lcd_st7789_test.c` (frame-memory row 0 vs 80) |

## PANEL Y, which is the row the report could not read

- **What it is**: the 240x240 panel is bonded to a slice of the controller's 240x320 frame memory.
  `lcd_yoff` adds the slice to every window address (`lcd.c:110`, `lcd.c:134`) and a whole-screen fill
  clears the rows the slice leaves out (`lcd.c:123`). With 0 the UI's row 0 is frame-memory row 0;
  with 80 it is row 80 and the panel shows rows 80..319.
- **Why it looks like it does nothing**: it changes nothing in the picture the firmware composes — the
  UI is the same 240x240 either way. On a unit whose panel is bonded at 0, switching to 80 moves every
  row 80 lines down in the controller's memory, which on the glass reads as a blank or torn image, not
  as a setting doing something useful. On a unit bonded at 80, 0 is the wrong one. It is a bring-up
  switch, not a preference.
- **The value now says which slice it is**: `0 TOP` / `80 LOW` instead of `0` / `80` (`menu_items.c`,
  `YOFF_N`), so the row explains itself next to the number.
- **Direction**: stepping (OCT+ / OCT-) walks 0 → 80 → 0. There is no 80-to-0 range and never was: the
  two states are the two slices, stored as one bit.
- **Recovery**: if 80 is wrong for the unit, the same key steps back to `0 TOP`; the setting is saved
  with the others (`settings_persist.c` `panel.magic == PANEL_MAGIC`), so it survives a power cycle —
  which is what a bring-up switch needs and also why it must be easy to step back.

## What was found and what was left alone

- Every one of the six rows is wired end to end (get → apply → persist) and none was dead, inverted or
  unreachable with the wrong range. `VOICES` looks inverted only because its stored byte is; the row
  shows 8/10/12 and applies the budget at once.
- A hint line under the rows was prototyped and **removed**: with six rows the panel ends at row 214
  and the key hints start at 224, nine rows of band where a small-font line needs eleven. The `RUNNING`
  line that used to sit there had become unreachable for the same reason when VOICES joined the tab,
  and its content (cores, CPU load) is on ABOUT. Nothing was drawn in a space that does not exist.
