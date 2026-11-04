#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""0x12 identification and 0x13 read-only flash reads (host side).

The running app answers 0x12 with the JEDEC id its own SPI driver read, the
geometry that id implies, a status and a session token; 0x13 reads flash and
nothing else (no erase, no program, no status-register write).

Two identifications with the same nonce must agree on the part and geometry,
and each must bring a fresh session token: a replayed frame cannot pass, and
a reply that arrives late belongs to an older token and is refused.

No reply, a truncation, a bad CRC, a status other than 0, a geometry that does
not match the identified part or a token mismatch is an error. Nothing here
invents a value: a JEDEC id typed by a person is never accepted.
"""
import secrets
import struct
import time

PROTO = 1
CMD_IDENT = 0x12
CMD_READ = 0x13
CHUNK = 256            # the device answers at most this many bytes per 0x13
CAP = 0x100000         # one 1 MiB part

# JEDEC -> (part, capacity, page, sector, block). The device checks the same table.
PARTS = {
    0x856014: ("P25Q80H", 0x100000, 256, 0x1000, 0x10000),
    0xB36014: ("P25Q80H-2", 0x100000, 256, 0x1000, 0x10000),
}

STATUS = {
    0: "ok",
    1: "no-nor-answer",
    2: "unknown-part",
    3: "capacity-not-corroborated",
    4: "no-session",
    5: "range",
    6: "length-or-read",
    7: "malformed",
}


class ProbeError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def crc16(data, c=0):
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c & 0xFFFF


def pack7(data):
    acc = nb = 0
    out = bytearray()
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return bytes(out)


def unpack7(s):
    acc = nb = 0
    out = bytearray()
    for b in s:
        acc |= (b & 0x7F) << nb
        nb += 7
        if nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def frame(cmd, body):
    msg = bytearray([0, 0x59, cmd, len(body) & 0xFF, (len(body) >> 8) & 0xFF, 0])
    msg += body
    msg.append((~sum(body)) & 0xFF)
    return b"\xF0" + pack7(bytes(msg)) + b"\xF7"


def decode(pkt):
    """one complete device message -> (cmd, body); raises ProbeError('frame'/'crc')"""
    if not pkt or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        raise ProbeError("frame", "not a SysEx message")
    d = unpack7(pkt[1:-1])
    if len(d) < 7 or d[0] != 0 or d[1] != 0x59:
        raise ProbeError("frame", "not an FM-1 message")
    n = d[3] | d[4] << 8 | d[5] << 16
    if len(d) != n + 7:
        raise ProbeError("frame", "length %d, body says %d" % (len(d) - 7, n))
    body = d[6:6 + n]
    if (~sum(body)) & 0xFF != d[6 + n]:
        raise ProbeError("crc", "message checksum")
    return d[2], body


def _transact(link, cmd, body, reply, timeout):
    if not link.send(frame(cmd, body)):
        raise ProbeError("lost", "the FM-1 is not there")
    end = time.monotonic() + timeout
    seen = 0
    while (left := end - time.monotonic()) > 0:
        pkt = link.read(left)
        if pkt is None:
            break
        try:
            rcmd, rbody = decode(pkt)
        except ProbeError:
            continue                      # a corrupt frame is not a reply
        seen += 1
        if rcmd == reply:
            return rbody
        if seen > 8:
            break
    raise ProbeError("timeout", "no 0x%02X answer" % reply)


def identify(link, nonce, timeout=1.0):
    """one 0x12 exchange -> dict; raises ProbeError with the device's status"""
    if not isinstance(nonce, (bytes, bytearray)):
        nonce = int(nonce).to_bytes(4, "little")
    body = bytes([PROTO]) + bytes(nonce[:4])
    body += struct.pack("<H", crc16(body))
    r = _transact(link, CMD_IDENT, body, CMD_IDENT, timeout)
    if len(r) != 24:
        raise ProbeError("frame", "identification reply is %d bytes, not 24" % len(r))
    if crc16(r[:21]) != int.from_bytes(r[21:23], "little"):
        raise ProbeError("crc", "identification reply crc")
    status = r[1]
    jedec = int.from_bytes(r[2:5], "big")
    if status:
        raise ProbeError(STATUS.get(status, "status-%d" % status),
                         "the FM-1 refused to identify the NOR (status %d%s)"
                         % (status, ", JEDEC %06X" % jedec if jedec else ""))
    part = PARTS.get(jedec)
    if not part:
        raise ProbeError("unknown-part", "JEDEC %06X is not a part we know" % jedec)
    got = (int.from_bytes(r[5:9], "little"), int.from_bytes(r[9:11], "little"),
           int.from_bytes(r[11:13], "little"), int.from_bytes(r[13:17], "little"))
    if got != part[1:]:
        raise ProbeError("geometry", "geometry %s does not match the identified %s (expected %s)"
                         % (got, part[0], part[1:]))
    session = int.from_bytes(r[17:21], "little")
    if not session:
        raise ProbeError("no-session", "identification without a session token")
    return {"proto": r[0], "status": status, "jedec": jedec, "part": part[0], "cap": got[0],
            "page": got[1], "sector": got[2], "block": got[3], "session": session}


