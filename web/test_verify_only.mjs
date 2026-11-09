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
import * as diag from "./fm1diag.js";
import { Updater, requireProof, verifyManifest, requireChip, chipCheck, pack7, unpack7 } from "./fm1ota.js";

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
  // the two loaders share the port name and are told apart by the version the unit answers:
  // 900 is this project's loader (resumable), any other is the vendor's update mode (not a target).
  const cases = [["FM-1_015", "stock", true], ["FM-1_982", "ours", true], ["ota-FM-1_900", "ourloader", true],
                 ["ota-FM-1_015", "otherloader", true],
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
  const at = html.indexOf("async function runVerify()");
  const body = html.slice(at, html.indexOf("\n}\n$(" + JSON.stringify("verify") + ").addEventListener", at));
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
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_900")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  check("this project's loader (ota-FM-1_900): ourloader, an install was cut off",
    fw.kind === "ourloader" && fw.model === "ota-FM-1" && fw.compatibility === "finish-loader");
  check("this project's loader: only the identity query was sent, no resume", onlyQueries(a.sent) && !anyWrite(a.sent));
  /* The vendor's own update mode answers the same port name. Calling it "a loader we can resume"
   * is the mistake that would feed a Felucca image to a program nobody has shown to accept one, so
   * the report has to say foreign, and say it without sending a frame. */
  const b = access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_015")] });
  const fw2 = await verifyDevice(b.midi, { product: "FM-1_982" });
  check("the vendor's update mode (ota-FM-1_015): otherloader, reported as foreign, not resumable",
    fw2.kind === "otherloader" && fw2.compatibility === "foreign-loader" && fw2.compatible === false);
  check("the vendor's update mode: nothing was sent to it", onlyQueries(b.sent) && !anyWrite(b.sent));
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
  const found = await new Updater(a.midi).find();          // the install path: no probeAll
  check("the install path never probes gear that is not an FM-1", found === null && a.sent.length === 0);
  const b = access(["Roland JUNO-X", "Microphone"], { "Roland JUNO-X": () => [identityFrame("FM-1_982")] });
  const fw = await verifyDevice(b.midi, { product: "FM-1_982" });
  check("Verify Only probes it, with the read-only query and nothing else",
    fw.kind === "ours" && onlyQueries(b.sent) && !anyWrite(b.sent));
}
{
  // Windows exposes one device under two different names. Pairing by exact name found nothing, so the
  // unit looked absent even though both directions were there.
  const sent = [];
  const input = { name: "MIDIIN2 (FM-1 Midi)", state: "connected", onmidimessage: null, open: async () => {} };
  const output = { name: "MIDIOUT2 (FM-1 Midi)", state: "connected", open: async () => {},
    send(bytes) {
      const b = Array.from(bytes);
      sent.push({ name: output.name, bytes: b });        // same shape the fake access records
      if (isQuery(b)) setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from(identityFrame("FM-1_015")) }), 0);
    } };
  const midi = { inputs: new Map([[input.name, input]]), outputs: new Map([[output.name, output]]) };
  const fw = await verifyDevice(midi, { product: "FM-1_982" });
  check("one unit under two Windows names (MIDIIN2 / MIDIOUT2) is paired and read",
    fw.kind === "stock" && fw.text === "FM-1_015" && onlyQueries(sent) && !anyWrite(sent));
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
  /* verifyPackage is an offline check of the package bytes against the manifest: it opens no port
     and has no way to send anything, so it belongs to the read-only surface exactly as verifyDevice
     does. What must not exist is an install/write/resume entry point, and that is the next check. */
  check("fm1verify.js exports only the read-only surface: verifyDevice, verifyPackage and the query",
    keys.join(",") === "READ_ONLY_QUERY,verifyDevice,verifyPackage");
  check("fm1verify.js exports no install/write/resume entry point",
    !keys.some((k) => /install|write|resume|upgrade|serve|put/i.test(k)));
}
{
  // and the page must call it, not a handler of its own. The handler is a named function now (a port
  // change re-runs it), so the check reads that function, from its definition to its closing brace.
  const fs = await import("node:fs/promises");
  const html = await fs.readFile(new URL("./index_pkg.html", import.meta.url), "utf8");
  const at = html.indexOf("async function runVerify()");
  const body = html.slice(at, html.indexOf("\n}\n$(" + JSON.stringify("verify") + ").addEventListener", at));
  check("the page's Verify only button calls verifyDevice", /verifyDevice\s*\(/.test(body));
  check("the page's Verify only handler names no write path",
    !/\b(install|resume|write|chipEvidence|probeChip|captureBackup)\s*\(/.test(body) && !/UPGRADE/.test(body));
  check("a port change re-runs Verify Only instead of needing a reload",
    /addEventListener\("statechange"/.test(html) && /if \(lastVerify && !writing\) runVerify\(\)/.test(html));
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
  const at = html.indexOf("async function runVerify()");
  const body = html.slice(at, html.indexOf("\n}\n$(" + JSON.stringify("verify") + ").addEventListener", at));
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
  const cases = [["FM-1_015", "stock"], ["FM-1_982", "ours"], ["FM-1_909", "other"],
                 ["ota-FM-1_900", "ourloader"], ["ota-FM-1_015", "otherloader"]];
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
  check("the panel renders the checklist and the verdict, not only the field list",
    /id="vrep-gate"/.test(html) && /id="vrep-verdict"/.test(html) &&
    /for \(const row of fw\.gate \|\| \[\]\)/.test(html) && /fw\.verdict/.test(html));
  check("every checklist row and state has words in the three languages",
    ["en", "es", "zh"].every((l) => new RegExp(`\\b${l}: \\{`).test(html)) &&
    (html.match(/"gate\.detect"/g) || []).length === 3 && (html.match(/"state\.pass"/g) || []).length === 3);
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
  const ldr = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_900")] }).midi,
    { product: "FM-1_982" });
  check("this project's loader + this package: it can finish the interrupted install",
    ldr.kind === "ourloader" && ldr.compatibility === "finish-loader" && ldr.authorised === false);
  const vend = await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("ota-FM-1_015")] }).midi,
    { product: "FM-1_982" });
  check("the vendor's loader + this package: blocked as foreign, never \"finish the install\"",
    vend.kind === "otherloader" && vend.compatibility === "foreign-loader" && vend.compatible === false);
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
  const at = html.indexOf("async function runVerify()");
  const body = html.slice(at, html.indexOf("\n}\n$(" + JSON.stringify("verify") + ").addEventListener", at));
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

