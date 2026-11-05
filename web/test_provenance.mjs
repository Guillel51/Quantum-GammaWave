// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The installer must not write a byte until the package has passed every provenance check.
//
// This is the evidence for that sentence, in four layers:
//   1. web/fm1provenance.js refuses each way a package or its record can be wrong, and the token it
//      hands out cannot be forged or reused for other bytes;
//   2. Updater.install/write/resume refuse without that token *before* they find a device, so the
//      simulated FM-1 below receives zero frames in every invalid case (it counts them);
//   3. the page reaches no write call without the verdict (static check of index_pkg.html);
//   4. web/make_site.py refuses to publish a package that contradicts its build record.
//
//   node web/test_provenance.mjs            (from the repo root; skips the site build without a package)

import { execFileSync } from "node:child_process";
import { createHash, generateKeyPairSync, sign as edSign } from "node:crypto";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { requireProvenance, verifyProvenance, verifyVendorPackage } from "./fm1provenance.js";
import { Updater, pack7, unpack7 } from "./fm1ota.js";
import { productOf } from "./fm1pkg.js";

const HERE = fileURLToPath(new URL(".", import.meta.url));
const ROOT = join(HERE, "..");
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(76)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const ref = async (fn) => { try { return { value: await fn() }; } catch (e) { return { error: e }; } };
const refused = async (fn, code = "provenance") => {
  const { error } = await ref(fn);
  return !!error && error.code === code;
};

