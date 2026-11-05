// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// (v1.9.0) The installer must not write a byte until the package has passed every provenance check.
//
// This is the evidence for that sentence, in three layers:
//   1. web/fm1provenance.js refuses each way a package or its record can be wrong, and the token it
//      hands out cannot be forged or reused for other bytes;
//   2. Updater.install/resume/write refuse without that token *before* they find a device, so the
//      simulated FM-1 below receives zero frames in every invalid case (it counts them);
//   3. the page cannot reach a write call without the verdict (static check of index_pkg.html), and
//      web/make_site.py refuses to build a site whose package contradicts its record.
//
//   node web/test_provenance.mjs            (from the repo root; skips the site build without a package)

import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { requireProvenance, verifyProvenance, verifyVendorPackage } from "./fm1provenance.js";
import { Updater, pack7 } from "./fm1ota.js";

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

  ok(!(await refused(() => verifyProvenance(PKG.slice(0, 100), base, {}))).valueOf() === false, "provenance: the truncated case above is a size mismatch, not a hash match");
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

/* an FM-1 on WebMIDI that counts every frame the page sends, and complains loudly if a write starts */
class WatchedFM1 {
  constructor(identity = "FM-1_982") {
    this.identity = identity; this.sent = [];
    this.input = { id: "i", name: "Felucca FM-1", state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o", name: "Felucca FM-1", state: "connected", open: async () => {},
      send: (d) => { const a = Array.from(d); this.sent.push(a); this.reply(a); } };
    this.access = { inputs: new Map([["i", this.input]]), outputs: new Map([["o", this.output]]) };
  }
  get frames() { return this.sent.length; }
  get writes() { return this.sent.filter((a) => a.length === UPGRADE.length && a.every((b, i) => b === UPGRADE[i])).length; }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 0); }
  reply(d) {
    if (d.length === HS.length && d.every((b, i) => b === HS[i])) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (d.length === UPGRADE.length && d.every((b, i) => b === UPGRADE[i])) {
      // one impossible read request: serve() refuses it at once, so the check stays fast
      const u = [0, 0x59, 0x30, 0, 0, 0, 0, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0x7F, 0];
      let s = 0;
      for (let i = 6; i < 14; i++) s += u[i];
      u.push(~s & 0xFF);
      this.tx([0xF0, ...pack7(u), 0xF7]);
    }
  }
}

const EVIDENCE = { jedec: 0x856014 };
const timing = { loaderDelay: 5, loaderWait: 200, rebootDelay: 5, rebootWait: 200 };

async function installWith(prov, dev = new WatchedFM1()) {
  const err = (await ref(() => new Updater(dev.access, null, timing).install(IMAGE, "FM-1_982", null, EVIDENCE, prov))).error;
  return { err, dev };
}

