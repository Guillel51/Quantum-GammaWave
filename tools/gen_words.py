#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The screen's words, as the FM-1 Screen Reader reads them (issue #105, FASE 8).

The protocol (fm1-reader/PROTOCOL.md, "The words file") has the device send its screen's own text and answer
a reader's HELLO with the https URL of a JSON file that says how to read it. The file's name is a fingerprint
of its content, so the same words always give the same name and a link never moves:

    {"format":1,"words":[["ATK","attack"],["REV","reverb"]]}

This tool is the whole of that path here: it reads firmware/words.tsv, writes site/sr/w/<fingerprint>.json
(published with the site, next to the emulator), and --check verifies that announce.c's ANN_WORDS_URL still
names that exact file. It is deliberately independent of fm1-base's tools/gen_words.py (which is MPL-2.0 and
adds the reader's own common words): the reader applies its common rules by itself, so all this file adds is
this firmware's own abbreviations.

    python3 tools/gen_words.py            # write the file the words.tsv asks for
    python3 tools/gen_words.py --check    # 0 or a reason, for the test suite
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TSV = ROOT / "firmware" / "words.tsv"
OUT = ROOT / "site" / "sr" / "w"
ANNOUNCE = ROOT / "firmware" / "src" / "announce.c"


def entries():
    """[from, to] in file order: an earlier entry wins a tie, as the protocol says."""
    if not TSV.exists():
        raise SystemExit(f"gen_words: {TSV} is missing")
    out = []
    for n, line in enumerate(TSV.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 2 or not parts[0].strip() or not parts[1].strip():
            raise SystemExit(f"gen_words: {TSV.name}:{n}: want 'text<TAB>words', got {line!r}")
        src, dst = parts[0].strip(), parts[1].strip()
        if any(ord(c) < 0x20 or ord(c) > 0x7E for c in src + dst):
            raise SystemExit(f"gen_words: {TSV.name}:{n}: printable ASCII only (the frame is 7-bit)")
        if [src.upper()] in [[a.upper()] for a, _ in out]:
            raise SystemExit(f"gen_words: {TSV.name}:{n}: {src!r} is already listed")
        out.append([src, dst])
    if not out:
        raise SystemExit(f"gen_words: {TSV.name} lists no words")
    return out


def body(words):
    # The exact bytes that are hashed and served: no trailing spaces, one line, keys in this order.
    return json.dumps({"format": 1, "words": words}, ensure_ascii=True, separators=(",", ":"))


def fingerprint(text):
    return hashlib.sha256(text.encode("ascii")).hexdigest()[:8]


def built():
    text = body(entries())
    return text, fingerprint(text), OUT / f"{fingerprint(text)}.json"


def check():
    text, fp, path = built()
    if not path.exists():
        return f"{path.relative_to(ROOT)} does not exist (run tools/gen_words.py)"
    if path.read_bytes() != text.encode("ascii"):
        return f"{path.relative_to(ROOT)} does not match words.tsv (run tools/gen_words.py)"
    m = re.findall(r'#define\s+ANN_WORDS_URL\s+"([^"]*)"', ANNOUNCE.read_text(encoding="utf-8"))
    if not m:
        return "announce.c has no ANN_WORDS_URL"
    url = m[-1]
    if not url:
        return "ANN_WORDS_URL is empty: the reader falls back to its common rules only"
    if not url.startswith("https://") or not url.endswith(f"/{fp}.json"):
        return f"ANN_WORDS_URL {url!r} does not name {fp}.json"
    if not url.startswith("https://guillel51.github.io/Quantum-GammaWave/sr/w/"):
        return f"ANN_WORDS_URL {url!r} is not under the published site's /sr/w/"
    # The answer to HELLO carries name, 0, link: it has to fit one SAY frame or the firmware drops the link.
    name = re.search(r'#define\s+ANN_NAME\s+.*?(?:\n|$)', ANNOUNCE.read_text(encoding="utf-8"))
    if name and len("Quantum GammaWave " + "0" * 8) + 1 + len(url) + 2 > 120:
        return "ANN_WORDS_URL + ANN_NAME is too long for one HELLO frame"
    return None


def main(argv):
    if "--check" in argv:
        why = check()
        if why:
            print(f"gen_words: FAIL: {why}")
            return 1
        text, fp, path = built()
        print(f"gen_words: ok: {path.relative_to(ROOT)} ({len(entries())} words, {len(text)} B, {fp})")
        return 0
    text, fp, path = built()
    OUT.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode("ascii"))
    print(f"gen_words: wrote {path.relative_to(ROOT)} ({len(entries())} words, {len(text)} B fingerprint {fp})")
    print(f"gen_words: ANN_WORDS_URL should be {published_url(fp)}")
    return 0


def published_url(fp):
    return f"https://guillel51.github.io/Quantum-GammaWave/sr/w/{fp}.json"


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
