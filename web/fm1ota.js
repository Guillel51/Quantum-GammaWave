// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol over WebMIDI (after tools/fm1_install.py).
// Step 1: the running firmware (official
// or Felucca) reads parts of the package and stages the update loader. Step 2:
// the loader reads the whole image and writes it. Both steps are "the device
// asks, we answer": cmd 0x30 read requests on the logical image.
//
// Nothing here trusts its caller: install(), write() and resume() each verify the
// package themselves (requireProof) and serve only the private copy that was
// verified, so neither a direct call nor a buffer changed after the fact can
// reach the device.

import { logicalImage, productOf, sha256hex } from "./fm1pkg.js";
import { requireProvenance } from "./fm1provenance.js";

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);

// The documented 1 MiB parts. The update is refused unless the caller can present the JEDEC of
// THIS unit, read from the device or with an external reader: the identity frame carries the model
// only, so the page today cannot produce it and the install never starts (tools/fm1_gate.py).
// A part may be READ as soon as it is identified; only a validated part may be WRITTEN.
// 0xB36014 is accepted by the read-only 0x13 path (diagnostics, backup) and refused by
// requireChip(), exactly as tools/fm1_gate.py does: one policy for both installers.
const KNOWN_JEDEC = { 0x856014: "Puya P25Q80H (1 MiB)" };
const READ_ONLY_JEDEC = { 0xB36014: "UCUN UC25HQ80 (1 MiB): readable, not validated for writing" };

export function requireChip(evidence) {
  const jedec = evidence && evidence.jedec;
  if (jedec === undefined || jedec === null)
    throw fail("chip", "The physical flash chip was not identified: the update was not started.");
  if (jedec === 0xFFFFFF || jedec === 0 || jedec === 0xFFFFFFFF)
    throw fail("chip", `The flash answered ${jedec.toString(16)}: no usable JEDEC. Nothing was sent.`);
  if (jedec in READ_ONLY_JEDEC)
    throw fail("chip", `JEDEC 0x${jedec.toString(16)} (${READ_ONLY_JEDEC[jedec]}) is not validated for writing: nothing was sent.`);
  if (!(jedec in KNOWN_JEDEC))
    throw fail("chip", `JEDEC 0x${jedec.toString(16)} is not in the documented 1 MiB allowlist. Nothing was sent.`);
  return KNOWN_JEDEC[jedec];
}

// errors carry a code the pages translate: notfound, model, stopped, noloader, lost, noreturn,
// mismatch (detail: the identity the device reports), badreq
function fail(code, msg, detail = "") { const e = new Error(msg); e.code = code; e.detail = detail; return e; }

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

function response(addr, data, fl = 0) {
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

function parseIdentity(pkt) {
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  return m ? { text: txt, model: m[1], version: parseInt(m[2], 10) } : null;
}

// one MIDI in/out pair with a SysEx queue
class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // false when the port is gone (unplugged): WebMIDI throws on a disconnected output
  send(bytes) {
    try { this.output.send(bytes); return true; } catch (_) { return false; }
  }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

async function handshake(link, tries = 3) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HS_QUERY)) return null;
    const end = Date.now() + 1000;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = parseIdentity(p);
      if (id) return id;
    }
  }
  return null;
}

export class Updater {
  constructor(access, log, timing = {}) {
    this.access = access;
    this.log = log || (() => {});
    this.loaderDelay = timing.loaderDelay ?? 3000;
    this.loaderWait = timing.loaderWait ?? 30000;
    this.rebootDelay = timing.rebootDelay ?? 3000;
    this.rebootWait = timing.rebootWait ?? 40000;
  }

  // every in/out pair with the same port name that answers the handshake
  async find(filter) {
    const outs = [...this.access.outputs.values()];
    for (const input of this.access.inputs.values()) {
      if (input.state === "disconnected") continue;
      if (!/fm-1|felucca|ota|composite|sinco|usb-midi/i.test(input.name || "")) continue;   // never probe other gear
      const output = outs.find((o) => o.name === input.name && o.state !== "disconnected");
      if (!output) continue;
      try { await input.open(); await output.open(); } catch (_) { continue; }
      const link = new Link(input, output);
      const id = await handshake(link, 2);
      if (id && (!filter || filter(id))) return { link, id, name: input.name };
      link.close();
    }
    return null;
  }

