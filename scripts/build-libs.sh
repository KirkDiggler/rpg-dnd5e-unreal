#!/usr/bin/env bash
# Builds gRPC, protobuf, abseil, re2 and what they need with Unreal's own
# toolchain, into Plugins/RpgWire/Source/ThirdParty (ignored by git), and
# protoc and grpc_cpp_plugin for this machine beside them (design ruling R3:
# neither another plugin's code nor its binaries; the recipe is this file).
#
# The versions are the package tree's at one commit, scripts/libs/vcpkg.pin —
# the only place a version is chosen. vcpkg is cloned at the pin into the
# cache, the packages are built in classic mode with the triplet and the
# toolchain file in scripts/libs/, and what the module links is copied into
# the shape RpgWire.Build.cs expects. Linux only, for now (ruling R9).
#
# About ten minutes cold on a 32-core machine; about two from vcpkg's binary
# cache (~/.cache/vcpkg/archives — the machine's, not the repository's).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/rpg-dnd5e-unreal"
WORK="$ROOT/.libs/vcpkg"
DEST="$ROOT/Plugins/RpgWire/Source/ThirdParty"
VCPKG="$CACHE/vcpkg"
UPSTREAM="https://github.com/microsoft/vcpkg.git"
LIBS="$ROOT/scripts/libs"
START="$(date +%s)"

say() { echo "build-libs: $*" >&2; }

# The toolchain: the one Unreal ships for Linux x86_64, unless UE_LINUX_SDK names it.
if [ -n "${UE_LINUX_SDK:-}" ]; then
  SDK="$UE_LINUX_SDK"
else
  SDK="$(ls -d "$UE_ROOT"/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v*/x86_64-unknown-linux-gnu 2>/dev/null || true)"
  case "$SDK" in
    *$'\n'*) say "more than one Linux toolchain is under $UE_ROOT; name one with UE_LINUX_SDK:"; echo "$SDK" >&2; exit 1 ;;
  esac
fi
[ -n "$SDK" ] && [ -x "$SDK/bin/clang++" ] || { say "no Unreal Linux toolchain is under $UE_ROOT (see UE_ROOT and UE_LINUX_SDK)"; exit 1; }
say "toolchain: $SDK ($("$SDK/bin/clang++" --version | head -1))"

# vcpkg, at the pin.
TAG="$(awk '/^tag /{print $2}' "$LIBS/vcpkg.pin")"
COMMIT="$(awk '/^commit /{print $2}' "$LIBS/vcpkg.pin")"
[ -n "$TAG" ] && [ -n "$COMMIT" ] || { say "scripts/libs/vcpkg.pin names no tag and commit"; exit 1; }
if [ ! -d "$VCPKG/.git" ]; then
  mkdir -p "$CACHE"
  say "cloning vcpkg $TAG into $VCPKG"
  git clone -q --depth 1 --branch "$TAG" "$UPSTREAM" "$VCPKG"
fi
AT="$(git -C "$VCPKG" rev-parse HEAD)"
[ "$AT" = "$COMMIT" ] || { say "vcpkg at $AT is not the pinned $COMMIT; remove $VCPKG to take the pin"; exit 1; }
[ -x "$VCPKG/vcpkg" ] || (cd "$VCPKG" && ./bootstrap-vcpkg.sh > /dev/null)

# Two installs, in this order: the libraries for the engine's target, then the
# generators for this machine. The triplet and the toolchain file do the rest.
export UE_LINUX_SDK="$SDK" UE_VCPKG_INSTALL_ROOT="$WORK"
"$VCPKG/vcpkg" install grpc --classic --triplet x64-linux-ue --overlay-triplets="$LIBS/triplets" --x-install-root="$WORK"
"$VCPKG/vcpkg" install grpc --classic --triplet x64-linux --x-install-root="$WORK"

# Into the shape the module links: one include tree; the libraries by family,
# Release only. Left out: what the kept libraries already define a second time
# (the unsecure builds), and a plugin's own support library.
IN="$WORK/x64-linux-ue/lib"
rm -rf "$DEST"
mkdir -p "$DEST/grpc"
cp -a "$WORK/x64-linux-ue/include" "$DEST/grpc/include"
put() {
  local family="$1"; shift
  mkdir -p "$DEST/$family/lib/linux/Release"
  local pattern f n=0
  for pattern in "$@"; do
    for f in "$IN"/$pattern; do
      [ -f "$f" ] || continue
      cp -a "$f" "$DEST/$family/lib/linux/Release/"
      n=$((n + 1))
    done
  done
  [ "$n" -gt 0 ] || { say "no library matched $* in $IN"; exit 1; }
}
for f in "$IN"/libgrpc*.a; do
  case "$(basename "$f")" in
    libgrpc_plugin_support.a | libgrpc++_alts.a | libgrpc_unsecure.a | libgrpc++_unsecure.a) ;;
    *) put grpc "$(basename "$f")" ;;
  esac
done
put grpc 'libgpr.a' 'libupb*.a' 'libaddress_sorting.a' 'libcares.a' 'libssl.a' 'libcrypto.a' 'libz.a'
put protobuf 'libprotobuf.a' 'libutf8_*.a'
put abseil 'libabsl_*.a'
put re2 'libre2.a'

# What was built, from where, with what: read by scripts/test-libs.sh. The
# versions are vcpkg's own record of the install; a missing one stops the
# build here, because a record with a hole in it is worse than none.
port_version() { printf '%s\n' "$2" | awk -v p="$1" '$1 == p {print $2; found=1} END {exit !found}'; }
TARGET_LIST="$("$VCPKG/vcpkg" list --classic --x-install-root="$WORK" --triplet=x64-linux-ue)"
HOST_LIST="$("$VCPKG/vcpkg" list --classic --x-install-root="$WORK" --triplet=x64-linux)"
VERSIONS=""
for p in grpc protobuf abseil re2 c-ares openssl zlib; do
  v="$(port_version "$p:x64-linux-ue" "$TARGET_LIST")" || { say "vcpkg list does not name $p for x64-linux-ue"; exit 1; }
  VERSIONS="$VERSIONS${VERSIONS:+ · }$p $v"
done
grpc_cpp="$(port_version "grpc:x64-linux" "$HOST_LIST")" || { say "vcpkg list does not name grpc for x64-linux (the generators)"; exit 1; }
PROTOC_VERSION="$("$WORK/x64-linux/tools/protobuf/protoc" --version)" || { say "protoc was not built for this machine"; exit 1; }

COUNT="$(find "$DEST" -name '*.a' | wc -l)"
cat > "$DEST/SOURCE" <<NOTE
built $(date -u +%Y-%m-%dT%H:%MZ) by scripts/build-libs.sh
vcpkg $TAG ($COMMIT), the pin in scripts/libs/vcpkg.pin
$VERSIONS
toolchain $SDK
flags -fPIC -fvisibility=hidden -fno-rtti -fno-exceptions -DGOOGLE_PROTOBUF_NO_RTTI; the engine's libc++
generators .libs/vcpkg/x64-linux/tools: protoc $PROTOC_VERSION, grpc_cpp_plugin $grpc_cpp
$COUNT static libraries, Release
NOTE
cat "$DEST/SOURCE"
echo "build-libs: $COUNT libraries from vcpkg $TAG, in $(( $(date +%s) - START )) s"
