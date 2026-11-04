#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Two-pass, read-only backup of an FM-1's NOR over USB-MIDI.

Everything here is read-only: the only frames sent are 0x11 (handshake), 0x12
(identification) and 0x13 (read). No 0x30, no UPGRADE frame, no erase, no
program, no status-register write. `tests/backup_tool_test.py` asserts exactly
that against a simulated FM-1.

  fm1_backup.py --out DIR [--port NAME] [--chunk 256] [--max-errors 8]

Writes DIR/<identity>-<date>.bin (pass A; pass B too when the passes differ)
and DIR/<identity>-<date>.manifest.json. The manifest is only "complete" when
every byte of the part was read twice, the two passes are byte-identical and
the sizes match: a partial or inconsistent backup says so, and keeps the
ranges it could not read.

A backup is not a recovery path: restoring it needs the loader, which this
tool does not touch, and no restore has been verified on a bench.
"""
import argparse
import hashlib
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import fm1_install as I
import fm1_probe as P

TOOL = "fm1_backup 1.0"
FORMAT = "qg1-backup-1"
SENT = {0x11, 0x12, 0x13}          # the only commands this tool may send


class BackupError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


class Watch:
    """counts every frame we send, so the test (and the manifest) can prove read-only"""

    def __init__(self, link):
        self.link = link
        self.frames = []
        send = link.send

        def guarded(pkt):
            self.frames.append(bytes(pkt))
            return send(pkt)

        link.send = guarded

    def commands(self):
        out = []
        for f in self.frames:
            try:
                cmd, _ = P.decode(f)
                out.append(cmd)
            except P.ProbeError:
                out.append(None)
        return out


def _pass(link, ev, chunk, max_errors, progress):
    """one independent read of the whole part -> (bytes, unread ranges, errors)"""
    cap = ev["cap"]
    data = bytearray()
    unread = []
    errors = []
    off = 0
    while off < cap:
        n = min(chunk, cap - off)
        got, errs = P.read_range(link, off, n, ev["session"], chunk=chunk)
        if errs:
            errors.extend(errs)
            unread.append([off, off + n])
            if len(errors) >= max_errors:
                unread.append([off + n, cap])
                break
            data += bytes(n)              # keep the offsets aligned; the range is marked unread
            off += n
            continue
        data += got
        off += n
        if progress:
            progress(off, cap)
    return bytes(data), unread, errors


def backup(link, out_dir, chunk=P.CHUNK, max_errors=8, progress=None):
    ev = P.probe(link)
    watch = Watch(link)                # from here on, every frame is recorded
    t0 = time.time()
    a, unread_a, err_a = _pass(link, ev, chunk, max_errors, progress)
    b, unread_b, err_b = _pass(link, ev, chunk, max_errors, progress)
    elapsed = time.time() - t0

    sha_a = hashlib.sha256(a).hexdigest() if a else ""
    sha_b = hashlib.sha256(b).hexdigest() if b else ""
    unread = sorted({tuple(r) for r in unread_a + unread_b})
    same = bool(a) and a == b and not unread
    if unread:
        status = "partial"
    elif a != b:
        status = "inconsistent"
    elif not a:
        status = "unverifiable"
    else:
        status = "complete"

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = "%s-%s" % (ev.get("identity", "FM-1").replace("/", "_"), stamp)
    bin_a = out_dir / (stem + ".bin")
    bin_b = out_dir / (stem + ".passB.bin")
    if a:
        bin_a.write_bytes(a)
    if a != b and b:
        bin_b.write_bytes(b)

    manifest = {
        "format": FORMAT,
        "tool": TOOL,
        "created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "device": {
            "identity": ev.get("identity", ""),
            "product": ev.get("product", ""),
            "firmware": ev.get("firmware", ""),
            "jedec": "0x%06X" % ev["jedec"],
            "part": ev["part"],
            "capacity": ev["cap"],
            "page": ev["page"],
            "sector": ev["sector"],
            "block": ev["block"],
            "identification": "0x12, two readings, session %08X" % ev["session"],
            "nonce": ev.get("nonce", ""),
        },
        "passes": 2,
        "chunk": chunk,
        "size": len(a),
        "expected_size": ev["cap"],
        "sha256_pass_a": sha_a,
        "sha256_pass_b": sha_b,
        "sha256": sha_a if same else "",
        "identical": bool(a) and a == b,
        "status": status,
        "unread_ranges": unread,
        "errors": [{"addr": e[0], "code": e[1], "message": e[2]} for e in (err_a + err_b)],
        "commands_sent": sorted({c for c in watch.commands() if c is not None}),
        "files": {"pass_a": bin_a.name if a else None, "pass_b": bin_b.name if bin_b.exists() else None},
        "seconds": round(elapsed, 1),
        "restore": "not implemented and not verified: a backup is not a recovery path",
    }
    if any(c not in SENT for c in manifest["commands_sent"]):
        raise BackupError("write", "a non read-only frame was sent: %s" % manifest["commands_sent"])
    man_path = out_dir / (stem + ".manifest.json")
    man_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    if status == "complete":
        (out_dir / (stem + ".sha256")).write_text("%s  %s\n" % (sha_a, bin_a.name))
    return manifest, man_path, bin_a


def run(a, backend=None):
    backend = backend or I.MidiBackend()
    up = I.Updater(backend, a.port)
    dev = up.find(None)
    if not dev:
        raise BackupError("notfound", "no FM-1 answered on the MIDI ports")
    if I.model_of(dev.id.text) != "FM-1":
        raise BackupError("model", "%r is not an FM-1 (%r)" % (dev.name, dev.id.text))
    if not a.yes and sys.stdin.isatty():
        print("read-only backup of %s (%s). Nothing is written to the FM-1." % (dev.name, dev.id.text))
        if input("continue? [y/N] ").strip().lower() not in ("y", "yes"):
            raise BackupError("cancelled", "cancelled")
    link = dev.link
    ev_identity = {"identity": dev.id.text, "product": "FM-1", "firmware": dev.id.version}

    def progress(off, cap):
        if sys.stderr.isatty():
            sys.stderr.write("\r  %d/%d KiB" % (off // 1024, cap // 1024))
            sys.stderr.flush()

    # probe() builds its own evidence; keep the identity next to it for the manifest
    real_probe = P.probe

    def with_identity(link_, *args, **kw):
        ev = real_probe(link_, *args, **kw)
        ev.update(ev_identity)
        return ev

    P.probe = with_identity
    try:
        manifest, path, bin_path = backup(link, a.out, a.chunk, a.max_errors, progress)
    finally:
        P.probe = real_probe
        link.close()
    print("\n%s: %s" % (manifest["status"], path))
    if manifest["status"] == "complete":
        print("  %s  %s" % (manifest["sha256"], bin_path))
        return 0
    print("  unread: %s" % (manifest["unread_ranges"] or "none"))
    print("  a %s backup is not a verified backup" % manifest["status"], file=sys.stderr)
    return 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="read-only, two-pass backup of an FM-1")
    ap.add_argument("--out", default="backup", help="output directory")
    ap.add_argument("--port")
    ap.add_argument("--chunk", type=int, default=P.CHUNK)
    ap.add_argument("--max-errors", type=int, default=8)
    ap.add_argument("--yes", action="store_true")
    a = ap.parse_args(argv)
    if not 1 <= a.chunk <= P.CHUNK:
        print("--chunk must be 1..%d (the device answers at most that much)" % P.CHUNK, file=sys.stderr)
        return 2
    try:
        return run(a)
    except BackupError as e:
        print("refused: %s" % e, file=sys.stderr)
        return 1
    except P.ProbeError as e:
        print("identification failed: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
