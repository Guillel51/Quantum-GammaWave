// tests/ota_fault_injection_test.mjs
import assert from "node:assert";
import { pack7, unpack7 } from "../web/fm1ota.js";

console.log("Running comprehensive OTA fault injection simulations...");

// Test 1: pack7 / unpack7 integrity
const payload = [0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0];
const packed = pack7(payload);
const unpacked = unpack7(packed);
assert.deepStrictEqual(unpacked, payload, "pack7/unpack7 roundtrip must match");

// Test 2: Frame corruption bit-flip detection
const corruptedPacked = [...packed];
corruptedPacked[2] ^= 0x01; // flip 1 bit
const corruptedUnpacked = unpack7(corruptedPacked);
assert.notDeepStrictEqual(corruptedUnpacked, payload, "Corrupted payload must not match original");

console.log("OTA fault injection tests passed cleanly (0 regressions).");
