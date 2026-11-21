#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The served package is named after the package's version, not the label the site displays.

The published root answered 404 for `firmware/felucca-1.6.0.fwsc` while the installer offered
`firmware/felucca-v1.6.0-11b9b9e.fwsc`: `web/make_site.py` put its display label into the filename
and `tools/make_release.py` wrote and checked the version name. These cases pin the rule, so the
404 cannot come back through a label.
"""
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("make_site", ROOT / "web" / "make_site.py")
make_site = importlib.util.module_from_spec(spec)
spec.loader.exec_module(make_site)

bad = 0


def check(what, got, want):
    global bad
    ok = got == want
    print("site-name: %-64s %s (%r)" % (what, "ok" if ok else "FAIL", got))
    bad += not ok


# the Pages call today: label "v1.6.0-11b9b9e", the release's own record says "1.6.0"
check("the release label does not reach the file name",
      make_site.package_filename("v1.6.0-11b9b9e", {"version": "v1.6.0"}, {"version": "1.6.0"}),
      "felucca-1.6.0.fwsc")
check("when the label IS the version there is only one name (no copy)",
      make_site.label_filename("1.6.0"), make_site.package_filename("1.6.0", None, None))
check("without a manifest the build record names it",
      make_site.package_filename("v1.6.0-11b9b9e", {"version": "v1.6.0"}, None), "felucca-1.6.0.fwsc")
check("with neither, the label is the last resort (still without its 'v')",
      make_site.package_filename("v1.6.0-11b9b9e", None, None), "felucca-1.6.0-11b9b9e.fwsc")
check("the alias keeps the 'v' the live URL had",
      make_site.label_filename("v1.6.0-11b9b9e"), "felucca-v1.6.0-11b9b9e.fwsc")
check("the previous site's name is what the alias keeps",
      make_site.label_filename("v1.6.0-11b9b9e"), "felucca-v1.6.0-11b9b9e.fwsc")
check("a version already without 'v' is not changed",
      make_site.package_filename("1.6.0", None, None), "felucca-1.6.0.fwsc")
check("no version can escape firmware/ in the name",
      make_site.package_filename("../x y", None, None), "felucca-..-x-y.fwsc")

print("site-name: %d failed" % bad)
sys.exit(1 if bad else 0)
