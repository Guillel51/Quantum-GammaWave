// SPDX-License-Identifier: GPL-3.0-only
// The report a person can attach to a message, and the eight states the installer speaks in.
// Pure functions over a verify report: this module never speaks to a device, holds no key and
// cannot authorise anything, which is why it lives outside fm1verify.js (whose export surface is
// deliberately just verifyDevice and the read-only query).

/* The eight states the installer speaks in, weakest first. Each one is derived from the gate rows
 * above and can never exceed them: `INSTALL READY` needs every row to be a pass, so a lower state
 * cannot be shown as a higher one. `RECOVERY NOT VERIFIED` is a state, not a note: no recovery has
 * been demonstrated on any unit from this page, and until one is, that is the honest name for it. */
export const DIAGNOSTIC_STATE_ORDER = Object.freeze([
  "DIAGNOSTIC READY", "VERIFY READY", "PACKAGE VERIFIED", "SIGNATURE VERIFIED",
  "DEVICE COMPATIBLE", "RECOVERY NOT VERIFIED", "INSTALL READY", "INSTALL BLOCKED"]);

export function diagnosticStates(report) {
  const rows = (report && report.gate) || [];
  const state = (key) => (rows.find((r) => r.key === key) || {}).state;
  const allPass = rows.length > 0 && rows.every((r) => r.state === "pass");
  const installReady = !!report && allPass && !!(report.verdict && report.verdict.canInstall);
  const why = {
    "DIAGNOSTIC READY": "the page ran a read-only exchange and reported what it saw",
    "VERIFY READY": "an FM-1 answered the identity query",
    "PACKAGE VERIFIED": "the package this page offers describes itself consistently",
    "SIGNATURE VERIFIED": "a signature was checked against a pinned key in this page",
    "DEVICE COMPATIBLE": "the unit's own answer is compatible with this package",
    "RECOVERY NOT VERIFIED": "no recovery was demonstrated on this unit",
    "INSTALL READY": "every gate row is a pass and the installer would accept this unit",
    "INSTALL BLOCKED": "at least one gate row is not a pass",
  };
  const reached = {
    "DIAGNOSTIC READY": !!report,
    "VERIFY READY": !!report && report.kind !== "none",
    "PACKAGE VERIFIED": state("gate.package") === "pass",
    "SIGNATURE VERIFIED": state("gate.signature") === "pass",
    "DEVICE COMPATIBLE": state("gate.compat") === "pass",
    "RECOVERY NOT VERIFIED": state("gate.recovery") !== "pass",
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
    fields: r.fields || [],
    missing: r.missing || [],
    gate: r.gate || [],
    states: diagnosticStates(r),
    verdict: r.verdict || { result: "NO-GO", blocked: [], canInstall: false },
    note: "read only: this report is not an install authorisation, and it carries no key material",
  };
}
