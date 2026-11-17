#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""`docs/EVIDENCE.md`: what this tree has measured, and what it has not -- generated, never typed.

    python3 tools/evidence_index.py            # rewrite docs/EVIDENCE.md
    python3 tools/evidence_index.py --check    # exit 1 when the file is stale

`docs/VALIDATION_MATRIX.md` is the record (2 000 lines, one row per requirement, each with the class of its
evidence), and it is the file the installation gate reads. It also drifts: three rows still said `PENDING`
for work the suite had been measuring for releases -- the LATCH audio path, the WASM numbers and the CI
attestation -- because a row is only as current as the last person who remembered to edit it.

So the summary is derived from the matrix rather than maintained beside it: `--check` runs in the host suite,
and a row that says PENDING (or NO-GO, or FAIL) appears here with its line number on the next push. It does
not decide what the rows mean; it counts them and points at them, which is the part a person forgets.
"""
import argparse
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MATRIX = os.path.join(ROOT, "docs", "VALIDATION_MATRIX.md")
OUT = os.path.join(ROOT, "docs", "EVIDENCE.md")

# a row is unmeasured when its state cell says so; the words are the ones the matrix itself uses
UNMEASURED = re.compile(r"\b(PENDING|NOT RUN|NOT DONE|NO-GO|STILL PENDING|FAIL|FAILED|NOT EXECUTED)\b", re.I)
# the classes of evidence AGENTS.md asks every claim to name
CLASSES = ("AC791N", "host", "emul", "WASM", "render", "browser", "physical", "HW", "workerd")


def rows():
    """[(line, cells)] for every table row of the matrix, keeping the line so the index can point at it."""
    out = []
    with open(MATRIX, encoding="utf-8") as f:
        for no, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.startswith("| ") or line.startswith("| Requirement") or set(line) <= set("|-: "):
                continue
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) < 2 or not cells[0]:
                continue
            out.append((no, cells))
    return out


def unmeasured(cells):
    """A row asks for evidence when its state cell says so.

    The matrix is mostly requirement rows (`| Requirement | **STATE** | evidence |`), but it also carries
    dated run logs whose third cell is command output (`0 FAILED`, `4 394 ok, 2 FAILED`). A run log is
    history, not a pending requirement, so it is only counted when the state cell is a verdict -- it starts
    with `**` or an uppercase word -- and not when it is a count of passing checks.
    """
    state = cells[1]
    if re.match(r"^0\s+(FAILED|failed)", state) or state.strip().lower().startswith("0 failed"):
        return False
    if not (state.startswith("**") or re.match(r"^[A-Z][A-Z0-9 ,:/()\[\].-]{1,}", state)):
        return False
    return bool(UNMEASURED.search(state))


def render():
    table = rows()
    todo = [(no, c) for no, c in table if unmeasured(c)]
    classes = collections.Counter()
    for _, c in table:
        text = " ".join(c[2:])
        for k in CLASSES:
            if re.search(r"\b%s" % re.escape(k), text, re.I):
                classes[k] += 1
    lines = []
    w = lines.append
    w("# What this tree has measured, and what it has not")
    w("")
    w("Generated from [VALIDATION_MATRIX.md](VALIDATION_MATRIX.md) by `tools/evidence_index.py`; do not edit by")
    w("hand. The matrix is the record -- one row per requirement, with the class of its evidence -- and this")
    w("page is the part a person forgets: the rows that are still **not** measured, with the line they are on.")
    w("")
    w("`python3 tools/evidence_index.py --check` runs in the host suite, so a row that goes back to PENDING")
    w("cannot stay invisible: the file is regenerated in the same push.")
    w("")
    w("## Counts")
    w("")
    w("| | |")
    w("|---|---:|")
    w("| rows in the matrix | %d |" % len(table))
    w("| rows whose state is a verdict (GO / NO-GO / PENDING / FAIL) | %d |" % len([1 for _, c in table if c[1].startswith("**") or c[1][:1].isupper()]))
    w("| rows still asking for evidence | %d |" % len(todo))
    w("")
    w("Classes of evidence named across the matrix (a row can name more than one):")
    w("")
    w("| class | rows |")
    w("|---|---:|")
    for k in CLASSES:
        if classes[k]:
            w("| %s | %d |" % (k, classes[k]))
    w("")
    w("## Not measured yet")
    w("")
    if not todo:
        w("Nothing: every row of the matrix carries a measurement of its own class.")
    else:
        w("| line | requirement | state | evidence |")
        w("|---:|---|---|---|")
        for no, c in todo:
            evidence = " ".join(c[2:]).replace("|", "/")
            if len(evidence) > 220:
                evidence = evidence[:217].rstrip() + "…"
            w("| [%d](VALIDATION_MATRIX.md#L%d) | %s | %s | %s |"
              % (no, no, c[0].replace("|", "/"), c[1].replace("|", "/"), evidence))
    w("")
    w("## What this page cannot say")
    w("")
    w("A row is only as current as the last person who edited it, which is why the matrix is checked and this")
    w("index is generated -- but neither can turn a host test into a reading from a unit. Every row that needs")
    w("a physical FM-1 stays `PENDING` here until someone runs it on one, and nothing in this repository")
    w("writes to the owner's unit.")
    w("")
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="fail when docs/EVIDENCE.md is not what the matrix says")
    a = ap.parse_args(argv)
    want = render()
    if a.check:
        try:
            have = open(OUT, encoding="utf-8").read()
        except OSError:
            print("evidence: docs/EVIDENCE.md is missing; run tools/evidence_index.py")
            return 1
        if have != want:
            print("evidence: docs/EVIDENCE.md does not match docs/VALIDATION_MATRIX.md; "
                  "run tools/evidence_index.py")
            return 1
        print("evidence: docs/EVIDENCE.md matches the matrix (%d row(s) still asking for evidence)"
              % len([1 for _, c in rows() if unmeasured(c)]))
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(want)
    print("evidence: docs/EVIDENCE.md (%d row(s) in the matrix, %d still asking for evidence)"
          % (len(rows()), len([1 for _, c in rows() if unmeasured(c)])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
