// SPDX-License-Identifier: GPL-3.0-only
// The report a person can attach to a message, and the eight states the installer speaks in.
// Pure functions over a verify report: this module never speaks to a device, holds no key and
// cannot authorise anything, which is why it lives outside fm1verify.js (whose export surface is
// deliberately just verifyDevice and the read-only query).

/* The eight states the installer speaks in, weakest first. Each one is derived from the gate rows
 * above and can never exceed them. `INSTALL READY` follows the verdict, which knows which path it is
 * talking about: a verified unit whose dump exists is ready for the host install (M-Flasher) even when
 * this page must not write to it, and the page's own installer is ready only on a unit that already
 * runs this firmware. `RECOVERY NOT VERIFIED` is a state, not a note: it holds until a verified dump of
 * this unit exists and a documented tool covers the range an install overwrites -- and even then it
 * means "covered", never "exercised". */
export const DIAGNOSTIC_STATE_ORDER = Object.freeze([
  "DIAGNOSTIC READY", "VERIFY READY", "PACKAGE VERIFIED", "SIGNATURE VERIFIED",
  "DEVICE COMPATIBLE", "RECOVERY NOT VERIFIED", "INSTALL READY", "INSTALL BLOCKED"]);

export function diagnosticStates(report) {
  const rows = (report && report.gate) || [];
  const state = (key) => (rows.find((r) => r.key === key) || {}).state;
  const v = (report && report.verdict) || {};
  const installReady = !!(report && (v.canInstall || v.canInstallHost));
  const recoveryCovered = ["pass", "covered"].includes(state("gate.recovery"));
  const why = {
    "DIAGNOSTIC READY": "the page ran a read-only exchange and reported what it saw",
    "VERIFY READY": "an FM-1 answered the identity query",
    "PACKAGE VERIFIED": "the package this page offers describes itself consistently",
    "SIGNATURE VERIFIED": "the package was checked against its published SHA-256 in this page",
    "DEVICE COMPATIBLE": "the unit's own answer is compatible with this package",
    "RECOVERY NOT VERIFIED": "no verified dump of this unit plus a tool covering the written range, or none demonstrated",
    "INSTALL READY": v.path === "m-flasher"
      ? "this unit is ready for the host install (M-Flasher writes the app area and keeps the bootloader); this page still does not write to a stock unit"
      : "every precondition is met and this page's installer would accept this unit",
    "INSTALL BLOCKED": "at least one precondition is not met",
  };
  const reached = {
    "DIAGNOSTIC READY": !!report,
    "VERIFY READY": !!report && report.kind !== "none",
    "PACKAGE VERIFIED": state("gate.package") === "pass",
    "SIGNATURE VERIFIED": state("gate.signature") === "pass",
    "DEVICE COMPATIBLE": state("gate.compat") === "pass",
    "RECOVERY NOT VERIFIED": !recoveryCovered,
    "INSTALL READY": installReady,
    "INSTALL BLOCKED": !installReady,
  };
  return DIAGNOSTIC_STATE_ORDER.map((name) => ({ state: name, reached: !!reached[name], why: why[name] }));
}

/* A report the user can attach to a message instead of retyping six screens. It holds only what the
 * read-only exchange produced: no private key (this page never has one), no session token, no port
 * serial beyond what WebMIDI already exposes to the page, and no package bytes. */
export function diagnosticJson(report, extra = {}) {
  const r = report || {};
  return {
    schema: "qg1-diagnostic-1",
    generated: new Date().toISOString(),
    page: extra.page || null,
    device: {
      kind: r.kind || "none", reported: r.text || "", model: r.model || "",
      firmware: r.firmware || "unknown", version: r.version || 0,
      hardware: r.hardware || "", hardwareSource: r.hardwareSource || "",
      ports: r.ports || [], compatibility: r.compatibility || "", authorised: !!r.authorised,
    },
    chip: r.chip || null,          // a read-only 0x12 check, when one was run
    fields: r.fields || [],
    missing: r.missing || [],
    gate: r.gate || [],
    states: diagnosticStates(r),
    verdict: r.verdict || { result: "NO-GO", blocked: [], canInstall: false },
    note: "read only: this report is not an install authorisation, and it carries no key material",
  };
}
