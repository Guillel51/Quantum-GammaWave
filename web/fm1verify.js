// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// VERIFY ONLY: what the installer's "Verify only" button is allowed to do to a connected
// FM-1. It reads the running firmware's identity and nothing else.
//
// The whole flow is one frame type: the identity query HS_QUERY (F0 00 32 45 00 00 00 40 7F
// F7), sent by Updater.find() to each port whose name looks like an FM-1, and repeated only
// while the answer does not arrive. No UPGRADE (F0 22 24 35 7F F7), no 0x30 read answers, no
// E0000000/F0000000 acknowledgements, no 0x12/0x13 chip session, no archive PUT, no settings
// write: none of those functions is called from here, and none of them can be reached from
// this module (it exports this function and nothing else).
//
// The report separates the two questions the one identity frame can answer, because confusing
// them is how an installer decides the wrong thing:
//
//   hardware   "FM-1" comes from the model part of "<MODEL>_<version>", whatever firmware runs.
//              FM-1_909 is an FM-1: Jiant's own numbering does not change the hardware.
//   firmware   what answered: stock V15 (FM-1_015), this package's tag ("ours"), the update
//              loader (ota-FM-1_<v>), or another build -- Jiant or a custom image -- whose
//              version is reported exactly as it answered and never interpreted.
//
// Everything else in the report is either a USB-MIDI descriptor string the browser had before
// any frame was sent, or a field this path cannot read. The flash's JEDEC id, its geometry, the
// SoC, the clock and the boot loader are NOT in the answer to the identity query, and the one
// command that would read the chip (0x12) is part of the install path's admission, not of a
// read-only check: it is not sent here. Those fields are listed as missing, with the reason, so
// the report says what it does not know instead of filling the gap with a guess.
//
// Classification never guesses. The identity frame carries "<MODEL>_<version>" and nothing
// more, so that string is the whole truth the device gives away without a protocol of its own:
//   stock   FM-1_015            the official V15 the installer can replace
//   ours    the package's tag   this firmware (the page passes it in `product`)
//   loader  ota-FM-1_<v>        the update loader: an install was cut off
//   other   any other FM-1_<n>  another firmware (JIANT or a custom build): its identity is
//                               reported exactly as it answered, never as "unknown"
//   silent  no answer           the port is there and did not answer the query: a firmware
//                               that does not implement it (JIANT may be one). Nothing else
//                               is sent: an unknown firmware never gets a frame it may not know.
//   none    no port             nothing named like an FM-1 is connected
//
// A disconnect, a timeout or a malformed answer ends the same way: the kind is reported from
// what was actually read, every port is closed, and there is no next step that writes.
//
// THE PACKAGE IS NOT THE DEVICE. The page also carries a package (this build: FM-1_982), and the
// two identities must never be mixed on screen: "FM-1_982" is the tag of the .fwsc this page
// offers, "FM-1_015" is what a stock FM-1 answers, and a unit that answers FM-1_982 is NOT a unit
// running official V15 -- it is running this build. So the report carries them in two separate
// blocks (`firmware.*` from the frame, `pkg.*` from this page's build record), and the
// compatibility between them is a third, derived row that is explicitly informational.
// The identity of the package is NEVER written into a device field, and the package being V15 does
// not turn a device that answered FM-1_982 into stock: kind is decided by what the unit said.

import { Updater, PORT_RE, verifyManifest, isOurLoader } from "./fm1ota.js";
import { derive, describe, unobservable } from "./fm1state.js";
import { productOf } from "./fm1pkg.js";

// the one frame Verify Only may put on the wire (tests/test_verify_only.mjs checks the log)
export const READ_ONLY_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];

// what the browser knows about a port before any frame is sent: the USB-MIDI descriptor strings.
// No device command is involved, so these survive any firmware, including one that answers nothing.
function portInfo(p) {
  return {
    name: String(p.name || ""),
    manufacturer: String(p.manufacturer || ""),
    version: String(p.version || ""),
    id: String(p.id || ""),
    state: String(p.state || ""),
    type: String(p.type || ""),
  };
}

