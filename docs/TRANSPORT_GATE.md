# The transport gate — no frame is sent without a verified package

`web/fm1ota.js` is the only code that talks to an FM-1 while it is being updated. Before this
change it checked one thing: the flash part (`requireChip`). Everything else depended on the page
having verified the download first, which is not the same as the transport knowing it: a direct
call to `install()`, `write()` or `resume()` from a console, a mutated buffer, a swapped
`Uint8Array`, or a resumed session with a stale "already verified" flag all reached the same
`link.send()` that makes the device erase and program.

The gate below closes that. It is enforced inside the transport, for every entry point, and it is
what the negative tests assert with counters, not with error strings.

## 1. What the transport now proves, and against what

`requireProof({ pkg, manifest, keys, product, evidence, firmware })` runs **before `find()`**, so
a refusal costs zero frames. It verifies:

| Claim | How | Refusal |
|---|---|---|
| the bytes are the manifest's | SHA-256 and `size` over the exact `Uint8Array` | `signature` |
| the manifest is the release's | Ed25519 over the canonical manifest, `keyid` pinned in `tools/fm1_keys.json`; empty pin set trusts nothing | `signature` |
| the package is for this model | `manifest.product` (e.g. `FM-1`) against the product's model | `signature` |
| the package is this identity | `productOf(pkg) === product` (e.g. `FM-1_982`), read from the package itself | `proof` |
| the version floor is usable | `version` and `min_firmware` parse as dotted numbers; `firmware` (the running version, when known) is not below `min_firmware` | `version` / `signature` |
| the part may be written | `requireChip(evidence)`: identified, 1 MiB allowlist, not the read-only UCUN | `chip` |
| it is a package at all | non-empty, longer than the 20-block header, not a string | `image` |

The result is an opaque frozen object. The verified bytes are **not** handed back: the gate keeps a
private copy of the package and derives the logical image (`logicalImage(pkg)`) from that copy once,
so what the device reads is byte-for-byte what the signature covered.

## 2. One authorisation, one transfer

`install()`, `write()` and `resume()` each call `admit()` with the authorisation they were given
(or with a fresh one they build themselves from `ctx.manifest`/`ctx.keys`). `admit()` re-checks the
product, runs the chip policy again for *this* transfer, requires the same JEDEC, hashes the bytes
the caller passed *now* and refuses if they differ from the verified ones, then marks the
authorisation used. Consequences:

* a second transfer with the same authorisation is refused (`proof`) — `install()` therefore gives
  `write()` its own authorisation instead of forwarding the step-1 one;
* a buffer edited between verification and transmission is refused (`image`) and, even if it were
  not, the served bytes come from the private copy;
* `write()` is a public entry point and verifies from scratch; it does not inherit `install()`'s
  decision. A loader identity must match the authorised product (`ota-<MODEL>_<n>`), or `proof`;
* the unit that answers the handshake must report exactly the authorised identity, not merely the
  same model (`model`), and the check happens **before** the `UPGRADE` frame.
* the only code that sends image bytes, `#serve`, is a private method: the class exposes
  `find`, `waitFor`, `install`, `write` and `resume`, and none of them can be used to push an
  unverified buffer. A test asserts that surface, so a future public `serve()` fails the suite.
* the transport identifies the chip itself, on the link it is about to write through
  (`identify()` → two `0x12` replies, a fresh session token each). `requireProof()` takes no
  evidence: a caller cannot hand in a JEDEC it never read, an identification from an earlier
  connection is not reused, and `#serve()` refuses any link other than the one `identify()` bound.
* the installed version is required. Neither the loader nor the handshake exposes it, so a transfer
  whose version is unknown is refused (`version`) instead of assuming the current one. The page
  remembers the version it installed and sends nothing else.

## 3. Who passes what

`web/index_pkg.html` keeps the downloaded `.fwsc` (`image`) and the manifest that verified it
(`releaseManifest`, set by `loadVersion()`), and hands both to the transport:

```js
await up.install(image, meta.product, step, ctxFor(ev, versionOf(fw.info)));
```

