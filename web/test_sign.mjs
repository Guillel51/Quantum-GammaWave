// SPDX-License-Identifier: GPL-3.0-only
// The authenticity half of the install gate, checked the way the page checks it: WebCrypto Ed25519
// against the key pinned in tools/fm1_keys.json. tools/fm1_sign.py produces the manifest (tests/
// sign_test.py covers that side); this file covers what the browser does with it.
//
// A hash says the bytes arrived whole. Only the signature says who made them, so every case here is
// about the signature failing: if any of them passed, the page would enable INSTALL for bytes nobody
// signed.
import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import { verifyManifest } from "./fm1ota.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const refused = async (fn, code, what) => {
  try { await fn(); } catch (e) { assert.strictEqual(e.code, code, `${what}: ${e.code} ${e.message}`); n++; console.log("ok  " + what); return; }
  assert.fail("accepted: " + what);
};

const release = path.join(import.meta.dirname, "..", "release");
const pkgPath = fs.readdirSync(release).find((f) => f.endsWith(".fwsc"));
if (!pkgPath) { console.log("skip: no signed release in release/"); process.exit(0); }
const pkg = new Uint8Array(fs.readFileSync(path.join(release, pkgPath)));
const manifest = JSON.parse(fs.readFileSync(path.join(release, pkgPath + ".manifest.json"), "utf8"));
const keys = JSON.parse(fs.readFileSync(path.join(import.meta.dirname, "..", "tools", "fm1_keys.json"), "utf8")).keys;

const pass = await verifyManifest(pkg, manifest, keys, { product: "FM-1_982" });
ok(pass.keyid === "qg1-release-1" && pass.sha256 === manifest.sha256,
   "the published package verifies against the pinned key, and names itself");

/* the manifest names the product's MODEL (FM-1); the page names the build's identity (FM-1_982) */
ok((await verifyManifest(pkg, manifest, keys, { product: "FM-1" })).keyid === "qg1-release-1",
   "a manifest for FM-1 is accepted for FM-1_982: the model is what a package may not cross");

await refused(() => verifyManifest(pkg, manifest, keys, { product: "FM-2_982" }), "signature",
  "a manifest for FM-1 is refused for another model");

{
  const bad = pkg.slice(); bad[bad.length - 1] ^= 0x01;
  await refused(() => verifyManifest(bad, manifest, keys, { product: "FM-1" }), "signature",
    "one changed byte in the package is refused (the SHA-256 no longer matches)");
}
{
  const bad = { ...manifest, sig: "00".repeat(64) };
  await refused(() => verifyManifest(pkg, bad, keys, { product: "FM-1" }), "signature",
    "a signature by another key is refused");
}
{
  const bad = { ...manifest, version: "2.0.1" };
  await refused(() => verifyManifest(pkg, bad, keys, { product: "FM-1" }), "signature",
    "an edited field in the manifest is refused: the signature covers every field");
}
{
  const bad = { ...manifest, images: { ...manifest.images, "felucca.bin": "11".repeat(32) } };
  await refused(() => verifyManifest(pkg, bad, keys, { product: "FM-1" }), "signature",
    "an edited firmware-image hash is refused");
}
await refused(() => verifyManifest(pkg, manifest, {}, { product: "FM-1" }), "signature",
  "no pinned key trusts nothing: every package is refused (fail closed)");
await refused(() => verifyManifest(pkg, manifest, { ...keys, "qg1-release-1": "00".repeat(32) }, { product: "FM-1" }), "signature",
  "a pinned key that is not the signer is refused");
await refused(() => verifyManifest(pkg, null, keys, { product: "FM-1" }), "signature",
  "no manifest is refused");
await refused(() => verifyManifest(pkg, { ...manifest, format: "qg1-release-0" }, keys, { product: "FM-1" }), "signature",
  "a manifest of another format is refused");

/* the image hashes are what ties a signature to firmware: they must be the ones this build produced */
ok(Object.values(manifest.images || {}).every((h) => /^[0-9a-f]{64}$/.test(h)) &&
   manifest.images["loader/ota.bin"] && manifest.images["felucca.bin"],
   "the signed manifest names the app image and the loader by hash");

console.log(`signing (browser path): ${n} checks, 0 FAILED`);
