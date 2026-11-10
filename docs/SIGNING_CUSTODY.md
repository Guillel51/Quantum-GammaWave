# Signing key custody (Ed25519)

Status: **one release key, `qg1-release-1`, pinned and in use.**

| | |
|---|---|
| key id | `qg1-release-1` |
| public key (Ed25519, 32 bytes) | `70b6036fd33da3297a41da2141c5d135b3efce2d815d55911fc4f9b1d07fcd40` |
| pinned in | `tools/fm1_keys.json` (travels with the page, so the installer checks it in the browser) |
| private half | outside the repository, `~/.fm1-keys/qg1-release.pem`, mode `0600`, directory `0700` |
| what it signs | `release/*.manifest.json`: the canonical JSON of the manifest, which names the package's SHA-256, its size, the version and the firmware image hashes |

A build that is not signed with that key is refused by the installer, whatever its hash: the page
enables INSTALL only after `verifyManifest` (WebCrypto Ed25519) passes against the pinned key, and
`tools/fm1_sign.py verify` refuses the same things again on the release side. With no key pinned,
everything is refused — that fail-closed state is still the default of the code, not a special case.

This page is the procedure a custodian follows. It is not a key, and this repository never holds
the private half.

---

## 1. What the code enforces (not only asks for)

The rules below are in `tools/fm1_sign.py` and the tests fail if any of them stops holding:

| Rule | Where | Refusal |
|------|-------|---------|
| A private key is never generated inside the checkout | `keygen`: the destination is resolved (`..`, symlinks) and compared with the repository root | `custody` |
| A private key is never read from the checkout to sign | `sign`: `--key` resolved against the repository root | `custody` |
| A key file is created `0600`, never truncated, never overwritten | `keygen`: `os.open(..., O_CREAT | O_EXCL, 0o600)` | `exists` |
| The manifest's `keyid` must be the key that actually signs | `sign`: the public half of `--key` is compared with the pin | `key` |
| A pinned id cannot be moved to a different key by accident | `pin`: needs `--replace` | `key` |
| Pins are validated, not trusted as text | `pin`, `pinned`: 32 raw bytes, 64 hex characters or an Ed25519 PEM; a 65-byte EC point is named as such | `key` |
| A malformed pin file refuses everything | `pinned()` raises instead of skipping the entry | `keys` |
| The empty pin set is reportable, and can be made fatal | `check` (exit 0) and `check --require-key` (exit 1) | `keys` |

```bash
python3 tools/fm1_sign.py check                    # prints the pinned keys, or the fail-closed state
python3 tools/fm1_sign.py sign build/felucca-X.Y.Z.fwsc \
    --key ~/.fm1-keys/qg1-release.pem --keyid qg1-release-1 \
    --version X.Y.Z --min 1.0.0 --build-manifest build/manifest.json
python3 tools/fm1_sign.py check --require-key      # exit 1 while no key is pinned (use in release CI)
python3 tests/key_custody_test.py                  # the whole cycle, with an ephemeral key
```

## 2. Roles

* **Owner** — authorises a release and the key ceremony. Never holds the key as a second copy
  unless that is written down.
* **Custodian** — generates the key, holds the encrypted copies, signs packages. One person, named
  in the signed record of §3.3.
* **Signer** — the machine and account that run `fm1_sign.py sign`. It may be the custodian's own
  offline machine; a signing host that also has the package build is a single point of failure.

Two people are needed for a rotation seen by the project (§5), because a pin change is a trust
decision, not an edit.

## 3. Generation ceremony

Run this on an offline machine, from a checkout that is already built (the key ceremony does not
build anything, but the machine should have the tools it needs before it loses the network).

### 3.1 Prepare

1. Fresh or wiped machine, no network, no cloud sync folder, no screen recording.
2. Work in a directory outside every git work tree, e.g. `/secure/qg1` (mode `0700`).
3. Confirm the tree is clean and the tools are the intended commit:
   `git rev-parse HEAD`, `python3 tools/fm1_sign.py check`.

### 3.2 Generate

```bash
python3 tools/fm1_sign.py keygen --out /secure/qg1/qg1-release-1.pem --keyid qg1-release-1
```

`keygen` refuses any destination inside the checkout and creates the file `0600` with `O_EXCL`.
It prints the public key; nothing else is written. Write the public key to removable media:

