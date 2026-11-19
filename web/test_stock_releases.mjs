// The two official M-VAVE releases this installer knows (V15 and V16).
//
// What this proves, and what it does not: the pins, the layout checks and the loader-window rule
// are host logic and are proven here. With no argument it never touches an official file. Pass the
// path of an official FM-1.fwsc (`node web/test_stock_releases.mjs /path/FM-1.fwsc`) and it also
// parses that file and checks it against its profile -- host, bytes, no unit involved.
import assert from "node:assert/strict";
import fs from "node:fs";
import {
  LOADER_APP_HI, STOCK_RELEASES, STOCK_V15_SHA256, STOCK_V16_SHA256, STOCK_V16_SIZE,
  STOCK_V16_IMAGE_SHA256, STOCK_APP_SIZE, STOCK_IDENTITIES, Package, hasFeluccaLoader,
  isStockIdentity, loaderWindowIssue, stockLayoutIssue, stockReleaseBySize, validateStockPackage,
} from "./fm1pkg.js";

let n = 0;
const ok = (cond, what) => { assert.ok(cond, what); n++; console.log("ok  " + what); };

// --- the pins -------------------------------------------------------------------------------
ok(STOCK_V15_SHA256 === "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a",
  "V15 is pinned by the digest this project has always shipped");
ok(STOCK_V16_SHA256 === "d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5",
  "V16 is pinned to the digest of the file M-VAVE publishes");
ok(STOCK_V16_SIZE === 718196, "V16 is 718 196 B, the size of the published file");
ok(STOCK_V16_IMAGE_SHA256 === "cca5d6910017344674e6128e529876572ce443bcae41186d0d69768f0d1f1d2a",
  "V16's logical image (the bytes the device reads) is pinned too");
ok(STOCK_RELEASES.length === 2 && STOCK_RELEASES.every((r) => r.identity === `FM-1_0${r.version}`),
  "each release carries its own identity: V15 -> FM-1_015, V16 -> FM-1_016");

// --- the V16 layout is the one measured from the file --------------------------------------
{
  const v16 = stockReleaseBySize(718196);
  const L = v16.layout;
  ok(v16 && v16.version === 16 && v16.identity === "FM-1_016", "718 196 B is the official V16 release");
  ok(L.flashFileOff === 1044 && L.flashSize === 0xA9000, "V16: flash.bin at 1044, image 0xA9000 B");
  ok(L.appBase === 0x4000 && L.appOff === 0x4140, "V16: app_dir_head at 0x4000, app.bin at 0x4140");
  ok(L.appSize === 597928 && L.appDirSize === 0xA4071, "V16: app slot 597 928 B, app area block 0xA4071 B");
  ok(L.otaSize === 19969, "V16: the vendor's own ota.bin is 19 969 B");
  ok(v16.reference.headSha256 === "8f5228c2d6ea2068838ef4916f946bea877d14b07e6cfca49e41957f4c7b234f" &&
     v16.reference.appRegionSha256 === "c47d8895963c070f9334efd6a617d7aae5daf8480ffcf3438ffb634c03de8ef7" &&
     v16.reference.appRegion93000Sha256 === "980ff51ae2d90004cf3887602ebd11cd31796977410eb77cc7faabad4023003c",
  "V16: the three published region hashes are recorded with the profile");
  /* the consequence that decides the install path: V16's app area leaves the loader's window */
  ok(L.appBase + L.appDirSize > LOADER_APP_HI,
    "V16's app area ends past the update loader's window (0x93000): a vendor V16 file is not patched into a QG package");
  ok(L.appOff + L.appSize <= L.flashSize, "V16's app.bin still fits inside its own flash image");
}

// --- identity helpers ----------------------------------------------------------------------
ok(isStockIdentity("FM-1_015") && isStockIdentity("FM-1_016"), "both official identities are stock");
ok(!isStockIdentity("FM-1_017") && !isStockIdentity("FM-1_965") && !isStockIdentity(""),
  "an unread stock number and a Felucca identity are not stock");
ok(STOCK_IDENTITIES.join(",") === "FM-1_015,FM-1_016", "the stock identity list is exactly V15 and V16");
ok(stockReleaseBySize(699956).version === 15 && stockReleaseBySize(1234) === null,
  "a size matches a release only when it is that release's size");

// --- the validator refuses what it cannot pin ----------------------------------------------
{
  let msg = "";
  try { await validateStockPackage(new Uint8Array(STOCK_V16_SIZE)); } catch (e) { msg = e.message; }
  ok(/official M-VAVE FM-1 file \(FM-1\.fwsc, V15 or V16\)/.test(msg),
    "a file of the right size but the wrong digest is refused, and the message names both releases");

  let msg2 = "";
  try { await validateStockPackage(new Uint8Array(1000)); } catch (e) { msg2 = e.message; }
  ok(/V15 or V16/.test(msg2), "a file of an unknown size is refused");
}

// --- the layout checker, driven with a profile of our own ----------------------------------
{
  const rel = "release/felucca-v1.4.17.fwsc";
  if (!fs.existsSync(rel)) {
    console.log("skip the layout checker against a real package: " + rel + " is not in this tree");
  } else {
    const bytes = new Uint8Array(fs.readFileSync(rel));
    const pkg = new Package(bytes);
    /* the measured shape of this project's own package: it is what the loader can write */
    const profile = {
      identity: pkg.product, measured: false,
      layout: { flashFileOff: pkg.flashFileOff, flashSize: pkg.flashSize, appBase: pkg.appBase,
        appOff: pkg.appOff, appSize: pkg.app.size, otaSize: 7803 },
    };
    ok(stockLayoutIssue(pkg, profile) === null,
      "the layout checker accepts this project's package against its own measured values");
    const wrong = { ...profile, layout: { ...profile.layout, appSize: STOCK_APP_SIZE - 1 } };
    ok(/app slot/.test(stockLayoutIssue(pkg, wrong) || ""),
      "and names the app slot when it does not match");
    ok(loaderWindowIssue(pkg) === null,
      "this project's package's app area is inside the update loader's window");
    ok(hasFeluccaLoader(bytes), "and it carries this project's update loader marker");
    ok(!hasFeluccaLoader(new Uint8Array(64)), "a buffer without the marker is not claimed to carry it");
  }
}

// --- optionally: the official file itself --------------------------------------------------
const official = process.argv[2];
if (!official) {
  console.log("skip the official-file check: pass the path of FM-1.fwsc to check those bytes");
} else {
  const bytes = new Uint8Array(fs.readFileSync(official));
  const verified = await validateStockPackage(bytes);
  const profile = verified.profile;
  ok(verified.product === profile.identity && verified.sha256 === profile.sha256,
    `the file at ${official} is the pinned official ${profile.identity}`);
  ok(verified.image.length === profile.imageSize,
    `its logical image is ${profile.imageSize} B, as pinned`);
  ok(verified.layout.flashSize === profile.layout.flashSize,
    `its flash image is 0x${profile.layout.flashSize.toString(16)}, as measured`);
}
console.log("stock releases: " + n + " checks, 0 FAILED");
