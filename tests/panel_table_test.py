#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The ST7789's init table, checked at the source (firmware/src/lcd.c).

The rule changed in 1.4.10, and this file is where the change is written down.

1.4 wrote a gamma curve (0xE0/0xE1) and turned the brightness control on (0x53/0x51) from a comparison against
the emulator, never from a reading on a panel. On the owner's FM-1 those values crushed the low half of the range
-- plates, dividers, dim labels: black -- in every screen, the splash included. 1.4.2.1 answered by removing
everything after MADCTL, and that was too much: it left gate voltage, VCOM, power and gamma on the panel's
power-on defaults, a state nobody chose and nobody measured, and the unit stayed almost black. Removing a wrong
value is not the same as writing the right one.

So the table must now carry the full published 240x240 sequence -- porch (0xB2), gate (0xB7), VCOM (0xBB), power
(0xC0/0xC2/0xC3/0xC4/0xC6/0xD0) and both gamma tables (0xE0/0xE1) -- byte for byte as Adafruit_ST7789's
init_240x240 ships it, which is the same block TFT_eSPI and Pimoroni carry for 240x240. Those numbers are the
module's driver's, not ours: inventing them against the emulator is exactly what 1.4 did wrong.

What stays out, and this test still fails the build if it comes back: brightness (0x51/0x53; the FM-1's backlight
is a GPIO and the register changed nothing) and CABC (0xC7; adaptive dimming on an instrument's screen is a bug).

    python3 tests/panel_table_test.py [firmware/src/lcd.c]

It reads the array; it does not run the firmware. Class: static check of the source, not a measurement of a panel.
"""
import re
import sys

# the 240x240 sequence, in the order it has to go out: (command, its data)
REFERENCE = [
    (0xB2, [0x0C, 0x0C, 0x00, 0x33, 0x33]),          # PORCTRL: the porches
    (0xB7, [0x35]),                                   # GCTRL: VGH / VGL
    (0xBB, [0x19]),                                   # VCOMS: the common voltage
    (0xC0, [0x2C]),                                   # LCMCTRL
    (0xC2, [0x01]),                                   # VDVVRHEN
    (0xC3, [0x12]),                                   # VRHS
    (0xC4, [0x20]),                                   # VDVS
    (0xC6, [0x0F]),                                   # FRCTRL2
    (0xD0, [0xA4, 0xA1]),                             # PWCTRL1
    (0xE0, [0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23]),
    (0xE1, [0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23]),
]
REQUIRED = {
    0x01: "SWRESET",
    0x11: "SLPOUT (the power block needs the panel awake)",
    0x3A: "COLMOD (16-bit colour)",
    0x36: "MADCTL (scan direction)",
    0x21: "INVON (IPS inversion)",
    0x13: "NORON (normal display mode)",
}
FORBIDDEN = {
    0x51: "brightness value (WRDISBV): the backlight is a GPIO, the unit ignored it",
    0x53: "brightness control (WRCTRLD): the unit ignored it",
    0xC7: "CABC: content-adaptive dimming on an instrument's screen",
}


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
    cmds = [c for c, _ in entries if c]
    bad, note = [], []

    # 1. what may never come back
    for cmd, data in entries:
        if cmd in FORBIDDEN:
            bad.append(f"LCD_SEQ writes {cmd:#04x}: {FORBIDDEN[cmd]}")

    # 2. the commands the panel cannot do without
    for cmd, what in REQUIRED.items():
        if cmd not in cmds:
            bad.append(f"LCD_SEQ does not send {cmd:#04x}: {what}")

    # 3. the published 240x240 block, exactly, once each
    for cmd, want in REFERENCE:
        got = [d for c, d in entries if c == cmd]
        if not got:
            bad.append(f"LCD_SEQ does not send {cmd:#04x}: the 240x240 sequence is incomplete "
                       f"(it wants {len(want)} byte(s): {' '.join(f'{b:02X}' for b in want)})")
            continue
        if len(got) > 1:
            bad.append(f"LCD_SEQ sends {cmd:#04x} {len(got)} times: a register written twice is a table nobody read")
        if list(got[0]) != want:
            bad.append(f"{cmd:#04x} is {' '.join(f'{b:02X}' for b in got[0])}, "
                       f"not the driver's {' '.join(f'{b:02X}' for b in want)}")

    # 4. the block goes out with the panel awake, before the inversion that makes it visible
    pos = {}
    for idx, (c, _) in enumerate(entries):
        pos.setdefault(c, idx)
    if 0x11 in pos and 0x21 in pos:
        for cmd, _ in REFERENCE:
            if cmd in pos and not (pos[0x11] < pos[cmd] < pos[0x21]):
                bad.append(f"{cmd:#04x} is sent outside SLPOUT..INVON: {pos[cmd]} not between "
                           f"{pos[0x11]} and {pos[0x21]}")

    # 5. no command twice (the 0x00 wait is a delay, not a register)
    dups = sorted({c for c in cmds if cmds.count(c) > 1} - {0x00})
    if dups:
        bad.append("LCD_SEQ repeats " + " ".join(f"{c:02X}" for c in dups))

    colmod = [d for c, d in entries if c == 0x3A]
    if colmod and colmod[0] and colmod[0][0] != 0x55:
        bad.append(f"COLMOD is {colmod[0][0]:#04x}, not 0x55 (RGB565): the UI writes 16-bit colour")

    note.append(f"{len(entries)} entries, {len(cmds)} commands: " + " ".join(f"{c:02X}" for c in cmds))
    for n in note:
        print("panel: " + n)
    for b in bad:
        print("panel: FAIL " + b)
    if bad:
        print(f"panel test FAILED ({len(bad)} problem(s))")
        return 1
    print("panel: the init table is the published 240x240 sequence (porch, gate, VCOM, power, both gammas), "
          "with RGB565 and inversion, and no brightness and no CABC; ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
