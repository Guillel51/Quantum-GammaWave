# Signing key custody (Ed25519) — HISTORICAL

Status: **obsolete for ordinary installs as of v2.6.0.** Retained for audit history.

The Ed25519 release key `qg1-release-1` (pinned in `tools/fm1_keys.json`) is no longer a
requirement to publish or install a package. The v2.6.0 model uses SHA-256 + size + model/identity +
version floor + chip check + GitHub Artifact Attestations (see `docs/SIGNING.md` and
`docs/RELEASE_PROCESS.md`).

The private half of `qg1-release-1` was never stored in this repository and is not being requested,
generated, rotated or modified here. Custody files (`tools/fm1_sign.py`, `tools/fm1_keys.json`,
`release/sign-request-*.json`) are kept as historical evidence and marked obsolete rather than
deleted. Do not present the GitHub attestation as an Ed25519 signature of the package.