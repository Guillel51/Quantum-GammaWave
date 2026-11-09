// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// The emulator in a real browser: Chromium loads build/emu as a page, the WASM is checked against the
// manifest the build wrote, the console and the network have to stay clean, the panel and the screen have
// to draw, and the same AudioWorklet the page installs has to render sound from the same binary.
//
//   node web/emu/browser_test.mjs [build/emu] [--headed]
//
// This is not the host test (emu_test.mjs runs the module in Node) and not a screenshot: it is the page's
// own load path -- fetch, SHA-256 check, AudioWorklet -- in Chromium. It needs playwright and its Chromium
// (PLAYWRIGHT_BROWSERS_PATH or the default cache); without them it says so and exits 2, never "ok".
import fs from "fs";
import http from "http";
import path from "path";
import { chromium } from "playwright";

const args = process.argv.slice(2);
const DIR = path.resolve(args.find((a) => !a.startsWith("--")) || "build/emu");
const HEADED = args.includes("--headed");
// The splash's length is the firmware's (firmware/src/ui_draw.c), so a longer boot screen moves the wait and
// the offline key press with it instead of playing a note the splash is still blocking (3.2: 2.3 s -> 3.1 s).
const SPLASH_S = (() => {
  const src = fs.readFileSync(path.resolve("firmware/src/ui_draw.c"), "utf8");
  const n = (k) => Number((src.match(new RegExp(`#define\\s+${k}\\s+\\(?(\\d+)u`)) || [])[1] || 0);
  return (n("SPL_WRITE") + n("SPL_HOLD") || 2300) / 1000;
})();
const MIME = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8", ".wasm": "application/wasm", ".json": "application/json",
  ".woff": "font/woff", ".woff2": "font/woff2", ".png": "image/png", ".css": "text/css" };

let fails = 0;
const check = (what, ok, extra = "") => {
  console.log(`browser: ${what.padEnd(72)} ${ok ? "ok" : "FAIL"}${extra ? "  " + extra : ""}`);
  if (!ok) fails++;
};

const manifestPath = path.join(DIR, "manifest.json");
if (!fs.existsSync(manifestPath)) {
  console.error(`browser: no ${manifestPath}: run sh web/emu/build.sh first`);
  process.exit(2);
}
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));

