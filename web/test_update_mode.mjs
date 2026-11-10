// SPDX-License-Identifier: GPL-3.0-only
// The update-mode path: what the installer does when the program answering is an update loader.
//
// Two facts drive every case here, both from this repository's own firmware:
//   * the loader is built with FELUCCA_LOADER=1, which compiles 0x12 out (firmware/src/ota.c,
//     docs/IDENTIFICATION.md), so it cannot identify the chip;
//   * a stock unit's own update mode is a different program ("ota-FM-1_015"), which never agreed to
//     take a Felucca image.
// So the chip gate on the loader link is the loader's own check (it refuses a part the firmware does
// not write before erasing anything) or the same attempt's app-link reading -- and a foreign loader
// must be refused before a single write frame. Every case asserts the frames, not the intention.
import assert from "node:assert";
import crypto from "node:crypto";
import fs from "node:fs";
import { Updater, requireProof, pack7, isOurLoader } from "./fm1ota.js";
import { logicalImage, productOf } from "./fm1pkg.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

function unpack7(bytes) {
  const out = []; let acc = 0, nb = 0;
  for (const v of bytes) { acc |= v << nb; nb += 7; while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } }
  return out;
}
function crc16(a) {
  let c = 0;
  for (const x of a) { c ^= x << 8; for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
  return c & 0xFFFF;
}

/* A unit that can be either program: the app (answers 0x12, stages the loader) or a loader (no 0x12
 * at all, or one that answers a part this installer must refuse). */
class Unit {
  constructor(image, { identity, nor = true, jedec = 0x856014, writes = 3 } = {}) {
    this.image = image; this.nor = nor; this.jedec = jedec; this.writes = writes;
    this.frames = 0; this.upgrades = 0; this.queries = 0; this.served = 0; this.session = 0x7000;
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(identity, identity.startsWith("ota-") ? "Felucca Update" : "Felucca");
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  qry12() {
    this.queries++;
    if (!this.nor) return;                                  // a loader: the query code is not in the build
    const b = new Array(24).fill(0);
    b[0] = 1;
    b[2] = (this.jedec >> 16) & 0xFF; b[3] = (this.jedec >> 8) & 0xFF; b[4] = this.jedec & 0xFF;
    const cap = 0x100000;
    b[5] = cap & 0xFF; b[6] = (cap >> 8) & 0xFF; b[7] = (cap >> 16) & 0xFF; b[8] = 0;
    b[9] = 0; b[10] = 1; b[11] = 0; b[12] = 0x10; b[13] = 0; b[14] = 0; b[15] = 1; b[16] = 0;
    this.session = (this.session + 1) >>> 0;
    b[17] = this.session & 0xFF; b[18] = (this.session >> 8) & 0xFF;
    b[19] = (this.session >> 16) & 0xFF; b[20] = (this.session >>> 24) & 0xFF;
    b[21] = crc16(b.slice(0, 21)) & 0xFF; b[22] = (crc16(b.slice(0, 21)) >> 8) & 0xFF;
    const c = (~b.reduce((x, y) => x + y, 0)) & 0xFF;
    this.tx([0xF0, ...pack7([0, 0x59, 0x12, 24, 0, 0, ...b, c]), 0xF7]);
  }
  rx(d) {
    this.frames++;
    if (d.length > 3 && d[0] === 0xF0 && d.at(-1) === 0xF7) {
      let u = null;
      try { u = unpack7(d.slice(1, -1)); } catch (_) { u = null; }
      if (u && u[0] === 0 && u[1] === 0x59 && u[2] === 0x12) return this.qry12();
    }
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      this.upgrades++;
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: this.writes }, (_, k) => [k * 512, 512]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = this.waiting;
      const fin = addr >= 0xE0000000;
      const got = u.slice(14, 14 + (fin ? 8 : len));
      const want = fin ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.served = -1e9;                  // a wrong byte, not a slow unit
      this.waiting = null;
      if (this.served > -1e8) this.served++;
      if (addr === 0xE0000000) setTimeout(() => this.boot("ota-FM-1_965", "Felucca Update"), 200);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.after, "Felucca"), 200);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

/* the real release chain: the package people install, and the record/provenance the page needs */
/* The package people install: the release fixture when this checkout carries one, otherwise whatever
 * it built (CI builds one before running the tools). No fixture, no case: the run says so and stops. */
function findPackage() {
  const cands = ["tests/fixtures/felucca-v1.9.0.fwsc"];
  for (const dir of ["build", "tests/fixtures"]) {
    let names = [];
    try { names = fs.readdirSync(dir); } catch (_) { continue; }
    for (const n of names) if (n.endsWith(".fwsc")) cands.push(`${dir}/${n}`);
  }
  let dirs = [];
  try { dirs = fs.readdirSync("build").filter((n) => n.startsWith("release-")); } catch (_) {}
  for (const d of dirs) {
    let names = [];
    try { names = fs.readdirSync(`build/${d}`); } catch (_) { continue; }
    for (const n of names) if (n.endsWith(".fwsc")) cands.push(`build/${d}/${n}`);
  }
  const ok = cands.filter((p) => { try { return fs.statSync(p).size > 0; } catch (_) { return false; } });
  /* a package this project built beats a scratch file left in build/ */
  ok.sort((a, b) => (/felucca/.test(b) ? 1 : 0) - (/felucca/.test(a) ? 1 : 0));
  return ok[0] || null;
}
const pkgPath = findPackage();
if (!pkgPath) {
  console.log("skip: no package in this checkout (tests/fixtures or build/felucca-*.fwsc)");
  process.exit(0);
}
const pkg = fs.readFileSync(pkgPath);
/* the device reads the LOGICAL image of the package (fm1pkg.js), not the container file */
const image = logicalImage(pkg);
/* the identity the package itself carries: the page must name the package it was given, and this
 * test must pass on any release, whichever number docs/IDENTITY.md has it out on */
const PRODUCT = productOf(pkg);
const ctxFor = (extra = {}) => ({ firmware: "15", ...extra });

ok(isOurLoader({ model: "ota-FM-1", text: "ota-FM-1_965", version: 965 }) === true &&
   isOurLoader({ model: "ota-FM-1", text: "ota-FM-1_900", version: 900 }) === true &&
   isOurLoader({ model: "ota-FM-1", text: "ota-FM-1_919", version: 919 }) === false &&
   isOurLoader({ model: "ota-FM-1", text: "ota-FM-1_015", version: 15 }) === false &&
   isOurLoader({ model: "FM-1", text: "FM-1_965", version: 965 }) === false,
   "the loader's own identity (965, 900 before docs/IDENTITY.md) tells this project's loader from a unit's own update mode");

const fast = { loaderDelay: 60, rebootDelay: 60 };
const run = async (unit, ctx) => {
  const up = new Updater(unit.access, () => {}, fast);
  const proof = await requireProof({ ...ctx, pkg, product: PRODUCT });
  return up.install(pkg, PRODUCT, () => {}, { ...ctx, proof }).catch((e) => e);
};

/* 1. The real-world fix: a unit running QG updates through a loader that cannot answer 0x12.
 *    The chip fact is the app link's own 0x12 from the same attempt, and the write completes. */
{
  const u = new Unit(image, { identity: PRODUCT });
  u.after = PRODUCT;
  const before = u.upgrades;
  const res = await run(u, ctxFor());
  ok(res === PRODUCT, `a QG unit updates through a loader with no chip query (result ${JSON.stringify(res && res.message || res)})`);
  ok(u.upgrades === before + 2, "the app took UPGRADE once and the loader took it once: two separate programs");
  ok(u.served > 0 && u.served < 1e8, `every byte the device asked for matched the package (${u.served} requests)`);
}

/* 2. The same unit, but the page opens with the loader already up (an interrupted install): with no
 *    chip fact on the host, this project's loader checks the chip itself (loader.c), so it resumes. */
{
  const u = new Unit(image, { identity: "ota-FM-1_965", nor: false });
  u.after = PRODUCT;
  const res = await run(u, ctxFor());
  ok(res === PRODUCT, `a resumed install with no chip record completes (${JSON.stringify(res && res.message || res)})`);
  ok(u.upgrades === 1, "...the loader took UPGRADE once");
}

/* 3. A stock unit's own update mode is not our loader: no image may go there. */
{
  const u = new Unit(image, { identity: "ota-FM-1_015", nor: false });
  const err = await run(u, ctxFor());
  ok(err && err.code === "foreignloader", `the unit's own update mode is refused as "foreignloader" (${err && err.code})`);
  ok(u.upgrades === 0 && u.served === 0, "...with zero UPGRADE and zero bytes offered to it");
}

/* 4. A stock unit running its app installs: it cannot answer 0x12, its updater starts this package's
 *    loader, and the loader checks the chip itself (firmware/loader/loader.c). */
{
  const u = new Unit(image, { identity: "FM-1_015", nor: false });
  u.after = PRODUCT;
  const res = await run(u, ctxFor());
  ok(res === PRODUCT, `a stock unit installs (${JSON.stringify(res && res.message || res)})`);
  ok(u.upgrades === 2, "...the stock app took UPGRADE once and the loader took it once");
}

/* 5. An answered 0x12 decides, even when it is a part this installer does not write: nothing on the
 *    host can paper over a chip that answered and was refused. */
{
  const u = new Unit(image, { identity: "ota-FM-1_965", nor: true, jedec: 0xC84014 });   // read-only GD25Q80
  const err = await run(u, ctxFor());
  ok(err && err.code === "chip", `a loader that answers with a read-only part is refused (${err && err.code})`);
  ok(u.upgrades === 0, "...and nothing on the host stood in for the answer: no UPGRADE");
}

console.log(`update mode: ${n} checks, 0 FAILED`);
