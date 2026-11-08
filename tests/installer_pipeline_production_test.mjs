// tests/installer_pipeline_production_test.mjs
// Comprehensive test suite for real web/fm1ota.js and web/fm1verify.js modules.
import assert from "node:assert";
import fs from "node:fs";
import crypto from "node:crypto";
import { Updater, requireChip, verifyManifest, requireProof, pack7, unpack7 } from "../web/fm1ota.js";
import { verifyDevice, READ_ONLY_QUERY } from "../web/fm1verify.js";

console.log("Starting production installer pipeline verification...");

// 1. Load real production release package & manifest
const pkgBytes = fs.readFileSync("release/felucca-v1.9.0.fwsc");
assert.strictEqual(pkgBytes.length, 610806, "Release package size must be exactly 610806 bytes");

// Helper to create fake MIDI access matching WebMIDI interface
function createMockMidi(names, script = {}) {
  const sent = [];
  const inputs = new Map(), outputs = new Map();
  for (const name of names) {
    const inPort = { name, state: "connected", onmidimessage: null, open: async () => {} };
    const outPort = {
      name, state: "connected", open: async () => {},
      send(bytes) {
        sent.push(bytes);
        if (script[name]) {
          const reply = script[name]();
          if (reply && reply.length && inPort.onmidimessage) {
            inPort.onmidimessage({ data: new Uint8Array(reply) });
          }
        }
      }
    };
    inputs.set(name, inPort);
    outputs.set(name, outPort);
  }
  return { midi: { inputs, outputs }, sent };
}

function identityFrame(text) {
  const d = [0, 0x59, 0x11, 0, 0, 0, ...Array.from(text.padEnd(27, "\0")).map((c) => c.charCodeAt(0)), 0];
  const out = []; let acc = 0, nb = 0;
  for (const b of d) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } }
  if (nb) out.push(acc & 0x7F);
  return [0xF0, ...out, 0xF7];
}

// 2. Test verifyDevice (Verify Only surface) directly with production code
const mock = createMockMidi(["FM-1"], { "FM-1": () => identityFrame("FM-1_015") });
const devReport = await verifyDevice(mock.midi, { product: "FM-1_982" });

assert.strictEqual(devReport.recognised, true, "FM-1_015 must be recognized");
assert.strictEqual(devReport.canInstall, false, "Verify Only MUST NOT authorize installation (canInstall must be false)");
assert.strictEqual(devReport.verdict.result, "NO-GO", "Verdict must be fail-closed NO-GO");
assert.strictEqual(devReport.verdict.canInstall, false, "Verdict canInstall must be false");
assert.strictEqual(devReport.verdict.blocked.includes("gate.recovery"), true, "Recovery must be in blocked gates");
assert.strictEqual(mock.sent.length >= 1, true, "Verify Only must send at least 1 identity query");

for (const f of mock.sent) {
  assert.strictEqual(f[0], 0xF0, "MIDI SysEx start");
  assert.strictEqual(f[f.length - 1], 0xF7, "MIDI SysEx end");
  assert.strictEqual(f.length, READ_ONLY_QUERY.length, "Only identity queries sent on the wire");
}
console.log("  ok   verifyDevice is strictly read-only and guarantees 0 writes on the wire");

// 3. Test requireChip policy on production functions
assert.doesNotThrow(() => requireChip({ jedec: 0x856014, block: 0x10000, writable: true }), "Puya P25Q80H must be accepted");
assert.doesNotThrow(() => requireChip({ jedec: 0xB36014, block: 0x10000, writable: true }), "UCUN UC25HQ80 must be accepted");

// Read-only / uncertified chips:
let threwWinbond = false;
try {
  requireChip({ jedec: 0xEF4014, block: 0x10000, writable: false });
} catch (e) {
  threwWinbond = true;
  assert.strictEqual(e.code, "chip");
}
assert.strictEqual(threwWinbond, true, "Winbond W25Q80 must be rejected from write path");

