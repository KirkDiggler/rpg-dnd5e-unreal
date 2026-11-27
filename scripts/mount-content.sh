#!/usr/bin/env bash
# Mounts the licensed content plugin, if rpg-game-assets is checked out beside
# this repository and holds one: its unreal/RpgGameAssets is linked at
# Plugins/RpgGameAssets (ignored by git). The project builds and tests without
# it; nothing licensed is ever committed here (design, R6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Beside the main checkout, whether this is the checkout itself or a worktree.
MAIN="$(cd "$(git -C "$ROOT" rev-parse --git-common-dir)/.." 2>/dev/null && pwd || echo "$ROOT")"
SRC="${RPG_GAME_ASSETS_DIR:-$MAIN/../rpg-game-assets}/unreal/RpgGameAssets"
DEST="$ROOT/Plugins/RpgGameAssets"

if [ ! -d "$SRC" ]; then
  echo "mount-content: no content plugin at $SRC; building without one"
  exit 0
fi
rm -rf "$DEST"
ln -s "$SRC" "$DEST"
echo "mount-content: $SRC -> Plugins/RpgGameAssets"
