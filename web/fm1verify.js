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
    return report(kind, {
      text: "", model: "", version: 0, ports,
      hardware: ports.length ? "FM-1" : "", hardwareSource: ports.length ? "port name" : "",
      firmware: "unknown", reading: { sysex: !!midi.sysexEnabled, inputs, outputs },
      fields: [], missing: MISSING.map(([key, why]) => ({ key, why })),
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
  ];
  const missing = MISSING.filter(([key]) => key !== "bootloader" || kind !== "loader").map(([key, why]) => ({ key, why }));
  return report(kind, {
    text, model, version, ports, hardware, hardwareSource: "identity answer", firmware,
    reading: { sysex: !!midi.sysexEnabled, inputs, outputs },
    fields, missing,
    compatible: !!product && text === product,      /* informational: never an authorisation */
  });
}
