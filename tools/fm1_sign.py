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
  fm1_sign.py pin KEY.pub --keyid qg1-release-1 [--replace] [--keys tools/fm1_keys.json]
  fm1_sign.py unpin --keyid qg1-release-1 [--allow-empty]     # revocation, not an edit of the JSON
  fm1_sign.py sign PACKAGE.fwsc --key /secure/qg1-release.pem --version 3.0.0 \
              --min 2.9.0 --keyid qg1-release-1 [--out PACKAGE.manifest.json]
  fm1_sign.py verify PACKAGE.fwsc --manifest PACKAGE.manifest.json [--keys ...] \
              [--product FM-1] [--firmware 2.9.0]
  fm1_sign.py check [--keys ...] [--require-key]

Custody is enforced here, not merely documented: keygen refuses any --out that resolves inside this
checkout, sign refuses a private key stored in it, sign refuses a --keyid whose pinned key is not the
one the private key belongs to, pin refuses to change an id without --replace, and a malformed pin
file refuses every package instead of being read leniently.
"""
import argparse
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

FORMAT = "qg1-release-1"
HERE = Path(__file__).resolve().parent
KEYS = HERE / "fm1_keys.json"
REPO = HERE.parent
KEYID = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}")
_HEX64 = re.compile(r"^(?:0[xX])?([0-9a-fA-F]{64})$")


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


def inside_repo(path):
    """True when `path` resolves inside this checkout: private key material must never land there."""
    try:
        Path(path).resolve().relative_to(REPO)
        return True
    except (OSError, ValueError):
        return False


def public_hex(value):
    """-> 64 lowercase hex characters, from 32 raw bytes, hex text or a PEM public key file."""
    if isinstance(value, str):
        value = value.encode()
    if not isinstance(value, (bytes, bytearray)):
        raise SignatureError("key", "a public key must be bytes or hex text")
    raw = bytes(value)
    if b"-----BEGIN" in raw:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        try:
            key = serialization.load_pem_public_key(raw)
        except Exception as e:              # any parse failure is a refusal, never a fallback
            raise SignatureError("key", "the PEM public key could not be read: %s" % e)
        if not isinstance(key, Ed25519PublicKey):
            raise SignatureError("key", "that PEM key is not Ed25519")
        return key.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()
    m = _HEX64.match(raw.decode("ascii", "ignore").strip())
    if m:
        return m.group(1).lower()
    if len(raw) == 32:
        return raw.hex()
    if len(raw) == 65 and raw[0] == 4:      # an uncompressed EC point is not Ed25519
        raise SignatureError("key", "that is an uncompressed EC point, not an Ed25519 public key")
    raise SignatureError("key", "an Ed25519 public key is 32 raw bytes or 64 hex characters, this is %d byte(s)"
                         % len(raw))


def public_of_private(private_key_bytes):
    """-> the public key that belongs to a private key, so a manifest cannot name the wrong key id."""
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    try:
        key = serialization.load_pem_private_key(private_key_bytes, password=None)
    except Exception as e:
        raise SignatureError("key", "the private key could not be read: %s" % e)
    if not isinstance(key, Ed25519PrivateKey):
        raise SignatureError("key", "that private key is not Ed25519")
    return key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()


def pinned(path=KEYS):
    """-> {key id: 32 raw bytes}. A missing file pins nothing; a malformed entry refuses everything."""
    path = Path(path)
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError) as e:
        raise SignatureError("keys", "%s is not readable JSON: %s" % (path, e))
    keys = data.get("keys", data) if isinstance(data, dict) else None
    if not isinstance(keys, dict):
        raise SignatureError("keys", "%s: 'keys' must be an object of key id -> public key" % path)
    out = {}
    for kid, value in keys.items():
        try:
            out[kid] = bytes.fromhex(public_hex(value))
        except SignatureError as e:
            raise SignatureError("keys", "%s: key %r is invalid: %s" % (path, kid, e))
    return out


def record_sha256(path):
    """The digest of a build record (build/manifest.json), over its canonical JSON.

    Not the file's bytes: the installer holds the record as parsed JSON (inlined in the page), so
    both sides hash sorted keys with no spaces and the signature covers the same value the transport
    compares against the record it verified. ASCII escapes match json.dumps' default in JS too.
    """
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        raise SignatureError("record", "%s is not readable JSON: %s" % (path, e))
    if not isinstance(data, dict):
        raise SignatureError("record", "%s is not a build record (not an object)" % (path,))
    return hashlib.sha256(canonical(data)).hexdigest()


def images_of(record):
    """{artefact: sha256} from a qg1-build-1 record: what the package's signature is tied to.

    The package bytes are reproducible -- the same sources and toolchain give the same .fwsc on any
    commit that does not touch the firmware -- while the build record names the commit that produced
    it. Signing the image hashes lets a page prove that the firmware inside the package is the one the
    custodian signed, whatever commit rebuilt it, and still refuse the moment those bytes change."""
    arts = (record or {}).get("artifacts") if isinstance(record, dict) else None
    # The package's own entry is left out: its bytes are what `sha256` in the manifest already covers,
    # and its file name changes with the site's version label. What the signature ties to is the
    # firmware inside -- the app image and the loader -- under the names every build record uses.
    return {k: str(v.get("sha256")) for k, v in (arts or {}).items()
            if isinstance(v, dict) and v.get("sha256") and not k.endswith(".fwsc")}


def manifest_for(pkg_bytes, version, minimum, product="FM-1", keyid=None, created=None, build_sha256=None,
                 images=None, build=None):
    out = {
        "format": FORMAT,
        "product": product,
        "version": str(version),
        "min_firmware": str(minimum),
        "sha256": hashlib.sha256(pkg_bytes).hexdigest(),
        "size": len(pkg_bytes),
        "created": created or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "keyid": keyid or "",
    }
    if build_sha256:
        out["build_sha256"] = str(build_sha256)
    if images:
        out["images"] = {str(k): str(v) for k, v in sorted(images.items())}
    # The record travels inside the signature: the commit, the toolchain and the artefact list of the
    # build that produced these bytes. A page that serves the signed release can then show, and check,
    # the build it installs without trusting a second file.
    if build:
        out["build"] = build
    return out


def sign_manifest(manifest, private_key_bytes):
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives.serialization import load_pem_private_key

    key = load_pem_private_key(private_key_bytes, password=None)
    payload = {k: v for k, v in manifest.items() if k != "sig"}
    sig = key.sign(canonical(payload))
    out = dict(payload)
    out["sig"] = sig.hex()
    return out


def verify(pkg_bytes, manifest, keys, product="FM-1", firmware=None, build_sha256=None, require_build=False,
           images=None):
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
    key = keys[keyid]
    if not isinstance(key, (bytes, bytearray)) or len(key) != 32:
        raise SignatureError("key", "the pinned key %r is not a 32-byte Ed25519 public key" % keyid)
    payload = {k: v for k, v in manifest.items() if k != "sig"}
    try:
        Ed25519PublicKey.from_public_bytes(bytes(key)).verify(sig, canonical(payload))
    except InvalidSignature:
        raise SignatureError("signature", "the manifest is not signed by %r" % keyid)
    # The signature covers the build record it names; the installer refuses a release that names
    # none, so a signature over a manifest without build_sha256 is not a releasable manifest.
    # The images, when both sides name them: a signature over the firmware the package carries.
    if "images" in manifest:
        imgs = manifest["images"]
        if not isinstance(imgs, dict) or not imgs:
            raise SignatureError("images", "the manifest's images are not a map")
        for name, value in imgs.items():
            if not isinstance(value, str) or not _HEX64.match(value):
                raise SignatureError("images", "%s is not a SHA-256" % (name,))
        if images is not None:
            missing = sorted(set(imgs) - set(images))
            wrong = sorted(k for k in set(imgs) & set(images) if imgs[k] != images[k])
            if missing or wrong:
                raise SignatureError("images", "the signature names images this build does not have (%s)"
                                     % ", ".join(missing + wrong))
    if "build_sha256" in manifest:
        if not _HEX64.match(str(manifest["build_sha256"])):
            raise SignatureError("record", "build_sha256 is not a SHA-256: the manifest is malformed")
        if build_sha256 is not None and str(manifest["build_sha256"]) != str(build_sha256):
            raise SignatureError("record", "the manifest names build record %s, this build is %s"
                                 % (manifest["build_sha256"], build_sha256))
    elif build_sha256 is not None or require_build:
        raise SignatureError("record", "the manifest does not name a build record: the installer refuses "
                                       "it, sign again with --build-manifest")
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

    if not KEYID.fullmatch(keyid or ""):
        raise SignatureError("keyid", "%r is not a key id: use [A-Za-z0-9][A-Za-z0-9._-]{0,63}" % (keyid,))
    out = Path(out)
    if inside_repo(out):
        raise SignatureError("custody", "refusing to write a private key inside the repository: %s resolves under "
                                        "%s. Generate it offline and keep it outside the checkout" % (out, REPO))
    if not out.parent.is_dir():
        raise SignatureError("path", "%s: the directory does not exist" % out.parent)
    key = Ed25519PrivateKey.generate()
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption())
    try:
        # O_EXCL + 0600: never an existing file, and never a moment where the key is world-readable
        fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise SignatureError("exists", "%s already exists, refusing to overwrite a key" % out)
    with os.fdopen(fd, "wb") as f:
        f.write(pem)
    os.chmod(out, 0o600)
    pub = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    print("private key: %s (mode 600, outside the checkout: keep a second copy offline)" % out)
    print("public key (%s): %s" % (keyid, pub.hex()))
    print("next: pin that public key with `fm1_sign.py pin KEY.pub --keyid %s`" % keyid)
    return pub.hex()


def pin(pub_path, keyid, keys_path=KEYS, replace=False):
    """Trust one public key. Moving an id to a different key is a trust decision: it needs replace."""
    if not KEYID.fullmatch(keyid or ""):
        raise SignatureError("keyid", "%r is not a key id: use [A-Za-z0-9][A-Za-z0-9._-]{0,63}" % (keyid,))
    try:
        raw = Path(pub_path).read_bytes()
    except OSError as e:
        raise SignatureError("key", "cannot read %s: %s" % (pub_path, e))
    pub = public_hex(raw)
    keys_path = Path(keys_path)
    if keys_path.exists():
        try:
            data = json.loads(keys_path.read_text())
        except (OSError, ValueError) as e:
            raise SignatureError("keys", "%s is not readable JSON: %s" % (keys_path, e))
        if not isinstance(data, dict):
            raise SignatureError("keys", "%s must hold an object" % keys_path)
    else:
        data = {"keys": {}}
    keys = data.setdefault("keys", {})
    if not isinstance(keys, dict):
        raise SignatureError("keys", "%s: 'keys' must be an object" % keys_path)
    known = keys.get(keyid)
    if known is not None:
        if str(known).lower() == pub:
            print("%r is already pinned to %s in %s" % (keyid, pub, keys_path))
            return pub
        if not replace:
            raise SignatureError("key", "%r is already pinned to %s; --replace is needed to trust a different key "
                                         "under the same id" % (keyid, known))
        print("replacing %r: %s -> %s" % (keyid, known, pub))
    keys[keyid] = pub
    keys_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    print("pinned %s = %s in %s" % (keyid, pub, keys_path))
    return pub


def unpin(keyid, keys_path=KEYS, allow_empty=False):
    """Revoke one key id: the packages it signed stop verifying, the others are untouched.

    Revocation is an operation, not an edit: it has to name the id, it refuses an id that is not
    pinned (so a typo cannot look like a revocation), and it refuses to leave the ring empty unless
    --allow-empty says so out loud -- an empty ring is the fail-closed state, and reaching it by
    accident would hide a mistake behind "every package is refused anyway".
    """
    if not KEYID.fullmatch(keyid or ""):
        raise SignatureError("keyid", "%r is not a key id: use [A-Za-z0-9][A-Za-z0-9._-]{0,63}" % (keyid,))
    path = Path(keys_path)
    if not path.exists():
        raise SignatureError("keys", "%s does not exist: nothing is trusted and nothing can be revoked" % path)
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError) as e:
        raise SignatureError("keys", "%s is not readable JSON: %s" % (path, e))
    keys = data.get("keys") if isinstance(data, dict) else None
    if not isinstance(keys, dict):
        raise SignatureError("keys", "%s: 'keys' must be an object" % path)
    if keyid not in keys:
        raise SignatureError("key", "%r is not pinned in %s: nothing to revoke" % (keyid, path))
    del keys[keyid]
    if not keys and not allow_empty:
        raise SignatureError("keys", "revoking %r would leave no key pinned: pass --allow-empty if that state "
                                     "is what you mean" % keyid)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    print("revoked %r from %s (%d key(s) left: %s)"
          % (keyid, path, len(keys), ", ".join(sorted(keys)) or "none"))
    return sorted(keys)


def check(keys_path=KEYS, require_key=False):
    """Report the pinned keys. An empty pin set is a state (fail-closed), not an error, unless required."""
    path = Path(keys_path)
    if not path.exists():
        raise SignatureError("keys", "%s does not exist: nothing is trusted and every package is refused" % path)
    keys = pinned(path)
    if not keys:
        if require_key:
            raise SignatureError("keys", "%s pins no key: every package is refused (fail-closed)" % path)
        print("%s pins no key: every package is refused (fail-closed)" % path)
        print("a release key is generated outside this checkout and pinned here: docs/SIGNING_CUSTODY.md")
        return {}
    print("%s pins %d key(s):" % (path, len(keys)))
    for kid in sorted(keys):
        print("  %-24s %s" % (kid, keys[kid].hex()))
    return keys


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
    p.add_argument("--replace", action="store_true",
                   help="trust a different key under an id that is already pinned (a trust decision)")
    s = sub.add_parser("sign", help="sign a package")
    s.add_argument("package")
    s.add_argument("--key", required=True)
    s.add_argument("--version", required=True)
    s.add_argument("--min", required=True)
    s.add_argument("--keyid", required=True)
    s.add_argument("--product", default="FM-1")
    s.add_argument("--out")
    s.add_argument("--build-manifest", metavar="MANIFEST",
                   help="build/manifest.json of the build that produced the package: its canonical SHA-256 goes "
                        "inside the signature, and the installer refuses a release that does not name one")
    s.add_argument("--keys", default=str(KEYS),
                   help="the pin file the key id must match (default tools/fm1_keys.json)")
    v = sub.add_parser("verify", help="verify a package against its manifest")
    v.add_argument("package")
    v.add_argument("--manifest", required=True)
    v.add_argument("--keys", default=str(KEYS))
    v.add_argument("--product", default="FM-1")
    v.add_argument("--firmware")
    v.add_argument("--build-manifest", metavar="MANIFEST",
                   help="the build record the release must name (default: require that it names one)")
    u = sub.add_parser("unpin", help="revoke a pinned key id (packages it signed stop verifying)")
    u.add_argument("--keyid", required=True)
    u.add_argument("--keys", default=str(KEYS))
    u.add_argument("--allow-empty", action="store_true",
                   help="allow a revocation that leaves no key pinned (the fail-closed state)")
    c = sub.add_parser("check", help="report the pinned public keys (fail-closed state included)")
    c.add_argument("--keys", default=str(KEYS))
    c.add_argument("--require-key", action="store_true", help="exit 1 when no key is pinned")
    a = ap.parse_args(argv)

    try:
        if a.cmd == "keygen":
            keygen(a.out, a.keyid)
            return 0
        if a.cmd == "pin":
            pin(a.pub, a.keyid, a.keys, a.replace)
            return 0
        if a.cmd == "unpin":
            unpin(a.keyid, a.keys, a.allow_empty)
            return 0
        if a.cmd == "sign":
            if inside_repo(a.key):
                raise SignatureError("custody", "the private key is inside the repository (%s): key material in the "
                                                "checkout is treated as leaked. Sign from a key kept outside it"
                                     % a.key)
            raw = Path(a.package).read_bytes()
            pem = Path(a.key).read_bytes()
            derived = public_of_private(pem)
            try:
                trusted = pinned(a.keys)    # a broken pin file cannot block signing; `check` reports it
            except SignatureError:
                trusted = {}
            if a.keyid in trusted and trusted[a.keyid].hex() != derived:
                raise SignatureError("key", "this private key is not the one pinned as %r (%s): the manifest would "
                                             "never verify" % (a.keyid, trusted[a.keyid].hex()))
            if a.keyid not in trusted:
                print("note: %r is not pinned; its public key is %s" % (a.keyid, derived), file=sys.stderr)
            rec = record_sha256(a.build_manifest) if a.build_manifest else None
            record = json.loads(Path(a.build_manifest).read_text()) if a.build_manifest else None
            imgs = images_of(record) if record else None
            if not rec:
                print("warning: no --build-manifest: the manifest names no build record, and the installer "
                      "refuses such a release", file=sys.stderr)
            man = manifest_for(raw, a.version, a.min, a.product, a.keyid, build_sha256=rec, images=imgs,
                               build=record)
            out = sign_manifest(man, pem)
            dest = Path(a.out or (a.package + ".manifest.json"))
            dest.write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
            print("signed %s -> %s (key %s, sha256 %s%s)"
                  % (a.package, dest, a.keyid, out["sha256"], ", record %s" % rec[:16] if rec else ""))
            return 0
        if a.cmd == "verify":
            raw = Path(a.package).read_bytes()
            man = json.loads(Path(a.manifest).read_text())
            rec = record_sha256(a.build_manifest) if a.build_manifest else None
            imgs = images_of(json.loads(Path(a.build_manifest).read_text())) if a.build_manifest else None
            info = verify(raw, man, pinned(a.keys), a.product, a.firmware, build_sha256=rec, require_build=True,
                          images=imgs)
            print("verified: %s version %s, key %s, sha256 %s%s"
                  % (info["product"], info["version"], info["keyid"], info["sha256"],
                     ", record %s" % rec[:16] if rec else ""))
            return 0
        if a.cmd == "check":
            check(a.keys, a.require_key)
            return 0
    except SignatureError as e:
        print("refused: %s" % e, file=sys.stderr)
        return 1
    except OSError as e:
        print("refused: %s" % e, file=sys.stderr)
        return 1
    return 2


if __name__ == "__main__":
    sys.exit(main())
