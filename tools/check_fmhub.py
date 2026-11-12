#!/usr/bin/env python3
"""Check the Firmware Hub card file (`fmhub.json`) against the hub's published rules.

The hub reads `fmhub.json` from the default branch and IGNORES THE WHOLE FILE when one field is
unknown or out of bounds, so a typo silently costs the card its text. This tool makes that visible
before a push, and it is what `docs/FM1_HUB_READINESS.md` cites.

Rules, from https://thefirmwarehub.com/kb/ (read 2026-11-12) and the live examples of other
firmwares' files (RemoteFM1, zp12):

  * every field is optional, and only these are accepted:
        what, author, kinds, play, editor, cheat, image, color
  * what     plain text, 20..300 characters, no HTML, no links
  * author   2..60 characters
  * kinds    exactly one of synth, groove, drums, chords, tracker, other
  * play / editor / cheat   https:// addresses
  * image    a path in THIS repository: square PNG/WebP/JPEG, 240..1024 px a side, at most 300 KB
  * color    #RRGGBB; the card's accent. Checked for contrast: a bright accent needs a high ratio
             against black and enough luminance to read as a colour rather than a dark panel.

Exit status: 0 every rule holds, 1 a rule is broken, 2 the file or the image cannot be read.

    python3 tools/check_fmhub.py            # the file in the repository root
    python3 tools/check_fmhub.py --offline  # do not fetch the links
"""

import argparse
import json
import re
import struct
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIELDS = {"what", "author", "kinds", "play", "editor", "cheat", "image", "color"}
KINDS = {"synth", "groove", "drums", "chords", "tracker", "other"}
HEX = re.compile(r"^#[0-9a-fA-F]{6}$")
HTML = re.compile(r"<[a-zA-Z/][^>]*>")
URL = re.compile(r"https?://|www\.", re.I)


def rel_luminance(hex6):
    chan = [int(hex6[i:i + 2], 16) / 255.0 for i in (1, 3, 5)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in chan]
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]


def contrast(a, b):
    la, lb = rel_luminance(a), rel_luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def image_size(path):
    """(width, height, format) from the file header, so the check needs no PIL."""
    with open(path, "rb") as f:
        head = f.read(64)
    if head[:8] == b"\x89PNG\r\n\x1a\n":
        w, h = struct.unpack(">II", head[16:24])
        return w, h, "png"
    if head[:2] == b"\xff\xd8":
        # JPEG: walk to the first SOFn segment
        with open(path, "rb") as f:
            f.seek(2)
            while True:
                b = f.read(1)
                if not b:
                    return 0, 0, "jpeg"
                if b != b"\xff":
                    continue
                m = f.read(1)
                while m == b"\xff":
                    m = f.read(1)
                if m and 0xC0 <= m[0] <= 0xCF and m[0] not in (0xC4, 0xC8, 0xCC):
                    f.read(3)
                    h, w = struct.unpack(">HH", f.read(4))
                    return w, h, "jpeg"
                seg = f.read(2)
                if len(seg) < 2:
                    return 0, 0, "jpeg"
                f.seek(struct.unpack(">H", seg)[0] - 2, 1)
    if head[:4] == b"RIFF" and head[8:12] == b"WEBP":
        return 0, 0, "webp"
    return 0, 0, "?"