function hardwareOf(model) {
  const base = String(model || "").replace(/^ota-/i, "");
  return base === "FM-1" ? "FM-1" : "";          /* the model part, never the firmware number */
}

// every answer carries the same shape, and none of them authorises an install: verify only reads.
//   found        an FM-1 answered (or, for "silent", a port that looks like one is connected)
//   recognised   this installer knows the firmware that answered
//   canInstall   false, always: only the install path, with the manifest, the chip and the sizes,
//                decides that, and it decides it on its own
function report(kind, base) {
  const found = kind !== "none";
  const recognised = kind === "stock" || kind === "ours" || kind === "ourloader" || kind === "otherloader";
  return { kind, found, recognised, canInstall: false, ports: base.ports, ...base };
}

// the package this page offers, described from its own build record (never from the device):
// identity tag, the release version the site names, and the size and hash the manifest covers.
// The hash here is the EXPECTED one -- verify only does not re-read the .fwsc -- so it says so.
function packageOf(product, pkg) {
  const identity = String(product || "");
  const m = /^([^_]+)(?:_(\d+))?$/.exec(identity);
  const p = pkg || {};
  /* the build record the package carries (make_site.py embeds it): the binary and the loader it
   * was packed from, and the commit it was built at. It describes the PACKAGE, not the unit. */
  const b = p.build || {};
  const art = b.artifacts || {};
  const bin = art["felucca.bin"] || {};
  const loader = art["loader/ota.bin"] || {};
  return {
    identity,
    model: m ? m[1] : "",
    tag: m && m[2] ? m[2] : "",
    release: String(p.version || ""),
    size: Number(p.size || 0),
    sha256: String(p.sha256 || ""),
    commit: String(b.commit || ""),
    version: String(b.version || ""),
    binary: Number(bin.bytes || 0),
    binarySha: String(bin.sha256 || ""),
    loader: Number(loader.bytes || 0),
    keys: (pkg && pkg.keys) || {},
  };
}

/* named apart from the page's own sha256hex(): make_site.py inlines this module into the page's
 * single module scope, so two functions with one name are a SyntaxError, not a shadow. */
async function digestHex(bytes) {
  const d = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(d)).map((b) => b.toString(16).padStart(2, "0")).join("");
}

// The package against the signed manifest and the pinned key -- offline, and therefore true even when
// no unit is attached. It answers one question and does not throw: `state` is "pass" or "fail", and
// `detail` says why, so the report can print the refusal instead of a stack trace. It never returns
// "pass" for bytes that are not the ones the build record names: the hash is recomputed here, the
// package's own identity is read out of the container, and only then is the Ed25519 signature checked.
export async function verifyPackage(bytes, { manifest = null, keys = {}, product = "", sha256 = "", size = 0 } = {}) {
  const out = { state: "fail", keyid: "", version: "", sha256: "", size: 0, detail: "" };
  try {
    if (!bytes || typeof bytes.length !== "number" || !bytes.length) throw new Error("there are no package bytes to check");
    const buf = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    out.size = buf.length;
    out.sha256 = await digestHex(buf);
    if (size && buf.length !== size) throw new Error(`the package is ${buf.length} B, the build record says ${size} B`);
    if (sha256 && out.sha256 !== sha256) throw new Error("the package's SHA-256 is not the build record's");
    if (product && productOf(buf) !== product) throw new Error(`the package identifies as ${productOf(buf)}, not ${product}`);
    const v = await verifyManifest(buf, manifest, keys, { product });
    out.state = "pass"; out.keyid = v.keyid; out.version = v.version;
    out.detail = `key ${v.keyid}, ${buf.length} B, sha256 ${out.sha256.slice(0, 16)}\u2026`;
  } catch (e) {
    out.detail = String((e && e.message) || e);
  }
  return out;
}

