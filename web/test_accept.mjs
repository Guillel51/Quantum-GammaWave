// SPDX-License-Identifier: GPL-3.0-only
// The trace the installer page shows before a first install: M-Flasher's own conditions on the release,
// and the exact sector plan against a dump. Nothing here touches a unit.
import assert from "node:assert";
import fs, { readFileSync } from "node:fs";
import { Package } from "./fm1pkg.js";
import { acceptance, sectorPlan, hexRange, ACCEPT_FLASH_MAX, ACCEPT_BOOT_AREA } from "./fm1accept.js";

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
const PKG = findPackage();
if (!PKG) {
  console.log("skip: no package in this checkout (tests/fixtures or build/felucca-*.fwsc)");
  process.exit(0);
}
let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

const raw = new Uint8Array(readFileSync(PKG));
const acc = acceptance(raw);

ok(acc.ok, "the release passes every check M-Flasher makes before its first erase");
/* The numbers below are the 1.9.0 fixture's, and so is its identity: it answered FM-1_982, and a fixture
 * that suddenly answered the current number would be a rewritten fixture, not evidence. The number this
 * project ships is checked on the signed release and against the tree's own PRODUCT, below -- commit
 * 03d3f80 changed this line to the new identity and left the fixture, so the suite has been red since. */
ok(acc.product === "FM-1_982", `${PKG} parses as its own identity, ${acc.product} (a 1.9.0 fixture)`);
ok(acc.model === "FM-1", "its model, as M-Flasher reads it, is " + acc.model);
ok(acc.image === 0x93000, `the flash image is ${acc.image} B`);
ok(acc.checks.some((c) => c.key === "accKey" && c.ok), "the chip key 0x980F decrypts every app file");
const app = acc.pkg.appFiles();
ok(app.length === 2 && app.every((f) => f.ok), `app files checked: ${app.map((f) => f.name).join(", ")}`);
ok(app.find((f) => f.name === "app.bin").size === 583612, "app.bin is 583612 B");
ok(app.find((f) => f.name === "cfg_tool.bin").crc === 0x2e75, "cfg_tool.bin CRC16 is 0x2E75");

/* (audit 2026-11-10) What the project publishes: the newest signed release in release/ must pass the same
 * acceptance checks, and the identity in its bytes must be the one tools/build.py declares. A fixture can
 * prove the plan; only the release can prove the number that ships. */
const PRODUCT = (() => {
  const src = readFileSync("tools/build.py", "utf8");
  const m = src.match(/^PRODUCT\s*=\s*"([^"]+)"/m);
  return m ? m[1] : null;
})();
const newest = (() => {
  try {
    return fs.readdirSync("release").filter((n) => /^felucca-\d+\.\d+\.\d+\.fwsc$/.test(n)).sort().pop() || null;
  } catch (_) { return null; }
})();
if (newest && PRODUCT) {
  const a2 = acceptance(new Uint8Array(readFileSync(`release/${newest}`)));
  ok(a2.ok, `the signed release passes the same checks: ${newest}`);
  ok(a2.product === PRODUCT, `release/${newest} answers ${a2.product}, the tree's PRODUCT (${PRODUCT})`);
  ok(a2.model === "FM-1", "its model is FM-1");
} else {
  console.log("skip: no signed release in release/ to check the shipped identity against");
}

// the plan, before any dump is loaded: the whole range is the bound
const bound = sectorPlan(acc.pkg, null);
ok(!bound.exact && bound.bound === 143, "without a dump the plan is bounded by the whole app area (143 sectors)");
ok(bound.range[0] === ACCEPT_BOOT_AREA && bound.range[1] === 0x93000, "the range is 0x04000-0x92FFF");

// a dump that does not read as the package: every sector of the range differs
const wiped = new Uint8Array(ACCEPT_FLASH_MAX).fill(0xff);
wiped.set(acc.pkg.flash.subarray(0, ACCEPT_BOOT_AREA), 0);
const p1 = sectorPlan(acc.pkg, wiped);
ok(p1.exact && p1.sectors.length === 143, "a blank app area means all 143 sectors are written");
ok(p1.runs.length === 1 && hexRange(p1.runs[0]) === "0x04000-0x92FFF", "one run: 0x04000-0x92FFF");

// a dump that already holds this firmware: nothing to write
const same = new Uint8Array(ACCEPT_FLASH_MAX).fill(0xff);
same.set(acc.pkg.flash, 0);
const p2 = sectorPlan(acc.pkg, same);
ok(p2.exact && p2.sectors.length === 0, "a dump that already holds this image needs no write");

// one byte in the middle: exactly one sector, and only that one
const one = new Uint8Array(same);
one[0x5000] ^= 0x01;
const p3 = sectorPlan(acc.pkg, one);
ok(p3.sectors.length === 1 && p3.sectors[0] === 0x5000, "one changed byte is one sector: 0x05000");
ok(p3.unchanged === 142, "the other 142 sectors are left alone");

ok(sectorPlan(acc.pkg, new Uint8Array(1024)).badDump === 1024, "a dump that is not 1 MiB is refused, not guessed");

// refusals, with M-Flasher's wording keys
const foreign = new Uint8Array(raw);
foreign[0x2f] = (0x53 + 1) & 0xff;                      // 'F' -> 'S': SM-1_982
const r1 = acceptance(foreign);
ok(!r1.ok && r1.refusal.key === "refuseModel" && r1.refusal.value === "SM-1",
   "a package for another model is refused: " + r1.refusal.value);

const damaged = new Uint8Array(raw);
damaged[acc.pkg.flashFileOff + 0x5000] ^= 0xff;         // inside flash.bin
const r2 = acceptance(damaged);
ok(!r2.ok && r2.refusal.key === "refuseContainer", "a damaged flash image is refused: " + r2.refusal.value);

ok(!acceptance(new Uint8Array(64)).ok, "something that is not a package is refused");

// and the image the plan is built from is the one the installer would send
const pkg = new Package(raw);
ok(pkg.flash.length === acc.pkg.flash.length && pkg.flashSize === 0x93000,
   "the plan is built from the same image the transport addresses");

console.log(`\nm-flasher acceptance (browser): ${n} checks, 0 FAILED`);
