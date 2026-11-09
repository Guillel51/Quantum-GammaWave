// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// WHAT STATE IS THE UNIT IN, AND WHAT MAY BE SAID ABOUT IT.
//
// The installer used to describe the unit with whatever string arrived last ("found a device",
// "loader", "done"). That is not a state: a port name is not a loader, a loader is not a chip, and
// "done" from a loader that never committed is not an install. This module is the one place that
// turns observations into a named state, so the page can print the state, decide whether anything
// destructive is happening, and say what the user may safely do next.
//
// Three of the seventeen states are NOT reachable from this transport, and they are declared as
// such rather than being faked when the page guesses:
//
//   ENTERING_DOWNLOAD_MODE  the vendor's ROM download mode is entered by a USB-MIDI frame
//   ROM_DEVICE_DETECTED     (F0 22 24 35 7D F7) sent to the *running stock app*, and the unit then
//                           re-enumerates as 4C4A:8057 -- a mass-storage/SCSI device, not MIDI.
//                           WebMIDI exposes neither that device nor its VID:PID, so from this page
//                           the mode is invisible: no port appears, no frame answers, and the unit
//                           leaves the browser's MIDI world entirely. Reaching it needs M-Flasher
//                           or another host program (docs/FM1_MODES.md).
//
//   ERASE_IN_PROGRESS       the loader erases as it writes (0xF0000000 serve) and reports a byte
//                           count, not an erase phase. The page can say "writing, do not
//                           interrupt"; it cannot honestly claim to be watching an erase.
//
// Everything else maps to something the page actually observed. `writes` is true only while the
// loader is inside its write pass, which is what the disconnect message keys off.

// The order is the order of the install path, not of the numbers: a state list is read top to
// bottom by whoever is debugging a stuck unit.
export const STATES = [
  { id: "DEVICE_NOT_CONNECTED", writes: false, reachable: true },
  { id: "NORMAL_FIRMWARE_DETECTED", writes: false, reachable: true },
  { id: "IDENTITY_READ", writes: false, reachable: true },
  { id: "ENTERING_DOWNLOAD_MODE", writes: false, reachable: false,
    why: "the ROM download mode is entered over USB-MIDI by the running stock app and the unit then "
      + "re-enumerates as 4C4A:8057 (mass storage, SCSI). WebMIDI exposes neither it nor VID:PID." },
  { id: "ROM_DEVICE_DETECTED", writes: false, reachable: false,
    why: "same transport gap: a ROM-mode unit is not a MIDI device, so this page cannot see it." },
  { id: "OTA_LOADER_DETECTED", writes: false, reachable: true },
  { id: "CHIP_IDENTIFIED", writes: false, reachable: true },
  { id: "PREFLIGHT_COMPLETE", writes: false, reachable: true },
  { id: "BACKUP_COMPLETE", writes: false, reachable: true },
  { id: "READY_TO_INSTALL", writes: false, reachable: true },
  { id: "ERASE_IN_PROGRESS", writes: true, reachable: false,
    why: "the loader erases inside its write pass and reports bytes written, not an erase phase." },
  { id: "WRITE_IN_PROGRESS", writes: true, reachable: true },
  { id: "VERIFYING", writes: true, reachable: false,
    why: "the loader checks every sector inside its write pass; there is no separate verification "
      + "phase this page can observe, and claiming one would be a progress bar invented by the host." },
  { id: "REBOOTING", writes: true, reachable: true },
  { id: "INSTALL_COMPLETE", writes: false, reachable: true },
  { id: "RECOVERY_REQUIRED", writes: false, reachable: true },
  { id: "FAILED_SAFE", writes: false, reachable: true },
];

const BY_ID = new Map(STATES.map((s) => [s.id, s]));

// the two questions a state has to answer at once: is anything being written, and is the user
// allowed to unplug the unit.
export function stateOf(id) {
  const s = BY_ID.get(id);
  if (!s) throw new Error(`unknown state ${id}`);
  return { ...s };
}

export function reachable() {
  return STATES.filter((s) => s.reachable).map((s) => s.id);
}

// states this transport cannot observe, with the reason -- printed next to the live state so the
// page never implies it watched something it could not see.
export function unobservable() {
  return STATES.filter((s) => !s.reachable).map((s) => ({ id: s.id, why: s.why }));
}

