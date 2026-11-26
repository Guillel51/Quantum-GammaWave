#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Download JieLi's Linux toolchain (clang 4.0.1 for pi32v2) and link it as DEST/toolchain.
#   tools/get_toolchain.sh [DEST]      (default: ~/.jieli)
#
# The archive is pinned by SHA-256: a mirror that serves something else is refused before anything is
# unpacked. The pin lives in tools/toolchain.sha256 (reviewed and committed), and JIELI_TOOLCHAIN_SHA256
# -- the repository variable of that name, or the environment -- overrides it for a deliberate review.
# The URL serves the latest build, so the pin holds until JieLi replaces it upstream; then the job fails
# loudly, naming both hashes, instead of silently building with a compiler nobody chose.
#
# IDEMPOTENT AND VERIFIED (2.16): the install leaves DEST/toolchain.stamp with the archive hash it came
# from. A second run with the same pin does nothing (no 26 MB download, no unpack), and a DEST that is
# not stamped with the current pin -- a CI cache from before a review, a partial unpack, a hand-made
# symlink -- is reinstalled instead of being trusted. That was a real hole: the workflows only ran this
# script when DEST/toolchain was absent, so a cached toolchain from an earlier archive was used forever
# and the pin only ever applied to a cold cache.
set -e
DEST="${1:-$HOME/.jieli}"
PIN_FILE="$(dirname "$0")/toolchain.sha256"
STAMP="$DEST/toolchain.stamp"
mkdir -p "$DEST"
sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
WANT="${JIELI_TOOLCHAIN_SHA256:-}"
[ -n "$WANT" ] || WANT="$(sed -n 's/^[0-9a-f]\{64\}$/&/p' "$PIN_FILE" 2>/dev/null | tail -1)"
if [ -z "$WANT" ]; then
    echo "error: no pinned toolchain hash: $PIN_FILE has none and JIELI_TOOLCHAIN_SHA256 is empty." >&2
    echo "       Nothing was unpacked. Review the archive and add its sha256 to that file." >&2
    exit 1
fi
if [ -s "$STAMP" ] && [ -x "$DEST/toolchain/pi32v2/bin/clang" ] && [ "$(cat "$STAMP")" = "$WANT" ]; then
    echo "toolchain: already installed from the pinned archive ($WANT), nothing to download"
    exit 0
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -fL --retry 3 -o "$TMP/toolchain.tar.xz" https://pkgman.jieliapp.com/s/linux-toolchain
GOT="$(sha "$TMP/toolchain.tar.xz")"
if [ "$GOT" != "$WANT" ]; then
    echo "error: the toolchain archive is sha256 $GOT, the pinned value is $WANT." >&2
    echo "       Nothing was unpacked. Either JieLi replaced the build upstream (review it, then update" >&2
    echo "       $PIN_FILE) or the download did not come from them." >&2
    exit 1
fi
echo "toolchain archive: sha256 $GOT (pinned, verified)"
# unpack beside the old install, then swap: an interrupted or failed unpack never leaves DEST without a
# toolchain (the previous one, and its stamp, stay until the new one is complete)
mkdir -p "$TMP/x"
tar -xJ -f "$TMP/toolchain.tar.xz" -C "$TMP/x"
NEW="$(ls -d "$TMP"/x/jieli-linux-toolchains-* | sort | tail -1)"
rm -rf "$DEST"/jieli-linux-toolchains-*
mv "$NEW" "$DEST/"
TC="$(ls -d "$DEST"/jieli-linux-toolchains-* | sort | tail -1)"
ln -sfn "$(basename "$TC")" "$DEST/toolchain"
printf '%s\n' "$GOT" > "$STAMP"
echo "JIELI_TOOLCHAIN=$DEST/toolchain ($(basename "$TC"))"
