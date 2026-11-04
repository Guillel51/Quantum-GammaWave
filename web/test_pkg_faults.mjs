// SPDX-License-Identifier: GPL-3.0-only
// Fault injection for the real .fwsc container: truncation and critical-byte corruption must be refused
// before the updater can build a logical image. Run from the repo root:
//   node web/test_pkg_faults.mjs build/felucca.fwsc
import { readFileSync } from "node:fs";
import { Package, productOf } from "./fm1pkg.js";

const path = process.argv[2] || "build/felucca.fwsc";
let failed = 0;
const ok = (cond, what) => {
  console.log(`${what.padEnd(78)} ${cond ? "ok" : "FAIL"}`);
  if (!cond) failed++;
};

let raw;
try {
  raw = new Uint8Array(readFileSync(path));
} catch (e) {
  console.error(`faults: cannot read ${path}: ${e.message}`);
  process.exit(2);
}

let good = null;
try { good = new Package(raw); } catch (e) {
  console.error(`faults: the reference package is already invalid: ${e.message}`);
  process.exit(2);
}
ok(!!good && good.app.size > 0, `reference package parses (${good.product}, ${good.app.size} B app)`);

function refused(label, bytes) {
  try {
    new Package(bytes);
    ok(false, label);
  } catch (_) {
    ok(true, label);
  }
}

// A download cut at a structural boundary or inside flash.bin must never be accepted. A cut in the
// unused tail after flash.bin is not a fault: the container is still complete.
const flashEnd = good.flashFileOff + good.flashSize;
for (const cut of [0, 1, 16, 0x30, 0x3c0, 0x800, flashEnd - 1, flashEnd - 0x1000]) {
  if (cut < 0 || cut >= raw.length) continue;
  refused(`truncated at ${cut} B is refused`, raw.subarray(0, cut));
}

// The UFW header and the flash header/entry list are structural. Byte 0x2f is the identity marker:
// changing it is allowed to parse but must change the identity the installer compares.
for (const at of [0, 1, 2, 3, 0x30, good.flashFileOff, good.flashFileOff + 32, good.flashFileOff + 64]) {
  if (at >= raw.length) continue;
  const bad = raw.slice();
  bad[at] ^= 0x5a;
  refused(`critical byte ${at} corrupted is refused`, bad);
}
{
  const bad = raw.slice();
  bad[0x2f] ^= 0x5a;
  ok(productOf(bad) !== good.product, "identity marker corruption changes the reported product");
}

// A corrupt flash payload is refused by the flash.bin CRC, not by a plausible-looking parse.
if (good.flashFileOff + 64 < raw.length) {
  const bad = raw.slice();
  bad[good.flashFileOff + 64] ^= 0xff;
  refused("corrupt flash payload is refused", bad);
}

if (failed) {
  console.error(`${failed} fault-injection check(s) failed`);
  process.exit(1);
}
console.log("package fault injection: all checks passed");