// How this package relates to what answered. Informational: `canInstall` is false regardless, and
// only the install path's own gate (signature, chip JEDEC, sizes) decides. A silent unit is
// "unproven", never "compatible": nothing was read, so nothing is known about its firmware.
function compatibility(kind, deviceModel, pkgModel) {
  if (!pkgModel) return "no-package";
  if (kind === "none") return "no-device";
  if (kind === "silent") return "unproven-device";
  if (!deviceModel || deviceModel !== pkgModel) return "blocked-model";
  if (kind === "stock") return "replaces-stock";
  if (kind === "ours") return "reinstall-same";
  if (kind === "ourloader") return "finish-loader";
  /* Both loaders answer "ota-FM-1", and the name alone says nothing: the unit's own update mode
   * (the vendor's, the one a stock FM-1 enters) and this project's loader (ota-FM-1_900). Only the
   * version separates them, so a loader that is not ours is reported as foreign -- readable, never
   * a target. */
  if (kind === "otherloader") return "foreign-loader";
  return "blocked-firmware";
}

// the package block, rendered in every report: the same rows whatever the device did or did not say.
// Every value here comes from the page's own build record -- it is the package, never the unit.
function pkgFields(pkg, compat, sig = null) {
  const nkeys = Object.keys(pkg.keys || {}).length;
  const rows = [
    { key: "pkg.identity", value: pkg.identity || "-", source: "this page's build record", confidence: pkg.identity ? "high" : "none" },
    { key: "pkg.release", value: pkg.release || "-", source: "this page's build record", confidence: pkg.release ? "high" : "none" },
    { key: "pkg.size", value: pkg.size ? `${pkg.size} B` : "-", source: "this page's build record", confidence: pkg.size ? "high" : "none" },
    { key: "pkg.sha256", value: pkg.sha256 ? `${pkg.sha256.slice(0, 16)}\u2026` : "-", source: sig && sig.state === "pass"
      ? "the build record, and the bytes re-hashed here: they match"
      : "this page's build record (expected, not re-hashed here)", confidence: sig && sig.state === "pass" ? "high" : (pkg.sha256 ? "medium" : "none") },
  ];
  /* the offline half of the trust chain: the bytes were hashed here, the manifest was checked against
   * a pinned key, and the package's own identity was read out of it. Every word of it was measured in
   * this run -- without a unit, which is why it can be true on a page where nothing is plugged in. */
  if (sig) rows.push({ key: "pkg.verified", value: sig.state === "pass" ? `${sig.keyid || "?"} \u00b7 ${sig.size} B` : "failed",
    source: sig.state === "pass" ? `Ed25519 verified here over the package's own bytes: ${String(sig.detail || "").slice(0, 96)}`
      : `not verified here: ${String(sig.detail || "no reason given").slice(0, 120)}`,
    confidence: "high" });
  if (pkg.commit) rows.push({ key: "pkg.build", value: `${pkg.version || "?"} ${pkg.commit.slice(0, 7)}`,
    source: "the build record inside the package", confidence: "high" });
  if (pkg.binary) rows.push({ key: "pkg.binary", value: `${pkg.binary} B${pkg.binarySha ? ` ${pkg.binarySha.slice(0, 12)}\u2026` : ""}`,
    source: "the build record's artifact list", confidence: "high" });
  if (pkg.loader) rows.push({ key: "pkg.loader", value: `${pkg.loader} B`, source: "the build record's artifact list", confidence: "high" });
  rows.push({ key: "compat", value: compat, source: "device identity vs package tag", confidence: "high" });
  rows.push({ key: "install.auth", value: "none", source: "verify only never authorises an install", confidence: "high" });
  /* why nothing can be authorised yet, said in the one place the reader is looking for a verdict */
  rows.push({ key: "install.why", value: sig && sig.state === "pass" ? "verified" : (nkeys ? "signed" : "no-key"),
    source: sig && sig.state === "pass" ? "the package's signature was checked in this run"
      : nkeys ? `${nkeys} key(s) in this build's key map; this run did not check the package against them`
      : "this build's key map is empty: a signature can be neither made nor checked", confidence: "high" });
  return rows;
}