```bash
python3 - <<'PY' > /media/usb/qg1-release-1.pub
import sys; sys.path.insert(0, "tools")
import fm1_sign as S
print(S.public_of_private(open("/secure/qg1/qg1-release-1.pem", "rb").read()))
PY
```

### 3.3 Record, copy, close

* Record in the release log **outside the repository**: key id, public key hex, date, operator,
  machine, and the commit whose tools were used.
* Keep two encrypted copies in two separate physical places; a passphrase held by the owner or in
  a separate safe. One copy is not custody, it is a hope.
* Do not photograph the key, do not paste it into a chat, an issue, a CI secret or an agent.
* Clear the shell history of the ceremony (`history -c`) and unmount the media.

### 3.4 Pin the public key

On a normal machine, with the public key file only:

```bash
python3 tools/fm1_sign.py pin qg1-release-1.pub --keyid qg1-release-1
python3 tools/fm1_sign.py check --require-key
```

`pin` writes `tools/fm1_keys.json` (public keys only, comment preserved) and is idempotent for the
same key. That file is reviewed like code: it is the only thing both installers trust.

`web/make_site.py` ships that file to the installer page as `meta.keys`, so pinning is enough for
the browser verifier; no code change is needed.

## 4. Signing a package

The package must be the one that was built and tested, not a rebuild:

```bash
sha256sum build/felucca.fwsc                       # compare with the build log
python3 tools/fm1_sign.py sign build/felucca.fwsc \
        --key /secure/qg1/qg1-release-1.pem --keyid qg1-release-1 \
        --version 1.9.0 --min 1.8.0
python3 tools/fm1_sign.py verify build/felucca.fwsc \
        --manifest build/felucca.fwsc.manifest.json
```

`sign` refuses a private key inside the repository and a `keyid` whose pinned key is not the one
the private key belongs to; `verify` checks the SHA-256, the size, the product, the key id, the
signature and `min_firmware`. Then:

* keep the manifest beside the package (`make_site.py` publishes it per version as
  `meta.versions[i].manifest`);
* record the package hash and the manifest in the release log;
* `min_firmware` is signed too, so it cannot be lowered by a mirror.

### 4.1 Signing where the key lives

A build machine -- a CI runner above all -- should never be able to sign. From 2.1.1 the release is made in
two halves that meet at a file, and only one of them touches the key:

```bash
# 1. the machine that built it (no key, and none can be given to it): freeze exactly what will be signed
python3 tools/fm1_sign.py request build/felucca-2.1.1.fwsc --version 2.1.1 --min 1.0.0 \
        --keyid qg1-release-1 --build-manifest build/manifest.json --out build/sign-request.json

# 2. the machine that holds the key: sign those frozen bytes, not "the project"
python3 tools/fm1_sign.py sign --request build/sign-request.json \
        --signer-cmd "ssh signer-host python3 tools/fm1_sign.py sign-stdin --key /secure/qg1-release.pem" \
        --out release/felucca-2.1.1.fwsc.manifest.json
```

The request (`qg1-sign-request-1`) pins the package's SHA-256 and size, the firmware image hashes, the build
record and its digest, the version, `min_firmware`, the key id and the timestamp. The manifest that comes back
is the signature over exactly that JSON: if the build changes afterwards, the signature cannot be reused for
it.

`--signer-cmd` is a shell command with one contract: the canonical manifest on stdin, the 64-byte Ed25519
signature on stdout (hex or base64). `sign-stdin` is this repository's side of it -- read stdin, sign, print
the hex -- and it refuses a key that resolves inside the checkout like every other path. The signature the
command returns is verified against the pin (or `--expect-pub` for an id that is not pinned yet) **before**
the manifest is written: a signer holding another key, printing nonsense, or exiting non-zero produces no
file. That is why `--key` and `--signer-cmd` are mutually exclusive, and why neither one, on its own, is
accepted.

What this buys, and what it does not:

| | |
|---|---|
| The key is not in CI | `emulator.yml` writes `build/sign-request.json` and uploads it as an artifact; it never receives a private key, and no secret is configured for one |
| The key is not in the process that has the package | the custodian signs a 2 KB JSON that names bytes they did not build |
| A signature cannot be replayed onto another build | the request froze the hash, the images and the record inside what is signed |
| What is written is what was verified | `sign` re-verifies the returned signature against the pin before writing; `verify` does it again on the release side |
| Still not protected | a hostile signing host, a wrong `--expect-pub` chosen by the operator, and the browser origin (see §7) |

