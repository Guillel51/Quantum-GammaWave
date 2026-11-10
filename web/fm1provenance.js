// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// (v1.9.0) What authorises writing a package to an FM-1, from the build side.
//
// tools/build_manifest.py writes build/manifest.json (schema qg1-build-1) next to the artefacts it
// just produced: the commit, whether the tree was dirty, the identity, the version, every artefact's
// SHA-256 and size, the toolchain and SDK hashes, and the emulator built from the same tree. This
// module is the other half of that: it refuses a package unless the bytes are exactly the artefact a
// clean build of a named commit produced and the labels around it agree with that record.
//
// What this is, and what it is not:
//   integrity   yes: the bytes are the ones the manifest names (SHA-256 and exact size).
//   provenance  yes: the manifest names a commit, a clean tree, a version and an identity, and the
//               site's labels and the emulator's manifest must agree with them.
//   authenticity NO. The manifest travels with the package and is fetched from the same origin, so
//               whoever can replace the package can replace the manifest. Only an Ed25519 signature
//               over a pinned key (fm1ota.js verifyManifest, tools/fm1_sign.py) establishes who
//               built it, and with no production key pinned every package is refused. This module
//               never turns a hash into a signature: it says so in the message the user gets.
//
// Every check throws { code: "provenance" }. There is no partial pass and no override: a package
// that fails any of them is never handed to Updater, and Updater refuses to send a frame without the
// token this module creates.

const PROV_SCHEMA = "qg1-build-1";
const EMU_SCHEMA = "qg1-emu-1";
const HEX64 = /^[0-9a-f]{64}$/;
const COMMIT = /^[0-9a-f]{40}$/;
const VERSION = /^v\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?$/;
const IDENTITY = /^FM-1_\d{3}$/;
// the token is only obtainable from this module: a plain object cannot pass for one
const VERDICT = Symbol("qg1-provenance");

function provFail(msg) { const e = new Error(msg); e.code = "provenance"; return e; }

async function sha256(bytes) {
  const d = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// The record's own digest, over the same canonical JSON the signing tool hashes (sorted keys, no
// spaces, ASCII): tools/fm1_sign.py puts it in the signed release manifest as build_sha256, and
// requireProof() refuses a release whose signature covers another record.
const canon = (v) => v === null || typeof v !== "object" ? JSON.stringify(v)
  : Array.isArray(v) ? "[" + v.map(canon).join(",") + "]"
  : "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canon(v[k])).join(",") + "}";

async function recordDigest(build) {
  return sha256(new TextEncoder().encode(canon(build)));
}

const isInt = (n) => typeof n === "number" && Number.isInteger(n) && n > 0;

// "v1.9.0" or "v1.9.0-61a3198": the label may carry the commit it was built from, never another
function labelMatches(label, version, commit) {
  if (label === version) return true;
  const pre = version + "-";
  if (!label.startsWith(pre)) return false;
  const suffix = label.slice(pre.length);
  return /^[0-9a-f]{7,40}$/.test(suffix) && commit.startsWith(suffix);
}

function requireFields(build) {
  for (const k of ["version", "commit", "identity", "dirty", "artifacts", "emulator", "config", "sdk", "toolchain"])
    if (!(k in build)) throw provFail(`The build manifest has no ${k}: it is incomplete, so the package is not trusted.`);
}

function checkArtifact(art, name) {
  if (!art || typeof art !== "object" || Array.isArray(art))
    throw provFail(`The build manifest has no ${name} record: it is incomplete, so the package is not trusted.`);
  if (!isInt(art.bytes)) throw provFail(`The manifest's ${name} size is not a positive whole number: the package is not trusted.`);
  if (!HEX64.test(String(art.sha256 || ""))) throw provFail(`The manifest's ${name} SHA-256 is not 64 hex digits: the package is not trusted.`);
}

function checkEmulator(emu, build) {
  if (!emu || typeof emu !== "object" || Array.isArray(emu))
    throw provFail("The build manifest has no emulator record: it cannot be tied to this firmware.");
  if (emu.schema !== undefined && emu.schema !== EMU_SCHEMA)
    throw provFail(`The emulator manifest says ${JSON.stringify(emu.schema)}, not ${EMU_SCHEMA}: the build record is inconsistent.`);
  if (emu.version !== build.version)
    throw provFail(`The emulator is ${emu.version}, the firmware ${build.version}: one of the two is from another build.`);
  if (typeof emu.commit !== "string" || !build.commit.startsWith(emu.commit) || emu.commit.length < 7)
    throw provFail(`The emulator was built from ${emu.commit}, the firmware from ${build.commit}: they are not the same build.`);
  if (!isInt(emu.wasm_bytes) || !HEX64.test(String(emu.wasm_sha256 || "")))
    throw provFail("The emulator manifest has no usable wasm size or SHA-256: the build record is incomplete.");
}

