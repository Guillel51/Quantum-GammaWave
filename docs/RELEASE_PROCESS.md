# Release process — QG-1

Releases are GitHub releases, built by CI from a tag. Nothing is committed to the repository for a release.

1. Push a tag `vX.Y` (or `vX.Y.Z`, one digit each) on `main`.
2. `.github/workflows/release.yml` builds the firmware with the JieLi toolchain, creates the GitHub release with
   the `.fwsc` package, the app binary and `SHA256SUMS`, and notifies mutant.cloud.
3. Write the release notes on the release page.

The site (GitHub Pages, `.github/workflows/emulator.yml`) serves the build of `main`. The installer checks the
package against the SHA-256 and size published next to it before it writes anything, and the update loader
checks the flash chip before it erases anything.

Step by step, in Spanish: [RELEASE.es.md](RELEASE.es.md).