const hex = async (bytes) =>
  [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map((b) => b.toString(16).padStart(2, "0")).join("");

// the package and its record: this test plays the build system, hashing what it hands over
const PKG = Uint8Array.from({ length: 2048 }, (_, i) => (i * 13 + 5) & 0xFF);
const IMAGE = Uint8Array.from({ length: 1024 }, (_, i) => (i * 7 + 1) & 0xFF);
const COMMIT = "0123456789abcdef0123456789abcdef01234567";

async function record(pkg = PKG, over = {}, art = {}) {
  const sha = await hex(pkg);
  return {
    schema: "qg1-build-1",
    version: "v1.9.0",
    commit: COMMIT,
    dirty: [],
    identity: "FM-1_982",
    config: { FELUCCA_FLASH: "default" },
    toolchain: { clang_sha256: "a".repeat(64), path: "/opt/toolchain" },
    sdk: { commit: "b".repeat(40), files: {}, path: "/opt/sdk" },
    artifacts: {
      "felucca.bin": { bytes: 1024, sha256: "c".repeat(64) },
      "felucca.fwsc": { bytes: pkg.length, sha256: sha, ...art },
    },
    emulator: { schema: "qg1-emu-1", commit: COMMIT.slice(0, 12), version: "v1.9.0",
                wasm_bytes: 4096, wasm_sha256: "d".repeat(64) },
    ...over,
  };
}

async function valid() {
  return verifyProvenance(PKG, await record(), { product: "FM-1_982", version: "v1.9.0", image: IMAGE });
}

// ---- 1. the record and the bytes ---------------------------------------------------------------
{
  const t = await valid();
  ok(t.schema === "qg1-provenance-1" && t.identity === "FM-1_982" && t.version === "v1.9.0" && t.commit === COMMIT,
     "provenance: a clean record and the package it names pass");
  ok((await hex(PKG)) === t.sha256 && t.imageSha256 === (await hex(IMAGE)),
     "provenance: the token carries the package's and the image's digests");

  const base = await record();
  const cases = [
    ["no record at all", () => verifyProvenance(PKG, null, { product: "FM-1_982" })],
    ["a malformed record (not an object)", () => verifyProvenance(PKG, [base], {})],
    ["another schema", () => verifyProvenance(PKG, { ...base, schema: "qg1-release-1" }, {})],
    ["a missing field", () => verifyProvenance(PKG, { ...base, commit: undefined }, {})],
    ["a short commit", () => verifyProvenance(PKG, { ...base, commit: "0123456" }, {})],
    ["a malformed version", () => verifyProvenance(PKG, { ...base, version: "banana" }, {})],
    ["a malformed identity", () => verifyProvenance(PKG, { ...base, identity: "FM1-982" }, {})],
    ["a dirty tree", () => verifyProvenance(PKG, { ...base, dirty: ["firmware/src/felucca.c"] }, {})],
    ["a dirty list that is not a list", () => verifyProvenance(PKG, { ...base, dirty: "no" }, {})],
    ["no artefacts", () => verifyProvenance(PKG, { ...base, artifacts: null }, {})],
    ["no package entry", () => verifyProvenance(PKG, { ...base, artifacts: { "felucca.bin": base.artifacts["felucca.bin"] } }, {})],
    ["no firmware image entry", () => verifyProvenance(PKG, { ...base, artifacts: { "felucca.fwsc": base.artifacts["felucca.fwsc"] } }, {})],
    ["a zero size", () => verifyProvenance(PKG, { ...base, artifacts: { ...base.artifacts, "felucca.fwsc": { bytes: 0, sha256: "e".repeat(64) } } }, {})],
    ["a SHA-256 that is not hex", () => verifyProvenance(PKG, { ...base, artifacts: { ...base.artifacts, "felucca.fwsc": { bytes: PKG.length, sha256: "z".repeat(64) } } }, {})],
    ["an empty package", () => verifyProvenance(new Uint8Array(0), base, {})],
    ["another package of the same size", () => verifyProvenance(PKG.map((b) => b ^ 1), base, {})],
    ["a truncated download", () => verifyProvenance(PKG.slice(0, PKG.length - 1), base, {})],
    ["a package the record is not for", () => verifyProvenance(PKG, { ...base, artifacts: { ...base.artifacts, "felucca.fwsc": { bytes: PKG.length, sha256: "f".repeat(64) } } }, {})],
    ["a device the record is not for", () => verifyProvenance(PKG, base, { product: "FM-1_900" })],
    ["a version label from another build", () => verifyProvenance(PKG, base, { version: "v1.8.9" })],
    ["a version label naming another commit", () => verifyProvenance(PKG, base, { version: "v1.9.0-deadbee" })],
    ["an emulator from another commit", () => verifyProvenance(PKG, { ...base, emulator: { ...base.emulator, commit: "f".repeat(12) } }, {})],
    ["an emulator from another version", () => verifyProvenance(PKG, { ...base, emulator: { ...base.emulator, version: "v1.8.9" } }, {})],
    ["an emulator with no wasm digest", () => verifyProvenance(PKG, { ...base, emulator: { ...base.emulator, wasm_sha256: "nope" } }, {})],
    ["no emulator record", () => verifyProvenance(PKG, { ...base, emulator: undefined }, {})],
    ["a second emulator manifest that disagrees", () => verifyProvenance(PKG, base, { emulator: { ...base.emulator, wasm_sha256: "9".repeat(64) } })],
  ];
  for (const [what, fn] of cases) ok(await refused(fn), `provenance refuses ${what}`);

  const v = await ref(() => verifyProvenance(PKG.slice(0, PKG.length - 1), base, {}));
  ok(/incomplete or the file is not this build/.test(v.error.message), "provenance: the size error names the interrupted download");

  // the label may name the commit it was built from, and only that one
  const labelled = await ref(() => verifyProvenance(PKG, base, { version: `v1.9.0-${COMMIT.slice(0, 7)}` }));
  ok(!labelled.error, "provenance: a label carrying the record's own commit passes");
  ok(await refused(() => verifyProvenance(PKG, base, { version: `v1.9.0-${COMMIT.slice(0, 7)}1` })),
     "provenance: a label with one extra commit digit is refused");
}

// ---- 2. the token -------------------------------------------------------------------------------
{
  const t = await valid();
  ok(!!(await ref(() => requireProvenance(t, "FM-1_982", IMAGE))).value, "token: the verified token is accepted");
  ok(await refused(() => requireProvenance(null, "FM-1_982", IMAGE)), "token: no token -> refused");
  ok(!!(await ref(() => requireProvenance({ ...t }, "FM-1_982", IMAGE))).value,
     "token: a shallow copy still carries the verdict (same facts), and is accepted");
  const roundtrip = JSON.parse(JSON.stringify(t));
  ok(await refused(() => requireProvenance(roundtrip, "FM-1_982", IMAGE)),
     "token: a JSON round-trip (the symbol is lost) is not a verdict");
  ok(await refused(() => requireProvenance({ schema: "qg1-provenance-1", identity: "FM-1_982" }, "FM-1_982", IMAGE)),
     "token: a hand-written object with the same fields is refused");
  ok(await refused(() => requireProvenance(t, "FM-1_900", IMAGE)), "token: another device identity is refused");
  ok(await refused(() => requireProvenance(t, "FM-1_982", IMAGE.map((b) => b ^ 0xFF))),
     "token: an image other than the one verified is refused");
  const vbytes = Uint8Array.from([1, 2, 3]), vsha = await hex(vbytes);
  const vendor = await verifyVendorPackage(vbytes, { product: "FM-1_015", sha256: vsha, size: 3 });
  ok(vendor.kind === "vendor" && !vendor.commit && !vendor.version, "token: a vendor package is marked as a pin, not a build");
  ok(await refused(() => verifyVendorPackage(vbytes, { product: "FM-1_015", sha256: "0".repeat(64), size: 3 })),
     "vendor: a file that is not the pinned official one is refused");
  ok(await refused(() => verifyVendorPackage(vbytes, { product: "FM-1_015", sha256: vsha, size: 4 })),
     "vendor: a truncated vendor file is refused");
}

// ---- 3. zero frames without a verdict -----------------------------------------------------------
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const crc16 = (d, c = 0) => {
  for (const x of d) {
    c ^= x << 8;
    for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c & 0xFFFF;
};

/* a .fwsc whose logical image is `img` (the same construction test_web.mjs uses) */
function packageOf(img, identity) {
  const pkg = new Uint8Array(20 * 48 + img.length - 20 * 47);
  for (let i = 0; i < 20; i++) {
    pkg.set(img.subarray(i * 47, i * 47 + 47), i * 48);
    const c = identity.charCodeAt(i);
    pkg[i * 48 + 47] = Number.isNaN(c) ? 0x7D : (c + i + 1) & 0xFF;
  }
  pkg.set(img.subarray(20 * 47), 20 * 48);
  return pkg;
}

/* an FM-1 on WebMIDI that counts every frame the page sends, and complains loudly if a write starts */
class WatchedFM1 {
  constructor(identity = "FM-1_982") { this.set(identity); }
  set(identity) {
    this.identity = identity; this.sent = []; this.session = 0xA000;
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name: "Felucca FM-1", state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name: "Felucca FM-1", state: "connected", open: async () => {},
      send: (d) => { const a = Array.from(d); this.sent.push(a); this.reply(a); } };
    this.access = { inputs: new Map([[this.input.id, this.input]]), outputs: new Map([[this.output.id, this.output]]) };
  }
  get frames() { return this.sent.length; }
  get writes() { return this.sent.filter((a) => eq(a, UPGRADE)).length; }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 0); }
  reply(d) {
    let u = null;
    try { u = d.length > 3 && d[0] === 0xF0 && d.at(-1) === 0xF7 ? unpack7(d.slice(1, -1)) : null; } catch (_) { u = null; }
    if (u && u[0] === 0 && u[1] === 0x59 && u[2] === 0x12) {
      const b = new Array(24).fill(0);
      b[0] = 1; b[2] = 0x85; b[3] = 0x60; b[4] = 0x14;                     // P25Q80H, 1 MiB
      b[5] = 0x00; b[6] = 0x00; b[7] = 0x10; b[8] = 0; b[10] = 1; b[12] = 0x10; b[15] = 1;
      this.session = (this.session + 1) >>> 0;
      b[17] = this.session & 0xFF; b[18] = (this.session >> 8) & 0xFF;
      const s = crc16(b.slice(0, 21)); b[21] = s & 0xFF; b[22] = (s >> 8) & 0xFF;
      this.tx([0xF0, ...pack7([0, 0x59, 0x12, 24, 0, 0, ...b, (~b.reduce((x, y) => x + y, 0)) & 0xFF]), 0xF7]);
    } else if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      // one impossible read request: serve() refuses it at once, so the control stays fast
      const q = [0, 0x59, 0x30, 0, 0, 0, 0, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0x7F, 0];
      let s = 0;
      for (let i = 6; i < 14; i++) s += q[i];
      q.push(~s & 0xFF);
      this.tx([0xF0, ...pack7(q), 0xF7]);
    }
  }
}
/* an ephemeral key signs the test release: no production key exists or is created here */
const TEST_KEY = generateKeyPairSync("ed25519");
const PINNED = { "test-1": Buffer.from(TEST_KEY.publicKey.export({ format: "jwk" }).x, "base64url").toString("hex") };
const canon = (v) => v === null || typeof v !== "object" ? JSON.stringify(v)
  : Array.isArray(v) ? "[" + v.map(canon).join(",") + "]"
  : "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canon(v[k])).join(",") + "}";