// The platform this firmware is built for. Documented, not measured: none of it is read from the unit
// by any command this page may send, and saying "documented" is the difference between a report and a
// guess. They are printed with the rest so nobody has to guess them either.
const PLATFORM = [
  ["static.soc", "AC791N / WL82 (JieLi)", "this repository's hardware notes"],
  ["static.cpu", "dual-core pi32v2, 32-bit LE, single-precision FPU", "this repository's hardware notes"],
  ["static.ram", "578 KB SRAM; this firmware's .data+.bss budget is 96 KiB", "the linker script and the budget tests"],
  ["static.flash", "1 MiB SPI NOR, XIP at 0x02000000; JEDEC 0x856014 / 0xB36014", "the flash identification table"],
];
function platformFields() {
  return PLATFORM.map(([key, value, source]) => ({ key, value, source: `${source} (documented, not read from the unit)`, confidence: "documented" }));
}

// What the read-only exchange measured about itself. These rows exist because they are true and
// knowable: how many frames went out, on which port, and how long the answer took. They say nothing
// about the unit's flash, clock or boot loader, which is why those stay in `missing`.
const PROBE_WHY = {
  "no-ports": "this browser sees no MIDI device at all: no input and no output",
  "no-match": "MIDI ports exist, but none carries a name this installer recognises",
  "no-pair": "a port looks like an FM-1, but it has no output of the same name",
  "unpaired": "MIDI ports exist and were probed, but none answered the identity query",
  "send-failed": "the port refused the identity frame",
  "silent": "every probed port stayed silent",
  "answered": "the port that answered",
};

/* What a read-only 0x12 identification measured, or why it refused. Read from the unit, not documented:
 * the JEDEC and geometry are the chip's own answer, and the refusal is the unit's own silence. */
function chipFields(chip) {
  if (!chip) return [];
  if (!chip.ok) return [{ key: "chip.refused", value: chip.code || "refused",
    source: String(chip.why || "").slice(0, 160), confidence: "high" }];
  return [
    { key: "chip.jedec", value: "0x" + Number(chip.jedec).toString(16).toUpperCase().padStart(6, "0") + " " + (chip.part || ""),
      source: "two 0x12 identifications of this unit (read only)", confidence: "high" },
    { key: "chip.geometry", value: `${Number(chip.cap || 0)} B · page ${Number(chip.page || 0)} · sector ${Number(chip.sector || 0)} · block ${Number(chip.block || 0)}`,
      source: "the identification answer, cross-checked between the two replies", confidence: "high" },
    { key: "chip.write", value: chip.writable ? "a validated write part" : "not a part this installer writes",
      source: "the flash identification table and the two identical answers", confidence: "high" },
  ];
}

/* The chip fact and the backup, both from the record M-Flasher wrote and from the dump this page
 * hashed against it. "verified here" is literal: the bytes were hashed in the browser. */
function recordFields(record) {
  if (!record) return [];
  if (!record.ok) return [{ key: "record.refused", value: record.code || "refused",
    source: String(record.why || "").slice(0, 160), confidence: "high" }];
  return [
    { key: "record.chip", value: `${record.model} · flash 0x${Number(record.jedec).toString(16).toUpperCase().padStart(6, "0")} ${record.part || ""}`,
      source: "M-Flasher's reading of this unit's NOR, carried in its record", confidence: "high" },
    { key: "record.backup", value: `${record.dumpBytes} B · sha256 ${String(record.dumpSha256).slice(0, 16)}…`,
      source: "the dump's SHA-256, computed in this page from the bytes loaded here", confidence: "high" },
    { key: "record.source", value: record.tool || "M-Flasher",
      source: "the tool and version the record names", confidence: "high" },
  ];
}

/* Two independent readings of this unit, agreeing on the whole 1 MiB. Reported as corroboration and
 * named that way: the page did not hash the dumps, so it says what it checked (two records, same
 * hash, different timestamps) instead of borrowing the authority of a hash it computed. */
function corroboratedFields(c) {
  if (!c) return [];
  return [
    { key: "record.readings", value: `${c.readings} readings \u00b7 ${c.flash} ${c.part || ""}`, source: c.source || "M-Flasher records", confidence: "high" },
    { key: "record.agreement", value: `sha256 ${String(c.dumpSha256).slice(0, 16)}\u2026 · saved ${c.saved.join(", ")}`,
      source: "the SHA-256 M-Flasher computed for each read, compared here: they are the same image", confidence: "high" },
  ];
}

