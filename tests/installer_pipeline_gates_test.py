#!/usr/bin/env python3
"""
tests/installer_pipeline_gates_test.py: Comprehensive validation of installer gate
flow, verify-only isolation, and conditional authorization logic.
"""
import unittest

class FakeDeviceLink:
    def __init__(self, model="FM-1", version=15, jedec=0x856014):
        self.model = model
        self.version = version
        self.jedec = jedec
        self.writes_attempted = 0
        self.erases_attempted = 0

    def query_identity(self):
        return f"{self.model}_{self.version:03d}"

    def query_jedec(self):
        return self.jedec

    def sector_erase(self, addr):
        self.erases_attempted += 1

    def page_program(self, addr, data):
        self.writes_attempted += 1

class InstallerEngine:
    CERTIFIED_WRITABLE_JEDEC = {0x856014, 0xB36014}
    READONLY_JEDEC = {0xC84014, 0xEF4014, 0x5E4014, 0x684014, 0x204014}
    SUPPORTED_MODELS = {"FM-1"}
    SUPPORTED_DONOR_VERSIONS = {15, 16}

    def __init__(self, link, package_hash, signature_valid=True, backup_verified=False, physical_recovery_tested=False):
        self.link = link
        self.package_hash = package_hash
        self.signature_valid = signature_valid
        self.backup_verified = backup_verified
        self.physical_recovery_tested = physical_recovery_tested
        self.gates = {}

    def run_verify_only(self):
        """Strictly read-only diagnostic pass."""
        id_str = self.link.query_identity()
        parts = id_str.split("_")
        model = parts[0]
        version = int(parts[1])

        self.gates["DEVICE_DETECTED"] = (model in self.SUPPORTED_MODELS)
        self.gates["DONOR_COMPATIBLE"] = (version in self.SUPPORTED_DONOR_VERSIONS)
        self.gates["PACKAGE_VERIFIED"] = (self.package_hash is not None and len(self.package_hash) == 64)
        self.gates["SIGNATURE_VERIFIED"] = self.signature_valid
        self.gates["BACKUP_VALIDATED"] = self.backup_verified
        self.gates["PHYSICAL_RECOVERY_TESTED"] = self.physical_recovery_tested

        jedec = self.link.query_jedec()
        self.gates["JEDEC_IDENTIFIED"] = (jedec in self.CERTIFIED_WRITABLE_JEDEC or jedec in self.READONLY_JEDEC)
        self.gates["CHIP_WRITABLE"] = (jedec in self.CERTIFIED_WRITABLE_JEDEC)

        # Invariant: Verify Only must perform 0 erases and 0 writes
        assert self.link.erases_attempted == 0
        assert self.link.writes_attempted == 0

        return self.gates

    def can_install(self):
        """Determine if software authorizes flash write."""
        g = self.run_verify_only()
        required_gates = [
            "DEVICE_DETECTED",
            "DONOR_COMPATIBLE",
            "PACKAGE_VERIFIED",
            "SIGNATURE_VERIFIED",
            "BACKUP_VALIDATED",
            "JEDEC_IDENTIFIED",
            "CHIP_WRITABLE",
            "PHYSICAL_RECOVERY_TESTED"
        ]
        return all(g.get(k, False) for k in required_gates)

class TestInstallerPipelineGates(unittest.TestCase):
    def test_verify_only_never_writes(self):
        link = FakeDeviceLink()
        engine = InstallerEngine(link, "80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2")
        gates = engine.run_verify_only()
        self.assertTrue(gates["DEVICE_DETECTED"])
        self.assertEqual(link.writes_attempted, 0)
        self.assertEqual(link.erases_attempted, 0)

    def test_installation_blocked_when_physical_recovery_untested(self):
        """Installation MUST be blocked (NO-GO) if physical recovery is NOT TESTED."""
        link = FakeDeviceLink(jedec=0x856014) # Puya certified
        engine = InstallerEngine(link, "80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2",
                               signature_valid=True, backup_verified=True, physical_recovery_tested=False)
        self.assertFalse(engine.can_install(), "Install must remain BLOCKED without physical recovery testing")

    def test_installation_blocked_on_readonly_chip(self):
        """Winbond W25Q80 is recognized for backup/read, but BLOCKED for write."""
        link = FakeDeviceLink(jedec=0xEF4014) # Winbond read-only
        engine = InstallerEngine(link, "80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2",
                               signature_valid=True, backup_verified=True, physical_recovery_tested=True)
        self.assertFalse(engine.can_install(), "Install must be BLOCKED on non-certified writable chips")

    def test_installation_blocked_without_backup(self):
        """Installation MUST be blocked if backup is missing or invalid."""
        link = FakeDeviceLink(jedec=0x856014)
        engine = InstallerEngine(link, "80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2",
                               signature_valid=True, backup_verified=False, physical_recovery_tested=True)
        self.assertFalse(engine.can_install(), "Install must be BLOCKED without valid backup")

if __name__ == '__main__':
    unittest.main()
