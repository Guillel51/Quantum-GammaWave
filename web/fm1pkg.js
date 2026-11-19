// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// FM-1 .fwsc packages in the browser: the package identity and the image the
// device reads during an update.

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F;

/* The app window of the update loader this project ships (firmware/loader/ldr_core.c
 * LDR_APP_LO / LDR_APP_HI). The loader erases and writes exactly this range and nothing
 * else, so a package whose app area leaves it cannot be installed by that loader: the
 * image would arrive truncated. It is a boundary of the loader, not of the flash. */
export const LOADER_APP_LO = 0x4000;
export const LOADER_APP_HI = 0x93000;
/* the marker firmware/loader (and tools/fm1pkg_make.py) plant in the package when ota.bin is
 * this project's loader: a package that carries it is written by OUR loader, with OUR window. */
export const FELUCCA_LOADER_MARK = "FELUCCA-LOADER-1";

/* Two official M-VAVE FM-1 packages are known, each pinned by the digest of the exact file
 * M-VAVE published. A pin is integrity, not authenticity: it proves the bytes are those the
 * vendor shipped, not who shipped them.
 *
 * V15 (699 956 B) is the release this project has installed from since 1.0. V16 (718 196 B)
 * was measured here with this repository's own parser: the values in `layout` and `reference`
 * are readings of the file, not assumptions. V16 moved the app area: app.bin starts at 0x4140
 * and is 597 928 B, the app area block is 671 857 B (it ends at 0xA8071, past V15's 0x93000),
 * and the flash image grows from 0x93000 to 0xA9000. The head (0x0000-0x3FFF, the bootloader
 * the install never writes) is a different SPL: 0x8f5228c2… against the V16 file.
 *
 * `imageSha256` is the logical image (the .fwsc minus its 20 marker bytes): that is what the
 * device reads, so it is the value worth pinning beside the file digest. */
export const STOCK_V15_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a";
export const STOCK_APP_SIZE = 0x8E7BC;
export const STOCK_V15_SIZE = 699956;
export const STOCK_V16_SHA256 = "d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5";
export const STOCK_V16_SIZE = 718196;
export const STOCK_V16_IMAGE_SHA256 = "cca5d6910017344674e6128e529876572ce443bcae41186d0d69768f0d1f1d2a";

export const STOCK_RELEASES = Object.freeze([
  Object.freeze({
    version: 15, identity: "FM-1_015", sha256: STOCK_V15_SHA256, size: STOCK_V15_SIZE,
    imageSha256: null, imageSize: STOCK_V15_SIZE - BLOCKS,
    /* V15 is pinned by digest and by the four layout values this project has always checked.
     * Its other structures are not re-derived here: the file is not in this tree. */
    layout: Object.freeze({ flashFileOff: 1044, flashSize: 0x93000, appBase: 0x4000,
      appOff: 16672, appSize: STOCK_APP_SIZE, otaSize: 19969 }),
    measured: false,
  }),
  Object.freeze({
    version: 16, identity: "FM-1_016", sha256: STOCK_V16_SHA256, size: STOCK_V16_SIZE,
    imageSha256: STOCK_V16_IMAGE_SHA256, imageSize: STOCK_V16_SIZE - BLOCKS,
    layout: Object.freeze({ flashFileOff: 1044, flashSize: 0xA9000, appBase: 0x4000,
      appOff: 0x4140, appSize: 597928, appDirSize: 0xA4071, otaSize: 19969 }),
    /* what the file's own regions hash to, from this repository's parser: these are the
     * reference values published for V16 and they reproduce byte for byte. */
    reference: Object.freeze({
      headSha256: "8f5228c2d6ea2068838ef4916f946bea877d14b07e6cfca49e41957f4c7b234f",
      appRegionSha256: "c47d8895963c070f9334efd6a617d7aae5daf8480ffcf3438ffb634c03de8ef7",
      appRegion93000Sha256: "980ff51ae2d90004cf3887602ebd11cd31796977410eb77cc7faabad4023003c",
    }),
    measured: true,
  }),
]);

