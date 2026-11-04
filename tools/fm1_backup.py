#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Verified musical archive of an FM-1 (backup / verify / restore), from the command line.

This is the command-line counterpart of web/fm1backup.js and speaks the same protocol
(SysEx F0 7D 46 4C cmd args F7, commands LIST / GET / PUT over the object ids 0..10 and
40..71). It is a *musical* archive: the device exports named objects (current music,
settings, projects, the song index and its sections), not raw flash. Nothing here can read
or write the bootloader, the ROM, the app area or the flash head, and nothing claims to:
a raw-flash dump needs the USB_KEY / Mask-ROM path in docs/RECOVERY_GUIDE.md, not this tool.

What the tool guarantees, and why each step exists:

  * two independent passes (a full LIST + GET of every object each time), compared byte for
    byte; a device whose content moves under the reader (a running sequencer, a settings
    write) is caught instead of being archived half-old half-new;
  * a SHA-256 per read, per object and of the final file, so a later restore can tell that
    the bytes are the ones this run wrote, and not just "a JSON that parses";
  * a manifest with the device identity, date, method, object capacity, tool version and the
    result, so an archive can never be mistaken for something it is not;
  * an explicit classification: complete / partial / inconsistent / unverifiable;
  * restore validates the manifest, the file hash, every object hash and CRC, the device
    model and the object geometry BEFORE the first write, writes chunk by chunk, reads every
    object back and compares it, and reports a selective restore as a partial restore;
  * private archives live outside the repository by default (a backup holds the user's own
    music; it must never be committed).

