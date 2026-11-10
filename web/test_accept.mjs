// SPDX-License-Identifier: GPL-3.0-only
// The trace the installer page shows before a first install: M-Flasher's own conditions on the release,
// and the exact sector plan against a dump. Nothing here touches a unit.
import assert from "node:assert";
import { readFileSync } from "node:fs";
import { Package } from "./fm1pkg.js";
import { acceptance, sectorPlan, hexRange, ACCEPT_FLASH_MAX, ACCEPT_BOOT_AREA } from "./fm1accept.js";

const PKG = "tests/fixtures/felucca-v1.9.0.fwsc";
let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

const raw = new Uint8Array(readFileSync(PKG));
const acc = acceptance(raw);

ok(acc.ok, "the release passes every check M-Flasher makes before its first erase");
ok(acc.product === "FM-1_982", "the container parses as " + acc.product);
ok(acc.model === "FM-1", "its model, as M-Flasher reads it, is " + acc.model);
ok(acc.image === 0x93000, `the flash image is ${acc.image} B`);
ok(acc.checks.some((c) => c.key === "accKey" && c.ok), "the chip key 0x980F decrypts every app file");
const app = acc.pkg.appFiles();
ok(app.length === 2 && app.every((f) => f.ok), `app files checked: ${app.map((f) => f.name).join(", ")}`);
ok(app.find((f) => f.name === "app.bin").size === 581564, "app.bin is 581564 B");
ok(app.find((f) => f.name === "cfg_tool.bin").crc === 0x2e75, "cfg_tool.bin CRC16 is 0x2E75");

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
