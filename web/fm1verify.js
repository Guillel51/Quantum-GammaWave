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

export async function verifyDevice(midi, opts = {}) {
  const product = String(opts.product || "");
  const log = opts.log || (() => {});
  const ports = [...midi.inputs.values()]
    .map((p) => String(p.name || ""))
    .filter((n) => PORT_RE.test(n));
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
    return report(kind, { text: "", model: "", version: 0, ports });
  }
  const { text, model, version } = dev.id;
  dev.link.close();
  const base = { text, model, version, ports };
  // the identity frame carries "<MODEL>_<version>" and nothing else. FM-1_909 is a real FM-1 whose
  // firmware is not one this installer knows: its own numbering (JIANT's, or a custom build's),
  // not a byte pattern to match against. The device is recognised, the firmware is not.
  if (/^ota-/i.test(model)) return report("loader", base);
  if (text === "FM-1_015") return report("stock", base);
  if (product && text === product) return report("ours", base);
  return report("other", base);
}