def probe(link, timeout=1.0):
    """two identifications of the same unit: same part, fresh token each time"""
    nonce = secrets.token_bytes(4)
    a = identify(link, nonce, timeout)
    b = identify(link, nonce, timeout)
    for k in ("jedec", "cap", "page", "sector", "block"):
        if a[k] != b[k]:
            raise ProbeError("mismatch", "two identifications disagree on %s: %r vs %r" % (k, a[k], b[k]))
    if a["session"] == b["session"]:
        raise ProbeError("replay", "the device reused a session token: the reply may be old")
    ev = dict(b)
    ev["readings"] = 2
    ev["nonce"] = nonce.hex()
    ev["source"] = "device-0x12"
    return ev


def read(link, addr, length, session, timeout=1.0):
    """one 0x13 read (<= CHUNK bytes), fully validated"""
    if length <= 0:
        raise ProbeError("length", "zero-length read")
    if length > CHUNK:
        raise ProbeError("length", "a single read is at most %d bytes" % CHUNK)
    if addr < 0 or addr + length > CAP:
        raise ProbeError("range", "0x%X+%d is outside the 1 MiB part" % (addr, length))
    body = struct.pack("<BIH", PROTO, addr, length) + session.to_bytes(4, "little")
    body += struct.pack("<H", crc16(body))
    r = _transact(link, CMD_READ, body, CMD_READ, timeout)
    if len(r) < 14:
        raise ProbeError("frame", "read reply is %d bytes" % len(r))
    if crc16(r[:12]) != int.from_bytes(r[12:14], "little"):
        raise ProbeError("crc", "read reply crc")
    if r[1]:
        raise ProbeError(STATUS.get(r[1], "status-%d" % r[1]), "read of 0x%X refused (status %d)" % (addr, r[1]))
    if int.from_bytes(r[2:4], "little") != length:
        raise ProbeError("short", "asked %d bytes, got %d" % (length, int.from_bytes(r[2:4], "little")))
    if int.from_bytes(r[4:8], "little") != addr:
        raise ProbeError("address", "reply is for 0x%X, not 0x%X"
                         % (int.from_bytes(r[4:8], "little"), addr))
    if int.from_bytes(r[8:12], "little") != session:
        raise ProbeError("stale", "reply carries an old session token")
    if len(r) != 14 + length:
        raise ProbeError("short", "reply is %d bytes, expected %d" % (len(r), 14 + length))
    return r[14:]


def read_range(link, addr, length, session, timeout=1.0, tries=3, chunk=CHUNK):
    """chunked read; -> (bytes, [ (addr, code, message) ])"""
    out = bytearray()
    errors = []
    off = addr
    while off < addr + length:
        n = min(chunk, addr + length - off)
        got = None
        last = None
        for _ in range(tries):
            try:
                got = read(link, off, n, session, timeout)
                break
            except ProbeError as e:
                last = e
                if e.code in ("range", "length", "unknown-part"):
                    break
        if got is None:
            errors.append((off, last.code, str(last)))
            return bytes(out), errors
        out += got
        off += n
    return bytes(out), errors
