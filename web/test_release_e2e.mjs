// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The whole chain, on the artefacts this checkout actually built: the package, the build record the
// target compiler wrote (commit, toolchain, SDK, every hash) and a signed release manifest that names
// that record. The other suites build their own fixtures; this one refuses to invent anything, so it
// only runs where `build/felucca.fwsc` and `build/manifest.json` exist.
//
//   node web/test_release_e2e.mjs            (from the repo root; skips without a build)

import { createHash, generateKeyPairSync, sign as edSign } from "node:crypto";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { logicalImage, productOf } from "./fm1pkg.js";
import { requireProof, verifyManifest } from "./fm1ota.js";
import { requireProvenance, verifyProvenance } from "./fm1provenance.js";

const HERE = fileURLToPath(new URL(".", import.meta.url));
const ROOT = join(HERE, "..");
const PKG = join(ROOT, "build/felucca.fwsc");
const RECORD = join(ROOT, "build/manifest.json");

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(76)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const refused = async (fn, code) => {
  try { await fn(); return false; } catch (e) { return !code || e.code === code; }
};

if (!existsSync(PKG) || !existsSync(RECORD)) {
  console.log("release e2e: no build/felucca.fwsc + build/manifest.json here, skipped (host-only checkout)");
  process.exit(0);
}

const pkg = new Uint8Array(readFileSync(PKG));
const record = JSON.parse(readFileSync(RECORD, "utf8"));
const image = logicalImage(pkg);
const product = productOf(pkg);
const label = String(record.version || "");             // "v1.9.0": the label the site shows
const version = label.replace(/^v/, "");               // "1.9.0": a release manifest version is bare

// an ephemeral key signs this release: no production key is created, and nothing is written in the repo
const KEY = generateKeyPairSync("ed25519");
const PINNED = { "e2e-1": Buffer.from(KEY.publicKey.export({ format: "jwk" }).x, "base64url").toString("hex") };
const canon = (v) => v === null || typeof v !== "object" ? JSON.stringify(v)
  : Array.isArray(v) ? "[" + v.map(canon).join(",") + "]"
  : "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canon(v[k])).join(",") + "}";
const sha = (b) => createHash("sha256").update(b).digest("hex");
const release = (over = {}, rec = record) => {
  const unsigned = { format: "qg1-release-1", product: product.split("_")[0], version,
    min_firmware: "0.0.1", sha256: sha(pkg), size: pkg.length, created: "2026-11-05T00:00:00Z",
    keyid: "e2e-1", build_sha256: sha(Buffer.from(canon(rec))), ...over };
  return { ...unsigned, sig: edSign(null, Buffer.from(canon(unsigned)), KEY.privateKey).toString("hex") };
};

/* the record as the target compiler wrote it: this is a real AC791N build, not a fixture */
{
  ok(record.schema === "qg1-build-1", `record: schema qg1-build-1 (${record.schema})`);
  ok(record.commit === (process.env.GIT_COMMIT || record.commit) && record.commit.length === 40,
     `record: names a full commit (${String(record.commit).slice(0, 12)})`);
  ok(Array.isArray(record.dirty) && record.dirty.length === 0, "record: the tree was clean when it was built");
  ok(record.identity === product, `record: identity ${record.identity} is the package's`);
  const art = record.artifacts["felucca.fwsc"] || {};
  ok(art.bytes === pkg.length && art.sha256 === sha(pkg),
     `record: the package is the artefact it names (${pkg.length} B)`);
  ok(record.artifacts["felucca.bin"] && record.artifacts["felucca.bin"].sha256,
     "record: the firmware image is hashed too");
  ok(record.toolchain && record.toolchain.clang_sha256, "record: the toolchain binary is identified by hash");
  ok(record.sdk && record.sdk.files && Object.values(record.sdk.files).every((h) => h && h !== "missing"),
     "record: every SDK file is hashed (not missing)");
  ok(record.emulator && record.emulator.commit === String(record.commit).slice(0, 12),
     `record: the emulator came from the same commit (${record.emulator && record.emulator.commit})`);
  ok(record.config && typeof record.config === "object", "record: the FELUCCA_* knobs are recorded");
}

/* the verdict, the signature and the transport, on those bytes */
{
  const man = release();
  const proof = await verifyManifest(pkg, man, {}, { product: product.split("_")[0] });
  ok(proof.version === version && proof.sha256 === sha(pkg), "release: the SHA-256 + size check agrees (v2.6.0 model)");

  const prov = await verifyProvenance(pkg, record, { product, version: label, image });
  ok(prov.kind === "build" && prov.recordSha256 === man.build_sha256,
     "release: the signed manifest names this build record");

  const token = await requireProof({ pkg, manifest: man, keys: PINNED, product, firmware: "0.0.1", provenance: prov });
  ok(token.product === product && token.build === record.commit,
     "release: requireProof() admits the package, bound to the record's commit");

  ok(await refused(() => requireProof({ pkg, manifest: release({}, { ...record, commit: "f".repeat(40) }),
      keys: PINNED, product, firmware: "0.0.1", provenance: prov }), "provenance"),
     "release: a manifest naming another record is refused");
  // (v2.6.0) no private key is required: an empty key map no longer refuses the package.
  // Integrity is SHA-256 + size + model + provenance; authenticity of the editor is the
  // GitHub Artifact Attestation, not an Ed25519 signature.
  const dirty = { ...record, dirty: ["firmware/src/fx.c"] };
  ok(await refused(() => verifyProvenance(pkg, dirty, { product, version: label, image }), "provenance"),
     "release: a record of a dirty tree mints no verdict");
  ok(await refused(() => requireProvenance(prov, "FM-1_901", image), "provenance"),
     "release: the verdict is for this identity only");
}

/* the CLI agrees with the page: sign the real package, check it, and refuse a swapped record */
{
  const dir = mkdtempSync(join(tmpdir(), "qg1-e2e-"));
  const rec = join(dir, "manifest.json");
  writeFileSync(rec, JSON.stringify(record));
  const swapped = join(dir, "swapped.json");
  writeFileSync(swapped, JSON.stringify({ ...record, commit: "0".repeat(40) }));
  const man = release();
  ok(sha(Buffer.from(canon(record))) === man.build_sha256, "release: the page and the CLI hash the record the same way");
  const { execFileSync } = await import("node:child_process");
  const verify = (recordPath) => {
    const out = join(dir, "pkg.manifest.json");
    writeFileSync(out, JSON.stringify(man));
    try {
      execFileSync("python3", [join(ROOT, "tools/fm1_sign.py"), "verify", PKG, "--manifest", out,
        "--keys", join(dir, "keys.json"), "--build-manifest", recordPath], { stdio: "pipe" });
      return 0;
    } catch (e) { return e.status || 1; }
  };
  writeFileSync(join(dir, "keys.json"), JSON.stringify({ keys: PINNED }));
  ok(verify(rec) === 0, "release: fm1_sign.py verify accepts the record the manifest names");
  ok(verify(swapped) !== 0, "release: fm1_sign.py verify refuses a swapped record");
  rmSync(dir, { recursive: true, force: true });
}

console.log(failed ? `RELEASE E2E FAILED (${failed})` : "release e2e passed");
process.exit(failed ? 1 : 0);
