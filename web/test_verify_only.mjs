// SPDX-License-Identifier: GPL-3.0-only
// "Verify only" must be read-only, not by name but on the wire.
//
// Every case below drives the real module (web/fm1verify.js, the one make_site.py inlines into
// the installer page) with a fake WebMIDI access that records every byte the page sends. The
// assertions are about that log, not about the intention of the code: after a verify, the only
// frame on the wire is the identity query, there is no UPGRADE (F0 22 24 35 7F F7), no 0x30
// answer, no E0000000/F0000000 acknowledgement and no archive PUT, whatever the device says,
// however it answers, and whatever goes wrong while it answers.

import { verifyDevice, READ_ONLY_QUERY } from "./fm1verify.js";
import * as mod from "./fm1verify.js";
import { Updater, requireProof } from "./fm1ota.js";

let bad = 0, total = 0;
const check = (name, ok) => {
  total++;
  if (!ok) { bad++; console.log(`FAIL ${name}`); } else console.log(`ok   ${name}`);
};

// --- the protocol constants the test recognises -------------------------------------------
const isQuery = (b) => b.length === READ_ONLY_QUERY.length && b.every((v, i) => v === READ_ONLY_QUERY[i]);
const isUpgrade = (b) => b.length === 6 && b[0] === 0xF0 && b[1] === 0x22 && b[2] === 0x24 && b[3] === 0x35 && b[4] === 0x7F && b[5] === 0xF7;
const isImageAnswer = (b) => b[0] === 0xF0 && b.length > 8 && b[1] === 0x00 && b[2] === 0x59 && b[3] === 0x30;
const isFinishAck = (b) => isImageAnswer(b) && /success/.test(String.fromCharCode(...b));
const isArchivePut = (b) => b.length > 5 && b[0] === 0xF0 && b[1] === 0x7D && b[2] === 0x46 && b[3] === 0x4C && b[4] === 67;

function identityFrame(text) {                       // 0x45 identity answer (34 bytes, 7-bit packed)
  const d = [0, 0x59, 0x11, 0, 0, 0, ...Array.from(text.padEnd(27, "\0")).map((c) => c.charCodeAt(0)), 0];   // 6 header + 27 + 1 = 34
  const out = []; let acc = 0, nb = 0;
  for (const b of d) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } }
  if (nb) out.push(acc & 0x7F);
  return [0xF0, ...out, 0xF7];
}

// a fake MIDI access: one in/out pair per named port, and a script for what the device answers
function access(names, script = {}) {
  const sent = [];
  const inputs = new Map(), outputs = new Map();
  for (const name of names) {
    const inPort = { name, state: "connected", onmidimessage: null, open: async () => {} };
    const outPort = {
      name, state: "connected", open: async () => {},
      send(bytes) {
        const b = Array.from(bytes);
        sent.push({ name, bytes: b });
        const answer = script[name];
        if (answer && isQuery(b)) {
          const frames = answer(b) || [];
          for (const f of frames) setTimeout(() => inPort.onmidimessage && inPort.onmidimessage({ data: Uint8Array.from(f) }), 0);
        }
        if (answer && answer.disconnectAfter && answer.disconnectAfter(b)) {
          inPort.state = "disconnected"; outPort.state = "disconnected";
        }
      },
    };
    inputs.set(name, inPort); outputs.set(name, outPort);
  }
  return { sent, midi: { inputs, outputs }, closeAll: () => { for (const p of inputs.values()) p.state = "disconnected"; } };
}

// --- the verdict on any log: the identity query and nothing else ---------------------------
function onlyQueries(sent) {
  return sent.length > 0 && sent.every(({ bytes }) => isQuery(bytes));
}
function anyWrite(sent) {
  return sent.some(({ bytes }) => isUpgrade(bytes) || isImageAnswer(bytes) || isFinishAck(bytes) || isArchivePut(bytes));
}

