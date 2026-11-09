// SPDX-License-Identifier: GPL-3.0-only
// What the installer is allowed to claim about the unit's state.
//
// The seventeen states came from the owner's brief; this file is where each one is either tied to an
// observation this page can make, or declared unreachable with the reason. The tests below are about
// that boundary: a state is never reached by a guess, the three blind spots of WebMIDI stay blind
// spots, and "writing" is only true while the loader is inside its write pass -- because that flag is
// what the page uses to tell a user whether unplugging costs anything.
import assert from "node:assert";
import { STATES, stateOf, reachable, unobservable, derive, advice, describe } from "./fm1state.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

/* 1. The list is the brief's list, in one place, with no duplicates and no silent additions. */
{
  const ids = STATES.map((s) => s.id);
  ok(ids.length === 17, `seventeen states, no more (${ids.length})`);
  ok(new Set(ids).size === 17, "no state appears twice");
  for (const want of ["DEVICE_NOT_CONNECTED", "NORMAL_FIRMWARE_DETECTED", "IDENTITY_READ",
    "ENTERING_DOWNLOAD_MODE", "ROM_DEVICE_DETECTED", "OTA_LOADER_DETECTED", "CHIP_IDENTIFIED",
    "PREFLIGHT_COMPLETE", "BACKUP_COMPLETE", "READY_TO_INSTALL", "ERASE_IN_PROGRESS",
    "WRITE_IN_PROGRESS", "VERIFYING", "REBOOTING", "INSTALL_COMPLETE", "RECOVERY_REQUIRED",
    "FAILED_SAFE"])
    ok(ids.includes(want), `state ${want} is named exactly as the brief asks`);
}

/* 2. The three states this transport cannot observe are declared, with a reason, and are not in the
 *    reachable list. A page that printed "ROM_DEVICE_DETECTED" would be describing a device WebMIDI
 *    cannot return: the ROM mode enumerates as 4C4A:8057 mass storage, outside the MIDI world. */
{
  const blind = unobservable().map((u) => u.id);
  ok(blind.length === 4 && ["ENTERING_DOWNLOAD_MODE", "ROM_DEVICE_DETECTED", "ERASE_IN_PROGRESS",
    "VERIFYING"].every((id) => blind.includes(id)), `four blind spots (${blind.join(", ")})`);
  ok(unobservable().every((u) => typeof u.why === "string" && u.why.length > 40),
    "each blind spot carries its reason, so the page can say why it cannot see it");
  ok(!reachable().includes("ROM_DEVICE_DETECTED") && !reachable().includes("ENTERING_DOWNLOAD_MODE"),
    "the ROM states are not reachable from a WebMIDI page");
  ok(stateOf("WRITE_IN_PROGRESS").writes === true && stateOf("READY_TO_INSTALL").writes === false,
    "a state knows whether flash is being written");
  let threw = false;
  try { stateOf("INSTALLING"); } catch (_) { threw = true; }
  ok(threw, "an invented state id is refused, not silently accepted");
}

/* 3. Derivation from observations: every path ends in a state this file defines, and the absence of
 *    an observation is DEVICE_NOT_CONNECTED, never an optimistic guess. */
{
  ok(derive({}) === "DEVICE_NOT_CONNECTED", "nothing observed: not connected");
  ok(derive({ fw: { kind: "none" } }) === "DEVICE_NOT_CONNECTED", "no port: not connected");
  ok(derive({ fw: { kind: "silent" } }) === "IDENTITY_READ",
    "a port that answered nothing: identity read, not a firmware");
  ok(derive({ fw: { kind: "stock" } }) === "NORMAL_FIRMWARE_DETECTED", "stock V15: normal firmware");
  ok(derive({ fw: { kind: "jiant" } }) === "NORMAL_FIRMWARE_DETECTED", "another firmware: normal firmware");
  ok(derive({ fw: { kind: "stock" }, chip: { jedec: 0x856014 } }) === "CHIP_IDENTIFIED",
    "a chip reading on the same link: chip identified");
  ok(derive({ fw: { kind: "jiant" }, chip: { jedec: 0x856014 }, backup: true }) === "BACKUP_COMPLETE",
    "an archive captured: backup complete");
  ok(derive({ fw: { kind: "ourloader" } }) === "OTA_LOADER_DETECTED", "our loader: loader detected");
  ok(derive({ fw: { kind: "otherloader" } }) === "OTA_LOADER_DETECTED",
    "the vendor's loader: also a loader state, and the report says whose");
}

/* 4. The loader's step names, mapped to the truth of each: the transfer to the unit is not a write,
 *    the write is not a verification pass, and only the write pass makes RECOVERY_REQUIRED the right
 *    answer to a lost link. Getting this pair backwards is how a user powers off mid-write. */
{
  ok(derive({ fw: { kind: "jiant" }, step: "start" }) === "PREFLIGHT_COMPLETE", "start: preflight complete");
  ok(derive({ fw: { kind: "jiant" }, step: "verify" }) === "READY_TO_INSTALL",
    "the image transfer (0xE0000000): ready to install, flash untouched");
  ok(derive({ fw: { kind: "jiant" }, step: "loader" }) === "OTA_LOADER_DETECTED",
    "switching to the loader: loader detected");
  ok(derive({ fw: { kind: "jiant" }, step: "write" }) === "WRITE_IN_PROGRESS",
    "the write pass (0xF0000000): write in progress");
  ok(derive({ fw: { kind: "jiant" }, step: "reboot" }) === "REBOOTING", "reboot: rebooting");
  ok(derive({ fw: { kind: "jiant" }, step: "done" }) === "INSTALL_COMPLETE", "done: install complete");
  const lost = Object.assign(new Error("cable"), { code: "lost" });
  ok(derive({ fw: { kind: "jiant" }, error: lost, phase: "write" }) === "RECOVERY_REQUIRED",
    "link lost inside the write pass: recovery required, not 'failed safely'");
  ok(derive({ fw: { kind: "stock" }, error: lost }) === "FAILED_SAFE",
    "link lost before any write: failed safe, nothing changed");
  ok(derive({ error: Object.assign(new Error("x"), { code: "foreignloader" }) }) === "FAILED_SAFE",
    "a refused foreign loader ends safe, with nothing sent");
}

/* 5. Every state has advice a user can act on, and the advice in the destructive states says not to
 *    cut the power. This is the text the page shows, so it is tested, not written and forgotten. */
{
  for (const { id } of STATES) {
    const a = advice(id);
    ok(typeof a === "string" && a.length > 20, `advice exists for ${id}`);
  }
  for (const id of ["WRITE_IN_PROGRESS", "VERIFYING", "REBOOTING"])
    ok(/(do not cut the power|power must not be cut)/i.test(advice(id)),
      `${id}: the advice says not to cut the power`);
  ok(/replug/i.test(advice("RECOVERY_REQUIRED")), "recovery required: replug, do not power-cycle");
  ok(/nothing/i.test(advice("FAILED_SAFE")) && /nothing/i.test(advice("NORMAL_FIRMWARE_DETECTED")),
    "the safe states say that nothing was written");
  ok(describe("WRITE_IN_PROGRESS").startsWith("WRITE_IN_PROGRESS (writing)")
    && describe("READY_TO_INSTALL").startsWith("READY_TO_INSTALL --"),
    "the one-line form marks writing, and only in the writing states");
}

console.log(`state: ${n} checks, 0 FAILED`);