function probeFields(probe, port) {
  const p = probe || {};
  const c = p.counts || {};
  const seen = p.seen || {};
  const rows = [
    { key: "query.frames", value: String(p.frames || 0), source: "identity frames this page sent (read-only)", confidence: "high" },
    { key: "query.answer", value: p.answered ? p.answered : "no answer",
      source: p.answered ? PROBE_WHY.answered : (PROBE_WHY[p.reason] || PROBE_WHY.silent),
      confidence: p.answered ? "high" : "none" },
    /* what the browser itself saw: the counts are the difference between a cable that carries no data
     * and a unit that is there but does not answer, and a report that omitted them sent the reader to
     * "another app is using MIDI" when the page had not seen a single port. */
    { key: "ports.inputs", value: String(c.inputs === undefined ? "-" : c.inputs), source: "WebMIDI, this browser (read only)", confidence: "high" },
    { key: "ports.outputs", value: String(c.outputs === undefined ? "-" : c.outputs), source: "WebMIDI, this browser (read only)", confidence: "high" },
    { key: "ports.seen", value: [...(seen.inputs || []), ...(seen.outputs || [])].length
      ? [...new Set([...(seen.inputs || []), ...(seen.outputs || [])])].join(", ") : "-",
      source: "USB-MIDI descriptors this browser enumerated", confidence: (seen.inputs || seen.outputs) ? "high" : "none" },
    { key: "query.latencyMs", value: p.latencyMs === null || p.latencyMs === undefined ? "-" : `${p.latencyMs} ms`, source: "measured from the first frame to the answer", confidence: p.latencyMs === null || p.latencyMs === undefined ? "none" : "high" },
    { key: "port.state", value: (port && port.state) || "-", source: "USB-MIDI descriptor", confidence: port && port.state ? "high" : "none" },
    { key: "port.type", value: (port && port.type) || "-", source: "USB-MIDI descriptor", confidence: port && port.type ? "high" : "none" },
    { key: "usb.secureContext", value: (typeof isSecureContext === "boolean" ? (isSecureContext ? "yes" : "no") : "unknown"), source: "this browsing context (WebMIDI needs it)", confidence: "high" },
  ];
  if (p.probed && p.probed.length > 1) rows.push({ key: "query.ports", value: p.probed.join(", "), source: "every port this page probed", confidence: "high" });
  return rows;
}

const MISSING = [
  ["flash.jedec", "the identity query does not carry it; 0x12 is part of the install path, not of a read-only check"],
  ["flash.size", "geometry is not in the identity answer and the running firmware refuses region reads"],
  ["soc", "the SoC is not reported by the protocol: it is the documented platform (AC791N/WL82), not a reading"],
  ["clock", "no register or query exposes it without a firmware-specific protocol"],
  ["bootloader", "which loader last ran is only visible through the ota-<model> port, when it is up"],
  ["usb.vidpid", "WebMIDI does not expose USB vendor/product ids to the page"],
  ["usb.serial", "WebMIDI exposes a port id, not the descriptor's serial number or a chip-unique id"],
  ["device.fuse", "no documented read-only command reports the OTP/fuse state of this SoC"],
];

function partsOf(pkg) {
  const n = Object.keys((pkg && pkg.keys) || {}).length;
  return { keys: n };
}

// The regions, from the loader that writes them (firmware/loader/ldr_core.c: LDR_APP_LO/LDR_APP_HI).
// A package whose image does not fit here is refused by the loader itself, so it is checked before.
const APP_LO = 0x4000, APP_HI = 0x93000;