const sha256 = (b) => createHash("sha256").update(b).digest("hex");
// the signed release manifest names the build record it covers: build_sha256 is inside the signature
const release = (pkg, build = recordFor()) => {
  const unsigned = { format: "qg1-release-1", product: "FM-1", version: "9.9.9", min_firmware: "1.0.0",
    sha256: sha256(pkg), size: pkg.length, created: "2026-11-05T00:00:00Z", keyid: "test-1",
    ...(build ? { build_sha256: sha256(Buffer.from(canon(build))) } : {}) };
  return { ...unsigned, sig: edSign(null, Buffer.from(canon(unsigned)), TEST_KEY.privateKey).toString("hex") };
};

const IDENTITY = "FM-1_982";
const IMG = Uint8Array.from({ length: 0x2000 }, (_, i) => (i * 7) & 0xFF);
const PKG2 = packageOf(IMG, IDENTITY);
const timing = { loaderDelay: 5, loaderWait: 200, rebootDelay: 5, rebootWait: 200 };

const recordFor = (over = {}) => ({
  schema: "qg1-build-1", version: "v9.9.9", commit: COMMIT, dirty: [], identity: IDENTITY,
  config: { FELUCCA_FLASH: "default" },
  toolchain: { clang_sha256: "a".repeat(64), path: "/opt/toolchain" },
  sdk: { commit: "b".repeat(40), files: {}, path: "/opt/sdk" },
  artifacts: { "felucca.bin": { bytes: 1024, sha256: "c".repeat(64) },
               "felucca.fwsc": { bytes: PKG2.length, sha256: sha256(PKG2) } },
  emulator: { schema: "qg1-emu-1", commit: COMMIT.slice(0, 12), version: "v9.9.9",
              wasm_bytes: 4096, wasm_sha256: "d".repeat(64) },
  ...over,
});

