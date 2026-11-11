// SPDX-License-Identifier: GPL-3.0-only
// Install-integrity gate for v2.6.0: SHA-256 + size + model/identity + version floor.
// Ed25519 is no longer required for ordinary installs (see docs/RELEASE_PROCESS.md).
// A SHA-256 published alongside the package (same origin) is not proof of authorship:
// see docs/SIGNING.md. GitHub Artifact Attestations (release.yml) provide traceability.
import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import { verifyManifest } from "./fm1ota.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const refused = async (fn, what) => {
  try { await fn(); } catch (e) { n++; console.log("ok  " + what + " (" + e.code + ")"); return; }
  assert.fail("accepted: " + what);
};

const release = path.join(import.meta.dirname, "..", "release");
const pkgPath = fs.readdirSync(release).find((f) => f.endsWith(".fwsc"));
if (!pkgPath) { console.log("skip: no package in release/"); process.exit(0); }
const pkg = new Uint8Array(fs.readFileSync(path.join(release, pkgPath)));
const manifest = JSON.parse(fs.readFileSync(path.join(release, pkgPath + ".manifest.json"), "utf8"));

const pass = await verifyManifest(pkg, manifest, {}, { product: "FM-1_965" });
ok(pass.sha256 === manifest.sha256, "a valid package is accepted (SHA-256 + size + model)");

// altered bytes -> wrong hash
{
  const bad = pkg.slice(); bad[bad.length - 1] ^= 0x01;
  await refused(() => verifyManifest(bad, manifest, {}, { product: "FM-1_965" }),
    "altered package bytes are refused");
}

// wrong hash in manifest
{
  const m = JSON.parse(JSON.stringify(manifest));
  m.sha256 = "0".repeat(64);
  await refused(() => verifyManifest(pkg, m, {}, { product: "FM-1_965" }),
    "wrong SHA-256 in manifest is refused");
}

// wrong size in manifest
{
  const m = JSON.parse(JSON.stringify(manifest));
  m.size = manifest.size + 1;
  await refused(() => verifyManifest(pkg, m, {}, { product: "FM-1_965" }),
    "wrong size in manifest is refused");
}

// incompatible model
await refused(() => verifyManifest(pkg, manifest, {}, { product: "FM-2_982" }),
  "an incompatible model is refused");

// inconsistent manifest (missing field)
{
  const m = JSON.parse(JSON.stringify(manifest));
  delete m.sha256;
  await refused(() => verifyManifest(pkg, m, {}, { product: "FM-1_965" }),
    "a manifest missing sha256 is refused");
}

console.log(n + " checks passed");