export const STOCK_IDENTITIES = Object.freeze(STOCK_RELEASES.map((r) => r.identity));
export const isStockIdentity = (text) => STOCK_IDENTITIES.includes(String(text || ""));
export const stockReleaseBySize = (size) =>
  STOCK_RELEASES.find((r) => r.size === size) || null;
export const stockReleaseByIdentity = (identity) =>
  STOCK_RELEASES.find((r) => r.identity === identity) || null;

const T = new Uint16Array(256);
for (let i = 0; i < 256; i++) {
  let c = i << 8;
  for (let k = 0; k < 8; k++) c = c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1;
  T[i] = c & 0xFFFF;
}

export function crc16(d, crc = 0) {
  for (let i = 0; i < d.length; i++) crc = ((crc << 8) & 0xFFFF) ^ T[((crc >> 8) ^ d[i]) & 0xFF];
  return crc;
}

function enc(buf, off, size, key = 0xFFFF) {
  if (off < 0 || size < 0 || off + size > buf.length) throw new Error("package field out of bounds");
  for (let i = 0; i < size; i++) {
    buf[off + i] ^= key & 0xFF;
    key = ((key * 2) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
}

function sfc(buf, off, size, base, key) {
  for (let i = 0; i < size; i += 32) enc(buf, off + i, Math.min(size - i, 32), key ^ ((off + i - base) >> 2));
}

function chipkeyDecode(d) {
  let sum = 0;
  for (let i = 0; i < 16; i++) sum += d[i];
  sum &= 0xFF;
  sum = sum >= 0xE0 ? 0xAA : sum <= 0x10 ? 0x55 : sum;
  let k = 0;
  for (let i = 0; i < 16; i++) if ((d[16 + i] ^ d[15 - i]) < sum) k |= 1 << i;
  return k;
}

const align32 = (v) => (v + 31) & ~31;
const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
function w16(b, o, v) { b[o] = v & 0xFF; b[o + 1] = (v >> 8) & 0xFF; }
function w32(b, o, v) { w16(b, o, v & 0xFFFF); w16(b, o + 2, (v >>> 16) & 0xFFFF); }

class Entry {                                   // JLFS head, 32 bytes
  constructor(raw, off) {
    if (!Number.isInteger(off) || off < 0 || off + 32 > raw.length)
      throw new Error(`JLFS header out of bounds @${off}`);
    this.hdrOff = off;
    this.hcrc = u16(raw, off);
    this.dcrc = u16(raw, off + 2);
    this.offset = u32(raw, off + 4);
    this.size = u32(raw, off + 8);
    this.flags = raw[off + 12];
    this.resvd = raw[off + 13];
    this.index = u16(raw, off + 14);
    this.rawName = raw.slice(off + 16, off + 32);
    const z = this.rawName.indexOf(0);
    this.name = String.fromCharCode(...this.rawName.slice(0, z < 0 ? 16 : z));
    if (crc16(raw.subarray(off + 2, off + 32)) !== this.hcrc) throw new Error(`JLFS header CRC mismatch @${off.toString(16)}`);
  }
  packInto(buf) {
    const o = this.hdrOff;
    w16(buf, o + 2, this.dcrc); w32(buf, o + 4, this.offset); w32(buf, o + 8, this.size);
    buf[o + 12] = this.flags; buf[o + 13] = this.resvd; w16(buf, o + 14, this.index);
    buf.set(this.rawName, o + 16);
    this.hcrc = crc16(buf.subarray(o + 2, o + 32));
    w16(buf, o, this.hcrc);
  }
}

export class Package {
  constructor(bytes) {
    this.raw = new Uint8Array(bytes);
    this.parseUfw();
    this.flash = this.raw.slice(this.flashFileOff, this.flashFileOff + this.flashSize);
    this.parseFlash();
  }

  get product() { return productOf(this.raw); }

  setProduct(name) {
    if (name.length > BLOCKS) throw new Error("product name too long");
    for (let i = 0; i < BLOCKS; i++) {
      const m = i < name.length ? (name.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
      if (i < name.length && m === 0x7D) throw new Error("product name encodes to the unused marker");
      this.raw[i * BLK + KEEP] = m;
    }
  }

  headerBytes() {
    const h = new Uint8Array(BLOCKS * KEEP);
    for (let i = 0; i < BLOCKS; i++) h.set(this.raw.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
    return h;
  }

  hdrFileOff(i) { return Math.floor(i / KEEP) * BLK + (i % KEEP); }

  parseUfw() {
    /* (QG-1 stage 2) a file shorter than the 20 blocks the identity lives in cannot be a package:
     * headerBytes() would read an empty tail and all-zero CRCs would compare equal */
    if (this.raw.length < BLOCKS * BLK) throw new Error("not an FM-1 package (too short)");
    const h = this.headerBytes();
    enc(h, 0, 0x40);
    const hcrc = u16(h, 0), lcrc = u16(h, 2), nent = u16(h, 8);
    if (crc16(h.subarray(2, 0x40)) !== hcrc) throw new Error("not an FM-1 .fwsc (UFW header CRC mismatch)");
    this.skew = BLOCKS * BLK - h.length;
    this.ufw = h;
    this.nent = nent;
    const hsize = 0x40 + nent * 0x50;
    if (hsize > h.length) throw new Error("UFW entry list is larger than the header");
    if (crc16(h.subarray(0x40, hsize)) !== lcrc) throw new Error("UFW entry list CRC mismatch");
    this.entries = [];
    for (let off = 0x40; off < hsize; off += 0x50) {
      const e = h.slice(off, off + 0x50);
      enc(e, 0, 0x50);
      const z = e.subarray(0x40, 0x50).indexOf(0);
      this.entries.push({ off, type: u16(e, 0), dcrc: u16(e, 4), dataOff: u32(e, 8), size: u32(e, 12),
        name: String.fromCharCode(...e.subarray(0x40, z < 0 ? 0x50 : 0x40 + z)), plain: e });
    }
    const fl = this.entries.find((e) => e.type === 0);
    if (!fl) throw new Error("no flash.bin in the package");
    this.flashEntry = fl;
    this.flashFileOff = fl.dataOff + this.skew;
    this.flashSize = fl.size;
    if (this.flashFileOff < 0 || this.flashFileOff + fl.size > this.raw.length)
      throw new Error("flash.bin is outside the package");
    if (crc16(this.raw.subarray(this.flashFileOff, this.flashFileOff + fl.size)) !== fl.dcrc) throw new Error("flash.bin CRC mismatch");
  }

  parseFlash() {
    if (this.flash.length < 32) throw new Error("flash image is too short");
    const dec = this.flash.slice();
    const h = dec.slice(0, 32);
    enc(h, 0, 32);
    if (crc16(h.subarray(2, 32)) !== u16(h, 0)) throw new Error("flash header CRC mismatch");
    this.chipKey = null; this.appBase = null;
    let listEnd = false;
    for (let off = 32; off + 32 <= dec.length; off += 32) {
      enc(dec, off, 32);
      const e = new Entry(dec, off);
      if (e.name === "isd_config.ini" && e.offset + 34 <= dec.length) {
        const blob = dec.subarray(e.offset, e.offset + 34);
        if (crc16(blob.subarray(0, 32)) === u16(blob, 32)) this.chipKey = chipkeyDecode(blob.subarray(0, 32));
      }
      if (e.flags === 0x81 && e.name === "app_dir_head" && e.offset + 32 <= dec.length && this.appBase === null)
        this.appBase = e.offset;
      if (e.index) { listEnd = true; break; }
    }
    if (!listEnd) throw new Error("flash entry list has no end");
    if (this.chipKey === null || this.appBase === null) throw new Error("chip key or app_dir_head not found");
    const b = this.appBase, k = this.chipKey, end = align32(b + 32);
    if (!Number.isInteger(b) || b < 0 || end > dec.length) throw new Error("app_dir_head is outside the flash");
    sfc(dec, b, end - b, b, k);
    const head = new Entry(dec, b);
    const blkEnd = b + head.size;
    if (blkEnd > dec.length) throw new Error("app area is outside the flash");
    if (blkEnd > end) sfc(dec, end, blkEnd - end, b, k);
    this.sfcEnd = Math.max(end, blkEnd);
    this.cfgLen = 0;
    const probe = this.flash.slice(b, align32(blkEnd) + 32);
    sfc(probe, 0, probe.length, 0, k);
    try {
      const cfg = new Entry(probe, blkEnd - b);
      if (cfg.name === "cfg" && blkEnd + cfg.size <= this.flash.length) this.cfgLen = cfg.size;
    } catch (_) { /* no cfg block */ }
    if (this.cfgLen) {
      const region = this.flash.slice(b, blkEnd + this.cfgLen);
      sfc(region, 0, region.length, 0, k);
      dec.set(region, b);
      this.sfcEnd = blkEnd + this.cfgLen;
    }
    this.head = head;
    this.app = null;
    let appEnd = false;
    /* (QG-1 stage 2) the app entry list lives inside the app area (head.size): the parser decrypts
     * exactly [b, blkEnd), so an entry past blkEnd is still ciphertext and can never pass its header
     * CRC. Stop there, as setApp() does, instead of walking the rest of a truncated list */
    const appLimit = Math.min(dec.length, blkEnd);
    for (let off = b + 32; off + 32 <= appLimit; off += 32) {
      const e = new Entry(dec, off);
      if (e.flags === 0x82 && e.name === "app.bin") this.app = e;
      if (e.index) { appEnd = true; break; }
    }
    if (!appEnd) throw new Error("app entry list has no end");
    if (!this.app) throw new Error("app.bin not found in app area");
    this.appOff = b + this.app.offset;
    if (this.appOff < 0 || this.appOff + this.app.size > dec.length)
      throw new Error("app.bin is outside the flash");
    this.dec = dec;
  }

  /* every file the app area declares with attr 0x82 (app.bin, cfg_tool.bin, ...), with its CRC16
   * checked against the bytes the chip key decrypts: formats.rs app_files_status does the same walk,
   * and that is the check M-Flasher makes before it writes (ops.rs plan_firmware). */
  appFiles() {
    const out = [], b = this.appBase, dec = this.dec, end = Math.min(dec.length, b + this.head.size);
    for (let off = b + 32; off + 32 <= end; off += 32) {
      let e;
      try { e = new Entry(dec, off); } catch (_) { break; }
      if (e.flags === 0x82) {
        const a = b + e.offset;
        const ok = a + e.size <= dec.length && crc16(dec.subarray(a, a + e.size)) === e.dcrc;
        out.push({ name: e.name, size: e.size, crc: e.dcrc, ok });
      }
      if (e.index) break;
    }
    return out;
  }

  // the new app, padded to the stock size (Felucca never changes the layout)
  setApp(app) {
    if (app.length > this.app.size) throw new Error(`app is ${app.length} B, the slot is ${this.app.size} B`);
    const nw = new Uint8Array(this.app.size).fill(0xFF);
    nw.set(app);
    const a = this.app, h = this.head, b = this.appBase;
    this.dec.set(nw, this.appOff);
    a.dcrc = crc16(nw);
    a.packInto(this.dec);
    h.dcrc = crc16(this.dec.subarray(h.hdrOff + 32, h.hdrOff + h.size));
    h.packInto(this.dec);
    this.sfcEnd = Math.max(align32(b + 32), b + h.size + this.cfgLen);
    const region = this.dec.slice(b, this.sfcEnd);
    sfc(region, 0, region.length, 0, this.chipKey);
    this.flash.set(region, b);
  }

  serialize() {
    const out = this.raw.slice();
    out.set(this.flash, this.flashFileOff);
    const fl = this.flashEntry, e = fl.plain.slice();
    w16(e, 4, crc16(this.flash));
    enc(e, 0, 0x50);
    const h = this.ufw.slice();
    h.set(e, fl.off);
    const hsize = 0x40 + this.nent * 0x50;
    w16(h, 2, crc16(h.subarray(0x40, hsize)));
    w16(h, 0, crc16(h.subarray(2, 0x40)));
    enc(h, 0, 0x40);
    for (let i = 0; i < hsize; i++) out[this.hdrFileOff(i)] = h[i];
    return out;
  }
}

// the package identity ("FM-1_904"): one marker byte after each of the first 20 blocks
export function productOf(fwsc) {
  if (fwsc.length < BLOCKS * BLK) throw new Error("not an FM-1 package (too short)");
  let s = "";
  for (let i = 0; i < BLOCKS; i++) {
    const m = fwsc[i * BLK + KEEP];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

// the image the device addresses during an update: the .fwsc without the 20 marker bytes
export function logicalImage(fwsc) {
  const out = new Uint8Array(fwsc.length - BLOCKS);
  for (let i = 0; i < BLOCKS; i++) out.set(fwsc.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  out.set(fwsc.subarray(BLOCKS * BLK), BLOCKS * KEEP);
  return out;
}

export async function sha256hex(bytes) {
  const d = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

/* The layout the pin promises, checked against what the container actually says. Pure and
 * synchronous, so a test can drive it with a profile of its own. Returns a reason or null. */
export function stockLayoutIssue(pkg, profile) {
  const L = profile.layout;
  if (pkg.product !== profile.identity)
    return `the package identifies as ${pkg.product}, not ${profile.identity}`;
  if (pkg.flashFileOff !== L.flashFileOff)
    return `flash.bin is at ${pkg.flashFileOff}, not ${L.flashFileOff}`;
  if (pkg.flashSize !== L.flashSize)
    return `the flash image is ${pkg.flashSize} B, not ${L.flashSize} B`;
  if (pkg.appBase !== L.appBase)
    return `app_dir_head is at 0x${pkg.appBase.toString(16)}, not 0x${L.appBase.toString(16)}`;
  if (pkg.appOff !== L.appOff)
    return `app.bin is at 0x${pkg.appOff.toString(16)}, not 0x${L.appOff.toString(16)}`;
  if (!pkg.app || pkg.app.size !== L.appSize)
    return `the app slot is ${pkg.app ? pkg.app.size : "missing"}, not ${L.appSize} B`;
  const ota = pkg.entries.find((e) => e.type === 100 && e.name === "ota.bin");
  if (!ota || ota.size !== L.otaSize)
    return `ota.bin is ${ota ? ota.size + " B" : "missing"}, not ${L.otaSize} B`;
  if (L.appDirSize !== undefined) {
    if (pkg.head.size !== L.appDirSize)
      return `app_area_head declares ${pkg.head.size} B, not ${L.appDirSize} B`;
    if (pkg.appBase + pkg.head.size > pkg.flashSize)
      return "app_area_head reaches past the flash image";
  }
  /* the readings this repository took of the file: the chip key and every attr-0x82 app file
   * decrypting with it and matching its CRC16 (M-Flasher's own app_files_status check). Only
   * for a profile that was actually measured -- V15 is pinned by digest, not re-derived. */
  if (profile.measured) {
    if (pkg.chipKey !== 0x980F)
      return `the chip key decodes to ${pkg.chipKey === null ? "nothing" : "0x" + pkg.chipKey.toString(16)}, not 0x980F`;
    const files = pkg.appFiles();
    if (!files.length) return "the app area declares no attr-0x82 file";
    const bad = files.filter((f) => !f.ok);
    if (bad.length) return `${bad[0].name} does not decrypt/match its CRC16 with the chip key`;
  }
  return null;
}

// Which official release are these bytes? Matched on size and SHA-256 only: a filename and the
// identity woven into the container are claims, the digest is the pin.
export async function stockReleaseOf(bytes) {
  if (!bytes || typeof bytes.length !== "number") return null;
  const bySize = stockReleaseBySize(bytes.length);
  if (!bySize) return null;
  return (await sha256hex(bytes)) === bySize.sha256 ? bySize : null;
}

// Only the user's unmodified official file is accepted for a return to stock, whichever release
// it is. The digest pins every file, loader and flash layout; filenames are not evidence.
export async function validateStockPackage(bytes) {
  const profile = await stockReleaseOf(bytes);
  if (!profile)
    throw new Error("Select the unmodified official M-VAVE FM-1 file (FM-1.fwsc, V15 or V16).");
  const pkg = new Package(bytes);
  const issue = stockLayoutIssue(pkg, profile);
  if (issue) throw new Error(`The official ${profile.identity} file has an unsupported FM-1 layout: ${issue}.`);
  const image = logicalImage(bytes);
  if (profile.imageSize && image.length !== profile.imageSize)
    throw new Error(`The official ${profile.identity} image is ${image.length} B, not ${profile.imageSize} B.`);
  if (profile.imageSha256 && (await sha256hex(image)) !== profile.imageSha256)
    throw new Error(`The official ${profile.identity} image is not the pinned one.`);
  return { product: pkg.product, version: profile.version, profile,
    image, sha256: profile.sha256, layout: profile.layout, loaderWindow: [LOADER_APP_LO, LOADER_APP_HI] };
}

/* Is this project's update loader inside these bytes? The UFW entry names ota.bin, but the
 * marker is what the release job checks (web/make_site.py) and what tells the transport that
 * OUR loader, with OUR write window, is the program that will do the erase. */
export function hasFeluccaLoader(bytes) {
  const mark = FELUCCA_LOADER_MARK;
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  outer: for (let i = 0; i + mark.length <= b.length; i++) {
    for (let j = 0; j < mark.length; j++) if (b[i + j] !== mark.charCodeAt(j)) continue outer;
    return true;
  }
  return false;
}

/* The app area a package declares, and whether our loader can write all of it. A package whose
 * flash image is shorter than the loader's window is refused by the loader itself
 * (ldr_core.c: `fl_size < LDR_APP_HI`); one whose app area crosses LDR_APP_HI is the dangerous
 * case -- the loader would stop mid-app and the unit would boot a truncated image. Both are
 * answered here, before a frame leaves the host. */
export function loaderWindowIssue(pkg) {
  if (!pkg || !pkg.head) return "the package has no app_area_head";
  if (pkg.flashSize < LOADER_APP_HI)
    return `the flash image is ${pkg.flashSize} B, the update loader needs at least ${LOADER_APP_HI} B`;
  const end = pkg.appBase + pkg.head.size;
  if (end > LOADER_APP_HI)
    return `the app area ends at 0x${end.toString(16)}, past the update loader's window `
      + `(0x${LOADER_APP_LO.toString(16)}-0x${LOADER_APP_HI.toString(16)}): it would be installed truncated`;
  return null;
}

// official V15 + Felucca app -> installable package (Uint8Array). V16's app area ends past the
// loader's window, so a V16 donor is not turned into a package here: tools/fm1pkg_make.py builds
// the package this project installs, and its app area is inside the window by construction.
export async function buildFeluccaPackage(stockBytes, appBytes, product) {
  const profile = await stockReleaseOf(stockBytes);
  if (!profile) throw new Error("this is not an official FM-1 firmware file (FM-1.fwsc)");
  if (profile.version !== 15)
    throw new Error(`the official ${profile.identity} layout keeps app.bin past the update loader's `
      + "window: build the package with tools/fm1pkg_make.py instead of patching a vendor file");
  const pkg = new Package(stockBytes);
  pkg.setApp(appBytes);
  pkg.setProduct(product);
  return pkg.serialize();
}
