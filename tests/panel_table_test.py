#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The ST7789's init table, checked at the source (firmware/src/lcd.c).

Why this exists: 1.4 wrote a gamma curve (0xE0/0xE1) and turned the brightness control on (0x53/0x51) from a
comparison against the emulator, never from a reading on a panel. On the owner's FM-1 those values crushed the low
half of the range -- plates, dividers, dim labels: black -- in every screen, the splash included, and 1.4.2.1 undid
them. A comment is not a guard: this test fails the build if any of those commands comes back, if the table stops
parsing cleanly, or if the three commands the panel needs to show anything at all disappear.

    python3 tests/panel_table_test.py [firmware/src/lcd.c]

It reads the array; it does not run the firmware. Class: static check of the source, not a measurement of a panel.
"""
import re
import sys

FORBIDDEN = {
    0xE0: "positive gamma (the 1.4 curve that crushed the low end)",
    0xE1: "negative gamma (the 1.4 curve)",
    0x53: "brightness control (WRCTRLD): not honoured by this unit",
    0x51: "brightness value (WRDISBV)",
    0x55: "content adaptive brightness (CABC)",
    0xBB: "VCOM: someone's panel-ager, nobody's default",
}
REQUIRED = {0x3A: "COLMOD (16-bit colour)", 0x36: "MADCTL (scan direction)", 0x21: "INVON (IPS inversion)"}


def sequence(src):
    """the entries of LCD_SEQ: [(cmd, [data...]), ...], waiting entries as (0x00, [ms])"""
    m = re.search(r"static\s+const\s+uint8_t\s+LCD_SEQ\s*\[\s*\]\s*=\s*\{", src)
    if not m:
        raise SystemExit("panel: LCD_SEQ is gone from lcd.c (renamed? moved?)")
    i = src.index("{", m.end() - 1)
    depth, j, block = 0, i, []
    while j < len(src):
        c = src[j]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                break
        block.append(c)
        j += 1
    body = "".join(block)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)      # the table is documented inline
    body = re.sub(r"//[^\n]*", "", body)
    body = body.replace("{", "").replace("}", "")
    vals = []
    for tok in body.split(","):
        tok = tok.strip()
        if not tok:
            continue
        try:
            vals.append(int(tok, 0))
        except ValueError:
            raise SystemExit(f"panel: LCD_SEQ holds something that is not a byte: {tok!r}")
    entries, k = [], 0
    while k < len(vals):
        if k + 1 >= len(vals):
            raise SystemExit(f"panel: LCD_SEQ ends with a command and no length (byte {k}: {vals[k]:#04x})")
        cmd, n = vals[k], vals[k + 1]
        if k + 2 + n > len(vals):
            raise SystemExit(f"panel: LCD_SEQ command {cmd:#04x} asks for {n} bytes, {len(vals) - k - 2} left")
        entries.append((cmd, vals[k + 2:k + 2 + n]))
        k += 2 + n
    return entries


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "firmware/src/lcd.c"
    entries = sequence(open(path, encoding="utf-8").read())
    cmds = [c for c, _ in entries]
    bad, note = [], []
    for cmd, data in entries:
        if cmd == 0x00:                                   # the wait pseudo-command
            continue
        if cmd in FORBIDDEN:
            bad.append(f"LCD_SEQ writes {cmd:#04x}: {FORBIDDEN[cmd]}")
    for cmd, what in REQUIRED.items():
        if cmd not in cmds:
            bad.append(f"LCD_SEQ does not send {cmd:#04x}: {what} (the panel would show nothing usable)")
    colmod = [d for c, d in entries if c == 0x3A]
    if colmod and colmod[0] and colmod[0][0] != 0x55:
        bad.append(f"COLMOD is {colmod[0][0]:#04x}, not 0x55 (RGB565): the UI writes 16-bit colour")
    note.append(f"{len(entries)} entries, {len([c for c in cmds if c])} commands: "
                + " ".join(f"{c:02X}" for c in cmds if c))
    for n in note:
        print("panel: " + n)
    for b in bad:
        print("panel: FAIL " + b)
    if bad:
        print(f"panel test FAILED ({len(bad)} problem(s))")
        return 1
    print("panel: the init table writes no gamma, no brightness and no VCOM, and still sets RGB565 + inversion; ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