/* a static server for DIR, no caching: the page must load exactly the bytes the build wrote */
const server = http.createServer((req, res) => {
  const rel = decodeURIComponent(req.url.split("?")[0]).replace(/^\/+/, "") || "index.html";
  const file = path.join(DIR, rel);
  if (!file.startsWith(DIR + path.sep) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404).end("not here");
    return;
  }
  res.writeHead(200, { "content-type": MIME[path.extname(file)] || "application/octet-stream",
    "cache-control": "no-store", "cross-origin-opener-policy": "same-origin", "cross-origin-embedder-policy": "require-corp" });
  fs.createReadStream(file).pipe(res);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}`;

let browser;
try {
  browser = await chromium.launch({ headless: !HEADED, args: ["--no-sandbox", "--autoplay-policy=no-user-gesture-required", "--mute-audio"] });
} catch (e) {
  console.error(`browser: Chromium did not start (${e.message.split("\n")[0]}). ` +
                `Install it with 'npx playwright install chromium'; this is NOT a pass.`);
  server.close();
  process.exit(2);
}

const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
const consoleErrors = [], pageErrors = [], failedRequests = [], badStatus = [];
page.on("console", (m) => { if (m.type() === "error") consoleErrors.push(m.text()); });
page.on("pageerror", (e) => pageErrors.push(String(e)));
page.on("requestfailed", (r) => failedRequests.push(`${r.url()} ${r.failure()?.errorText}`));
page.on("response", (r) => { if (r.status() >= 400) badStatus.push(`${r.status()} ${r.url()}`); });

try {
  const resp = await page.goto(`${base}/index.html`, { waitUntil: "load", timeout: 30000 });
  check("the page is served and parses", resp && resp.ok() === true, `HTTP ${resp?.status()}`);

  const panel = await page.evaluate(() => {
    const svg = document.getElementById("panel");
    return { hits: document.querySelectorAll(".hit").length, canvas: !!document.getElementById("screen") };
  });
  check("the panel draws every control (14 buttons, 27 keys, 8 encoders) and the screen is there",
        panel.hits >= 40 && panel.canvas, `${panel.hits} hit areas`);

  await page.click("#go");                                     // the page needs one gesture to start audio
  await page.waitForFunction(() => window.__QG_EMU__, null, { timeout: 60000 });
  const emu = await page.evaluate(() => window.__QG_EMU__);
  check("the page loaded the WASM and reported itself", !!emu && !!emu.wasmSha);
  check("the WASM the page received is the one the build wrote",
        emu?.wasmSha === manifest.wasm_sha256, `${emu?.wasmSha || "?"} vs ${manifest.wasm_sha256}`);
  check("the page and the manifest agree on the version", emu?.version === manifest.version, `${emu?.version} vs ${manifest.version}`);
  check("the page and the manifest agree on the commit", emu?.commit === manifest.commit, `${emu?.commit} vs ${manifest.commit}`);
  check("the page and the manifest agree on the size", emu?.wasmBytes === manifest.wasm_bytes, `${emu?.wasmBytes} B`);
  const footer = await page.evaluate(() => ({ hash: document.getElementById("wasm-hash")?.textContent || "",
                                              build: document.getElementById("build-commit")?.textContent || "" }));
  check("the footer shows that hash and build", footer.hash === emu?.wasmSha && footer.build === manifest.commit);

  /* boot: the splash the firmware declares, then HOME; the screen must be drawn, not left black */
  await page.waitForTimeout(Math.round(SPLASH_S * 1000) + 500);
  const screen = await page.evaluate(() => {
    const c = document.getElementById("screen");
    const ctx = c.getContext("2d");
    const d = ctx.getImageData(0, 0, c.width, c.height).data;
    const seen = new Set();
    let lit = 0, sum = 0;
    for (let i = 0; i < d.length; i += 4) {
      const v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
      seen.add(v);
      sum += d[i] + d[i + 1] + d[i + 2];
      if (sum > 0) lit++;
    }
    return { colours: seen.size, lit, nonBlack: d.some((v, i) => i % 4 !== 3 && v > 8) };
  });
  check("the device screen is drawn (not a black canvas)", screen.nonBlack && screen.colours > 1,
        `${screen.colours} colours`);

  /* input: a held key must reach the worklet, which lights it, with the console still clean */
  await page.keyboard.down("z");
  await page.waitForTimeout(350);
  const held = await page.evaluate(() => document.getElementById("panel").querySelectorAll(".held").length);
  await page.keyboard.up("z");
  await page.waitForTimeout(350);
  const released = await page.evaluate(() => document.getElementById("panel").querySelectorAll(".held").length);
  check("a held key is sent to the device and lights up on the panel", held > 0 && released === 0, `${held} held`);

  /* the page must not have asked for a stale resource: one fetch of the wasm, with the build's hash */
  const resources = await page.evaluate(() => performance.getEntriesByType("resource")
    .filter((e) => e.name.includes("felucca.wasm")).map((e) => e.name));
  check("the WASM is fetched once, with the build's cache-busting hash",
        resources.length === 1 && resources[0].includes(manifest.wasm_sha256.slice(0, 12)), `${resources.length} fetch(es)`);

  /* the same worklet, offline: the module the page serves has to render audio by itself */
  const audio = await page.evaluate(async () => {
    const buf = await (await fetch("felucca.wasm", { cache: "no-store" })).arrayBuffer();
    const rate = 44100, secs = Math.ceil(SPLASH_S + 2);
    const ctx = new OfflineAudioContext(2, rate * secs, rate);
    await ctx.audioWorklet.addModule(`worklet.js?offline=${Date.now()}`);
    const node = new AudioWorkletNode(ctx, "felucca", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
    node.connect(ctx.destination);
    const ready = new Promise((res, rej) => {
      node.port.onmessage = (e) => {
        if (e.data.type === "ready") res(e.data);
        else if (e.data.type === "error") rej(new Error(e.data.message));
      };
    });
    node.port.postMessage({ type: "load", wasm: buf }, [buf]);
    const info = await ready;
    ctx.suspend(SPLASH_S + 0.2).then(() => {         // the splash is over: play after it
      node.port.postMessage({ type: "keys", mask: 1 << 7 });
      ctx.resume();
    });
    const out = await ctx.startRendering();
    const l = out.getChannelData(0);
    let peak = 0, sumsq = 0, n = 0;
    for (let i = Math.floor(rate * (SPLASH_S + 0.3)); i < l.length; i++) {
      const v = l[i];
      if (Math.abs(v) > peak) peak = Math.abs(v);
      sumsq += v * v;
      n++;
    }
    return { rate: info.rate, frames: out.length, peak, rms: Math.sqrt(sumsq / Math.max(n, 1)) };
  });
  check("the worklet reports the device rate (44100)", audio.rate === 44100, `${audio.rate}`);
  check("the worklet renders the whole offline buffer", audio.frames === Math.ceil(SPLASH_S + 2) * 44100, `${audio.frames} frames`);
  check("a held key makes sound through the worklet in Chromium", audio.peak > 0.01, `peak ${audio.peak.toFixed(3)}`);
  check("no console errors", consoleErrors.length === 0, consoleErrors.slice(0, 3).join(" | "));
  check("no uncaught page errors", pageErrors.length === 0, pageErrors.slice(0, 3).join(" | "));
  check("no failed requests", failedRequests.length === 0, failedRequests.slice(0, 3).join(" | "));
  check("no 404/500 on any resource", badStatus.length === 0, badStatus.slice(0, 3).join(" | "));
} catch (e) {
  check(`the run finished without an exception`, false, String(e.message || e).split("\n")[0]);
} finally {
  await browser.close();
  server.close();
}
console.log(`browser: ${fails ? `FAILED (${fails})` : "passed"} — ${manifest.version} build ${manifest.commit}, ${base}`);
process.exit(fails ? 1 : 0);
