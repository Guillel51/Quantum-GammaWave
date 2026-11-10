// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Would M-Flasher install this package, and exactly which sectors will it write?
//
// The first install on a stock FM-1 is M-Flasher's `Install firmware` (docs/M-FLASHER.md §6), and its
// own conditions are what the package has to meet. They are quoted here from M-Flasher 0.9.4, tag
// `v0.9.4` (MIT, (c) jvitkauskas, https://github.com/jvitkauskas/m-flasher):
//
//   src/formats.rs  Package::parse      identity weave, header CRC16, file-list CRC16, flash.bin type 0
//                                       and CRC16, image longer than the boot area and a whole number
//                                       of 4 KiB sectors
//   src/formats.rs  app_files_status    every attr-0x82 file decrypts with the chip key and its CRC16
//                                       matches (on the unit's key: that is the chip-key refusal)
//   src/ops.rs      plan_firmware       model in the board's models, image <= 1 MiB, the write is
//                                       [0x4000, len) and no changed sector may fall outside it
//   src/boards.rs   FM1                 models ["FM-1", "MiniFM"], factory 0xFF000..0x100000
//
// The values below are computed here, in the page, from the package's bytes and -- when the owner
// loads it -- their own dump. Nothing is sent to the unit, and no step of this is M-Flasher: it is the
// check M-Flasher will make, answered before the cable is plugged in.
//
// tools/fm1_mflasher_accept.py is the same trace on the host, with the same refusal wording; the
// browser one is here so reading it needs no Python at all.

import { Package } from "./fm1pkg.js";

export const ACCEPT_BOOT_AREA = 0x4000;      // formats.rs BOOT_AREA
export const ACCEPT_SECTOR = 0x1000;         // formats.rs SECTOR
export const ACCEPT_FLASH_MAX = 0x100000;    // boards.rs FLASH_SIZE
export const ACCEPT_FACTORY = 0xFF000;       // boards.rs FM1.factory
export const ACCEPT_MODELS = ["FM-1", "MiniFM"];
export const ACCEPT_CHIP_KEY = 0x980F;       // boards.rs CHIP_KEY, and what an FM-1 record reports

/* its own name: the inlined modules share one scope (make_site refuses a duplicate declaration) */
const accU16 = (b, o) => b[o] | (b[o + 1] << 8);

/* M-Flasher's refusals, in its order. Each is { key, value }: the page translates key and prints the
 * value, so a refusal reads the way the tool reads on the owner's screen. */
export function acceptance(bytes, chipKey = ACCEPT_CHIP_KEY) {
  const checks = [];
  let pkg = null;
  try {
    pkg = new Package(bytes);
  } catch (e) {
    return { ok: false, checks, pkg: null, refusal: { key: "refuseContainer", value: String((e && e.message) || e) } };
  }
  const product = pkg.product;
  const model = product.split("_")[0];
  const image = pkg.flashSize;
  checks.push({ key: "accContainer", value: product, ok: true });
  checks.push({ key: "accImage", value: `${image} B`, ok: true });
  checks.push({ key: "accHeaderCrc", value: "0x" + accU16(pkg.ufw, 0).toString(16).toUpperCase().padStart(4, "0"), ok: true });
  checks.push({ key: "accListCrc", value: `${pkg.entries.length}`, ok: true });
  checks.push({ key: "accFlashCrc", value: "0x" + pkg.flashEntry.dcrc.toString(16).toUpperCase().padStart(4, "0"), ok: true });

  if (!ACCEPT_MODELS.includes(model))
    return { ok: false, checks, pkg, refusal: { key: "refuseModel", value: model } };
  checks.push({ key: "accModel", value: model, ok: true });

  if (image > ACCEPT_FLASH_MAX)
    return { ok: false, checks, pkg, refusal: { key: "refuseSize", value: `${image} B` } };
  checks.push({ key: "accSize", value: `${image} B <= ${ACCEPT_FLASH_MAX} B`, ok: true });

  const app = pkg.appFiles();
  const bad = app.filter((f) => !f.ok);
  for (const f of app) checks.push({ key: "accAppFile", value: `${f.name} ${f.size} B 0x${f.crc.toString(16).toUpperCase()}`, ok: f.ok });
  if (!app.length)
    return { ok: false, checks, pkg, refusal: { key: "refuseKey", value: "0x" + chipKey.toString(16).toUpperCase() } };
  if (bad.length)
    return { ok: false, checks, pkg, refusal: { key: "refuseKey", value: "0x" + chipKey.toString(16).toUpperCase() } };
  checks.push({ key: "accKey", value: "0x" + chipKey.toString(16).toUpperCase(), ok: true });

  const end = image;
  checks.push({ key: "accRange", value: `0x${ACCEPT_BOOT_AREA.toString(16).toUpperCase().padStart(5, "0")}-0x${(end - 1).toString(16).toUpperCase().padStart(5, "0")}`, ok: true });
  checks.push({ key: "accSectors", value: String((end - ACCEPT_BOOT_AREA) / ACCEPT_SECTOR), ok: true });
  checks.push({ key: "accBoot", value: "0x00000-0x03FFF", ok: true });
  checks.push({ key: "accFactory", value: `0x${ACCEPT_FACTORY.toString(16).toUpperCase()}`, ok: end <= ACCEPT_FACTORY });
  if (end > ACCEPT_FACTORY)
    return { ok: false, checks, pkg, refusal: { key: "refuseFactory", value: `0x${(end - 1).toString(16).toUpperCase()}` } };
  return { ok: true, checks, pkg, refusal: null, product, model, image };
}

const runsOf = (sectors) => {
  const out = [];
  for (const a of sectors) {
    const last = out[out.length - 1];
    if (last && last[1] === a) last[1] = a + ACCEPT_SECTOR;
    else out.push([a, a + ACCEPT_SECTOR]);
  }
  return out;
};

/* The plan M-Flasher builds: target = the unit's image with [0x4000, len) taken from the package.
 * Without a dump only the bound is known (every sector of the range may differ); with one, the exact
 * sectors it will erase and write -- and nothing outside that range can change, by construction. */
export function sectorPlan(pkg, dump) {
  const len = pkg.flashSize;
  const range = [ACCEPT_BOOT_AREA, len];
  const bound = (len - ACCEPT_BOOT_AREA) / ACCEPT_SECTOR;
  if (!dump) return { range, bound, exact: false, sectors: [], runs: [], unchanged: null };
  if (dump.length !== ACCEPT_FLASH_MAX) return { range, bound, exact: false, sectors: [], runs: [], unchanged: null, badDump: dump.length };
  const sectors = [];
  let unchanged = 0;
  for (let a = ACCEPT_BOOT_AREA; a < len; a += ACCEPT_SECTOR) {
    const d = dump.subarray(a, a + ACCEPT_SECTOR), p = pkg.flash.subarray(a, a + ACCEPT_SECTOR);
    let diff = false;
    for (let i = 0; i < ACCEPT_SECTOR; i++) if (d[i] !== p[i]) { diff = true; break; }
    if (diff) sectors.push(a); else unchanged++;
  }
  return { range, bound, exact: true, sectors, runs: runsOf(sectors), unchanged };
}

export const hexRange = (r) =>
  `0x${r[0].toString(16).toUpperCase().padStart(5, "0")}-0x${(r[1] - 1).toString(16).toUpperCase().padStart(5, "0")}`;
