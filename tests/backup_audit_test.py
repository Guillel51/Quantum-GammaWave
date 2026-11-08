#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Tests for multi-chip SPI NOR definitions, backup bounds, and V15/V16 donor gating.

import sys
import unittest
from pathlib import Path

# Add tools to sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import fm1_gate as gate
import fm1_image as image


class BackupAuditTest(unittest.TestCase):
    def test_multichip_capabilities(self):
        # 1. Production chips are readable, backup-capable, and writable
        puya = gate.capabilities(0x856014)
        self.assertIsNotNone(puya)
        self.assertTrue(puya["read"])
        self.assertTrue(puya["backup"])
        self.assertTrue(puya["write_app"])
        self.assertTrue(puya["qg_install"])

        ucun = gate.capabilities(0xB36014)
        self.assertIsNotNone(ucun)
        self.assertTrue(ucun["read"])
        self.assertTrue(ucun["backup"])
        self.assertTrue(ucun["write_app"])
        self.assertTrue(ucun["qg_install"])

        # 2. Documented 1 MiB variants are readable & backup-capable, but strictly NOT write-authorised
        variants = [0xC84014, 0xEF4014, 0x5E4014, 0x684014, 0x204014]
        for jedec in variants:
            cap = gate.capabilities(jedec)
            self.assertIsNotNone(cap, f"Missing capabilities for 0x{jedec:06X}")
            self.assertTrue(cap["read"])
            self.assertTrue(cap["backup"])
            self.assertFalse(cap["write_app"], f"Chip 0x{jedec:06X} must NOT be writable")
            self.assertFalse(cap["qg_install"], f"Chip 0x{jedec:06X} must NOT allow QG install")

    def test_chip_authorisation_refusal(self):
        # Authorisation for writing should succeed ONLY on production chips
        auth_puya = gate.authorise("FM-1_982", "FM-1_015", 0x856014)
        self.assertTrue(auth_puya["allowed"])

        auth_ucun = gate.authorise("FM-1_982", "FM-1_015", 0xB36014)
        self.assertTrue(auth_ucun["allowed"])

        # Variants must be refused with chip_read_only
        auth_gd = gate.authorise("FM-1_982", "FM-1_015", 0xC84014)
        self.assertFalse(auth_gd["allowed"])
        self.assertEqual(auth_gd["reason"], "chip_read_only")

        # Unknown JEDEC must be refused with chip_unknown
        auth_unk = gate.authorise("FM-1_982", "FM-1_015", 0x123456)
        self.assertFalse(auth_unk["allowed"])
        self.assertEqual(auth_unk["reason"], "chip_unknown")

    def test_image_capacity_bounds(self):
        # Flash image must be exactly 1 MiB
        self.assertEqual(image.CAP, 0x100000)
        self.assertEqual(image.BOOT_AREA, 0x4000)

        # Truncated image raises ImageError
        short_buf = bytes(1024)
        with self.assertRaises(image.ImageError):
            image.app_read(short_buf, 0, 32)


if __name__ == "__main__":
    unittest.main()
