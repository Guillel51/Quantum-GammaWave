#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The perf log's host side: read `perf,` lines off the unit's USB serial and plot them.

The firmware side is the console's `perf` command (firmware/src/console.c, FASE 7). It emits one CSV line per
interview, absolute counters since boot:

    perf,ms,halves,late,shed,t5_nested,max_us,last_us,cpu_q8,ui_frames,in_audio,stage,page,home,song

This reads that stream from a serial port, a capture file or stdin, differentiates the counters into rates, and
writes three pictures:

    timeline   CPU% of the audio half over time, with the screen in bands and the song marked
    by_screen  what each screen costs: mean, p95 and peak CPU, lateness and voices shed
    by_song    the same per song

Usage:

    python3 tools/perf_log.py --port /dev/ttyACM0            # live from the unit
    python3 tools/perf_log.py --file capture.log --json out/perf.json
    python3 tools/perf_log.py -  < capture.log               # stdin

The console is what paces the stream (`perf on 250`), so a capture of a minute is a few hundred lines; nothing
here is needed on the unit to record it -- `tee` is enough.

What it reads, and what it can only be:

    cpu_q8         Q8 fraction of the half spent inside the render: 256 = 256/256, a whole half
    late           halves whose render ran past the deadline
    shed           voices dropped to stay inside the deadline (audio.c shed_count)
    t5_nested      TIMER4 ticks TIMER5 spent nested in the last half (a key scan inside the render)
    max_us         the worst half since boot (one number, not a window: the ISR does not keep a window max)
    ui_frames      frames the UI drew; its rate is the frame rate the song was drawn at
    stage/page/home and song are the breadcrumbs the firmware already keeps

