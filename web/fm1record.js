// SPDX-License-Identifier: GPL-3.0-only
// The chip identification read out of band, and the dump that goes with it.
//
// A stock unit answers the identity handshake but not 0x12 (docs/JEDEC_READ.md §1), so the running
// firmware cannot say which flash it has. M-Flasher can: it reaches the chip's ROM download mode and
// reads the part's own identification, then reads the whole 1 MiB. That is a reading of the unit, taken
// by a program instead of by this page -- and it is the only way a first install gets a chip fact
// before any write frame.
//
// This module does not trust the record: it checks every field, refuses a part this installer does not
// write, and -- when the dump is given -- hashes the bytes here, in the page, against the hash the
// record names. A record without its dump is a claim; a record whose dump hashes to it is a reading
// this page has repeated.

export const DUMP_BYTES = 0x100000;

// Only the parts this project writes. A recognised chip is not a writable one: the five read-only
// parts (GD25Q80, W25Q80, ZB25VQ80, BY25D80, XM25QH80B) are named so the refusal can say which it was.
export const WRITABLE_PARTS = { 0x856014: "P25Q80H", 0xB36014: "UC25HQ80" };
export const READ_ONLY_PARTS = {
  0xC84014: "GD25Q80", 0xEF4014: "W25Q80", 0x5E4014: "ZB25VQ80", 0x684014: "BY25D80", 0x204014: "XM25QH80B",
};

const recFail = (why) => Object.assign(new Error(why), { code: "record" });
/* named apart from the page's own sha256hex(): make_site.py inlines this module into the page's single
 * module scope, so two functions with one name are a SyntaxError on load, not a shadow. */
const recordHex = async (bytes) => {
  const d = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(d), (b) => b.toString(16).padStart(2, "0")).join("");
};

// "FM-1 015 (stock)" -> { model: "FM-1", version: 15, stock: true }
export function firmwareOf(text) {
  const m = /^\s*([A-Za-z0-9-]+)\s+(\d{3})\s*(\(stock\))?\s*$/i.exec(String(text || ""));
  if (!m) return null;
  return { model: m[1], version: Number(m[2]), stock: !!m[3] };
}

// The record M-Flasher writes next to a backup. Every field it needs is required: a missing one is a
// record that cannot be tied to a unit, and an untied record must not authorise anything.
export function parseRecord(text) {
  let rec;
  try { rec = typeof text === "string" ? JSON.parse(text) : text; }
  catch (_) { throw recFail("The record is not JSON: drop the file M-Flasher wrote, unchanged."); }
  if (!rec || typeof rec !== "object" || Array.isArray(rec)) throw recFail("The record is not an object.");
  const fw = firmwareOf(rec.firmware);
  const flash = /^0x([0-9a-fA-F]{6})$/.exec(String(rec.flash || ""));
  if (!flash) throw recFail("The record does not carry a flash id (expected a field like \"flash\": \"0x856014\").");
  const jedec = Number.parseInt(flash[1], 16);
  const sha = String(rec.sha256 || "").toLowerCase();
  if (!/^[0-9a-f]{64}$/.test(sha))
    throw recFail("The record has no SHA-256 of the dump: without it nothing can be checked against the bytes.");
  if (String(rec.model || "") !== "FM-1") throw recFail(`The record is for ${JSON.stringify(rec.model)}, not FM-1.`);
  if (!fw || fw.model !== rec.model) throw recFail(`The record's firmware ${JSON.stringify(rec.firmware)} does not name ${rec.model}.`);
  if (!/stock/i.test(String(rec.bootloader || "")))
    throw recFail(`The record's bootloader is ${JSON.stringify(rec.bootloader)}; only a stock bootloader is a donor.`);
  if (!/m-flasher\s+\d+\.\d+\.\d+/i.test(String(rec.tool || "")))
    throw recFail(`The record names no M-Flasher version (got ${JSON.stringify(rec.tool)}).`);
  return {
    model: rec.model, jedec, flash: "0x" + flash[1].toUpperCase(), firmware: rec.firmware,
    version: fw.version, bootloader: rec.bootloader, dumpSha256: sha, tool: rec.tool,
    unitId: typeof rec.unit === "string" ? rec.unit : (typeof rec["unit id"] === "string" ? rec["unit id"] : null),
    saved: typeof rec.saved === "string" ? rec.saved : null,
    app: typeof rec.app === "string" ? rec.app : null,
  };
}

