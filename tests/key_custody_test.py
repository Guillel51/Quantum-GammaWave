#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Key custody, not only signing: a private key that cannot live in the repository, a pin that
cannot drift quietly, and every state in which the chain must refuse.

The key here is ephemeral and lives in a temporary directory: this test proves the procedure, it
does not create a release key. `tools/fm1_keys.json` ships pinning exactly the release key, and this
test never adds to it: the pin exercises below run on a copy whose `keys` object starts empty, and the
repository section checks that the shipped pin trusts that one key and refuses another under its id.

  python3 tests/key_custody_test.py
"""
import json
import os
import stat
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_sign as S  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<70} {'ok' if cond else 'FAIL'}")
    failed += not cond


def refuses(code, fn, what):
    try:
        fn()
    except S.SignatureError as e:
        ok(e.code == code, "%s -> %s" % (what, e.code))
        return
    ok(False, "%s -> accepted" % what)


def main():
    TMP = Path(tempfile.mkdtemp(prefix="qg1-custody-"))

    # -- a private key can never be generated inside the checkout -------------------------------
    inside = ROOT / "tools" / "custody-leak.pem"
    refuses("custody", lambda: S.keygen(inside, "leak-1"), "keygen in tools/")
    ok(not inside.exists(), "keygen in tools/: no file written")
    refuses("custody", lambda: S.keygen(ROOT / ".." / ROOT.name / "up.pem", "leak-1"),
            "keygen through .. into the repository")
    link = TMP / "repo-link"
    os.symlink(ROOT / "tools", link)
    refuses("custody", lambda: S.keygen(link / "x.pem", "leak-1"),
            "keygen through a symlink into the repository")
    refuses("keyid", lambda: S.keygen(TMP / "k.pem", "../qg1"), "keygen with a key id that is a path")
    refuses("path", lambda: S.keygen(TMP / "nope" / "k.pem", "qg1-release-1"),
            "keygen into a directory that does not exist")

    # -- generation outside: 0600 from the first byte, 32-byte public key, never overwritten ----
    priv = TMP / "qg1-release-1.pem"
    pub1 = S.keygen(priv, "qg1-release-1")
    ok(len(bytes.fromhex(pub1)) == 32, "keygen outside the checkout: a 32-byte Ed25519 key")
    ok(stat.S_IMODE(priv.stat().st_mode) == 0o600, "keygen: the private key is 0600 from the first byte")
    ok(priv.read_bytes().startswith(b"-----BEGIN PRIVATE KEY-----"), "keygen: PKCS#8 PEM")
    refuses("exists", lambda: S.keygen(priv, "qg1-release-1"), "keygen never overwrites a key")

    from cryptography.hazmat.primitives import serialization
    key = serialization.load_pem_private_key(priv.read_bytes(), password=None)
    raw_pub = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    hex_pub = TMP / "release.pub"
    hex_pub.write_text(pub1 + "\n")
    bin_pub = TMP / "release.raw"
    bin_pub.write_bytes(raw_pub)
    pem_pub = TMP / "release.pub.pem"
    pem_pub.write_bytes(key.public_key().public_bytes(serialization.Encoding.PEM,
                                                      serialization.PublicFormat.SubjectPublicKeyInfo))

    # -- pin: three encodings of one key, every malformed encoding refused ----------------------
    keys = TMP / "keys.json"
    # the real file's comment, with its keys object emptied: the key it ships is what the repository
    # section checks, and pinning it again here would prove idempotence and nothing else
    real_keys = json.loads((ROOT / "tools" / "fm1_keys.json").read_text())
    keys.write_text(json.dumps({"comment": real_keys.get("comment", ""), "keys": {}}))
    ok(S.pin(bin_pub, "qg1-release-1", keys) == pub1, "pin: 32 raw bytes")
    ok(S.pin(pem_pub, "qg1-release-1", keys) == pub1, "pin: the same key as PEM is idempotent")
    ok(S.pin(hex_pub, "qg1-release-1", keys) == pub1, "pin: the same key as hex is idempotent")
    data = json.loads(keys.read_text())
    ok(bool(data.get("comment")) and data["keys"] == {"qg1-release-1": pub1},
       "pin: the comment survives and the entry is 64 lowercase hex")
    refuses("keyid", lambda: S.pin(hex_pub, "bad id", keys), "pin: a key id with a space")
    refuses("key", lambda: S.pin(TMP / "missing.pub", "qg1-x", keys), "pin: a public key file that is not there")

    for name, blob, why in (
            ("short.raw", raw_pub[:31], "31 raw bytes"),
            ("long.raw", raw_pub + b"\0", "33 raw bytes"),
            ("ec.bin", b"\x04" + raw_pub + b"\0" * 32, "a 65-byte uncompressed EC point"),
            ("text.txt", b"zz" * 32, "64 characters that are not hex"),
            ("empty.bin", b"", "an empty file")):
        p = TMP / name
        p.write_bytes(blob)
        refuses("key", lambda p=p: S.pin(p, "qg1-x", keys), "pin: %s" % why)

    # moving an id to a different key is a trust decision, and the file is not rewritten blindly
    priv2 = TMP / "qg1-release-2.pem"
    pub2 = S.keygen(priv2, "qg1-release-2")
    hex2 = TMP / "release2.pub"
    hex2.write_text(pub2)
    rot = TMP / "rot.json"
    rot.write_text(keys.read_text())
    refuses("key", lambda: S.pin(hex2, "qg1-release-1", rot), "pin: another key under a pinned id")
    ok(S.pin(hex2, "qg1-release-1", rot, replace=True) == pub2, "pin --replace: an explicit trust decision")
    ok(S.pinned(rot)["qg1-release-1"] == bytes.fromhex(pub2), "pin --replace: the new key is the pinned one")

    broken = TMP / "broken.json"
    broken.write_text("{not json")
    refuses("keys", lambda: S.pin(hex_pub, "qg1-release-1", broken), "pin: a corrupt pin file")
    ok(broken.read_text() == "{not json", "pin: a corrupt pin file is left untouched")

    # -- check and pinned(): an empty set is a state, a malformed one refuses everything --------
    empty = TMP / "empty.json"
    empty.write_text(json.dumps({"comment": "none", "keys": {}}))
    ok(S.check(empty) == {}, "check: an empty pin set is reported as fail-closed")
    refuses("keys", lambda: S.check(empty, require_key=True), "check --require-key: no key is an error")
    ok(S.pinned(empty) == {}, "pinned(): the empty file trusts nothing")
    refuses("keys", lambda: S.check(TMP / "missing.json"), "check: a pin file that does not exist")
    refuses("keys", lambda: S.pinned(broken), "pinned(): corrupt JSON refuses every package")
    bad_entry = TMP / "bad-entry.json"
    bad_entry.write_text(json.dumps({"keys": {"qg1-x": "1234"}}))
    refuses("keys", lambda: S.pinned(bad_entry), "pinned(): an entry that is not a key refuses everything")
    ok(sorted(S.check(keys)) == ["qg1-release-1"], "check: the pinned key is listed")

    # -- the repository's own pin set: the release key, and it refuses another key under that id --
    repo_pins = ROOT / "tools" / "fm1_keys.json"
    ok(sorted(S.pinned(repo_pins)) == ["qg1-release-1"],
       "the repository pins the release key and nothing else")
    ok(len(S.pinned(repo_pins)["qg1-release-1"]) == 32,
       "the pinned release key is a 32-byte Ed25519 public key")
    pkg = TMP / "felucca.fwsc"
    pkg.write_bytes(bytes(range(256)) * 40)
    raw = pkg.read_bytes()
    man = S.sign_manifest(S.manifest_for(raw, "1.9.0", "1.8.0", "FM-1", "qg1-release-1"), priv.read_bytes())
    refuses("signature", lambda: S.verify(raw, man, S.pinned(repo_pins)),
            "the repository's pin set refuses another key under the release id")
    stranger = S.sign_manifest(S.manifest_for(raw, "1.9.0", "1.8.0", "FM-1", "qg1-not-pinned"),
                               priv.read_bytes())
    refuses("key", lambda: S.verify(raw, stranger, S.pinned(repo_pins)),
            "the repository's pin set refuses a key id it does not pin")
    ok(S.verify(raw, man, S.pinned(keys))["keyid"] == "qg1-release-1",
       "the same package verifies once the key is pinned")

    # -- rotation: the next id next to the old one, old packages still accepted ----------------
    ok(S.pin(hex2, "qg1-release-2", keys) == pub2, "rotation: the next key is pinned alongside")
    ok(set(S.pinned(keys)) == {"qg1-release-1", "qg1-release-2"}, "rotation: two ids trusted at once")
    ok(S.verify(raw, man, S.pinned(keys))["version"] == "1.9.0", "rotation: the old package still verifies")

    # -- revocation: deleting the id stops the package; no online list exists ------------------
    data = json.loads(keys.read_text())
    del data["keys"]["qg1-release-1"]
    keys.write_text(json.dumps(data))
    refuses("key", lambda: S.verify(raw, man, S.pinned(keys)), "revocation: the removed key is refused")

    # -- sign: the key id must be the one the private key belongs to, and the key out of tree ---
    keys.write_text(json.dumps({"keys": {"qg1-release-1": pub1}}))
    ok(S.main(["sign", str(pkg), "--key", str(priv2), "--version", "1.9.0", "--min", "1.8.0",
               "--keyid", "qg1-release-1", "--keys", str(keys)]) == 1,
       "sign: a private key that is not the pinned one")
    ok(S.main(["sign", str(pkg), "--key", str(ROOT / "tools" / "fm1_sign.py"), "--version", "1.9.0",
               "--min", "1.8.0", "--keyid", "qg1-release-1"]) == 1, "sign: a key path inside the repository")

    # -- the CLI agrees: sign then verify, and check reports the state -------------------------
    signed = TMP / "signed"
    signed.mkdir()
    p = signed / "p.fwsc"
    p.write_bytes(raw)
    # the build record the release must name, and the sign/verify pair that carries it
    record = signed / "manifest.json"
    record.write_text(json.dumps({"schema": "qg1-build-1", "version": "1.9.0", "commit": "0" * 40,
                                  "dirty": [], "identity": "FM-1_982",
                                  "artifacts": {"felucca.fwsc": {"sha256": S.__dict__["hashlib"].sha256(raw).hexdigest(),
                                                                 "bytes": len(raw)}}}))
    ok(S.main(["sign", str(p), "--key", str(priv), "--version", "1.9.0", "--min", "1.8.0",
               "--keyid", "qg1-release-1", "--keys", str(keys), "--build-manifest", str(record)]) == 0,
       "CLI sign: the pinned key and its build record -> 0")
    ok(S.main(["verify", str(p), "--manifest", str(p) + ".manifest.json", "--keys", str(keys),
               "--build-manifest", str(record)]) == 0,
       "CLI verify: against the pinned key -> 0")
    other_record = signed / "other.json"
    other_record.write_text(json.dumps({"schema": "qg1-build-1", "commit": "1" * 40}))
    ok(S.main(["verify", str(p), "--manifest", str(p) + ".manifest.json", "--keys", str(keys),
               "--build-manifest", str(other_record)]) == 1,
       "CLI verify: against another build record -> 1")
    ok(S.main(["verify", str(p), "--manifest", str(p) + ".manifest.json", "--keys", str(keys)]) == 0,
       "CLI verify: the record it names is enough (no record given) -> 0")
    ok(S.main(["verify", str(p), "--manifest", str(p) + ".manifest.json"]) == 1,
       "CLI verify: against the repository's pins, which hold another key -> 1")
    ok(S.main(["check", "--keys", str(empty)]) == 0, "CLI check: empty pin set -> 0")
    ok(S.main(["check", "--keys", str(empty), "--require-key"]) == 1, "CLI check --require-key -> 1")
    ok(S.main(["check", "--keys", str(keys)]) == 0, "CLI check: one pinned key -> 0")
    ok(S.main(["keygen", "--out", str(ROOT / "tools" / "x.pem"), "--keyid", "x-1"]) == 1,
       "CLI keygen inside the repository -> 1")
    ok(S.main(["pin", str(hex2), "--keyid", "qg1-release-1", "--keys", str(keys)]) == 1,
       "CLI pin over a pinned id without --replace -> 1")
    ok(S.main(["pin", str(hex2), "--keyid", "qg1-release-1", "--keys", str(keys), "--replace"]) == 0,
       "CLI pin --replace -> 0")

    ok(sorted(S.pinned(repo_pins)) == ["qg1-release-1"],
       "the repository still pins only the release key after the test")
    print("key custody test %s (%d failed)" % ("passed" if not failed else "FAILED", failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
