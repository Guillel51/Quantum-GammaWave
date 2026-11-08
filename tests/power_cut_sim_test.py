#!/usr/bin/env python3
"""
tests/power_cut_sim_test.py: Simulation of power cuts, mid-transfer disconnections,
incomplete writes, and corrupt transmission frames during OTA updates.
"""
import unittest

class FakeOtaTransport:
    def __init__(self, fail_at_frame=None, drop_ack=False, corrupt_at_frame=None):
        self.fail_at_frame = fail_at_frame
        self.drop_ack = drop_ack
        self.corrupt_at_frame = corrupt_at_frame
        self.sent_frames = 0
        self.aborted = False
        self.error_code = None

    def send_frame(self, frame_idx, data, crc):
        if self.aborted:
            raise RuntimeError("Link is already aborted")

        # Simulate power cut / cable disconnect
        if self.fail_at_frame is not None and frame_idx >= self.fail_at_frame:
            self.aborted = True
            self.error_code = "POWER_CUT_OR_DISCONNECT"
            raise ConnectionResetError("Device disconnected mid-transfer")

        # Simulate corrupt frame transmission
        if self.corrupt_at_frame is not None and frame_idx == self.corrupt_at_frame:
            crc ^= 0xA5
            # Device verifies frame CRC:
            if not self._device_verify_frame(data, crc):
                self.aborted = True
                self.error_code = "CRC_ERROR"
                return False

        # Simulate lost ACK from device
        if self.drop_ack and frame_idx == 5:
            self.aborted = True
            self.error_code = "ACK_TIMEOUT"
            return False

        self.sent_frames += 1
        return True

    def _device_verify_frame(self, data, crc):
        expected_crc = sum(data) & 0xFF
        return (crc & 0xFF) == expected_crc

class TestPowerCutAndFaultSimulations(unittest.TestCase):
    def test_power_cut_mid_transfer(self):
        """Simulate power loss at frame 10 out of 50. Ensure transmission aborts cleanly."""
        t = FakeOtaTransport(fail_at_frame=10)
        frames_written = 0
        with self.assertRaises(ConnectionResetError):
            for i in range(50):
                dummy_data = [i % 256] * 128
                crc = sum(dummy_data) & 0xFF
                if t.send_frame(i, dummy_data, crc):
                    frames_written += 1
        self.assertEqual(frames_written, 10)
        self.assertTrue(t.aborted)
        self.assertEqual(t.error_code, "POWER_CUT_OR_DISCONNECT")

    def test_corrupted_frame_handling(self):
        """Simulate bit flip during transmission. Ensure receiver rejects frame."""
        t = FakeOtaTransport(corrupt_at_frame=4)
        frames_written = 0
        for i in range(10):
            dummy_data = [i % 256] * 128
            crc = sum(dummy_data) & 0xFF
            try:
                if t.send_frame(i, dummy_data, crc):
                    frames_written += 1
                else:
                    break
            except Exception:
                break
        self.assertEqual(frames_written, 4)
        self.assertTrue(t.aborted)
        self.assertEqual(t.error_code, "CRC_ERROR")

    def test_lost_ack_timeout(self):
        """Simulate missing ACK response from target device. Transmission must halt."""
        t = FakeOtaTransport(drop_ack=True)
        frames_written = 0
        for i in range(10):
            dummy_data = [i % 256] * 128
            crc = sum(dummy_data) & 0xFF
            if t.send_frame(i, dummy_data, crc):
                frames_written += 1
            else:
                break
        self.assertEqual(frames_written, 5)
        self.assertTrue(t.aborted)
        self.assertEqual(t.error_code, "ACK_TIMEOUT")

if __name__ == '__main__':
    unittest.main()
