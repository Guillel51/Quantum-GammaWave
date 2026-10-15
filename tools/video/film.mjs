// SPDX-License-Identifier: GPL-3.0-only
// (JIANT 0.6.66) the v5 films: tools/video/film.html drawn frame by frame in Chromium (Playwright) from a capture
// directory (tools/video/capture_dark.c + film_prep.py), piped as JPEG to ffmpeg with the audio:
//   NODE_PATH=$(npm root -g) node tools/video/film.mjs build/video5/feat feat out.mp4
//   NODE_PATH=$(npm root -g) node tools/video/film.mjs build/video5/feat feat preview 100,400,900   -> preview-*.jpg
//   film "promo": tools/video/promo.html (the presentation film, tools/video/capture_promo.c); "story": stories.html (1080 x 1920)
// The fonts (the landing's: Michroma, IBM Plex Mono, Instrument Sans, VT323) come from build/video5/fonts.
import { createRequire } from "module";
import { fileURLToPath } from "url";
import { spawn } from "child_process";
import fs from "fs";
import path from "path";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const [dir, film, out, list] = process.argv.slice(2);
const FONTS = path.join(root, "build/video5/fonts");
const scr = fs.openSync(path.join(dir, "screen.rgb"), "r");
const ver = /FELUCCA_VERSION "(v[^"]+)"/.exec(fs.readFileSync(path.join(root, "firmware/src/felucca.c"), "utf8"))[1];
const exe = process.env.CHROMIUM || "/opt/pw-browsers/chromium-1194/chrome-linux/chrome";
const browser = await chromium.launch({ executablePath: exe }).catch(() => chromium.launch());
const page = await browser.newPage({ viewport: film.startsWith("story") ? { width: 1080, height: 1920 } : { width: 1920, height: 1080 } });
page.on("console", m => { if (m.type() === "error") console.error("page:", m.text()); });
page.on("pageerror", e => console.error("page error:", e.message));
await page.route("http://film/**", async r => {
  const u = new URL(r.request().url()), p = u.pathname;
  if (p.startsWith("/scr/")) {
    const f = parseInt(p.slice(5), 10), b = Buffer.alloc(172800);
    fs.readSync(scr, b, 0, 172800, f * 172800);
    return r.fulfill({ body: b, contentType: "application/octet-stream" });
  }
  if (p === "/data.json") return r.fulfill({ path: path.join(dir, "data.json"), contentType: "application/json" });
  if (p === "/version") return r.fulfill({ body: ver, contentType: "text/plain" });
  if (p.startsWith("/fonts/")) return r.fulfill({ path: path.join(FONTS, path.basename(p)) });
  return r.fulfill({ path: path.join(root, film === "promo" ? "tools/video/promo.html" : film === "plate" ? "tools/video/plate.html" : film.startsWith("story") ? "tools/video/stories.html" : "tools/video/film.html"), contentType: "text/html" });
});
await page.goto("http://film/film.html");
const n = await page.evaluate(f => boot(f), film);
if (out === "preview") {
  const want = (list || "60").split(",").map(Number);
  const last = Math.max(...want);
  for (let f = Math.max(0, Math.min(...want) - 90); f <= last; f++) {      // (from a little before: the beings swim)
    const d = await page.evaluate(([f]) => frameJpeg(f, .9), [f]);
    if (want.includes(f)) fs.writeFileSync(path.join(dir, `preview-${String(f).padStart(5, "0")}.jpg`), Buffer.from(d.split(",")[1], "base64"));
  }
} else {
  const ff = spawn("ffmpeg", ["-y", "-loglevel", "error", "-f", "image2pipe", "-framerate", "30", "-c:v", "mjpeg", "-i", "-",
    "-i", path.join(dir, "audio.wav"), "-c:v", "libx264", "-preset", "medium", "-crf", "20", "-pix_fmt", "yuv420p",
    "-c:a", "aac", "-b:a", "256k", "-shortest", "-movflags", "+faststart", out], { stdio: ["pipe", "inherit", "inherit"] });
  const t0 = Date.now();
  for (let f = 0; f < n; f++) {
    const d = await page.evaluate(([f]) => frameJpeg(f, .93), [f]);
    if (!ff.stdin.write(Buffer.from(d.split(",")[1], "base64"))) await new Promise(r => ff.stdin.once("drain", r));
    if (f % 300 === 0) console.log(`${f}/${n} frames, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
  }
  ff.stdin.end();
  await new Promise(r => ff.on("close", r));
  console.log(out);
}
await browser.close();
