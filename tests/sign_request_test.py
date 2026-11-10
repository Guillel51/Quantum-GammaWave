#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Signing that does not need the key in the same process, machine or runner.

A release is frozen into a qg1-sign-request-1 document by the machine that built it (no key there),
signed where the key lives (`--key`, or `--signer-cmd` for an offline host, smartcard or HSM helper),
and the signature that comes back is verified against the pinned public key before anything is
written. This proves the three paths agree and that each way of getting it wrong is refused.

  python3 tests/sign_request_test.py
"""
import io
import json
import shutil
import sys
import tempfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_sign as S  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<74} {'ok' if cond else 'FAIL'}")
    failed += not cond


def run(argv, stdin=b""):
    """The CLI as a caller sees it: (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    old = sys.stdin
    try:
        sys.stdin = io.TextIOWrapper(io.BytesIO(stdin), encoding="utf-8")
        with redirect_stdout(out), redirect_stderr(err):
            rc = S.main(argv)
    finally:
        sys.stdin = old
    return rc, out.getvalue(), err.getvalue()


def main():
    TMP = Path(tempfile.mkdtemp(prefix="qg1-req-"))
    try:
        pkg = TMP / "felucca-9.9.9.fwsc"
        pkg.write_bytes(b"QG-1 package for the request test\n" * 40)
        record = TMP / "manifest.json"
        record.write_text(json.dumps({
            "schema": "qg1-build-1", "version": "9.9.9", "commit": "0" * 40,
            "artifacts": {
                "felucca.bin": {"sha256": "a" * 64},
                "loader/ota.bin": {"sha256": "b" * 64},
                "felucca-9.9.9.fwsc": {"sha256": "c" * 64},
            },
        }, indent=1) + "\n")

        keyid = "test-release-1"
        priv = TMP / "test-release-1.pem"
        pub = S.keygen(priv, keyid)
        keys = TMP / "keys.json"
        keys.write_text(json.dumps({"keys": {keyid: pub}}, indent=2) + "\n")

        # -- the request: everything the signature will cover, frozen without a key --------------------
        req = TMP / "sign-request.json"
        rc, out, err = run(["request", str(pkg), "--version", "9.9.9", "--min", "1.0.0",
                            "--keyid", keyid, "--build-manifest", str(record), "--out", str(req),
                            "--created", "2026-11-10T00:00:00Z"])
        ok(rc == 0 and req.exists(), "request: written from a package and its build record, no key involved")
        doc = json.loads(req.read_text())
        man = doc["manifest"]
        import hashlib
        ok(doc["format"] == S.REQUEST_FORMAT, "request: names its format (%s)" % S.REQUEST_FORMAT)
        ok(man["sha256"] == hashlib.sha256(pkg.read_bytes()).hexdigest(), "request: the package's SHA-256")
        ok(man["size"] == pkg.stat().st_size, "request: the package's size")
        ok(man["images"] == {"felucca.bin": "a" * 64, "loader/ota.bin": "b" * 64},
           "request: the firmware images, and not the package's own file name")
        ok(man["build"]["commit"] == "0" * 40 and man["build_sha256"] == S.record_sha256(record),
           "request: the build record travels inside it")
        ok(man["created"] == "2026-11-10T00:00:00Z", "request: the timestamp is pinned, not left to the signer")
        ok("sig" not in man, "request: unsigned, as it must be")
        ok(run(["request", str(pkg), "--version", "9.9.9", "--min", "1.0.0", "--keyid", keyid,
                "--out", str(TMP / "x.json")])[0] == 1,
           "request without a build record -> refused (the installer would refuse that release)")

        # -- signing a request with the key in this process -------------------------------------------
        withkey = TMP / "with-key.manifest.json"
        rc, out, err = run(["sign", "--request", str(req), "--key", str(priv), "--keys", str(keys),
                            "--out", str(withkey)])
        ok(rc == 0 and withkey.exists(), "sign --request --key: signs the frozen bytes")
        m1 = json.loads(withkey.read_text())
        ok(S.verify(pkg.read_bytes(), m1, S.pinned(keys))["keyid"] == keyid,
           "sign --request --key: the manifest verifies against the pin and the package")

        # -- signing the same request through an external signer ---------------------------------------
        cmd = "python3 %s sign-stdin --key %s" % (ROOT / "tools" / "fm1_sign.py", priv)
        external = TMP / "external.manifest.json"
        rc, out, err = run(["sign", "--request", str(req), "--signer-cmd", cmd, "--keys", str(keys),
                            "--out", str(external)])
        ok(rc == 0 and external.exists(), "sign --signer-cmd: a command holds the key, this process does not")
        m2 = json.loads(external.read_text())
        ok(S.verify(pkg.read_bytes(), m2, S.pinned(keys))["version"] == "9.9.9",
           "sign --signer-cmd: the manifest verifies against the pin")
        ok(m2["sig"] == m1["sig"], "the external signer signed the same bytes (identical Ed25519 signature)")
        ok(external.read_text() == withkey.read_text(),
           "both paths write the same manifest, byte for byte")

        # -- every way of getting it wrong is refused, with a code --------------------------------
        def refused(code, argv, what):
            rc, out, err = run(argv)
            ok(rc == 1 and code in err, "%s -> %s" % (what, code))

        refused("signer", ["sign", "--request", str(req), "--signer-cmd", "echo not-a-signature",
                           "--keys", str(keys), "--out", str(TMP / "n1.json")],
                "a signer that prints nonsense")
        refused("signer", ["sign", "--request", str(req), "--signer-cmd", "exit 3", "--keys", str(keys),
                           "--out", str(TMP / "n2.json")], "a signer that fails")
        other = TMP / "other.pem"
        S.keygen(other, "other-1")
        refused("signer", ["sign", "--request", str(req), "--keys", str(keys), "--out", str(TMP / "n3.json"),
                           "--signer-cmd", "python3 %s sign-stdin --key %s" % (ROOT / "tools" / "fm1_sign.py",
                                                                              other)],
                "a signer holding a key that is not the pinned one")
        refused("signer", ["sign", "--request", str(req), "--signer-cmd", cmd, "--out", str(TMP / "n4.json"),
                           "--keys", str(TMP / "empty.json")],
                "no pin and no --expect-pub: nothing could verify the signer")
        refused("exactly one of", ["sign", "--request", str(req), "--key", str(priv), "--signer-cmd", cmd,
                                   "--keys", str(keys), "--out", str(TMP / "n5.json")],
                "both --key and --signer-cmd")
        refused("exactly one of", ["sign", "--request", str(req), "--keys", str(keys),
                                   "--out", str(TMP / "n6.json")], "neither --key nor --signer-cmd")
        refused("version", ["sign", "--request", str(req), "--key", str(priv), "--keys", str(keys),
                            "--version", "9.9.8", "--out", str(TMP / "n7.json")],
                "a request signed as a version it does not carry")
        tampered = TMP / "tampered.fwsc"
        tampered.write_bytes(pkg.read_bytes() + b"x")
        refused("not the one the request froze", ["sign", "--request", str(req), str(tampered), "--key", str(priv),
                         "--keys", str(keys), "--out", str(TMP / "n8.json")],
                "a package that is not the one the request froze")

        # -- an unpinned id is signable only when the caller says which key to expect --------------
        rc, out, err = run(["sign", "--request", str(req), "--signer-cmd", cmd, "--keys", str(keys),
                            "--expect-pub", pub.upper(), "--out", str(TMP / "e1.json")])
        ok(rc == 0, "--expect-pub: same key, uppercase hex, accepted")
        refused("key", ["sign", "--request", str(req), "--signer-cmd", cmd, "--keys", str(keys),
                        "--expect-pub", "0" * 64, "--out", str(TMP / "e2.json")],
                "--expect-pub naming another key than the pin")

        # -- a request that is edited after signing stops verifying -------------------------------
        edited = json.loads(withkey.read_text())
        edited["min_firmware"] = "0.0.1"
        verdict = "verified"
        try:
            S.verify(pkg.read_bytes(), edited, S.pinned(keys))
        except S.SignatureError as e:
            verdict = e.code
        ok(verdict == "signature", "an edited manifest is refused by the signature (%s)" % verdict)

        # -- sign-stdin never signs with a key from inside the checkout ----------------------------
        inside = ROOT / "tools" / "leak.pem"
        shutil.copy(priv, inside)
        try:
            rc, out, err = run(["sign-stdin", "--key", str(inside)], stdin=b"payload")
            ok(rc == 1 and "inside the repository" in err,
               "sign-stdin with a key inside the checkout -> custody")
        finally:
            inside.unlink(missing_ok=True)
    finally:
        shutil.rmtree(TMP, ignore_errors=True)

    print("\n%d failure(s)" % failed)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