// --- cases ---------------------------------------------------------------------------------
{
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_982")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("our firmware: reported as ours, with its own identity", fw.kind === "ours" && fw.text === "FM-1_982" && fw.version === 982);
  check("our firmware: only the identity query was sent", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("official V15: reported as stock", fw.kind === "stock" && fw.text === "FM-1_015");
  check("official V15: only the identity query was sent", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  // JIANT, or any FM-1 firmware that is not this one: its identity is reported, never "unknown"
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_210")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("JIANT/another FM-1: its identity is reported as it answered", fw.kind === "other" && fw.text === "FM-1_210" && fw.version === 210);
  check("JIANT/another FM-1: only the identity query was sent", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  // the unit this was reported from: a real FM-1 running JIANT answers FM-1_909. It is a device,
  // its firmware is simply not one this installer knows, and verify must say exactly that.
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_909")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("FM-1_909 (JIANT): detected as an FM-1, model and version read from the frame",
    fw.kind === "other" && fw.found === true && fw.model === "FM-1" && fw.version === 909 && fw.text === "FM-1_909");
  check("FM-1_909 (JIANT): the firmware is reported as not recognised, and no install is authorised",
    fw.recognised === false && fw.canInstall === false);
  check("FM-1_909 (JIANT): only the identity query was sent", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  // every answer says what it is: found / recognised / canInstall, and only the reading one moves
  const cases = [["FM-1_015", "stock", true], ["FM-1_982", "ours", true], ["ota-FM-1_982", "loader", true],
                 ["FM-1_909", "other", false], ["FM-1_210", "other", false]];
  for (const [id, kind, ok] of cases) {
    const a = access(["FM-1"], { "FM-1": () => [identityFrame(id)] });
    const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
    check(`${id}: kind ${kind}, recognised ${ok}, never an install authorisation`,
      fw.kind === kind && fw.found === true && fw.recognised === ok && fw.canInstall === false);
  }
}
{
  // the texts the button can print must exist in every language of the page, and say what happened
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf('$("verify").addEventListener');
  const body = html.slice(at, html.indexOf("\n});", at));
  const keys = [...body.matchAll(/sayK\("(v[A-Za-z]+)"/g)].map((m) => m[1]);
  check("the button names its messages", keys.length >= 5 && keys.includes("vOther"));
  const langs = ["en", "fr", "es", "zh"];
  const missing = [];
  for (const k of keys) {
    const n = (html.match(new RegExp(k + ": \"", "g")) || []).length;
    if (n < langs.length) missing.push(`${k} x${n}`);
  }
  check("every message exists in en / fr / es / zh (a missing key printed the key itself)", !missing.length);
  check("the not-recognised message says the device was detected and that nothing was written",
    /detected, but its firmware is not one this installer knows/.test(html) && /nothing was written/.test(html));
}
{
  // a firmware that does not implement the query at all (JIANT may be one): the port is named,
  // nothing else is sent, and it is not reported as "no FM-1 found"
  const a = access(["FM-1"], { "FM-1": () => [] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("a silent FM-1 port: reported as silent, with the port name", fw.kind === "silent" && fw.ports.includes("FM-1"));
  check("a silent FM-1 port: only identity queries were sent", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  const a = access([], {});
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("no FM-1 at all: none, nothing sent", fw.kind === "none" && a.sent.length === 0);
}
{
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_982")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("the update loader: reported as loader (an install was cut off)", fw.kind === "loader" && fw.model === "ota-FM-1");
  check("the update loader: only the identity query was sent, no resume", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  // malformed and hostile answers: a truncated frame, a wrong header, a huge blob, another command
  const junk = [[0xF0, 0x01, 0xF7], [0xF0], [0xF0, 0x7D, 0x46, 0x4C, 65, 0xF7]];
  for (const j of junk) {
    const a = access(["FM-1"], { "FM-1": () => [j] });
    const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
    check(`a malformed answer (${j.length} bytes): still silent, no write`, fw.kind === "silent" && onlyQueries(a.sent) && !anyWrite(a.sent));
  }
}
{
  // the port vanishes in the middle of the handshake
  const a = access(["FM-1"], { "FM-1": (b) => { a.closeAll(); return []; } });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("a disconnect mid-handshake: reported, nothing else sent", (fw.kind === "silent" || fw.kind === "none") && !anyWrite(a.sent) && onlyQueries(a.sent));
}
{
  // other gear on the same computer is never probed
  const a = access(["Roland JUNO-X", "Microphone"], { "Roland JUNO-X": () => [identityFrame("FM-1_982")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("gear that is not an FM-1 is not probed at all", fw.kind === "none" && a.sent.length === 0);
}
{
  // a package tag that is not this device must not make it "ours"
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_982")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_983" });
  check("a different package tag: reported as another firmware", fw.kind === "other" && fw.text === "FM-1_982");
}
{
  // the module itself: no exported way to write, no import of the writer it would need
  const keys = Object.keys(mod).sort();
  check("fm1verify.js exports only verifyDevice and the query it is allowed to send",
    keys.join(",") === "READ_ONLY_QUERY,verifyDevice");
  check("fm1verify.js exports no install/write/resume entry point",
    !keys.some((k) => /install|write|resume|upgrade|serve|put/i.test(k)));
}
{
  // and the page must call it, not a handler of its own
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf('$("verify").addEventListener');
  const body = html.slice(at, html.indexOf("\n});", at));
  check("the page's Verify only button calls verifyDevice", /verifyDevice\s*\(/.test(body));
  check("the page's Verify only handler names no write path",
    !/\b(install|resume|write|chipEvidence|probeChip|captureBackup)\s*\(/.test(body) && !/UPGRADE/.test(body));
}

{
  // the verifier's read-only property is a property of the module, and the writer's is a refusal:
  // install() called directly, with no proof and no installed version, must not open the port at all
  const sent = [];
  const port = { name: "FM-1", state: "connected", open: async () => {}, send(b) { sent.push(Array.from(b)); } };
  const midi = { inputs: new Map([["in", { ...port, onmidimessage: null }]]), outputs: new Map([["out", port]]) };
  const up = new Updater(midi);
  const e = await up.install(new Uint8Array(64), "FM-1_982", () => {}, {}).then(() => null, (x) => x);
  check("install() with no proof and no installed version is refused fail-closed",
    !!e && ["proof", "version", "signature"].includes(e.code));
  check("and that refusal reaches the device as zero frames", sent.length === 0);
}
{
  // the page may not reach a write even by name: the handler's whole vocabulary is checked, not
  // only the functions the module happens to export
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf('$("verify").addEventListener');
  const body = html.slice(at, html.indexOf("\n});", at));
  check("the Verify only handler names no erase, restore, backup capture or reboot",
    !/\b(erase|restoreBackup|captureBackup|reboot|sendPackage)\b/.test(body));
}

// --- the diagnosis, not just the verdict -----------------------------------------------------
{
  // the unit that motivated this: an FM-1 running Jiant answers FM-1_909. The hardware is an FM-1;
  // 909 is that firmware's own numbering, not a hardware revision and not a byte pattern to match.
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_909")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("JIANT on a real unit (FM-1_909): the hardware is recognised as an FM-1",
    fw.hardware === "FM-1" && fw.kind === "other" && fw.found === true);
  check("JIANT (FM-1_909): the firmware is reported as another build, with its own version",
    fw.firmware === "other" && fw.version === 909 && fw.text === "FM-1_909");
  check("JIANT (FM-1_909): canInstall stays false", fw.canInstall === false && fw.recognised === false);
  check("JIANT (FM-1_909): still one frame type on the wire", onlyQueries(a.sent) && !anyWrite(a.sent));
  const byKey = Object.fromEntries((fw.fields || []).map((f) => [f.key, f]));
  check("the report names the hardware field with its source",
    byKey["hardware.model"] && byKey["hardware.model"].value === "FM-1" && byKey["hardware.model"].source === "identity answer");
  check("the report names the firmware family and its version",
    byKey["firmware.family"].value === "other" && byKey["firmware.version"].value === "909");
  check("the report says what it could not read (JEDEC, flash size, SoC, clock, VID/PID)",
    ["flash.jedec", "flash.size", "soc", "clock", "usb.vidpid"].every((k) => (fw.missing || []).some((m) => m.key === k && m.why)));
  check("the report never claims the SoC or the flash as a reading",
    !(fw.fields || []).some((f) => f.key === "soc" || f.key === "flash.jedec"));
}
{
  // every kind leaves canInstall false: the diagnosis never authorises an install
  const cases = [["FM-1_015", "stock"], ["FM-1_982", "ours"], ["FM-1_909", "other"], ["ota-FM-1_982", "loader"]];
  let allFalse = true;
  for (const [id, kind] of cases) {
    const a = access(["FM-1"], { "FM-1": () => [identityFrame(id)] });
    const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
    if (fw.kind !== kind || fw.canInstall !== false) allFalse = false;
  }
  check("canInstall is false for stock, ours, another build and the loader", allFalse);
}
{
  // the descriptors are read before any frame: a port that names its maker is reported as such
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_909")] });
  for (const port of a.midi.inputs.values()) { port.manufacturer = "M-VAVE"; port.version = "1.00"; port.id = "usb-1"; }
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  const byKey = Object.fromEntries((fw.fields || []).map((f) => [f.key, f]));
  check("the USB-MIDI manufacturer is reported when the port exposes it",
    byKey["port.manufacturer"].value === "M-VAVE" && /descriptor/i.test(byKey["port.manufacturer"].source));
  check("the USB descriptor version is reported too", byKey["port.descriptorVersion"].value === "1.00");
  check("and the readout counts as a diagnosis, not as an authorisation", fw.canInstall === false);
}
{
  // the page has to show it: a panel, filled from the report, with the limits named
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  check("the page has a report panel and a row list", /id="vrep"/.test(html) && /id="vrep-rows"/.test(html));
  check("the panel is filled from the report's fields and limits",
    /function showVerify\(fw\)/.test(html) && /vw|fw\.fields/.test(html) && /fw\.missing/.test(html));
  check("the Verify handler renders the report", /showVerify\(fw\)/.test(html));
  check("the panel says detection is not an authorisation", /canInstall|not an authorisation|no autoriza/.test(html));
}

// --- the package is not the device -----------------------------------------------------------
// The reported failure: a stock FM-1 printed "Verify: official V15 (FM-1_015). This package can be
// installed. FM-1_982". FM-1_982 is the tag of the package the page offers, and the handler appended
// it to a sentence about the unit, so the only identifier left visible looked like the installed
// firmware. The fixes below keep the two identities in separate fields, in separate rows, and never
// let the package tag decide what the device is.
{
  const fw = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] }).midi,
    { product: "FM-1_982", pkg: { version: "v1.9.0", size: 611163, sha256: "372a5fde126ab6f1" } });
  const byKey = Object.fromEntries((fw.fields || []).map((f) => [f.key, f]));
  check("stock unit: the device identity is what the unit answered (FM-1_015)",
    byKey["firmware.identity"].value === "FM-1_015" && fw.text === "FM-1_015");
  check("stock unit: the package is named in its own rows (FM-1_982), never as the device's identity",
    byKey["pkg.identity"].value === "FM-1_982" && byKey["firmware.identity"].value !== "FM-1_982");
  check("stock unit: the package rows carry the release version, the size and the expected hash, with sources",
    byKey["pkg.release"].value === "v1.9.0" && /611163/.test(byKey["pkg.size"].value) &&
    /^372a5fde/.test(byKey["pkg.sha256"].value) && /build record/.test(byKey["pkg.identity"].source));
  check("stock unit + this package: compatibility says it replaces official V15, and authorisation stays none",
    fw.compatibility === "replaces-stock" && byKey["install.auth"].value === "none" && fw.authorised === false);
  check("stock unit: the package tag is in no device row at all",
    !(fw.fields || []).some((f) => /^(hardware|firmware|port)\./.test(f.key) && f.value === "FM-1_982"));
}
{
  // the reverse: a unit answering FM-1_982 is NOT stock, whatever the package is called. Comparing the
  // package against FM-1_015 must never promote a device that answered something else into "stock".
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_982")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_015", pkg: { version: "v15", size: 1, sha256: "00" } });
  check("a unit answering FM-1_982 is not stock, even when the package is a V15 one",
    fw.kind === "other" && fw.firmware === "other" && fw.hardware === "FM-1" && fw.recognised === false);
  check("that unit is reported with its own identity and is not offered an install",
    fw.text === "FM-1_982" && fw.compatibility === "blocked-firmware" && fw.canInstall === false);
  check("and it still heard nothing but the identity query", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  // our own firmware already running: a reinstall, not stock and not "another build"
  const fw = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_982")] }).midi,
    { product: "FM-1_982" });
  check("FM-1_982 + package FM-1_982: ours, same firmware (a reinstall)",
    fw.kind === "ours" && fw.compatibility === "reinstall-same" && fw.canInstall === false);
}
{
  // the loader, and a package for a different model
  const ldr = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_982")] }).midi,
    { product: "FM-1_982" });
  check("the update loader + this package: it can finish the interrupted install",
    ldr.kind === "loader" && ldr.compatibility === "finish-loader" && ldr.authorised === false);
  const wrong = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("FM-2_982")] }).midi,
    { product: "FM-1_982" });
  check("a unit of another model is blocked by model, not by firmware",
    wrong.kind === "other" && wrong.compatibility === "blocked-model" && wrong.hardware === "");
}
{
  // no answer and no port: the package block is still there, and the unit is "unproven", not compatible
  const silent = await verifyDevice(access(["FM-1"], { "FM-1": () => [] }).midi, { product: "FM-1_982" });
  const none = await verifyDevice(access([], {}).midi, { product: "FM-1_982" });
  const has = (fw, key) => (fw.fields || []).some((f) => f.key === key);
  check("a silent port: unproven, never compatible, and the package is still named",
    silent.compatibility === "unproven-device" && has(silent, "pkg.identity") && silent.canInstall === false);
  check("no port at all: no device to compare, and the package is still named",
    none.compatibility === "no-device" && has(none, "pkg.identity") && none.canInstall === false);
  check("neither of them claims an authorisation", silent.authorised === false && none.authorised === false);
}
{
  // the handler and the panel: the status line speaks about the unit, the package gets its own rows
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf('$("verify").addEventListener');
  const body = html.slice(at, html.indexOf("\n});", at));
  check("the stock message is no longer given the package tag as an argument",
    /sayK\("vStock"\)/.test(body) && !/sayK\("vStock"\s*,/.test(body));
  check("the Verify handler passes the package to the report",
    /pkg:\s*\{[^}]*version[^}]*sha256/.test(body));
  const vstocks = [...html.matchAll(/\bvStock:\s*"([^"]*)"/g)].map((m) => m[1]);   // not navStock:
  check("every stock message names the unit's identity and none of them names the package's",
    vstocks.length >= 4 && vstocks.every((s) => /FM-1_015/.test(s) && !/FM-1_9\d\d/.test(s)));
  check("the panel renders the package rows and translates the compatibility and authorisation values",
    /vf\("compat\."\s*\+/.test(html) && /vf\("auth\."\s*\+/.test(html) && /"pkg\.identity"/.test(html));
  const langBlocks = ["en", "es", "zh"].map((l) => html.slice(html.indexOf(`\n  ${l}: {`, html.indexOf("const VF"))));
  check("the new panel labels exist in en, es and zh",
    langBlocks.every((b) => ["pkg.identity", "pkg.release", "pkg.sha256", "install.auth", "compat.no-device"]
      .every((k) => b.slice(0, b.indexOf("\n  },")).includes(`"${k}"`) || b.slice(0, b.indexOf("\n  },")).includes(`${k}:`))));
}

// --- the write routes refuse internally, not only at the buttons ---------------------------------
// verify only is read-only by construction; this is the other half of the same property: the routes
// that DO write cannot be reached by calling them directly with nothing to authorise them, and their
// refusal is a count of frames, not a message.
{
  const sent = [];
  const port = { name: "FM-1", state: "connected", open: async () => {}, send(b) { sent.push(Array.from(b)); } };
  const midi = { inputs: new Map([["in", { ...port, onmidimessage: null }]]), outputs: new Map([["out", port]]) };
  const up = new Updater(midi);
  const link = { send(b) { sent.push(Array.from(b)); }, read: async () => null, close() {}, lost: false };
  const ota = { link, id: { model: "ota-FM-1", text: "ota-FM-1_982" } };
  const pkg = new Uint8Array(2000);
  const e1 = await up.write(ota, pkg, () => {}, { product: "FM-1_982" }).then(() => null, (x) => x);
  check("write() called directly, with no proof, is refused and sends nothing",
    !!e1 && ["proof", "version", "signature"].includes(e1.code) && sent.length === 0);
  const e2 = await up.resume(pkg, () => {}, "FM-1_982", {}).then(() => null, (x) => x);
  check("resume() called directly, with no proof, is refused and sends nothing",
    !!e2 && ["proof", "version", "signature"].includes(e2.code) && sent.length === 0);
  const manifest = { format: "qg1-release-1", product: "FM-1", version: "1.9.0", min_firmware: "0.0",
    sha256: "00".repeat(32), size: pkg.length, keyid: "k", sig: "00".repeat(64) };
  const e3 = await requireProof({ pkg, manifest, keys: {}, product: "FM-1_982", firmware: "1.9.0" })
    .then(() => null, (x) => x);
  check("with the pinned key map empty, requireProof refuses the package: a hash is integrity, not authorship",
    !!e3 && e3.code === "signature" && sent.length === 0);
}

// --- the page's script parses ---------------------------------------------------------------
// A missing comma in one language block of the page's messages took the whole installer down in the
// browser ("SyntaxError: Unexpected identifier 'vOurs'") while every string looked right in the
// source. The page is a module with two placeholders that make_site.py fills, so this assembles the
// same text it produces and asks Node to parse it: no browser, no build, and the failure is loud.
{
  const fs = await import("node:fs/promises");
  const os = await import("node:os");
  const path = await import("node:path");
  const { execFileSync } = await import("node:child_process");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const m = /<script type="module">([\s\S]*?)<\/script>/.exec(html);
  const body = m ? m[1].replace("/*LIB*/", "").replace("/*META*/", "{}") : "";
  const tmp = path.join(os.tmpdir(), `qg-installer-${process.pid}.mjs`);
  await fs.writeFile(tmp, body);
  let err = "";
  try { execFileSync(process.execPath, ["--check", tmp], { stdio: "pipe" }); } catch (e) { err = String(e.stderr || e.message); }
  await fs.unlink(tmp).catch(() => {});
  check("the installer page's script parses (a missing comma is a dead installer, not a typo)", body.length > 1000 && !err);
  if (err) console.log(err.split("\n").slice(0, 4).join("\n"));
}

console.log(`verify only: ${total} checks, ${bad} FAILED`);
if (bad) process.exit(1);