{
  // every invalid case, through the real Updater: nothing may be sent
  const foreign = await verifyProvenance(PKG, await record(PKG, { identity: "FM-1_900" }), { product: "FM-1_900", image: IMAGE });
  const otherBytes = await verifyProvenance(PKG, await record(), { product: "FM-1_982", image: IMAGE.map((b) => b ^ 1) });
  for (const [what, prov] of [["no verdict", null], ["a forged verdict", { identity: "FM-1_982" }],
                              ["a verdict for another device", foreign],
                              ["a verdict for other bytes", otherBytes]]) {
    const { err, dev } = await installWith(prov);
    ok(err && err.code === "provenance" && dev.frames === 0 && dev.writes === 0,
       `updater: ${what} -> 'provenance', ${dev.frames} frames, ${dev.writes} writes`);
  }

  // the named failures end to end: the verdict is never minted for any of them, and a verdict is the
  // only thing Updater accepts, so the simulated FM-1 sees nothing at all
  const good = await record();
  const broken = [
    ["a truncated download", PKG.slice(0, PKG.length - 1), good, {}],
    ["a wrong hash", PKG, { ...good, artifacts: { ...good.artifacts, "felucca.fwsc": { bytes: PKG.length, sha256: "c".repeat(64) } } }, {}],
    ["a version label from another build", PKG, good, { version: "v1.8.9" }],
    ["a version label naming another commit", PKG, good, { version: "v1.9.0-deadbee" }],
    ["a record for another device", PKG, { ...good, identity: "FM-1_900" }, {}],
    ["a record from a dirty tree", PKG, { ...good, dirty: ["firmware/src/fx.c"] }, {}],
    ["no record at all", PKG, null, {}],
    ["a manifest naming another commit than the emulator", PKG,
      { ...good, emulator: { ...good.emulator, commit: "f".repeat(12) } }, {}],
  ];
  for (const [what, pkgv, recv, optv] of broken) {
    const vv = (await ref(() => verifyProvenance(pkgv, recv, { product: "FM-1_982", image: IMAGE, ...optv }))).value;
    const { err: eb, dev: db } = await installWith(vv ?? null);
    ok(!vv && eb && eb.code === "provenance" && db.frames === 0 && db.writes === 0,
       `updater: ${what} -> no verdict, 'provenance', ${db.frames} frames, ${db.writes} writes`);
  }

  // the same for resume(): the loader is already waiting, and still nothing may be written
  const dev = new WatchedFM1("ota-FM-1_982");
  const err = (await ref(() => new Updater(dev.access, null, timing).resume(IMAGE, null, "FM-1_982", EVIDENCE, null))).error;
  ok(err && err.code === "provenance" && dev.frames === 0 && dev.writes === 0,
     `updater: resume() without a verdict -> 'provenance', ${dev.frames} frames`);

  // write() is the last door and refuses on its own, not only through install()/resume()
  const dev2 = new WatchedFM1("ota-FM-1_982");
  const up = new Updater(dev2.access, null, timing);
  const fake = { link: { send: (b) => { dev2.output.send(b); return true; }, close: () => {} } };
  const e2 = (await ref(() => up.write(fake, IMAGE, null, EVIDENCE, null))).error;
  ok(e2 && e2.code === "provenance" && dev2.frames === 0, "updater: write() without a verdict sends nothing");

  // and the control: a valid verdict does open the door (otherwise the checks above prove nothing)
  const { err: e3, dev: dev3 } = await installWith(await valid());
  ok(e3 && e3.code === "badreq" && dev3.writes === 1,
     `updater: a valid verdict opens the door (1 UPGRADE sent, then '${e3 && e3.code}'; frames ${dev3.frames}, writes ${dev3.writes}, ${e3 && e3.message})`);
}