  async waitFor(filter, ms) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const dev = await this.find(filter);
      if (dev) return dev;
      await sleep(1000);
    }
    return null;
  }

  // serve read requests until the device asks for one of the finish addresses; stops early
  // when the port disappears (lost: true) instead of waiting out idleMs. Private: this is the
  // only place that sends image bytes, and it must not be reachable from outside the class —
  // install()/write() reach it only after admit() returned the verified copy.
  async #serve(pass, link, finishAddr, idleMs, onProgress) {
    if (!pass || pass.link !== link) throw fail("proof", "this transfer has no admission on this link: nothing was sent.");
    const image = pass.image;
    let served = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(1000);
      if (!pkt) {
        if (link.lost) return { served, finished: false, lost: true };
        if (Date.now() - last > idleMs) return { served, finished: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = Date.now();
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        if (!link.send(response(r.addr, [...new TextEncoder().encode("success"), 0])))
          return { served, finished: false, lost: true };
        if (r.addr === finishAddr) return { served, finished: true };
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      await sleep(10);
      if (!link.send(response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      if (onProgress) onProgress(served, r.addr);
    }
  }

  // full install: running firmware -> loader -> new firmware. `pkg` is the .fwsc the manifest
  // covers; the image the device reads is derived from the verified private copy (admit).
  async install(pkg, product, onStep, ctx = {}) {
    const step = onStep || (() => {});
    const proof = ctx.proof || await requireProof({ ...ctx, pkg, product });
    const pass = await admit(proof, pkg, product);             // refuses a used or wrong proof before any frame
    const dev = await this.find((id) => !IS_OTA(id));
    if (!dev) throw fail("notfound", "FM-1 not found (USB cable, and no other app using it?)");
    const [model] = product.split("_");
    if (dev.id.model !== model || dev.id.text !== product) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`); }
    await identify(pass, dev.link);                            // 0x12 on this link, before any write frame
    step("start", dev.id.text);
    dev.link.send(UPGRADE);
    await sleep(2000);
    const s1 = await this.#serve(pass, dev.link, 0xE0000000, 8000, (n) => step("verify", n));
    dev.link.close();
    if (!s1.finished) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
    step("loader");
    await sleep(this.loaderDelay);
    const ota = await this.waitFor((id) => id.model === "ota-" + model, this.loaderWait);
    if (!ota) throw fail("noloader", "the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.");
    const s2 = await this.write(ota, pkg, step, { ...ctx, product, proof: null });   // its own authorisation
    if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader ${s2.lost ? "was disconnected" : "stopped"} after ${s2.served} requests. Replug and press Install again to resume.`);
    step("reboot");
    await sleep(this.rebootDelay);
    const back = await this.waitFor((id) => !IS_OTA(id), this.rebootWait);
    if (!back) throw fail("noreturn", "the device did not come back: power-cycle it");
    back.link.close();
    if (back.id.text !== product) throw fail("mismatch", `installed, but the device reports ${back.id.text}`, back.id.text);
    step("done", back.id.text);
    return back.id.text;
  }

  // step 2 with the loader found by find()/waitFor(). It verifies the package again: it is a
  // public entry point, so it cannot rely on install() or resume() having done so.
  async write(ota, pkg, step, ctx = {}) {
    const product = ctx.product || null;
    const model = String(ota.id.model).replace(/^ota-/i, "");
    if (!product || model !== product.split("_")[0])
      throw fail("proof", `the loader is ${ota.id.text}, the authorisation is for ${product}: nothing was sent.`);
    const proof = ctx.proof || await requireProof({ ...ctx, pkg, product });
    const pass = await admit(proof, pkg, product);
    await identify(pass, ota.link);                            // 0x12 on the loader's own link
    ota.link.send(UPGRADE);
    await sleep(2000);
    const s2 = await this.#serve(pass, ota.link, 0xF0000000, 180000, (n) => step("write", n));
    ota.link.close();
    return s2;
  }

  // resume: the device is already in update mode (loader) -> true when the write finished
  async resume(pkg, onStep, product = null, ctx = {}) {
    const step = onStep || (() => {});
    await requireProof({ ...ctx, pkg, product });               // refuse a bad package before touching a port
    const model = product && product.split("_")[0];
    const ota = await this.find((id) => model ? id.model === "ota-" + model : IS_OTA(id));
    if (!ota) return false;
    const result = await this.write(ota, pkg, step, { ...ctx, product, proof: null });
    if (!result.finished) return false;
    if (product) {
      step("reboot");
      await sleep(this.rebootDelay);
      const back = await this.waitFor((id) => id.model === model, this.rebootWait);
      if (!back) throw fail("noreturn", "The FM-1 did not return after resuming the write.");
      back.link.close();
      if (back.id.text !== product) throw fail("mismatch", "The FM-1 reports a different firmware after resuming.", back.id.text);
      step("done", back.id.text);
    }
    return true;
  }
}

