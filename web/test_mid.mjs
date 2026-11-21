// SPDX-License-Identifier: GPL-3.0-only
// (FASE 6) The .MID export: the 4-bar song as a Standard MIDI File.
//
// The studio could render the song to .WAV and save it as a project, but the notes were trapped in the page. This
// test lifts `gwMidiBytes` out of editor.html and reads the file it writes back with an SMF parser written here:
// header, division, tempo, one track per unmuted lane, drums on channel 10, every note-on closed, the pad held
// to its next onset, and the muted lane absent. Class: host (node), the real function, no browser.
import assert from "node:assert";
import fs from "node:fs";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };
const page = fs.readFileSync(new URL("./editor.html", import.meta.url), "utf8");

// ---- the function, lifted (its body ends at the first line that is a lone closing brace)
const start = page.indexOf("function gwMidiBytes(parts, bpm) {");
ok(start > 0, "editor.html defines gwMidiBytes");
let i = page.indexOf("{", start), depth = 0, end = -1;
for (; i < page.length; i++) {
  if (page[i] === "{") depth++;
  else if (page[i] === "}" && --depth === 0) { end = i + 1; break; }
}
ok(end > start, "its body is closed");
const gwMidiBytes = new Function(page.slice(start, end) + "\nreturn gwMidiBytes;")();

// ---- a song, invented here: three melodic lanes and drums, one lane muted
const stepsOf = (spec) => Array.from({ length: 64 }, (_, k) => spec[k] || 0);
const PARTS = [
  { name: "BASS", steps: stepsOf({ 0: 36, 4: 43, 8: 36, 12: 48 }) },
  { name: "ARP", steps: stepsOf({ 0: 60, 2: 67 }), muted: true },
  { name: "PAD", steps: stepsOf({ 0: 48, 16: 51 }), hold: true },
  { name: "DRUMS", drum: true, steps: stepsOf({ 0: 1, 4: 2, 8: 3, 12: 1 }) },
];
const BPM = 124;
const bytes = gwMidiBytes(PARTS, BPM);

// ---- an SMF reader: header, then each MTrk's events with their running tick
const be32 = (b, o) => ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0;
const be16 = (b, o) => (b[o] << 8) | b[o + 1];
const rd = (b, o) => { let v = 0, k = 0; for (;;) { const c = b[o + k++]; v = (v << 7) | (c & 127); if (!(c & 128)) break; } return [v, k]; };
const asciiAt = (b, o) => String.fromCharCode(...b.slice(o, o + 4));

ok(bytes instanceof Uint8Array, "it returns bytes");
ok(asciiAt(bytes, 0) === "MThd", "the file starts with MThd");
ok(be32(bytes, 4) === 6, "the header is 6 bytes");
const format = be16(bytes, 8), ntrks = be16(bytes, 10), div = be16(bytes, 12);
ok(format === 1, "format 1 (a conductor track and one per part)");
ok(div === 480, "480 ticks a quarter");
ok(ntrks === 4, `one conductor track and three part tracks (the muted lane is not written): ${ntrks}`);

const parse = (b) => {
  const ntrks = be16(b, 10), out = [];
  for (let o = 14, t = 0; t < ntrks; t++) {
    ok(asciiAt(b, o) === "MTrk", `track ${t} is an MTrk chunk`);
    const len = be32(b, o + 4), from = o + 8, to = from + len;
    let p = from, tick = 0, running = 0;
    const ev = [];
    while (p < to) {
      const [d, k] = rd(b, p); p += k; tick += d;
      if (b[p] === 0xFF) {
        const type = b[p + 1], ln = b[p + 2];
        ev.push({ tick, meta: type, data: b.slice(p + 3, p + 3 + ln) });
        p += 3 + ln;
        if (type === 0x2F) break;
        continue;
      }
      let st = b[p];
      if (st & 0x80) { running = st; p++; } else st = running;
      const hi = st & 0xF0, chan = st & 0x0F, d1 = b[p];
      const d2 = (hi === 0xC0 || hi === 0xD0) ? null : b[p + 1];
      p += (hi === 0xC0 || hi === 0xD0) ? 1 : 2;
      ev.push({ tick, hi, chan, d1, d2 });
    }
    ok(p === to, `track ${t} parses to its declared length`);
    ok(ev.some((e) => e.meta === 0x2F), `track ${t} ends with an end-of-track`);
    out.push(ev);
    o = to;
  }
  return out;
};
const tracks = parse(bytes);
ok(tracks.length === 4, "four tracks read");