/* The ten conditions, in the order the installer walks them, each with the state the evidence actually
 * supports. Two lessons are baked into the list below, because both cost this project a false block:
 *
 *   - a condition that is only knowable AFTER the write (the identity the unit reports afterwards) is
 *     not a precondition. It is listed as `after`: the install verifies it, and requiring it in advance
 *     is requiring the result to prove itself;
 *   - "no recovery has been demonstrated on this unit" is true of every unit that has never been
 *     broken. The precondition that can be met is that a verified dump of this unit exists and that a
 *     documented tool covers exactly the range the install overwrites. That is `covered`: weaker than
 *     `pass`, stronger than "unknown", and it is named so nobody reads it as a demonstration.
 *
 * states:
 *   pass          established here, by this run or by this unit's own record
 *   corroborated  two independent readings of this unit agree (fm1record.js corroborate())
 *   covered       a verified backup plus a documented tool covering the written range; never exercised
 *   fail          contradicted by what was read
 *   unknown       this path cannot establish it yet
 *   not-here      verify only does not do it (the chip session belongs to the install path)
 *   not-required  the update path does not need it (a unit already running this firmware)
 *   after         a post-write obligation, verified by the install, never a precondition
 */
function gateOf({ found, read, pkg, compat, sig = null, chip = null, record = null, corroborated = null, kind = "none" }) {
  const { keys } = partsOf(pkg);
  const compatible = compat === "replaces-stock" || compat === "reinstall-same" || compat === "finish-loader";
  const image = Number((pkg && pkg.binary) || 0);
  const fits = image > 0 && APP_LO + image <= APP_HI;
  const backup = record && record.ok ? "pass" : (corroborated && corroborated.ok ? "corroborated" : "unknown");
  const rows = [
    { key: "gate.detect", state: found ? "pass" : "fail" },
    { key: "gate.read", state: read ? "pass" : "unknown" },
    { key: "gate.package", state: pkg && pkg.identity ? "pass" : "fail" },
    { key: "gate.compat", state: found && read ? (compatible ? "pass" : "fail") : "unknown" },
    /* The signature is a property of the package, not of the unit: these bytes, the signed manifest
     * and the pinned key decide it, and all three are already on this page. It used to be "unknown"
     * until a write session existed, which was both false and the reason the installer could never
     * say it trusted what it was about to write. */
    { key: "gate.signature", state: sig ? (sig.state === "pass" ? "pass" : "fail") : (keys ? "unknown" : "fail") },
    /* A stock unit cannot answer 0x12, so its chip fact comes from the record out of band -- either
     * the record whose dump the page hashed, or two records of this unit that agree. Both are readings. */
    { key: "gate.chip", state: chip ? (chip.ok ? "pass" : "fail")
      : (record && record.ok ? "pass" : (corroborated && corroborated.ok ? "corroborated" : "not-here")) },
    /* The map is a property of the package and of the loader's own limits, not of a write session:
     * the image has to fit `[0x4000, 0x93000)` and the boot head is never in any write range. */
    { key: "gate.flashmap", state: image ? (fits ? "pass" : "fail") : "unknown" },
    { key: "gate.backup", state: kind === "ours" && !record ? "not-required" : backup },
    { key: "gate.recovery", state: (backup === "pass" || backup === "corroborated") && fits ? "covered" : "unknown" },
    { key: "gate.revalidate", state: "after" },
  ];
  /* What each path needs. This page writes to a unit that already runs this firmware (and to its own
   * loader); it never writes to a stock app, because that app has no UPGRADE path at all. The host path
   * writes the same app area through the chip's ROM mode and keeps the bootloader -- a different
   * transport, so a different list. */
  const page = kind === "ours" || kind === "ourloader";
  const okPage = { pass: 1, "not-required": 1, covered: 1 };
  const okHost = { pass: 1, corroborated: 1, covered: 1 };
  const ok = page ? okPage : okHost;
  const required = rows.filter((r) => r.state !== "after" && !(page && r.key === "gate.read" && kind === "ourloader"));
  const blocking = required.filter((r) => !ok[r.state]).map((r) => r.key);
  const limitations = [];
  if (rows.find((r) => r.key === "gate.recovery").state === "covered")
    limitations.push("recovery-not-exercised");
  if (rows.find((r) => r.key === "gate.backup").state === "corroborated")
    limitations.push("dump-not-hashed-here");
  const ready = blocking.length === 0 && found && read && pkg && pkg.identity;
  const verdict = ready
    ? { result: page ? "GO" : "GO-HOST", path: page ? "this-page" : "m-flasher", blocked: [],
        canInstall: page, canInstallHost: !page, limitations }
    : { result: "NO-GO", path: page ? "this-page" : "m-flasher", blocked: blocking,
        canInstall: false, canInstallHost: false, limitations };
  return { rows, verdict };
}

