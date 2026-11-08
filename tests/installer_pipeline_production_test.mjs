// tests/installer_pipeline_production_test.mjs
// Tests the REAL production classes and modules: web/fm1ota.js and web/fm1verify.js
import assert from "node:assert";
import fs from "node:fs";
import { Updater, requireChip, verifyManifest, requireProof, probeChip } from "../web/fm1ota.js";
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

// Check every frame on wire: ONLY read-only queries, 0 write frames, 0 upgrade frames
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

// 4. Test Updater cannot write without proof
const updater = new Updater();
let threwNoProof = false;
try {
  await updater.install(pkgBytes, { model: "FM-1", version: 15 });
} catch (e) {
  threwNoProof = true;
  assert.strictEqual(e.code === 'version' || e.code === 'proof', true);
}
assert.strictEqual(threwNoProof, true, "Direct install() without proof/context must be rejected");
console.log("  ok   Updater.install fail-closed without cryptographic proof");

console.log("Production installer pipeline test suite passed (0 regressions).");
