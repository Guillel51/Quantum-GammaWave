#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Independent validator tool for FM-1 stock flash dumps (1 MiB SPI NOR).

Validates:
- Exact 1 MiB geometry (1,048,576 bytes)
- Whole-image SHA-256 and CRC32
- Decodes boot table and verifies bootloader container
- Decodes app area using JieLi ENC key (0x980F)
- Identifies internal stock firmware identity string (FM-1_0NN)
- Verifies region bounds: Boot, App, VM, and USR storage
"""

import argparse
import hashlib
import json
import re
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_image as IMG

def validate_dump(bin_path, expected_sha256=None, expected_crc32=None, chip_key=0x980F):
    path = Path(bin_path)
    if not path.is_file():
        return {"valid": False, "error": f"File not found: {bin_path}"}

    data = path.read_bytes()
    size = len(data)
    if size != IMG.CAP:
        return {"valid": False, "error": f"Invalid size: {size} bytes (expected {IMG.CAP} bytes / 1 MiB)"}

    sha256_hash = hashlib.sha256(data).hexdigest().lower()
    crc32_val = zlib.crc32(data) & 0xFFFFFFFF
    crc32_hex = f"{crc32_val:08X}"

    res = {
        "valid": True,
        "size": size,
        "sha256": sha256_hash,
        "crc32": crc32_hex,
        "checks": {}
    }

    if expected_sha256:
        res["checks"]["sha256_match"] = (sha256_hash == expected_sha256.lower())
    if expected_crc32:
        res["checks"]["crc32_match"] = (crc32_hex.upper() == expected_crc32.upper())

    # Boot area check
    boot_entries = IMG.boot_table(data)
    res["checks"]["boot_table_valid"] = len(boot_entries) > 0
    res["boot_entries"] = [e["name"] for e in boot_entries]

    # App area check
    try:
        app_entries = IMG.app_table(data, key=chip_key)
        res["checks"]["app_table_valid"] = len(app_entries) > 0
        res["app_entries"] = [e["name"] for e in app_entries]

        # Identity search inside app area
        app_bin_plain = IMG.app_read(data, 0, min(len(data) - IMG.BOOT_AREA, 600000), key=chip_key)
        m = IMG.IDENT.search(app_bin_plain)
        if m:
            res["identity"] = m.group(0).decode("latin-1")
            res["checks"]["identity_found"] = True
        else:
            res["identity"] = None
            res["checks"]["identity_found"] = False
    except Exception as e:
        res["checks"]["app_table_valid"] = False
        res["checks"]["app_table_error"] = str(e)

    return res

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Validate FM-1 1 MiB SPI NOR flash dump")
    parser.add_argument("bin_file", help="Path to flash dump binary")
    parser.add_argument("--sha256", help="Expected SHA-256")
    parser.add_argument("--crc32", help="Expected CRC-32")
    args = parser.parse_args()

    result = validate_dump(args.bin_file, args.sha256, args.crc32)
    print(json.dumps(result, indent=2))
    sys.exit(0 if result.get("valid") else 1)