// --- the report says where every number came from ----------------------------------------------
// The panel is a diagnosis, so a value that was not read from the unit must not look read from the unit.
// The build record and the key map are the two things the page knows and the unit cannot, and both are
// printed under their own labels; the platform block is documented, and says so in every row.
{
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: {
    version: "v1.9.0", size: 611163, sha256: "306964883e271ba3",
    build: { commit: "abcdef0123456789abcdef0123456789abcdef01", version: "v1.9.0",
             artifacts: { "felucca.bin": { bytes: 559552, sha256: "c25cc427a24ab682" },
                          "loader/ota.bin": { bytes: 8007, sha256: "468fb0e20eddf855" } } },
    keys: {},
  } });
  const row = (k) => (fw.fields || []).find((f) => f.key === k) || {};
  check("the report names the package's build commit (the package's, not the unit's)",
        String(row("pkg.build").value || "").includes("abcdef0") &&
        row("pkg.build").source === "the build record inside the package");
  check("the report names the image and the loader the package was packed from",
        String(row("pkg.binary").value || "").startsWith("559552 B") && String(row("pkg.loader").value || "").startsWith("8007 B"));
  check("with an empty key map the report says no-key, and says why",
        row("install.why").value === "no-key" && /key map is empty/.test(row("install.why").source || ""));
  const st = (fw.fields || []).filter((f) => String(f.key).startsWith("static."));
  check("the platform block is present and every row is marked documented, never read",
        st.length === 4 && st.every((f) => f.confidence === "documented" && /not read from the unit/.test(f.source)));
  check("no row from the platform block claims to have been answered by the device",
        st.every((f) => f.source !== "identity answer"));
  const devRows = (fw.fields || []).filter((f) => /^(hardware|firmware)\./.test(f.key));
  check("every device row still cites the identity answer", devRows.length > 0 && devRows.every((f) => f.source === "identity answer"));
}