// The record alone is not enough to write: the part must be one this project writes. Reading and
// backing up a read-only part is fine; installing on it is refused, here and again in requireChip().
export function checkPart(rec) {
  if (WRITABLE_PARTS[rec.jedec]) return { part: WRITABLE_PARTS[rec.jedec], writable: true };
  if (READ_ONLY_PARTS[rec.jedec])
    throw Object.assign(new Error(`JEDEC ${rec.flash} (${READ_ONLY_PARTS[rec.jedec]}) is not a part this installer writes:`
      + " read it, back it up, and install only on a part whose write is proven."), { code: "chip" });
  throw Object.assign(new Error(`JEDEC ${rec.flash} is not in the documented 1 MiB allowlist: nothing was sent.`), { code: "chip" });
}

/* Two readings of the same unit, taken independently, are the only thing that turns "a backup exists"
 * into "the unit can be put back": a single read that nobody repeated is a claim, and a read that was
 * repeated and disagreed is two different images. `saved` must differ, because two files with the same
 * timestamp are one reading copied -- the tool writes that field, and a copy cannot fake a later one
 * without also claiming a reading that has a different hash. Everything else must agree exactly.
 *
 * This is corroboration, not proof: the files are on the reader's disk, and the page cannot see when
 * they were written. It is reported as such, and it lets the report say "two readings agree" instead
 * of "unknown" -- which is what the evidence actually is. */
export function corroborate(records) {
  const recs = (records || []).filter(Boolean);
  if (recs.length < 2)
    throw recFail("One record is one reading: drop the second record M-Flasher wrote for the same unit as well.");
  const [a] = recs;
  for (const r of recs) {
    if (r.model !== a.model) throw recFail(`The records name different models (${a.model}, ${r.model}): not one unit.`);
    if (r.jedec !== a.jedec) throw recFail(`The records name different chips (${a.flash}, ${r.flash}): not one unit.`);
    if (r.dumpSha256 !== a.dumpSha256)
      throw recFail(`The readings hash differently (${a.dumpSha256.slice(0, 16)}\u2026, ${r.dumpSha256.slice(0, 16)}\u2026):`
        + " two reads that disagree are two different images, so take a third read before trusting either.");
    if (r.unitId && a.unitId && r.unitId !== a.unitId)
      throw recFail(`The records name different units (${a.unitId}, ${r.unitId}): a dump from another unit cannot stand in.`);
    if (!r.saved || !a.saved) throw recFail("A record carries no `saved` timestamp: it cannot be told from a copy.");
  }
  const stamps = recs.map((r) => r.saved);
  if (new Set(stamps).size !== stamps.length)
    throw recFail("Two records carry the same `saved` timestamp: that is one reading, not two.");
  return { ok: true, readings: recs.length, dumpSha256: a.dumpSha256, jedec: a.jedec, flash: a.flash,
    model: a.model, unitId: a.unitId || null, tool: a.tool, saved: stamps.slice().sort(),
    source: `${recs.length} m-flasher records of this unit, same dump hash` };
}

// The dump, hashed here. This is what turns "a backup exists" into "these 1 MiB hash to the record".
export async function verifyDump(rec, bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (b.length !== DUMP_BYTES)
    throw recFail(`The dump is ${b.length} bytes, not ${DUMP_BYTES}: a partial read cannot be restored.`);
  const got = await recordHex(b);
  if (got !== rec.dumpSha256)
    throw recFail(`The dump hashes to ${got.slice(0, 16)}…, the record says ${rec.dumpSha256.slice(0, 16)}…: this is not that backup.`);
  return { ...rec, dumpBytes: b.length, verified: true, source: "m-flasher record + dump" };
}

// The evidence the install path accepts, or a refusal. `id` is what the unit answered just now: the
// record and the unit must agree on the model and on the firmware version, so a record kept from
// another unit (or another firmware) cannot stand in for this one.
export function recordFor(rec, id) {
  if (!rec || rec.verified !== true) throw recFail("The chip record has not been checked against its dump.");
  if (!id || rec.model !== id.model)
    throw Object.assign(new Error(`The record is for ${rec.model}, the unit answered ${id && id.model}: nothing was sent.`), { code: "chip" });
  if (Number(rec.version) !== Number(id.version))
    throw Object.assign(new Error(`The record was taken on ${rec.firmware}, the unit runs ${id.text}: nothing was sent.`), { code: "chip" });
  const { part } = checkPart(rec);
  // verified: the dump was hashed here and matched. bindRecord() requires it; nothing else sets it.
  return { ok: true, verified: true, jedec: rec.jedec, part, model: rec.model, version: Number(rec.version),
    dumpSha256: rec.dumpSha256, dumpBytes: rec.dumpBytes, tool: rec.tool, source: "m-flasher record + dump" };
}
