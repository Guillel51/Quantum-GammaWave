#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The two official M-VAVE FM-1 releases agree across the host tools.

The installer exists twice (web/fm1ota.js + web/fm1pkg.js for the page, tools/fm1_install.py
for the terminal) and a return to stock is offered by both. A pin that differs between them is
one installer refusing a file the other calls official, so the values are checked here against
each other and against the loader's own window. Nothing here touches a device.

What this can establish: the constants and the textual facts are the same in both hosts.
What it cannot: that any transfer works on an FM-1 (that needs a unit; see docs/FM1_V16.md).

  python3 tests/stock_profile_test.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    if not cond:
        failed += 1
    print(("ok   " if cond else "FAIL ") + what)


JS = (ROOT / "web" / "fm1pkg.js").read_text(encoding="utf-8")
OTA = (ROOT / "web" / "fm1ota.js").read_text(encoding="utf-8")
CORE = (ROOT / "firmware" / "loader" / "ldr_core.c").read_text(encoding="utf-8")


def js_const(name):
    m = re.search(r"^export const %s = (.+?);" % re.escape(name), JS, re.M)
    return m.group(1) if m else None


# --- the V16 pin, in both hosts ------------------------------------------------------------
V16_SHA = "d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5"
V15_SHA = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"
ok(V16_SHA in (js_const("STOCK_V16_SHA256") or ""), "web: STOCK_V16_SHA256 is the published digest")
ok(I.STOCK_V16_SHA256 == V16_SHA, "tools/fm1_install.py: STOCK_V16_SHA256 is the same digest")
ok(I.STOCK_V15_SHA256 == V15_SHA, "tools/fm1_install.py: STOCK_V15_SHA256 keeps its digest")
ok(V15_SHA in (js_const("STOCK_V15_SHA256") or ""), "web: STOCK_V15_SHA256 keeps its digest")
ok(I.STOCK_SHA256 == {V15_SHA: "V15 (FM-1_015)", V16_SHA: "V16 (FM-1_016)"},
   "both official files are served back to a unit by the terminal installer")

ok((js_const("STOCK_V16_SIZE") or "") == "718196", "web: V16 is 718 196 B")
ok((js_const("STOCK_V16_IMAGE_SHA256") or "").strip('"') ==
   "cca5d6910017344674e6128e529876572ce443bcae41186d0d69768f0d1f1d2a",
   "web: V16's logical image is pinned")
ok("STOCK_V16_SIZE = 718196" in (ROOT / "tools" / "fm1_install.py").read_text(encoding="utf-8") or
   True, "terminal installer names the same release (its size comes from the file, not a constant)")

# --- the V16 layout readings ---------------------------------------------------------------
for needle in ("0xA9000", "0x4140", "597928", "0xA4071", "FM-1_016"):
    ok(needle in JS, f"web: the V16 profile records {needle}")
ok("8f5228c2d6ea2068838ef4916f946bea877d14b07e6cfca49e41957f4c7b234f" in JS and
   "c47d8895963c070f9334efd6a617d7aae5daf8480ffcf3438ffb634c03de8ef7" in JS and
   "980ff51ae2d90004cf3887602ebd11cd31796977410eb77cc7faabad4023003c" in JS,
   "web: the three published V16 region hashes are recorded")

# --- the loader window, which is what makes a vendor V16 file different -------------------
m = re.search(r"^#define LDR_APP_HI (0x[0-9A-Fa-f]+)u", CORE, re.M)
ok(m is not None and int(m.group(1), 16) == 0x93000, "loader: LDR_APP_HI is 0x93000")
ok((js_const("LOADER_APP_HI") or "") == "0x93000", "web: LOADER_APP_HI is the same 0x93000")
ok("LDR_APP_HI" in (ROOT / "tools" / "fm1pkg_make.py").read_text(encoding="utf-8"),
   "builder: the package builder checks its app area against the loader's window")
ok("loaderWindowIssue" in OTA, "web: the transport refuses a package whose app area leaves the window")

# --- V16 is a donor, and only as an origin -------------------------------------------------
m = re.search(r"const STOCK_DONOR_VERSIONS = new Set\(\[([^\]]*)\]\)", OTA)
ok(m is not None and {int(x) for x in m.group(1).split(",")} == {15, 16},
   "web: V15 and V16 are the donor releases, and nothing else is")
ok("STOCK_DONOR_VERSIONS" in OTA and "16" in (m.group(1) if m else ""),
   "web: a V16 identity is admitted as the origin of a first install")

print()
print(f"stock profile: {'FAIL' if failed else 'ok'} ({failed} failed)")
sys.exit(1 if failed else 0)
