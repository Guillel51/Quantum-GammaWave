#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The perf log's host side, on a capture made by hand.

The firmware's `perf` command and this tool have to agree on one thing: the shape of the line. Nothing here can
prove what the unit does -- that needs the unit -- but it does prove that a stream in that shape is read,
differentiated into rates, grouped by screen and by song, and drawn. The numbers are exact, so a change in the
unit's order of fields fails here instead of on the unit.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / "tools" / "perf_log.py"

bad = 0


def check(what, ok, detail=""):
    global bad
    print("perf log: %-78s %s%s" % (what, "ok" if ok else "FAIL", ("  " + detail) if detail and not ok else ""))
    bad += not ok


def make_log(path, samples=240, step=250):
    """Three screens at 40/60/80 % and two songs; the third screen is the one that runs late and sheds."""
    t = 1000
    halves = late = shed = frames = 0
    lines = ["Felucca v9.9.9 console - 'help'", "> perf on 250", "perf_ms 250"]
    for i in range(samples):
        song = 0 if i < samples // 2 else 1
        stage, page, home = (0, 1, 0) if i % 3 == 0 else (0, 2, 0) if i % 3 == 1 else (4, 0, 1)
        q8 = 102 if stage == 0 and page == 1 else 154 if page == 2 else 205
        halves += 43                                   # ~172 halves a second at a 250 ms step
        frames += 15
        if page == 0:
            late += 1
            shed += 2
        t += step
        lines.append("perf,%d,%d,%d,%d,%d,%d,%d,%d,%d,0,%d,%d,%d,%d"
                     % (t, halves, late, shed, 3, 2631, 2400 + q8 * 3, q8, frames, stage, page, home, song))
    path.write_text("\n".join(lines) + "\n")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        log = tmp / "capture.log"
        make_log(log)
        js = tmp / "perf.json"
        out = tmp / "pics"
        r = subprocess.run([sys.executable, str(TOOL), "--file", str(log), "--out", str(out), "--json", str(js)],
                           capture_output=True, text=True)
        check("a capture is read and summarised", r.returncode == 0, r.stderr.strip()[:200])
        if r.returncode:
            print(r.stdout)
            return 1
        j = json.loads(js.read_text())
        # 239 intervals of 250 ms: the first sample has no interval before it
        check("240 samples over 59.75 s", j["samples"] == 240 and abs(j["seconds"] - 59.75) < 1e-6, str(j["seconds"]))
        screens = j["by_screen"]
        check("three screens, grouped by stage/page/home", len(screens) == 3, str(list(screens)))
        means = {k: v["cpu_mean"] for k, v in screens.items()}
        near = all(abs(m - want) < 1.0 for m, want in zip(sorted(means.values()), [39.8, 60.2, 80.1]))
        check("each screen's mean CPU is what the capture said (40 / 60 / 80 %)", near,
              str(sorted(round(m, 1) for m in means.values())))
        # the busy screen adds one late half and two shed voices every 250 ms: 240 and 480 a minute, not 240*240
        worst = max(v["late_per_min"] for v in screens.values())
        worst_shed = max(v["shed_per_min"] for v in screens.values())
        check("late and shed are attributed to the screen that produced them, per minute",
              abs(worst - 240.0) < 2.0 and abs(worst_shed - 480.0) < 4.0
              and min(v["late_per_min"] for v in screens.values()) == 0.0,
              "late %.1f shed %.1f" % (worst, worst_shed))
        songs = j["by_song"]
        check("two songs, grouped by the song field", len(songs) == 2 and all(v["n"] == 120 for v in songs.values()),
              str(songs.get("SONG 1", {}).get("n")))
        check("the frame rate of the UI comes out of ui_frames (60 fps at a 250 ms step)",
              all(abs(v["fps_mean"] - 60.0) < 0.5 for v in songs.values()),
              str([round(v["fps_mean"], 1) for v in songs.values()]))
        for name in ("timeline.png", "by_screen.png", "by_song.png"):
            f = out / name
            check("%s is written and is a picture" % name, f.exists() and f.stat().st_size > 5000,
                  str(f.stat().st_size) if f.exists() else "missing")
        check("the summary is printed for both groupings",
              "screen" in r.stdout and "song" in r.stdout and "cpu mean" in r.stdout)

        empty = tmp / "empty.log"
        empty.write_text("Felucca console\r\n> status\r\n")
        r2 = subprocess.run([sys.executable, str(TOOL), "--file", str(empty), "--out", str(out)],
                            capture_output=True, text=True)
        check("a capture with no perf line fails instead of drawing nothing",
              r2.returncode != 0 and "no 'perf,' lines" in (r2.stdout + r2.stderr))

    if bad:
        print("perf log: %d FAILED" % bad)
    else:
        print("perf log: all passed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
