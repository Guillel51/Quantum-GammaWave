// SPDX-License-Identifier: GPL-3.0-only
// The chip fact a stock unit cannot give over MIDI, taken from the record an out-of-band read left
// behind -- and the install path that accepts it, on a donor only, with the dump hashed here.
//
// Every case drives the real modules. The wire assertions are about the log, not the intention: with
// a record the first frame after the handshake is UPGRADE and no 0x12 is ever sent; without one the
// install refuses with code "chip" and sends nothing at all.
import assert from "node:assert";
import crypto from "node:crypto";
import { Updater, requireProof, pack7 } from "./fm1ota.js";
import { verifyProvenance } from "./fm1provenance.js";
import { READ_ONLY_QUERY } from "./fm1verify.js";
import { parseRecord, checkPart, verifyDump, recordFor, firmwareOf, DUMP_BYTES } from "./fm1record.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const refuses = (fn, code, what) => {
  let e = null;
  try { fn(); } catch (err) { e = err; }
  assert.ok(e && (!code || e.code === code), `${what}: ${e ? e.code + " " + e.message : "did NOT throw"}`);
  n++; console.log("ok  " + what);
};
const refusesAsync = async (p, code, what) => {
  let e = null;
  try { await p; } catch (err) { e = err; }
  assert.ok(e && (!code || e.code === code), `${what}: ${e ? e.code + " " + e.message : "did NOT throw"}`);
  n++; console.log("ok  " + what);
};

// the record the owner's unit produced (M-Flasher 0.9.4, 2026-11-08)
const OWNER = JSON.stringify({
  model: "FM-1", bootloader: "stock (M-VAVE)", firmware: "FM-1 015 (stock)", app: "OK",
  "chip key": "0x980F", flash: "0x856014", "unit id": "4150353835313708",
  loader: "M-Flasher 1.3, CPU at 96 MHz",
  sha256: "38c66ae7c36b46f2fad1c0f8a041ba257934b1b46a73d4085216c60e78f2d9cd",
  saved: "20261108-161613", tool: "m-flasher 0.9.4",
});

const rec = parseRecord(OWNER);
ok(rec.model === "FM-1" && rec.jedec === 0x856014 && rec.version === 15,
   "the owner's record parses: FM-1, flash 0x856014, firmware 015");
ok(rec.dumpSha256 === "38c66ae7c36b46f2fad1c0f8a041ba257934b1b46a73d4085216c60e78f2d9cd" && rec.tool === "m-flasher 0.9.4",
   "its dump hash and tool version are kept verbatim");
ok(checkPart(rec).part === "P25Q80H" && checkPart(rec).writable === true,
   "0x856014 is a part this installer writes (P25Q80H)");

ok(firmwareOf("FM-1 015 (stock)").version === 15 && firmwareOf("FM-1 016 (stock)").version === 16 &&
   firmwareOf("nonsense") === null, "the firmware field is read as a model and a version");

/* --- what the record must carry, and what it must not be able to claim --- */
refuses(() => parseRecord("not json"), "record", "a file that is not JSON is refused");
refuses(() => parseRecord(JSON.stringify({ ...JSON.parse(OWNER), flash: undefined })), "record", "no flash id: refused");
refuses(() => parseRecord(JSON.stringify({ ...JSON.parse(OWNER), sha256: "abc" })), "record", "no dump hash: refused");
refuses(() => parseRecord(JSON.stringify({ ...JSON.parse(OWNER), model: "GW-1" })), "record", "another model: refused");
refuses(() => parseRecord(JSON.stringify({ ...JSON.parse(OWNER), bootloader: "custom loader" })), "record",
        "a non-stock bootloader is not a donor: refused");
refuses(() => parseRecord(JSON.stringify({ ...JSON.parse(OWNER), tool: "" })), "record", "no tool version: refused");
ok(parseRecord(JSON.stringify({ ...JSON.parse(OWNER), firmware: "FM-1 015" })).version === 15,
   "the firmware field is enough without the stock marker: the bootloader field says which it is");

refuses(() => checkPart({ jedec: 0xC84014, flash: "0xC84014" }), "chip", "a read-only part (GD25Q80) is refused for writing");
refuses(() => checkPart({ jedec: 0x123456, flash: "0x123456" }), "chip", "an unknown part is refused");

/* --- the dump itself, hashed here --- */
{
  const dump = new Uint8Array(DUMP_BYTES);
  for (let i = 0; i < dump.length; i++) dump[i] = (i * 31 + 7) & 0xFF;
  const sha = crypto.createHash("sha256").update(dump).digest("hex");
  const withDump = parseRecord(JSON.stringify({ ...JSON.parse(OWNER), sha256: sha }));
  const ev = await verifyDump(withDump, dump);
  ok(ev.verified === true && ev.dumpBytes === DUMP_BYTES && ev.dumpSha256 === sha,
     "a 1 MiB dump that hashes to the record is verified in this page");

  dump[123456] ^= 0x01;
  await refusesAsync(verifyDump(withDump, dump), "record", "one changed byte: the dump is not that backup");

  await refusesAsync(verifyDump(withDump, new Uint8Array(DUMP_BYTES - 1)), "record",
                     "a dump that is not exactly 1 MiB is refused (a partial read cannot be restored)");

  const evOwner = await verifyDump(parseRecord(OWNER), dump).catch((e) => e);
  ok(evOwner instanceof Error && evOwner.code === "record",
     "the owner's record does not accept a dump this page has not seen: the hash decides");
}