// The observation is what the caller actually saw. Nothing here reads the clock, the DOM or MIDI:
// a state is derived from evidence, and an absent observation is not an observation.
//   fw      { kind } from fwKind()/fm1verify: none|silent|stock|jiant|other
//           or the loader kinds: ourloader|otherloader
//   chip    a chip reading ({ part, jedec }) or null
//   backup  true when an archive was captured this session
//   step    the last loader step: start|verify|loader|write|reboot|done
//   error   the error object the flow ended with, if it ended with one
//   phase   "write" once the loader's write pass began (the page tracks it)
export function derive({ fw, chip, backup, step, error, phase } = {}) {
  if (error) {
    const lost = error.code === "lost" || error.code === "stopped";
    /* Losing the link means two different things and the difference is the whole message: before
     * the first byte nothing changed on the unit, inside the write pass a sector is half done and
     * the loader did not commit. The former can be retried; the latter needs the unit replugged and
     * the same package installed again to finish, without touching the power. */
    if (phase === "write") return "RECOVERY_REQUIRED";
    if (lost || (error.code && /^(notfound|model|foreignloader|loaderchip|stockfirst|proof)$/.test(error.code)))
      return "FAILED_SAFE";
    return "FAILED_SAFE";
  }
  if (step === "done") return "INSTALL_COMPLETE";
  if (step === "reboot") return "REBOOTING";
  /* The loader's own step names, read from firmware/src/ota.c's host side (web/fm1ota.js):
   *   start   admission passed, handshake done
   *   verify  the image is being transferred to the unit (0xE0000000); flash is untouched
   *   loader  the unit is switching into its update loader
   *   write   the loader is writing flash (0xF0000000) and checks every sector as it goes
   * So the transfer is not a write and the write is not a verification pass: mapping them the
   * other way round would tell the user the wrong thing at the one moment it matters. */
  if (step === "write") return "WRITE_IN_PROGRESS";
  if (step === "verify") return "READY_TO_INSTALL";
  if (step === "loader") return "OTA_LOADER_DETECTED";
  if (step === "start") return "PREFLIGHT_COMPLETE";
  if (phase === "write") return "WRITE_IN_PROGRESS";
  const kind = fw && fw.kind;
  if (!kind || kind === "none") return "DEVICE_NOT_CONNECTED";
  if (kind === "ourloader" || kind === "otherloader") return "OTA_LOADER_DETECTED";
  if (kind === "silent" || kind === "other") return "IDENTITY_READ";
  if (kind === "stock" || kind === "jiant") {
    /* the chip fact is a reading on the same link (0x12), so it is a state only when it happened */
    if (backup) return "BACKUP_COMPLETE";
    if (chip && chip.jedec) return "CHIP_IDENTIFIED";
    return "NORMAL_FIRMWARE_DETECTED";
  }
  return "DEVICE_NOT_CONNECTED";
}

// What the user may do in this state, in the words of the thing that is true right now. Kept here,
// next to the states, so a state cannot be added without somebody deciding what it means for the
// cable.
export function advice(id) {
  switch (id) {
    case "DEVICE_NOT_CONNECTED":
      return "plug the unit in over USB; nothing is being written.";
    case "NORMAL_FIRMWARE_DETECTED":
      return "the unit is running a firmware this page does not write over; nothing is being "
        + "written and it is safe to unplug.";
    case "IDENTITY_READ":
      return "a port answered nothing usable; nothing is being written and it is safe to unplug.";
    /* the four states this page cannot see: the advice is not a step of this installer, it is where
     * the operation actually lives, so nobody waits in front of a page that cannot move. */
    case "ENTERING_DOWNLOAD_MODE":
    case "ROM_DEVICE_DETECTED":
      return "the ROM download mode is not visible to a WebMIDI page (the unit re-enumerates as "
        + "4C4A:8057 mass storage). M-Flasher or a host tool enters and uses it; nothing is being "
        + "written by this page.";
    case "ERASE_IN_PROGRESS":
    case "VERIFYING":
      return "inside the loader's write pass, which erases and checks each sector as it goes: it is "
        + "reported as WRITE_IN_PROGRESS, and the power must not be cut.";
    case "OTA_LOADER_DETECTED":
      return "the unit is in an update loader; nothing is being written unless a write is already "
        + "running. Replug the cable and press Install again to resume.";
    case "CHIP_IDENTIFIED":
    case "PREFLIGHT_COMPLETE":
      return "the checks passed but no write has begun: safe to unplug, safe to retry.";
    case "BACKUP_COMPLETE":
      return "the archive is saved; nothing on the unit was written.";
    case "READY_TO_INSTALL":
      return "the package, the unit and the chip agree; the next step writes flash.";
    case "WRITE_IN_PROGRESS":
    case "VERIFYING":
    case "REBOOTING":
      return "do not cut the power and do not unplug: the loader is writing or has just finished.";
    case "INSTALL_COMPLETE":
      return "the unit came back on the new firmware and said so.";
    case "RECOVERY_REQUIRED":
      return "the link dropped inside the write pass. Do not power-cycle: replug the cable, let the "
        + "unit re-enter its loader, and install the same package again to finish.";
    case "FAILED_SAFE":
      return "the flow stopped before any destructive step; nothing was written.";
    default:
      return "";
  }
}

// One line for the log and the report: the state, whether flash is being written, and the advice.
export function describe(id) {
  const s = stateOf(id);
  return `${s.id}${s.writes ? " (writing)" : ""} -- ${advice(s.id)}`;
}