// ---- 4. the page reaches no write without the verdict -------------------------------------------
{
  const html = readFileSync(join(HERE, "index_pkg.html"), "utf8");
  const prov = html.indexOf("await verifyProvenance(pkg, v.build");
  const ready = html.indexOf("releaseOk = true; provOk = prov;");
  ok(prov > 0 && ready > prov, "page: the build verdict is obtained before INSTALL is enabled");
  ok(/if \(!meta\.build\)|verifyProvenance\(pkg, v\.build/.test(html) && html.includes("log(`provenance:"),
     "page: a package with no build record is refused");
  ok(/if \(!releaseOk \|\| !provOk\)/.test(html), "page: INSTALL refuses without both verdicts");
  const installs = html.match(/up\.install\([^)]*\)/g) || [];
  const resumes = html.match(/up\.resume\([^)]*\)/g) || [];
  ok(installs.length === 3 && installs.every((s) => /,\s*(provOk|selected\.provenance)\)$/.test(s)),
     `page: every install() call passes the build verdict (${installs.length})`);
  ok(resumes.length === 3 && resumes.every((s) => /,\s*(provOk|selected\.provenance)\)$/.test(s)),
     `page: every resume() call passes the build verdict (${resumes.length})`);
  ok(!/up\.write\(/.test(html), "page: write() is never called directly");
  ok(/verifyVendorPackage\(sbytes/.test(html) && (html.match(/verifyVendorPackage\(/g) || []).length === 2,
     "page: the official package is admitted only through its pinned digest");
  ok(/!releaseOk \|\| !stock\.provenance/.test(html), "page: the stock paths refuse without a vendor verdict");
}

// ---- 5. make_site.py refuses a site that contradicts its record ---------------------------------
{
  const pkg = join(ROOT, "build/felucca.fwsc");
  const man = join(ROOT, "build/manifest.json");
  if (!existsSync(pkg) || !existsSync(man)) {
    console.log("make_site: no build/felucca.fwsc + manifest.json here, site checks skipped (a web-only checkout)");
  } else {
    const dir = mkdtempSync(join(tmpdir(), "qg1-site-"));
    const raw = readFileSync(pkg), build = JSON.parse(readFileSync(man, "utf8"));
    const bad = JSON.parse(JSON.stringify(build));
    bad.artifacts["felucca.fwsc"].sha256 = "0".repeat(64);
    const badPath = join(dir, "bad.json");
    writeFileSync(badPath, JSON.stringify(bad));
    let out = "";
    let exit = 0;
    try {
      execFileSync("python3", [join(ROOT, "web/make_site.py"), pkg, "v1.9.0", join(dir, "site-bad"), "--manifest", badPath],
                   { stdio: "pipe" });
    } catch (e) { exit = e.status; out = String(e.stderr); }
    ok(exit !== 0 && /refusing to publish it/.test(out) && !existsSync(join(dir, "site-bad/webapp/installer/index.html")),
       "make_site: a record whose SHA-256 is not the package's refuses to build the site");

    // a record with another commit in it: the label and the record must agree
    const bad2 = JSON.parse(JSON.stringify(build));
    bad2.commit = "f".repeat(40);
    const bad2Path = join(dir, "bad2.json");
    writeFileSync(bad2Path, JSON.stringify(bad2));
    let exit2 = 0, out2 = "";
    try {
      execFileSync("python3", [join(ROOT, "web/make_site.py"), pkg, "v1.9.0", join(dir, "site-bad2"), "--manifest", bad2Path],
                   { stdio: "pipe" });
    } catch (e) { exit2 = e.status; out2 = String(e.stderr); }
    ok(exit2 !== 0 && /refusing to publish it/.test(out2),
       "make_site: a record naming another commit than the label refuses to build the site");

    // the honest path: the real record, whose dirty list this tree may have (a normal build records it)
    const clean = JSON.parse(JSON.stringify(build));
    clean.dirty = [];
    const cleanPath = join(dir, "clean.json");
    writeFileSync(cleanPath, JSON.stringify(clean));
    const label = `${clean.version}-${String(clean.commit).slice(0, 7)}`;
    let exit3 = 1, out3 = "";
    try {
      execFileSync("python3", [join(ROOT, "web/make_site.py"), pkg, label, join(dir, "site"), "--manifest", cleanPath],
                   { stdio: "pipe" });
      exit3 = 0;
    } catch (e) { exit3 = e.status; out3 = String(e.stderr); }
    const page = join(dir, "site/webapp/installer/index.html");
    const meta = exit3 === 0 && existsSync(page)
      ? JSON.parse(/const meta = (\{.*?\});\n/s.exec(readFileSync(page, "utf8"))[1]) : null;
    ok(exit3 === 0 && meta && meta.build && meta.build.commit === clean.commit && meta.versions[0].build,
       "make_site: a clean record and its package build the site, with the record inlined" + (out3 ? ` (${out3.slice(0, 80)})` : ""));
    ok(meta && /Content-Security-Policy/.test(readFileSync(page, "utf8")) && !/"build":\s*null/.test(JSON.stringify(meta.versions[0])),
       "make_site: the record travels inside the page's CSP-protected script, not as a separate file");
    rmSync(dir, { recursive: true, force: true });
  }
}

console.log(failed ? `PROVENANCE TESTS FAILED (${failed})` : "provenance tests passed");
process.exit(failed ? 1 : 0);
