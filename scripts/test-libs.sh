#!/usr/bin/env bash
# Four facts about the libraries the module links.
#   1. No TurboLink anywhere in the repository's own words (ruling R3).
#   2. Built the way the module is built: no RTTI, no exceptions
#      (RpgWire.Build.cs compiles the module with neither).
#   3. One pin, written down: what was built says which commit it came from.
#   4. Bundled OpenSSL has the Linux linker's symbol-version script.
# The exit status is 0 only when all four hold.
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
  # The versions line is seven "name version" pairs: a blank one is a hole.
  if grep -q "$tag" "$DEST/SOURCE" && grep -q "$commit" "$DEST/SOURCE" \
     && awk '/^grpc .* · protobuf .* · abseil /{ if (NF != 20) exit 1 }' "$DEST/SOURCE"; then
    ok "3. built from vcpkg $tag ($commit) with every version named, as scripts/libs/vcpkg.pin says"
  else
    bad "3. ThirdParty/SOURCE does not name the pin ($tag, $commit) with every version filled"
  fi
else
  bad "3. no scripts/libs/vcpkg.pin, or no ThirdParty/SOURCE"
fi

# 4. Unreal reads this path next to the bundled archives when it links a module.
openssl_dir="$DEST/grpc/lib/linux/Release"
version_script="$openssl_dir/UE_OpenSSL.ver"
if [ -f "$openssl_dir/libcrypto.a" ] && [ -f "$openssl_dir/libssl.a" ] && [ -f "$version_script" ] &&
   grep -qE '^[[:space:]]*OPENSSL_init_crypto;$' "$version_script" &&
   grep -qE '^[[:space:]]*OPENSSL_init_ssl;$' "$version_script"; then
  ok "4. bundled OpenSSL has UE_OpenSSL.ver beside the archives, naming OPENSSL_init_crypto and OPENSSL_init_ssl"
else
  bad "4. bundled OpenSSL is missing its archives or a complete $version_script; run scripts/build-libs.sh"
fi

exit $fail