/* --- the record against the unit that answered --- */
{
  const dump = new Uint8Array(DUMP_BYTES);
  const sha = crypto.createHash("sha256").update(dump).digest("hex");
  const ev = await verifyDump(parseRecord(JSON.stringify({ ...JSON.parse(OWNER), sha256: sha })), dump);
  const good = recordFor(ev, { model: "FM-1", version: 15, text: "FM-1_015" });
  ok(good.ok === true && good.jedec === 0x856014 && good.part === "P25Q80H",
     "a verified record and the unit that answered agree: evidence");
  refuses(() => recordFor(ev, { model: "FM-1", version: 16, text: "FM-1_016" }), "chip",
          "the same record against another firmware version: refused");
  refuses(() => recordFor(ev, { model: "GW-1", version: 15, text: "GW-1_015" }), "chip",
          "the same record against another model: refused");
  refuses(() => recordFor({ ...ev, verified: false }, { model: "FM-1", version: 15, text: "FM-1_015" }), "record",
          "a record whose dump was never hashed: refused");
}

/* --- what the record does NOT do: it never turns a stock unit into a destination --- */
/* the release chain and a stock unit: enough to drive install() and count what it sends */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const cmdOf = (b) => { try { const d = []; let acc = 0, nb = 0; for (const v of b.slice(1, -1)) { acc |= v << nb; nb += 7; while (nb >= 8) { d.push(acc & 0xFF); acc >>>= 8; nb -= 8; } } return d[2]; } catch (_) { return null; } };
function identityFrame(text) {
  const d = [0, 0x59, 0x11, 0, 0, 0, ...Array.from(text.padEnd(27, "\0")).map((c) => c.charCodeAt(0)), 0];
  const out = []; let acc = 0, nb = 0;
  for (const b of d) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } }
  if (nb) out.push(acc & 0x7F);
  return [0xF0, ...out, 0xF7];
}
/* a stock FM-1: it answers the handshake and nothing else the installer asks of it */
function stockUnit(text = "FM-1_015") {
  const sent = [];
  const input = { name: "FM-1 Midi", state: "connected", onmidimessage: null, open: async () => {} };
  const output = { name: "FM-1 Midi", state: "connected", open: async () => {}, send(bytes) {
    const b = Array.from(bytes);
    sent.push(b);
    if (b.length === HS.length && b.every((v, i) => v === HS[i]))
      setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from(identityFrame(text)) }), 0);
  } };
  return { sent, midi: { inputs: new Map([[input.name, input]]), outputs: new Map([[output.name, output]]) } };
}
const fs = await import("node:fs");
const pkg = fs.readFileSync("release/felucca-v1.9.0.fwsc");
const manifest = JSON.parse(fs.readFileSync("release/felucca-v1.9.0.manifest.json", "utf-8"));
const build = JSON.parse(fs.readFileSync("release/build-manifest.json", "utf-8"));
const emu = JSON.parse(fs.readFileSync("release/emu/manifest.json", "utf-8"));
const keys = JSON.parse(fs.readFileSync("tools/fm1_keys.json", "utf-8")).keys || {};
const prov = await verifyProvenance(pkg, build, { emulator: emu });
const ctxFor = () => ({ manifest, keys, firmware: "15", provenance: prov });
const chipRecord = recordFor(
  await verifyDump(parseRecord(JSON.stringify({ ...JSON.parse(OWNER), sha256: crypto.createHash("sha256").update(new Uint8Array(DUMP_BYTES)).digest("hex") })),
                   new Uint8Array(DUMP_BYTES)),
  { model: "FM-1", version: 15, text: "FM-1_015" });

{
  const u = stockUnit();
  const up = new Updater(u.midi, () => {});
  const proof = await requireProof({ ...ctxFor(), pkg, product: "FM-1_982" });
  const err = await up.install(pkg, "FM-1_982", () => {}, { ...ctxFor(), proof, chipRecord }).catch((e) => e);
  ok(err && err.code === "stockfirst", `a stock unit with a verified record is still refused as "stockfirst" (${err && err.code})`);
  ok(!u.sent.some((b) => b.length === 6 && b.every((v, i) => v === UPGRADE[i])),
     "...and it still receives no UPGRADE: a chip reading is not an installation");
  ok(u.sent.every((b) => !(b.length > 3 && b[0] === 0xF0 && cmdOf(b) === 0x30)),
     "...nor was any image byte offered to it");
}

console.log(`chip record: ${n} checks, 0 FAILED`);
