# RELEASE PROCESS — QG-1

A release is a set of bytes that can be named, verified and reproduced. This page defines those
steps and states which ones this repository can already do.

---

## 1. One source of releases

`web/make_site.py` writes `firmware/versions.json`, the only list the installer reads:

```json
[{ "version": "v1.8.7-<commit>", "file": "felucca-v1.8.7-<commit>.fwsc",
   "sha256": "<64 hex>", "size": <bytes> }]
```

Rules:

* The current package is first; at most five earlier builds are kept, one per version.
* A package is published only if it carries the `FELUCCA-LOADER-1` marker (no vendor-patched
  packages) and its identity is `FM-1_9xx`.
* Every entry has `sha256` and `size`; the web installer verifies the downloaded bytes against them
  and refuses anything else. The CLI verifies the same hash from `--sha256`, a `.sha256` sidecar or
  `SHA256SUMS`.
* **Never** point the installer at a mutable branch artefact. A build that is not a named, hashed
  package is not installable.

## 2. What a build record must contain

`tools/build.py` writes `build/manifest.json` (`tools/build_manifest.py`, schema `qg1-build-1`) after
every build, `--verify --require-clean` re-checks every claim, and `web/make_site.py` refuses to
publish a package the record does not describe. The installer (`web/fm1provenance.js`,
`requireProof()`) refuses a package that has no record, whose tree was dirty, or whose record names
other bytes or another identity — before the first frame.

| Field | Where it comes from | Today |
|-------|--------------------|-------|
| version | `FELUCCA_VERSION` in `firmware/src/felucca.c` | yes |
| commit | `git rev-parse HEAD` | yes (package name, emulator manifest) |
| date | build time | yes |
| toolchain | JieLi version, `AC79_SDK` commit | partially (documented in build logs) |
| configuration | `FELUCCA_*` knobs as the compiler saw them | yes (`config`, `qg1-build-1`) |
| package sha256 / size | `web/make_site.py`, re-checked against the record | yes |
| WASM sha256 / size | `build/emu/manifest.json`, folded into the record | yes, when the emulator was built |
| test results | `tests/run_tests.sh` exit status + summary | in the record as `checks` when a build writes them; otherwise manual |
| signature | `fm1_sign.py sign --build-manifest` -> `PACKAGE.fwsc.manifest.json`, beside the package | yes; `make_site.py` publishes it per version, and the installer refuses a manifest that does not name the record (`build_sha256`). **Nothing is signed yet: no key is pinned** (`docs/SIGNING_CUSTODY.md`) |

What is missing is not the format but the inputs: the toolchain and SDK hashes are "missing"/"unknown"
in a checkout without them (a host build here), the `checks` block is only filled when the build writes
it, and no production key exists. That is why the process is still documented as *not yet releasable*:
the record and the signature paths are in place and tested, but the artefacts they would describe can
only be produced with the JieLi toolchain.

## 3. Gates

1. `tests/run_tests.sh` green (host, sanitizers, UI lint/alignment, web, package, target budgets when
   the toolchain is present).
2. `python3 tests/memory_map_test.py` green; budgets in `docs/MEMORY_BUDGET.md` respected.
3. `python3 tests/readme_nav_test.py` and `python3 tests/backup_tool_test.py` green.
4. The package rebuilt from the released commit, installed on a **laboratory** unit, recovery
   demonstrated (`docs/INSTALLATION_SAFETY.md` §4).
5. A signed package, an anti-downgrade story and an independent recovery path
   (`docs/SECURITY_AUDIT.md` A–C) — otherwise the release is a laboratory build.
6. Key custody: `python3 tools/fm1_sign.py check --require-key` passes, the private key is outside
   the checkout, and the pin diff was reviewed (`docs/SIGNING_CUSTODY.md`). While no key is pinned
   this gate fails by design and no package may be called signed.

## 4. Vocabulary (do not blur these)

* **last commit** — the newest change; not a release.
* **last build** — what CI produced; not necessarily installable.
* **signed package** — a `.fwsc` whose manifest verifies against a pinned key. The chain is
  implemented and tested (`tests/sign_test.py`, `tests/key_custody_test.py`); *no package has been
  signed for release yet, because no key is pinned.*
* **release** — a published, versioned, hashed artefact with notes.
* **laboratory build** — software-verified, no physical validation.
* **approved for hardware** — demonstrated on the target, with the owner's decision recorded.

## 5. Publication

Publishing Pages or a GitHub Release is an owner action. Nothing in this repository publishes on its
own, and a green CI run is not an authorisation to install anything.

## 6. Making a release

Push a `v*` tag; `.github/workflows/release.yml` builds it, attaches the package to a GitHub release and
notifies mutant.cloud. Step by step (in Spanish): [RELEASE.es.md](RELEASE.es.md).
