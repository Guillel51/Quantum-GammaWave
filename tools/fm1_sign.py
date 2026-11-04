#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Ed25519 authenticity chain for QG-1 packages (.fwsc), host side.

A release is a package plus a signed manifest:

  {"format": "qg1-release-1", "product": "FM-1", "version": "3.0.0",
   "min_firmware": "2.9.0", "sha256": "...", "size": 610897,
   "created": "2026-11-04T00:00:00Z", "keyid": "qg1-dev-1",
   "sig": "<128 hex>"}

The signature covers the manifest without "sig", canonicalised as sorted,
compact JSON. It is verified against the keys pinned in tools/fm1_keys.json,
which contains PUBLIC keys only. An empty pin set means "nothing is trusted",
so every package is refused; that is the state until the release key is
published. The private key never lives in this repository.

This is authenticity, not secure boot: the stock loader does not check
signatures, so the chain is only as strong as the installer that runs it.

  fm1_sign.py keygen --out /secure/qg1-release.pem --keyid qg1-release-1
  fm1_sign.py pin KEY.pub --keyid qg1-release-1 [--keys tools/fm1_keys.json]
  fm1_sign.py sign PACKAGE.fwsc --key /secure/qg1-release.pem --version 3.0.0 \
              --min 2.9.0 --keyid qg1-release-1 [--out PACKAGE.manifest.json]
  fm1_sign.py verify PACKAGE.fwsc --manifest PACKAGE.manifest.json [--keys ...] \
              [--product FM-1] [--firmware 2.9.0]
