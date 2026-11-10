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
  fm1_sign.py request PACKAGE.fwsc --version 3.0.0 --min 2.9.0 --keyid qg1-release-1 \
              --build-manifest build/manifest.json [--out build/sign-request.json]
  fm1_sign.py sign --request build/sign-request.json --signer-cmd 'ssh signer sign-stdin' \
              --expect-pub HEX --out release/PACKAGE.fwsc.manifest.json
  fm1_sign.py sign-stdin --key /secure/qg1-release.pem     # reads the payload on stdin, prints the signature
  fm1_sign.py verify PACKAGE.fwsc --manifest PACKAGE.manifest.json [--keys ...] \
              [--product FM-1] [--firmware 2.9.0]
  fm1_sign.py check [--keys ...] [--require-key]

Custody is enforced here, not merely documented: keygen refuses any --out that resolves inside this
checkout, sign refuses a private key stored in it, sign refuses a --keyid whose pinned key is not the
one the private key belongs to, pin refuses to change an id without --replace, and a malformed pin
file refuses every package instead of being read leniently.

Signing does not have to happen in the process that has the key, or on the machine that built the
package: `request` freezes a build into a qg1-sign-request-1 document, `sign --signer-cmd` hands the
canonical manifest to a command that holds the key (an offline host over ssh, a smartcard helper, an
HSM wrapper) and verifies what comes back before writing anything, and `sign-stdin` is that command's
side. A CI runner can then produce the request and publish what a custodian signed, and never hold a
private key. That is how every release after 2.0.0 is made.
"""
import argparse
import base64
import subprocess
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


REQUEST_FORMAT = "qg1-sign-request-1"


def signature_from(external, payload):
    """A signature produced by a command that holds the key: this process never reads the private half.

    The contract is deliberately small: the canonical manifest goes to the command's stdin, the command
    writes the 64-byte Ed25519 signature on stdout, in hex or base64. Whatever signs -- an offline host
    reached over ssh, a smartcard or HSM helper -- the key stays where it is, and the caller verifies the
    signature against the pinned public key before it is written anywhere.
    """
    try:
        p = subprocess.run(external, shell=True, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError as e:
        raise SignatureError("signer", "the signer command could not run: %s" % e)
    if p.returncode != 0:
        err = (p.stderr or b"").decode("utf-8", "replace").strip()
        raise SignatureError("signer", "the signer failed (exit %d)%s" % (p.returncode, ": " + err if err else ""))
    text = (p.stdout or b"").decode("ascii", "ignore").strip()
    if not text:
        raise SignatureError("signer", "the signer wrote no signature on stdout")
    line = text.splitlines()[-1].strip()
    sig = b""
    try:
        sig = bytes.fromhex(line)
    except ValueError:
        try:
            sig = base64.b64decode(line, validate=True)
        except Exception:
            raise SignatureError("signer", "the signer's output is neither hex nor base64")
    if len(sig) != 64:
        raise SignatureError("signer", "the signer returned %d byte(s): an Ed25519 signature is 64" % len(sig))
    return sig


def signature_verifies(payload, sig, pub_hex, where="the signer"):
    """Never write a signature that does not verify: a wrong signer is caught here, not at the installer."""
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    try:
        Ed25519PublicKey.from_public_bytes(bytes.fromhex(pub_hex)).verify(sig, payload)
    except InvalidSignature:
        raise SignatureError("signer", "%s produced a signature that does not verify under %s" % (where, pub_hex))


def make_request(pkg_path, version, minimum, keyid, build_manifest, product="FM-1", created=None):
    """Freeze a built package into a qg1-sign-request-1 document a custodian can sign elsewhere.

    Everything the signature will cover is decided here, once: the package hash and size, the firmware
    image hashes, the build record, the version, the minimum firmware and the timestamp. The custodian
    signs those exact bytes, so a build that changes afterwards cannot reuse the signature.
    """
    raw = Path(pkg_path).read_bytes()
    if not build_manifest:
        raise SignatureError("record", "a signing request must name the build record: the installer refuses a "
                                       "release that names none, so such a request could never become one")
    record = json.loads(Path(build_manifest).read_text())
    man = manifest_for(raw, version, minimum, product, keyid, created=created,
                       build_sha256=record_sha256(build_manifest), images=images_of(record), build=record)
    return {"format": REQUEST_FORMAT, "package": Path(pkg_path).name, "manifest": man}


def request_manifest(request):
    """The unsigned manifest inside a request, after checking the request is one and says enough to sign."""
    if not isinstance(request, dict) or request.get("format") != REQUEST_FORMAT:
        raise SignatureError("request", "not a %s document" % REQUEST_FORMAT)
    man = request.get("manifest")
    if not isinstance(man, dict):
        raise SignatureError("request", "the request carries no manifest")
    for field in ("product", "version", "min_firmware", "sha256", "size", "keyid"):
        if field not in man:
            raise SignatureError("request", "the request's manifest has no %s" % field)
    if not _HEX64.match(str(man.get("build_sha256", ""))):
        raise SignatureError("request", "the request's manifest names no build record: sign again with a request "
                                         "made from a build")
    if not isinstance(man.get("images"), dict) or not man["images"]:
        raise SignatureError("request", "the request's manifest names no firmware images")
    if not str(man.get("created", "")).endswith("Z"):
        raise SignatureError("request", "the request's manifest has no created timestamp: the signature would not "
                                         "pin a moment")
    return dict(man)


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
    q = sub.add_parser("request", help="freeze a built package into a signing request (no key involved)")
    q.add_argument("package")
    q.add_argument("--version", required=True)
    q.add_argument("--min", required=True)
    q.add_argument("--keyid", required=True)
    q.add_argument("--product", default="FM-1")
    q.add_argument("--build-manifest", metavar="MANIFEST",
                   help="build/manifest.json of the build that produced the package (a request without one is "
                        "refused: no release exists without that record)")
    q.add_argument("--created", help="the timestamp to pin (default: now, UTC)")
    q.add_argument("--out")
    k = sub.add_parser("keygen", help="generate a release key (private key stays out of the repo)")
    k.add_argument("--out", required=True)
    k.add_argument("--keyid", required=True)
    p = sub.add_parser("pin", help="trust a public key")
    p.add_argument("pub")
    p.add_argument("--keyid", required=True)
    p.add_argument("--keys", default=str(KEYS))
    p.add_argument("--replace", action="store_true",
                   help="trust a different key under an id that is already pinned (a trust decision)")
    s = sub.add_parser("sign", help="sign a package, or a signing request, with a key or an external signer")
    s.add_argument("package", nargs="?")
    s.add_argument("--key", help="a PKCS#8 PEM this process reads (it must live outside the checkout)")
    s.add_argument("--signer-cmd", metavar="CMD",
                   help="a command that holds the key: the canonical manifest goes to its stdin and it writes the "
                        "64-byte signature on stdout, hex or base64. The key never reaches this process, so a "
                        "runner can sign through an offline host without holding one")
    s.add_argument("--expect-pub", metavar="HEX",
                   help="the public key the signer must sign with, for an id that is not pinned yet; without a pin "
                        "or this, nothing can be verified and signing is refused")
    s.add_argument("--request", metavar="REQUEST", help="a qg1-sign-request-1 document: sign exactly what was built")
    s.add_argument("--version")
    s.add_argument("--min")
    s.add_argument("--keyid")
    s.add_argument("--product", default="FM-1")
    s.add_argument("--out")
    s.add_argument("--build-manifest", metavar="MANIFEST",
                   help="build/manifest.json of the build that produced the package: its canonical SHA-256 goes "
                        "inside the signature, and the installer refuses a release that does not name one")
    s.add_argument("--keys", default=str(KEYS),
                   help="the pin file the key id must match (default tools/fm1_keys.json)")
    t = sub.add_parser("sign-stdin",
                       help="sign the payload on stdin and print it (the remote or hardware side of --signer-cmd)")
    t.add_argument("--key", required=True, help="the private key, on the machine that is allowed to hold it")
    t.add_argument("--keys", default=str(KEYS))
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
        if a.cmd == "request":
            req = make_request(a.package, a.version, a.min, a.keyid, a.build_manifest, a.product, a.created)
            dest = Path(a.out or (a.package + ".sign-request.json"))
            dest.write_text(json.dumps(req, indent=2, sort_keys=True) + "\n")
            man = req["manifest"]
            print("request for %s -> %s (sha256 %s, %d B, key %s, build %s)"
                  % (req["package"], dest, man["sha256"], man["size"], man["keyid"],
                     str(man["build_sha256"])[:16]))
            print("sign it where the key lives, then commit the manifest beside the package")
            return 0
        if a.cmd == "sign-stdin":
            if inside_repo(a.key):
                raise SignatureError("custody", "the private key is inside the repository (%s): key material in the "
                                                "checkout is treated as leaked" % a.key)
            from cryptography.hazmat.primitives.serialization import load_pem_private_key
            payload = sys.stdin.buffer.read()
            if not payload:
                raise SignatureError("signer", "nothing on stdin to sign")
            key = load_pem_private_key(Path(a.key).read_bytes(), password=None)
            print(key.sign(payload).hex())
            print("signed %d byte(s), sha256 %s" % (len(payload), hashlib.sha256(payload).hexdigest()[:16]),
                  file=sys.stderr)
            return 0
        if a.cmd == "sign":
            if bool(a.key) == bool(a.signer_cmd):
                raise SignatureError("usage", "sign needs exactly one of --key (a PEM this process reads) or "
                                              "--signer-cmd (a command that holds the key)")
            if a.key and inside_repo(a.key):
                raise SignatureError("custody", "the private key is inside the repository (%s): key material in the "
                                                "checkout is treated as leaked. Sign from a key kept outside it"
                                     % a.key)
            try:
                trusted = pinned(a.keys)    # a broken pin file cannot block signing; `check` reports it
            except SignatureError:
                trusted = {}
            expect = public_hex(a.expect_pub) if a.expect_pub else None
            if a.request:
                try:
                    req = json.loads(Path(a.request).read_text())
                except (OSError, ValueError) as e:
                    raise SignatureError("request", "%s is not readable JSON: %s" % (a.request, e))
                man = request_manifest(req)
                if a.version and str(a.version) != str(man["version"]):
                    raise SignatureError("version", "the request froze version %s, not %s"
                                         % (man["version"], a.version))
                if a.min and str(a.min) != str(man["min_firmware"]):
                    raise SignatureError("version", "the request needs firmware %s, not %s"
                                         % (man["min_firmware"], a.min))
                if a.keyid and str(a.keyid) != str(man["keyid"]):
                    raise SignatureError("key", "the request is for key %r, not %r" % (man["keyid"], a.keyid))
                if a.product and str(a.product) != str(man["product"]):
                    raise SignatureError("product", "the request is for %r, not %r" % (man["product"], a.product))
                if a.package:
                    raw = Path(a.package).read_bytes()
                    digest = hashlib.sha256(raw).hexdigest()
                    if digest != man["sha256"] or len(raw) != int(man["size"]):
                        raise SignatureError("hash", "this package is not the one the request froze (%s, %d B vs %s, "
                                                      "%s B)" % (digest, len(raw), man["sha256"], man["size"]))
            else:
                if not (a.package and a.version and a.min and a.keyid):
                    raise SignatureError("usage", "signing a package needs PACKAGE, --version, --min and --keyid, or "
                                                   "a --request document")
                raw = Path(a.package).read_bytes()
                rec = record_sha256(a.build_manifest) if a.build_manifest else None
                record = json.loads(Path(a.build_manifest).read_text()) if a.build_manifest else None
                if not rec:
                    print("warning: no --build-manifest: the manifest names no build record, and the installer "
                          "refuses such a release", file=sys.stderr)
                man = manifest_for(raw, a.version, a.min, a.product, a.keyid, build_sha256=rec,
                                   images=images_of(record) if record else None, build=record)
            payload = canonical(man)
            keyid = str(man["keyid"])
            if a.key:
                pem = Path(a.key).read_bytes()
                derived = public_of_private(pem)
                if keyid in trusted and trusted[keyid].hex() != derived:
                    raise SignatureError("key", "this private key is not the one pinned as %r (%s): the manifest "
                                                 "would never verify" % (keyid, trusted[keyid].hex()))
                if expect and expect != derived:
                    raise SignatureError("key", "this private key is not %s" % expect)
                if keyid not in trusted:
                    print("note: %r is not pinned; its public key is %s" % (keyid, derived), file=sys.stderr)
                sig = sign_manifest(man, pem)["sig"]
            else:
                if keyid in trusted:
                    expected = trusted[keyid].hex()
                elif expect:
                    expected = expect
                else:
                    raise SignatureError("signer", "key %r is not pinned and no --expect-pub was given: nothing "
                                                    "could verify what the signer returns, so signing is refused"
                                         % keyid)
                if expect and keyid in trusted and trusted[keyid].hex() != expect:
                    raise SignatureError("key", "--expect-pub %s is not the key pinned as %r (%s)"
                                         % (expect, keyid, trusted[keyid].hex()))
                sig = signature_from(a.signer_cmd, payload).hex()
                signature_verifies(payload, bytes.fromhex(sig), expected)
            out = dict(man)
            out["sig"] = sig
            dest = Path(a.out or ((a.package + ".manifest.json") if a.package
                                  else "release/felucca-%s.fwsc.manifest.json" % man["version"]))
            dest.write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
            rec = man.get("build_sha256")
            print("signed %s -> %s (key %s, sha256 %s%s)"
                  % (a.package or "version %s" % man["version"], dest, keyid, out["sha256"],
                     ", record %s" % str(rec)[:16] if rec else ""))
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