`ctxFor(ev, firmware)` is only a carrier: `{ evidence, manifest, keys, firmware }`. The page's own
pre-check decides when the button is enabled; the transport decides what is transmitted.

## 4. Restoring the official V15 no longer installs

The vendor package has no release manifest and this project has no key that could sign it, so the
`requireProof()` refuses the stock/restore route (`signature: No signed manifest`). The page passes
`manifest: null` deliberately and shows the transport's reason instead of bypassing it. Restoring
the vendor firmware remains possible with the vendor's own tool; making it possible here would need
a signed manifest for those exact bytes, which is a release decision, not a code change.

With `tools/fm1_keys.json` empty (the released state) **every** route is refused at the gate.

## 5. Evidence

`node web/test_web.mjs` builds a package, signs it with an ephemeral Ed25519 key generated in the
test process, and drives a simulated FM-1 that counts every frame it receives:

* positive: a signed, complete, compatible package installs (`11 reads, 2 UPGRADE frames`, every
  served byte compared by the simulator);
* 18 negative cases (`no context`, `no manifest`, `no pinned key`, unknown key id, tampered
  manifest, wrong hash, wrong size, another product, missing `min_firmware`, ambiguous version,
  downgrade, read-only chip, unknown chip, not bytes, empty package, `resume()` without a proof,
  `write()` without a proof, no evidence): each one asserts `code`, `frames === 0`,
  `upgrades === 0` (no UPGRADE command), `served === 0` (no image byte);
* package changed after verification → `image`, no frame; swapped package with a valid
  authorisation → `image`, no frame; authorisation reused → `proof`, no frame; a unit answering as
  another identity → `model`, zero writes;
* after admission the port can still be lost: `lost`, `noreturn` and `notfound` keep their old
  behaviour, so the gate did not turn a dead cable into a silent success.

## 6. What this does not cover

* **Hardware.** No unit was connected, flashed or restored. Counters and refusals are host/simulator
  evidence; how the real loader behaves on a cable drop is unchanged by this work.
* **Downgrade when the running version is unknown.** The identity frame carries a build number
  (`FM-1_982`), not a version, and the loader carries none at all, so `firmware` is `null` on the
  resume path. There the gate relies on the signature, the bytes, the product and the chip; the
  `min_firmware` comparison is active only where the page can read the running version.
* **Secure boot.** The vendor bootloader still does not verify the application (see
  `SECURITY_AUDIT.md`, row E).
* **The physical part.** JEDEC and geometry come from the device's own `0x12` reply; nothing here
  proves what a particular unit's NOR is.
* **`evidence` is gone, the probe is not.** The transport reads the JEDEC through `0x12` on its own
  link, but a device that lies in that reply is outside anything the host can check.
* **A script on the page can always speak MIDI itself.** `navigator.requestMIDIAccess` is available
  to any script on the origin, so this gate protects the supported paths, the protocol and the
  release chain — it is not a sandbox against an operator who opens the port by hand.

## 7. Artifacts

This change touches `web/fm1ota.js`, `web/index_pkg.html`, `web/test_web.mjs` and documentation
only. Nothing in `firmware/`, `loader/` or `web/emu/` was rebuilt, so the artifacts on disk are the
ones built at `f93ef06` (`integ/v1.8.9`), not at this commit:

| Artifact | Bytes | SHA-256 | Built at |
|---|---|---|---|
| `build/felucca.fwsc` | 611 163 | `9bcb862e785043d8ba2f5defd835f61c31ce677bad7cece3a465eac9d22249a3` | `f93ef06` |
| `build/loader/ota.bin` | 8 007 | `468fb0e20eddf855a32bbcccb51f8eda770478b7214ba3937680a7308f73acbf` | `f93ef06` |
| `build/emu/felucca.wasm` | 1 038 761 | `a268bcab7984ad867ced6a674dfbe5e1ea6ff066a8dafa2195fcb66122b0e645` | `f93ef06` |

No package is signed: `tools/fm1_keys.json` has no pinned key, so no `.manifest.json` exists for
`build/felucca.fwsc` and `web/make_site.py` publishes `meta.keys = {}`. That is the intended state
until a custodian exists (`SIGNING_CUSTODY.md`).
