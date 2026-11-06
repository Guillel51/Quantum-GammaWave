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

import { Updater, PORT_RE } from "./fm1ota.js";

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
  const recognised = kind === "stock" || kind === "ours" || kind === "loader";
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
  if (kind === "loader") return "finish-loader";
  return "blocked-firmware";
}

// the package block, rendered in every report: the same rows whatever the device did or did not say.
// Every value here comes from the page's own build record -- it is the package, never the unit.
function pkgFields(pkg, compat) {
  const nkeys = Object.keys(pkg.keys || {}).length;
  const rows = [
    { key: "pkg.identity", value: pkg.identity || "-", source: "this page's build record", confidence: pkg.identity ? "high" : "none" },
    { key: "pkg.release", value: pkg.release || "-", source: "this page's build record", confidence: pkg.release ? "high" : "none" },
    { key: "pkg.size", value: pkg.size ? `${pkg.size} B` : "-", source: "this page's build record", confidence: pkg.size ? "high" : "none" },
    { key: "pkg.sha256", value: pkg.sha256 ? `${pkg.sha256.slice(0, 16)}\u2026` : "-", source: "this page's build record (expected, not re-hashed here)", confidence: pkg.sha256 ? "medium" : "none" },
  ];
  if (pkg.commit) rows.push({ key: "pkg.build", value: `${pkg.version || "?"} ${pkg.commit.slice(0, 7)}`,
    source: "the build record inside the package", confidence: "high" });
  if (pkg.binary) rows.push({ key: "pkg.binary", value: `${pkg.binary} B${pkg.binarySha ? ` ${pkg.binarySha.slice(0, 12)}\u2026` : ""}`,
    source: "the build record's artifact list", confidence: "high" });
  if (pkg.loader) rows.push({ key: "pkg.loader", value: `${pkg.loader} B`, source: "the build record's artifact list", confidence: "high" });
  rows.push({ key: "compat", value: compat, source: "device identity vs package tag", confidence: "high" });
  rows.push({ key: "install.auth", value: "none", source: "verify only never authorises an install", confidence: "high" });
  /* why nothing can be authorised yet, said in the one place the reader is looking for a verdict */
  rows.push({ key: "install.why", value: nkeys ? "signed" : "no-key", source: nkeys
    ? `${nkeys} key(s) in this build's key map`
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

const MISSING = [
  ["flash.jedec", "the identity query does not carry it; 0x12 is part of the install path, not of a read-only check"],
  ["flash.size", "geometry is not in the identity answer and the running firmware refuses region reads"],
  ["soc", "the SoC is not reported by the protocol: it is the documented platform (AC791N/WL82), not a reading"],
  ["clock", "no register or query exposes it without a firmware-specific protocol"],
  ["bootloader", "which loader last ran is only visible through the ota-<model> port, when it is up"],
  ["usb.vidpid", "WebMIDI does not expose USB vendor/product ids to the page"],
];

export async function verifyDevice(midi, opts = {}) {
  const product = String(opts.product || "");
  const pkg = packageOf(product, opts.pkg);
  const log = opts.log || (() => {});
  const inputs = [...midi.inputs.values()].map(portInfo);
  const outputs = [...midi.outputs.values()].map(portInfo);
  const ports = inputs.filter((p) => PORT_RE.test(p.name)).map((p) => p.name);
  const up = new Updater(midi, log);            // find() sends the identity query only
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
    const kind = ports.length ? "silent" : "none";
    const port = inputs.find((p) => PORT_RE.test(p.name)) || { name: "", manufacturer: "", version: "" };
    const compat = compatibility(kind, ports.length ? "FM-1" : "", pkg.model);
    return report(kind, {
      text: "", model: "", version: 0, ports,
      hardware: ports.length ? "FM-1" : "", hardwareSource: ports.length ? "port name" : "",
      firmware: "unknown", reading: { sysex: !!midi.sysexEnabled, inputs, outputs },
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
        ...platformFields(),
        ...pkgFields(pkg, compat),
      ],
      missing: MISSING.map(([key, why]) => ({ key, why })),
    });
  }
  const { text, model, version } = dev.id;
  dev.link.close();
  const hardware = hardwareOf(model);
  let kind, firmware;
  if (/^ota-/i.test(model)) { kind = "loader"; firmware = "loader"; }
  else if (text === "FM-1_015") { kind = "stock"; firmware = "stock"; }
  else if (product && text === product) { kind = "ours"; firmware = "gammaWave"; }
  else { kind = "other"; firmware = "other"; }
  const port = inputs.find((p) => PORT_RE.test(p.name)) || { name: "", manufacturer: "", version: "", id: "" };
  const compat = compatibility(kind, hardware, pkg.model);
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
    ...platformFields(),
    ...pkgFields(pkg, compat),
  ];
  const missing = MISSING.filter(([key]) => key !== "bootloader" || kind !== "loader").map(([key, why]) => ({ key, why }));
  return report(kind, {
    text, model, version, ports, hardware, hardwareSource: "identity answer", firmware,
    reading: { sysex: !!midi.sysexEnabled, inputs, outputs },
    fields, missing,
    package: pkg, compatibility: compat, authorised: false,
    compatible: compat === "replaces-stock" || compat === "reinstall-same" || compat === "finish-loader",
  });
}
