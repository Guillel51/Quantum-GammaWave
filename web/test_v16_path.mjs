// SPDX-License-Identifier: GPL-3.0-only
// The V16 (FM-1_016) install path, and the diagnosis a stock unit deserves.
//
// Two defects this file exists for, both reported from a real unit running FM-1_016 (V16):
//
//   1. web/index_pkg.html told the transport "15" for every stock unit it found. A V16 answers
//      FM-1_016, so donorIdentity() saw an identity saying 16 and a firmware saying 15 -- two
//      contradictory claims -- and refused with "the device is FM-1_016, the package is for
//      FM-1_965". The version has to come from the unit's own identity.
//   2. A stock firmware carries no 0x12 (the command is this project's own), and some USB-MIDI
//      stacks echo a frame back unchanged. Both were reported as "Truncated identification reply",
//      which reads as a broken chip query on a unit where the query does not exist.
//
// Class: host (node), the real modules. No unit is involved and nothing is sent.
import assert from "node:assert";
import fs from "node:fs";
import { probeChip, donorIdentity, pack7 } from "./fm1ota.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const page = fs.readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");

// ---- 1. the page passes the unit's own version ------------------------------------------------
ok(!/ctxFor\(fw\.kind === "stock" \? "15"/.test(page),
  "the page does not tell the transport \"15\" for every stock unit");
ok(/stockVersionOf\(fw\.text\)/.test(page),
  "the stock install passes the version the unit's identity reports");

/* The function the page defines, lifted out and run: the identity the unit answered is the only
 * source, and a string that cannot be read stays unreadable (the transport refuses it). */
const m = /const stockVersionOf = \(text\) => \{[\s\S]*?\n\};/.exec(page);
ok(!!m, "stockVersionOf is defined in the page");
const stockVersionOf = new Function("return (text) => " + m[0].replace(/^const stockVersionOf = \(text\) =>/, "").replace(/;$/, "") + "")();
ok(stockVersionOf("FM-1_016") === "16", "FM-1_016 reads as 16");
ok(stockVersionOf("FM-1_015") === "15", "FM-1_015 reads as 15");
ok(stockVersionOf("FM-1_965") === "unreadable", "a Felucca identity is not a stock version");
ok(stockVersionOf("") === "unreadable", "silence is unreadable, never a guess");
ok(stockVersionOf("FM-1_017") === "17", "an unread stock number is read as its number (and still refused downstream)");

// the pair that failed on the unit: FM-1_016 + the unit's own version is a donor, the old "15" was not
const id16 = { text: "FM-1_016", model: "FM-1", version: 16 };
ok(donorIdentity(id16, "FM-1_965", stockVersionOf("FM-1_016")), "FM-1_016 with the version it reported is a donor");
ok(!donorIdentity(id16, "FM-1_965", "15"), "the old hardcoded \"15\" is exactly what refused it");
ok(!donorIdentity(id16, "FM-1_965", stockVersionOf("FM-1_017")), "a version the unit did not report is refused");

// ---- 2. the chip diagnosis separates echo, partial and silence ---------------------------------
const unpack7 = (bytes) => {
  const out = []; let acc = 0, nb = 0;
  for (const v of bytes) { acc |= v << nb; nb += 7; while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } }
  return out;
};
/* A link that answers 0x12 the way `reply(body)` says. `echo` returns the request's own body: that is
 * what a looping MIDI stack does to a command the program does not implement. */
const link = (reply) => ({
  sent: 0, lost: false,
  send(frame) { this.sent++; this.last = frame; return true; },
  async read() { await new Promise((r) => setTimeout(r, 0)); return reply(unpack7(this.last.slice(1, -1))); },
  close() {},
});
/* a 0x12 frame as the module builds it: [0,0x59,cmd,lenLo,lenHi,0,...,sum] */
const replyBody = (req) => {
  const body = req.slice(6, 6 + (req[3] | (req[4] << 8)));
  return { cmd: 0x12, body };
};

/* a MIDI loopback returns the identical packet: that is the echo a stock unit's port produces */
const echo = { sent: 0, lost: false, send(f) { this.sent++; this.last = f; return true; },
  async read() { await new Promise((r) => setTimeout(r, 0)); return this.last.slice(); }, close() {} };
let failed = null;
try { await probeChip(echo, 200); } catch (e) { failed = e; }
ok(failed && /echoed the identification query back unchanged/.test(failed.message),
  "an echoed frame is reported as an echo, not as a truncated identification");
ok(failed && failed.code === "chip", ".. and it is a refusal of the chip gate (code chip)");

/* a reply frame the module's own decoder accepts: [0,0x59,cmd,n0,n1,n2, body..., sum], 7-bit packed */
const mkReply = (body, cmd = 0x12) => {
  const full = [0, 0x59, cmd, body.length & 0xFF, (body.length >> 8) & 0xFF, 0, ...body];
  let s = 0; for (const x of body) s += x;
  full.push(~s & 0xFF);
  return [0xF0, ...pack7(full), 0xF7];
};
const short = { sent: 0, lost: false, send(f) { this.sent++; this.last = f; return true; },
  async read() { await new Promise((r) => setTimeout(r, 0)); return mkReply([0x01, 0x02, 0x03]); }, close() {} };
