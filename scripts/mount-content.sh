#!/usr/bin/env bash
# Link the licensed native Kingdom tree at its original /Game package root.
# The project also builds without it; nothing licensed is committed here (R6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Beside the main checkout, whether this is the checkout itself or a worktree.
COMMON="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)"
MAIN="$(cd "$COMMON/.." && pwd)"
STORE="${RPG_GAME_ASSETS_DIR:-$MAIN/../rpg-game-assets}/unreal/RpgGameAssets"
SRC="$STORE/Content/PolygonFantasyKingdom"
DEST="$ROOT/Content/PolygonFantasyKingdom"

# The previous mount used a content plugin. Remove only its old symlink.
LEGACY="$ROOT/Plugins/RpgGameAssets"
if [ -L "$LEGACY" ]; then
  unlink "$LEGACY"
fi

if [ ! -d "$SRC" ]; then
  echo "mount-content: no Kingdom content at $SRC; building without one"
  exit 0
fi
if [ -e "$DEST" ] && [ ! -L "$DEST" ]; then
  echo "mount-content: refusing to replace non-symlink $DEST" >&2
  exit 1
fi
mkdir -p "$ROOT/Content"
ln -sfnT "$(cd "$SRC" && pwd)" "$DEST"
echo "mount-content: $SRC -> Content/PolygonFantasyKingdom"

MANIFEST_SRC="$STORE/Config/RpgGameAssetsManifest.json"
MANIFEST_DEST="$ROOT/Config/RpgGameAssetsManifest.json"
if [ -f "$MANIFEST_SRC" ]; then
  if [ -e "$MANIFEST_DEST" ] && [ ! -L "$MANIFEST_DEST" ]; then
    echo "mount-content: refusing to replace non-symlink $MANIFEST_DEST" >&2
    exit 1
  fi
  mkdir -p "$ROOT/Config"
  ln -sfnT "$(realpath "$MANIFEST_SRC")" "$MANIFEST_DEST"
  echo "mount-content: $MANIFEST_SRC -> Config/RpgGameAssetsManifest.json"
fi
