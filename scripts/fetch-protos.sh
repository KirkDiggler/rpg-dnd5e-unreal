#!/usr/bin/env bash
# Generates the compiled contract into Plugins/RpgWire/Source/RpgWire/Private/pb
# (ignored by git): C++ from every proto in the protos repository beside this
# one, with buf.gen.yaml at the repository's root. Headers keep their package
# directories; the .cc files are flattened, because ten packages name a file
# service.proto and Unreal names build products by file name.
#
# Never edit what this writes. Change the protos, or this script.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/Plugins/RpgWire/Source/RpgWire/Private/pb"
# Beside the main checkout, whether this is the checkout itself or a worktree.
MAIN="$(cd "$(git -C "$ROOT" rev-parse --git-common-dir)/.." 2>/dev/null && pwd || echo "$ROOT")"
PROTOS="${RPG_API_PROTOS_DIR:-$MAIN/../rpg-api-protos}"

[ -d "$PROTOS" ] || { echo "fetch-protos: no protos checkout at $PROTOS (set RPG_API_PROTOS_DIR)" >&2; exit 1; }
command -v buf >/dev/null 2>&1 || { echo "fetch-protos: buf is not installed (https://buf.build/install)" >&2; exit 1; }
[ -f "$PROTOS/buf.yaml" ] || { echo "fetch-protos: $PROTOS is not the protos repository (no buf.yaml)" >&2; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/src"
# What is committed, at the commit checked out: not anyone's working tree.
git -C "$PROTOS" archive HEAD | tar -x -C "$TMP/src"
COMMIT="$(git -C "$PROTOS" rev-parse HEAD)"

# R2, until it lands: `InteractResponse.descriptor` and `TradeResponse.descriptor`
# (dnd5e.api.session.v1alpha1.service) collide with protobuf's own
# Message::descriptor() in C++. A field's name is not on the wire, so the two are
# renamed in the copy and the generated code speaks the same bytes. The day the
# protos carry the rename, the count below is not two and this stops: delete it.
PROTO="$TMP/src/dnd5e/api/session/v1alpha1/service.proto"
COUNT="$(grep -c '^  WorldNPCDescriptor descriptor = 1;' "$PROTO" || true)"
if [ "$COUNT" != "2" ]; then
  echo "fetch-protos: R2's rename expects two fields named \`descriptor\` in InteractResponse and TradeResponse, and finds $COUNT." >&2
  echo "fetch-protos: if R2 has landed, remove this step." >&2
  exit 1
fi
sed -i 's/^  WorldNPCDescriptor descriptor = 1;/  WorldNPCDescriptor npc_descriptor = 1;/' "$PROTO"

buf generate --template "$ROOT/buf.gen.yaml" --output "$TMP/gen" "$TMP/src"

rm -rf "$OUT"
mkdir -p "$OUT"
(cd "$TMP/gen/cpp" && find . -name '*.pb.h' -print0 | cpio -pdm0 --quiet "$OUT")
(cd "$TMP/gen/cpp" && find . -name '*.pb.cc' -print0 | while IFS= read -r -d '' f; do mv "$f" "$OUT/$(echo "${f#./}" | tr / _)"; done)
cat > "$OUT/SOURCE" <<NOTE
generated $(date -u +%Y-%m-%dT%H:%MZ) by scripts/fetch-protos.sh
protos: $PROTOS @ $COMMIT
plugins: $(grep -E 'remote:' "$ROOT/buf.gen.yaml" | sed -E 's/.*remote: *//; s/^/  /')
with R2's rename applied to the copy (descriptor -> npc_descriptor in InteractResponse, TradeResponse)
NOTE
echo "fetch-protos: $(find "$OUT" -name '*.pb.cc' | wc -l) .pb.cc, $(find "$OUT" -name '*.pb.h' | wc -l) .pb.h from $(basename "$PROTOS") @ ${COMMIT:0:7}"