async function ctxFor(over = {}) {
  const build = recordFor();
  const provenance = await verifyProvenance(PKG2, build, { product: IDENTITY, image: IMG });
  return { manifest: release(PKG2, build), keys: PINNED, firmware: "1.8.9", provenance, ...over };
}

async function installWith(ctx, dev = new WatchedFM1()) {
  const err = (await ref(() => new Updater(dev.access, null, timing).install(PKG2, IDENTITY, null, ctx))).error;
  return { err, dev };
}

{
  const good = await ctxFor();
  const foreign = await ctxFor({ provenance: await verifyProvenance(PKG2, {
      schema: "qg1-build-1", version: "v9.9.9", commit: COMMIT, dirty: [], identity: "FM-1_901",
      config: {}, toolchain: {}, sdk: {}, artifacts: { "felucca.bin": { bytes: 1, sha256: "c".repeat(64) },
        "felucca.fwsc": { bytes: PKG2.length, sha256: sha256(PKG2) } },
      emulator: { schema: "qg1-emu-1", commit: COMMIT.slice(0, 12), version: "v9.9.9", wasm_bytes: 1, wasm_sha256: "d".repeat(64) },
    }, { product: "FM-1_901" }) });
  const otherBytes = await ctxFor({ provenance: await verifyProvenance(packageOf(IMG.map((b) => b ^ 1), IDENTITY), {
      schema: "qg1-build-1", version: "v9.9.9", commit: COMMIT, dirty: [], identity: IDENTITY,
      config: {}, toolchain: {}, sdk: {}, artifacts: { "felucca.bin": { bytes: 1, sha256: "c".repeat(64) },
        "felucca.fwsc": { bytes: PKG2.length, sha256: sha256(packageOf(IMG.map((b) => b ^ 1), IDENTITY)) } },
      emulator: { schema: "qg1-emu-1", commit: COMMIT.slice(0, 12), version: "v9.9.9", wasm_bytes: 1, wasm_sha256: "d".repeat(64) },
    }, { product: IDENTITY }) });

  for (const [what, ctx] of [["no verdict", { ...good, provenance: null }], ["a forged verdict", { ...good, provenance: { identity: IDENTITY } }],
                              ["a verdict for another device", foreign],
                              ["a verdict for other bytes", otherBytes],
                              ["a release naming another build record", { ...good, manifest: release(PKG2, recordFor({ commit: "f".repeat(40) })) }],
                              ["a release that names no record", { ...good, manifest: release(PKG2, null) }]]) {
    const { err, dev } = await installWith(ctx);
    ok(err && err.code === "provenance" && dev.frames === 0 && dev.writes === 0,
       `updater: ${what} -> 'provenance', ${dev.frames} frames, ${dev.writes} writes`);
  }

  // the same for resume(): the loader is already waiting, and still nothing may be written
  const dev = new WatchedFM1("ota-FM-1_982");
  const err = (await ref(() => new Updater(dev.access, null, timing).resume(PKG2, null, IDENTITY, { ...good, provenance: null }))).error;
  ok(err && err.code === "provenance" && dev.frames === 0 && dev.writes === 0,
     `updater: resume() without a verdict -> 'provenance', ${dev.frames} frames`);

  // write() is the last door and refuses on its own, not only through install()/resume()
  const dev2 = new WatchedFM1("ota-FM-1_982");
  const fake = { id: { model: "ota-" + IDENTITY.split("_")[0], text: "ota-" + IDENTITY },
                 link: { send: (b) => { dev2.output.send(b); return true; }, close: () => {}, read: async () => null } };
  const e2 = (await ref(() => new Updater(dev2.access, null, timing).write(fake, PKG2, null, { ...good, provenance: null, product: IDENTITY }))).error;
  ok(e2 && e2.code === "provenance" && dev2.frames === 0, "updater: write() without a verdict sends nothing");

  // and the control: a valid verdict does open the door (otherwise the checks above prove nothing)
  const { err: e3, dev: dev3 } = await installWith(good);
  ok(e3 && e3.code === "badreq" && dev3.writes === 1,
     `updater: a valid verdict opens the door (1 UPGRADE sent, then '${e3 && e3.code}'; frames ${dev3.frames})`);
}