// --- what the read-only exchange measured about itself -----------------------------------------
// These are the only extra numbers a read-only check can honestly produce: how many identity frames
// went out, on which port, and how long the answer took. They are reported with the run, and they are
// the reason the report does not need to pretend it read a JEDEC id, a clock or a boot-loader id.
{
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  const row = (k) => (fw.fields || []).find((f) => f.key === k) || {};
  const p = fw.reading.probe || {};
  check("the report counts the identity frames it actually sent (1 here)",
        p.frames === a.sent.length && a.sent.length === 1 && row("query.frames").value === "1");
  check("the report measures the answer latency instead of guessing it",
        typeof p.latencyMs === "number" && p.latencyMs >= 0 && /^\d+ ms$/.test(row("query.latencyMs").value) &&
        row("query.latencyMs").source === "measured from the first frame to the answer");
  check("the report names the port that answered",
        p.answered === "FM-1" && row("query.answer").value === "FM-1");
  check("the port's descriptor state and type are reported as descriptor data, not as device data",
        row("port.state").source === "USB-MIDI descriptor" && row("port.type").source === "USB-MIDI descriptor");
  check("the browsing context's secure-context state is reported (WebMIDI needs it)",
        row("usb.secureContext").source === "this browsing context (WebMIDI needs it)");
}
{
  /* a port that never answers: two identity frames, no latency, and the report says "no answer" */
  const a = access(["FM-1"], { "FM-1": () => [] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  const row = (k) => (fw.fields || []).find((f) => f.key === k) || {};
  check("silent unit: both identity frames are counted and the report says no answer",
        fw.kind === "silent" && a.sent.length === 2 && fw.reading.probe.frames === 2 &&
        row("query.answer").value === "no answer" && row("query.latencyMs").value === "-");
  check("silent unit: still only identity queries on the wire", onlyQueries(a.sent) && !anyWrite(a.sent));
}

// --- the nine conditions, and the verdict that follows from them -------------------------------
// The installer's own checklist, printed by the read-only path: each row says what this run could
// establish. A read-only check cannot pass the signature, the chip session, the backup or a recovery
// demonstration, so the verdict is NO-GO whatever the unit answered. That is the point: the panel has
// to show where the chain stops, not just a disabled button.
{
  const real = { version: "v1.9.0-1ef1b07", size: 611163,
    sha256: "237205de6df87963b07fe29c2a2b285fa8ee4972c7d89ddc77bd5d897f50e115", keys: {} };
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: real });
  const g = Object.fromEntries((fw.gate || []).map((r) => [r.key, r.state]));
  check("the real case: FM-1_015 answered, the package offered is FM-1_982",
        fw.kind === "stock" && fw.text === "FM-1_015" && fw.package.identity === "FM-1_982");
  check("the real case: the package replaces stock V15 (same model, other tag)",
        fw.compatibility === "replaces-stock" && fw.package.size === 611163 && fw.package.sha256.startsWith("237205de"));
  check("the checklist has its ten rows, in the installer's order",
        (fw.gate || []).map((r) => r.key).join(",") ===
        "gate.detect,gate.read,gate.package,gate.compat,gate.signature,gate.chip,gate.flashmap,gate.backup,gate.recovery,gate.revalidate");
  /* the map is a property of the package, so it can be established without a write -- but only when the
     build record names the image size. The post-install identity is `after`: the install verifies it,
     and requiring it in advance would be requiring the result to prove itself. */
  check("without an image size the flash map stays unknown, and the post-install identity is an obligation, not a precondition",
        g["gate.flashmap"] === "unknown" && g["gate.revalidate"] === "after" &&
        !fw.verdict.blocked.includes("gate.revalidate"));
  check("the four conditions this run can establish are pass",
        g["gate.detect"] === "pass" && g["gate.read"] === "pass" && g["gate.package"] === "pass" && g["gate.compat"] === "pass");
  check("an empty key map fails the signature row (it is not 'unknown' when no key exists at all)",
        g["gate.signature"] === "fail");
  check("the chip session is marked as not done by verify only, never as pass", g["gate.chip"] === "not-here");
  check("with no record and no dump, backup and recovery are unknown -- never pass, never 'documented'",
        g["gate.backup"] === "unknown" && g["gate.recovery"] === "unknown");
  check("the verdict is NO-GO, lists what blocks it, and cannot install on either path",
        fw.verdict.result === "NO-GO" && fw.verdict.canInstall === false && fw.verdict.canInstallHost === false &&
        fw.authorised === false &&
        ["gate.signature", "gate.chip", "gate.flashmap", "gate.backup", "gate.recovery"]
          .every((k) => fw.verdict.blocked.includes(k)) &&
        fw.verdict.blocked.length === 5);
  check("a key map does not let verify only claim the signature was checked",
        (await verifyDevice(access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] }).midi,
          { product: "FM-1_982", pkg: { ...real, keys: { prod: "00".repeat(32) } } }))
          .gate.find((r) => r.key === "gate.signature").state === "unknown");
  check("no report row claims a JEDEC id, a clock or a flash geometry was read",
        !(fw.fields || []).some((f) => /jedec|clock|geometry/i.test(f.key) && f.confidence !== "documented"));
}
{
  /* The owner's own evidence: two M-Flasher readings of one stock unit that agree on the whole 1 MiB, a
     signed package whose image fits `[0x4000, 0x93000)`, and a unit that answers its identity. This is
     the case the checklist used to call "blocked by gates 6..10": the chip, the map and the backup are
     all knowable, recovery is covered by a documented tool, and the only thing this page cannot do is
     write to a stock app -- a transport limit, not a missing gate. */
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const built = { version: "1.9.0", size: 610806, sha256: "80".repeat(32), keys: { "qg1-release-1": "00".repeat(32) },
    build: { commit: "7eb9636", artifacts: { "felucca.bin": { bytes: 531992 }, "loader/ota.bin": { bytes: 7650 } } } };
  const corroborated = { ok: true, readings: 2, dumpSha256: "38".repeat(32), jedec: 0x856014, flash: "0x856014",
    model: "FM-1", unitId: "4150353835313708", tool: "m-flasher 0.9.4",
    saved: ["20261109-102008", "20261109-102035"], source: "2 m-flasher records of this unit, same dump hash" };
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: built, corroborated });
  const g = Object.fromEntries((fw.gate || []).map((r) => [r.key, r.state]));
  check("two agreeing readings: the chip and the backup are established, as corroboration",
        g["gate.chip"] === "corroborated" && g["gate.backup"] === "corroborated");
  check("the map is established from the package, not from a write session",
        g["gate.flashmap"] === "pass");
  check("recovery is covered (verified backup + a tool over the written range), never 'exercised'",
        g["gate.recovery"] === "covered" && fw.verdict.limitations.includes("recovery-not-exercised"));
  check("the signature is still the blocker when no key is present, and nothing claims otherwise",
        g["gate.signature"] === "unknown" && fw.verdict.result === "NO-GO" &&
        fw.verdict.blocked.length === 1 && fw.verdict.blocked[0] === "gate.signature");
  check("the verdict names M-Flasher as the path, so a stock unit is never read as page-installable",
        fw.verdict.path === "m-flasher" && fw.verdict.canInstall === false && fw.verdict.canInstallHost === false);
  check("the read-only path still sent nothing but identity queries", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  /* the same unit and package with the signature checked: every precondition is a pass or a covered
     limitation, so the honest verdict for a stock unit is GO for the host install and still no write
     from this page. */
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const built = { version: "1.9.0", size: 610806, sha256: "80".repeat(32), keys: { "qg1-release-1": "00".repeat(32) },
    build: { commit: "7eb9636", artifacts: { "felucca.bin": { bytes: 531992 }, "loader/ota.bin": { bytes: 7650 } } } };
  const corroborated = { ok: true, readings: 2, dumpSha256: "38".repeat(32), jedec: 0x856014, flash: "0x856014",
    model: "FM-1", unitId: "4150353835313708", tool: "m-flasher 0.9.4",
    saved: ["20261109-102008", "20261109-102035"], source: "2 m-flasher records of this unit, same dump hash" };
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: built, corroborated,
    signature: { state: "pass", keyid: "qg1-release-1", size: 610806, sha256: "80".repeat(32), detail: "checked" } });
  check("a prepared stock unit is GO-HOST: the host writes, this page still does not",
        fw.verdict.result === "GO-HOST" && fw.verdict.canInstallHost === true && fw.verdict.canInstall === false &&
        fw.verdict.blocked.length === 0 && fw.verdict.path === "m-flasher");
  check("its limitations are named, not hidden", fw.verdict.limitations.includes("recovery-not-exercised") &&
        fw.verdict.limitations.includes("dump-not-hashed-here"));
  check("GO-HOST is not an authorisation to send anything", fw.authorised === false && onlyQueries(a.sent) && !anyWrite(a.sent));
  check("the diagnostic file says RECOVERY AVAILABLE honestly: covered, still not exercised",
        (() => { const st = Object.fromEntries(diag.diagnosticStates(fw).map((s) => [s.state, s.reached]));
          return st["INSTALL READY"] === true && st["INSTALL BLOCKED"] === false && st["RECOVERY NOT VERIFIED"] === false; })());
}
{
  /* The path that already exists on the wire: a unit running this firmware. Here the installer IS the
     writer, the chip is identified by the install's own 0x12 on the same link, and an interrupted
     update is resumable by design -- so nothing about a full-flash dump belongs in this list. This is
     the case that used to say "recovery not verified" and block an update that the transport supports. */
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_982")] });
  const built = { version: "1.9.0", size: 610806, sha256: "80".repeat(32), keys: { "qg1-release-1": "00".repeat(32) },
    build: { commit: "7eb9636", artifacts: { "felucca.bin": { bytes: 531992 }, "loader/ota.bin": { bytes: 7650 } } } };
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: built,
    signature: { state: "pass", keyid: "qg1-release-1", size: 610806, sha256: "80".repeat(32), detail: "checked" } });
  const g = Object.fromEntries((fw.gate || []).map((r) => [r.key, r.state]));
  check("a unit already running QG: the installer's own path is GO", fw.kind === "ours" &&
        fw.verdict.result === "GO" && fw.verdict.canInstall === true && fw.verdict.path === "this-page" &&
        fw.verdict.blocked.length === 0);
  check("the chip is not a gap here: the install reads it with 0x12 before its first write frame",
        g["gate.chip"] === "at-install");
  check("an update needs no full-flash dump, and its recovery is the loader's resume",
        g["gate.backup"] === "not-required" && g["gate.recovery"] === "covered" &&
        fw.verdict.limitations.includes("update-resumable-not-exercised"));
  check("verify only still sent nothing but the identity query", onlyQueries(a.sent) && !anyWrite(a.sent));
}
{
  /* a unit that answers another firmware: the model still matches, so compatibility is decided by what
     the unit said, and the checklist says the firmware is not one this installer may replace */
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_909")] });
  const fw = await verifyDevice(a.midi, { product: "FM-1_982" });
  const g = Object.fromEntries((fw.gate || []).map((r) => [r.key, r.state]));
  check("another JIANT build: compatibility fails in the checklist and the verdict is still NO-GO",
        fw.kind === "other" && fw.compatibility === "blocked-firmware" && g["gate.compat"] === "fail" && fw.verdict.result === "NO-GO");
}