failed = null;
try { await probeChip(short, 200); } catch (e) { failed = e; }
ok(failed && /is \d+ bytes, not 24/.test(failed.message),
  "a short, non-echo body says how long it was and that the answering program is not this firmware");

const silent = { sent: 0, lost: false, send() { this.sent++; return true; }, async read() { return null; }, close() {} };
const none = await probeChip(silent, 120, { optional: true });
ok(none === null, "silence stays silence where silence is expected (a loader link)");
failed = null;
try { await probeChip(silent, 120); } catch (e) { failed = e; }
ok(failed && /did not answer 0x12/.test(failed.message), ".. and is a refusal where an answer was required");

/* A well-formed identification of each admitted part, and the two refusals that matter: an unknown
 * JEDEC and a geometry that does not match the part named. */
const identify = (jedec, geom = { cap: 0x100000, page: 256, sector: 0x1000, block: 0x10000 }) => {
  let session = 0x1000;
  const frame = (req) => {
    const nonceBody = req.slice(6, 6 + (req[3] | (req[4] << 8)));
    const body = [0, 0, (jedec >> 16) & 0xFF, (jedec >> 8) & 0xFF, jedec & 0xFF,
      geom.cap & 0xFF, (geom.cap >> 8) & 0xFF, (geom.cap >> 16) & 0xFF, (geom.cap >>> 24) & 0xFF,
      geom.page & 0xFF, (geom.page >> 8) & 0xFF, geom.sector & 0xFF, (geom.sector >> 8) & 0xFF,
      geom.block & 0xFF, (geom.block >> 8) & 0xFF, (geom.block >> 16) & 0xFF, (geom.block >>> 24) & 0xFF,
      (session >> 0) & 0xFF, (session >> 8) & 0xFF, (session >> 16) & 0xFF, (session >> 24) & 0xFF];
    session += 0x11;
    /* 24 bytes: 21 of data, the CRC16 over them, and the one trailing byte the frame carries
     * (tools/fm1_probe.py reads crc16(r[:21]) from r[21:23] and requires len(r) == 24) */
    let c = 0;
    for (const x of body) { c ^= x << 8; for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
    body.push(c & 0xFF, (c >> 8) & 0xFF, 0);
    void nonceBody;
    /* the reply frame the module's qryDecode reads: [0,0x59,cmd,n0,n1,n2, body..., sum], 7-bit packed */
    const full = [0, 0x59, 0x12, body.length & 0xFF, (body.length >> 8) & 0xFF, 0, ...body];
    let s = 0; for (const x of body) s += x;
    full.push(~s & 0xFF);
    return [0xF0, ...pack7(full), 0xF7];
  };
  return { sent: 0, lost: false, send(f) { this.sent++; this.last = f; return true; },
    async read() { await new Promise((r) => setTimeout(r, 0)); return frame(this.last); }, close() {} };
};

const a = await probeChip(identify(0x856014), 200);
ok(a.jedec === 0x856014 && a.part === "P25Q80H" && a.writable, "P25Q80H (0x856014) is identified and writable");
const b = await probeChip(identify(0xB36014), 200);
ok(b.jedec === 0xB36014 && b.part === "UC25HQ80" && b.writable, "UC25HQ80 (0xB36014) is identified and writable");

failed = null;
try { await probeChip(identify(0x123456), 200); } catch (e) { failed = e; }
ok(failed && /not a part we know/.test(failed.message), "an unknown JEDEC is refused");
failed = null;
try { await probeChip(identify(0x856014, { cap: 0x800000, page: 256, sector: 0x1000, block: 0x10000 }), 200); } catch (e) { failed = e; }
ok(failed && /geometry/i.test(failed.message), "a geometry that is not the part's is refused");

// ---- 3. the V16 path has its own preconditions on the page ------------------------------------
ok(/id="v16-dump"/.test(page) && /id="v16-ack"/.test(page), "the page asks for the unit's complete copy");
ok(/sayK\("v16NeedDump"\)/.test(page), "an install on V16 without that copy sends nothing and says why");
ok(/sayK\("v16NeedAck"\)/.test(page), ".. and one without the acknowledgement does too");
ok(/V16_DUMP_SIZE = 0x100000/.test(page), "only a whole 1 MiB part is accepted as that copy");
ok(/v16Dump,/.test(page), "the record reaches the read-only verify report");
for (const lang of ["v16Title", "v16Text", "v16Dump", "v16Ack", "v16Warn"]) {
  const hits = page.split(lang + ":").length - 1;
  ok(hits >= 5, `${lang} is written in the five languages the page carries (${hits})`);
}
ok(/chipStock/.test(page), "a stock unit's chip check explains itself instead of probing an absent command");
ok(!/if \(lastVerify\.kind === "stock"\)[\s\S]{0,400}chipCheck\(/.test(page.split("if (lastVerify.kind === \"stock\")")[1]?.slice(0, 400) || ""),
  ".. and does not send the 0x12 frames a stock firmware does not answer");

console.log("v16 path: " + n + " checks, 0 FAILED");