"""
import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

FORMAT = "qg1-release-1"
HERE = Path(__file__).resolve().parent
KEYS = HERE / "fm1_keys.json"


class SignatureError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def canonical(obj):
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()


def version_tuple(v):
    try:
        return tuple(int(x) for x in str(v).split("."))
    except ValueError:
        raise SignatureError("version", "bad version %r" % (v,))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def pinned(path=KEYS):
    path = Path(path)
    if not path.exists():
        return {}
    data = json.loads(path.read_text())
    keys = data.get("keys", data)
    return {k: bytes.fromhex(v) for k, v in keys.items()}


def manifest_for(pkg_bytes, version, minimum, product="FM-1", keyid=None, created=None):
    return {
        "format": FORMAT,
        "product": product,
        "version": str(version),
        "min_firmware": str(minimum),
        "sha256": hashlib.sha256(pkg_bytes).hexdigest(),
        "size": len(pkg_bytes),
        "created": created or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "keyid": keyid or "",
    }


def sign_manifest(manifest, private_key_bytes):
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives.serialization import load_pem_private_key

    key = load_pem_private_key(private_key_bytes, password=None)
    payload = {k: v for k, v in manifest.items() if k != "sig"}
    sig = key.sign(canonical(payload))
    out = dict(payload)
    out["sig"] = sig.hex()
    return out


def verify(pkg_bytes, manifest, keys, product="FM-1", firmware=None):
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

    if not isinstance(manifest, dict):
        raise SignatureError("manifest", "no manifest")
    if manifest.get("format") != FORMAT:
        raise SignatureError("manifest", "format %r is not %r" % (manifest.get("format"), FORMAT))
    for field in ("product", "version", "min_firmware", "sha256", "size", "keyid", "sig"):
        if field not in manifest:
            raise SignatureError("manifest", "manifest has no %s" % field)
    sig = manifest["sig"]
    try:
        sig = bytes.fromhex(sig)
    except (TypeError, ValueError):
        raise SignatureError("signature", "the signature is not hex")
    if len(sig) != 64:
        raise SignatureError("signature", "Ed25519 signatures are 64 bytes, this is %d" % len(sig))
    keyid = manifest["keyid"]
    if keyid not in keys:
        raise SignatureError("key", "key id %r is not pinned; %d key(s) trusted" % (keyid, len(keys)))
    if manifest["product"] != product:
        raise SignatureError("product", "the package is for %r, this is %r"
                             % (manifest["product"], product))
    digest = hashlib.sha256(pkg_bytes).hexdigest()
    if digest != manifest["sha256"]:
        raise SignatureError("hash", "sha256 %s does not match the manifest's %s"
                             % (digest, manifest["sha256"]))
    if len(pkg_bytes) != int(manifest["size"]):
        raise SignatureError("size", "package is %d bytes, the manifest says %s"
                             % (len(pkg_bytes), manifest["size"]))
    payload = {k: v for k, v in manifest.items() if k != "sig"}
    try:
        Ed25519PublicKey.from_public_bytes(keys[keyid]).verify(sig, canonical(payload))
    except InvalidSignature:
        raise SignatureError("signature", "the manifest is not signed by %r" % keyid)
    if firmware is not None and version_tuple(firmware) < version_tuple(manifest["min_firmware"]):
        raise SignatureError("downgrade", "the package needs firmware %s, this one is %s"
                             % (manifest["min_firmware"], firmware))
    if version_tuple(manifest["min_firmware"]) > version_tuple(manifest["version"]):
        raise SignatureError("version", "min_firmware is newer than the package")
    return {"keyid": keyid, "version": manifest["version"], "min_firmware": manifest["min_firmware"],
            "product": manifest["product"], "sha256": digest, "size": len(pkg_bytes)}


def keygen(out, keyid):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

    out = Path(out)
    if out.exists():
        raise SignatureError("exists", "%s already exists, refusing to overwrite a key" % out)
    key = Ed25519PrivateKey.generate()
    out.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                                      serialization.PrivateFormat.PKCS8,
                                      serialization.NoEncryption()))
    os.chmod(out, 0o600)
    pub = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    print("private key: %s (mode 600, keep it out of the repository)" % out)
    print("public key (%s): %s" % (keyid, pub.hex()))
    return pub.hex()


def pin(pub_path, keyid, keys_path=KEYS):
    raw = Path(pub_path).read_bytes()
    text = raw.decode(errors="ignore").strip()
    pub = bytes.fromhex(text) if len(text) in (64, 66) and "\n" not in text else raw
    if len(pub) == 65 and pub[0] == 4:      # an uncompressed EC point is not Ed25519
        raise SignatureError("key", "that is not an Ed25519 raw public key")
    if len(pub) != 32:
        raise SignatureError("key", "an Ed25519 public key is 32 bytes, this is %d" % len(pub))
    keys_path = Path(keys_path)
    data = json.loads(keys_path.read_text()) if keys_path.exists() else {"keys": {}}
    data.setdefault("keys", {})[keyid] = pub.hex()
    keys_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    print("pinned %s = %s in %s" % (keyid, pub.hex(), keys_path))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Ed25519 authenticity for QG-1 packages")
    sub = ap.add_subparsers(dest="cmd", required=True)
    k = sub.add_parser("keygen", help="generate a release key (private key stays out of the repo)")
    k.add_argument("--out", required=True)
    k.add_argument("--keyid", required=True)
    p = sub.add_parser("pin", help="trust a public key")
    p.add_argument("pub")
    p.add_argument("--keyid", required=True)
    p.add_argument("--keys", default=str(KEYS))
    s = sub.add_parser("sign", help="sign a package")
    s.add_argument("package")
    s.add_argument("--key", required=True)
    s.add_argument("--version", required=True)
    s.add_argument("--min", required=True)
    s.add_argument("--keyid", required=True)
    s.add_argument("--product", default="FM-1")
    s.add_argument("--out")
    v = sub.add_parser("verify", help="verify a package against its manifest")
    v.add_argument("package")
    v.add_argument("--manifest", required=True)
    v.add_argument("--keys", default=str(KEYS))
    v.add_argument("--product", default="FM-1")
    v.add_argument("--firmware")
    a = ap.parse_args(argv)

    try:
        if a.cmd == "keygen":
            keygen(a.out, a.keyid)
            return 0
        if a.cmd == "pin":
            pin(a.pub, a.keyid, a.keys)
            return 0
        if a.cmd == "sign":
            raw = Path(a.package).read_bytes()
            man = manifest_for(raw, a.version, a.min, a.product, a.keyid)
            if a.keyid not in pinned():
                print("note: %r is not pinned yet; run `pin` with its public key" % a.keyid,
                      file=sys.stderr)
            out = sign_manifest(man, Path(a.key).read_bytes())
            dest = Path(a.out or (a.package + ".manifest.json"))
            dest.write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
            print("signed %s -> %s (key %s, sha256 %s)" % (a.package, dest, a.keyid, out["sha256"]))
            return 0
        if a.cmd == "verify":
            raw = Path(a.package).read_bytes()
            man = json.loads(Path(a.manifest).read_text())
            info = verify(raw, man, pinned(a.keys), a.product, a.firmware)
            print("verified: %s version %s, key %s, sha256 %s"
                  % (info["product"], info["version"], info["keyid"], info["sha256"]))
            return 0
    except SignatureError as e:
        print("refused: %s" % e, file=sys.stderr)
        return 1
    return 2


if __name__ == "__main__":
    sys.exit(main())