The remote host in the example must run the same reviewed `tools/fm1_sign.py`; `ssh signer-host ...` is a
convenience, not a trust anchor -- the anchor is the pinned public key, which never leaves the repository.
A smartcard or HSM helper works the same way: it wraps the module and prints the signature.

Measured on 2026-11-10 against the released build: an `sshd` on loopback with `ForceCommand` limited to
`sign-stdin`, and `sign --request --signer-cmd "ssh …"` over it, returned the **byte-identical** manifest
and signature that `release/felucca-2.1.1.fwsc.manifest.json` carries. That proves the procedure and the
contract; the machine and the uid were the same, so it is not evidence of a physical separation -- a real
custodian signs on a host that holds nothing else, which is the point of `--signer-cmd`.

`tests/sign_request_test.py` exercises the cycle with an ephemeral key: request -> `--key` and request ->
`--signer-cmd` produce the same manifest byte for byte, and every wrong path (nonsense output, failing
command, other key, no pin and no `--expect-pub`, both flags, neither flag, a package that is not the one the
request froze, an edited manifest) is refused. `signed-release.yml` runs it, plus `key_custody_test.py`, on
every change to `release/` or to the signer.

## 5. Rotation

Rotate on a schedule, on a suspected exposure, or when the signing host changes.

1. Generate the next key **offline** under a new id (`qg1-release-2`) and record it.
2. Pin the new id **next to** the old one (`pin ... --keyid qg1-release-2`). Two ids trusted at
   once is the normal transition state.
3. Re-sign the packages that are still distributed with the new key.
4. Ship an installer build that carries both pins, and let the old packages age out.
5. Only then revoke the old id with `fm1_sign.py unpin --keyid qg1-release-1`, commit the pin
   diff, and confirm the old package is now refused with `key`. (`--replace` is *not* for this: it
   moves an id to a different key, which is a different decision. `unpin` refuses an id that is not
   pinned, so a typo cannot pass for a revocation, and it refuses to leave the ring empty unless
   `--allow-empty` says so.)

`tests/key_custody_test.py` performs this cycle with an ephemeral key, and `tests/sign_test.py`
does it again as operations: two ids pinned and both verifying, `unpin` removing one, the revoked
key's package refused with `key` while the other key's still verifies, the last key needing
`--allow-empty`, and re-pinning being an explicit `pin`.

## 6. Revocation and compromise

There is no online revocation list: an installer trusts the pin file it ships with.

* **Revoke an id** with `fm1_sign.py unpin --keyid <id>` and ship a new installer build. Packages
  signed with it stop verifying, with the code `key`.
* **Suspected compromise** means everything signed with that id after the suspected moment is
  hostile, including packages already published. Delete the pin, publish a new key id, and say so
  in the release notes: users must reinstall from a package signed by the new id.
* The device cannot help: the stock loader checks no signature, so a unit can still be flashed with
  an unsigned image by other means. Revocation protects the installers, not the boot chain.
* **Loss of the private key** is not recoverable in place. Generate a new id, pin it, sign again:
  the packages signed with the lost key stay valid while its pin is present, which is exactly why
  the pin is reviewed.

## 7. What this does not protect

* **Secure boot:** not claimed. The vendor loader does not verify the app.
* **A compromised signing host or installer host:** the manifest is checked by the host that has
  the key and by the host that writes the device. Either one being hostile is outside the model.
* **The browser page's origin:** WebCrypto verification runs in the page, so a hostile page (or a
  compromised origin) can lie. CSP and the published hash make that harder, not impossible.
* **Downgrade at the loader level:** `min_firmware` is checked by the host before the first frame
  that can write; the loader does not enforce it.

## 8. Checklist for a release signing

- [ ] `python3 tools/fm1_sign.py check --require-key` passes (one pinned id, the intended one).
- [ ] The package's SHA-256 equals the build log's and `tests/run_tests.sh` was green on that commit.
- [ ] `pin` was not used with `--replace` since the last review (a silent key change is a red flag).
- [ ] The manifest is beside the package and `make_site.py` published it.
- [ ] `verify` passes against the shipped `tools/fm1_keys.json`, and against the page's `meta.keys`.
- [ ] The release log has the key id, package hash and manifest.
- [ ] `git status` shows no key material and `tests/key_custody_test.py` is green.

Everything in §3–§6 is **host procedure only**. No key was generated for this project here, no
HSM was used, and no physical unit was flashed: `[NOT TESTED]` for a real ceremony.
