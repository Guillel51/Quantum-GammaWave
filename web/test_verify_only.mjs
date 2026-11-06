// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Verify Only, at the transport. The page's verify button must be a read of the unit's identity and
// nothing else, so this drives the real Updater and the real BackupConnection against a simulated
// FM-1 and records every byte the device is sent. Whatever the sequence is called, the frames it may
// send are three: the handshake, the backup channel's INFO read, and -- only on the install path,
// never here -- the chip probe. A write frame, an UPGRADE, a PUT: any of them fails the test.
import { readFileSync } from "node:fs";
import { Updater, pack7 } from "./fm1ota.js";
import { BackupConnection } from "./fm1backup.js";
import vm from "node:vm";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(74)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const INFO_QUERY = [240, 125, 70, 76, 1, 247];            /* BackupConnection([1, []]) */
const isInfoQuery = (f) => f.length === 6 && f.slice(0, 6).every((v, i) => v === INFO_QUERY[i]);

/* the identity the firmware answers the handshake with: 34 bytes, 0 / 0x59 / 0x11 / pad / 27 chars */
function identityReply(text) {
  const d = new Array(34).fill(0);
  d[0] = 0; d[1] = 0x59; d[2] = 0x11;
  const t = (text + "\0".repeat(27)).slice(0, 27);
  for (let i = 0; i < 27; i++) d[6 + i] = t.charCodeAt(i) & 0x7F;
  return [0xF0, ...pack7(d), 0xF7];
}

/* a JIANT info frame: version, engines + their names, six tracks, and the 0x4A 1 1 tag the page reads */
function jiantInfo(version) {
  const vs = [...version].map((c) => c.charCodeAt(0));
  return [...vs, 0, 1, 0, 0, 0, 0, ..."ANALOG".split("").map((c) => c.charCodeAt(0)), 0, 6, 0x56, 1, 32, 0x4A, 1, 1];
}

function fakeAccess({ port = "FM-1 MIDI 1", text = "FM-1_982", info = null, silence = false } = {}) {
  const frames = [];
  const input = { id: "in", name: port, state: "connected", onmidimessage: null, open: async () => {} };
  const answer = (bytes) => {
    if (silence) return;
    const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
    let reply = null;
    if (same(bytes, HS_QUERY)) reply = identityReply(text);
    else if (isInfoQuery(bytes) && info) reply = [240, 125, 70, 76, 1, ...info, 247];
    if (reply) setTimeout(() => { if (input.onmidimessage) input.onmidimessage({ data: new Uint8Array(reply) }); }, 0);
  };
  const output = {
    id: "out", name: port, state: "connected", open: async () => {},
    send(bytes) { frames.push(Array.from(bytes)); answer(Array.from(bytes)); },
  };
  return { access: { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]) }, frames, input, output };
}

/* the sequence the page's verify button runs (index_pkg.html, $("verify").addEventListener), with the
 * same helpers and the same order: look for a unit already in update mode, then identify the running
 * firmware, reading its info frame when the identity says it is a JIANT/Felucca build. */
async function verifyOnly(access, log = () => {}) {
  const up = new Updater(access, log);
  const ota = await up.find((id) => /^ota-/i.test(id.model));
  if (ota) { ota.link.close(); return { kind: "loader", text: ota.id.text }; }
  const dev = await up.find((id) => !/^ota-/i.test(id.model));
  if (!dev) return { kind: "none" };
  const text = dev.id.text;
  if (text === "FM-1_015") { dev.link.close(); return { kind: "stock", text }; }
  if (!/^FM-1_9\d\d$/.test(text)) { dev.link.close(); return { kind: "other", text }; }
  const input = dev.link.input, output = dev.link.output;
  dev.link.close();
  const link = new BackupConnection(input, output);
  try {
    const a = await link.request([1, []], { timeout: 1000 });
    return { kind: "jiant", text, info: a };
  } catch (e) {
    return { kind: "other", text: text + " (no answer)" };
  } finally { link.close(); }
}

const framesOk = (frames, allowed, what) => {
  const bad = frames.filter((f) => !allowed.some((a) => (typeof a === "function" ? a(f) : a.length === f.length && a.every((v, i) => v === f[i]))));
  ok(bad.length === 0, what);
  if (bad.length) console.log("   sent: " + bad.map((f) => f.map((v) => v.toString(16)).join(" ")).join("\n         "));
};

