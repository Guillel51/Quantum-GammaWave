#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The power-on order of the audio path, checked on the source that implements it.

The pop is an ordering defect and cannot be heard here, but the order that prevents it can be read:

  * `main.c` must hold the output (`audio_boot_hold`) BEFORE `audio_init()`: the codec bring-up starts ALNK0,
    renders 25 ms and only then unmutes the output stage, so a gate armed during it spends the fade behind a
    mute and the unmute meets a DAC already most of the way up.
  * `audio_fade_in()` must come after the second core's start (which runs with interrupts off), i.e. after
    the last stage that can drop halves on silence.
  * `hal/fm1_audio.h` must start the engine (`CON0 |= 0x800`) before it unmutes (`pc_out(6, 1)`), and must
    wait on the zeroed buffer in between.

Class: static check of this tree. What it cannot do is prove the codec settles: that is a measurement on the
unit (docs/VALIDATION_MATRIX.md).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "firmware/src/main.c").read_text()
hal = (ROOT / "firmware/hal/fm1_audio.h").read_text()
bad = []


def at(text, needle, what):
    i = text.find(needle)
    if i < 0:
        bad.append(f"missing: {what} ({needle})")
    return i


hold = at(main, "audio_boot_hold();", "the boot's hold in main.c")
init = at(main, "audio_init();", "audio_init() in main.c")
arm = at(main, "audio_fade_in();", "audio_fade_in() in main.c")
if -1 not in (hold, init) and hold > init:
    bad.append("main.c arms the engine before holding the output: the fade is spent behind the mute")
if -1 not in (init, arm) and arm < init:
    bad.append("main.c arms the fade before the engine exists")

mc = main.find("fm1_mc_start();")
if mc >= 0 and arm >= 0 and arm < mc:
    bad.append("main.c opens the output before the second core starts: its interrupts-off halves are not on silence")

eng = at(hal, "CON0 |= 0x800u;", "the engine start in fm1_audio_init")
unmute = at(hal, "fm1__pc_out(6, 1);", "the output unmute in fm1_audio_init")
wait = at(hal, "fm1_delay_ms(25);", "the settle wait")
if -1 not in (eng, unmute) and unmute < eng:
    bad.append("fm1_audio_init unmutes before the engine runs: the pop the 1.0.2 note fixed is back")
if -1 not in (wait, unmute) and wait > unmute:
    bad.append("fm1_audio_init unmutes before the settle wait")

for label, i in (("hold", hold), ("init", init), ("arm", arm), ("engine", eng), ("settle", wait), ("unmute", unmute)):
    print(f"boot order: {label:8s} {'missing' if i < 0 else 'ok'}")
print(f"boot order: {'FAILED' if bad else 'ok'}" + ("" if not bad else ": " + "; ".join(bad)))
sys.exit(1 if bad else 0)