// ---------------------------------------------------------------------------------------------
// 0x12 identification and 0x13 read-only flash, plus the Ed25519 manifest check.
// Same frames and the same policy as tools/fm1_probe.py and tools/fm1_sign.py: the web
// page must identify the chip and verify the manifest before it enables Install. Both are
// pure functions of the link, so the page wires them in without touching this protocol.

const QRY_PROTO = 1;
const QRY_CHUNK = 256;
const QRY_CAP = 0x100000;
const QRY_PARTS = {
  0x856014: { part: "P25Q80H", cap: 0x100000, page: 256, sector: 0x1000, block: 0x10000, writable: true },
  0xB36014: { part: "UC25HQ80", cap: 0x100000, page: 256, sector: 0x1000, block: 0x10000, writable: false },
};
const QRY_STATUS = { 1: "no NOR answer", 2: "unknown part", 3: "capacity not corroborated", 4: "no session",
  5: "range", 6: "length or read error", 7: "malformed request" };

function qryCrc16(data, c = 0) {
  for (const b of data) {
    c ^= b << 8;
    for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c & 0xFFFF;
}

function qryFrame(cmd, body) {
  const b = [0, 0x59, cmd, body.length & 0xFF, (body.length >> 8) & 0xFF, 0, ...body];
  let s = 0;
  for (const x of body) s += x;
  b.push(~s & 0xFF);
  return [0xF0, ...pack7(b), 0xF7];
}

function qryDecode(pkt) {
  if (!pkt || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59) return null;
  const n = d[3] | (d[4] << 8) | (d[5] << 16);
  if (d.length !== n + 7) return null;
  const body = d.slice(6, 6 + n);
  let s = 0;
  for (const x of body) s += x;
  if (((~s) & 0xFF) !== d[6 + n]) return null;
  return { cmd: d[2], body };
}

async function qryAsk(link, cmd, body, timeoutMs) {
  if (!link.send(qryFrame(cmd, body))) throw fail("chip", "The FM-1 was disconnected: nothing was sent.");
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const p = await link.read(end - Date.now());
    if (!p) break;
    const r = qryDecode(p);
    if (r && r.cmd === cmd) return r.body;
  }
  throw fail("chip", `The FM-1 did not answer 0x${cmd.toString(16)}: nothing was sent.`);
}

