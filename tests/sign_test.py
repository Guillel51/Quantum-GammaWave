#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Ed25519 chain of authenticity: sign, verify, and every way it must refuse.

  python3 tests/sign_test.py
"""
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_sign as S  # noqa: E402

failed = 0


def man_sha(raw):
    """the package digest a build record carries (the fixture's own, without importing hashlib twice)"""
    import hashlib
    return hashlib.sha256(raw).hexdigest()


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


def refuses(code, fn, what):
    try:
        fn()
    except S.SignatureError as e:
        ok(e.code == code, "%s -> %s" % (what, e.code))
        return
    ok(False, "%s -> accepted" % what)


def main():
    TMP = Path(tempfile.mkdtemp(prefix="qg1-sign-"))
    pkg = TMP / "felucca.fwsc"
    pkg.write_bytes(bytes(range(256)) * 40)
    raw = pkg.read_bytes()

    priv = TMP / "key.pem"
    S.keygen(priv, "test-1")
    pub = TMP / "key.pub"
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    key = serialization.load_pem_private_key(priv.read_bytes(), password=None)
    pub.write_text(key.public_key().public_bytes(serialization.Encoding.Raw,
                                                 serialization.PublicFormat.Raw).hex())
    keys = TMP / "keys.json"
    S.pin(pub, "test-1", keys)
    pin = S.pinned(keys)
    ok(list(pin) == ["test-1"] and len(pin["test-1"]) == 32, "keygen + pin: one 32-byte Ed25519 key")

    # the build record the signature will name: what tools/build_manifest.py writes
    rec_path = TMP / "manifest.json"
    rec_path.write_text(json.dumps({"schema": "qg1-build-1", "version": "3.0.0",
                                    "commit": "0" * 40, "dirty": [], "identity": "FM-1_965",
                                    "artifacts": {"felucca.fwsc": {"sha256": man_sha(raw), "bytes": len(raw)}}}))
    rec = S.record_sha256(rec_path)
    man = S.manifest_for(raw, "3.0.0", "2.9.0", "FM-1", "test-1", build_sha256=rec)
    signed = S.sign_manifest(man, priv.read_bytes())
    info = S.verify(raw, signed, pin)
    ok(info["version"] == "3.0.0" and info["keyid"] == "test-1", "signed manifest verifies")
    ok(S.verify(raw, signed, pin, "FM-1", firmware="2.9.0")["sha256"] == man["sha256"], "firmware == min is allowed")

    # the signature covers the build record: without it a swapped record would look clean
    ok(signed.get("build_sha256") == rec and len(rec) == 64, "the signed manifest names the build record")
    ok(S.verify(raw, signed, pin, build_sha256=rec, require_build=True)["version"] == "3.0.0",
       "verify: the record it names is accepted")
    refuses("record", lambda: S.verify(raw, signed, pin, build_sha256="1" * 64, require_build=True),
            "another build record")
    refuses("signature", lambda: S.verify(raw, {k: v for k, v in signed.items() if k != "build_sha256"}, pin),
            "a record id removed after signing (the signature covers it)")
    no_record = S.sign_manifest(S.manifest_for(raw, "3.0.0", "2.9.0", "FM-1", "test-1"), priv.read_bytes())
    refuses("record", lambda: S.verify(raw, no_record, pin, require_build=True),
            "a signed manifest that names no record")
    bad_digest = S.sign_manifest(S.manifest_for(raw, "3.0.0", "2.9.0", "FM-1", "test-1", build_sha256="nope"),
                                 priv.read_bytes())
    refuses("record", lambda: S.verify(raw, bad_digest, pin, require_build=True),
            "a build_sha256 that is not a digest")
    edited_record = dict(signed, build_sha256="1" * 64)
    refuses("signature", lambda: S.verify(raw, edited_record, pin, require_build=True),
            "a record id edited after signing")

    refuses("hash", lambda: S.verify(raw + b"\0", signed, pin), "package changed")
    refuses("size", lambda: S.verify(raw[:-1], {**signed, "sha256": S.__dict__["hashlib"].sha256(raw[:-1]).hexdigest()},
                                     pin), "size not the manifest's")
    tampered = dict(signed, version="9.9.9")
    refuses("signature", lambda: S.verify(raw, tampered, pin), "manifest edited")
    refuses("key", lambda: S.verify(raw, signed, {}), "no key pinned")
    refuses("key", lambda: S.verify(raw, dict(signed, keyid="other"), pin), "unknown key id")
    refuses("product", lambda: S.verify(raw, signed, pin, "GW-1"), "package for another product")
    refuses("downgrade", lambda: S.verify(raw, signed, pin, "FM-1", firmware="2.8.0"), "firmware below min")
    refuses("manifest", lambda: S.verify(raw, {k: v for k, v in signed.items() if k != "sig"}, pin), "no signature")
    refuses("signature", lambda: S.verify(raw, dict(signed, sig="zz"), pin), "signature not hex")
    refuses("version", lambda: S.verify(raw, S.sign_manifest(S.manifest_for(raw, "2.0.0", "3.0.0", "FM-1", "test-1", build_sha256=rec),
                                                             priv.read_bytes()), pin), "min newer than version")

    # (1.9) rotation and revocation, as operations rather than as edits to a JSON file: a second key
    # is pinned beside the first, a package signed by either verifies, revoking one id takes only that
    # id's packages out, and a revocation that would leave nothing trusted has to be asked for.
    priv_b = TMP / "key-b.pem"
    S.keygen(priv_b, "test-2")
    key_b = serialization.load_pem_private_key(priv_b.read_bytes(), password=None)
    pub_b = TMP / "key-b.pub"
    pub_b.write_text(key_b.public_key().public_bytes(serialization.Encoding.Raw,
                                                     serialization.PublicFormat.Raw).hex())
    S.pin(pub_b, "test-2", keys)
    both = S.pinned(keys)
    ok(set(both) == {"test-1", "test-2"}, "rotation: two keys pinned, the ring holds both")
    signed_b = S.sign_manifest(S.manifest_for(raw, "3.0.0", "2.9.0", "FM-1", "test-2", build_sha256=rec),
                               priv_b.read_bytes())
    ok(S.verify(raw, signed_b, both)["keyid"] == "test-2", "rotation: a package signed by the new key verifies")
    ok(S.verify(raw, signed, both)["keyid"] == "test-1", "rotation: the old key's package still verifies")
    refuses("key", lambda: S.pin(pub_b, "test-1", keys), "pin: an id moved to another key without --replace")
    ok(S.unpin("test-1", keys) == ["test-2"], "revocation: unpin removes the id and names what is left")
    after = S.pinned(keys)
    ok(set(after) == {"test-2"}, "revocation: the revoked id is gone from the ring")
    refuses("key", lambda: S.verify(raw, signed, after), "revocation: a package signed by the revoked key")
    ok(S.verify(raw, signed_b, after)["keyid"] == "test-2", "revocation: the other key is untouched")
    refuses("key", lambda: S.unpin("missing", keys), "unpin: an id that is not pinned (a typo is not a revocation)")
    refuses("keys", lambda: S.unpin("test-2", keys), "unpin: the last key needs --allow-empty")
    ok(S.unpin("test-2", keys, allow_empty=True) == [], "unpin: --allow-empty reaches the fail-closed state")
    refuses("key", lambda: S.verify(raw, signed_b, S.pinned(keys)), "an empty ring verifies nothing")
    S.pin(pub, "test-1", keys)                                  # put the ring back for the CLI checks below
    ok(set(S.pinned(keys)) == {"test-1"}, "re-pinning a revoked key is an explicit pin")

    man_path = TMP / "pkg.manifest.json"
    man_path.write_text(json.dumps(signed))
    ok(S.main(["verify", str(pkg), "--manifest", str(man_path), "--keys", str(keys),
               "--build-manifest", str(rec_path)]) == 0, "CLI verify: 0 (with the record it names)")
    ok(S.main(["verify", str(pkg), "--manifest", str(man_path), "--keys", str(keys)]) == 0,
       "CLI verify: 0 (record named, none given)")
    man_path.write_text(json.dumps(dict(signed, version="9.9.9")))
    ok(S.main(["verify", str(pkg), "--manifest", str(man_path), "--keys", str(keys)]) == 1, "CLI verify: edited manifest -> 1")
    print("sign test %s (%d failed)" % ("passed" if not failed else "FAILED", failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
