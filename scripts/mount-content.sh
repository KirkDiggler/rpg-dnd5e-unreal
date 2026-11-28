#!/usr/bin/env bash
# Link the licensed native Kingdom tree at its original /Game package root.
# The project also builds without it; nothing licensed is committed here (R6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Beside the main checkout, whether this is the checkout itself or a worktree.
COMMON="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)"
MAIN="$(cd "$COMMON/.." && pwd)"
SRC="${RPG_GAME_ASSETS_DIR:-$MAIN/../rpg-game-assets}/unreal/RpgGameAssets/Content/PolygonFantasyKingdom"
DEST="$ROOT/Content/PolygonFantasyKingdom"

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
