#!/usr/bin/env bash
# Three facts about the libraries the module links, each said in one line.
#   1. No TurboLink anywhere in the repository's own words (ruling R3).
#   2. Built the way the module is built: no RTTI, no exceptions
#      (RpgWire.Build.cs compiles the module with neither).
#   3. One pin, written down: what was built says which commit it came from.
# The exit status is 0 only when all three hold.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/Plugins/RpgWire/Source/ThirdParty"
PIN="$ROOT/scripts/libs/vcpkg.pin"
fail=0
ok()  { echo "test-libs: ok    $1"; }
bad() { echo "test-libs: FAIL  $1" >&2; fail=1; }

# 1. The repository's own words; this file names it only to refuse it.
names="$(git -C "$ROOT" grep -i -l turbolink -- . ':!scripts/test-libs.sh' || true)"
if [ -n "$names" ]; then
  bad "1. TurboLink is still named in: $(echo "$names" | tr '\n' ' ')"
else
  ok "1. no TurboLink in the repository's own words"
fi

# 2. Measured on the libraries themselves, not on what a script meant to do.
count() { grep -c "$1" || true; }
for pair in "protobuf libprotobuf.a" "grpc libgrpc++.a"; do
  set -- $pair
  f="$DEST/$1/lib/linux/Release/$2"
  if [ ! -f "$f" ]; then
    bad "2. $2 is not built ($f); run scripts/build-libs.sh"
    continue
  fi
  rtti="$(nm --defined-only "$f" | count ' [^ ] _ZTI')"
  exc="$(nm --undefined-only "$f" | count '__gxx_personality')"
  if [ "$rtti" = 0 ] && [ "$exc" = 0 ]; then
    ok "2. $2: no RTTI, no exceptions"
  else
    bad "2. $2: $rtti RTTI symbols, $exc exception references"
  fi
done

# 3. The pin and the record of the build agree, by tag and by commit.
if [ -f "$PIN" ] && [ -f "$DEST/SOURCE" ]; then
  tag="$(awk '/^tag /{print $2}' "$PIN")"
  commit="$(awk '/^commit /{print $2}' "$PIN")"
  if grep -q "$tag" "$DEST/SOURCE" && grep -q "$commit" "$DEST/SOURCE"; then
    ok "3. built from vcpkg $tag ($commit), as scripts/libs/vcpkg.pin names it"
  else
    bad "3. ThirdParty/SOURCE does not name the pin ($tag, $commit)"
  fi
else
  bad "3. no scripts/libs/vcpkg.pin, or no ThirdParty/SOURCE"
fi

exit $fail