// ---- the conductor: tempo and time signature
const cond = tracks[0];
const tempo = cond.find((e) => e.meta === 0x51);
ok(!!tempo, "the conductor carries a tempo");
const us = (tempo.data[0] << 16) | (tempo.data[1] << 8) | tempo.data[2];
ok(Math.abs(us - Math.round(60000000 / BPM)) <= 1, `the tempo is the song's own (${us} us = ${(60000000 / us).toFixed(2)} BPM)`);
const ts = cond.find((e) => e.meta === 0x58);
ok(ts && ts.data[0] === 4 && Math.pow(2, ts.data[1]) === 4, "the time signature is 4/4");

// ---- the parts: notes, channels, ticks and gates
const STEP = 120;
const ons = (tr, ch) => tr.filter((e) => e.hi === 0x90 && e.d2 > 0 && (ch === undefined || e.chan === ch));
const offs = (tr, ch) => tr.filter((e) => e.hi === 0x80 && (ch === undefined || e.chan === ch));
const bass = tracks[1], pad = tracks[2], drum = tracks[3];
const bOn = ons(bass);
ok(bOn.length === 4, `the bass track has its four notes (${bOn.length})`);
ok(bOn.map((e) => e.tick).join(",") === [0, 4, 8, 12].map((x) => x * STEP).join(","),
  "the bass notes sit on their sixteenths");
ok(bOn.map((e) => e.d1).join(",") === "36,43,36,48", "the bass notes are the ones the song has");
ok(bOn.every((e) => e.d2 === 96), "the notes carry a velocity");
for (const on of bOn)
  ok(!!offs(bass).find((e) => e.d1 === on.d1 && e.tick === on.tick + STEP), `note ${on.d1} at ${on.tick} is released a sixteenth later`);
const pOn = ons(pad);
ok(pOn.length === 2 && pOn[0].tick === 0 && pOn[1].tick === 16 * STEP, "the pad's two notes sit at its bars");
ok(!!offs(pad).find((e) => e.d1 === pOn[0].d1 && e.tick === 16 * STEP),
  "the pad's first note is held until the next onset, not cut a sixteenth into the bar");
const dOn = ons(drum);
ok(dOn.length === 4 && dOn.every((e) => e.chan === 9), "the drums are on channel 10");
ok(dOn.map((e) => e.d1).join(",") === "36,38,42,36", "the drum lanes become GM notes (KICK, SNARE, HAT)");
ok(offs(drum).length === dOn.length, "every drum hit is released");

const names = tracks.map((tr) => {
  const m = tr.find((e) => e.meta === 0x03);
  return m ? String.fromCharCode(...m.data) : "";
});
ok(names.includes("BASS") && names.includes("PAD") && names.includes("DRUMS"), `the live lanes name their tracks (${names.join(", ")})`);
ok(!names.includes("ARP"), "the muted lane is not written at all");

// ---- every note that opens, closes; nothing hangs
for (const [t, tr] of tracks.entries())
  ok(ons(tr).length === offs(tr).length, `track ${t}: ${ons(tr).length} note-ons and ${offs(tr).length} note-offs`);
ok(Math.max(...tracks.slice(1).flatMap((tr) => ons(tr).map((e) => e.tick))) < 64 * STEP, "no note lands past the song's four bars");

// ---- the edges: no parts at all, a lane longer than 64, a zero tempo, a bad lane value
const empty = gwMidiBytes([{ name: "X", steps: Array(64).fill(0) }], 120);
ok(be16(empty, 10) === 1, "a song with nothing in it still writes a valid file (the conductor alone)");
const long = parse(gwMidiBytes([{ name: "L", steps: Array(200).fill(60) }], 120));
ok(ons(long[1]).length === 64, `a lane longer than the song is cut at its four bars (${ons(long[1]).length})`);
ok(be16(gwMidiBytes([{ name: "X", steps: Array(64).fill(0) }], 0), 10) === 1, "a tempo of zero does not break the file");
const odd = gwMidiBytes([{ name: "D", drum: true, steps: [9, 9] }, { name: "M", steps: [0, 200] }], 100);
ok(odd.length > 14, "an out-of-range lane value is written, not a crash");
ok(parse(odd).length === 3, "an out-of-range value still writes one track per live lane (and the conductor)");

console.log("mid export: " + n + " checks, 0 FAILED");
