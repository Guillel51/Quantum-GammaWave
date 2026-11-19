// The donor rule: a stock FM-1 is an origin, never a destination.
// V15 (FM-1_015) and V16 (FM-1_016) are the releases this project has read; both may be the
// firmware a first install starts from, and neither may be reported as the target afterwards.
import assert from "node:assert/strict";
import { donorIdentity } from "./fm1ota.js";

const id = (text) => { const m = /^(.*)_(\d+)$/.exec(text); return { text, model: m[1], version: Number(m[2]) }; };
const QG = "FM-1_965";
let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

ok(donorIdentity(id("FM-1_015"), QG, "15"), "stock V15 is admitted as a donor");
ok(donorIdentity(id("FM-1_015"), QG, "V15"), "the donor firmware may be written V15");
ok(donorIdentity(id("FM-1_015"), QG, ""), "no firmware string does not contradict the identity");
ok(donorIdentity(id("FM-1_016"), QG, "16"), "stock V16 is admitted as a donor (the release this project read)");
ok(donorIdentity(id("FM-1_016"), QG, "V16"), "the donor firmware may be written V16");
ok(donorIdentity(id("FM-1_016"), QG, ""), "V16 with no firmware string is still the identity's release");
ok(!donorIdentity(id("FM-1_016"), QG, "15"), "a V16 identity contradicted by firmware V15 is refused");
ok(!donorIdentity(id("FM-1_015"), QG, "16"), "a V15 identity contradicted by firmware V16 is refused");
ok(!donorIdentity(id("FM-1_017"), QG, "17"), "an unread stock version is refused even when the numbers agree");
ok(!donorIdentity(id("FM-1_965"), QG, "15"), "the target identity is not a donor");
ok(!donorIdentity(id("GW-1_015"), QG, "15"), "never across models");
ok(!donorIdentity(id("FM-1_911"), QG, "15"), "Pocket GammaWave 911 is not a stock donor");
ok(!donorIdentity(null, QG, "15"), "no identity is refused");
ok(!donorIdentity(id("FM-1_015"), "FM-2_982", "15"), "a package for another model never admits this device");
ok(!donorIdentity(id("FM-1_016"), "GW-1_915", "16"), "a V16 unit never admits a package for another model");
console.log("donor: " + n + " checks, 0 FAILED");
