// The donor rule: a stock FM-1 is an origin, never a destination.
import assert from "node:assert/strict";
import { donorIdentity } from "./fm1ota.js";

const id = (text) => { const m = /^(.*)_(\d+)$/.exec(text); return { text, model: m[1], version: Number(m[2]) }; };
const QG = "FM-1_965";
let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

ok(donorIdentity(id("FM-1_015"), QG, "15"), "stock V15 is admitted as a donor");
ok(donorIdentity(id("FM-1_015"), QG, "V15"), "the donor firmware may be written V15");
ok(donorIdentity(id("FM-1_015"), QG, ""), "no firmware string does not contradict the identity");
ok(!donorIdentity(id("FM-1_015"), QG, "16"), "stock V16 is not a donor this project has read");
ok(!donorIdentity(id("FM-1_016"), QG, "15"), "an unread stock version is refused even when firmware says V15");
ok(!donorIdentity(id("FM-1_965"), QG, "15"), "the target identity is not a donor");
ok(!donorIdentity(id("GW-1_015"), QG, "15"), "never across models");
ok(!donorIdentity(id("FM-1_911"), QG, "15"), "Pocket GammaWave 911 is not a stock donor");
ok(!donorIdentity(null, QG, "15"), "no identity is refused");
ok(!donorIdentity(id("FM-1_015"), "FM-2_982", "15"), "a package for another model never admits this device");
console.log("donor: " + n + " checks, 0 FAILED");