// verify a package against the build manifest that produced it (and, optionally, the site labels)
//   pkg       the .fwsc bytes the device will be served
//   build     the qg1-build-1 manifest (parsed JSON)
//   product   the identity the caller expects (the site's meta.product); null to skip
//   version   the label the site shows; null to skip
//   emulator  a separate qg1-emu-1 manifest to hold against the build; null to use build.emulator
//   image     the logical image derived from pkg, hashed into the token so Updater can prove the
//             bytes it serves are the ones that were verified
// returns a frozen token that Updater.install/write/resume accept, and nothing else does
export async function verifyProvenance(pkg, build, { product = null, version = null, emulator = null, image = null } = {}) {
  if (!build || typeof build !== "object" || Array.isArray(build))
    throw provFail("No build manifest is published with this package: the update was not started.");
  if (build.schema !== PROV_SCHEMA)
    throw provFail(`The build manifest is ${JSON.stringify(build.schema)}, not ${PROV_SCHEMA}: the update was not started.`);
  requireFields(build);
  if (!COMMIT.test(String(build.commit)))
    throw provFail("The build manifest's commit is not a full commit id: the update was not started.");
  if (!VERSION.test(String(build.version)))
    throw provFail(`The build manifest's version ${JSON.stringify(build.version)} is malformed: the update was not started.`);
  if (!IDENTITY.test(String(build.identity)))
    throw provFail(`The build manifest's identity ${JSON.stringify(build.identity)} is malformed: the update was not started.`);
  if (!Array.isArray(build.dirty))
    throw provFail("The build manifest has no list of uncommitted files: the update was not started.");
  if (build.dirty.length)
    throw provFail(`The package was built from a tree with uncommitted changes (${build.dirty.slice(0, 4).join(", ")}${build.dirty.length > 4 ? ", …" : ""}): a published package must come from a clean tree, so the update was not started.`);

  if (!build.artifacts || typeof build.artifacts !== "object" || Array.isArray(build.artifacts))
    throw provFail("The build manifest lists no artefacts: the update was not started.");
  for (const [name, art] of Object.entries(build.artifacts)) checkArtifact(art, name);
  const art = build.artifacts["felucca.fwsc"];
  if (!art) throw provFail("The build manifest does not name the package (felucca.fwsc): the update was not started.");
  if (!build.artifacts["felucca.bin"])
    throw provFail("The build manifest does not name the firmware image (felucca.bin): the update was not started.");
  if (!(pkg instanceof Uint8Array) || pkg.length === 0)
    throw provFail("The package is empty or truncated: the update was not started.");

  // the bytes are the artefact, exactly
  if (pkg.length !== art.bytes)
    throw provFail(`The package is ${pkg.length} bytes, the manifest's is ${art.bytes}: the download is incomplete or the file is not this build. Nothing was sent.`);
  const digest = await sha256(pkg);
  if (digest !== art.sha256)
    throw provFail(`The package's SHA-256 ${digest.slice(0, 16)}… is not the manifest's ${String(art.sha256).slice(0, 16)}…: the bytes are not this build. Nothing was sent.`);

  // the labels agree with the record
  if (product !== null && build.identity !== product)
    throw provFail(`The package is built for ${build.identity}, the site offers ${product}: the update was not started.`);
  if (version !== null && !labelMatches(String(version), build.version, build.commit))
    throw provFail(`The site labels this package ${JSON.stringify(version)}, the build record says ${build.version} (${build.commit.slice(0, 7)}): the label and the artefact disagree. Nothing was sent.`);

  // the emulator in the same record
  checkEmulator(emulator || build.emulator, build);
  if (emulator && build.emulator && String(build.emulator.wasm_sha256) !== String(emulator.wasm_sha256))
    throw provFail("The emulator manifest and the build manifest name different wasm: the site mixes two builds.");

  const imageSha256 = image ? await sha256(image) : null;
  return Object.freeze({
    [VERDICT]: true,
    schema: "qg1-provenance-1",
    kind: "build",
    version: build.version,
    commit: build.commit,
    identity: build.identity,
    sha256: digest,
    bytes: pkg.length,
    imageSha256,
    recordSha256: await recordDigest(build),   // the hash the signed release manifest must name
  });
}

// A vendor package (the official FM-1 V15) has no build record of ours. It is accepted only through
// the digest compiled into the page (fm1pkg.js STOCK_V15_SHA256), which is a pin, not provenance:
// the token says so, and only that pin can create it.
export async function verifyVendorPackage(pkg, { product, sha256: pinned, size, image = null } = {}) {
  if (!(pkg instanceof Uint8Array) || !pkg.length) throw provFail("The vendor package is empty or truncated: nothing was sent.");
  if (!isInt(size) || pkg.length !== size)
    throw provFail(`The vendor package is ${pkg.length} bytes, the pinned size is ${size}: nothing was sent.`);
  if (!HEX64.test(String(pinned || ""))) throw provFail("There is no pinned digest for this vendor package: nothing was sent.");
  const digest = await sha256(pkg);
  if (digest !== pinned) throw provFail("The vendor package is not the pinned official file: nothing was sent.");
  if (!IDENTITY.test(String(product || ""))) throw provFail("The vendor package has no usable identity: nothing was sent.");
  return Object.freeze({
    [VERDICT]: true,
    schema: "qg1-provenance-1",
    kind: "vendor",
    version: null,
    commit: null,
    identity: product,
    sha256: digest,
    bytes: pkg.length,
    imageSha256: image ? await sha256(image) : null,
    recordSha256: null,                        // a pin, not a build: there is no record to name
  });
}

// the gate Updater calls before it sends anything. Throws unless the token came from this module,
// names the same identity, and (when it was bound to an image) those are still the bytes served.
export async function requireProvenance(token, product = null, image = null) {
  if (!token || token[VERDICT] !== true)
    throw provFail("This package has no verified build provenance: nothing was sent.");
  if (product !== null && token.identity !== product)
    throw provFail(`The verified package is ${token.identity}, the update is for ${product}: nothing was sent.`);
  if (image && token.imageSha256) {
    const got = await sha256(image);
    if (got !== token.imageSha256)
      throw provFail("The image being written is not the one that was verified: nothing was sent.");
  }
  return token;
}
