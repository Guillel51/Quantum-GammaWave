// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Bounds checks for the .fwsc reader: a malformed package must fail with an error, not hang the page.
// Run from the repo root: node web/test_pkg_bounds.mjs

import { Package, crc16 } from "./fm1pkg.js";

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, SKEW = BLOCKS * BLK - BLOCKS * KEEP;
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };

function enc(buf, off, size, key = 0xFFFF) {
  for (let i = 0; i < size; i++) {
    buf[off + i] ^= key & 0xFF;
    key = ((key * 2) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
}
function w16(b, o, v) { b[o] = v & 0xFF; b[o + 1] = (v >> 8) & 0xFF; }
function w32(b, o, v) { w16(b, o, v & 0xFFFF); w16(b, o + 2, (v >>> 16) & 0xFFFF); }
function hdrFileOff(i) { return Math.floor(i / KEEP) * BLK + (i % KEEP); }

/* A minimal UFW header: one flash entry by default. The entry list is plain, each entry encrypted;
 * the first 0x40 header bytes are encrypted after their CRCs are set (the same order as serialize()). */
function makeFlashHeader() {
  const f = new Uint8Array(32);                    // body all zero: crc16(zeros) = 0, then encrypted
  enc(f, 0, 32);
  return f;
}

function makePackage({ nent = 1, size = 32, dataOff = 0x800, flashHeader = false } = {}) {
  const raw = new Uint8Array(4096);
  const h = new Uint8Array(BLOCKS * KEEP);
  for (let i = 0; i < BLOCKS; i++) h.set(raw.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  w16(h, 8, nent);
  if (nent === 1) {
    const e = new Uint8Array(0x50);
    w16(e, 0, 0);                                  // type 0: flash.bin
    w16(e, 4, 0);                                  // dcrc: 32 zero bytes
    w32(e, 8, dataOff);
    w32(e, 12, size);
    for (let i = 0; i < 9; i++) e[0x40 + i] = "flash.bin".charCodeAt(i);
    enc(e, 0, 0x50);
    h.set(e, 0x40);
  }
  const hsize = 0x40 + nent * 0x50;
  if (hsize <= h.length) w16(h, 2, crc16(h.subarray(0x40, hsize)));
  w16(h, 0, crc16(h.subarray(2, 0x40)));
  enc(h, 0, 0x40);
  for (let i = 0; i < Math.min(hsize, h.length); i++) raw[hdrFileOff(i)] = h[i];
  if (flashHeader) raw.set(makeFlashHeader(), dataOff + SKEW);   // the flash image starts at dataOff + skew
  return raw;
}

function throws(label, fn, re) {
  let good = false, msg = "";
  try { fn(); } catch (e) { msg = String(e && e.message || e); good = re.test(msg); }
  ok(good, `${label}${good ? "" : ` (${msg || "no error"})`}`);
}

throws("package: fewer than 20 blocks is refused", () => new Package(new Uint8Array(10)), /too short|no flash|UFW/i);
throws("package: a zero-size flash.bin is refused", () => new Package(makePackage({ size: 0 })), /flash image is too short/i);
throws("package: a flash with no entry-list end is refused",
       () => new Package(makePackage({ size: 32, flashHeader: true })), /no end/i);
throws("package: an oversized UFW entry count is refused", () => new Package(makePackage({ nent: 0xFFFF })), /entry list is larger/i);
throws("package: flash.bin outside the package is refused", () => new Package(makePackage({ size: 32, dataOff: 0xFFFFFF })), /outside the package/i);

if (failed) {
  console.error(`${failed} package bounds check(s) failed`);
  process.exit(1);
}
console.log("package bounds: all checks passed");