async function qryIdentify(link, nonce, timeoutMs) {
  const body = [QRY_PROTO, ...nonce];
  body.push(qryCrc16(body) & 0xFF, (qryCrc16(body) >> 8) & 0xFF);
  const r = await qryAsk(link, 0x12, body, timeoutMs);
  if (r.length !== 24) throw fail("chip", "Truncated identification reply: nothing was sent.");
  if (qryCrc16(r.slice(0, 21)) !== (r[21] | (r[22] << 8)))
    throw fail("chip", "The identification reply's CRC is wrong: nothing was sent.");
  if (r[1]) throw fail("chip", `The FM-1 refused to identify the NOR (${QRY_STATUS[r[1]] || "status " + r[1]}): nothing was sent.`);
  const jedec = (r[2] << 16) | (r[3] << 8) | r[4];
  const part = QRY_PARTS[jedec];
  if (!part) throw fail("chip", `JEDEC 0x${jedec.toString(16)} is not a part we know: nothing was sent.`);
  const got = { cap: r[5] | (r[6] << 8) | (r[7] << 16) | (r[8] << 24), page: r[9] | (r[10] << 8),
    sector: r[11] | (r[12] << 8), block: r[13] | (r[14] << 8) | (r[15] << 16) | (r[16] << 24) };
  if (got.cap !== part.cap || got.page !== part.page || got.sector !== part.sector || got.block !== part.block)
    throw fail("chip", `The geometry ${JSON.stringify(got)} is not the identified ${part.part}'s: nothing was sent.`);
  const session = (r[17] | (r[18] << 8) | (r[19] << 16) | (r[20] << 24)) >>> 0;
  if (!session) throw fail("chip", "Identification without a session token: nothing was sent.");
  return { jedec, session, ...got, part: part.part };
}

// two identifications of the same unit: same part, a fresh token each time
export async function probeChip(link, timeoutMs = 1000) {
  const nonce = Array.from(crypto.getRandomValues(new Uint8Array(4)));
  const a = await qryIdentify(link, nonce, timeoutMs);
  const b = await qryIdentify(link, nonce, timeoutMs);
  for (const k of ["jedec", "cap", "page", "sector", "block"])
    if (a[k] !== b[k]) throw fail("chip", `Two identifications disagree on ${k}: nothing was sent.`);
  if (a.session === b.session) throw fail("chip", "The device reused a session token: the reply may be old.");
  return { ...b, writable: !!(QRY_PARTS[b.jedec] && QRY_PARTS[b.jedec].writable), readings: 2, source: "device-0x12" };
}

// one 0x13 read (<= 256 bytes), fully validated; read-only
export async function readFlash(link, addr, len, session, timeoutMs = 1000) {
  if (!(len > 0) || len > QRY_CHUNK) throw fail("chip", "a single read is 1..256 bytes");
  if (addr < 0 || addr + len > QRY_CAP) throw fail("chip", "read outside the 1 MiB part");
  const body = [QRY_PROTO, addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, session & 0xFF, (session >> 8) & 0xFF, (session >> 16) & 0xFF,
    (session >>> 24) & 0xFF];
  body.push(qryCrc16(body) & 0xFF, (qryCrc16(body) >> 8) & 0xFF);
  const r = await qryAsk(link, 0x13, body, timeoutMs);
  if (r.length < 14) throw fail("chip", "truncated read reply");
  if (qryCrc16(r.slice(0, 12)) !== (r[12] | (r[13] << 8))) throw fail("chip", "read reply CRC");
  if (r[1]) throw fail("chip", `read of 0x${addr.toString(16)} refused (${QRY_STATUS[r[1]] || "status " + r[1]})`);
  if ((r[2] | (r[3] << 8)) !== len || (r[4] | (r[5] << 8) | (r[6] << 16) | (r[7] << 24)) !== addr ||
      ((r[8] | (r[9] << 8) | (r[10] << 16) | (r[11] << 24)) >>> 0) !== (session >>> 0) || r.length !== 14 + len)
    throw fail("chip", "read reply does not match the request");
  return r.slice(14);
}

function canonJson(v) {
  if (v === null || typeof v !== "object") return JSON.stringify(v);
  if (Array.isArray(v)) return "[" + v.map(canonJson).join(",") + "]";
  return "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canonJson(v[k])).join(",") + "}";
}

function hexBytes(hex) {
  if (typeof hex !== "string" || hex.length % 2) throw fail("signature", "not a hex string");
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) {
    const b = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    if (Number.isNaN(b)) throw fail("signature", "not a hex string");
    out[i] = b;
  }
  return out;
}

