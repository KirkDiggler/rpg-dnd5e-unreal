#!/usr/bin/env bash
# Generates the compiled contract into Plugins/RpgWire/Source/RpgWire/Private/pb
# (ignored by git): C++ from every proto in the protos repository, at the tag
# this repository pins, with buf.gen.yaml at the repository's root. Headers keep
# their package directories; the .cc files are flattened, because ten packages
# name a file service.proto and Unreal names build products by file name.
#
# The contract comes from the published tag (design ruling R8): fetched from
# GitHub, one commit deep, into a cache; no checkout of the protos is needed
# here. To work on a branch of the contract before it is published, point
# RPG_API_PROTOS_DIR at a checkout: what is committed at its HEAD is used,
# never a working tree.
#
# Never edit what this writes. Change the protos, or this script.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/Plugins/RpgWire/Source/RpgWire/Private/pb"
UPSTREAM="${RPG_API_PROTOS_URL:-https://github.com/KirkDiggler/rpg-api-protos.git}"
REF="$(grep -v '^#' "$ROOT/protos.ref" | head -1 | tr -d '[:space:]')"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/rpg-dnd5e-unreal/protos.git"

say() { echo "fetch-protos: $*" >&2; }
[ -n "$REF" ] || { say "protos.ref names no tag"; exit 1; }
command -v buf >/dev/null 2>&1 || { say "buf is not installed (https://buf.build/install)"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/src"

if [ -n "${RPG_API_PROTOS_DIR:-}" ]; then
  [ -f "$RPG_API_PROTOS_DIR/buf.yaml" ] || { say "$RPG_API_PROTOS_DIR is not the protos repository (no buf.yaml)"; exit 1; }
  COMMIT="$(git -C "$RPG_API_PROTOS_DIR" rev-parse HEAD)"
  git -C "$RPG_API_PROTOS_DIR" archive "$COMMIT" | tar -x -C "$TMP/src"
  FROM="a checkout at $RPG_API_PROTOS_DIR, by RPG_API_PROTOS_DIR, @ ${COMMIT:0:7}"
else
  # A bare cache that holds only the pinned tag's tree, one commit deep.
  mkdir -p "$(dirname "$CACHE")"
  [ -d "$CACHE" ] || git init -q --bare "$CACHE"
  git --git-dir="$CACHE" fetch -q --depth 1 --force "$UPSTREAM" "refs/tags/$REF:refs/tags/$REF" \
    || { say "could not fetch tag $REF from $UPSTREAM"; exit 1; }
  COMMIT="$(git --git-dir="$CACHE" rev-parse --verify "refs/tags/$REF^{commit}")"
  git --git-dir="$CACHE" archive "$COMMIT" | tar -x -C "$TMP/src"
  FROM="KirkDiggler/rpg-api-protos @ $REF, commit ${COMMIT:0:7}"
fi
[ -f "$TMP/src/buf.yaml" ] || { say "what was fetched is not the protos repository (no buf.yaml): $FROM"; exit 1; }


# The contract as the protos carry it. A checkout from before the two `descriptor`
# fields were renamed (rpg-api-protos#457) does not compile in C++: the accessor
# collides with the one every message has. Said here, where the cause is plain,
# rather than in a generated header.
if grep -q '^  WorldNPCDescriptor descriptor = ' "$TMP/src/dnd5e/api/session/v1alpha1/service.proto"; then
  echo "fetch-protos: $FROM predates rpg-api-protos#457, which renamed the two \`descriptor\` fields; the C++ it generates does not compile." >&2
  echo "fetch-protos: raise the tag in protos.ref to one that has it, or update the checkout RPG_API_PROTOS_DIR names." >&2
  exit 1
fi

buf generate --template "$ROOT/buf.gen.yaml" --output "$TMP/gen" "$TMP/src"

rm -rf "$OUT"
mkdir -p "$OUT"
(cd "$TMP/gen/cpp" && find . -name '*.pb.h' -print0 | cpio -pdm0 --quiet "$OUT")
(cd "$TMP/gen/cpp" && find . -name '*.pb.cc' -print0 | while IFS= read -r -d '' f; do mv "$f" "$OUT/$(echo "${f#./}" | tr / _)"; done)
cat > "$OUT/SOURCE" <<NOTE
generated $(date -u +%Y-%m-%dT%H:%MZ) by scripts/fetch-protos.sh
protos: $FROM
plugins: $(grep -E 'remote:' "$ROOT/buf.gen.yaml" | sed -E 's/.*remote: *//; s/^/  /')
NOTE
echo "fetch-protos: $(find "$OUT" -name '*.pb.cc' | wc -l) .pb.cc, $(find "$OUT" -name '*.pb.h' | wc -l) .pb.h from $FROM"