{
  /* the eight named states and the exportable report: a state can never sit above its evidence, and
     the file a user attaches carries no key material and no session token */
  const a = access(["FM-1"], { "FM-1": () => [identityFrame("FM-1_015")] });
  const pkgHere = { version: "1.9.0", size: 611163, sha256: "ab".repeat(32), build: "abc1234",
    keys: { prod: "00".repeat(32) } };
  const fw = await verifyDevice(a.midi, { product: "FM-1_982", pkg: pkgHere });
  const states = diag.diagnosticStates(fw);
  const st = Object.fromEntries(states.map((s) => [s.state, s.reached]));
  check("the eight states are named and ordered", states.map((s) => s.state).join("|") === diag.DIAGNOSTIC_STATE_ORDER.join("|"));
  check("a stock unit reaches VERIFY READY and DEVICE COMPATIBLE", st["VERIFY READY"] && st["DEVICE COMPATIBLE"]);
  check("verify only never reaches SIGNATURE VERIFIED or INSTALL READY",
        !st["SIGNATURE VERIFIED"] && !st["INSTALL READY"] && st["INSTALL BLOCKED"]);
  check("recovery is reported as not verified, never as a pass", st["RECOVERY NOT VERIFIED"]);
  const j = diag.diagnosticJson(fw, { page: { build: "abc1234", version: "1.9.0" } });
  check("the export is a qg1-diagnostic-1 document with all eight states",
        j.schema === "qg1-diagnostic-1" && j.states.length === 8);
  check("the export repeats the report's own fields and verdict",
        j.fields.length === (fw.fields || []).length && j.verdict.result === fw.verdict.result);
  check("the export carries no key material and no session token",
        !/PRIVATE KEY|BEGIN |session|token|secret/i.test(JSON.stringify(j)));
}


