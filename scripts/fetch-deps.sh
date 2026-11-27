#!/usr/bin/env bash
# Vendors gRPC, protobuf, abseil and re2 for Unreal's toolchain into
# Plugins/RpgWire/Source/ThirdParty (ignored by git), from a pinned archive.
#
# A STOPGAP until ruling R3 lands on the design PR (rpg-project#1155): the
# libraries are the ones the TurboLink release vendors, built for UE 5.6 and
# usable in 5.9, not a build of our own. Only its Source/ThirdParty is taken;
# none of its code is. Linux only, for now.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/Plugins/RpgWire/Source/ThirdParty"
REPO="thejinchao/turbolink"
RELEASE="v1.4.2"
ASSET="TurboLink.UE56.zip"
SHA256="9ad1ecd9e64d4166fff973bcb5b3f493756987a99f2d3c2e9933b9c2908eaf39"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/rpg-dnd5e-unreal"

mkdir -p "$CACHE"
if [ ! -f "$CACHE/$ASSET" ]; then
  echo "downloading https://github.com/$REPO/releases/download/$RELEASE/$ASSET (912 MB) into $CACHE"
  curl -fL --retry 3 -o "$CACHE/$ASSET.part" "https://github.com/$REPO/releases/download/$RELEASE/$ASSET"
  mv "$CACHE/$ASSET.part" "$CACHE/$ASSET"
fi
if ! echo "$SHA256  $CACHE/$ASSET" | sha256sum --check --quiet; then
  echo "fetch-deps: $ASSET does not match the pinned sha256; refusing it" >&2
  exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
# Where the plugin sits in the archive (its root, as it happens), so the
# patterns below hold whichever folder the release was wrapped in.
FIRST="$(unzip -Z1 "$CACHE/$ASSET" | grep -m1 'Source/ThirdParty/' || true)"
[ -n "$FIRST" ] || { echo "fetch-deps: no Source/ThirdParty in $ASSET; the release's layout changed" >&2; exit 1; }
PREFIX="${FIRST%%Source/ThirdParty/*}"
# Headers and the Linux libraries; the other platforms are later.
unzip -q "$CACHE/$ASSET" "${PREFIX}Source/ThirdParty/*/include/*" "${PREFIX}Source/ThirdParty/*/lib/linux/*" -d "$TMP"
[ -d "$TMP/${PREFIX}Source/ThirdParty/grpc/include" ] || { echo "fetch-deps: the archive did not yield the headers" >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$DEST"
cp -a "$TMP/${PREFIX}Source/ThirdParty/." "$DEST/"
echo "fetch-deps: vendored $(ls "$DEST" | tr '\n' ' ')from $REPO $RELEASE ($ASSET, sha256 ${SHA256:0:12}…)"