The class of the measurement is the unit's: these are counters the firmware maintains, read back over the
console. Nothing here can be produced without a unit attached, and a host-side test only proves the parsing and
the drawing.
"""
import argparse
import json
import math
import os
import re
import sys
from pathlib import Path

FIELDS = ("ms halves late shed t5_max max_us last_us cpu_q8 ui_frames in_audio "
          "stage page home song").split()
LINE = re.compile(r"^perf,(-?\d+(?:,-?\d+){%d})\s*$" % (len(FIELDS) - 1))


def parse(lines):
    """[dict] of the good samples, in order. Anything that is not a whole perf line is the console talking."""
    out, bad = [], 0
    for ln in lines:
        m = LINE.match(ln.strip())
        if not m:
            if ln.strip():
                bad += 1
            continue
        v = [int(x) for x in m.group(1).split(",")]
        s = dict(zip(FIELDS, v))
        if s["cpu_q8"] < 0 or s["halves"] < 0:
            bad += 1
            continue
        out.append(s)
    return out, bad


def feed(path=None, port=None, baud=115200, seconds=None):
    """The lines, from a port (needs pyserial), a file, or stdin."""
    if port:
        try:
            import serial                                            # noqa: F401  (optional, for the unit)
        except ImportError:
            raise SystemExit("perf_log: --port needs pyserial (pip install pyserial); "
                             "without a unit, use --file or stdin")
        import serial
        import time
        with serial.Serial(port, baud, timeout=1) as sp:
            t0 = time.monotonic()
            while seconds is None or time.monotonic() - t0 < seconds:
                ln = sp.readline().decode("utf-8", "replace")
                if ln:
                    yield ln
        return
    if path and path != "-":
        with open(path, "r", errors="replace") as f:
            yield from f
        return
    yield from sys.stdin


def rates(samples):
    """The per-sample view: CPU%, and the counters as rates over the interval to the previous sample."""
    rows = []
    prev = None
    for s in samples:
        r = dict(s)
        r["cpu_pct"] = s["cpu_q8"] * 100.0 / 256.0
        if prev and s["ms"] > prev["ms"]:
            dt = (s["ms"] - prev["ms"]) / 1000.0
            r["d_late"] = s["late"] - prev["late"]          # the counts themselves: the groups multiply by 60
            r["d_shed"] = s["shed"] - prev["shed"]          # once, at the end, not per sample
            r["late_min"] = r["d_late"] / dt * 60.0
            r["shed_min"] = r["d_shed"] / dt * 60.0
            r["halves_s"] = (s["halves"] - prev["halves"]) / dt
            r["fps"] = (s["ui_frames"] - prev["ui_frames"]) / dt
            r["dt"] = dt
        else:
            r["late_min"] = r["shed_min"] = r["halves_s"] = r["fps"] = None
            r["d_late"] = r["d_shed"] = 0
            r["dt"] = None
        rows.append(r)
        prev = s
    return rows


def screen_key(s):
    return (s["stage"], s["page"], s["home"])


def agg(rows, key):
    """{group: {n, cpu mean/p95/max, late, shed, fps, halves_s, seconds}} for one grouping."""
    groups = {}
    for r in rows:
        g = groups.setdefault(key(r), {"n": 0, "cpu": [], "late": 0.0, "shed": 0.0, "fps": [], "h": [],
                                       "seconds": 0.0, "peak_us": 0})
        g["n"] += 1
        g["cpu"].append(r["cpu_pct"])
        g["peak_us"] = max(g["peak_us"], r["last_us"])
        if r["dt"]:
            g["seconds"] += r["dt"]
            g["late"] += r["d_late"]
            g["shed"] += r["d_shed"]
            g["fps"].append(r["fps"] or 0.0)
            g["h"].append(r["halves_s"] or 0.0)
    for g in groups.values():
        c = sorted(g["cpu"])
        g["cpu_mean"] = sum(c) / len(c)
        g["cpu_p95"] = c[min(len(c) - 1, int(math.ceil(0.95 * len(c))) - 1)]
        g["cpu_max"] = c[-1]
        g["fps_mean"] = (sum(g["fps"]) / len(g["fps"])) if g["fps"] else 0.0
        g["halves_s"] = (sum(g["h"]) / len(g["h"])) if g["h"] else 0.0
        sec = g["seconds"] or 1e-9
        g["late_per_min"] = g["late"] / sec * 60.0
        g["shed_per_min"] = g["shed"] / sec * 60.0
        del g["cpu"], g["fps"], g["h"]
    return groups


def label_for(key, names, song=False):
    if song:
        return names.get(str(key), "SONG %d" % (int(key) + 1))
    k = "%d/%d/%d" % key
    return names.get(k, "S%d P%d H%d" % key)


# ---------------------------------------------------------------- drawing (Pillow, no other dependency)

BG = (16, 18, 22)
FG = (226, 232, 240)
DIM = (120, 132, 148)
GRID = (38, 44, 54)
CYAN = (34, 211, 238)
GREEN = (74, 222, 128)
AMBER = (251, 191, 36)
RED = (248, 113, 113)
BANDS = [(28, 32, 40), (22, 26, 33)]


def _font(size=13):
    from PIL import ImageFont
    try:
        return ImageFont.load_default(size=size)
    except TypeError:                                                # Pillow < 10.1
        return ImageFont.load_default()


def _canvas(w, h, title, sub=""):
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    d.text((14, 10), title, font=_font(16), fill=FG)
    if sub:
        d.text((14, 30), sub, font=_font(12), fill=DIM)
    return img, d


def _axes(d, box, ymax, ylabel):
    x0, y0, x1, y1 = box
    d.rectangle(box, outline=GRID)
    f = _font(11)
    for i in range(5):
        y = y1 - (y1 - y0) * i / 4.0
        d.line([(x0, y), (x1, y)], fill=GRID)
        d.text((x0 - 6, y - 6), "%.0f" % (ymax * i / 4.0), font=f, fill=DIM, anchor="ra")
    d.text((x0 - 6, y0 - 16), ylabel, font=f, fill=DIM, anchor="la")


def _line(d, box, xs, ys, color, width=2):
    x0, y0, x1, y1 = box
    pts = []
    for x, y in zip(xs, ys):
        if y is None:
            continue
        pts.append((x0 + (x1 - x0) * x, y1 - (y1 - y0) * y))
    if len(pts) > 1:
        d.line(pts, fill=color, width=width)
    return pts


def draw_timeline(rows, names, seat, path):
    """CPU% over time with the screen as bands and the song as a dashed line.

    The bands are drawn for every segment, not only where the screen changes: a capture that alternates screens
    quickly (a player turning pages) would otherwise be all background with two labels at the edges.
    """
    w, h = 1280, 660
    span = max(rows[-1]["ms"] - rows[0]["ms"], 1)
    t0 = rows[0]["ms"]
    img, d = _canvas(w, h, "Quantum GammaWave - performance over time",
                     "%s  |  %d samples  |  %.1f s  |  cpu_q8 of the audio half, the unit's own counters"
                     % (seat, len(rows), span / 1000.0))
    x0, x1 = 66, w - 24
    cpu_box = (x0, 74, x1, 372)
    rate_box = (x0, 424, x1, 566)
    ymax = max(110.0, max(r["cpu_pct"] for r in rows) * 1.1)

    def px(ms):
        return x0 + (x1 - x0) * (ms - t0) / span

    # the screen, as bands: one segment per run of samples, coloured in cycle, labelled where there is room
    segs, start = [], rows[0]
    for r in rows[1:]:
        if screen_key(r) != screen_key(start):
            segs.append((start, r))
            start = r
    segs.append((start, rows[-1]))
    shades = [(30, 35, 44), (26, 30, 39), (34, 30, 42), (28, 36, 38)]
    order = []
    for a, b in segs:
        k = screen_key(a)
        if k not in order:
            order.append(k)
        d.rectangle([px(a["ms"]), cpu_box[1], px(b["ms"]), rate_box[3]],
                    fill=shades[order.index(k) % len(shades)])
        xa, xb = px(a["ms"]), px(b["ms"])
        if xb - xa > 70:
            d.text((xa + 4, cpu_box[1] + 3), label_for(k, names), font=_font(11), fill=DIM)

    # one scale for both rates: a shared axis says 240 late = 240 shed, which is the point of putting them together
    rmax = max(1.0, max((r["late_min"] or 0) for r in rows), max((r["shed_min"] or 0) for r in rows)) * 1.1
    _axes(d, cpu_box, ymax, "%")
    _axes(d, rate_box, rmax, "per min")

    xs = [(r["ms"] - t0) / span for r in rows]
    _line(d, cpu_box, xs, [r["cpu_pct"] / ymax for r in rows], CYAN)
    _line(d, rate_box, xs, [(r["late_min"] or 0) / rmax for r in rows], AMBER)
    _line(d, rate_box, xs, [(r["shed_min"] or 0) / rmax for r in rows], RED)

    # the song: a dashed line where it changes, named at the top
    last = rows[0]["song"]
    for r in rows[1:]:
        if r["song"] != last:
            x = px(r["ms"])
            for y in range(cpu_box[1], cpu_box[3], 8):
                d.line([(x, y), (x, y + 4)], fill=FG)
            d.text((x + 4, cpu_box[1] + 3), label_for(r["song"], names, song=True), font=_font(11), fill=FG)
            last = r["song"]

    for i, (c, lab) in enumerate(((CYAN, "cpu %"), (AMBER, "late / min"), (RED, "shed / min"))):
        d.rectangle([72 + i * 210, h - 52, 88 + i * 210, h - 48], fill=c)
        d.text((94 + i * 210, h - 56), lab, font=_font(11), fill=FG)
    d.text((x0, h - 30), "bands: the screen (stage/page/home)  |  dashed: the song  |  counters are absolute since "
                         "boot, the host differentiates them", font=_font(11), fill=DIM)
    img.save(path)


def draw_bars(groups, names, song, title, sub, path):
    items = sorted(groups.items(), key=lambda kv: -kv[1]["cpu_mean"])
    w, rowh = 1180, 34
    h = 110 + rowh * max(len(items), 1)
    img, d = _canvas(w, h, title, sub)
    f = _font(12)
    x0, x1 = 230, w - 350
    scale = max(10.0, max(g["cpu_mean"] for _, g in items) * 1.15) if items else 100.0
    for i, (k, g) in enumerate(items):
        y = 80 + i * rowh
        d.rectangle([x0, y, x1, y + rowh - 10], fill=GRID)
        xm = x0 + (x1 - x0) * min(1.0, g["cpu_mean"] / scale)
        xp = x0 + (x1 - x0) * min(1.0, g["cpu_p95"] / scale)
        d.rectangle([x0, y, xm, y + rowh - 10], fill=GREEN)
        d.rectangle([xm, y, xp, y + rowh - 10], fill=(120, 190, 140))
        d.text((12, y + 4), label_for(k, names, song), font=f, fill=FG)
        d.text((x1 + 10, y + 2), "%.1f%% mean   %.1f p95   %.0f peak" % (g["cpu_mean"], g["cpu_p95"], g["cpu_max"]),
               font=f, fill=DIM)
        d.text((x1 + 10, y + 17), "late %.1f/min   shed %.1f/min   %.0f fps" %
               (g["late_per_min"], g["shed_per_min"], g["fps_mean"]), font=_font(10), fill=DIM)
    d.text((12, h - 24), "green: mean  ·  pale: p95  ·  scale 0..%.0f%%" % scale, font=_font(11), fill=DIM)
    img.save(path)


def summary(groups, names, song):
    lines = []
    head = ("song" if song else "screen")
    lines.append("%-24s %5s %8s %8s %8s %9s %9s %7s" %
                 (head, "n", "cpu mean", "cpu p95", "cpu peak", "late/min", "shed/min", "fps"))
    for k, g in sorted(groups.items(), key=lambda kv: -kv[1]["cpu_mean"]):
        lines.append("%-24s %5d %7.1f%% %7.1f%% %7.1f%% %9.1f %9.1f %7.0f" %
                     (label_for(k, names, song), g["n"], g["cpu_mean"], g["cpu_p95"], g["cpu_max"],
                      g["late_per_min"], g["shed_per_min"], g["fps_mean"]))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="read the unit's perf log and plot it")
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--port", help="serial device (needs pyserial), e.g. /dev/ttyACM0")
    src.add_argument("--file", help="a capture of the console's output ('-' for stdin)")
    src.add_argument("capture", nargs="?", help="a capture file, or '-' for stdin")
    ap.add_argument("--baud", type=int, default=115200, help="ignored by the device: the CDC rate is nominal")
    ap.add_argument("--seconds", type=float, help="stop reading the port after this many seconds")
    ap.add_argument("--out", default="build/perf", help="where the pictures go (default build/perf)")
    ap.add_argument("--json", help="write the summary as JSON here")
    ap.add_argument("--screens", default="", help="names: '1/2/0=EDIT,0/3/0=HOME' for the screen bands")
    ap.add_argument("--songs", default="", help="names: '0=MY SONG,1=OTHER'")
    a = ap.parse_args()

    if not (a.port or a.file or a.capture):
        ap.error("give --port, --file or a capture")
    src_path = a.file or a.capture

    def names_of(spec):
        out = {}
        for part in spec.split(","):
            if "=" in part:
                k, v = part.split("=", 1)
                out[k.strip()] = v.strip()
        return out

    screens, songs = names_of(a.screens), names_of(a.songs)
    seat = a.port or src_path or "stdin"

    samples, bad = parse(feed(src_path, a.port, a.baud, a.seconds))
    if not samples:
        raise SystemExit("perf_log: no 'perf,' lines in %s (%d other lines). On the unit: 'perf on 250'"
                         % (seat, bad))
    rows = rates(samples)
    by_screen = agg(rows, screen_key)
    by_song = agg(rows, lambda r: r["song"])

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    span = (samples[-1]["ms"] - samples[0]["ms"]) / 1000.0
    sub = "%s · %.1f s · %d samples · %d lines that were not perf" % (seat, span, len(samples), bad)
    draw_timeline(rows, {**screens}, seat, out / "timeline.png")
    draw_bars(by_screen, {**screens}, False, "The audio half, by screen", sub, out / "by_screen.png")
    draw_bars(by_song, {**songs}, True, "The audio half, by song", sub, out / "by_song.png")

    text = "\n".join([summary(by_screen, screens, False), "", summary(by_song, songs, True)])
    print(text)
    print("\nperf_log: %s  (%d samples, %.1f s, worst half %.0f us, shed %d)"
          % (out, len(samples), span, max(s["max_us"] for s in samples),
             samples[-1]["shed"] - samples[0]["shed"]))
    if a.json:
        Path(a.json).write_text(json.dumps({
            "source": seat, "samples": len(samples), "seconds": span,
            "by_screen": {label_for(k, screens): v for k, v in by_screen.items()},
            "by_song": {label_for(k, songs, True): v for k, v in by_song.items()},
        }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