export async function verifyDevice(midi, opts = {}) {
  const product = String(opts.product || "");
  const pkg = packageOf(product, opts.pkg);
  const sig = opts.signature || null;       // the offline check of the package (verifyPackage)
  const chip = opts.chip || null;           // a read-only 0x12 check, when one was run
  const record = opts.record || null;       // an M-Flasher record whose dump was hashed here
  const corroborated = opts.corroborated || null;  // two records of this unit that agree (no dump here)
  const log = opts.log || (() => {});
  const inputs = [...midi.inputs.values()].map(portInfo);
  const outputs = [...midi.outputs.values()].map(portInfo);
  const ports = inputs.filter((p) => PORT_RE.test(p.name)).map((p) => p.name);
  // probeAll: Verify Only asks "is my unit visible at all?", so it probes every in/out pair it can
  // see, not only the ones whose Windows descriptor says FM-1. find() sends the identity query and
  // nothing else either way.
  const up = new Updater(midi, log, { probeAll: true });
  let dev = null;
  try {
    dev = await up.find();
  } catch (e) {
    log(`verify: ${e && e.message}`);
    dev = null;
  }
  if (!dev) {
    // no answer: either nothing is plugged in, or something named like an FM-1 is and it does not
    // implement the query. Both are "no firmware recognised", never "no device" when a port is there
    const probe = up.lastProbe;
    /* a port that was probed and did not answer is "silent", even when its name is not one this
     * installer recognises: that is what was done and what came back */
    const kind = (ports.length || (probe.probed || []).length) ? "silent" : "none";
    const state = derive({ fw: { kind } });
    const port = inputs.find((p) => PORT_RE.test(p.name)) || { name: "", manufacturer: "", version: "" };
    const compat = compatibility(kind, ports.length ? "FM-1" : "", pkg.model);
    const gate = gateOf({ found: ports.length > 0, read: false, pkg, compat, sig, chip, record, corroborated, kind });
    return report(kind, {
      text: "", model: "", version: 0, ports,
      hardware: ports.length ? "FM-1" : "", hardwareSource: ports.length ? "port name" : "",
      firmware: "unknown", reading: { sysex: !!midi.sysexEnabled, inputs, outputs, probe },
      package: pkg, compatibility: compat, authorised: false,
      /* a silent unit still tells us what its USB descriptors say: that is information, and it is
       * the only information this path gets from it */
      fields: [
        { key: "hardware.model", value: ports.length ? "FM-1" : "unknown", source: ports.length ? "port name" : "none", confidence: ports.length ? "medium" : "none" },
        { key: "protocol", value: "MIDI SysEx identity query (read only)", source: "this module", confidence: "high" },
        { key: "usb.sysex", value: midi.sysexEnabled ? "granted" : "not granted", source: "WebMIDI", confidence: "high" },
        { key: "port.name", value: port.name || "-", source: "USB-MIDI descriptor", confidence: port.name ? "high" : "none" },
        { key: "port.manufacturer", value: port.manufacturer || "-", source: "USB-MIDI descriptor", confidence: port.manufacturer ? "high" : "none" },
        { key: "port.descriptorVersion", value: port.version || "-", source: "USB-MIDI descriptor", confidence: port.version ? "high" : "none" },
        ...probeFields(probe, port),
        ...platformFields(),
        ...pkgFields(pkg, compat, sig),
        ...chipFields(chip),
        ...recordFields(record),
        ...corroboratedFields(corroborated),
        { key: "state.current", value: describe(state), source: "derived here: no usable identity answer", confidence: "high" },
      ],
      state,
      signature: sig, chip, record, corroborated,
      gate: gate.rows, verdict: gate.verdict,
      missing: MISSING.map(([key, why]) => ({ key, why })),
    });
  }
  const { text, model, version } = dev.id;
  dev.link.close();
  const hardware = hardwareOf(model);
  let kind, firmware;
  if (/^ota-/i.test(model)) {
    /* /^ota-/ is the shape of the update loader, not a claim about whose it is: isOurLoader() reads
     * the version (900 is this project's, FM-1 015's vendor loader is 015). Splitting them here is
     * what stops the report from calling a foreign loader "a interrupted install to finish". */
    kind = isOurLoader(dev.id) ? "ourloader" : "otherloader";
    firmware = isOurLoader(dev.id) ? "loader" : "vendorLoader";
  }
  else if (text === "FM-1_015") { kind = "stock"; firmware = "stock"; }
  else if (product && text === product) { kind = "ours"; firmware = "gammaWave"; }
  else { kind = "other"; firmware = "other"; }
  const probe = up.lastProbe;
  const port = inputs.find((p) => PORT_RE.test(p.name)) || { name: "", manufacturer: "", version: "", id: "" };
  const compat = compatibility(kind, hardware, pkg.model);
  const gate = gateOf({ found: true, read: true, pkg, compat, sig, chip, record, corroborated, kind });
  const fields = [
    { key: "hardware.model", value: hardware || "unknown", source: "identity answer", confidence: hardware ? "high" : "none" },
    { key: "firmware.identity", value: text, source: "identity answer", confidence: "high" },
    { key: "firmware.version", value: String(version), source: "identity answer", confidence: "high" },
    { key: "firmware.family", value: firmware, source: "identity answer", confidence: "high" },
    { key: "protocol", value: "MIDI SysEx identity query (read only)", source: "this module", confidence: "high" },
    { key: "usb.sysex", value: midi.sysexEnabled ? "granted" : "not granted", source: "WebMIDI", confidence: "high" },
    { key: "port.name", value: port.name || "-", source: "USB-MIDI descriptor", confidence: port.name ? "high" : "none" },
    { key: "port.manufacturer", value: port.manufacturer || "-", source: "USB-MIDI descriptor", confidence: port.manufacturer ? "high" : "none" },
    { key: "port.descriptorVersion", value: port.version || "-", source: "USB-MIDI descriptor", confidence: port.version ? "high" : "none" },
    ...probeFields(probe, port),
    ...platformFields(),
    ...pkgFields(pkg, compat, sig),
    ...chipFields(chip),
    ...recordFields(record),
    ...corroboratedFields(corroborated),
  ];
  /* the boot loader of a unit sitting in its update loader is not readable through that loader, so
   * the field is dropped there -- for both loaders, ours and the vendor's, since neither answers. */
  const missing = MISSING.filter(([key]) => key !== "bootloader" || !/loader$/.test(kind)).map(([key, why]) => ({ key, why }));
  /* The named state, derived from what was read, printed with the transport's blind spots next to
   * it: a reader must be able to tell "observed" from "this page cannot observe that at all". */
  const state = derive({ fw: { kind }, chip });
  fields.push({ key: "state.current", value: describe(state), source: "derived here from the identity answer and, when it exists, the chip reading", confidence: "high" });
  fields.push({ key: "state.unobservable", value: `${unobservable().length} of ${17}: `
    + `${unobservable().map((u) => u.id).join(", ")}`, source: "WebMIDI exposes no VID:PID and no mass-storage device; the loader erases inside its write pass (docs/FM1_MODES.md)", confidence: "high" });
  return report(kind, {
    state,
    text, model, version, ports, hardware, hardwareSource: "identity answer", firmware,
    reading: { sysex: !!midi.sysexEnabled, inputs, outputs, probe },
    fields, missing, signature: sig, chip, record, corroborated, gate: gate.rows, verdict: gate.verdict,
    package: pkg, compatibility: compat, authorised: false,
    compatible: compat === "replaces-stock" || compat === "reinstall-same" || compat === "finish-loader",
  });
}