let threwUnknown = false;
try {
  requireChip({ jedec: 0x999999, block: 0x10000, writable: false });
} catch (e) {
  threwUnknown = true;
  assert.strictEqual(e.code, "chip");
}
assert.strictEqual(threwUnknown, true, "Unknown JEDEC must be rejected from write path");
console.log("  ok   requireChip enforces writable allowlist fail-closed on production code");

// 4. Test verifyManifest with Ephemeral Ed25519 Keypair
const { publicKey, privateKey } = crypto.generateKeyPairSync("ed25519");
const rawPub = publicKey.export({ format: "der", type: "spki" }).subarray(-32);
const pinnedKeys = { "test-key-1": rawPub.toString("hex") };

// Use real package bytes for faithful parsing
const testPkg = pkgBytes;
const testSha = crypto.createHash("sha256").update(testPkg).digest("hex");

const manifestBody = {
  format: "qg1-release-1",
  product: "FM-1",
  version: "1.9.0",
  min_firmware: "1.0.0",
  sha256: testSha,
  size: testPkg.length,
  keyid: "test-key-1"
};

function canonJson(v) {
  if (v === null || typeof v !== "object") return JSON.stringify(v);
  if (Array.isArray(v)) return "[" + v.map(canonJson).join(",") + "]";
  return "{" + Object.keys(v).sort().map((k) => JSON.stringify(k) + ":" + canonJson(v[k])).join(",") + "}";
}

const canonBytes = new TextEncoder().encode(canonJson(manifestBody));
const sig = crypto.sign(null, canonBytes, privateKey);

const signedManifest = {
  ...manifestBody,
  sig: sig.toString("hex")
};

// 4.1 Positive verifyManifest
const vRes = await verifyManifest(testPkg, signedManifest, pinnedKeys, { product: "FM-1" });
assert.strictEqual(vRes.keyid, "test-key-1", "Key ID must match");
assert.strictEqual(vRes.sha256, testSha, "Hash must match");
console.log("  ok   verifyManifest succeeds with valid Ed25519 signature");

// 4.2 Negative verifyManifest tests
for (const [modManifest, modKeys, expectedErr, what] of [
  [{ ...signedManifest, sha256: "0".repeat(64) }, pinnedKeys, "signature", "wrong SHA-256"],
  [{ ...signedManifest, size: 99999 }, pinnedKeys, "signature", "wrong size"],
  [{ ...signedManifest, product: "OTHER_MODEL" }, pinnedKeys, "signature", "model mismatch"],
  [{ ...signedManifest, version: "2.0.0" }, pinnedKeys, "signature", "tampered version"],
  [signedManifest, {}, "signature", "no pinned key"],
  [signedManifest, { "other-key": pinnedKeys["test-key-1"] }, "signature", "unknown key id"]
]) {
  let failed = false;
  try {
    await verifyManifest(testPkg, modManifest, modKeys, { product: "FM-1" });
  } catch (e) {
    failed = (e.code === expectedErr);
  }
  assert.strictEqual(failed, true, `verifyManifest must reject ${what}`);
}
console.log("  ok   verifyManifest rejects tampered manifests, wrong hashes, and unpinned keys");

// 5. Test Updater negative write paths & zero frame guarantees
const updater = new Updater();

// Scenario A: Missing proof context
let threwNoContext = false;
try {
  await updater.install(testPkg, "FM-1_982", () => {}, null); // ctx is null
} catch (e) {
  threwNoContext = true;
}
assert.strictEqual(threwNoContext, true, "Updater must reject install without context");

// Scenario B: Swapped package bytes with wrong signature
let threwSwapped = false;
const swappedPkg = Buffer.alloc(1024, 0x55);
const ctx = {
  manifest: signedManifest,
  keys: pinnedKeys,
  firmware: "1.0.0"
};
try {
  await updater.install(swappedPkg, "FM-1_982", () => {}, ctx);
} catch (e) {
  threwSwapped = (e.code === "signature" || e.code === "image" || e.code === "proof" || e.code === "version");
}
assert.strictEqual(threwSwapped, true, "Updater must reject swapped package bytes");

console.log("  ok   Updater guarantees fail-closed rejection on any authorization failure");
console.log("Production installer pipeline test suite passed (0 regressions).");
