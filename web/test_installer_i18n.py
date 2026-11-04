#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static i18n checks for the self-contained USB installer.

The installer has two layers: semantic `data-t` keys in TEXT and a source-English STATIC map for
the header, spec cards, controls and footer. This checks that every data-t key exists in EN, ES
and ZH and that the language selector applies both layers and persists the choice.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
source = (HERE / "index_pkg.html").read_text(encoding="utf-8")
bad = []


def block(name):
    match = re.search(rf"TEXT\.{name} = \{{(.*?)\n\}};", source, re.S)
    return match.group(1) if match else ""


def object_block(name):
    match = re.search(rf"{name}: \{{(.*?)\n  \}},", source, re.S)
    return match.group(1) if match else ""


def has_key(text, key):
    return bool(re.search(rf"\b{re.escape(key)}\s*:", text))


en = block("en")
# TEXT.en is nested in const TEXT; use the explicit heading and the first closing brace at its level.
m = re.search(r"^  en: \{(.*?)^  \},", source, re.S | re.M)
if m:
    en = m.group(1)
es, zh = block("es"), block("zh")
if not (en and es and zh):
    bad.append("TEXT.en / TEXT.es / TEXT.zh is missing")

used = sorted(set(re.findall(r'data-t="([A-Za-z0-9_]+)"', source)))
for key in used:
    for name, text in (("en", en), ("es", es), ("zh", zh)):
        if not has_key(text, key):
            bad.append(f"{name} misses data-t key {key}")

static_es, static_zh = object_block("es"), object_block("zh")
for name, text in (("STATIC.es", static_es), ("STATIC.zh", static_zh)):
    if not text:
        bad.append(f"{name} is missing")
        continue
    for phrase in ("THEME: GAMMA", "WEBMIDI FIRMWARE UPLOADER (M-VAVE FM-1)", "Portal & Emulator", "Licences"):
        if phrase not in text:
            bad.append(f"{name} misses {phrase!r}")

if 'const LANGS = ["en", "es", "zh"];' not in source:
    bad.append("language order is not EN, ES, ZH")
if 'return "en";' not in source:
    bad.append("English is not the default")
if 'localStorage.setItem(LANG_KEY, lang)' not in source:
    bad.append("language choice is not persisted")
if not re.search(r"function applyLang\(\) \{.*?applyStatic\(\);.*?querySelectorAll\(\"\[data-t\]\"\)", source, re.S):
    bad.append("applyLang does not apply STATIC before data-t")
if 'id="lang"' not in source:
    bad.append("visible language selector is missing")

print(f"installer i18n: {len(used)} data-t keys, {len(static_es.splitlines()) + len(static_zh.splitlines())} STATIC lines")
for item in bad:
    print(f"  FAIL {item}")
if bad:
    print(f"installer i18n: {len(bad)} failure(s)", file=sys.stderr)
    raise SystemExit(1)
print("installer i18n: EN/ES/ZH keys and selector wiring verified")