{
  /* THE PACKAGE IS CHECKED WITHOUT A UNIT, AND THE PRODUCT IS NOT THE IDENTITY.
   *
   * Two defects met here and both are covered below against the real release bytes:
   *   - the signed manifest names the PRODUCT ("FM-1"), the page names the IDENTITY of the build
   *     ("FM-1_982"), and comparing them literally refused every published package -- so `releaseOk`
   *     stayed false and INSTALL was disabled on the live page for everyone, hardware or not;
   *   - the signature gate was reported as "cannot be established without a write session", which was
   *     false: these bytes, the manifest and the pinned key decide it, and all three are on the page.
   * A package check needs bytes, so it runs only when the release is in the tree; the model rule and
   * the state machine are checked in every case.
   */
  const fs = await import("node:fs");
  const rel = (f) => { try { return new Uint8Array(fs.readFileSync(new URL(`../release/${f}`, import.meta.url))); } catch (_) { return null; } };
  const man = (() => { try { return JSON.parse(fs.readFileSync(new URL("../release/felucca-v1.9.0.manifest.json", import.meta.url), "utf8")); } catch (_) { return null; } })();
  const keys = (() => { try { return JSON.parse(fs.readFileSync(new URL("../tools/fm1_keys.json", import.meta.url), "utf8")).keys || {}; } catch (_) { return {}; } })();
  const rec = (() => { try { return JSON.parse(fs.readFileSync(new URL("../release/build-manifest.json", import.meta.url), "utf8")); } catch (_) { return null; } })();
  const bytes = rel("felucca-v1.9.0.fwsc");

  if (bytes && man && rec) {
    const identity = String(rec.identity || "");
    const sha = String(rec.artifacts && rec.artifacts["felucca.fwsc"] ? rec.artifacts["felucca.fwsc"].sha256 : "");
    const size = Number(rec.artifacts && rec.artifacts["felucca.fwsc"] ? rec.artifacts["felucca.fwsc"].bytes : 0);

    /* the regression: a manifest for the product accepts the identity of that product's build */
    let accepted = true;
    try { await verifyManifest(bytes, man, keys, { product: identity }); } catch (_) { accepted = false; }
    check("a manifest naming the product (FM-1) is accepted for the build's identity (FM-1_982)", accepted);
    let crossed = false;
    try { await verifyManifest(bytes, man, keys, { product: "FM-2_982" }); } catch (_) { crossed = true; }
    check("the same manifest is refused for another model: a package never crosses models", crossed);

    /* the offline package check itself */
    const good = await mod.verifyPackage(bytes, { manifest: man, keys, product: identity, sha256: sha, size });
    check("the released package verifies offline: hash, identity and Ed25519 against the pinned key",
          good.state === "pass" && good.keyid === man.keyid && good.sha256 === sha && good.size === size);
    const wrongHash = await mod.verifyPackage(bytes, { manifest: man, keys, product: identity, sha256: "00".repeat(32), size });
    check("a package whose bytes are not the record's hash is refused", wrongHash.state === "fail" && /SHA-256|sha256/.test(wrongHash.detail));
    const tampered = bytes.slice(); tampered[4000] ^= 0xFF;
    const bad = await mod.verifyPackage(tampered, { manifest: man, keys, product: identity, sha256: sha, size });
    check("one altered byte is refused before any device is involved", bad.state === "fail");
    const otherModel = await mod.verifyPackage(bytes, { manifest: man, keys, product: "FM-2_982", sha256: sha, size });
    check("the released package is refused for another model", otherModel.state === "fail");
    const noKey = await mod.verifyPackage(bytes, { manifest: man, keys: {}, product: identity, sha256: sha, size });
    check("with no pinned key nothing is trusted, not even the real package", noKey.state === "fail");
    const forged = { ...man }; forged.size = man.size + 1;
    const forgedCheck = await mod.verifyPackage(bytes, { manifest: forged, keys, product: identity, sha256: sha, size });
    check("a manifest edited after signing is refused", forgedCheck.state === "fail");

    /* and it reaches the installer's own state machine without a unit attached */
    const none = access([]);
    const fw = await verifyDevice(none.midi, { product: identity, signature: good,
      pkg: { version: `v${man.version}-${String(rec.commit).slice(0, 7)}`, size, sha256: sha, build: rec, keys } });
    const st = Object.fromEntries(diag.diagnosticStates(fw).map((s) => [s.state, s.reached]));
    const g = Object.fromEntries((fw.gate || []).map((r) => [r.key, r.state]));
    check("with no unit, the package still reaches PACKAGE VERIFIED and SIGNATURE VERIFIED",
          st["PACKAGE VERIFIED"] && st["SIGNATURE VERIFIED"] && g["gate.signature"] === "pass");
    check("no unit still means no VERIFY READY, no DEVICE COMPATIBLE and NO-GO",
          !st["VERIFY READY"] && !st["DEVICE COMPATIBLE"] && fw.verdict.result === "NO-GO" && st["INSTALL BLOCKED"]);
    check("an empty WebMIDI reports zero inputs and zero outputs, and says so",
          fw.reading.probe.counts.inputs === 0 && fw.reading.probe.counts.outputs === 0 &&
          fw.reading.probe.reason === "no-ports" && fw.reading.probe.frames === 0);
    check("the report shows the signature as checked here, not as expected",
          (fw.fields || []).some((f) => f.key === "pkg.verified" && /verified/i.test(f.source)));

    /* a port that looks like an FM-1 but stays silent is a different diagnosis from no port at all */
    const silent = access(["FM-1"]);
    const fwS = await verifyDevice(silent.midi, { product: identity, signature: good, pkg: { version: "x", size, sha256: sha, build: rec, keys } });
    check("a silent FM-1 port is reported as silent, not as absent",
          fwS.kind === "silent" && fwS.reading.probe.reason === "silent" && fwS.reading.probe.frames > 0);
    check("a silent port still reaches SIGNATURE VERIFIED but never DEVICE COMPATIBLE",
          Object.fromEntries(diag.diagnosticStates(fwS).map((s) => [s.state, s.reached]))["SIGNATURE VERIFIED"] &&
          !fwS.compatible);

    /* unrelated gear is never probed, and the report says which case this was */
    const other = access(["Some Synth"]);
    const fwO = await verifyDevice(other.midi, { product: identity, signature: good, pkg: { version: "x", size, sha256: sha, build: rec, keys } });
    check("a unit whose descriptor says nothing is still probed, and reported as silent",
          fwO.kind === "silent" && fwO.reading.probe.reason === "silent" && fwO.reading.probe.frames > 0 &&
          onlyQueries(other.sent) && !anyWrite(other.sent));
  } else {
    console.log("skip: release/ is not in the tree, so the offline package cases cannot run against real bytes");
  }
}