def http_ok(url):
    try:
        req = urllib.request.Request(url, method="GET", headers={"User-Agent": "qg1-check-fmhub"})
        with urllib.request.urlopen(req, timeout=25) as r:
            return 200 <= r.status < 400, r.status
    except Exception as e:                                       # noqa: BLE001 - any failure is a failure
        return False, str(e)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", default=str(ROOT / "fmhub.json"))
    ap.add_argument("--offline", action="store_true", help="do not fetch the links")
    a = ap.parse_args()

    path = Path(a.path)
    bad = []

    def fail(msg):
        bad.append(msg)
        print(f"FAIL  {msg}")

    def good(msg):
        print(f"ok    {msg}")

    try:
        raw = path.read_text(encoding="utf-8")
        card = json.loads(raw)
    except Exception as e:                                       # noqa: BLE001
        print(f"cannot read {path}: {e}")
        return 2
    good(f"{path.name} is valid JSON")

    if not isinstance(card, dict):
        fail("the file must be a JSON object")
        card = {}

    unknown = sorted(set(card) - FIELDS)
    if unknown:
        fail(f"unknown field(s) {unknown}: the hub ignores the WHOLE file, not just the field")
    else:
        good(f"only accepted fields ({', '.join(sorted(card))})")

    what = card.get("what")
    if what is not None:
        if not isinstance(what, str):
            fail("what must be a string")
        else:
            n = len(what)
            if not 20 <= n <= 300:
                fail(f"what is {n} characters (the hub allows 20..300)")
            elif HTML.search(what):
                fail("what carries HTML tags (plain text only)")
            elif URL.search(what):
                fail("what carries a link (links belong in play / editor / cheat)")
            elif not what[0].isupper() or not what.rstrip().endswith((".", "!", "?")):
                fail("what does not read as a sentence (starts upper case, ends with punctuation)")
            else:
                good(f"what: {n} characters, plain text, no links")
    else:
        print("note  no what: the hub writes the card's text itself")

    author = card.get("author")
    if author is not None:
        if not isinstance(author, str) or not 2 <= len(author) <= 60:
            fail("author must be 2..60 characters")
        else:
            good(f"author: {author}")

    kinds = card.get("kinds")
    if kinds is not None:
        if (not isinstance(kinds, list) or len(kinds) != 1 or not isinstance(kinds[0], str)
                or kinds[0] not in KINDS):
            fail(f"kinds must be a list with exactly one of {sorted(KINDS)}")
        else:
            good(f"kinds: ['{kinds[0]}']")

    for field in ("play", "editor", "cheat"):
        v = card.get(field)
        if v is None:
            continue
        if not isinstance(v, str) or not v.startswith("https://"):
            fail(f"{field} must be an https:// address")
            continue
        if a.offline:
            good(f"{field}: {v} (not fetched: --offline)")
            continue
        ok, status = http_ok(v)
        if ok:
            good(f"{field}: {v} answers {status}")
        else:
            fail(f"{field}: {v} does not answer ({status})")

    img = card.get("image")
    if img is not None:
        p = (ROOT / img).resolve() if not Path(img).is_absolute() else Path(img)
        try:
            if ROOT not in p.parents:
                fail(f"image {img} is outside this repository")
            elif not p.is_file():
                fail(f"image {img} does not exist in this repository")
            else:
                size = p.stat().st_size
                w, h, fmt = image_size(p)
                if fmt not in ("png", "jpeg"):
                    fail(f"image {img} is {fmt}: the hub takes PNG, WebP or JPEG")
                elif w and h and w != h:
                    fail(f"image {img} is {w}x{h}: it must be square")
                elif w and not 240 <= w <= 1024:
                    fail(f"image {img} is {w} px a side: the hub takes 240..1024")
                elif size > 300 * 1024:
                    fail(f"image {img} is {size} B: the hub takes at most 300 KB")
                else:
                    good(f"image: {img} {w}x{h} {fmt.upper()}, {size} B")
        except OSError as e:
            fail(f"image {img}: {e}")

    col = card.get("color")
    if col is not None:
        if not isinstance(col, str) or not HEX.match(col):
            fail("color must be #RRGGBB")
        else:
            lum = rel_luminance(col)
            c_black = contrast(col, "#000000")
            c_white = contrast(col, "#ffffff")
            c_ink = contrast(col, "#101014")
            print(f"      {col}: luminance {lum:.3f}; contrast on black {c_black:.2f}:1, "
                  f"on white {c_white:.2f}:1; dark ink #101014 on it {c_ink:.2f}:1")
            if lum < 0.25:
                fail(f"color {col} is not bright (luminance {lum:.3f} < 0.25)")
            elif c_black < 6.0:
                fail(f"color {col} is too close to a dark card ({c_black:.2f}:1 on black)")
            elif c_ink < 4.5 and c_white < 4.5:
                fail(f"color {col} leaves no readable ink on it "
                     f"(dark {c_ink:.2f}:1, white {c_white:.2f}:1)")
            else:
                ink = "#101014" if c_ink >= c_white else "#ffffff"
                good(f"color: {col}, bright, {c_black:.1f}:1 on a dark card, "
                     f"readable ink {ink} ({max(c_ink, c_white):.1f}:1)")

    print()
    if bad:
        print(f"fmhub.json: {len(bad)} problem(s)")
        return 1
    print("fmhub.json: every rule the hub publishes holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
