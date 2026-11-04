#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static coverage check for the portal dictionaries.

The portal keeps Spanish as the source language so it stays readable without JavaScript. This
extracts every text node and user-facing attribute outside script/style and requires a key in both
the `en` and `zh` maps of web/site/i18n.js. It catches a new button or warning that would otherwise
remain Spanish when English is selected.
"""
import html
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "site"
SKIP = re.compile(r"^(https?:|/|#)")
LETTERS = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]")
PATHLIKE = re.compile(r"^[^\s]+/[^\s]+$")


class Text(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.values = []
        self.stack = []

    def handle_starttag(self, tag, attrs):
        if tag in ("script", "style", "noscript"):
            self.stack.append(tag)
        for key, value in attrs:
            if key in ("alt", "aria-label", "title", "placeholder") and value:
                self.values.append(value.strip())

    def handle_endtag(self, tag):
        if tag in ("script", "style", "noscript") and self.stack and self.stack[-1] == tag:
            self.stack.pop()

    def handle_data(self, data):
        if not self.stack and data.strip():
            self.values.append(data.strip())


def keys(block):
    return set(re.findall(r'^\s+"((?:[^"\\]|\\.)*)"\s*:', block, re.M))


def main():
    page = Text()
    page.feed((SOURCE / "index.html").read_text(encoding="utf-8"))
    source = (SOURCE / "i18n.js").read_text(encoding="utf-8")
    try:
        en = source.split("    en: {", 1)[1].split("\n    zh: {", 1)[0]
        zh = source.split("    zh: {", 1)[1].split("\n  };", 1)[0]
    except IndexError:
        print("i18n: cannot find the en/zh dictionaries", file=sys.stderr)
        return 1
    en, zh = keys(en), keys(zh)
    en = {html.unescape(k) for k in en}
    zh = {html.unescape(k) for k in zh}
    values = []
    for value in page.values:
        value = " ".join(value.split())
        if not value or not LETTERS.search(value) or SKIP.match(value) or PATHLIKE.match(value):
            continue
        if value not in values:
            values.append(value)
    missing_en = [v for v in values if v not in en]
    missing_zh = [v for v in values if v not in zh]
    print(f"i18n: {len(values)} source strings, {len(en)} EN keys, {len(zh)} ZH keys")
    for label, missing in (("EN", missing_en), ("ZH", missing_zh)):
        for value in missing:
            print(f"  {label} missing: {value!r}")
    if missing_en or missing_zh:
        print(f"i18n: {len(missing_en)} EN and {len(missing_zh)} ZH missing", file=sys.stderr)
        return 1
    print("i18n: every source string has EN and ZH")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
