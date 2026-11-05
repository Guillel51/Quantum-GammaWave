# Package authenticity: Ed25519

Status: **IMPLEMENTED, HOST TESTED** (Python `cryptography`, and WebCrypto in the browser
verifier). **No release key exists yet**, so `tools/fm1_keys.json` pins nothing and every
package is refused: that is the released, fail-closed state.

This is authenticity (who built these bytes), not secure boot. The stock loader does not check
signatures, so the chain is only as strong as the installer that runs it. The procedure for the
custodian — ceremony, pinning, rotation, revocation — is in [`SIGNING_CUSTODY.md`](SIGNING_CUSTODY.md).

## Manifest (`qg1-release-1`)

```json
{
  "format": "qg1-release-1",
  "product": "FM-1",
  "version": "3.0.0",
  "min_firmware": "2.9.0",
  "sha256": "<sha256 of the .fwsc>",
  "size": 610897,
  "created": "2026-11-04T12:00:00Z",
  "keyid": "qg1-release-1",
  "sig": "<128 hex characters>"
}
```

The signature covers the manifest **without `sig`**, canonicalised as sorted keys, compact
separators, ASCII: `json.dumps(obj, sort_keys=True, separators=(",",":"), ensure_ascii=True)`
in Python, the same construction in JavaScript. Any change to a field breaks it.

## Commands

```bash
python3 tools/fm1_sign.py keygen --out /secure/qg1-release.pem --keyid qg1-release-1
python3 tools/fm1_sign.py pin qg1-release.pub --keyid qg1-release-1    # writes tools/fm1_keys.json
python3 tools/fm1_sign.py sign build/felucca.fwsc --key /secure/qg1-release.pem \
        --version 3.0.0 --min 2.9.0 --keyid qg1-release-1
python3 tools/fm1_sign.py verify build/felucca.fwsc --manifest build/felucca.fwsc.manifest.json
python3 tools/fm1_sign.py check [--require-key]                        # the pinned ids, or fail-closed
```

`keygen` refuses a destination inside this checkout, `sign` refuses a private key stored there and a
`keyid` that does not match the private key, and `pin` needs `--replace` to move an id to a different
key. A malformed pin file refuses every package instead of being read leniently, and
`check --require-key` is the release gate.

The installer runs the same verification (`tools/fm1_sign.py:verify`) after identifying the
chip and before the confirmation prompt; the web page has `verifyManifest` in
`web/fm1ota.js`, which uses WebCrypto and the same canonical form. With no key pinned, both
refuse.

## Custody

- The **private key never enters the repository, the client or the web page.** It lives on the
  custodian's machine or an HSM (`/secure/...` above), mode 600, and is never committed.
- `tools/fm1_keys.json` holds **public** keys only, and is reviewed like code. A private key found
  inside the checkout is treated as leaked; the tooling will not create or use one there.
- Generation, rotation and revocation are written down here and exist as commands, but the
  custodian is a person, not this repository: **no key has been generated for release**, and
  generating one is an act of the project owner.
- **Rotation:** pin the new key id next to the old one, sign new packages with the new key,
  keep the old pin until no supported package depends on it, then remove it.
- **Revocation:** delete the key id from `tools/fm1_keys.json` and ship an installer build.
  Packages signed with a revoked key then fail with `key` — there is no online list, so a
  revoked key only stops being accepted by installers that carry the new pin file.
- **min_firmware** refuses a package on firmware older than it claims; a package whose
  `min_firmware` is newer than its `version` is refused as inconsistent.

## Limits

- No secure boot, no rollback protection at the loader level, no signature check by the SPL.
- The hash and the signature are checked by the host before the first frame that can write;
  once the loader runs, the signature is not re-checked.
- An empty pin set refuses everything; that is intentional and is the current state.