// ---- 4. the page reaches no write without the verdict -------------------------------------------
{
  const html = readFileSync(join(HERE, "index_pkg.html"), "utf8");
  ok(/if \(!v\.build\)/.test(html) && html.indexOf("await verifyProvenance(pkg, v.build") > 0,
     "page: the build verdict is obtained, and a package with no record is refused");
  ok(/releaseProvenance = await verifyProvenance/.test(html) && /log\(`provenance:/.test(html),
     "page: the verdict is stored, and its refusal is logged");
  ok(/!preflightOk\(\) \|\| !releaseOk \|\| !releaseProvenance/.test(html),
     "page: INSTALL is disabled until the record is verified too");
  const ctx = html.slice(html.indexOf("const ctxFor"), html.indexOf("const ctxFor") + 220);
  ok(/manifest: releaseManifest/.test(ctx) && /provenance: releaseProvenance/.test(ctx),
     "page: every ctx carries the build verdict next to the manifest");
  const installs = html.match(/up\.install\([^)]*\)/g) || [];
  const resumes = html.match(/up\.resume\([^)]*\)/g) || [];
  ok(installs.length === 3 && installs.every((s) => /,\s*(ctxFor\(|\{ manifest)/.test(s)),
     `page: every install() call passes the ctx (${installs.length})`);
  ok(resumes.length === 3 && resumes.every((s) => /,\s*(ctxFor\(|\{ manifest)/.test(s)),
     `page: every resume() call passes the ctx (${resumes.length})`);
  ok(!/up\.write\(/.test(html), "page: write() is never called directly");
}

// ---- 5. make_site.py refuses a site that contradicts its record ---------------------------------
{
  const pkg = join(ROOT, "build/felucca.fwsc");
  if (!existsSync(pkg)) {
    console.log("make_site: no build/felucca.fwsc here, site checks skipped (a web-only checkout)");
  } else {
    const raw = readFileSync(pkg);
    const dir = mkdtempSync(join(tmpdir(), "qg1-site-"));
    const prod = productOf(raw);
    const record = {
      schema: "qg1-build-1", version: "v9.9.9", commit: COMMIT, dirty: [], identity: prod,
      config: {}, toolchain: {}, sdk: {},
      artifacts: { "felucca.bin": { bytes: 1, sha256: "c".repeat(64) },
                   "felucca.fwsc": { bytes: raw.length, sha256: sha256(raw) } },
      emulator: { schema: "qg1-emu-1", commit: COMMIT.slice(0, 12), version: "v9.9.9", wasm_bytes: 1, wasm_sha256: "d".repeat(64) },
    };
    const buildDir = join(dir, "build");
    execFileSync("mkdir", ["-p", buildDir]);
    writeFileSync(join(buildDir, "felucca.fwsc"), raw);
    const run = (manifest, label, out) => {
      writeFileSync(join(buildDir, "manifest.json"), JSON.stringify(manifest));
      let code = 0, err = "";
      try { execFileSync("python3", [join(ROOT, "web/make_site.py"), join(buildDir, "felucca.fwsc"), label, out], { stdio: "pipe" }); }
      catch (e) { code = e.status; err = String(e.stderr); }
      return { code, err };
    };
    const bad = JSON.parse(JSON.stringify(record));
    bad.artifacts["felucca.fwsc"].sha256 = "0".repeat(64);
    let r = run(bad, "v9.9.9", join(dir, "site-bad"));
    ok(r.code !== 0 && /refusing to publish it/.test(r.err) && !existsSync(join(dir, "site-bad/webapp/installer/index.html")),
       "make_site: a record whose SHA-256 is not the package's refuses to build the site");
    const dirty = { ...JSON.parse(JSON.stringify(record)), dirty: ["firmware/src/fx.c"] };
    r = run(dirty, "v9.9.9", join(dir, "site-dirty"));
    ok(r.code !== 0 && /dirty tree/.test(r.err), "make_site: a record built from a dirty tree is refused");
    r = run(record, `v9.9.9-${"f".repeat(7)}`, join(dir, "site-label"));   // the label names another commit
    ok(r.code !== 0 && /refusing to publish it/.test(r.err), "make_site: a label that names another commit than the record is refused");
    r = run(record, `v9.9.9-${COMMIT.slice(0, 7)}`, join(dir, "site"));
    const page = join(dir, "site/webapp/installer/index.html");
    const meta = r.code === 0 && existsSync(page)
      ? JSON.parse(/const meta = (\{.*?\});\n/s.exec(readFileSync(page, "utf8"))[1]) : null;
    ok(meta && meta.build && meta.build.commit === COMMIT,
       "make_site: a clean record and its package build the site, with the record inlined" + (r.err ? ` (${r.err.slice(0, 90)})` : ""));
    ok(meta && /Content-Security-Policy/.test(readFileSync(page, "utf8")) && meta.versions[0].build,
       "make_site: the record travels inside the page's CSP-protected script, per version");
    rmSync(dir, { recursive: true, force: true });
  }
}

console.log(failed ? `PROVENANCE TESTS FAILED (${failed})` : "provenance tests passed");
process.exit(failed ? 1 : 0);
