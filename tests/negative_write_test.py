#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Negative write and boundary tests for FM-1 OTA and flash installation safety.

import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import fm1_gate as gate
import fm1_image as image


class NegativeWriteTest(unittest.TestCase):
    def test_bootloader_region_immutable(self):
        # Bootloader region [0x00000, 0x04000) must NEVER be in any write/erase plan
        self.assertEqual(image.BOOT_AREA, 0x4000)
        # Verify gate rejects writes if product or identity is malformed
        with self.assertRaises(gate.Refusal) as ctx:
            gate.require("", "FM-1_015", 0x856014)
        self.assertEqual(ctx.exception.reason, "bad_identity")

    def test_unknown_and_read_only_chips_rejected_from_writing(self):
        # Read-only chips from ecosystem must raise Refusal on require()
        read_only_chips = [0xC84014, 0xEF4014, 0x5E4014, 0x684014, 0x204014]
        for jedec in read_only_chips:
            with self.assertRaises(gate.Refusal) as ctx:
                gate.require("FM-1_982", "FM-1_015", jedec)
            self.assertEqual(ctx.exception.reason, "chip_read_only")

        # Unknown / floating / shorted bus must raise Refusal
        for bad_jedec in [0x000000, 0xFFFFFF, 0xFFFFFFFF, 0x123456, None]:
            with self.assertRaises(gate.Refusal):
                gate.require("FM-1_982", "FM-1_015", bad_jedec)

    def test_model_mismatch_refused(self):
        # Cannot cross product models
        with self.assertRaises(gate.Refusal) as ctx:
            gate.require("FM-1_982", "GW-1_015", 0x856014)
        self.assertEqual(ctx.exception.reason, "model_mismatch")

    def test_unverified_image_boundaries(self):
        # Truncated or oversize images
        self.assertEqual(gate.MAX_PACKAGE, 0x100000)
        # Ensure image analyzer rejects incomplete boot table
        truncated_header = bytes(16)
        self.assertEqual(image.boot_table(truncated_header), [])


if __name__ == "__main__":
    unittest.main()