// The Ed25519 manifest of tools/fm1_sign.py, verified with WebCrypto. `keys` maps key id -> public
// key (hex). An empty map trusts nothing, so every package is refused: that is the released state.
export async function verifyManifest(pkg, manifest, keys, { product = "FM-1", firmware = null } = {}) {
  if (!manifest || manifest.format !== "qg1-release-1") throw fail("signature", "No signed manifest.");
  for (const f of ["product", "version", "min_firmware", "sha256", "size", "keyid", "sig"])
    if (manifest[f] === undefined) throw fail("signature", `The manifest has no ${f}.`);
  const pub = keys && keys[manifest.keyid];
  if (!pub) throw fail("signature", `Key ${manifest.keyid} is not pinned: nothing is trusted.`);
  if (manifest.product !== product) throw fail("signature", `The package is for ${manifest.product}, not ${product}.`);
  const digest = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", pkg)))
    .map((b) => b.toString(16).padStart(2, "0")).join("");
  if (digest !== manifest.sha256) throw fail("signature", "The package's SHA-256 is not the manifest's.");
  if (pkg.byteLength !== manifest.size) throw fail("signature", "The package size is not the manifest's.");
  if (firmware) {
    const a = String(firmware).split(".").map(Number), b = String(manifest.min_firmware).split(".").map(Number);
    for (let i = 0; i < Math.max(a.length, b.length); i++)
      if ((a[i] || 0) < (b[i] || 0)) throw fail("signature", `The package needs firmware ${manifest.min_firmware}.`);
  }
  const unsigned = Object.fromEntries(Object.entries(manifest).filter(([k]) => k !== "sig"));
  let ok = false;
  try {
    const key = await crypto.subtle.importKey("raw", hexBytes(pub), { name: "Ed25519" }, false, ["verify"]);
    ok = await crypto.subtle.verify({ name: "Ed25519" }, key, hexBytes(manifest.sig),
                                     new TextEncoder().encode(canonJson(unsigned)));
  } catch (e) {
    throw fail("signature", `WebCrypto cannot check Ed25519 here: ${e.message}`);
  }
  if (!ok) throw fail("signature", `The manifest is not signed by ${manifest.keyid}.`);
  return { keyid: manifest.keyid, version: manifest.version, sha256: digest };
}

// ---------------------------------------------------------------------------------------------
// The transport gate. A package is verified once, into an opaque authorisation that binds the
// exact bytes, the manifest, the product and the chip. install(), write() and resume() each
// consume an authorisation (one package, one transfer) and serve the private copy it holds, so
// the bytes that were verified are the bytes that leave the host, whatever the caller still has.
//
// Codes: signature (manifest, signature, product, version), image (not bytes, empty, changed after
// it was verified), chip (no, unknown or read-only JEDEC, or a different one now), proof (no,
// wrong or already used authorisation), model (a different unit answered).

const RELEASE_FIELDS = ["format", "product", "version", "min_firmware", "sha256", "size", "keyid", "sig"];
const PROOFS = new WeakMap();

function toBytes(x) {
  if (x instanceof Uint8Array) return x.slice();
  if (ArrayBuffer.isView(x)) return new Uint8Array(x.buffer.slice(x.byteOffset, x.byteOffset + x.byteLength));
  if (x instanceof ArrayBuffer) return new Uint8Array(x.slice(0));
  throw fail("image", "the package is not a byte array: nothing was sent.");
}

function versionString(v, what) {
  const s = String(v === undefined || v === null ? "" : v);
  if (!/^\d+(\.\d+)*$/.test(s))
    throw fail("version", `the manifest's ${what} ${JSON.stringify(s)} is not a version: nothing was sent.`);
  return s;
}

