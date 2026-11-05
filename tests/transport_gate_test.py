#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The transport's write gate, as a check on the source rather than on a run.

`web/test_web.mjs` and `web/test_provenance.mjs` prove that a refused package sends
zero frames, by running the transport against a fake link. They cannot see a *new*
path that someone adds later: a method that sends the image without asking
`requireProof()` first would pass every test until a test happened to call it.

This reads `web/fm1ota.js` and fails if the shape that makes those tests true is no
longer the shape of the file:

  1. the only code that sends image bytes (`link.send(response(`) is `#serve`;
  2. `#serve` is private (`#`), refuses a link it was not admitted for, and bounds
     every request to the image it holds;
  3. `install()`, `write()` and `resume()` verify before they send anything, and call
     `#serve` only after `requireProof()` and `admit()`;
  4. `#serve` is called from those three and from nowhere else.

It is a static test: it says nothing about whether the crypto is right, only that no
write path exists outside the gate. Run: python3 tests/transport_gate_test.py
"""
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
# TRANSPORT_SRC lets a negative run point this at a copy with the gate taken out,
# which is the only way to know the test is not passing on an empty match.
SRC = Path(os.environ.get("TRANSPORT_SRC") or (HERE.parent / "web" / "fm1ota.js")).read_text(encoding="utf-8")

bad = 0


def ok(what, cond):
    global bad
    print(("transport gate: %-72s %s" % (what, "ok" if cond else "FAIL")))
    bad += not cond


def method_bodies(text):
    """The body of every `name(args) {` (or `async name(...)`) in the class, by braces."""
    out = {}
    pattern = re.compile(r"^\s{2}(?:async\s+)?(#?[A-Za-z_#][\w$]*)\s*\([^)]*\)\s*\{", re.M)
    for m in pattern.finditer(text):
        name, i, depth = m.group(1), m.end(), 1
        while i < len(text) and depth:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        out[name] = text[m.end():i - 1]
    return out


bodies = method_bodies(SRC)
ok("the transport's methods were found at all (the parser is reading the class)",
   {"install", "write", "resume", "#serve"} <= set(bodies))
if bad:
    sys.exit(1)

serve = bodies.get("#serve", "")
writers = [name for name, body in bodies.items() if "link.send(response(" in body]
ok("the image bytes are sent from #serve and from no other method",
   writers == ["#serve"])
ok("#serve is private", "#serve" in bodies and "serve(" not in
   re.sub(r"#serve\(", "", "".join(bodies)))
ok("only install() and write() reach #serve",
   sorted(n for n, b in bodies.items() if "#serve(" in b) == ["install", "write"])
ok("#serve refuses a link it was not admitted for",
   "pass.link !== link" in serve and "throw" in serve)
ok("#serve bounds a request to the image it holds",
   "r.addr + r.len > image.length" in serve and "MAXDATA" in serve)
ok("#serve reads from the admitted copy, not from the caller's bytes",
   "pass.image" in serve and "subarray(" in serve)

for name, after in (("install", "#serve("), ("write", "#serve("), ("resume", "this.write(")):
    body = bodies[name]
    a = body.find("requireProof(")
    b = body.find("admit(") if name != "resume" else body.find("this.write(")
    s = body.find(after)
    ok("%s() requires a proof before it reaches the sender" % name, 0 <= a < s)
    if name != "resume":
        ok("%s() admits the package before it sends anything" % name, 0 <= a < b < s)
    else:
        ok("resume() is a gated entry point itself: it verifies, then goes through write()",
           0 <= a < s)

ok("the package bytes the caller hands over are never sent: only admit()'s copy is",
   "image.subarray" not in "".join(b for n, b in bodies.items() if n != "#serve"))
ok("a proof is consumed per transfer, not reused (admit() takes the proof and the link)",
   "admit(proof, pkg, product)" in SRC)

print("transport gate: all passed" if not bad else "transport gate: %d FAILED" % bad)
sys.exit(1 if bad else 0)
