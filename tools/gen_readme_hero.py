#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (JIANT 0.5; 0.6.30: from the presentation video's footage) the README's images, docs/jiant-hero.png and
# docs/jiant-screens.png, from the FM-1 screens the video's capture recorded (tools/video/capture.c -> screen.rgb,
# 30 fps, its song at 125 BPM): every screen alive, as the firmware drew it while the song played. (0.6.89) The newer
# pages from tools/video/capture_shots.c (the same song): shots.rgb, shots.txt.
#   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video/capture tools/video/capture.c -lm
#   build/video/capture build/video
#   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video/capture_shots tools/video/capture_shots.c -lm
#   build/video/capture_shots build/video
#   python3 tools/gen_readme_hero.py build/video docs        (needs PIL, numpy)
import re, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
R = Path(__file__).resolve().parents[1]
V = Path(sys.argv[1] if len(sys.argv) > 1 else R / 'build/video')
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else R / 'docs')
VERSION = re.search(r'FELUCCA_VERSION "(v[^"]+)"', (R / 'firmware/src/felucca.c').read_text()).group(1)
F = R / 'assets/fonts'
def font(w, s): return ImageFont.truetype(str(F / f'ChakraPetch-{w}.ttf'), s)
H = [(0, 210, 255), (90, 60, 255), (180, 50, 230), (240, 40, 50), (255, 122, 0), (255, 215, 0), (255, 255, 255)]
def heat(v):
    v = max(0, min(1, v)) * 6; k = min(5, int(v)); f = v - k
    return tuple(int(H[k][j] + (H[k + 1][j] - H[k][j]) * f) for j in range(3))
BG, TXT, MID, DIM = (0, 0, 0), (244, 241, 234), (160, 154, 150), (80, 76, 84)

SCR = np.memmap(V / 'screen.rgb', dtype=np.uint8, mode='r').reshape(-1, 240, 240, 3)
BEAT = 60 / 125
# (0.6.89) the newer pages, from tools/video/capture_shots.c: shots.rgb, their names in shots.txt
SHOTS = np.fromfile(V / 'shots.rgb', dtype=np.uint8).reshape(-1, 240, 240, 3)
SHOT_N = [l for l in (V / 'shots.txt').read_text().split('\n') if l]
def shot(beat):                                      # the screen at that beat of the film, or a newer page by its name
    if isinstance(beat, str):
        return Image.fromarray(np.array(SHOTS[SHOT_N.index(beat)]))
    return Image.fromarray(np.array(SCR[int(beat * BEAT * 30)]))
# the film's beats (capture.c's script, 0.6.37)
HOME, ANALOG, DRUMX, DICE, ARRANGE, LOAD = 232, 34.5, 18.5, 193.5, 186.5, 205.5
MIXER, RESO, SLICER, PUNCH, FM6, MSEQ, MASTER, DIST, TOOLS, SCALE = 178, 122.5, 146.5, 155, 50.5, 98.5, 162, 138.5, 222.5, 209.5

def signature(d, ox, oy, sc, width):
    src = (R / 'firmware/src/ui_signature.h').read_text()
    nums = [int(x) for x in re.findall(r'\b\d+\b', src.split('{', 1)[1])]
    pts = [(nums[i], nums[i + 1]) for i in range(0, len(nums) - 1, 2)]
    segs, L = [], 0
    for i in range(len(pts) - 1):
        if pts[i + 1][1] & 128: continue
        a = (pts[i][0], pts[i][1] & 127); b = (pts[i + 1][0], pts[i + 1][1] & 127)
        l = max(abs(b[0] - a[0]), abs(b[1] - a[1])); segs.append((a, b, L)); L += l
    for a, b, acc in segs:
        d.line([(ox + a[0] * sc, oy + a[1] * sc), (ox + b[0] * sc, oy + b[1] * sc)], fill=heat(0.08 + 0.86 * acc / L), width=width)

def framed(im, img, x, y, s, resample=Image.NEAREST):
    d = ImageDraw.Draw(im)
    d.rectangle([x - 3, y - 3, x + 240 * s + 2, y + 240 * s + 2], outline=(40, 34, 48), width=2)
    im.paste(img.resize((int(240 * s), int(240 * s)), resample), (x, y))

