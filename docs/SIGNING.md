# Package integrity and authenticity (SHA-256 model, since v2.6.0; current in v2.7.0)

Status: **SHA-256 + size, published with the package; GitHub Artifact Attestations for traceability.**
Ed25519 (`qg1-release-1`) is **historical/obsolete** for ordinary installs — see §"Historical".

## Install gate (web + CLI)

The installer accepts a package when all of the following hold:

1. The package's SHA-256 equals the value published next to it (`SHA256SUMS` / the release manifest).
2. The package's byte size equals the published size.
3. The model/identity in the manifest matches the unit being served (a package for `FM-1` is never
   accepted for another model).
4. The version floor (`min_firmware`) is satisfied by the unit's current firmware.
5. The chip is identified (JEDEC allowlist) and the loader checks the chip before any erase
   (PRs #86–#89, #93).
6. Build provenance is consistent (commit, build record hash) where a build record is available.

No private key is required for any of these steps. `tools/fm1_keys.json` is retained for historical
compatibility but is no longer a gate.

## Limitation (read this)

A SHA-256 published by the same origin (this repository / GitHub Releases) proves **integrity of the
transfer**, not **authorship**. Anyone who can publish to that origin can publish a matching hash.
Authenticity of the editor is provided separately by **GitHub Artifact Attestations** (see
`.github/workflows/release.yml`, `id-token: write` + `attestations: write`), which attest that the
package was built from this source on GitHub. These attestations are **not** an Ed25519 signature of
the package and must not be presented as one.

## Historical (Ed25519, obsolete for installs)

`tools/fm1_sign.py`, `tools/fm1_keys.json` and `qg1-release-1` remain in the tree for audit/custody
history. They are **not** part of the active install or publish path since v2.6.0. Do not delete them
without review; mark as obsolete rather than removing. The custodian procedure lives in
[`SIGNING_CUSTODY.md`](SIGNING_CUSTODY.md) (historical).