// Verify a .fwsc and everything that authorises writing it. Every refusal happens before the
// first frame, so a caller that ignores the result has still caused nothing to be sent.
export async function requireProof({ pkg, manifest, keys = {}, product = null, firmware = null, provenance = null } = {}) {
  if (!product || typeof product !== "string" || !product.includes("_"))
    throw fail("proof", "no product was named for this transfer: nothing was sent.");
  // The installed version is not optional: without it a downgrade cannot be ruled out, and neither
  // the loader nor the handshake exposes it. Unknown version means refused, not "assume current".
  if (firmware === null || firmware === undefined || firmware === "" || typeof firmware === "object")
    throw fail("version", "the installed firmware version is unknown: a downgrade cannot be ruled out, so nothing was sent.");
  if (!manifest || typeof manifest !== "object")
    throw fail("signature", "No signed manifest: nothing was sent.");
  for (const f of RELEASE_FIELDS)
    if (manifest[f] === undefined) throw fail("signature", `The manifest has no ${f}: nothing was sent.`);
  versionString(manifest.version, "version");
  versionString(manifest.min_firmware, "min_firmware");
  if (!Number.isInteger(manifest.size) || manifest.size <= 0)
    throw fail("signature", "The manifest's size is not a byte count: nothing was sent.");
  const bytes = toBytes(pkg);
  if (bytes.length <= 960) throw fail("image", "the package is too short to hold an image: nothing was sent.");
  const model = product.split("_")[0];
  const checked = await verifyManifest(bytes, manifest, keys, { product: model, firmware });
  if (productOf(bytes) !== product)                             // the package's own identity, once it is authenticated
    throw fail("proof", `the package identifies as ${productOf(bytes)}, not ${product}: nothing was sent.`);
  if (manifest.sha256 !== checked.sha256 || manifest.size !== bytes.length)
    throw fail("signature", "The manifest does not describe this package: nothing was sent.");
  const image = logicalImage(bytes);
  if (!image.length) throw fail("image", "the package holds no image: nothing was sent.");
  // Provenance is not optional: the bytes must be the artefact a clean build of a named commit
  // produced (or the pinned vendor file), the record must name this same package, and the image it
  // is bound to must be the one derived from these bytes. A missing, dirty or foreign record is
  // refused exactly like a bad signature, before any frame.
  const prov = await requireProvenance(provenance, product, image);
  if (prov.sha256 !== checked.sha256)
    throw fail("provenance", "the build record names another package than the signed manifest: nothing was sent.");
  // The signature must cover the record, not just the bytes: without this, anyone who can swap the
  // package can swap the record too, and the record would still look clean. A release manifest with
  // no build_sha256, or one naming another record, is refused exactly like a bad signature.
  if (prov.kind === "build") {
    if (!/^[0-9a-f]{64}$/.test(String(manifest.build_sha256 || "")))
      throw fail("provenance", "the signed manifest does not name the build record: nothing was sent.");
    if (manifest.build_sha256 !== prov.recordSha256)
      throw fail("provenance", "the signed manifest names another build record: nothing was sent.");
  }
  // No chip field here: the chip is identified by admit() on the live link, never by the caller.
  const proof = Object.freeze({ product, model, version: checked.version, keyid: checked.keyid,
    sha256: checked.sha256, size: bytes.length, imageSize: image.length,
    build: prov.kind === "build" ? prov.commit : null });
  PROOFS.set(proof, { image, used: false });
  return proof;
}

// Consume an authorisation and return the image to serve. Nothing is sent while this throws, and
// the image it returns is the verified private copy: a later change to the caller's buffer cannot
// reach the device.
async function admit(proof, pkg, product) {
  const rec = proof && PROOFS.get(proof);
  if (!rec) throw fail("proof", "this transfer has no verified package: nothing was sent.");
  if (rec.used) throw fail("proof", "this authorisation was already used for another transfer: nothing was sent.");
  if (product !== proof.product)
    throw fail("proof", `the authorisation is for ${proof.product}, not ${product}: nothing was sent.`);
  const bytes = toBytes(pkg);                                 // whatever the caller passed this time
  if (bytes.length !== proof.size || (await sha256hex(bytes)) !== proof.sha256)
    throw fail("image", "the package is not the one that was verified: nothing was sent.");
  rec.used = true;                                           // one package, one transfer
  return { image: rec.image, link: null, chip: null };
}

// Bind an admission to the link it will transmit on. The transport identifies the chip itself here,
// with two 0x12 replies that carry a fresh session token each: nothing the caller holds can stand in
// for that, an identification from an earlier connection is never reused, and #serve() refuses a link
// other than the one this function bound.
async function identify(pass, link) {
  if (!pass || !(pass.link === null) || !link || typeof link.send !== "function" || typeof link.read !== "function")
    throw fail("chip", "this transfer has no open link to identify: nothing was sent.");
  const chip = await probeChip(link, 1500);
  requireChip(chip);
  pass.link = link;
  pass.chip = chip;
  return pass;
}