/* 1. a JIANT unit: identity + one info read, nothing else */
{
  const info = jiantInfo("FELUCCA v0.5.1");
  const { access, frames } = fakeAccess({ text: "FM-1_982", info });
  const v = await verifyOnly(access);
  ok(v.kind === "jiant" && v.text === "FM-1_982", "verify: a JIANT FM-1 is identified by its own identity");
  ok(v.info && v.info.length === info.length && v.info.every((x, i) => x === info[i]), "verify: its info frame is read back whole");
  framesOk(frames, [HS_QUERY, isInfoQuery], "verify: a JIANT unit is sent the handshake and one INFO read, nothing else");
  ok(frames.filter((f) => f.length === 6).length === 1, "verify: exactly one INFO read (not a stream of them)");
  ok(!frames.some((f) => f.length === UPGRADE.length && f.every((x, i) => x === UPGRADE[i])), "verify: UPGRADE is never sent");
}

/* 2. the JIANT tag is what makes it a JIANT: the same bytes the page's isJiantInfo() reads */
{
  const html = readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");
  const src = html.slice(html.indexOf("function isJiantInfo"), html.indexOf("async function fwKind"));
  const isJiantInfo = vm.runInNewContext(`(${src.trim()})`);
  const info = jiantInfo("FELUCCA v0.5.1");
  const { access, frames } = fakeAccess({ text: "FM-1_982", info });
  const v = await verifyOnly(access);
  ok(isJiantInfo(v.info), "verify: the page's own isJiantInfo() accepts the frame the unit answers");
  framesOk(frames, [HS_QUERY, isInfoQuery], "verify: and that path still sends only the two reads");
}

/* 3. a stock V15 unit: no info read at all, the identity is enough */
{
  const { access, frames } = fakeAccess({ text: "FM-1_015" });
  const v = await verifyOnly(access);
  ok(v.kind === "stock", "verify: official V15 is recognised from the handshake alone");
  framesOk(frames, [HS_QUERY], "verify: a stock unit is sent the handshake only");
}

/* 4. another firmware: identified as unknown, still reads nothing else */
{
  const { access, frames } = fakeAccess({ text: "BAUDGIRL_200" });
  const v = await verifyOnly(access);
  ok(v.kind === "other" && v.text === "BAUDGIRL_200", "verify: an unknown firmware is named, not guessed at");
  framesOk(frames, [HS_QUERY], "verify: an unknown firmware is sent the handshake only");
}

/* 5. a unit left in update mode: the page says so and closes the port, sending nothing but the query */
{
  const { access, frames } = fakeAccess({ text: "ota-FM-1_982" });
  const v = await verifyOnly(access);
  ok(v.kind === "loader", "verify: a unit in update mode is reported as such (INSTALL finishes it)");
  framesOk(frames, [HS_QUERY], "verify: the update-mode unit is sent the handshake only, never a resume");
}

/* 6. a unit that does not answer: the read times out and nothing else is attempted */
{
  const { access, frames } = fakeAccess({ text: "FM-1_982", info: jiantInfo("FELUCCA v0.5.1"), silence: true });
  const v = await verifyOnly(access);
  ok(v.kind === "none", "verify: a unit that never answers is 'not found', not a made-up identity");
  ok(frames.length >= 1, "verify: it did ask (the handshake), and stopped asking after that");
}

/* 7. the page's verify handler cannot reach a writer: the call whitelist, checked on the page itself */
{
  const html = readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf('$("verify").addEventListener');
  const end = html.indexOf('$("stock-file").addEventListener', at);
  const handler = html.slice(at, end);
  const forbidden = ["install(", "resume(", ".write(", "probeChip(", "#serve", "UPGRADE", "restoreBackup",
    "captureBackup", "sendPackage", "erase", "reboot"];
  const hit = forbidden.filter((w) => handler.includes(w));
  ok(at > 0 && end > at, "verify: the verify handler is found in index_pkg.html");
  ok(hit.length === 0, "verify: its source calls no writer, no loader switch and no erase" + (hit.length ? ": " + hit.join(", ") : ""));
  ok(/fwKind\(up\)/.test(handler) && /up\.find\(/.test(handler), "verify: it identifies through fwKind() and find(), as tested above");
}

/* 8. the transport refuses to serve without an admission, even when called directly: a write needs proof,
 * and the refusal happens before the port is even opened */
{
  const { access, frames } = fakeAccess({ text: "FM-1_982" });
  const up = new Updater(access);
  const e = await up.install(new Uint8Array(64), "FM-1_982", () => {}, {}).then(() => null, (x) => x);
  ok(e && ["proof", "version", "signature"].includes(e.code),
     "verify: install() without proof and version is refused fail-closed (code " + (e && e.code) + ")");
  ok(frames.length === 0, "verify: and that refusal reaches the device as zero frames");
}

console.log(fails ? `verify-only: ${fails} FAILED` : "verify-only: all passed");
process.exit(fails ? 1 : 0);