/* ---- the chip gate, read only: two 0x12 queries on the link the unit already answers on ----
 * "cannot be established without a write session" was the wrong answer for a query that is itself a
 * read. On a unit running QG this returns the JEDEC and the geometry; on a stock V15 it returns the
 * silence, which is also a fact about that unit. Either way the wire holds nothing but queries. */
{
  /* the firmware's CRC-16/CCITT, written out here on purpose: if the module's copy changes and this
   * one does not, the replies below stop decoding and the test fails instead of quietly agreeing */
  const crc16 = (data) => {
    let c = 0;
    for (const b of data) {
      c ^= b << 8;
      for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
    }
    return c & 0xFFFF;
  };
  const chipReply = (jedec, session, opt = {}) => {
    const cap = opt.cap ?? 0x100000, page = opt.page ?? 256, sector = opt.sector ?? 0x1000, block = opt.block ?? 0x10000;
    const r = new Array(24).fill(0);
    r[0] = 1; r[1] = opt.status ?? 0;
    r[2] = (jedec >> 16) & 0xFF; r[3] = (jedec >> 8) & 0xFF; r[4] = jedec & 0xFF;
    r[5] = cap & 0xFF; r[6] = (cap >> 8) & 0xFF; r[7] = (cap >> 16) & 0xFF; r[8] = (cap >>> 24) & 0xFF;
    r[9] = page & 0xFF; r[10] = (page >> 8) & 0xFF;
    r[11] = sector & 0xFF; r[12] = (sector >> 8) & 0xFF;
    r[13] = block & 0xFF; r[14] = (block >> 8) & 0xFF; r[15] = (block >> 16) & 0xFF; r[16] = (block >>> 24) & 0xFF;
    r[17] = session & 0xFF; r[18] = (session >> 8) & 0xFF; r[19] = (session >> 16) & 0xFF; r[20] = (session >>> 24) & 0xFF;
    const c = crc16(r.slice(0, 21)); r[21] = c & 0xFF; r[22] = (c >> 8) & 0xFF;
    let s = 0; for (const x of r) s += x;
    return [0xF0, ...pack7([0, 0x59, 0x12, 24, 0, 0, ...r, ~s & 0xFF]), 0xF7];
  };
  /* a unit that answers the identity handshake, and 0x12 only when `chip` is given; `chip` may be a
   * function so the two identifications in one probe can carry different sessions */
  const unit = (name, chip) => {
    const sent = [];
    let n = 0;
    const input = { name, state: "connected", onmidimessage: null, open: async () => {} };
    const output = { name, state: "connected", open: async () => {}, send(bytes) {
      const b = Array.from(bytes);
      sent.push({ name, bytes: b });
      let cmd = null;
      try { cmd = unpack7(b.slice(1, -1))[2]; } catch (_) { cmd = null; }
      let f = null;
      if (isQuery(b)) f = identityFrame("FM-1_015");        // the handshake the installer probes with
      else if (cmd === 0x12 && chip) f = chipReply(typeof chip === "function" ? chip()[0] : chip, ++n);
      if (f) setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from(f) }), 0);
    } };
    return { sent, midi: { inputs: new Map([[input.name, input]]), outputs: new Map([[output.name, output]]) } };
  };
  const cmdOf = (b) => { try { return unpack7(b.slice(1, -1))[2]; } catch (_) { return null; } };
  const onlyHexQueries = (sent) => sent.every(({ bytes: b }) =>
    b[0] === 0xF0 && b[b.length - 1] === 0xF7 && [0x11, 0x12, 0x13].includes(cmdOf(b)));

  /* a stock V15: the identity handshake is answered, 0x12 is not -- the case the owner's unit is in */
  const stock = unit("FM-1 Midi", null);
  const noChip = await chipCheck(stock.midi, { timeoutMs: 300 });
  check("a stock unit that ignores 0x12: reported, not invented",
        noChip.ok === false && noChip.code === "chip" && /0x12/.test(noChip.why));
  check("the chip check sent the identity query and one 0x12, and nothing that writes",
        stock.sent.filter(({ bytes: b }) => cmdOf(b) === 0x12).length === 1 &&
        stock.sent.filter(({ bytes: b }) => isQuery(b)).length === 1 &&
        !anyWrite(stock.sent) && !stock.sent.some(({ bytes: b }) => isUpgrade(b)) && onlyHexQueries(stock.sent));

  /* a unit running QG on the documented part: the real JEDEC, the geometry, a fresh token each time */
  const qg = unit("FM-1 Midi", () => [0x856014]);
  const chipOk = await chipCheck(qg.midi, { timeoutMs: 300 });
  check("a unit answering 0x12: the JEDEC and geometry are the chip's own answer",
        chipOk.ok === true && chipOk.jedec === 0x856014 && chipOk.part === "P25Q80H" &&
        chipOk.cap === 0x100000 && chipOk.sector === 0x1000 && chipOk.readings === 2 && chipOk.writable === true);
  check("the chip check never opens the update path, even when the chip answers",
        !qg.sent.some(({ bytes: b }) => isUpgrade(b)) && !anyWrite(qg.sent) && onlyHexQueries(qg.sent));

  /* one of the five read-only parts: identified, and refused for writing by the same answer */
  const ro = unit("FM-1 Midi", () => [0xC84014]);
  const chipRo = await chipCheck(ro.midi, { timeoutMs: 300 });
  check("a read-only part is identified and reported as not writable",
        chipRo.ok === true && chipRo.jedec === 0xC84014 && chipRo.part === "GD25Q80" && chipRo.writable === false);
  let refused = null;
  try { requireChip({ jedec: chipRo.jedec, writable: chipRo.writable }); } catch (e) { refused = e; }
  check("that same answer is refused by requireChip before any write frame", !!refused && refused.code === "chip");

  /* a part whose JEDEC is not in the table: refused rather than reported as this unit's chip */
  const unknown = unit("FM-1 Midi", () => [0x123456]);
  const chipUnknown = await chipCheck(unknown.midi, { timeoutMs: 300 });
  check("a JEDEC outside the documented table is refused, not guessed",
        chipUnknown.ok === false && chipUnknown.code === "chip" && /not a part we know/.test(chipUnknown.why));

  /* no unit at all: the check says so instead of reporting a chip */
  const none = await chipCheck({ inputs: new Map(), outputs: new Map() }, { timeoutMs: 300 });
  check("no FM-1 port: no chip is claimed", none.ok === false && none.code === "notfound");

  /* and the gate reads that answer: a refusal is a failed gate, a pass is still not an install */
  const refusedGate = await verifyDevice(stock.midi, { product: "FM-1_982", chip: noChip });
  const chipRow = refusedGate.gate.find((r) => r.key === "gate.chip");
  check("the report's chip gate is a fail when the unit refused 0x12",
        chipRow && chipRow.state === "fail" && refusedGate.verdict.canInstall === false);
  const passedGate = await verifyDevice(qg.midi, { product: "FM-1_982", chip: chipOk });
  const chipRow2 = passedGate.gate.find((r) => r.key === "gate.chip");
  check("the chip gate passes on a real answer, and the install is still not authorised here",
        chipRow2 && chipRow2.state === "pass" && passedGate.verdict.canInstall === false);
}

console.log(`verify only: ${total} checks, ${bad} FAILED`);
if (bad) process.exit(1);