Exit codes: 0 ok, 1 cancelled or other error, 2 bad arguments or archive, 3 FM-1 not found,
4 connection lost, 5 inconsistent or unverifiable archive, 6 device/geometry mismatch.
"""
import argparse
import base64
import hashlib
import json
import os
import re
import sys
import time
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fm1_install import HS_QUERY, MidoBackend, PORT_RE, pair_output, parse_identity  # noqa: E402

TOOL = "qg1-fm1-backup"
TOOL_VERSION = "1.0.0"
FORMAT, FORMAT_VERSION = "felucca-backup", 1

LIST, GET, PUT = 65, 66, 67
CHUNK = 256
HEADER = (0xF0, 0x7D, 0x46, 0x4C)

# The object set of a current firmware (same list as web/fm1backup.js): 0 current music,
# 1 settings, 2..9 projects and banks, 10 the song index, 40..71 the song sections.
IDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10] + list(range(40, 72))
REQUIRED = (0, 1)          # current music and settings: without them an archive is partial
MAX_OBJECT = 3840

# The device's own answer codes (web/fm1backup.js bkCheck).
RC = {1: "Invalid archive object", 2: "Archive data failed validation", 3: "Stop playback first",
      4: "Flash write failed", 5: "Start a fresh backup"}

DEFAULT_DIR = Path.home() / ".local" / "share" / "qg1-backups"

EXIT = {"usage": 2, "badpkg": 2, "notfound": 3, "lost": 4, "inconsistent": 5, "unverifiable": 5,
        "mismatch": 6}


class BackupError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


# ------------------------------------------------------------------ time ---

class Watch:
    """Monotonic clock for every timeout in this tool.

    It is created before the device is opened and before any log line is written: a timeout
    that is measured from a watch started later silently loses the connection and handshake
    time, which is exactly the window where a USB device appears. One instance is passed
    down to the link and to every request.
    """

    def __init__(self):
        self.t0 = time.monotonic()
        self.mark = self.t0

    def reset(self):
        self.mark = time.monotonic()

    def elapsed(self):
        return time.monotonic() - self.mark

    def total(self):
        return time.monotonic() - self.t0

    def left(self, budget):
        return max(0.0, budget - self.elapsed())


class Log:
    """Long-format, timestamped log; quiet unless --verbose, always safe to redirect."""

    def __init__(self, watch, stream=sys.stderr, verbose=False):
        self.watch, self.stream, self.verbose = watch, stream, verbose

    def line(self, msg):
        print(f"[{self.watch.total():8.3f}s] {msg}", file=self.stream, flush=True)

    def step(self, msg):
        self.line(msg)

    def debug(self, msg):
        if self.verbose:
            self.line(msg)


# ------------------------------------------------------------- wire format ---

def u32(n):
    return [(n >> (i * 7)) & (15 if i == 4 else 127) for i in range(5)]


def r32(a, off=0):
    if len(a) < off + 5 or a[off + 4] > 15:
        raise BackupError("badpkg", "invalid archive number in a device reply")
    return (a[off] | a[off + 1] << 7 | a[off + 2] << 14 | a[off + 3] << 21 | a[off + 4] << 28) & 0xFFFFFFFF


def pack(bytes_):
    a = []
    for off in range(0, len(bytes_), 7):
        chunk = bytes_[off:off + 7]
        mask = 0
        for i, b in enumerate(chunk):
            mask |= (b >> 7) << i
        a.append(mask)
        a.extend(b & 127 for b in chunk)
    return a


def unpack(a, size):
    out, i, j = bytearray(), 0, 0
    while j < size:
        n = min(7, size - j)
        if i >= len(a):
            raise BackupError("badpkg", "short archive chunk")
        mask = a[i]
        i += 1
        if mask >> n:
            raise BackupError("badpkg", "invalid archive bytes")
        for k in range(n):
            if i >= len(a) or a[i] > 127:
                raise BackupError("badpkg", "short archive chunk")
            out.append(a[i] | ((mask >> k) & 1) << 7)
            i += 1
            j += 1
    if i != len(a):
        raise BackupError("badpkg", "trailing archive bytes")
    return bytes(out)


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


class Link:
    """One MIDI in/out pair that answers the archive protocol, with a bounded request."""

    def __init__(self, backend, in_name, out_name, watch, log):
        import mido
        self.name, self.mido, self.watch, self.log = in_name, mido, watch, log
        self.backend = backend
        self.q = []
        self.pending = None
        self.inp = mido.open_input(in_name, callback=self._rx)
        try:
            self.out = mido.open_output(out_name)
        except Exception:
            self.inp.close()
            raise
        time.sleep(0.3)          # some MIDI stacks drop a message sent right after opening

    def _rx(self, msg):
        if msg.type == "sysex":
            d = [0xF0, *msg.data, 0xF7]
            if len(d) >= 4 and tuple(d[:4]) == HEADER:
                self.q.append(d)

    @property
    def lost(self):
        return self.name not in self.backend.input_names()

    def send(self, pkt):
        try:
            self.out.send(self.mido.Message("sysex", data=pkt[1:-1]))
            return True
        except Exception:
            return False

    def close(self):
        for p in (self.inp, self.out):
            try:
                p.close()
            except Exception:
                pass

    def drain(self):
        self.q.clear()

    def read_reply(self, cmd, timeout):
        end = time.monotonic() + timeout
        while True:
            for i, d in enumerate(self.q):
                if d[4] == cmd:
                    del self.q[i]
                    return d[5:-1]
            left = end - time.monotonic()
            if left <= 0:
                return None
            time.sleep(min(0.005, left))


class Device:
    """The FM-1 as the archive protocol sees it."""

    def __init__(self, link, ident, name, watch, log):
        self.link, self.id, self.name, self.watch, self.log = link, ident, name, watch, log

    # one request/reply pair, retried once; None when the device went away
    def request(self, cmd, args, timeout=1.2, retries=1):
        pkt = bytes([*HEADER, cmd, *args, 0xF7])
        for _ in range(retries + 1):
            self.link.drain()
            if not self.link.send(pkt):
                if self.link.lost:
                    raise BackupError("lost", "the FM-1 was disconnected")
                return None
            r = self.link.read_reply(cmd, timeout)
            if r is not None:
                return r
            if self.link.lost:
                raise BackupError("lost", "the FM-1 was disconnected")
        return None

    def manifest(self):
        a = self.request(LIST, [], timeout=3.0, retries=0)
        if a is None:
            raise BackupError("lost", "the FM-1 did not answer the archive list")
        if a[0] != 1:
            raise BackupError("inconsistent", "unsupported archive protocol")
        if a[1]:
            raise BackupError("inconsistent", f"the FM-1 refused the archive list: {RC.get(a[1], a[1])}")
        if len(a) != 3 + a[2] * 11:
            raise BackupError("inconsistent", "incomplete archive manifest")
        objs = []
        for i in range(a[2]):
            p = 3 + i * 11
            objs.append({"id": a[p], "size": r32(a, p + 1), "crc": r32(a, p + 6)})
        return objs

    def get(self, obj, off, size):
        a = self.request(GET, [obj["id"], *u32(off), size & 127, size >> 7])
        if a is None:
            raise BackupError("lost", f"the FM-1 stopped answering while reading object {obj['id']}")
        if len(a) < 9 or a[1]:
            raise BackupError("inconsistent", f"object {obj['id']}: {RC.get(a[1] if len(a) > 1 else 0, 'bad reply')}")
        if a[0] != obj["id"] or r32(a, 2) != off or (a[7] | a[8] << 7) != size:
            raise BackupError("inconsistent", f"object {obj['id']}: unexpected reply")
        return unpack(a[9:], size)

    def read_object(self, obj):
        if obj["size"] > MAX_OBJECT:
            raise BackupError("inconsistent", f"object {obj['id']} is larger than the protocol allows")
        buf = bytearray()
        for off in range(0, obj["size"], CHUNK):
            size = min(CHUNK, obj["size"] - off)
            buf += self.get(obj, off, size)
        data = bytes(buf)
        if crc32(data) != obj["crc"]:
            raise BackupError("inconsistent", f"object {obj['id']} changed while it was read")
        return data

    def put_begin(self, oid, size, crc):
        a = self.request(PUT, [0, oid, *u32(size), *u32(crc)], timeout=2.0, retries=0)
        if a is None:
            raise BackupError("lost", f"the FM-1 did not answer the write of object {oid}")
        return a[2] if len(a) > 2 else 255

    def put_chunk(self, oid, off, data):
        a = self.request(PUT, [1, oid, *u32(off), *pack(data)], timeout=2.0, retries=1)
        if a is None:
            raise BackupError("lost", f"the FM-1 stopped answering while writing object {oid}")
        return a[2] if len(a) > 2 else 255

    def put_end(self, oid):
        a = self.request(PUT, [2, oid], timeout=2.0, retries=0)
        return a[2] if a and len(a) > 2 else 255

    def put_abort(self, oid):
        try:
            self.request(PUT, [3, oid], timeout=1.0, retries=0)
        except BackupError:
            pass

    def write_object(self, obj, data):
        rc = self.put_begin(obj["id"], len(data), crc32(data))
        if rc:
            raise BackupError("inconsistent", f"object {obj['id']}: {RC.get(rc, f'error {rc}')}")
        try:
            for off in range(0, len(data), CHUNK):
                rc = self.put_chunk(obj["id"], off, data[off:off + CHUNK])
                if rc:
                    raise BackupError("inconsistent", f"object {obj['id']}: {RC.get(rc, f'error {rc}')}")
            rc = self.put_end(obj["id"])
            if rc:
                raise BackupError("inconsistent", f"object {obj['id']}: {RC.get(rc, f'error {rc}')}")
        except BaseException:
            self.put_abort(obj["id"])
            raise


# ---------------------------------------------------------------- connect ---

def connect(backend, port, watch, log):
    outs = list(dict.fromkeys(backend.output_names()))
    names = [n for n in dict.fromkeys(backend.input_names()) if PORT_RE.search(n)]
    if port:
        names = [n for n in names if port.lower() in n.lower()]
    for name in names:
        out = pair_output(name, outs)
        if not out:
            continue
        try:
            link = (backend.open_backup(name, out, watch, log) if hasattr(backend, "open_backup")
                    else Link(backend, name, out, watch, log))
        except Exception:
            continue
        try:
            link.drain()
            for _ in range(2):
                if not link.send(HS_QUERY):
                    break
                end = time.monotonic() + 1.0
                while time.monotonic() < end:
                    ident = next((parse_identity(bytes(d)) for d in link.q), None)
                    if ident:
                        log.step(f"device: {ident.text} on {name}")
                        return Device(link, ident, name, watch, log)
                    time.sleep(0.02)
        except Exception:
            pass
        link.close()
    raise BackupError("notfound", "FM-1 not found (USB data cable? another app using it?)"
                                 + (f"\n  MIDI inputs: {', '.join(names) or '(none)'}"))


# ---------------------------------------------------------------- archive ---

def read_pass(dev, log, label):
    """one complete, independent read of every object: LIST + GET, CRC checked per object"""
    manifest = dev.manifest()
    log.step(f"{label}: {len(manifest)} objects, {sum(o['size'] for o in manifest)} B")
    out = []
    for o in manifest:
        data = dev.read_object(o)
        out.append({**o, "data": data})
        log.debug(f"  {label}: object {o['id']:>2} {o['size']:>5} B crc {o['crc']:08x}")
    return out


def required_ok(objects):
    return all(any(o["id"] == i for o in objects) for i in REQUIRED)


def compare(read_a, read_b):
    """[(id, what)]: the objects that are not byte-identical between the two passes"""
    a = {o["id"]: o for o in read_a}
    b = {o["id"]: o for o in read_b}
    bad = []
    for i in sorted(set(a) | set(b)):
        oa, ob = a.get(i), b.get(i)
        if oa is None or ob is None:
            bad.append((i, "present in one pass only"))
        elif oa["size"] != ob["size"] or oa["crc"] != ob["crc"] or oa["data"] != ob["data"]:
            bad.append((i, "changed between the two passes"))
    return bad


def pass_digest(read):
    h = hashlib.sha256()
    for o in sorted(read, key=lambda o: o["id"]):
        h.update(bytes([o["id"]]))
        h.update(o["size"].to_bytes(4, "little"))
        h.update(o["crc"].to_bytes(4, "little"))
        h.update(hashlib.sha256(o["data"]).digest())
    return h.hexdigest()


def archive_digest(doc):
    """SHA-256 over the archive's identity and every object's bytes (the file's own hash)"""
    h = hashlib.sha256()
    h.update(json.dumps({"format": doc["format"], "version": doc["version"], "firmware": doc["firmware"]},
                        sort_keys=True).encode())
    for o in sorted(doc["objects"], key=lambda o: o["id"]):
        h.update(bytes([o["id"]]))
        h.update(o["size"].to_bytes(4, "little"))
        h.update(o["crc"].to_bytes(4, "little"))
        h.update(o["sha256"].encode())
    return h.hexdigest()


def inside_repo(path):
    p = Path(path).resolve()
    for up in [p, *p.parents]:
        if (up / ".git").exists():
            return True
    return False


def dest_path(a, identity):
    """where the archive goes; the repository check runs before the device is even opened"""
    out = Path(a.out) if a.out else DEFAULT_DIR
    return out if out.suffix.lower() == ".json" else out / f"qg1-{identity}-{time.strftime('%Y%m%d-%H%M%S')}.json"


def check_private(a, path):
    if not a.allow_repo and inside_repo(path):
        raise BackupError("usage", f"{path} is inside a git repository: a backup holds your own music and must "
                                   "not be committed (use --allow-repo to override)")


def cmd_backup(a, backend, out, log, watch):
    check_private(a, dest_path(a, "FM-1_000"))     # refuse an unprivate destination before touching the FM-1
    dev = connect(backend, a.port, watch, log)
    try:
        if dev.id.loader:
            raise BackupError("inconsistent", "the FM-1 is in update mode: finish or cancel the update first")
        read_a = read_pass(dev, log, "pass 1")
        read_b = read_pass(dev, log, "pass 2")
        if (a.passes or 2) > 2:
            for n in range(3, (a.passes or 2) + 1):
                more = read_pass(dev, log, f"pass {n}")
                if compare(read_b, more):
                    read_b = more
                    break
    finally:
        dev.link.close()

    bad = compare(read_a, read_b)
    if bad:
        for i, why in bad:
            log.step(f"object {i}: {why}")
        raise BackupError("inconsistent", "the FM-1's content changed between two independent reads; "
                                          "stop playback and keep the device idle, then try again")

    objects = [{"id": o["id"], "size": o["size"], "crc": o["crc"],
                "sha256": hashlib.sha256(o["data"]).hexdigest(),
                "data": base64.b64encode(o["data"]).decode()} for o in read_a]
    doc = {"format": FORMAT, "version": FORMAT_VERSION, "firmware": dev.id.text,
           "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "objects": objects}
    doc["qg1"] = {
        "tool": TOOL, "tool_version": TOOL_VERSION, "method": "M-UPGRADE archive protocol (LIST/GET/PUT)",
        "device": {"identity": dev.id.text, "model": dev.id.model, "version": dev.id.version,
                   "port": dev.name},
        "capacity": {"objects": len(objects), "bytes": sum(o["size"] for o in objects),
                     "max_object": MAX_OBJECT, "ids": [o["id"] for o in objects]},
        "passes": 2,
        "pass_sha256": [pass_digest(read_a), pass_digest(read_b)],
        "object_sha256": {str(o["id"]): o["sha256"] for o in objects},
        "file_sha256": archive_digest(doc),
        "result": "complete" if required_ok(read_a) else "partial",
        "verified": "two independent reads, byte for byte, every object CRC and SHA-256",
        "no_raw_flash": "musical objects only: firmware, bootloader, ROM and the flash head are not readable here",
        "hardware_test": "none: no physical FM-1 was used to produce this file",
    }
    path = dest_path(a, dev.id.text)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(doc, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    os.chmod(tmp, 0o600)
    tmp.replace(path)
    log.step(f"archive: {path} ({path.stat().st_size} B, sha256 {doc['qg1']['file_sha256'][:16]}…)")
    print(f"backup: {doc['qg1']['result']}  {len(objects)} objects  {doc['qg1']['capacity']['bytes']} B  "
          f"{dev.id.text}\nfile:   {path}\nsha256: {doc['qg1']['file_sha256']}", file=out)
    return 0


# ----------------------------------------------------------------- verify ---

def load_archive(path):
    try:
        doc = json.loads(Path(path).read_text(encoding="utf-8"))
    except OSError as e:
        raise BackupError("badpkg", f"cannot read {path}: {e.strerror}")
    except ValueError as e:
        raise BackupError("badpkg", f"{path}: not JSON ({e})")
    if doc.get("format") != FORMAT or doc.get("version") != FORMAT_VERSION:
        raise BackupError("badpkg", f"{path}: not a Felucca/Quantum archive")
    q = doc.get("qg1") or {}
    if q.get("tool") != TOOL:
        raise BackupError("unverifiable", f"{path}: no {TOOL} manifest: nothing to verify it against")
    if archive_digest(doc) != q.get("file_sha256"):
        raise BackupError("badpkg", f"{path}: the file's SHA-256 does not match its manifest (altered or damaged)")
    objs = doc.get("objects")
    if not isinstance(objs, list) or not objs:
        raise BackupError("badpkg", f"{path}: no objects")
    seen, total = set(), 0
    for o in objs:
        if not isinstance(o, dict) or not isinstance(o.get("id"), int) or o["id"] in seen:
            raise BackupError("badpkg", f"{path}: bad object entry")
        seen.add(o["id"])
        if not isinstance(o.get("size"), int) or not 0 <= o["size"] <= MAX_OBJECT:
            raise BackupError("badpkg", f"{path}: object {o['id']} has an impossible size")
        try:
            data = base64.b64decode(o.get("data", ""), validate=True)
        except Exception:
            raise BackupError("badpkg", f"{path}: object {o['id']} is not base64")
        if len(data) != o["size"] or crc32(data) != o["crc"]:
            raise BackupError("badpkg", f"{path}: object {o['id']} fails its CRC")
        if hashlib.sha256(data).hexdigest() != o.get("sha256"):
            raise BackupError("badpkg", f"{path}: object {o['id']} fails its SHA-256")
        o["bytes"] = data
        total += o["size"]
    if (q.get("object_sha256") or {}) and q["object_sha256"] != {str(o["id"]): o["sha256"] for o in objs}:
        raise BackupError("badpkg", f"{path}: the manifest's object hashes do not match the archive")
    doc["qg1"]["capacity"]["bytes"] = total
    return doc


def cmd_verify(a, out, log):
    doc = load_archive(a.archive)
    q = doc["qg1"]
    result = q.get("result", "unverifiable")
    print(f"archive: {a.archive}\nresult:  {result}  ({len(doc['objects'])} objects, "
          f"{q['capacity']['bytes']} B)\ndevice:  {q['device']['identity']}  created {doc['created']}\n"
          f"sha256:  {q['file_sha256']}", file=out)
    if result == "inconsistent":
        raise BackupError("unverifiable", "the archive says its two reads disagreed")
    return 0


# ---------------------------------------------------------------- restore ---

def cmd_restore(a, backend, out, log, watch):
    doc = load_archive(a.archive)
    q = doc["qg1"]
    result = q.get("result")
    if result != "complete" and not a.force:
        raise BackupError("unverifiable", f"the archive is {result}: restoring it would write an incomplete set "
                                          "(--force overrides, and the device is left with a partial restore)")
    objs = {o["id"]: o for o in doc["objects"]}
    only = None
    if a.only:
        only = {int(x) for x in re.split(r"[,\s]+", a.only.strip()) if x}
        missing = only - set(objs)
        if missing:
            raise BackupError("usage", f"the archive has no object(s) {sorted(missing)}")
    targets = [objs[i] for i in sorted(only)] if only else [objs[i] for i in sorted(objs)]
    dev = connect(backend, a.port, watch, log)
    try:
        if dev.id.loader:
            raise BackupError("inconsistent", "the FM-1 is in update mode: finish or cancel the update first")
        if dev.id.model != q["device"]["model"]:
            raise BackupError("mismatch", f"the archive is from {q['device']['identity']}, "
                                          f"this device is {dev.id.text}")
        have = {o["id"]: o for o in dev.manifest()}
        unknown = [o["id"] for o in targets if o["id"] not in have]
        if unknown:
            raise BackupError("mismatch", f"this firmware has no archive object(s) {unknown}: "
                                          "install a current firmware, or restore with --only")
        # every byte validated before the first destructive request (load_archive did that);
        # now a last check that the target objects fit the device's own geometry
        for o in targets:
            if have[o["id"]]["size"] and o["size"] > have[o["id"]]["size"]:
                raise BackupError("mismatch", f"object {o['id']} is {o['size']} B in the archive but the "
                                              f"firmware allows {have[o['id']]['size']} B")
        if not a.yes:
            what = "everything" if only is None else f"objects {sorted(only)}"
            print(f"About to write {what} from {a.archive}\n  archive: {q['device']['identity']} "
                  f"({doc['created']}), file sha256 {q['file_sha256'][:16]}…\n  device:  {dev.id.text}\n"
                  "Stop playback first. Do not unplug the FM-1 while writing.", file=sys.stderr)
            try:
                if input("Restore? [y/N] ").strip().lower() not in ("y", "yes"):
                    print("cancelled", file=out)
                    return 1
            except EOFError:
                print("cancelled", file=out)
                return 1
        log.step(f"restoring {len(targets)} object(s) to {dev.id.text}")
        for o in targets:
            dev.write_object(o, o["bytes"])
            back = dev.get(o, 0, o["size"]) if o["size"] else b""
            if back != o["bytes"]:
                raise BackupError("inconsistent", f"object {o['id']} did not read back as written")
            log.step(f"  object {o['id']:>2} {o['size']:>5} B written and read back")
    finally:
        dev.link.close()
    kind = "complete" if only is None else "partial (selective)"
    print(f"restore: {kind}  {len(targets)} objects  {sum(o['size'] for o in targets)} B  {dev.id.text}", file=out)
    if only is not None:
        print("note: a selective restore does not restore the objects you left out; it is not a full recovery.",
              file=out)
    return 0


def cmd_info(a, backend, out, log, watch):
    dev = connect(backend, a.port, watch, log)
    try:
        m = dev.manifest()
        print(f"{dev.id.text}  {dev.name}  {len(m)} objects, {sum(o['size'] for o in m)} B", file=out)
        for o in m:
            print(f"  id {o['id']:>2}  {o['size']:>5} B  crc {o['crc']:08x}", file=out)
        return 0
    finally:
        dev.link.close()


# ------------------------------------------------------------------- CLI ---

def main(argv=None, backend=None, out=sys.stdout, log_stream=sys.stderr):
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    common.add_argument("--out", help=f"archive file or directory (default: {DEFAULT_DIR})")
    common.add_argument("--allow-repo", action="store_true", help="allow writing an archive inside a git repository")
    common.add_argument("--passes", type=int, default=2, help="independent reads to compare (2 or more)")
    common.add_argument("--verbose", action="store_true")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("backup", parents=[common], help="read the FM-1 twice and write a verified archive")
    v = sub.add_parser("verify", parents=[common], help="check an archive's manifest, hashes and CRCs")
    v.add_argument("archive")
    r = sub.add_parser("restore", parents=[common], help="validate and write an archive back")
    r.add_argument("archive")
    r.add_argument("--only", help="restore only these object ids (a partial restore)")
    r.add_argument("--yes", action="store_true")
    r.add_argument("--force", action="store_true", help="write an archive whose result is not complete")
    sub.add_parser("info", parents=[common], help="list the objects the connected FM-1 exposes")
    a = ap.parse_args(argv)
    if a.passes < 2:
        ap.error("--passes must be at least 2: one read cannot be compared")

    watch = Watch()                       # first: every timeout below is measured from here
    log = Log(watch, log_stream, a.verbose)
    log.step(f"{TOOL} {TOOL_VERSION}: {a.cmd}")
    try:
        if a.cmd == "verify":
            return cmd_verify(a, out, log)
        if backend is None:
            try:
                backend = MidoBackend()
            except ImportError:
                raise BackupError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        if a.cmd == "backup":
            return cmd_backup(a, backend, out, log, watch)
        if a.cmd == "restore":
            return cmd_restore(a, backend, out, log, watch)
        return cmd_info(a, backend, out, log, watch)
    except BackupError as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted: if the FM-1 is in update mode, run the install again to finish", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