# ---- the hero
W, Hh = 1600, 1900
im = Image.new('RGB', (W, Hh), BG); d = ImageDraw.Draw(im)
signature(d, 70, 40, 2.6, 5)
d.text((690, 70), "JIANT FM", font=font('Bold', 128), fill=TXT)
d.text((696, 215), "bio-synthetic operating system for the M-VAVE FM-1", font=font('Medium', 30), fill=MID)
d.text((696, 258), f"firmware {VERSION}  ·  synthesis · sequencing · mutation · performance", font=font('Medium', 24), fill=DIM)
for i in range(W - 140):
    d.line([(70 + i, 330), (70 + i, 335)], fill=heat(i / (W - 140)))
cap = font('SemiBold', 22)
y0 = 370
big = [(HOME, 'HOME · six living tracks'), (DICE, 'DICE ALL · a new song idea'), (ARRANGE, 'ARRANGE · the song in a column')]
for i, (b, c) in enumerate(big):
    x = 70 + i * 500; framed(im, shot(b), x, y0, 2); d.text((x, y0 + 492), c, font=cap, fill=heat(0.15 + 0.3 * i))
strips = [[('TIMING', 'TIMING · OFF THE GRID'), ('HUMAN', 'HUMAN'), ('STRETCH', 'STRETCH · REPEAT'), ('CHORD', 'CHORD MODE'),
           ('DLY', 'DELAY · FEEDBACK'), ('DLY 2', 'DELAY · MOD')],
          [(ANALOG, 'ACID ANALOG'), (DRUMX, 'DRUM-X'), (FM6, 'FM6 OPERATORS'), (RESO, 'RESO REVERB'), (PUNCH, 'PUNCH-IN FX'),
           (MIXER, 'MIXER · 6')]]
y1 = y0 + 545
for r, row in enumerate(strips):
    for i, (b, c) in enumerate(row):
        x = 70 + i * 245; y = y1 + r * 270
        im.paste(shot(b).resize((216, 216), Image.LANCZOS), (x, y))
        d.text((x, y + 222), c, font=font('SemiBold', 18), fill=MID)
y2 = y1 + 2 * 270 + 20
cards = [("OFF THE GRID", "REC Q keeps where you played;\nTIMING moves a step by 1/16."),
         ("HUMAN", "Velocity, timing and gate breathe\na little differently every loop."),
         ("STRETCH · REPEAT", "x2..x8 or squeezed; clone a pattern\nof any length up to 64 steps."),
         ("ANALOG DELAY", "Feedback past 100 % self-oscillates;\nresonant TONE, tape MOD."),
         ("CHORD MODE", "One key, a whole chord; recorded\nwhole, strummed, lit on the keys."),
         ("SONG IDEAS", "HOME + SELECT writes a whole song:\n21 genres, never the same twice."),
         ("ARRANGE", "Sections in a column, bars and scenes;\nEND plays x2..x8 or loops."),
         ("DRUM-X", "Synthesized drums: morph from A to B,\nFOLD, FM ENV, TONE: no samples."),
         ("FIVE REVERBS", "ROOM, SPRING, SHIMMER, RESO tuned\nto the scale and a granular CLOUD.")]
ft, fd = font('SemiBold', 27), font('Medium', 20)
for i, (t, txt) in enumerate(cards):
    cx = 70 + (i % 3) * 500; cy = y2 + (i // 3) * 125
    d.rectangle([cx, cy, cx + 5, cy + 92], fill=heat(i / (len(cards) - 1)))
    d.text((cx + 22, cy - 2), t, font=ft, fill=TXT)
    d.multiline_text((cx + 22, cy + 38), txt, font=fd, fill=MID, spacing=6)
d.text((70, Hh - 50), "GPL-3.0  ·  fork of Felucca (Hügelton Instruments)  ·  song mode from SLOOP  ·  juanjiant-bit.github.io/JIANT",
       font=font('Medium', 20), fill=DIM)
im.save(OUT / 'jiant-hero.png'); print(OUT / 'jiant-hero.png', im.size)

# ---- the screens: six of them, 2x, as a grid
sc = [(HOME, 'HOME'), (ARRANGE, 'ARRANGE'), ('TIMING', 'TIMING · OFF THE GRID'), ('STRETCH', 'STRETCH · REPEAT'),
      ('DLY', 'DELAY · FEEDBACK'), (DRUMX, 'DRUM-X')]
g = Image.new('RGB', (3 * 520 + 40, 2 * 560 + 30), BG); d = ImageDraw.Draw(g)
for i, (b, c) in enumerate(sc):
    x = 40 + (i % 3) * 520; y = 30 + (i // 3) * 560
    framed(g, shot(b), x, y, 2)
    d.text((x, y + 492), c, font=font('SemiBold', 24), fill=heat(0.1 + 0.16 * i))
g.save(OUT / 'jiant-screens.png'); print(OUT / 'jiant-screens.png', g.size)
